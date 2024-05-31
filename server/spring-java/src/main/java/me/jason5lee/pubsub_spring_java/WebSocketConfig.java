package me.jason5lee.pubsub_spring_java;

import org.springframework.beans.factory.annotation.Qualifier;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.task.TaskExecutor;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;
import org.springframework.web.socket.WebSocketHandler;
import org.springframework.web.socket.config.annotation.EnableWebSocket;
import org.springframework.web.socket.config.annotation.WebSocketConfigurer;
import org.springframework.web.socket.config.annotation.WebSocketHandlerRegistry;

@Configuration
@EnableWebSocket
public class WebSocketConfig implements WebSocketConfigurer {
    @Override
    public void registerWebSocketHandlers(WebSocketHandlerRegistry registry) {
        registry.addHandler(publisherHandler(), "/*/pub");
        registry.addHandler(subscriberHandler(), "/*/sub");
    }

    @Bean
    public ChannelHub channelHub() {
        return new ChannelHubImpl();
    }

    @Bean
    @Qualifier("publisherHandler")
    public WebSocketHandler publisherHandler() {
        return new PublisherHandler();
    }

    @Bean
    @Qualifier("subscriberHandler")
    public WebSocketHandler subscriberHandler() {
        return new SubscriberHandler();
    }

    @Bean
    @Qualifier("subscriber-task-executor")
    public TaskExecutor threadPoolTaskExecutor(
            @Value("${subscriber.thread-pool.core}") int corePoolSize,
            @Value("${subscriber.thread-pool.core}") int maxPoolSize
    ) {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(corePoolSize);
        executor.setMaxPoolSize(maxPoolSize);
        executor.setThreadNamePrefix("subscriber-thread");
        executor.initialize();
        return executor;
    }
}
