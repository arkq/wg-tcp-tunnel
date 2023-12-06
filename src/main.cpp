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
#include <vector>

#include <boost/asio.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/program_options.hpp>

#include "ngrok.h"
#include "tcp2udp.h"
#include "udp2tcp.h"
#include "version.h"

namespace asio = boost::asio;
namespace logging = boost::log;
namespace po = boost::program_options;
using std::size_t;

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

class counter : public typed_value<size_t> {
public:
	counter(size_t * store = nullptr) : typed_value<size_t>(store), m_count(0) {
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
	mutable size_t m_count;
};

}; // namespace program_options

}; // namespace boost

int main(int argc, char * argv[]) {

	asio::ip::tcp::endpoint ep_src_tcp;
	asio::ip::udp::endpoint ep_dst_udp;
	asio::ip::udp::endpoint ep_src_udp;
	asio::ip::tcp::endpoint ep_dst_tcp;
	unsigned int tcp_keep_alive = 0;
	size_t verbose;

	po::options_description options("Options");
	auto o_builder = options.add_options();
	o_builder("help,h", "print this help message and exit");
	o_builder("version,V", "print version and exit");
	o_builder("verbose,v", new po::counter(&verbose), "increase verbosity level");
	o_builder("src-tcp,T", po::value(&ep_src_tcp), "source TCP address and port");
	auto dst_udp_default = asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 51820);
	o_builder("dst-udp,u", po::value(&ep_dst_udp)->default_value(dst_udp_default),
	          "destination UDP address and port");
	o_builder("src-udp,U", po::value(&ep_src_udp), "source UDP address and port");
	o_builder("dst-tcp,t", po::value(&ep_dst_tcp), "destination TCP address and port");
	o_builder("tcp-keep-alive", po::value(&tcp_keep_alive)->implicit_value(120),
	          "enable TCP keep-alive on TCP socket(s) optionally specifying the keep-alive "
	          "idle time in seconds");

#if ENABLE_NGROK
	std::string ngrok_api_key;
	std::string ngrok_dst_tcp_endpoint;
	unsigned int ngrok_keep_alive = 0;
	o_builder("ngrok-api-key", po::value(&ngrok_api_key)->default_value("ENV:NGROK_API_KEY"),
	          "NGROK API key or 'ENV:VARIABLE' to read the key from the environment variable");
	o_builder("ngrok-dst-tcp-endpoint", po::value(&ngrok_dst_tcp_endpoint),
	          "NGROK endpoint used to forward TCP traffic; the endpoint can be specified as "
	          "'id=ID' or 'uri=REGEX', where ID is the endpoint identifier and REGEX is a "
	          "regular expression matching the endpoint URI; the special value 'list' can be "
	          "used to list all available endpoints");
	o_builder("ngrok-keep-alive", po::value(&ngrok_keep_alive)->implicit_value(270),
	          "enable keep-alive for NGROK connection");
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

#if ENABLE_SYSTEMD
	if (std::getenv("INVOCATION_ID") != nullptr)
		// If launched by systemd we do not need timestamp in our log message
		logging::add_console_log(std::clog, logging::keywords::format = "[%Severity%] %Message%");
#endif

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

	wg::tunnel::udp2tcp_dest_provider_simple udp2tcp_dest_provider_simple(ep_dst_tcp);
	wg::tunnel::udp2tcp_dest_provider * udp2tcp_dest_provider = &udp2tcp_dest_provider_simple;
	bool dynamic_dst_tcp = false;

#if ENABLE_NGROK

	if (ngrok_api_key.substr(0, 4) == "ENV:") {
		// Read NGROK API key from the environment variable
		const auto key = std::getenv(ngrok_api_key.substr(4).c_str());
		ngrok_api_key = key != nullptr ? key : "";
	}

	wg::ngrok::client ngrok(ngrok_api_key);
	wg::tunnel::udp2tcp_dest_provider_ngrok udp2tcp_dest_provider_ngrok(ngrok);

	if (!ngrok_dst_tcp_endpoint.empty()) {
		try {
			if (ngrok_dst_tcp_endpoint == "list") {
				for (const auto & ep : ngrok.endpoints())
					std::cout << ep << std::endl;
				return EXIT_SUCCESS;
			} else if (ngrok_dst_tcp_endpoint.substr(0, 3) == "id=") {
				udp2tcp_dest_provider_ngrok.filter_id(ngrok_dst_tcp_endpoint.substr(3));
				udp2tcp_dest_provider = &udp2tcp_dest_provider_ngrok;
				dynamic_dst_tcp = true;
			} else if (ngrok_dst_tcp_endpoint.substr(0, 4) == "uri=") {
				udp2tcp_dest_provider_ngrok.filter_uri(ngrok_dst_tcp_endpoint.substr(4));
				udp2tcp_dest_provider = &udp2tcp_dest_provider_ngrok;
				dynamic_dst_tcp = true;
			} else {
				throw std::runtime_error("Invalid NGROK endpoint specification");
			}
		} catch (const std::exception & e) {
			std::cerr << PROJECT_NAME << ": " << e.what() << std::endl;
			return EXIT_FAILURE;
		}
	}

#endif

	const bool is_server = ep_src_tcp.port() != 0 && ep_dst_udp.port() != 0;
	const bool is_client = ep_src_udp.port() != 0 && (ep_dst_tcp.port() != 0 || dynamic_dst_tcp);
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
	wg::tunnel::udp2tcp udp2tcp(ioc, ep_src_udp, *udp2tcp_dest_provider);

#if ENABLE_NGROK
	tcp2udp.keep_alive_app(ngrok_keep_alive);
	udp2tcp.keep_alive_app(ngrok_keep_alive);
#endif
	tcp2udp.keep_alive_tcp(tcp_keep_alive);
	udp2tcp.keep_alive_tcp(tcp_keep_alive);

restart:

	if (is_server)
		tcp2udp.init();
	if (is_client)
		udp2tcp.init();

	try {
		ioc.run();
	} catch (const std::exception & e) {
		std::cerr << PROJECT_NAME << ": " << e.what() << std::endl;
		goto restart;
	}

	return EXIT_SUCCESS;
}
