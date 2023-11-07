// utils.hpp
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#pragma once

#include <array>
#include <memory>

#include <boost/asio.hpp>

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
namespace buffer {
using ptr = std::shared_ptr<std::array<char, 4096>>;
}; // namespace buffer
namespace socket {
using ptr = std::shared_ptr<asio::ip::udp::socket>;
}; // namespace socket
}; // namespace ip::udp

static inline std::string to_string(const asio::ip::tcp::endpoint & ep) {
	return "tcp:" + ep.address().to_string() + ":" + std::to_string(ep.port());
}

static inline std::string to_string(const asio::ip::udp::endpoint & ep) {
	return "udp:" + ep.address().to_string() + ":" + std::to_string(ep.port());
}

} // namespace utils
} // namespace wg
