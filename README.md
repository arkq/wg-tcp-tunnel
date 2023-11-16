# WireGuard TCP tunneling

## About

This project is a simple UDP-over-TCP tunneling. The main purpose of it is to
allow [WireGuard](https://www.wireguard.com/) to work over TCP.

## Installation

### Dependencies

This project is based on [Boost](https://www.boost.org/), so in order to build
it you need to have Boost installed. Also, you need to have CMake and a C++
compiler that supports C++17.

On Debian-based systems all dependencies can be installed by running:

```sh
sudo apt install \
  libboost-dev libboost-log-dev libboost-program-options-dev \
  cmake g++
```

### Building

```sh
cmake -S . -B build
cmake --build build
sudo cmake --install build
```
