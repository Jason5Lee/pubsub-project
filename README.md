# PubSub Project

Publish messages to a channel and broadcast them to all subscribers.

## Implementations

- Server
    - [Beast C++](./server/beast-cpp/)
    - [Spring Java](./server/spring-java/)
- Client
    - [TypeScript](./client/typescript/)

## Protocol

The client and server communicate via WebSocket.

WebSocket URLs:
- Publisher: `/<channel-name>/pub`
- Subscriber: `/<channel-name>/sub`

The server will respond with a message containing the ping duration in milliseconds, represented as a hexadecimal value. After this duration has passed since the last message was sent or received, the server will send a ping frame to the client.

The publisher client can send messages to be broadcast, which will be received by all subscriber connections.

The system uses at-most-once message delivery semantics.
