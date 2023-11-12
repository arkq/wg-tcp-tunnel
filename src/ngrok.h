// ngrok.h
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#pragma once

#include <cstdint>
#include <ctime>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio.hpp>

namespace wg {
namespace ngrok {

namespace asio = boost::asio;

struct endpoint {

	enum class protocol : unsigned char { http, https, tcp, tls };
	static protocol protocol_from_string(const std::string_view str);
	static std::string protocol_to_string(const protocol proto);

	enum class etype : unsigned char { ephemeral, edge };
	static etype type_from_string(const std::string_view str);
	static std::string type_to_string(const etype type);

	std::string id;
	std::time_t created_at;
	std::time_t updated_at;
	protocol proto;
	std::string host;
	uint16_t port;
	etype type;

	asio::ip::address address() const;

	friend std::ostream & operator<<(std::ostream & os, const endpoint & ep) {
		os << ep.id << ": created_at=" << ep.created_at << " updated_at=" << ep.updated_at
		   << " type=" << type_to_string(ep.type) << " uri=" << protocol_to_string(ep.proto)
		   << "://" << ep.host << ":" << ep.port;
		return os;
	}
};

class client {
public:
	explicit client(const std::string_view key) : m_key(key) {}
	~client() = default;

	std::vector<endpoint> endpoints();

private:
	std::string m_key;
};

} // namespace ngrok
} // namespace wg
