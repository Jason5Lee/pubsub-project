#include "channelhub.hpp"
#include <mutex>

Channel *ChannelHub::connectChannel(const std::string &channelName) {
    std::unordered_map<std::string, Channel *>::const_iterator it;

    {
        std::shared_lock lock(mu);
        it = channels.find(channelName);
        if (it != channels.cend()) {
            it->second->incUsageCount();
            return it->second;
        }
    }

    std::unique_lock lock(mu);
    it = channels.find(channelName);
    if (it == channels.cend()) {
        it = channels.emplace(channelName, new Channel()).first;
    } else {
        it->second->incUsageCount();
    }
    return it->second;
}

void ChannelHub::disconnectChannel(const std::string &channelName, Channel *channel) {
    if (channel->decUsageCount()) {
        std::unique_lock lock(mu);
        if (channel->hasNoUsage()) {
            channels.erase(channelName);
            delete channel;
        }
    }
}