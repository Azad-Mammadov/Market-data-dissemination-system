cmake_minimum_required(VERSION 3.14)
project(MarketDataProject)

set(CMAKE_CXX_STANDARD 17)

# Find Protobuf and gRPC
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Include directories
include_directories(
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/generated
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/client
)

# Ensure the generated directory exists
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/generated)

# Proto files
set(PROTO_FILES
    ${CMAKE_SOURCE_DIR}/proto/marketdata.proto  # Updated path to the proto file
)

# Set gRPC C++ plugin path
set(gRPC_CPP_PLUGIN "C:/vcpkg/installed/x64-windows/tools/grpc/grpc_cpp_plugin.exe")

# Generate Protobuf and gRPC source files using protoc
foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    set(proto_src ${CMAKE_SOURCE_DIR}/generated/${proto_name}.pb.cc)
    set(proto_hdr ${CMAKE_SOURCE_DIR}/generated/${proto_name}.pb.h)
    set(grpc_src ${CMAKE_SOURCE_DIR}/generated/${proto_name}.grpc.pb.cc)
    set(grpc_hdr ${CMAKE_SOURCE_DIR}/generated/${proto_name}.grpc.pb.h)

    add_custom_command(
        OUTPUT ${proto_src} ${proto_hdr} ${grpc_src} ${grpc_hdr}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --grpc_out=${CMAKE_SOURCE_DIR}/generated
             --cpp_out=${CMAKE_SOURCE_DIR}/generated
             --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN}
             -I ${CMAKE_SOURCE_DIR}/proto  # Updated include path for the proto folder
             ${proto_file}
        DEPENDS ${proto_file}
    )

    list(APPEND GENERATED_SRC ${proto_src} ${grpc_src})
    list(APPEND GENERATED_HDR ${proto_hdr} ${grpc_hdr})
endforeach()

# Shared source files (e.g., OrderBook)
set(SHARED_SRC
    ${CMAKE_SOURCE_DIR}/server/orderbook.cpp
)

# Server target
add_executable(server
    ${CMAKE_SOURCE_DIR}/server/main.cpp
    ${SHARED_SRC}
    ${GENERATED_SRC}
)
target_link_libraries(server
    gRPC::grpc++
    optimized C:/vcpkg/installed/x64-windows/lib/libprotobuf.lib
    debug C:/vcpkg/installed/x64-windows/debug/lib/libprotobufd.lib
)

# Client target
add_executable(client
    ${CMAKE_SOURCE_DIR}/client/main.cpp
    ${GENERATED_SRC}
)
target_link_libraries(client
    gRPC::grpc++
    optimized C:/vcpkg/installed/x64-windows/lib/libprotobuf.lib
    debug C:/vcpkg/installed/x64-windows/debug/lib/libprotobufd.lib
)

# Print summary
message(STATUS "Protobuf include dirs: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf libraries: ${Protobuf_LIBRARIES}")
