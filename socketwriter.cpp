/// @file socketwriter.cpp
/// @brief Socket client functions.

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <chrono>
#include <cstring>
#include <vector>

#include "socketwriter.hpp"
#include "helpers.hpp"
#include "options.hpp"
#include "textsource.hpp"

static void CloseSockets(std::vector<int>& sockets);
static std::vector<int> OpenSockets(const std::string& addr, size_t port, size_t number);
static void WriteLinesToSockets(std::vector<int>& sockets, const std::string& textfile, size_t num_lines, size_t delay_ms);
static void WriteDurationToSockets(std::vector<int>& sockets, const std::string& textfile, size_t num_seconds, size_t delay_ms);

// Start a socket client.
void SocketWriter(const Options& opts)
{
    std::vector<int> sockets;

    try {
        sockets = OpenSockets(opts.ip_addr, opts.port, opts.number);
        if (opts.lines > 0) {
            WriteLinesToSockets(sockets, opts.text_file, opts.lines, opts.delay_msec);
        } else {
            WriteDurationToSockets(sockets, opts.text_file, opts.seconds, opts.delay_msec);
        }
        CloseSockets(sockets);
    } catch (const std::exception& e) {
        Err("Error: %s", e.what());
        CloseSockets(sockets);
        throw;
    } catch (...) {
        Err("Unknown error");
        CloseSockets(sockets);
        throw;
    }
}

/// @brief Close the sockets.
/// @param[in] sockets A vector of socket file descriptors.
static void CloseSockets(std::vector<int>& sockets)
{
    for (int fd : sockets) {
        if (fd >= 0) {
            Msg("Close (%d)", fd);
            close(fd);
        }
    }
    sockets.clear();
}

/// @brief Open N socket connections.
/// @param addr Remote IP Address
/// @param port Remote port
/// @param num_sockets Number of connections to open to the remote host.
/// @return A vector of open file descriptors.
static std::vector<int> OpenSockets(const std::string& addr, size_t port, size_t num_sockets)
{
    std::vector<int> sockets;
    sockets.reserve(num_sockets);

    for (size_t i = 0; i < num_sockets; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            throw std::runtime_error("socket: " + std::string(strerror(errno)));
        }

        struct sockaddr_in serv_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = {
                .s_addr = INADDR_ANY
            },
            .sin_zero = 0
        };

        int e = inet_pton(AF_INET, addr.c_str(), &(serv_addr.sin_addr));
        if (e == -1) {
            close(fd);
            throw std::runtime_error("inet_pton( " + addr + " ): " + std::string(strerror(errno)));
        } else if (e == 0) {
            close(fd);
            throw std::invalid_argument(addr + " is not a valid network address");
        }

        e = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (e == -1) {
            close(fd);
            throw std::runtime_error("connect: port(" + std::to_string(port) + " ): " + std::string(strerror(errno)));
        }

        sockets.push_back(fd);
    }

    Msg("Opened %d socket connections to port %d", sockets.size(), port);
    return sockets;
}

static void WriteDurationToSockets(std::vector<int>& sockets, const std::string& textfile, size_t num_seconds, size_t delay_ms)
{
    if (sockets.empty()) {
        throw std::runtime_error("No sockets!");
    }

    TextSource text_source(textfile);

    auto start_time = std::chrono::steady_clock::now();
    auto stop_time = start_time + std::chrono::seconds(num_seconds);

    size_t total_bytes = 0;
    size_t total_lines = 0;
    size_t open_sockets = sockets.size();
    Msg("Writing to sockets every %zd ms for %zu seconds", delay_ms, num_seconds);
    while (std::chrono::steady_clock::now() < stop_time) {
        int idx = 0;
        int fd = -1;

        if (open_sockets == 0) {
            Msg("No open sockets");
            break;
        }

        std::string line = text_source.GetText();
        do {
            // For reasons unknown, a write to idx size-1 causes a SIGPIPE
            // (according to the Internet) and causes the program termination.
            // The shell reports a return code of 141 (ENOTRECOVERABLE) or
            // 128 + 13 (SIGPIPE).  Does not trigger the signal handler.
            idx = RandInt(0, sockets.size() - 1);
            fd = sockets[idx];
        } while (fd == -1);

        Msg("Writing %d bytes to socket %d", line.size(), fd);
        ssize_t n = send(fd, line.c_str(), line.size(), 0);
        if (n == -1) {
            Err("send (%d): %s", fd, strerror(errno));
            sockets[idx] = -1;
            open_sockets--;
        } else if (n == 0) {
            Msg("No data sent (%d)", fd);  // Err or Msg?
        } else if (n < static_cast<ssize_t>(line.size())) {
            total_lines++;
            total_bytes += n;
            Err("Partial write (%d): %d of %d bytes", fd, n, line.size());
        } else if (n == static_cast<ssize_t>(line.size())) {
            total_lines++;
            total_bytes += n;
        }

        Pause(delay_ms);
    }

    const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
    PrintSummary(total_bytes, total_lines, duration_ms);

    Msg("Done writing");
}

static void WriteLinesToSockets(std::vector<int>& sockets, const std::string& textfile, size_t num_lines, size_t delay_ms)
{
    if (sockets.empty()) {
        throw std::runtime_error("No sockets!");
    }

    TextSource text_source(textfile);

    const auto start_time = std::chrono::steady_clock::now();

    size_t total_bytes = 0;
    size_t total_lines = 0;
    size_t open_sockets = sockets.size();
    Msg("Writing %zu lines to sockets every %zd ms", num_lines, delay_ms);
    while (total_lines < num_lines) {
        int idx = 0;
        int fd = -1;

        if (open_sockets == 0) {
            Msg("No open sockets");
            break;
        }

        std::string line = text_source.GetText();
        do {
            // For reasons unknown, a write to idx size-1 causes a SIGPIPE
            // (according to the Internet) and causes the program termination.
            // The shell reports a return code of 141 (ENOTRECOVERABLE) or
            // 128 + 13 (SIGPIPE).  Does not trigger the signal handler.
            idx = RandInt(0, sockets.size() - 1);
            fd = sockets[idx];
        } while (fd == -1);

        Msg("Writing %d bytes to socket %d", line.size(), fd);
        ssize_t n = send(fd, line.c_str(), line.size(), 0);
        if (n == -1) {
            Err("send (%d): %s", fd, strerror(errno));
            sockets[idx] = -1;
            open_sockets--;
        } else if (n == 0) {
            Msg("No data sent (%d)", fd);  // Err or Msg?
        } else if (n < static_cast<ssize_t>(line.size())) {
            total_lines++;
            total_bytes += n;
            Err("Partial write (%d): %d of %d bytes", fd, n, line.size());
        } else if (n == static_cast<ssize_t>(line.size())) {
            total_lines++;
            total_bytes += n;
        }

        Pause(delay_ms);
    }

    const auto stop_time = std::chrono::steady_clock::now();
    const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start_time).count();
    PrintSummary(total_bytes, total_lines, duration_ms);

    Msg("Done writing");
}
