// udp2tcp.cpp
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#include "udp2tcp.h"

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
#define LOG(lvl) BOOST_LOG_TRIVIAL(lvl) << "udp2tcp::"

void udp2tcp::init() {
	LOG(debug) << "init: " << utils::to_string(m_ep_udp_acc) << " >> "
	           << utils::to_string(m_ep_tcp_dest);
	do_connect();
	do_send();
}

void udp2tcp::do_connect() {
	auto handler = std::bind(&udp2tcp::do_connect_handler, this, _1);
	m_socket_tcp_dest.async_connect(m_ep_tcp_dest, handler);
}

void udp2tcp::do_connect_handler(const boost::system::error_code & ec) {

	if (ec) {
		LOG(error) << "connect [" << utils::to_string(m_ep_tcp_dest) << "]: " << ec.message();
		m_socket_tcp_dest.close();
	}

	LOG(debug) << "connect [" << utils::to_string(m_ep_tcp_dest) << "]: Connected";

	// Start handling TCP packets
	do_recv();
}

void udp2tcp::do_send() {
	auto buffer = std::make_shared<std::array<char, 4096>>();
	auto handler = std::bind(&udp2tcp::do_send_handler, this, _1, buffer, _2);
	m_socket_udp_acc.async_receive_from(asio::buffer(*buffer), m_ep_udp_sender, handler);
}

void udp2tcp::do_send_handler(const boost::system::error_code & ec,
                              utils::ip::udp::buffer::ptr buffer, size_t length) {

	if (ec) {
		LOG(error) << "send [" << utils::to_string(m_ep_udp_sender) << " >> "
		           << utils::to_string(tcp_ep_remote()) << "]: " << ec.message();
	} else {
		if (!m_socket_tcp_dest.is_open()) {
			do_connect();
		} else {
			LOG(trace) << "send [" << utils::to_string(m_ep_udp_sender) << " -> "
			           << utils::to_string(m_ep_udp_acc) << " >> "
			           << utils::to_string(tcp_ep_local()) << " -> "
			           << utils::to_string(tcp_ep_remote()) << "]: len=" << length;
			m_socket_tcp_dest.send(asio::buffer(*buffer, length));
		}
	}

	// Handle next UDP packet
	do_send();
}

void udp2tcp::do_recv() {
	auto buffer = std::make_shared<asio::streambuf>();
	auto handler = std::bind(&udp2tcp::do_recv_handler, this, _1, buffer, _2);
	asio::async_read(m_socket_tcp_dest, *buffer, asio::transfer_at_least(1), handler);
}

void udp2tcp::do_recv_handler(const boost::system::error_code & ec,
                              utils::ip::tcp::buffer::ptr buffer, size_t length) {

	if (ec) {
		LOG(error) << "recv [" << utils::to_string(tcp_ep_remote()) << " >> "
		           << utils::to_string(m_ep_udp_sender) << "]: " << ec.message();
		if (ec == asio::error::eof) {
			m_socket_tcp_dest.close();
			return;
		}
		// Try to recover from error
		do_recv();
		return;
	}

	LOG(trace) << "recv [" << utils::to_string(tcp_ep_remote()) << " -> "
	           << utils::to_string(tcp_ep_local()) << " >> " << utils::to_string(m_ep_udp_acc)
	           << " -> " << utils::to_string(m_ep_udp_sender) << "]: len=" << length;

	if (m_ep_udp_sender.port() != 0)
		m_socket_udp_acc.send_to(buffer->data(), m_ep_udp_sender);

	// Handle next TCP packet
	do_recv();
}

}; // namespace tunnel
}; // namespace wg
