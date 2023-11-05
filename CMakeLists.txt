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

add_executable(wg-tcp-tunnel src/main.cpp)
