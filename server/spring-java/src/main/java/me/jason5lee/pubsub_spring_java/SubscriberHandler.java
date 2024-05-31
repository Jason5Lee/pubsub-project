package me.jason5lee.pubsub_spring_java;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.task.TaskExecutor;
import org.springframework.lang.NonNull;
import org.springframework.web.socket.*;

import java.io.IOException;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicIntegerFieldUpdater;
import java.util.concurrent.atomic.AtomicReferenceFieldUpdater;

public class SubscriberHandler implements WebSocketHandler {
    private static final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
    private static final String HANDLER_STATE = "handlerState";
    private static final Logger logger = LoggerFactory.getLogger(SubscriberHandler.class);

    @Value("${ping.duration-ms}")
    private long pingDurationMs;
    @Autowired
    private ChannelHub channelHub;
    @Autowired
    @Qualifier("subscriber-task-executor")
    private TaskExecutor taskExecutor;

    @Override
    public void afterConnectionEstablished(@NonNull WebSocketSession session) throws Exception {
        java.net.URI uri = session.getUri();
        assert uri != null;
        String channelName = extractChannelName(uri.toString());
        session.sendMessage(new TextMessage(Long.toHexString(pingDurationMs)));
        State state = new State(session, pingDurationMs, taskExecutor);
        state.startConnection(channelHub.subscribe(channelName, state));
        putState(session, state);
    }

    @Override
    public void handleMessage(@NonNull WebSocketSession session, @NonNull WebSocketMessage<?> message) throws Exception {
        // Ignore incoming message in subscriber
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
        int indexEnd = uri.lastIndexOf("/sub");
        int indexStart = uri.lastIndexOf('/', indexEnd - 1) + 1;
        return uri.substring(indexStart, indexEnd);
    }

    private static class State implements Subscriber {
        ScheduledFuture<?> pingFuture;
        final WebSocketSession session;
        final long pingDurationMs;
        ChannelHub.SubscriberHandler handler;
        final TaskExecutor subscriberExecutor;

        // 0 - idle
        // 1 - sending message, no pending message
        // >= 2 - sending message, has pending message
        private volatile int state = 0;
        private final static AtomicIntegerFieldUpdater<State> stateUpdater =
                AtomicIntegerFieldUpdater.newUpdater(State.class, "state");

        private volatile WebSocketMessage<?> pendingMessage = null;
        private final static AtomicReferenceFieldUpdater<State, WebSocketMessage> pendingMessageUpdater = // Using `WebSocketMessage<?>` here will fail compilation
                AtomicReferenceFieldUpdater.newUpdater(State.class, WebSocketMessage.class, "pendingMessage");

        static final Logger logger = LoggerFactory.getLogger(State.class);

        State(WebSocketSession session, long pingDurationMs, TaskExecutor subscriberExecutor) {
            this.session = session;
            this.pingDurationMs = pingDurationMs;
            this.subscriberExecutor = subscriberExecutor;
        }

        void startConnection(ChannelHub.SubscriberHandler handler) {
            this.handler = handler;
            startTimer();
        }

        void stopConnection() {
            stopTimer();
            handler.remove();
        }

        private void startTimer() {
            pingFuture = scheduler.scheduleAtFixedRate(() -> {
                try {
                    if (session.isOpen()) {
                        session.sendMessage(new PingMessage());
                    }
                } catch (IOException ePing) {
                    logger.error("Failed to send ping", ePing);
                    stopConnection();
                    try {
                        session.close(CloseStatus.SESSION_NOT_RELIABLE);
                    } catch (IOException eClose) {
                        logger.error("Failed to close", eClose);
                    }
                }
            }, pingDurationMs, pingDurationMs, TimeUnit.MILLISECONDS);
        }

        private void stopTimer() {
            pingFuture.cancel(false);
        }

        @Override
        public void onMessage(@NonNull WebSocketMessage<?> message) {
            // Since we use at-most-one guarantee, new pending message override the old.
            pendingMessageUpdater.set(this, message);
            if (stateUpdater.incrementAndGet(this) == 1) {
                // Idle -> Running
                subscriberExecutor.execute(() -> {
                    WebSocketMessage<?> msg = null;
                    while (true) {
                        msg = pendingMessageUpdater.getAndSet(this, null);
                        if (msg != null) {
                            try {
                                session.sendMessage(msg);
                            } catch (IOException e) {
                                logger.error("Failed to send message", e);
                            }
                        }
                        if (stateUpdater.compareAndSet(this, 1, 0)) {
                            // If no pending message, go back to idle.
                            break;
                        } else {
                            // Pending message is going to be processed.
                            stateUpdater.set(this, 1);
                        }
                    }
                });
            }
        }
    }
}
