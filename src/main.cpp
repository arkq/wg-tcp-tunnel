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
#include <boost/program_options.hpp>

#include "version.h"

namespace asio = boost::asio;
namespace po = boost::program_options;

namespace boost {
namespace asio {
namespace ip {

template <typename T>
void validate(boost::any & v, const std::vector<std::string> & values, T *, int) {
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
}; // namespace boost

int main(int argc, char * argv[]) {

	asio::ip::tcp::endpoint ep_src_tcp;
	asio::ip::udp::endpoint ep_dst_udp;
	asio::ip::udp::endpoint ep_src_udp;
	asio::ip::tcp::endpoint ep_dst_tcp;

	po::options_description options("Options");
	auto o_builder = options.add_options();
	o_builder("help,h", "print this help message and exit");
	o_builder("version,V", "print version and exit");
	o_builder("src-tcp,T", po::value(&ep_src_tcp), "source TCP address and port");
	o_builder("dst-udp,u", po::value(&ep_dst_udp), "destination UDP address and port");
	o_builder("src-udp,U", po::value(&ep_src_udp), "source UDP address and port");
	o_builder("dst-tcp,t", po::value(&ep_dst_tcp), "destination TCP address and port");

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
		          << options << std::endl;
		return EXIT_SUCCESS;
	}
	if (args.count("version")) {
		std::cout << PROJECT_NAME << " " << PROJECT_VERSION << std::endl;
		return EXIT_SUCCESS;
	}

	return EXIT_SUCCESS;
}
