package me.jason5lee.pubsub_spring_java;

import org.springframework.lang.NonNull;
import org.springframework.web.socket.WebSocketMessage;

public interface Subscriber {
    void onMessage(@NonNull WebSocketMessage<?> message);
}
