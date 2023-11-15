// ngrok.cpp
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#include "ngrok.h"

#include <stdexcept>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "utils.hpp"

namespace wg {
namespace ngrok {

namespace asio = boost::asio;
namespace http = boost::beast::http;
namespace pt = boost::property_tree;
namespace time = boost::posix_time;

#define LOG(lvl) BOOST_LOG_TRIVIAL(lvl) << "ngrok::"

namespace api {
constexpr const char * host = "api.ngrok.com";
constexpr const char * port = "443";
} // namespace api

endpoint::protocol endpoint::protocol_from_string(const std::string_view str) {
	if (str == "http")
		return protocol::http;
	if (str == "https")
		return protocol::https;
	if (str == "tcp")
		return protocol::tcp;
	if (str == "tls")
		return protocol::tls;
	throw std::runtime_error("Unknown endpoint protocol: " + std::string(str));
}

std::string endpoint::protocol_to_string(const endpoint::protocol proto) {
	switch (proto) {
	case protocol::http:
		return "http";
	case protocol::https:
		return "https";
	case protocol::tcp:
		return "tcp";
	case protocol::tls:
		return "tls";
	default:
		return "unknown";
	}
}

endpoint::etype endpoint::type_from_string(const std::string_view str) {
	if (str == "ephemeral")
		return etype::ephemeral;
	if (str == "edge")
		return etype::edge;
	throw std::runtime_error("Unknown endpoint type: " + std::string(str));
}

std::string endpoint::type_to_string(const endpoint::etype type) {
	switch (type) {
	case etype::ephemeral:
		return "ephemeral";
	case etype::edge:
		return "edge";
	default:
		return "unknown";
	}
}

asio::ip::address endpoint::address() const {
	asio::io_context ioc;
	asio::ip::tcp::resolver resolver(ioc);
	for (auto & ep : resolver.resolve(host, std::to_string(port)))
		return ep.endpoint().address();
	return asio::ip::address();
}

std::string endpoint::uri() const {
	return protocol_to_string(proto) + "://" + host + ":" + std::to_string(port);
}

std::vector<endpoint> client::endpoints() {

	if (m_key.empty())
		throw std::runtime_error("NGROK API key is not set");

	asio::io_context ioc;
	asio::ssl::context ctx(asio::ssl::context::sslv23_client);
	asio::ssl::stream<asio::ip::tcp::socket> socket(ioc, ctx);
	asio::ip::tcp::resolver resolver(ioc);

	auto it = resolver.resolve(api::host, api::port);
	asio::connect(socket.lowest_layer(), it);
	socket.handshake(asio::ssl::stream_base::handshake_type::client);

	http::request<http::string_body> req(http::verb::get, "/endpoints", 11);
	req.set(http::field::host, api::host);
	req.set(http::field::authorization, "Bearer " + m_key);
	req.set("Ngrok-Version", "2");

	http::write(socket, req);

	http::response<http::string_body> res;
	boost::beast::flat_buffer buffer;
	http::read(socket, buffer, res);

	std::istringstream is(res.body());
	boost::property_tree::ptree pt;
	boost::property_tree::read_json(is, pt);

	std::vector<endpoint> endpoints;
	for (const auto & item : pt.get_child("endpoints")) {

		auto id = item.second.get<std::string>("id");

		auto created_at = time::to_time_t(time::from_iso_extended_string(
		    item.second.get<std::string>("created_at").substr(0, 19)));
		auto updated_at = time::to_time_t(time::from_iso_extended_string(
		    item.second.get<std::string>("updated_at").substr(0, 19)));

		auto proto_str = item.second.get<std::string>("proto");
		auto proto = endpoint::protocol_from_string(proto_str);
		auto host_port = item.second.get<std::string>("hostport");
		auto [host, port] = utils::split_host_port(host_port);
		auto type_str = item.second.get<std::string>("type");
		auto type = endpoint::type_from_string(type_str);

		auto ep = endpoint{ id, created_at, updated_at, proto, host, port, type };
		LOG(trace) << "endpoint: " << ep;

		endpoints.push_back(ep);
	}

	return endpoints;
}

} // namespace ngrok
} // namespace wg
