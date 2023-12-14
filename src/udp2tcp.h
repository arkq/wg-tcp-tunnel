// udp2tcp.h
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "ngrok.h"
#include "utils.hpp"

namespace wg {
namespace tunnel {

namespace asio = boost::asio;
using std::size_t;

class udp2tcp_dest_provider {
public:
	virtual asio::ip::tcp::endpoint tcp_dest_ep() = 0;
};

class udp2tcp {
public:
	udp2tcp(asio::io_context & ioc, asio::ip::udp::endpoint ep_udp_acc,
	        udp2tcp_dest_provider & ep_tcp_dest_provider)
	    : m_ep_udp_acc(std::move(ep_udp_acc)), m_socket_udp_acc(ioc, m_ep_udp_acc),
	      m_socket_tcp_dest(ioc), m_ep_tcp_dest_provider(ep_tcp_dest_provider),
	      m_app_keep_alive_timer(ioc) {}
	~udp2tcp() = default;

	void run(utils::transport transport);

	void keep_alive_app(unsigned int idle_time) { m_app_keep_alive_idle_time = idle_time; }
	void keep_alive_tcp(unsigned int idle_time) { m_tcp_keep_alive_idle_time = idle_time; }

private:
	std::string to_string(bool verbose = false);

	void do_connect();
	void do_connect_handler(const boost::system::error_code & ec);

	void do_app_keep_alive_init();
	void do_app_keep_alive(bool init = false);
	void do_app_keep_alive_handler(const boost::system::error_code & ec);

	void do_send();
	void do_send_buffer();
	void do_send_handler(const boost::system::error_code & ec, size_t length);

	void do_recv_init();
	void do_recv(size_t rlen, bool ctrl = false);
	void do_recv_handler(const boost::system::error_code & ec, size_t length, bool ctrl);

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
	unsigned int m_app_keep_alive_idle_time = 0;
	asio::deadline_timer m_app_keep_alive_timer;
	// TCP keep-alive idle time in seconds, 0 to disable
	unsigned int m_tcp_keep_alive_idle_time = 0;
	// Buffers for sending and receiving data
	std::array<char, 4096> m_buffer_send;
	size_t m_buffer_send_length;
	asio::streambuf m_buffer_recv;
};

class udp2tcp_dest_provider_simple : virtual public udp2tcp_dest_provider {
public:
	udp2tcp_dest_provider_simple(asio::ip::tcp::endpoint ep) : m_ep(std::move(ep)) {}
	asio::ip::tcp::endpoint tcp_dest_ep() override { return m_ep; }

private:
	asio::ip::tcp::endpoint m_ep;
};

#if ENABLE_NGROK
class udp2tcp_dest_provider_ngrok : virtual public udp2tcp_dest_provider {
public:
	udp2tcp_dest_provider_ngrok(wg::ngrok::client & client) : m_client(client) {}
	asio::ip::tcp::endpoint tcp_dest_ep() override;

	void filter_id(const std::string_view id) { m_endpoint_filter_id = id; }
	void filter_uri(const std::string_view uri) { m_endpoint_filter_uri = uri; }

private:
	wg::ngrok::client & m_client;
	std::string m_endpoint_filter_id;
	std::string m_endpoint_filter_uri;
};
#endif

}; // namespace tunnel
}; // namespace wg
