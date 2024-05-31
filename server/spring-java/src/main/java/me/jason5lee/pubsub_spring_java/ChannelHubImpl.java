package me.jason5lee.pubsub_spring_java;

import org.springframework.lang.NonNull;
import org.springframework.stereotype.Component;
import org.springframework.web.socket.WebSocketMessage;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

@Component
public class ChannelHubImpl implements ChannelHub {
    @Override
    public @NonNull SubscriberHandler subscribe(@NonNull String channelName, @NonNull Subscriber subscriber) {
        return new SubscriberHandlerImpl(channelName, connectChannel(channelName), subscriber);
    }

    @Override
    public @NonNull PublisherHandler addPublisher(@NonNull String channelName) {
        return new PublisherHandlerImpl(channelName, connectChannel(channelName));
    }

    private @NonNull Channel connectChannel(@NonNull String channelName) {
        return channelMap.compute(channelName, (name, ch) -> {
            if (ch == null) {
                return new Channel();
            }
            ch.incUsageCount();
            return ch;
        });
    }

    private void removeChannel(@NonNull String channelName, @NonNull Channel channel) {
        if (channel.decUsageCount()) {
            channelMap.compute(channelName, (name, ch) ->
            {
                assert ch == channel;
                // The usage count might increase between `decUsageCount` and `compute`, so we need to check it again.
                return ch.hasNoUsage() ? null : ch;
            });
        }
    }

    private final ConcurrentHashMap<String, Channel> channelMap = new ConcurrentHashMap<>();

    private static class SubscriberNode {
        SubscriberNode prev;
        SubscriberNode next;
        Subscriber subscriber;
    }

    private static class Channel {
        private final SubscriberNode sentinel = new SubscriberNode();
        private final ReadWriteLock lock = new ReentrantReadWriteLock();

        Channel() {
            sentinel.next = sentinel;
            sentinel.prev = sentinel;
        }

        void publish(WebSocketMessage<?> message) {
            Lock l = lock.readLock();
            l.lock();
            try {
                SubscriberNode node = sentinel.next;
                while (node != sentinel) {
                    node.subscriber.onMessage(message);
                    node = node.next;
                }
            } finally {
                l.unlock();
            }
        }

        SubscriberNode addSubscriber(Subscriber subscriber) {
            SubscriberNode newNode = new SubscriberNode();
            newNode.subscriber = subscriber;
            newNode.next = sentinel;

            Lock l = lock.writeLock();
            l.lock();
            try {
                newNode.prev = sentinel.prev;
                sentinel.prev.next = newNode;
                sentinel.prev = newNode;
            } finally {
                l.unlock();
            }

            return newNode;
        }

        void removeSubscriber(SubscriberNode subscriberNode) {
            Lock l = lock.writeLock();
            l.lock();
            try {
                subscriberNode.prev.next = subscriberNode.next;
                subscriberNode.next.prev = subscriberNode.prev;
            } finally {
                l.unlock();
            }
        }

        private final AtomicInteger usageCount = new AtomicInteger(1);

        public void incUsageCount() {
            if (usageCount.incrementAndGet() == 0) {
                throw new IllegalStateException("The usage count has unexpectedly been incremented to 0. This may be caused by either too many increments or more decrements being performed than increments.");
            }
        }

        public boolean decUsageCount() {
            int count = usageCount.decrementAndGet();
            if (count == -1) {
                throw new IllegalStateException("The usage count has unexpectedly been decremented from 0. This may be caused by either too many increments or more decrements being performed than increments.");
            }

            return count == 0;
        }

        public boolean hasNoUsage() {
            return usageCount.get() == 0;
        }
    }

    private class PublisherHandlerImpl implements PublisherHandler {
        private final String channelName;
        private final Channel channel;

        public PublisherHandlerImpl(String channelName, Channel channel) {
            this.channelName = channelName;
            this.channel = channel;
        }

        @Override
        public void publish(@NonNull WebSocketMessage<?> message) {
            channel.publish(message);
        }

        @Override
        public void remove() {
            ChannelHubImpl.this.removeChannel(channelName, channel);
        }
    }

    private class SubscriberHandlerImpl implements SubscriberHandler {
        private final String channelName;
        private final Channel channel;
        private final SubscriberNode subscriberNode;

        public SubscriberHandlerImpl(String channelName, Channel channel, Subscriber subscriber) {
            this.channelName = channelName;
            this.channel = channel;

            this.subscriberNode = channel.addSubscriber(subscriber);
        }

        @Override
        public void remove() {
            channel.removeSubscriber(subscriberNode);
            ChannelHubImpl.this.removeChannel(channelName, channel);
        }
    }
}
