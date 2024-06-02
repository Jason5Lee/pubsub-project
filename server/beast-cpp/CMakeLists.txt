cmake_minimum_required(VERSION 3.10)
project(PubSubProject)

set(CMAKE_CXX_STANDARD 17)

# Boost
find_package(Boost REQUIRED COMPONENTS system thread)
include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Server
add_executable(pubsub-server src/server.cpp
        src/channel.cpp
        include/session.hpp
        src/session.cpp
        include/channelhub.hpp
        src/channelhub.cpp
        include/listener.hpp
        src/listener.cpp)

target_link_libraries(pubsub-server ${Boost_LIBRARIES})

# If you have pthread in your system, link it as well
find_package(Threads REQUIRED)
if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(PUBLIC WebSocketServer "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(WebSocketServer "${CMAKE_THREAD_LIBS_INIT}")
endif()
