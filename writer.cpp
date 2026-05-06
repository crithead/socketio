/// Create a set of files under a specified directory and write records
// to them periodically for some time.

#include <signal.h>

#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>

#include "helpers.hpp"
#include "options.hpp"
#include "socketwriter.hpp"
#include "textsource.hpp"

static void HandleSignals(void);
static void PrintOptions(const Options& opts);
static void PrintUsage(void);

/// @brief Create files and write to them periodically for some time.
/// @param argc Argument count
/// @param argv Argument vector
/// @retval 0 Success
/// @retval 1 Failure
int main(int argc, char *argv[]) {
    try {
        Options opts(argc, argv);

        PrintOptions(opts);

        if (opts.print_usage) {
            PrintUsage();
            return 0;
        }

        if (opts.verbose) {
            EnableMessages(true);
        }

        HandleSignals();
        AdjustLimits();
        SocketWriter(opts);

    } catch (const std::invalid_argument& e) {
        Err("Invalid argument: %s", e.what());
        return 1;
    } catch (const std::exception& e) {
        Err("Error: %s", e.what());
        return 1;
    } catch (...) {
        Err("Unknown error");
        return 1;
    }

    Msg("Done");
    return 0;
}

/// @brief Install signal handlers.
static void HandleSignals(void)
{
    Msg("Install signal handlers");

    struct sigaction sa;
    sa.sa_handler = [](int signum) {
        switch (signum) {
            case SIGINT:
                Err("SIGINT (%d)", signum);
                exit(2);
                break;
            case SIGPIPE:
                Err("SIGPIPE (%d)", signum);
                break;
            default:
                Err("signal %d", signum);
                break;
        }
    };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    int e = sigaction(SIGINT, &sa, nullptr);
    if (e == -1) {
        throw std::runtime_error("sigaction: " + std::string(strerror(errno)));
    }
    e = sigaction(SIGPIPE, &sa, nullptr);
    if (e == -1) {
        throw std::runtime_error("sigaction: " + std::string(strerror(errno)));
    }
}

/// @brief  Print the options if verbose is enabled.
/// @param opts Program options.
static void PrintOptions(const Options& opts)
{
    if (opts.verbose) {
        std::cout << "Options:" << '\n'
            << "  -h  print_usage  : " << (opts.print_usage ? "true" : "false") << '\n'
            << "  -v  verbose      : " << (opts.verbose ? "true" : "false") << '\n'
            << "  -a  address      : " << opts.ip_addr << '\n'
            << "  -D  delay_msec   : " << opts.delay_msec << '\n'
            << "  -l  lines        : " << opts.lines << '\n'
            << "  -n  number       : " << opts.number << '\n'
            << "  -s  seconds      : " << opts.seconds << '\n'
            << "  -p  port         : " << opts.port << '\n'
            << "  -t  text_file    : " << opts.text_file << '\n'
            << std::endl;
    }
}

/// @brief  Print usage information to the standard output stream.
static void PrintUsage(void)
{
    static const char USAGE[] =
        "Usage: writer [OPTIONS]\n"
        "\n"
        "Socket Writer: Open N socket connections to the server at address A and port P,\n"
        "then write to one connection chosen at random every D milliseconds for S seconds.\n"
        "\n"
        "    writer -n 100 -s 60 -a 192.168.0.13 -p 20260 -D 100 -t /tmp/input.txt\n"
        "\n"
        "If the value of lines is non-zero, write N lines of text instead of writing\n"
        "for a number of seconds. For example: write 1 million lines of text with a\n"
        "delay of 0 ms.\n"
        "\n"
        "    writer -n 100 -l 1M -p 2025 -D 0 -t /tmp/input.txt\n"
        "\n"
        "Options:\n"
        "  -h, --help               Print this help message and exit\n"
        "  -v, --verbose            Enable extra messages\n"
        "  -a, --address IPA        IP Address to connect to (default: 127.0.0.1)\n"
        "  -d, --delay N            Average delay between writes in milliseconds (default: 10)\n"
        "  -l, --lines N            Number of lines to write (default: 0)\n"
        "  -n, --number N           Number of socket connections to open (default: 128)\n"
        "  -p, --port P             Port number for socket connections (default: 0)\n"
        "  -s, --seconds N          Number of seconds to run the program (default: 60)\n"
        "  -t, --text-file FILE     Path to the text file to read from (default: /tmp/poll/input.txt)\n"
        "  -w, --wait N             Number of seconds to wait between creating files\n"
        "                           and writing to them (default: 10)\n";

    std::cout << USAGE << std::endl;
}

