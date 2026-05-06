/// @file socket.hpp
/// @brief Socket server functions.
#pragma once

#include "options.hpp"

/// @brief  Start a socket server.
/// The server can use epoll(7), poll(2), or select(2) to wait.
/// @param opts Program options.
extern void SocketReader(const Options& opts);

