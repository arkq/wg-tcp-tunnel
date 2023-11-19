// udp2tcp.h
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "utils.hpp"

namespace wg {
namespace tunnel {

namespace asio = boost::asio;
using std::size_t;

class udp2tcp {
public:
	udp2tcp(asio::io_context & ioc, const asio::ip::udp::endpoint & ep_udp_acc,
	        const asio::ip::tcp::endpoint & ep_tcp_dest)
	    : m_ep_udp_acc(ep_udp_acc), m_ep_tcp_dest(ep_tcp_dest),
	      m_socket_udp_acc(ioc, m_ep_udp_acc), m_socket_tcp_dest(ioc),
	      m_app_keep_alive_timer(ioc) {}
	~udp2tcp() = default;

	void init();

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
	void do_send_handler(const boost::system::error_code & ec, utils::ip::udp::buffer::ptr buffer,
	                     size_t length);
	void send(utils::ip::udp::buffer::ptr buffer, size_t length);

	void do_recv_init();
	void do_recv(size_t rlen, bool ctrl = false);
	void do_recv_handler(const boost::system::error_code & ec, utils::ip::tcp::buffer::ptr buffer,
	                     size_t length, bool ctrl);

	asio::ip::udp::endpoint m_ep_udp_acc;
	asio::ip::udp::endpoint m_ep_udp_sender;
	asio::ip::tcp::endpoint m_ep_tcp_dest;
	asio::ip::udp::socket m_socket_udp_acc;
	asio::ip::tcp::socket m_socket_tcp_dest;
	// Application keep-alive idle time in seconds, 0 to disable
	unsigned int m_app_keep_alive_idle_time = 0;
	asio::deadline_timer m_app_keep_alive_timer;
	// TCP keep-alive idle time in seconds, 0 to disable
	unsigned int m_tcp_keep_alive_idle_time = 0;
	// Temporary buffer for keeping UDP packet received before TCP connection
	utils::ip::udp::buffer::ptr m_send_tmp_buffer;
	size_t m_send_tmp_buffer_length;
};

}; // namespace tunnel
}; // namespace wg
