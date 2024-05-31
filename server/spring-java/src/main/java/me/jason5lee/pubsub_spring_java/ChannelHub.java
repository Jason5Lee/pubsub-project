package me.jason5lee.pubsub_spring_java;

import org.springframework.lang.NonNull;
import org.springframework.web.socket.WebSocketMessage;

public interface ChannelHub {
    @NonNull
    SubscriberHandler subscribe(@NonNull String channelName, @NonNull Subscriber subscriber);

    interface SubscriberHandler {
        void remove();
    }

    @NonNull
    PublisherHandler addPublisher(@NonNull String channelName);

    interface PublisherHandler {
        void publish(@NonNull WebSocketMessage<?> message);

        void remove();
    }
}
