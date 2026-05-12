/// Open all files under a specified directory and print the records found
// there in for some time.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "helpers.hpp"
#include "options.hpp"
#include "socketreader.hpp"

static void PrintOptions(const Options& opts);
static void PrintUsage(void);

/// @brief Open files and print their contents.
int main(int argc, char *argv[])
{
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

        AdjustLimits();
        SocketReader(opts);

    } catch (const std::exception& ex) {
        Err("Error: %s", ex.what());
        return 1;
    }

    Msg("Done");
    return 0;
}

/// @brief  Print relevant options if verbose is enabled.
/// @param opts Program options.
static void PrintOptions(const Options& opts)
{
    if (opts.verbose) {
        std::cout << "Options:" << '\n'
                  << "  -h  print_usage : "
                  << (opts.print_usage ? "true" : "false") << '\n'
                  << "  -v  verbose     : true" << '\n'
                  // << (opts.verbose ? "true" : "false")
                  << "  -n  number      : " << opts.number << '\n'
                  << "  -s  seconds     : " << opts.seconds << '\n'
                  << "  -p  port        : " << opts.port << '\n'
                  << "  -w  wait_method : " << opts.wait_method << '\n'
                  << std::endl;
    }
}

/// @brief  Print usage information to the standard output stream.
static void PrintUsage(void)
{
    static const char USAGE[] =
            "Usage: reader [OPTIONS]\n"
            "\n"
            "Reads from N files or sockets for S seconds and writes to the "
            "console.\n"
            "If port is non-zero, it listens on that port for incoming "
            "connections.\n"
            "Otherwise, it reads from all files found under the base "
            "directory. \n"
            "\n"
            "Options:\n"
            "  -h, --help               Print this help message and exit\n"
            "  -v, --verbose            Enable extra runtime messages\n"
            "  -n, --number N           Number of connections to accept "
            "(default: 1)\n"
            "  -p, --port N             Port number to listen on [1025, "
            "65535].  (default: 0)\n"
            "  -s, --seconds N          Number of seconds to run the program "
            "(default: 60)\n"
            "  -w, --wait-method MTHD   Wait method to use { epoll, poll, "
            "select } (default: poll)\n"
            "  -d, --directory DIR      Base directory to read files under "
            "(default: /tmp/poll)\n";

    std::cout << USAGE << std::endl;
}
