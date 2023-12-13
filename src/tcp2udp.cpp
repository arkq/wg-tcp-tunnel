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
		std::make_shared<tcp::session>(*this, std::move(peer))->run();
	}
	// Handle next TCP connection
	do_accept();
}

void tcp2udp::tcp::session::run() {
	// Save remote endpoint, so it could be used after the socket is disconnected
	m_socket_ep_remote = m_socket.remote_endpoint();
	LOG(debug) << "session::run: " << to_string();
	// Start handling TCP packets
	do_send_init();
}

void tcp2udp::tcp::session::do_send_init() {
	do_send(sizeof(utils::ip::udp::header), true);
}

void tcp2udp::tcp::session::do_send(size_t rlen, bool ctrl) {
	auto buffer = std::make_shared<asio::streambuf>();
	auto handler = std::bind(&tcp2udp::tcp::session::do_send_handler, shared_from_this(), _1,
	                         buffer, _2, ctrl);
	asio::async_read(tcp(), *buffer, asio::transfer_exactly(rlen), handler);
}

void tcp2udp::tcp::session::do_send_handler(const boost::system::error_code & ec,
                                            utils::ip::tcp::buffer::ptr buffer, size_t length,
                                            bool ctrl) {

	if (ec) {
		if (ec == asio::error::eof || ec == asio::error::connection_reset) {
			LOG(debug) << "session::send: Connection closed: peer="
			           << utils::to_string(m_socket_ep_remote);
			// Stop UDP receiver if there is no TCP session
			udp().cancel();
			return;
		}
		LOG(error) << "session::send [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_send_init();
		return;
	}

	LOG(trace) << "session::send [" << to_string(true) << "]: len=" << length;

	if (ctrl) {
		auto header = reinterpret_cast<const utils::ip::udp::header *>(buffer->data().data());
		if (!header->valid()) {
			LOG(error) << "session::send [" << to_string() << "]: Invalid UDP header";
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

	udp().send(buffer->data());

	// Handle next TCP packet
	do_send_init();
}

void tcp2udp::tcp::session::do_recv() {
	auto buffer = std::make_shared<std::array<char, 4096>>();
	auto handler =
	    std::bind(&tcp2udp::tcp::session::do_recv_handler, shared_from_this(), _1, buffer, _2);
	udp().async_receive(asio::buffer(*buffer), handler);
}

void tcp2udp::tcp::session::do_recv_handler(const boost::system::error_code & ec,
                                            utils::ip::udp::buffer::ptr buffer, size_t length) {

	if (ec) {
		if (ec == asio::error::operation_aborted)
			return;
		LOG(error) << "session::recv [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_recv();
		return;
	}

	LOG(trace) << "session::recv [" << to_string(true) << "]: len=" << length;
	// Send payload with attached UDP header
	utils::ip::udp::header header(udp().remote_endpoint().port(), udp().local_endpoint().port(),
	                              static_cast<uint16_t>(length));
	std::array<asio::const_buffer, 2> iovec{ asio::buffer(&header, sizeof(header)),
		                                     asio::buffer(*buffer, length) };
	tcp().send(iovec);

	// Handle next UDP packet
	do_recv();
}

std::string tcp2udp::tcp::session::to_string(bool verbose) {
	std::string str = utils::to_string(m_socket_ep_remote);
	if (verbose)
		str += " -> " + utils::to_string(tcp().local_endpoint());
	str += " >> ";
	if (verbose)
		str += utils::to_string(udp().local_endpoint()) + " -> ";
	str += utils::to_string(udp().remote_endpoint());
	return str;
}

}; // namespace tunnel
}; // namespace wg
