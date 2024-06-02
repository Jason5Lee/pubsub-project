#ifndef PUBSUB_BEAST_CPP_LISTENER_HPP
#define PUBSUB_BEAST_CPP_LISTENER_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <memory>

#include "session.hpp"

class ChannelHub;

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(net::io_context &ioc, tcp::endpoint endpoint, std::int64_t ping_duration_ms,
             std::shared_ptr<ChannelHub> channels);

    // Start accepting incoming connections
    void run() { do_accept(); }

private:
    static void fail(beast::error_code ec, char const *what) { std::cerr << what << ": " << ec.message() << "\n"; }
    void do_accept();

    void on_accept(beast::error_code ec, tcp::socket socket);

    net::io_context &ioc_;
    tcp::acceptor acceptor_;
    std::int64_t ping_duration_ms;
    std::shared_ptr<ChannelHub> channelHub;
};

#endif // PUBSUB_BEAST_CPP_LISTENER_HPP
