#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/endian/conversion.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;

using tcp = boost::asio::ip::tcp;

class Session;

class Subscribers {
public:
    using SubscriberIter = std::list<std::weak_ptr<Session>>::const_iterator;

    SubscriberIter add(std::weak_ptr<Session> subscriber) {
        std::unique_lock lock(mu);

        subscribers.push_back(std::move(subscriber));
        return std::prev(subscribers.end());
    }

    void remove(SubscriberIter iter) {
        std::unique_lock lock(mu);

        subscribers.erase(iter);
    }

    template <typename F> void for_each(F f) {
        std::shared_lock lock(mu);

        for (auto &subscription : subscribers) {
            f(subscription);
        }
    }

private:
    std::shared_mutex mu;
    std::list<std::weak_ptr<Session>> subscribers;
};

class Channels {
public:
    Channels() = default;

    std::shared_ptr<Subscribers>
    getSubscribers(const std::string &channel_name) {
        std::unordered_map<std::string,
                           std::shared_ptr<Subscribers>>::const_iterator it;

        {
            std::shared_lock lock(mu);
            it = channels.find(channel_name);
            if (it != channels.cend()) {
                return it->second;
            }
        }

        std::unique_lock lock(mu);
        it = channels.find(channel_name);
        if (it == channels.cend()) {
            it = channels.emplace(channel_name, std::make_shared<Subscribers>())
                     .first;
        }
        return it->second;
    }

private:
    std::shared_mutex mu;
    std::unordered_map<std::string, std::shared_ptr<Subscribers>> channels;
};

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket &&socket, int64_t ping_duration_ms,
                     std::shared_ptr<Channels> channels)
        : ws(std::move(socket)), ping_timer(ws.get_executor()),
          ping_duration_ms(ping_duration_ms), subscribers(nullptr),
          channels(std::move(channels)), isPub(false) {
        ws.set_option(websocket::stream_base::timeout::suggested(
            beast::role_type::server));
        ws.text(false);
    }

    ~Session() {
        if (!isPub) {
            subscribers->remove(iter);
        }
    }

    void run() {
        ws.async_accept(
            beast::bind_front_handler(&Session::onAccept, shared_from_this()));
    }

private:
    void fail(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << "\n";
        ping_timer.cancel();
    }

    void onAccept(beast::error_code ec) {
        if (ec) {
            return fail(ec, "accept");
        }

        ping_timer.expires_after(std::chrono::milliseconds(ping_duration_ms));
        ping_timer.async_wait(
            beast::bind_front_handler(&Session::onTimer, shared_from_this()));

        ws.async_read(buffer, beast::bind_front_handler(&Session::onFirstRead,
                                                        shared_from_this()));
    }

    void onFirstRead(beast::error_code ec, std::size_t bytes_read) {
        if (ec) {
            return fail(ec, "read");
        }

        auto buf_size = buffer.size();
        if (buf_size <= 1) {
            return ws.async_close(websocket::close_code::bad_payload,
                                  beast::bind_front_handler(
                                      &Session::onClose, shared_from_this()));
        }
        auto buf = boost::asio::buffer_cast<const char *>(buffer.data());
        if (buf[0] == 0) {
            isPub = true;
        } else if (buf[0] != 1) {
            return ws.async_close(websocket::close_code::bad_payload,
                                  beast::bind_front_handler(
                                      &Session::onClose, shared_from_this()));
        }
        // auto firstByte = boost::asio::buffer_cast<const unsigned
        // char*>(buffer.data());
        std::string channel_name = std::string(buf + 1, buf_size - 1);
        buffer.consume(buffer.size());

        subscribers = channels->getSubscribers(channel_name);
        if (!isPub) {
            iter = subscribers->add(shared_from_this());
        }

        sendPingDuration();
    }

    void sendPingDuration() {
        auto duration_bytes = boost::endian::native_to_little(ping_duration_ms);
        ws.async_write(
            boost::asio::buffer(&duration_bytes, sizeof(duration_bytes)),
            beast::bind_front_handler(&Session::onPingDurationSent,
                                      shared_from_this()));
    }

    void onPingDurationSent(beast::error_code ec,
                            std::size_t bytes_transferred) {
        if (ec)
            return fail(ec, "write");
        if (isPub) {
            doPublisher();
        }
    }

    void doPublisher() {
        ws.async_read(buffer, beast::bind_front_handler(&Session::onReadPublish,
                                                        shared_from_this()));
    }

    void onReadPublish(beast::error_code ec, std::size_t bytes_read) {
        if (ec) {
            return fail(ec, "read");
        }
        if (buffer.size() == 0) {
            return ws.async_close(websocket::close_code::bad_payload,
                                  beast::bind_front_handler(
                                      &Session::onClose, shared_from_this()));
        }

        std::string message = boost::beast::buffers_to_string(buffer.data());
        buffer.consume(buffer.size());

        subscribers->for_each([&message](std::weak_ptr<Session> subscriber) {
            subscriber.lock()->sendMessage(message);
        });

        doPublisher();
    }

    void sendMessage(const std::string &message) {
        ws.async_write(boost::asio::buffer(message),
                       beast::bind_front_handler(&Session::onMessageSent,
                                                 shared_from_this()));
    }

    void onMessageSent(beast::error_code ec, std::size_t bytes_transferred) {
        if (ec) {
            return fail(ec, "write");
        }
    }

    void onTimer(beast::error_code ec) {
        if (ec) {
            fail(ec, "timer");
            ws.async_close(websocket::close_code::internal_error,
                           beast::bind_front_handler(&Session::onClose,
                                                     shared_from_this()));
            return;
        }

        ping_timer.expires_after(std::chrono::milliseconds(ping_duration_ms));
        ping_timer.async_wait(
            beast::bind_front_handler(&Session::onTimer, shared_from_this()));

        ws.async_ping({}, beast::bind_front_handler(&Session::onPing,
                                                    shared_from_this()));
    }

    void onClose(beast::error_code ec) {
        if (ec)
            return fail(ec, "close");
    }

    void onPing(beast::error_code ec) {
        if (ec)
            return fail(ec, "ping");
    }

    websocket::stream<tcp::socket> ws;
    net::steady_timer ping_timer;
    beast::flat_buffer buffer;
    int64_t ping_duration_ms;
    bool isPub;

    std::shared_ptr<Channels> channels;
    std::shared_ptr<Subscribers> subscribers;
    // Valid if !isPub.
    Subscribers::SubscriberIter iter;
};

class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(net::io_context &ioc, tcp::endpoint endpoint,
             std::int64_t ping_duration_ms, std::shared_ptr<Channels> channels)
        : ioc_(ioc), acceptor_(ioc), ping_duration_ms(ping_duration_ms),
          channels(std::move(channels)) {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void run() { do_accept(); }

private:
    void fail(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }

    void do_accept() {
        // The new connection gets its own strand
        acceptor_.async_accept(net::make_strand(ioc_),
                               beast::bind_front_handler(&Listener::on_accept,
                                                         shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            fail(ec, "accept");
        } else {
            // Create the session and run it
            std::make_shared<Session>(std::move(socket), ping_duration_ms,
                                      channels)
                ->run();
        }

        // Accept another connection
        do_accept();
    }

    net::io_context &ioc_;
    tcp::acceptor acceptor_;
    std::int64_t ping_duration_ms;
    std::shared_ptr<Channels> channels;
};

int main(int argc, char *argv[]) {
    // Check command line arguments.
    if (argc != 5) {
        std::cerr << "Usage: pubsub-server <address> <port> <threads> "
                     "<ping duration in milliseconds>\n"
                  << "Example:\n"
                  << "    pubsub-server 0.0.0.0 8080 1 60000\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));
    auto const ping_duration_ms = std::max<int>(1, std::atoi(argv[4]));

    auto channels = std::make_shared<Channels>();

    // The io_context is required for all I/O
    net::io_context ioc{threads};

    // Create and launch a listening port
    std::make_shared<Listener>(ioc, tcp::endpoint{address, port},
                               ping_duration_ms, std::move(channels))
        ->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    ioc.run();

    return EXIT_SUCCESS;
}
