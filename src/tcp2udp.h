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
	tcp2udp(asio::io_context & ioc, const asio::ip::tcp::endpoint & ep_tcp_src,
	        const asio::ip::udp::endpoint & ep_udp_dest)
	    : m_io_context(ioc), m_ep_tcp_acc(ep_tcp_src), m_ep_udp_dest(ep_udp_dest),
	      m_tcp_acceptor(ioc, ep_tcp_src) {}
	~tcp2udp() = default;

	void init();

	void keep_alive_app(unsigned int idle_time) { m_app_keep_alive_idle_time = idle_time; }
	void keep_alive_tcp(unsigned int idle_time) { m_tcp_keep_alive_idle_time = idle_time; }

private:
	union tcp {
		class peer : public std::enable_shared_from_this<peer> {
		public:
			using ptr = std::shared_ptr<peer>;

			peer(tcp2udp & tcp2udp)
			    : m_socket(tcp2udp.m_io_context), m_socket_udp_dest(tcp2udp.m_io_context) {
				m_socket_udp_dest.connect(tcp2udp.m_ep_udp_dest);
			}

			auto & tcp() { return m_socket; }
			auto & udp() { return m_socket_udp_dest; }

			bool init() { return std::exchange(m_initialized, true); }
			void keepalive(unsigned int idle_time);
			void connected();

			template <typename... A> auto send(A... args) { return m_socket.send(args...); }
			auto local_endpoint() const { return m_socket.local_endpoint(); }
			auto remote_endpoint() const { return m_socket_ep_remote; }

		private:
			asio::ip::tcp::socket m_socket;
			asio::ip::tcp::endpoint m_socket_ep_remote;
			asio::ip::udp::socket m_socket_udp_dest;
			bool m_initialized = false;
		};
	};

	std::string to_string(tcp::peer::ptr peer, bool verbose = false);

	void do_accept();
	void do_accept_handler(tcp::peer::ptr peer, const boost::system::error_code & ec);

	void do_send_init(tcp::peer::ptr peer);
	void do_send(tcp::peer::ptr peer, size_t rlen, bool ctrl = false);
	void do_send_handler(tcp::peer::ptr peer, const boost::system::error_code & ec,
	                     utils::ip::tcp::buffer::ptr buffer, size_t length, bool ctrl);

	void do_recv(tcp::peer::ptr peer);
	void do_recv_handler(tcp::peer::ptr peer, const boost::system::error_code & ec,
	                     utils::ip::udp::buffer::ptr buffer, size_t length);

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
