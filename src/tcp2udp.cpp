// tcp2udp.cpp
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#include "tcp2udp.h"

#include <array>
#include <cstddef>
#include <functional>
#include <memory>

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

#include "utils.hpp"

namespace wg {
namespace tunnel {

namespace asio = boost::asio;
using namespace std::placeholders;
#define LOG(lvl) BOOST_LOG_TRIVIAL(lvl) << "tcp2udp::"

void tcp2udp::run() {
	LOG(debug) << "run: " << utils::to_string(m_ep_tcp_acc) << " >> "
	           << utils::to_string(m_ep_udp_dest);
	do_accept();
}

void tcp2udp::do_accept() {
	auto socket = std::make_shared<asio::ip::tcp::socket>(m_io_context);
	auto handler = std::bind(&tcp2udp::do_accept_handler, this, _1, _2);
	m_tcp_acceptor.async_accept(m_io_context, handler);
}

void tcp2udp::do_accept_handler(const boost::system::error_code & ec, asio::ip::tcp::socket peer) {
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
		std::make_shared<tcp::session_raw>(*this, std::move(peer))->run();
	}
	// Handle next TCP connection
	do_accept();
}

void tcp2udp::tcp::session_raw::run() {
	LOG(debug) << "session-raw::run: " << to_string();
	// Start handling TCP packets
	do_send_init();
}

void tcp2udp::tcp::session_raw::do_send_init() {
	do_send(sizeof(utils::ip::udp::header), true);
}

void tcp2udp::tcp::session_raw::do_send(size_t rlen, bool ctrl) {
	auto handler =
	    std::bind(&tcp2udp::tcp::session_raw::do_send_handler, shared_from_this(), _1, _2, ctrl);
	m_buffer_send.consume(m_buffer_send.size()); // Clean any previous data
	asio::async_read(m_socket, m_buffer_send, asio::transfer_exactly(rlen), handler);
}

void tcp2udp::tcp::session_raw::do_send_handler(const boost::system::error_code & ec,
                                                size_t length, bool ctrl) {

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
			LOG(error) << "session-raw::send [" << to_string() << "]: Invalid UDP header";
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

void tcp2udp::tcp::session_raw::do_recv() {
	auto handler =
	    std::bind(&tcp2udp::tcp::session_raw::do_recv_handler, shared_from_this(), _1, _2);
	m_socket_udp_dest.async_receive(asio::buffer(m_buffer_recv), handler);
}

void tcp2udp::tcp::session_raw::do_recv_handler(const boost::system::error_code & ec,
                                                size_t length) {

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
	std::array<asio::const_buffer, 2> iovec{ asio::buffer(&header, sizeof(header)),
		                                     asio::buffer(m_buffer_recv, length) };
	m_socket.send(iovec);

	// Handle next UDP packet
	do_recv();
}

std::string tcp2udp::tcp::session::to_string(bool verbose) {
	std::string str = utils::to_string(m_socket_ep_remote);
	if (verbose)
		str += " -> " + utils::to_string(m_socket.local_endpoint());
	str += " >> ";
	if (verbose)
		str += utils::to_string(m_socket_udp_dest.local_endpoint()) + " -> ";
	str += utils::to_string(m_socket_udp_dest.remote_endpoint());
	return str;
}

}; // namespace tunnel
}; // namespace wg
