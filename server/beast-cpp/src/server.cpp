#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/format.hpp>
#include <cstddef>
#include <iostream>
#include <list>
#include <memory>
#include <shared_mutex>
#include <thread>

#include "channelhub.hpp"
#include "listener.hpp"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;

using tcp = boost::asio::ip::tcp;

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

    auto channels = std::make_shared<ChannelHub>();

    // The io_context is required for all I/O
    net::io_context ioc{threads};

    // Create and launch a listening port
    std::make_shared<Listener>(ioc, tcp::endpoint{address, port}, ping_duration_ms, std::move(channels))->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    ioc.run();

    return EXIT_SUCCESS;
}
