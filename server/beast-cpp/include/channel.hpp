#ifndef PUBSUB_BEAST_CPP_CHANNEL_HPP
#define PUBSUB_BEAST_CPP_CHANNEL_HPP

#include <atomic>
#include <list>
#include <memory>
#include <shared_mutex>

class Session;

class Channel {
public:
    using SubscriberIter = std::list<std::weak_ptr<Session>>::const_iterator;

    SubscriberIter addSubscriber(std::weak_ptr<Session> subscriber);
    void removeSubscriber(SubscriberIter iter);

    template <typename F> void forEach(F f) {
        std::shared_lock lock(mu);

        for (auto &subscriber_weak : subscribers) {
            auto subscriber = subscriber_weak.lock();
            if (subscriber != nullptr) {
                f(subscriber);
            }
        }
    }

    void incUsageCount() { usageCount.fetch_add(1, std::memory_order_relaxed); }

    // Returns whether it *might* have no usage.
    bool decUsageCount() { return usageCount.fetch_sub(1, std::memory_order_release) == 1; }

    [[nodiscard]] bool hasNoUsage() const { return usageCount.load(std::memory_order_acquire) == 0; }

private:
    std::shared_mutex mu;
    std::list<std::weak_ptr<Session>> subscribers;
    std::atomic<std::uint64_t> usageCount{1};
};

#endif // PUBSUB_BEAST_CPP_CHANNEL_HPP
