// tcp2udp.h
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
#include <utility>

#include <boost/asio.hpp>
#if ENABLE_WEBSOCKET
#	include <boost/beast/websocket.hpp>
#endif

#include "utils.hpp"

namespace wg {
namespace tunnel {

namespace asio = boost::asio;
#if ENABLE_WEBSOCKET
namespace beast = boost::beast;
namespace ws = beast::websocket;
#endif
using std::size_t;

class tcp2udp {
public:
	tcp2udp(asio::io_context & ioc, asio::ip::tcp::endpoint ep_tcp_acc,
	        asio::ip::udp::endpoint ep_udp_dest)
	    : m_io_context(ioc), m_ep_tcp_acc(std::move(ep_tcp_acc)),
	      m_ep_udp_dest(std::move(ep_udp_dest)), m_tcp_acceptor(ioc, m_ep_tcp_acc) {}
	~tcp2udp() = default;

	void run(utils::transport transport);

	void keep_alive_app(unsigned int idle_time) { m_app_keep_alive_idle_time = idle_time; }
	void keep_alive_tcp(unsigned int idle_time) { m_tcp_keep_alive_idle_time = idle_time; }
#if ENABLE_WEBSOCKET
	void ws_headers(utils::http::headers headers) { m_ws_headers = std::move(headers); }
#endif

private:
	union tcp {
		class session {
		public:
			session(tcp2udp & tcp2udp, asio::ip::tcp::socket socket)
			    : m_socket(std::move(socket)), m_socket_udp_dest(tcp2udp.m_io_context),
			      m_socket_ep_remote(m_socket.remote_endpoint()) {
				m_socket_udp_dest.connect(tcp2udp.m_ep_udp_dest);
			}

		protected:
			std::string to_string(bool verbose = false);

			asio::ip::tcp::socket m_socket;
			asio::ip::udp::socket m_socket_udp_dest;
			// Saved remote endpoint of the TCP socket, so we can get
			// the address after the socket is disconnected
			asio::ip::tcp::endpoint m_socket_ep_remote;
		};

		class session_raw : public session, public std::enable_shared_from_this<session_raw> {
		public:
			session_raw(tcp2udp & tcp2udp, asio::ip::tcp::socket socket)
			    : session(tcp2udp, std::move(socket)) {}

			void run();

		private:
			void do_send_init();
			void do_send(size_t rlen, bool ctrl = false);
			void do_send_handler(const boost::system::error_code & ec, size_t length, bool ctrl);

			void do_recv();
			void do_recv_handler(const boost::system::error_code & ec, size_t length);

			asio::streambuf m_buffer_send;
			std::array<char, 4096> m_buffer_recv;
			bool m_initialized = false;
		};

#if ENABLE_WEBSOCKET
		class session_ws : public session, public std::enable_shared_from_this<session_ws> {
		public:
			session_ws(tcp2udp & tcp2udp, asio::ip::tcp::socket socket)
			    : session(tcp2udp, std::move(socket)), m_ws(m_socket),
			      m_ws_headers(tcp2udp.m_ws_headers) {}

			void run();

		private:
			void do_accept();
			void do_accept_handler(const boost::system::error_code & ec);

			void do_send();
			void do_send_handler(const boost::system::error_code & ec, size_t length);

			void do_recv();
			void do_recv_handler(const boost::system::error_code & ec, size_t length);

			ws::stream<asio::ip::tcp::socket &> m_ws;
			utils::http::headers & m_ws_headers;
			beast::flat_buffer m_buffer_send;
			std::array<char, 4096> m_buffer_recv;
		};
#endif
	};

	void do_accept();
	void do_accept_handler(const boost::system::error_code & ec, asio::ip::tcp::socket peer);

	asio::io_context & m_io_context;
	asio::ip::tcp::endpoint m_ep_tcp_acc;
	asio::ip::udp::endpoint m_ep_udp_dest;
	asio::ip::tcp::acceptor m_tcp_acceptor;
	// Transport protocol used for the TCP connection
	utils::transport m_transport = utils::transport::raw;
	// Application keep-alive idle time in seconds, 0 to disable
	unsigned int m_app_keep_alive_idle_time = 0;
	// TCP keep-alive idle time in seconds, 0 to disable
	unsigned int m_tcp_keep_alive_idle_time = 0;
#if ENABLE_WEBSOCKET
	// List of WebSocket custom headers used during the handshake
	utils::http::headers m_ws_headers;
#endif
};

}; // namespace tunnel
}; // namespace wg
