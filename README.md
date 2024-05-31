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

## Old (C++ Server)

When a connection is established, the client should send a message that starts with a byte indicating the role: 0 for publishing or 1 for subscribing, followed by the channel name.

The server will respond with a message containing the ping duration in milliseconds, represented as a little-endian 64-bit integer.

The publisher client can then send messages to be broadcast, which will be received by all subscriber connections.

## New (Rest)

WebSocket URL: `/<channel-name>/pub` for publisher and `/<channel-name>/sub` for subscriber.

The server will respond with a message containing the ping duration in milliseconds, represented as hex.

The publisher client can then send messages to be broadcast, which will be received by all subscriber connections.

It exhibits at-most-once message delivery semantics.
