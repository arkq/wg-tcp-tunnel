// tcp2udp.cpp
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#include "tcp2udp.h"

#include <array>
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
		do_send(peer);
		do_recv(peer);
	}
	// Handle next TCP connection
	do_accept();
}

void tcp2udp::do_send(utils::ip::tcp::socket::ptr peer) {
	auto buffer = std::make_shared<asio::streambuf>();
	auto handler = std::bind(&tcp2udp::do_send_handler, this, peer, _1, buffer, _2);
	asio::async_read(*peer, *buffer, asio::transfer_at_least(1), handler);
}

void tcp2udp::do_send_handler(utils::ip::tcp::socket::ptr peer,
                              const boost::system::error_code & ec,
                              utils::ip::tcp::buffer::ptr buffer, size_t length) {

	if (ec) {
		LOG(error) << "send [" << utils::to_string(peer->remote_endpoint()) << " >> "
		           << utils::to_string(udp_ep_remote()) << "]: " << ec.message();
		if (ec == asio::error::eof) {
			// Stop UDP receiver if there is no TCP peer
			m_socket_udp_dest.cancel();
			return;
		}
		// Try to recover from error
		do_send(peer);
		return;
	}

	LOG(trace) << "send [" << utils::to_string(peer->remote_endpoint()) << " -> "
	           << utils::to_string(peer->local_endpoint()) << " >> "
	           << utils::to_string(udp_ep_local()) << " -> " << utils::to_string(udp_ep_remote())
	           << "]: len=" << length;

	m_socket_udp_dest.send(buffer->data());

	// Handle next TCP packet
	do_send(peer);
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
		LOG(error) << "recv [" << utils::to_string(udp_ep_remote()) << " >> "
		           << utils::to_string(peer->remote_endpoint()) << "]: " << ec.message();
		if (ec == asio::error::operation_aborted)
			return;
		// Try to recover from error
		do_recv(peer);
		return;
	}

	LOG(trace) << "recv [" << utils::to_string(udp_ep_remote()) << " -> "
	           << utils::to_string(udp_ep_local()) << " >> "
	           << utils::to_string(peer->local_endpoint()) << " -> "
	           << utils::to_string(peer->remote_endpoint()) << " len=" << length;

	peer->send(asio::buffer(*buffer, length));

	// Handle next UDP packet
	do_recv(peer);
}

}; // namespace tunnel
}; // namespace wg
