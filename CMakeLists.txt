# CMakeLists.txt
#
# Copyright (c) 2023 Arkadiusz Bokowy
#
# This file is a part of wg-tcp-tunnel.
#
# This project is licensed under the terms of the MIT license.

cmake_minimum_required(VERSION 3.12)

project(
	wg-tcp-tunnel
	VERSION 1.0.0
	DESCRIPTION "WireGuard TCP tunneling"
	LANGUAGES CXX)

find_package(Boost 1.40.0 REQUIRED COMPONENTS log program_options)
find_package(Threads REQUIRED)

add_executable(wg-tcp-tunnel src/main.cpp src/tcp2udp.cpp src/udp2tcp.cpp)
target_compile_features(wg-tcp-tunnel PRIVATE cxx_std_17)

target_compile_definitions(wg-tcp-tunnel PRIVATE PROJECT_NAME="${PROJECT_NAME}")
target_compile_definitions(wg-tcp-tunnel PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

target_link_libraries(wg-tcp-tunnel PRIVATE Boost::log)
target_link_libraries(wg-tcp-tunnel PRIVATE Boost::program_options)
target_link_libraries(wg-tcp-tunnel PRIVATE Threads::Threads)

install(TARGETS wg-tcp-tunnel RUNTIME DESTINATION bin)
