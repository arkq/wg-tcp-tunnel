// wg-tcp-tunnel - udp2tcp.h
// SPDX-FileCopyrightText: 2023-2025 Arkadiusz Bokowy and contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#if ENABLE_WEBSOCKET
#	include <boost/beast/websocket.hpp>
#endif

#include "ngrok.h"
#include "utils.hpp"

namespace wg::tunnel {

namespace asio = boost::asio;
#if ENABLE_WEBSOCKET
namespace beast = boost::beast;
namespace ws = beast::websocket;
#endif
using std::size_t;

class udp2tcp_dest_provider {
public:
	virtual auto tcp_dest_ep() -> asio::ip::tcp::endpoint = 0;
};

class udp2tcp {
public:
	udp2tcp(asio::io_context & ioc, asio::ip::udp::endpoint ep_udp_acc,
	        udp2tcp_dest_provider & ep_tcp_dest_provider)
	    : m_ep_udp_acc(std::move(ep_udp_acc)), m_socket_udp_acc(ioc, m_ep_udp_acc),
	      m_socket_tcp_dest(ioc), m_ep_tcp_dest_provider(ep_tcp_dest_provider),
	      m_app_keep_alive_timer(ioc) {}
	~udp2tcp() = default;

	auto run(utils::transport transport) -> void;

	auto keep_alive_app(int idle_time) -> void { m_app_keep_alive_idle_time = idle_time; }
	auto keep_alive_tcp(int idle_time) -> void { m_tcp_keep_alive_idle_time = idle_time; }
#if ENABLE_WEBSOCKET
	auto ws_headers(utils::http::headers headers) { m_ws_headers = std::move(headers); }
#endif

private:
	auto to_string(bool verbose = false) -> std::string;

	auto do_connect() -> void;
	auto do_connect_handler(const boost::system::error_code & ec) -> void;

	auto do_app_keep_alive_init() -> void;
	auto do_app_keep_alive(bool init = false) -> void;
	auto do_app_keep_alive_handler(const boost::system::error_code & ec) -> void;

	auto do_send() -> void;
	auto do_send_buffer() -> void;
	auto do_send_handler(const boost::system::error_code & ec, size_t length) -> void;

	auto do_recv_init() -> void;
	auto do_recv(size_t rlen, bool ctrl = false) -> void;
	auto do_recv_handler(const boost::system::error_code & ec, size_t length, bool ctrl) -> void;

#if ENABLE_WEBSOCKET
	auto do_ws_recv() -> void;
	auto do_ws_recv_handler(const boost::system::error_code & ec, size_t length) -> void;
#endif

	asio::ip::udp::endpoint m_ep_udp_acc;
	asio::ip::udp::endpoint m_ep_udp_sender;
	asio::ip::udp::socket m_socket_udp_acc;
	asio::ip::tcp::socket m_socket_tcp_dest;
	// Provider for obtaining TCP destination endpoint
	udp2tcp_dest_provider & m_ep_tcp_dest_provider;
	asio::ip::tcp::endpoint m_ep_tcp_dest_cache;
	// Transport protocol used for the TCP connection
	utils::transport m_transport = utils::transport::raw;
	// Application keep-alive idle time in seconds, 0 to disable
	int m_app_keep_alive_idle_time = 0;
	asio::deadline_timer m_app_keep_alive_timer;
	// TCP keep-alive idle time in seconds, 0 to disable
	int m_tcp_keep_alive_idle_time = 0;
	// Buffers for sending and receiving data
	std::array<char, 4096> m_buffer_send;
	size_t m_buffer_send_length;
	asio::streambuf m_buffer_recv;
#if ENABLE_WEBSOCKET
	ws::stream<asio::ip::tcp::socket &> m_ws{ m_socket_tcp_dest };
	beast::flat_buffer m_ws_buffer_recv;
	// List of WebSocket custom headers used during the handshake
	utils::http::headers m_ws_headers;
#endif
};

class udp2tcp_dest_provider_simple : virtual public udp2tcp_dest_provider {
public:
	udp2tcp_dest_provider_simple(asio::ip::tcp::endpoint ep) : m_ep(std::move(ep)) {}
	auto tcp_dest_ep() -> asio::ip::tcp::endpoint override { return m_ep; }

private:
	asio::ip::tcp::endpoint m_ep;
};

#if ENABLE_NGROK
class udp2tcp_dest_provider_ngrok : virtual public udp2tcp_dest_provider {
public:
	udp2tcp_dest_provider_ngrok(wg::ngrok::client & client) : m_client(client) {}
	auto tcp_dest_ep() -> asio::ip::tcp::endpoint override;

	auto filter_id(const std::string_view id) -> void { m_endpoint_filter_id = id; }
	auto filter_uri(const std::string_view uri) -> void { m_endpoint_filter_uri = uri; }

private:
	wg::ngrok::client & m_client;
	std::string m_endpoint_filter_id;
	std::string m_endpoint_filter_uri;
};
#endif

}; // namespace wg::tunnel
