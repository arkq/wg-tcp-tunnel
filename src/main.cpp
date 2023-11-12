// main.cpp
//
// Copyright (c) 2023 Arkadiusz Bokowy
//
// This file is a part of wg-tcp-tunnel.
//
// This project is licensed under the terms of the MIT license.

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/asio.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include "ngrok.h"
#include "tcp2udp.h"
#include "udp2tcp.h"
#include "version.h"

namespace asio = boost::asio;
namespace logging = boost::log;
namespace po = boost::program_options;

namespace boost {

namespace asio {
namespace ip {

template <typename T>
void validate(boost::any & v, const std::vector<std::string> & values, T *, int) {
	po::validators::check_first_occurrence(v);
	const std::string & s = po::validators::get_single_string(values);
	auto pos = s.find_last_of(':');
	if (pos == std::string::npos)
		throw po::error_with_option_name(
		    "unable to split IP address and port in option '%canonical_option%'");
	try {
		auto addr = asio::ip::make_address(s.substr(0, pos));
		auto port = std::stoi(s.substr(pos + 1));
		v = boost::any(T(addr, port));
	} catch (const boost::system::system_error &) {
		throw po::error_with_option_name(
		    "the IP address in option '%canonical_option%' is invalid");
	} catch (const std::exception &) {
		throw po::error_with_option_name(
		    "the port number in option '%canonical_option%' is invalid");
	}
}

}; // namespace ip
}; // namespace asio

namespace program_options {

class counter : public typed_value<std::size_t> {
public:
	counter(std::size_t * store = nullptr) : typed_value<std::size_t>(store), m_count(0) {
		// Make counter a non-value option
		default_value(0);
		zero_tokens();
	}
	virtual ~counter() = default;
	virtual void xparse(boost::any & store, const std::vector<std::string> &) const {
		// Increment counter on each option occurrence
		store = boost::any(++m_count);
	}

private:
	mutable std::size_t m_count;
};

}; // namespace program_options

}; // namespace boost

int main(int argc, char * argv[]) {

	asio::ip::tcp::endpoint ep_src_tcp;
	asio::ip::udp::endpoint ep_dst_udp;
	asio::ip::udp::endpoint ep_src_udp;
	asio::ip::tcp::endpoint ep_dst_tcp;
	std::size_t verbose;

	po::options_description options("Options");
	auto o_builder = options.add_options();
	o_builder("help,h", "print this help message and exit");
	o_builder("version,V", "print version and exit");
	o_builder("src-tcp,T", po::value(&ep_src_tcp), "source TCP address and port");
	o_builder("dst-udp,u", po::value(&ep_dst_udp), "destination UDP address and port");
	o_builder("src-udp,U", po::value(&ep_src_udp), "source UDP address and port");
	o_builder("dst-tcp,t", po::value(&ep_dst_tcp), "destination TCP address and port");
	o_builder("verbose,v", new po::counter(&verbose), "increase verbosity level");

#if ENABLE_NGROK
	std::string ngrok_dst_tcp_endpoint;
	// By default API key is read from the environment variable
	std::string ngrok_api_key = "ENV:NGROK_API_KEY";
	o_builder("ngrok-dst-tcp-endpoint", po::value(&ngrok_dst_tcp_endpoint),
	          "ngrok endpoint for destination TCP");
	o_builder("ngrok-api-key", po::value(&ngrok_api_key),
	          "ngrok API key, default: 'ENV:NGROK_API_KEY'");
#endif

	po::variables_map args;
	try {
		po::store(po::parse_command_line(argc, argv, options), args);
		po::notify(args);
	} catch (const std::exception & e) {
		std::cerr << PROJECT_NAME << ": " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	if (args.count("help")) {
		std::cout << "Usage:" << std::endl
		          << "  " << PROJECT_NAME << " [OPTION]..." << std::endl
		          << std::endl
		          << options << std::endl
		          << "Examples:" << std::endl
		          << "  " << PROJECT_NAME << " --src-tcp=127.0.0.1:12345 --dst-udp=127.0.0.1:51820"
		          << std::endl
		          << "  " << PROJECT_NAME << " --src-udp=127.0.0.1:51821 --dst-tcp=127.0.0.1:12345"
		          << std::endl;
		return EXIT_SUCCESS;
	}
	if (args.count("version")) {
		std::cout << PROJECT_NAME << " " << PROJECT_VERSION << std::endl;
		return EXIT_SUCCESS;
	}

	switch (verbose) {
	case 0:
		logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
		break;
	case 1:
		logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);
		break;
	default:
		logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::trace);
		break;
	}

#if ENABLE_NGROK
	if (!ngrok_dst_tcp_endpoint.empty()) {

		if (ngrok_api_key.substr(0, 4) == "ENV:")
			ngrok_api_key = std::getenv(ngrok_api_key.substr(4).c_str()) ?: "";

		try {
			wg::ngrok::client ngrok(ngrok_api_key);
			auto endpoints = ngrok.endpoints();

			if (ngrok_dst_tcp_endpoint == "list") {
				for (const auto & ep : endpoints)
					std::cout << ep << std::endl;
				return EXIT_SUCCESS;
			}

			if (ngrok_dst_tcp_endpoint == "tcp") {
				for (const auto & ep : endpoints) {
					if (ep.proto == wg::ngrok::endpoint::protocol::tcp &&
					    ep.type == wg::ngrok::endpoint::etype::ephemeral) {
						ep_dst_tcp = asio::ip::tcp::endpoint(ep.address(), ep.port);
						break;
					}
				}
			}

		} catch (const std::exception & e) {
			std::cerr << PROJECT_NAME << ": " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}
#endif

	const bool is_server = ep_src_tcp.port() != 0 && ep_dst_udp.port() != 0;
	const bool is_client = ep_src_udp.port() != 0 && ep_dst_tcp.port() != 0;
	if (!is_server && !is_client) {
		std::cerr << PROJECT_NAME << ": one of "
		          << "'--src-tcp' && '--dst-udp'"
		          << " or "
		          << "'--src-udp' && '--dst-tcp'"
		          << " must be given" << std::endl;
		return EXIT_FAILURE;
	}

	asio::io_context ioc;
	wg::tunnel::tcp2udp tcp2udp(ioc, ep_src_tcp, ep_dst_udp);
	wg::tunnel::udp2tcp udp2tcp(ioc, ep_src_udp, ep_dst_tcp);

	if (is_server)
		tcp2udp.init();
	if (is_client)
		udp2tcp.init();

	ioc.run();

	return EXIT_SUCCESS;
}
