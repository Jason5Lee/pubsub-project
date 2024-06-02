#ifndef PUBSUB_BEAST_CPP_CHANNELHUB_HPP
#define PUBSUB_BEAST_CPP_CHANNELHUB_HPP

#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "channel.hpp"

class ChannelHub {
public:
    ChannelHub() = default;

    Channel *connectChannel(const std::string &channelName);
    void disconnectChannel(const std::string &channelName, Channel *channel);

private:
    std::shared_mutex mu;
    std::unordered_map<std::string, Channel *> channels;
};

#endif // PUBSUB_BEAST_CPP_CHANNELHUB_HPP
