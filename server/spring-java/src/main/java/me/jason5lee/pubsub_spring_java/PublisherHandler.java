package me.jason5lee.pubsub_spring_java;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.lang.NonNull;
import org.springframework.web.socket.*;

import java.io.IOException;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

public class PublisherHandler implements WebSocketHandler {
    private static final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
    private static final String HANDLER_STATE = "handlerState";
    private static final Logger logger = LoggerFactory.getLogger(PublisherHandler.class);

    @Value("${ping.duration-ms}")
    private long pingDurationMs;
    @Autowired
    private ChannelHub channelHub;

    @Override
    public void afterConnectionEstablished(@NonNull WebSocketSession session) throws Exception {
        java.net.URI uri = session.getUri();
        assert uri != null;
        String channelName = extractChannelName(uri.toString());
        session.sendMessage(new TextMessage(Long.toHexString(pingDurationMs)));
        ChannelHub.PublisherHandler handler = channelHub.addPublisher(channelName);
        putState(session, new State(session, pingDurationMs, handler));
    }

    @Override
    public void handleMessage(@NonNull WebSocketSession session, @NonNull WebSocketMessage<?> message) throws Exception {
        getState(session).publishMessage(message);
    }

    @Override
    public void handleTransportError(@NonNull WebSocketSession session, @NonNull Throwable exception) throws Exception {
        logger.error("transport error", exception);
    }

    @Override
    public void afterConnectionClosed(@NonNull WebSocketSession session, @NonNull CloseStatus status) throws Exception {
        getState(session).stopConnection();
        logger.info("connection closed, status: {}", status);
    }

    @Override
    public boolean supportsPartialMessages() {
        return false;
    }

    private static void putState(@NonNull WebSocketSession session, @NonNull State state) {
        session.getAttributes().put(HANDLER_STATE, state);
    }

    private static @NonNull State getState(@NonNull WebSocketSession session) {
        return (State) session.getAttributes().get(HANDLER_STATE);
    }

    private static String extractChannelName(@NonNull String uri) {
        int indexEnd = uri.lastIndexOf("/pub");
        int indexStart = uri.lastIndexOf('/', indexEnd - 1) + 1;
        return uri.substring(indexStart, indexEnd);
    }

    private static class State {
        ScheduledFuture<?> pingFuture;
        final WebSocketSession session;
        final long pingDurationMs;
        final ChannelHub.PublisherHandler handler;

        static final Logger logger = LoggerFactory.getLogger(State.class);

        State(WebSocketSession session, long pingDurationMs, ChannelHub.PublisherHandler handler) {
            this.session = session;
            this.pingDurationMs = pingDurationMs;
            this.handler = handler;
            startTimer();
        }

        void publishMessage(WebSocketMessage<?> message) {
            handler.publish(message);
        }

        void stopConnection() {
            stopTimer();
            handler.remove();
        }

        private void startTimer() {
            pingFuture = scheduler.scheduleAtFixedRate(() -> {
                try {
                    synchronized (session) {
                        if (session.isOpen()) {
                            session.sendMessage(new PingMessage());
                        }
                    }
                } catch (IOException ePing) {
                    logger.error("Failed to send ping", ePing);
                    stopConnection();
                    try {
                        synchronized (session) {
                            session.close(CloseStatus.SESSION_NOT_RELIABLE);
                        }
                    } catch (IOException eClose) {
                        logger.error("Failed to close", eClose);
                    }
                }
            }, pingDurationMs, pingDurationMs, TimeUnit.MILLISECONDS);
        }

        private void stopTimer() {
            pingFuture.cancel(false);
        }
    }
}
