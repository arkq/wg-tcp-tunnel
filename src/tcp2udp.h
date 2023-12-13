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
#include <utility>

#include <boost/asio.hpp>

#include "utils.hpp"

namespace wg {
namespace tunnel {

namespace asio = boost::asio;
using std::size_t;

class tcp2udp {
public:
	tcp2udp(asio::io_context & ioc, asio::ip::tcp::endpoint ep_tcp_acc,
	        asio::ip::udp::endpoint ep_udp_dest)
	    : m_io_context(ioc), m_ep_tcp_acc(std::move(ep_tcp_acc)),
	      m_ep_udp_dest(std::move(ep_udp_dest)), m_tcp_acceptor(ioc, m_ep_tcp_acc) {}
	~tcp2udp() = default;

	void run();

	void keep_alive_app(unsigned int idle_time) { m_app_keep_alive_idle_time = idle_time; }
	void keep_alive_tcp(unsigned int idle_time) { m_tcp_keep_alive_idle_time = idle_time; }

private:
	union tcp {
		class session : public std::enable_shared_from_this<session> {
		public:
			session(tcp2udp & tcp2udp, asio::ip::tcp::socket socket)
			    : m_socket(std::move(socket)), m_socket_udp_dest(tcp2udp.m_io_context) {
				m_socket_udp_dest.connect(tcp2udp.m_ep_udp_dest);
			}

			void run();

		private:
			void do_send_init();
			void do_send(size_t rlen, bool ctrl = false);
			void do_send_handler(const boost::system::error_code & ec,
			                     utils::ip::tcp::buffer::ptr buffer, size_t length, bool ctrl);

			void do_recv();
			void do_recv_handler(const boost::system::error_code & ec,
			                     utils::ip::udp::buffer::ptr buffer, size_t length);

			std::string to_string(bool verbose = false);

			auto & tcp() { return m_socket; }
			auto & udp() { return m_socket_udp_dest; }

			asio::ip::tcp::socket m_socket;
			asio::ip::tcp::endpoint m_socket_ep_remote;
			asio::ip::udp::socket m_socket_udp_dest;
			bool m_initialized = false;
		};
	};

	void do_accept();
	void do_accept_handler(const boost::system::error_code & ec, asio::ip::tcp::socket peer);

	asio::io_context & m_io_context;
	asio::ip::tcp::endpoint m_ep_tcp_acc;
	asio::ip::udp::endpoint m_ep_udp_dest;
	asio::ip::tcp::acceptor m_tcp_acceptor;
	// Application keep-alive idle time in seconds, 0 to disable
	unsigned int m_app_keep_alive_idle_time = 0;
	// TCP keep-alive idle time in seconds, 0 to disable
	unsigned int m_tcp_keep_alive_idle_time = 0;
};

}; // namespace tunnel
}; // namespace wg
