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
	m_socket_udp_dest.connect(m_ep_udp_dest);
	do_accept();
}

std::string tcp2udp::to_string(utils::ip::tcp::socket::ptr peer, bool verbose) {
	std::string str = utils::to_string(peer->remote_endpoint());
	if (verbose)
		str += " -> " + utils::to_string(peer->local_endpoint());
	str += " >> ";
	if (verbose)
		str += utils::to_string(udp_ep_local()) + " -> ";
	str += utils::to_string(udp_ep_remote());
	return str;
}

void tcp2udp::do_accept() {
	auto peer = std::make_shared<asio::ip::tcp::socket>(m_io_context);
	auto handler = std::bind(&tcp2udp::do_accept_handler, this, peer, _1);
	m_tcp_acceptor.async_accept(*peer, handler);
}

void tcp2udp::do_accept_handler(utils::ip::tcp::socket::ptr peer,
                                const boost::system::error_code & ec) {
	if (ec) {
		LOG(error) << "accept [" << utils::to_string(m_ep_tcp_acc) << "]: " << ec.message();
	} else {
		LOG(debug) << "accept [" << utils::to_string(m_ep_tcp_acc)
		           << "]: New connection: peer=" << utils::to_string(peer->remote_endpoint());
		// Start handling TCP and UDP packets
		// TODO: create UDP "connection" per TCP connection
		do_send_init(peer);
		do_recv(peer);
	}
	// Handle next TCP connection
	do_accept();
}

void tcp2udp::do_send_init(utils::ip::tcp::socket::ptr peer) {
	do_send(peer, sizeof(utils::ip::udp::header), true);
}

void tcp2udp::do_send(utils::ip::tcp::socket::ptr peer, size_t rlen, bool ctrl) {
	auto buffer = std::make_shared<asio::streambuf>();
	auto handler = std::bind(&tcp2udp::do_send_handler, this, peer, _1, buffer, _2, ctrl);
	asio::async_read(*peer, *buffer, asio::transfer_exactly(rlen), handler);
}

void tcp2udp::do_send_handler(utils::ip::tcp::socket::ptr peer,
                              const boost::system::error_code & ec,
                              utils::ip::tcp::buffer::ptr buffer, size_t length, bool ctrl) {

	if (ec) {
		LOG(error) << "send [" << to_string(peer) << "]: " << ec.message();
		if (ec == asio::error::eof) {
			// Stop UDP receiver if there is no TCP peer
			m_socket_udp_dest.cancel();
			return;
		}
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
		// Handle UDP packet payload
		do_send(peer, header->m_length);
		return;
	}

	m_socket_udp_dest.send(buffer->data());

	// Handle next TCP packet
	do_send_init(peer);
}

void tcp2udp::do_recv(utils::ip::tcp::socket::ptr peer) {
	auto buffer = std::make_shared<std::array<char, 4096>>();
	auto handler = std::bind(&tcp2udp::do_recv_handler, this, peer, _1, buffer, _2);
	m_socket_udp_dest.async_receive(asio::buffer(*buffer), handler);
}

void tcp2udp::do_recv_handler(utils::ip::tcp::socket::ptr peer,
                              const boost::system::error_code & ec,
                              utils::ip::udp::buffer::ptr buffer, size_t length) {

	if (ec) {
		LOG(error) << "recv [" << to_string(peer) << "]: " << ec.message();
		if (ec == asio::error::operation_aborted)
			return;
		// Try to recover from error
		do_recv(peer);
		return;
	}

	LOG(trace) << "recv [" << to_string(peer, true) << "]: len=" << length;
	// Send payload with attached UDP header
	utils::ip::udp::header header(udp_ep_remote().port(), udp_ep_local().port(), length);
	std::array<asio::const_buffer, 2> iovec{ asio::buffer(&header, sizeof(header)),
		                                     asio::buffer(*buffer, length) };
	peer->send(iovec);

	// Handle next UDP packet
	do_recv(peer);
}

}; // namespace tunnel
}; // namespace wg
