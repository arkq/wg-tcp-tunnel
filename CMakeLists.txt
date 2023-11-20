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

include(GNUInstallDirs)

option(ENABLE_SYSTEMD "Enable systemd support" OFF)
option(ENABLE_NGROK "Enable NGROK support" OFF)

find_package(Boost 1.40.0 REQUIRED COMPONENTS log log_setup program_options)

add_executable(wg-tcp-tunnel src/main.cpp src/tcp2udp.cpp src/udp2tcp.cpp)
target_compile_features(wg-tcp-tunnel PRIVATE cxx_std_17)

target_compile_definitions(wg-tcp-tunnel PRIVATE PROJECT_NAME="${PROJECT_NAME}")
target_compile_definitions(wg-tcp-tunnel PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")
target_compile_definitions(wg-tcp-tunnel PRIVATE ENABLE_SYSTEMD=$<BOOL:${ENABLE_SYSTEMD}>)
target_compile_definitions(wg-tcp-tunnel PRIVATE ENABLE_NGROK=$<BOOL:${ENABLE_NGROK}>)

target_link_libraries(wg-tcp-tunnel PRIVATE Boost::log)
target_link_libraries(wg-tcp-tunnel PRIVATE Boost::log_setup)
target_link_libraries(wg-tcp-tunnel PRIVATE Boost::program_options)

if(ENABLE_NGROK)
	find_package(OpenSSL REQUIRED)
	target_sources(wg-tcp-tunnel PRIVATE src/ngrok.cpp)
	target_link_libraries(wg-tcp-tunnel PRIVATE OpenSSL::SSL)
endif()

if(ENABLE_SYSTEMD)
	set(SYSTEMD_SYSTEM_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/systemd/system")
	install(FILES ${CMAKE_SOURCE_DIR}/misc/systemd/wg-tcp-tunnel.service
			DESTINATION ${SYSTEMD_SYSTEM_DIR})
endif()

install(TARGETS wg-tcp-tunnel DESTINATION ${CMAKE_INSTALL_BINDIR})
