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

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

#include "utils.hpp"

namespace wg {
namespace tunnel {

namespace asio = boost::asio;
using namespace std::placeholders;
#define LOG(lvl) BOOST_LOG_TRIVIAL(lvl) << "tcp2udp::"

void tcp2udp::init() {
	LOG(debug) << "init: " << utils::to_string(m_ep_tcp_acc) << " >> "
	           << utils::to_string(m_ep_udp_dest);
	do_accept();
}

std::string tcp2udp::to_string(tcp::peer::ptr peer, bool verbose) {
	std::string str = utils::to_string(peer->remote_endpoint());
	if (verbose)
		str += " -> " + utils::to_string(peer->local_endpoint());
	str += " >> ";
	if (verbose)
		str += utils::to_string(peer->udp().local_endpoint()) + " -> ";
	str += utils::to_string(peer->udp().remote_endpoint());
	return str;
}

void tcp2udp::do_accept() {
	auto peer = std::make_shared<tcp::peer>(*this);
	auto handler = std::bind(&tcp2udp::do_accept_handler, this, peer, _1);
	m_tcp_acceptor.async_accept(peer->tcp(), handler);
}

void tcp2udp::tcp::peer::keepalive(unsigned int idle_time) {
	LOG(debug) << "tcp-keepalive [" << utils::to_string(m_socket.remote_endpoint())
	           << "]: idle=" << idle_time;
	utils::socket_set_keep_alive_idle(m_socket, idle_time);
	m_socket.set_option(asio::socket_base::keep_alive(true));
	m_socket.set_option(asio::socket_base::linger(true, 0));
}

void tcp2udp::tcp::peer::connected() {
	// Save remote endpoint, so it could be used after the socket is disconnected
	m_socket_ep_remote = m_socket.remote_endpoint();
}

void tcp2udp::do_accept_handler(tcp::peer::ptr peer, const boost::system::error_code & ec) {
	if (ec) {
		LOG(error) << "accept [" << utils::to_string(m_ep_tcp_acc) << "]: " << ec.message();
	} else {
		LOG(debug) << "accept [" << utils::to_string(m_ep_tcp_acc) << "]: New connection: peer="
		           << utils::to_string(peer->tcp().remote_endpoint());
		if (m_tcp_keep_alive_idle_time > 0)
			// Setup TCP keep-alive on the peer socket
			peer->keepalive(m_tcp_keep_alive_idle_time);
		// Mark peer as connected
		peer->connected();
		// Start handling TCP packets
		do_send_init(peer);
	}
	// Handle next TCP connection
	do_accept();
}

void tcp2udp::do_send_init(tcp::peer::ptr peer) {
	do_send(peer, sizeof(utils::ip::udp::header), true);
}

void tcp2udp::do_send(tcp::peer::ptr peer, size_t rlen, bool ctrl) {
	auto buffer = std::make_shared<asio::streambuf>();
	auto handler = std::bind(&tcp2udp::do_send_handler, this, peer, _1, buffer, _2, ctrl);
	asio::async_read(peer->tcp(), *buffer, asio::transfer_exactly(rlen), handler);
}

void tcp2udp::do_send_handler(tcp::peer::ptr peer, const boost::system::error_code & ec,
                              utils::ip::tcp::buffer::ptr buffer, size_t length, bool ctrl) {

	if (ec) {
		if (ec == asio::error::eof || ec == asio::error::connection_reset) {
			LOG(debug) << "send: Connection closed: peer="
			           << utils::to_string(peer->remote_endpoint());
			// Stop UDP receiver if there is no TCP peer
			peer->udp().cancel();
			return;
		}
		LOG(error) << "send [" << to_string(peer) << "]: " << ec.message();
		// Try to recover from error
		do_send_init(peer);
		return;
	}

	LOG(trace) << "send [" << to_string(peer, true) << "]: len=" << length;

	if (ctrl) {
		auto header = reinterpret_cast<const utils::ip::udp::header *>(buffer->data().data());
		if (!header->valid()) {
			LOG(error) << "send [" << to_string(peer) << "]: Invalid UDP header";
			// Handle next TCP packet
			do_send_init(peer);
			return;
		}
		// Check if the packet is a control packet
		if (header->m_length == 0) {
			// Handle next TCP packet
			do_send_init(peer);
			return;
		}
		// Handle UDP packet payload
		do_send(peer, header->m_length);
		return;
	}

	// At this point we know that the control header was valid
	if (!peer->init()) {
		// Start handling UDP packets
		do_recv(peer);
	}

	peer->udp().send(buffer->data());

	// Handle next TCP packet
	do_send_init(peer);
}

void tcp2udp::do_recv(tcp::peer::ptr peer) {
	auto buffer = std::make_shared<std::array<char, 4096>>();
	auto handler = std::bind(&tcp2udp::do_recv_handler, this, peer, _1, buffer, _2);
	peer->udp().async_receive(asio::buffer(*buffer), handler);
}

void tcp2udp::do_recv_handler(tcp::peer::ptr peer, const boost::system::error_code & ec,
                              utils::ip::udp::buffer::ptr buffer, size_t length) {

	if (ec) {
		if (ec == asio::error::operation_aborted)
			return;
		LOG(error) << "recv [" << to_string(peer) << "]: " << ec.message();
		// Try to recover from error
		do_recv(peer);
		return;
	}

	LOG(trace) << "recv [" << to_string(peer, true) << "]: len=" << length;
	// Send payload with attached UDP header
	utils::ip::udp::header header(peer->udp().remote_endpoint().port(),
	                              peer->udp().local_endpoint().port(), length);
	std::array<asio::const_buffer, 2> iovec{ asio::buffer(&header, sizeof(header)),
		                                     asio::buffer(*buffer, length) };
	peer->send(iovec);

	// Handle next UDP packet
	do_recv(peer);
}

}; // namespace tunnel
}; // namespace wg
