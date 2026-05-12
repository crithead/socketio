/**
 * Program options.
 */

#pragma once

#include <string>

enum WaitMethod { None, Select, Poll, Epoll };

class Options
{
   public:
    static const size_t DEFAULT_NUMBER;
    static const size_t MIN_NUMBER;
    static const size_t MAX_NUMBER;
    static const size_t DEFAULT_SECONDS;
    static const size_t MIN_SECONDS;
    static const size_t MAX_SECONDS;
    static const size_t DEFAULT_DELAY_MSEC;
    static const size_t MIN_DELAY;
    static const size_t MAX_DELAY;
    static const size_t DEFAULT_LINES;
    static const size_t MIN_LINES;
    static const size_t MAX_LINES;
    static const size_t DEFAULT_PORT;
    static const size_t MIN_PORT;
    static const size_t MAX_PORT;
    static const std::string DEFAULT_IP_ADDR;
    static const std::string DEFAULT_TEXT_FILE;
    static const std::string DEFAULT_WAIT_METHOD;

    Options(int argc, char *argv[]);

    bool print_usage;
    bool verbose;
    size_t delay_msec;
    size_t lines;
    size_t number;
    size_t seconds;
    size_t port;
    std::string ip_addr;
    std::string text_file;
    std::string wait_method;
};

/// @brief Convert a wait method string into a WaitMethod enumeration value.
/// @param method The wait method string to parse.
/// @return The corresponding WaitMethod enumeration value.
WaitMethod ParseWaitMethod(const std::string& method);
