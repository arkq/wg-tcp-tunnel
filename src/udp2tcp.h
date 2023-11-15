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
	        const asio::ip::tcp::endpoint & ep_tcp_dest, unsigned int tcp_keep_alive_idle_time)
	    : m_ep_udp_acc(ep_udp_acc), m_ep_tcp_dest(ep_tcp_dest),
	      m_socket_udp_acc(ioc, m_ep_udp_acc), m_socket_tcp_dest(ioc),
	      m_tcp_keep_alive_idle_time(tcp_keep_alive_idle_time) {}
	~udp2tcp() = default;

	void init();

private:
	std::string to_string(bool verbose = false);

	void do_connect();
	void do_connect_handler(const boost::system::error_code & ec);

	void do_send();
	void do_send_handler(const boost::system::error_code & ec, utils::ip::udp::buffer::ptr buffer,
	                     size_t length);

	void do_recv_init();
	void do_recv(size_t rlen, bool ctrl = false);
	void do_recv_handler(const boost::system::error_code & ec, utils::ip::tcp::buffer::ptr buffer,
	                     size_t length, bool ctrl);

	asio::ip::udp::endpoint m_ep_udp_acc;
	asio::ip::udp::endpoint m_ep_udp_sender;
	asio::ip::tcp::endpoint m_ep_tcp_dest;
	asio::ip::udp::socket m_socket_udp_acc;
	asio::ip::tcp::socket m_socket_tcp_dest;
	unsigned int m_tcp_keep_alive_idle_time;
};

}; // namespace tunnel
}; // namespace wg
