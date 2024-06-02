#include "listener.hpp"

#include <boost/asio/strand.hpp>

Listener::Listener(net::io_context &ioc, tcp::endpoint endpoint, std::int64_t ping_duration_ms,
                   std::shared_ptr<ChannelHub> channels)
    : ioc_(ioc), acceptor_(ioc), ping_duration_ms(ping_duration_ms), channelHub(std::move(channels)) {
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

void Listener::do_accept() {
    // The new connection gets its own strand
    acceptor_.async_accept(net::make_strand(ioc_), beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
}

void Listener::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        fail(ec, "accept");
    } else {
        // Create the session and run it
        std::make_shared<Session>(std::move(socket), ping_duration_ms, channelHub)->run();
    }

    // Accept another connection
    do_accept();
}
