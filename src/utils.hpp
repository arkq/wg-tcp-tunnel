// wg-tcp-tunnel - utils.hpp
// SPDX-FileCopyrightText: 2023-2025 Arkadiusz Bokowy and contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/crc.hpp>

namespace wg {
namespace utils {

namespace asio = boost::asio;

enum class transport {
	raw,
#if ENABLE_WEBSOCKET
	websocket,
#endif
};

namespace ip {
namespace udp {

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

}; // namespace udp
}; // namespace ip

namespace http {

using header = std::pair<std::string, std::string>;
using headers = std::vector<header>;

static inline header split_header(const std::string_view str) {
	auto pos = str.find_first_of(':');
	if (pos == std::string::npos)
		throw std::runtime_error("Unable to split HTTP header");
	// Trim any leading spaces from the value
	auto pos2 = str.substr(pos + 1).find_first_not_of(' ');
	return { std::string(str.substr(0, pos)), std::string(str.substr(pos + 1 + pos2)) };
}

}; // namespace http

static inline std::pair<std::string, uint16_t> split_host_port(const std::string_view str) {
	auto pos = str.find_last_of(':');
	if (pos == std::string::npos)
		throw std::runtime_error("Unable to split host and port");
	return { std::string(str.substr(0, pos)), std::stoi(std::string(str.substr(pos + 1))) };
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
