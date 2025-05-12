cmake_minimum_required(VERSION 3.14)
project(MarketDataProject)

set(CMAKE_CXX_STANDARD 17)

# gRPC & protobuf
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

include_directories(
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Proto files
find_program(PROTOC_EXECUTABLE protoc)
set(PROTO_FILES marketdata.proto)
foreach(proto ${PROTO_FILES})
    get_filename_component(proto_name ${proto} NAME_WE)
    add_custom_command(
        OUTPUT generated/${proto_name}.pb.cc generated/${proto_name}.grpc.pb.cc
        COMMAND ${PROTOC_EXECUTABLE}
        ARGS --grpc_out=generated --cpp_out=generated
             --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
             -I ${CMAKE_CURRENT_SOURCE_DIR} ${proto}
        DEPENDS ${proto}
    )
endforeach()

set(PROTO_SRC
    generated/marketdata.pb.cc
    generated/marketdata.grpc.pb.cc
)

# Shared source (e.g., OrderBook)
set(SHARED_SRC
    server/orderbook.cpp
)

# Server
add_executable(server
    server/main.cpp
    ${SHARED_SRC}
    ${PROTO_SRC}
)
target_link_libraries(server
    gRPC::grpc++
    ${Protobuf_LIBRARIES}
)

# Client
add_executable(client
    client/main.cpp
    ${PROTO_SRC}
)
target_link_libraries(client
    gRPC::grpc++
    ${Protobuf_LIBRARIES}
)
