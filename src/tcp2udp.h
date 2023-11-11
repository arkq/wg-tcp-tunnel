// tcp2udp.h
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

class tcp2udp {
public:
	tcp2udp(asio::io_context & ioc, const asio::ip::tcp::endpoint & ep_tcp_src,
	        const asio::ip::udp::endpoint & ep_udp_dest)
	    : m_io_context(ioc), m_ep_tcp_acc(ep_tcp_src), m_ep_udp_dest(ep_udp_dest),
	      m_tcp_acceptor(ioc, ep_tcp_src), m_socket_udp_dest(ioc) {}
	~tcp2udp() = default;

	void init();

private:
	std::string to_string(utils::ip::tcp::socket::ptr peer, bool verbose = false);
	auto udp_ep_local() const { return m_socket_udp_dest.local_endpoint(); }
	auto udp_ep_remote() const { return m_socket_udp_dest.remote_endpoint(); }

	void do_accept();
	void do_accept_handler(utils::ip::tcp::socket::ptr peer, const boost::system::error_code & ec);

	void do_send_init(utils::ip::tcp::socket::ptr peer);
	void do_send(utils::ip::tcp::socket::ptr peer, size_t rlen, bool ctrl = false);
	void do_send_handler(utils::ip::tcp::socket::ptr peer, const boost::system::error_code & ec,
	                     utils::ip::tcp::buffer::ptr buffer, size_t length, bool ctrl);

	void do_recv(utils::ip::tcp::socket::ptr peer);
	void do_recv_handler(utils::ip::tcp::socket::ptr peer, const boost::system::error_code & ec,
	                     utils::ip::udp::buffer::ptr buffer, size_t length);

	asio::io_context & m_io_context;
	asio::ip::tcp::endpoint m_ep_tcp_acc;
	asio::ip::udp::endpoint m_ep_udp_dest;
	asio::ip::tcp::acceptor m_tcp_acceptor;
	asio::ip::udp::socket m_socket_udp_dest;
};

}; // namespace tunnel
}; // namespace wg
