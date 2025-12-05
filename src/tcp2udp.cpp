// wg-tcp-tunnel - tcp2udp.cpp
// SPDX-FileCopyrightText: 2023-2025 Arkadiusz Bokowy and contributors
// SPDX-License-Identifier: MIT

#include "tcp2udp.h"

#include <array>
#include <cstddef>
#include <functional>
#include <memory>

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

#include "utils.hpp"

namespace wg::tunnel {

namespace asio = boost::asio;
#if ENABLE_WEBSOCKET
namespace beast = boost::beast;
namespace ws = beast::websocket;
#endif
using namespace std::placeholders;
#define LOG(lvl) BOOST_LOG_TRIVIAL(lvl) << "tcp2udp::"

auto tcp2udp::run(utils::transport transport) -> void {
	LOG(info) << "run: " << utils::to_string(m_ep_tcp_acc) << " >> "
	          << utils::to_string(m_ep_udp_dest);
	m_transport = transport;
	do_accept();
}

auto tcp2udp::do_accept() -> void {
	auto socket = std::make_shared<asio::ip::tcp::socket>(m_io_context);
	m_tcp_acceptor.async_accept(m_io_context, [this](const auto & ec, auto && peer) {
		do_accept_handler(ec, std::forward<decltype(peer)>(peer));
	});
}

auto tcp2udp::do_accept_handler(const boost::system::error_code & ec, asio::ip::tcp::socket peer)
    -> void {
	if (ec) {
		LOG(error) << "accept [" << utils::to_string(m_ep_tcp_acc) << "]: " << ec.message();
	} else {
		LOG(debug) << "accept [" << utils::to_string(m_ep_tcp_acc)
		           << "]: New connection: peer=" << utils::to_string(peer.remote_endpoint());
		if (m_tcp_keep_alive_idle_time > 0) {
			// Setup TCP keep-alive on the session socket
			LOG(debug) << "tcp-keepalive [" << utils::to_string(peer.remote_endpoint())
			           << "]: idle=" << m_tcp_keep_alive_idle_time;
			utils::socket_set_keep_alive_idle(peer, m_tcp_keep_alive_idle_time);
			peer.set_option(asio::socket_base::keep_alive(true));
			peer.set_option(asio::socket_base::linger(true, 0));
		}
		// Start handling TCP packets
		switch (m_transport) {
		case utils::transport::raw:
			std::make_shared<tcp::session_raw>(*this, std::move(peer))->run();
			break;
#if ENABLE_WEBSOCKET
		case utils::transport::websocket:
			std::make_shared<tcp::session_ws>(*this, std::move(peer))->run();
			break;
#endif
		}
	}
	// Handle next TCP connection
	do_accept();
}

auto tcp2udp::tcp::session_raw::run() -> void {
	LOG(info) << "session-raw::run: " << to_string();
	// Start handling TCP packets
	do_send_init();
}

auto tcp2udp::tcp::session_raw::do_send_init() -> void {
	do_send(sizeof(utils::ip::udp::header), true);
}

auto tcp2udp::tcp::session_raw::do_send(size_t rlen, bool ctrl) -> void {
	m_buffer_send.consume(m_buffer_send.size()); // Clean any previous data
	asio::async_read(m_socket, m_buffer_send, asio::transfer_exactly(rlen),
	                 [self = shared_from_this(), ctrl](const auto & ec, size_t length) {
		                 self->do_send_handler(ec, length, ctrl);
	                 });
}

auto tcp2udp::tcp::session_raw::do_send_handler(const boost::system::error_code & ec,
                                                size_t length, bool ctrl) -> void {

	if (ec) {
		if (ec == asio::error::eof || ec == asio::error::connection_reset) {
			LOG(debug) << "session-raw::send: Connection closed: peer="
			           << utils::to_string(m_socket_ep_remote);
			// Stop UDP receiver if there is no TCP session
			m_socket_udp_dest.cancel();
			return;
		}
		LOG(error) << "session-raw::send [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_send_init();
		return;
	}

	LOG(trace) << "session-raw::send [" << to_string(true) << "]: len=" << length;

	if (ctrl) {
		auto header =
		    reinterpret_cast<const utils::ip::udp::header *>(m_buffer_send.data().data());
		if (!header->valid()) {
			LOG(warning) << "session-raw::send [" << to_string() << "]: Invalid UDP header";
			// Handle next TCP packet
			do_send_init();
			return;
		}
		// Check if the packet is a control packet
		if (header->m_length == 0) {
			// Handle next TCP packet
			do_send_init();
			return;
		}
		// Handle UDP packet payload
		do_send(header->m_length);
		return;
	}

	// At this point we know that the control header was valid
	if (!std::exchange(m_initialized, true)) {
		// Start handling UDP packets
		do_recv();
	}

	m_socket_udp_dest.send(m_buffer_send.data());

	// Handle next TCP packet
	do_send_init();
}

auto tcp2udp::tcp::session_raw::do_recv() -> void {
	m_socket_udp_dest.async_receive(asio::buffer(m_buffer_recv),
	                                [self = shared_from_this()](const auto & ec, size_t length) {
		                                self->do_recv_handler(ec, length);
	                                });
}

auto tcp2udp::tcp::session_raw::do_recv_handler(const boost::system::error_code & ec,
                                                size_t length) -> void {

	if (ec) {
		if (ec == asio::error::operation_aborted)
			return;
		LOG(error) << "session-raw::recv [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_recv();
		return;
	}

	LOG(trace) << "session-raw::recv [" << to_string(true) << "]: len=" << length;
	// Send payload with attached UDP header
	utils::ip::udp::header header(m_socket_udp_dest.remote_endpoint().port(),
	                              m_socket_udp_dest.local_endpoint().port(),
	                              static_cast<uint16_t>(length));
	const std::array<asio::const_buffer, 2> iovec{ asio::buffer(&header, sizeof(header)),
		                                           asio::buffer(m_buffer_recv, length) };
	m_socket.send(iovec);

	// Handle next UDP packet
	do_recv();
}

#if ENABLE_WEBSOCKET

auto tcp2udp::tcp::session_ws::run() -> void {
	LOG(info) << "session-ws::run: " << to_string();
	// Ensure that the WebSocket stream will be binary
	m_ws.binary(true);
	// Start handling WebSocket handshake
	do_accept();
}

auto tcp2udp::tcp::session_ws::do_accept() -> void {
	// Set suggested timeout settings for the WebSocket server
	m_ws.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));
	// Modify the server handshake response headers
	m_ws.set_option(ws::stream_base::decorator([&](ws::response_type & res) {
		LOG(debug) << "session-ws::accept: Sending response: peer="
		           << utils::to_string(m_socket_ep_remote);
		for (const auto & [key, value] : m_ws_headers)
			res.insert(key, value);
	}));
	m_ws.async_accept(
	    [self = shared_from_this()](const auto & ec) { self->do_accept_handler(ec); });
}

auto tcp2udp::tcp::session_ws::do_accept_handler(const boost::system::error_code & ec) -> void {
	if (ec) {
		LOG(error) << "session-ws::accept [" << to_string() << "]: " << ec.message();
		return;
	}
	LOG(debug) << "session-ws::accept: Handshake accepted: peer="
	           << utils::to_string(m_socket_ep_remote);
	// Start handling WebSocket packets
	do_send();
	// Start handling UDP packets
	do_recv();
}

auto tcp2udp::tcp::session_ws::do_send() -> void {
	m_buffer_send.clear();
	m_ws.async_read(m_buffer_send, [self = shared_from_this()](const auto & ec, size_t length) {
		self->do_send_handler(ec, length);
	});
}

auto tcp2udp::tcp::session_ws::do_send_handler(const boost::system::error_code & ec, size_t length)
    -> void {

	if (ec) {
		if (ec == asio::error::operation_aborted)
			return;
		if (ec == asio::error::eof || ec == asio::error::connection_reset) {
			LOG(debug) << "session-ws::send: Connection closed: peer="
			           << utils::to_string(m_socket_ep_remote);
			// Stop UDP receiver if there is no TCP session
			m_socket_udp_dest.cancel();
			return;
		}
		LOG(error) << "session-ws::send [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_send();
		return;
	}

	try {
		LOG(trace) << "session-ws::send [" << to_string(true) << "]: len=" << length;
		m_socket_udp_dest.send(m_buffer_send.data());
	} catch (const std::exception & e) {
		LOG(error) << "session-ws::send [" << to_string() << "]: " << e.what();
	}

	// Handle next WebSocket packet
	do_send();
}

auto tcp2udp::tcp::session_ws::do_recv() -> void {
	m_socket_udp_dest.async_receive(asio::buffer(m_buffer_recv),
	                                [self = shared_from_this()](const auto & ec, size_t length) {
		                                self->do_recv_handler(ec, length);
	                                });
}

auto tcp2udp::tcp::session_ws::do_recv_handler(const boost::system::error_code & ec, size_t length)
    -> void {

	if (ec) {
		if (ec == asio::error::operation_aborted)
			return;
		LOG(error) << "session-ws::recv [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_recv();
		return;
	}

	LOG(trace) << "session-ws::recv [" << to_string(true) << "]: len=" << length;
	m_ws.write(asio::buffer(m_buffer_recv, length));

	// Handle next UDP packet
	do_recv();
}

#endif

auto tcp2udp::tcp::session::to_string(bool verbose) -> std::string {
	std::string str = utils::to_string(m_socket_ep_remote);
	if (verbose)
		str += " -> " + utils::to_string(m_socket.local_endpoint());
	str += " >> ";
	if (verbose)
		str += utils::to_string(m_socket_udp_dest.local_endpoint()) + " -> ";
	str += utils::to_string(m_socket_udp_dest.remote_endpoint());
	return str;
}

}; // namespace wg::tunnel
