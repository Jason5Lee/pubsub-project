#ifndef PUBSUB_BEAST_CPP_SESSION_HPP
#define PUBSUB_BEAST_CPP_SESSION_HPP

#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <memory>

#include "channel.hpp"
#include "channelhub.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
    template <typename Method> auto method_handler(Method method) {
        return beast::bind_front_handler(method, shared_from_this());
    }

public:
    explicit Session(boost::asio::ip::tcp::socket &&socket, int64_t pingDurationMs,
                     std::shared_ptr<ChannelHub> channelHub);

    ~Session();

    void run();

private:
    struct Message {
        bool isBinary;
        beast::flat_buffer content{};

        explicit Message(bool isBinary) : isBinary(isBinary) {}
    };

    static void fail(beast::error_code ec, char const *what);

    void onReadRequest(beast::error_code ec, std::size_t);
    void onAccept(beast::error_code ec);
    void sendPingDuration();
    void onPingDurationSent(beast::error_code ec, std::size_t);

    void doPublisher() { ws.async_read(receiveBuffer, method_handler(&Session::onReadPublish)); }

    void onReadPublish(beast::error_code ec, std::size_t);
    void sendMessage(std::shared_ptr<Message> message);
    void onMessageSent(beast::error_code ec, std::size_t);
    void setupTimer();
    void onTimer(beast::error_code ec);
    void onPing(beast::error_code ec);
    void send404();
    void on404Sent(beast::error_code ec, std::size_t) { onClose(ec); }
    void onClose(beast::error_code ec);

    websocket::stream<tcp::socket> ws;
    websocket::request_type req;
    net::steady_timer pingTimer;
    beast::flat_buffer receiveBuffer;
    int64_t pingDurationMs;
    bool isSub;

    std::shared_ptr<ChannelHub> channelHub;
    std::string channelName;
    Channel *channel;

    // Following fields are used if isSub.
    Channel::SubscriberIter subIter;
    std::mutex sendMsgMu;
    bool isSendingMessage;
    std::shared_ptr<Message> pendingMessage;
};

#endif // PUBSUB_BEAST_CPP_SESSION_HPP
