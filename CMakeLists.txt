# CMakeLists.txt
#
# Copyright (c) 2023-2025 Arkadiusz Bokowy
#
# This file is a part of wg-tcp-tunnel.
#
# This project is licensed under the terms of the MIT license.

cmake_minimum_required(VERSION 3.22)

project(
	wg-tcp-tunnel
	VERSION 1.1.0
	DESCRIPTION "WireGuard TCP tunneling"
	LANGUAGES CXX)

if(WIN32)
	enable_language(CSharp)
endif()

include(GNUInstallDirs)

option(ENABLE_NGROK "Enable NGROK support" OFF)
option(ENABLE_SYSTEMD "Enable systemd support" OFF)
option(ENABLE_WEBSOCKET "Enable WebSocket support" OFF)

add_compile_definitions(PROJECT_NAME="${PROJECT_NAME}")
add_compile_definitions(PROJECT_VERSION="${PROJECT_VERSION}")
add_compile_definitions(ENABLE_NGROK=$<BOOL:${ENABLE_NGROK}>)
add_compile_definitions(ENABLE_SYSTEMD=$<BOOL:${ENABLE_SYSTEMD}>)
add_compile_definitions(ENABLE_WEBSOCKET=$<BOOL:${ENABLE_WEBSOCKET}>)

if(WIN32)
	# NOTE: The selected Windows version needs to match the version against
	#       which the Boost library was compiled. Otherwise, application will
	#       not link properly.
	add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

find_package(Boost 1.40.0 REQUIRED COMPONENTS log log_setup program_options)

add_executable(wg-tcp-tunnel src/main.cpp src/tcp2udp.cpp src/udp2tcp.cpp)
target_compile_features(wg-tcp-tunnel PRIVATE cxx_std_17)

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

if(WIN32)
	add_executable(
		wg-tcp-tunnel-wintray
		src/wintray.cs
		src/wintray.ico)
	set_source_files_properties(
		src/wintray.ico
		PROPERTIES
		# Embed the icon as a .NET resource
		VS_TOOL_OVERRIDE "EmbeddedResource")
	set_target_properties(
		wg-tcp-tunnel-wintray
		PROPERTIES
		VS_DOTNET_REFERENCES "Microsoft.CSharp;System;System.Core;System.Drawing;System.Windows.Forms"
		WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS wg-tcp-tunnel DESTINATION ${CMAKE_INSTALL_BINDIR})
