#include "channel.hpp"
#include <mutex>

Channel::SubscriberIter Channel::addSubscriber(std::weak_ptr<Session> subscriber) {
    std::unique_lock lock(mu);

    subscribers.push_back(std::move(subscriber));
    return std::prev(subscribers.end());
}

void Channel::removeSubscriber(SubscriberIter iter) {
    std::unique_lock lock(mu);

    subscribers.erase(iter);
}
