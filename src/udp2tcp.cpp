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
	do_send();
}

std::string udp2tcp::to_string(bool verbose) {
	std::string str = utils::to_string(m_ep_udp_sender);
	if (verbose)
		str += " -> " + utils::to_string(m_ep_udp_acc);
	str += " >> ";
	if (verbose)
		str += utils::to_string(m_socket_tcp_dest.local_endpoint()) + " -> ";
	str += utils::to_string(m_socket_tcp_dest.remote_endpoint());
	return str;
}

void udp2tcp::do_connect() {
	auto handler = std::bind(&udp2tcp::do_connect_handler, this, _1);
	m_socket_tcp_dest.async_connect(m_ep_tcp_dest, handler);
}

void udp2tcp::do_connect_handler(const boost::system::error_code & ec) {

	if (ec) {
		LOG(error) << "connect [" << utils::to_string(m_ep_tcp_dest) << "]: " << ec.message();
		m_socket_tcp_dest.close();
		// Handle next UDP packet
		do_send();
		return;
	}

	LOG(debug) << "connect [" << utils::to_string(m_ep_tcp_dest) << "]: Connected";

	if (m_tcp_keep_alive_idle_time > 0) {
		LOG(debug) << "keepalive [" << utils::to_string(m_socket_tcp_dest.remote_endpoint())
		           << "]: " << m_tcp_keep_alive_idle_time;
		utils::socket_set_keep_alive_idle(m_socket_tcp_dest, m_tcp_keep_alive_idle_time);
		m_socket_tcp_dest.set_option(asio::socket_base::keep_alive(true));
		m_socket_tcp_dest.set_option(asio::socket_base::linger(true, 0));
	}

	// Send UDP packet which was waiting for TCP connection
	send(m_send_tmp_buffer, m_send_tmp_buffer_length);
	m_send_tmp_buffer.reset();

	// Handle next UDP packet
	do_send();
	// Start handling TCP packets
	do_recv_init();
}

void udp2tcp::do_send() {
	auto buffer = std::make_shared<std::array<char, 4096>>();
	auto handler = std::bind(&udp2tcp::do_send_handler, this, _1, buffer, _2);
	m_socket_udp_acc.async_receive_from(asio::buffer(*buffer), m_ep_udp_sender, handler);
}

void udp2tcp::do_send_handler(const boost::system::error_code & ec,
                              utils::ip::udp::buffer::ptr buffer, size_t length) {

	if (ec) {
		LOG(error) << "send [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_send();
		return;
	}

	if (!m_socket_tcp_dest.is_open()) {
		m_send_tmp_buffer = buffer;
		m_send_tmp_buffer_length = length;
		do_connect();
		return;
	}

	send(buffer, length);

	// Handle next UDP packet
	do_send();
}

void udp2tcp::send(utils::ip::udp::buffer::ptr buffer, size_t length) {
	LOG(trace) << "send [" << to_string(true) << "]: len=" << length;
	// Send payload with attached UDP header
	utils::ip::udp::header header(m_ep_udp_sender.port(), m_ep_udp_acc.port(), length);
	std::array<asio::const_buffer, 2> iovec{ asio::buffer(&header, sizeof(header)),
		                                     asio::buffer(*buffer, length) };
	m_socket_tcp_dest.send(iovec);
}

void udp2tcp::do_recv_init() {
	do_recv(sizeof(utils::ip::udp::header), true);
}

void udp2tcp::do_recv(std::size_t rlen, bool ctrl) {
	auto buffer = std::make_shared<asio::streambuf>();
	auto handler = std::bind(&udp2tcp::do_recv_handler, this, _1, buffer, _2, ctrl);
	asio::async_read(m_socket_tcp_dest, *buffer, asio::transfer_exactly(rlen), handler);
}

void udp2tcp::do_recv_handler(const boost::system::error_code & ec,
                              utils::ip::tcp::buffer::ptr buffer, size_t length, bool ctrl) {

	if (ec) {
		if (ec == asio::error::operation_aborted)
			return;
		if (ec == asio::error::eof || ec == asio::error::connection_reset) {
			LOG(debug) << "recv: Connection closed: peer=" << utils::to_string(m_ep_tcp_dest);
			m_socket_tcp_dest.close();
			return;
		}
		LOG(error) << "recv [" << to_string() << "]: " << ec.message();
		// Try to recover from error
		do_recv_init();
		return;
	}

	LOG(trace) << "recv [" << to_string(true) << "]: len=" << length;

	if (ctrl) {
		auto header = reinterpret_cast<const utils::ip::udp::header *>(buffer->data().data());
		if (!header->valid()) {
			LOG(error) << "recv [" << to_string() << "]: Invalid UDP header";
			// Handle next TCP packet
			do_recv_init();
			return;
		}
		// Handle UDP packet payload
		do_recv(header->m_length);
		return;
	}

	if (m_ep_udp_sender.port() != 0)
		m_socket_udp_acc.send_to(buffer->data(), m_ep_udp_sender);

	// Handle next TCP packet
	do_recv_init();
}

}; // namespace tunnel
}; // namespace wg
