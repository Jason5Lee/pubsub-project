#include "session.hpp"

Session::Session(boost::asio::ip::tcp::socket &&socket, int64_t pingDurationMs, std::shared_ptr<ChannelHub> channelHub)
    : ws(std::move(socket)), pingTimer(ws.get_executor()), pingDurationMs(pingDurationMs), isSub(false),
      channelHub(std::move(channelHub)), channel(nullptr), isSendingMessage(false) {
    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
}

Session::~Session() {
    if (channel != nullptr) {
        if (isSub) {
            channel->removeSubscriber(subIter);
        }
        channelHub->disconnectChannel(channelName, channel);
    }
}

void Session::run() {
    beast::http::async_read(ws.next_layer(), receiveBuffer, req, method_handler(&Session::onReadRequest));
}

void Session::fail(beast::error_code ec, char const *what) { std::cerr << what << ": " << ec.message() << std::endl; }

void Session::onReadRequest(beast::error_code ec, std::size_t) {
    if (ec) {
        return fail(ec, "read HTTP request");
    }
    const auto target = req.target();
    if (target.ends_with("/sub")) {
        isSub = true;
    } else if (!target.ends_with("/pub")) {
        return send404();
    }

    channelName = target.substr(1, target.size() - 5).to_string();

    if (channelName.find('/') != std::string::npos) {
        return send404();
    }

    ws.async_accept(req, method_handler(&Session::onAccept));
}

void Session::onAccept(beast::error_code ec) {
    if (ec) {
        return fail(ec, "accept");
    }

    sendPingDuration();
}

void Session::sendPingDuration() {
    ws.text(true);
    ws.async_write(boost::asio::buffer((boost::format("%x") % pingDurationMs).str()),
                   method_handler(&Session::onPingDurationSent));
}

void Session::onPingDurationSent(beast::error_code ec, std::size_t) {
    if (ec) {
        return fail(ec, "sendPingDuration");
    }

    channel = channelHub->connectChannel(channelName);
    if (isSub) {
        subIter = channel->addSubscriber(shared_from_this());
    }

    setupTimer();

    if (!isSub) {
        doPublisher();
    }
}

void Session::onReadPublish(beast::error_code ec, std::size_t) {
    pingTimer.cancel();
    setupTimer();
    if (ec) {
        if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::eof) {
            pingTimer.cancel();
            return;
        }
        fail(ec, "read");
    } else {
        auto message = std::make_shared<Message>(ws.got_binary());
        std::swap(message->content, receiveBuffer);
        channel->forEach([&message](const std::shared_ptr<Session> &session) { session->sendMessage(message); });
    }

    doPublisher();
}

void Session::sendMessage(std::shared_ptr<Message> message) {
    bool messageNotPending = false;
    {
        std::unique_lock lk(sendMsgMu);
        if (isSendingMessage) {
            pendingMessage = std::move(message);
        } else {
            isSendingMessage = true;
            messageNotPending = true;
        }
    }
    if (messageNotPending) {
        pingTimer.cancel();
        setupTimer();
        ws.binary(message->isBinary);
        ws.async_write(message->content.data(), method_handler(&Session::onMessageSent));
    }
}

void Session::onMessageSent(beast::error_code ec, std::size_t) {
    if (ec) {
        if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::broken_pipe) {
            pingTimer.cancel();
            return;
        }
        fail(ec, "write");
    }
    {
        std::unique_lock lk(sendMsgMu);
        if (pendingMessage != nullptr) {
            std::shared_ptr<Message> message = nullptr;
            std::swap(message, pendingMessage);
            ws.binary(message->isBinary);
            ws.async_write(message->content.data(), method_handler(&Session::onMessageSent));
        } else {
            isSendingMessage = false;
        }
    }
}

void Session::setupTimer() {
    pingTimer.expires_after(std::chrono::milliseconds(pingDurationMs));
    pingTimer.async_wait(method_handler(&Session::onTimer));
}

void Session::onTimer(beast::error_code ec) {
    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        fail(ec, "timer");
        ws.async_close(websocket::close_code::internal_error, method_handler(&Session::onClose));
        return;
    }
    setupTimer();
    ws.async_ping({}, method_handler(&Session::onPing));
}

void Session::onPing(beast::error_code ec) {
    if (ec) {
        pingTimer.cancel();
        if (ec == boost::asio::error::broken_pipe) {
            return;
        }
        fail(ec, "ping");
        ws.async_close(websocket::close_code::try_again_later, method_handler(&Session::onClose));
    }
}

void Session::send404() {
    beast::http::response<beast::http::string_body> res;

    res.version(req.version());
    res.result(beast::http::status::not_found);
    res.set(beast::http::field::content_type, "text/plain");
    res.body() = "404 Not Found";
    res.prepare_payload();

    return beast::http::async_write(ws.next_layer(), res, method_handler(&Session::on404Sent));
}

void Session::onClose(beast::error_code ec) {
    if (ec == boost::asio::error::operation_aborted) {
        return;
    }
    if (ec) {
        return fail(ec, "close");
    }
}
