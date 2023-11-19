// utils.hpp
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <boost/asio.hpp>
#include <boost/crc.hpp>

namespace wg {
namespace utils {

namespace asio = boost::asio;

namespace ip::tcp {
namespace buffer {
using ptr = std::shared_ptr<asio::streambuf>;
}; // namespace buffer
namespace socket {
using ptr = std::shared_ptr<asio::ip::tcp::socket>;
}; // namespace socket
}; // namespace ip::tcp

namespace ip::udp {

struct header {

	header(uint16_t src_port, uint16_t dst_port, uint16_t length)
	    : m_src_port(src_port), m_dst_port(dst_port), m_length(length) {
		boost::crc_16_type crc16;
		crc16.process_bytes(this, sizeof(*this) - sizeof(m_crc16));
		m_crc16 = crc16.checksum();
	}

	bool valid() const {
		boost::crc_16_type crc16;
		crc16.process_bytes(this, sizeof(*this) - sizeof(m_crc16));
		return crc16.checksum() == m_crc16;
	}

	uint16_t m_src_port;
	uint16_t m_dst_port;
	uint16_t m_length;
	uint16_t m_crc16;
};

// Make sure the header structure is not padded
static_assert(sizeof(header) == 8, "Invalid UDP header size");

namespace buffer {
using ptr = std::shared_ptr<std::array<char, 4096>>;
}; // namespace buffer
namespace socket {
using ptr = std::shared_ptr<asio::ip::udp::socket>;
}; // namespace socket

}; // namespace ip::udp

static inline std::pair<std::string, uint16_t> split_host_port(const std::string & str) {
	auto pos = str.find_last_of(':');
	if (pos == std::string::npos)
		throw std::runtime_error("Unable to split host and port");
	return { str.substr(0, pos), std::stoi(str.substr(pos + 1)) };
}

static inline std::string to_string(const asio::ip::tcp::endpoint & ep) {
	return "tcp:" + ep.address().to_string() + ":" + std::to_string(ep.port());
}

static inline std::string to_string(const asio::ip::udp::endpoint & ep) {
	return "udp:" + ep.address().to_string() + ":" + std::to_string(ep.port());
}

static inline int socket_set_keep_alive_idle(asio::ip::tcp::socket & socket, int time) {
	boost::system::error_code ec;
	socket.set_option(asio::detail::socket_option::integer<IPPROTO_TCP, TCP_KEEPIDLE>(time), ec);
	return ec.value();
}

} // namespace utils
} // namespace wg
