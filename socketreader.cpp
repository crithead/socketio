/// @file socket.cpp
/// @brief Socket server implementation.

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

#include <chrono>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "helpers.hpp"
#include "options.hpp"
#include "socketreader.hpp"

class no_data_exception : public std::exception {
public:
    const char* what() const noexcept override {
        return "No data available";
    }
};

/// Receive buffer length.
static constexpr size_t RXBUFLEN = 4096;

static void AcceptNewConnection(int, int, std::vector<int>&);
static void SetNonBlocking(int);
static void SocketServerEpoll(const Options&);
static void SocketServerPoll(const Options&);
static void SocketServerSelect(const Options&);

void SocketReader(const Options& opts)
{
    switch (ParseWaitMethod(opts.wait_method)) {
        case WaitMethod::Select:
            SocketServerSelect(opts);
            break;
        case WaitMethod::Poll:
            SocketServerPoll(opts);
            break;
        case WaitMethod::Epoll:
            SocketServerEpoll(opts);
            break;
        default:
            throw std::invalid_argument("wait-method");
            break;
    }
}

/// @brief Accept a new connection (epoll)
/// - Call accept on the lister's file descriptor.
/// - Set the client file descriptor to non-blocking mode.
/// - Add the client file descriptor for the epoll set.
/// - Add the client file descriptor for the connections list.
/// @param epoll_fd The epoll file descriptor.
/// @param reader_fd The reader file descriptor.
/// @param connections The list of connections.
static void AcceptNewConnection(int epoll_fd, int reader_fd, std::vector<int>& connections)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int fd = accept(reader_fd, (struct sockaddr *)&client_addr, &client_len);
    if (fd == -1) {
        Err("accept: %s", strerror(errno));
        return;
    }

    Msg("Accept (%d)", fd);

    SetNonBlocking(fd);

    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLHUP | EPOLLERR,
        .data = {
            .fd = fd
        }
     };
    int err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (err == -1) {
        Err("epoll_ctl: %s", strerror(errno));
        close(fd);
        return;
    }

    connections.push_back(fd);
}

/// @brief Close a connection and remove it from the epoll set and connections list.
/// It is not necessary to remove the file descriptor from the epoll set before
/// closing it since close will do it in this simple case.
/// @param epoll_fd The epoll file descriptor.
/// @param fd The file descriptor of interest.
/// @param connections Connections list
static void CloseConnection(int epoll_fd, int fd, std::vector<int>& connections)
{
    Msg("Close connection (%d)", fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);    // not necessary
    close(fd);
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        if (*it == fd) {
            connections.erase(it);
            break;
        }
    }
}

/// @brief Open a non-blocking listener socket.
/// - Open socket
/// - Bind to port
/// - Listen
/// @param port The port number to listen on.
/// @param backlog The maximum number of pending connections in the listen queue.
/// @return The file descriptor of the listener socket.
static int OpenListenerSocket(size_t port, size_t backlog)
{
    Msg("Open socket");
    int reader_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (reader_fd < 0) {
        throw std::runtime_error("socket: " + std::string(strerror(errno)));
    }

    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {
            .s_addr = htonl(INADDR_ANY)
        },
        .sin_zero = 0,
    };

    Msg("Bind");
    int err = bind(reader_fd, (struct sockaddr *)&sa, sizeof(sa));
    if (err == -1) {
        close(reader_fd);
        throw std::runtime_error("bind: " + std::string(strerror(errno)));
    }

    Msg("Listen");
    backlog = backlog > 0 && backlog < SOMAXCONN ? backlog : SOMAXCONN;
    err = listen(reader_fd, static_cast<int>(backlog));
    if (err == -1) {
        close(reader_fd);
        throw std::runtime_error("listen: " + std::string(strerror(errno)));
    }

    return reader_fd;
}


/// @brief Read from a connection and print the data to the console.
/// @param fd The file descriptor of interest.
/// @return The number of bytes read.
static size_t ReadFromConnection(int fd)
{
    // Read from existing connection
    Msg("Ready (%d)", fd);
    size_t value = 0;

    char rxbuf[RXBUFLEN];
    ssize_t n = recv(fd, rxbuf, RXBUFLEN, 0);
    if (n == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Err("recv: %s", strerror(errno));
        }
    } else if (n > 0) {
        // TODO Add flag to enable printing received data
        //std::cout << '[' << fd << "] " << std::string(rxbuf, n) << std::endl;
        value = static_cast<size_t>(n);
    } else {
        throw no_data_exception();
    }

    return value;
}

/// @brief Set the file descitor to non-blocking mode.
/// @param fd A file descriptor.
static void SetNonBlocking(int fd)
{
    Msg("Set non-blocking (%d)", fd);
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl F_GETFL: " + std::string(strerror(errno)));
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        throw std::runtime_error("fcntl F_SETFL: " + std::string(strerror(errno)));
    }
}

/// @brief Run a socket server that waits on connections with epoll(7).
/// @param opts Program options.
static void SocketServerEpoll(const Options& opts)
{
    Msg("%s", __func__);

    static constexpr int TIMEOUT_MS = 100;   // 0.1 seconds
    static constexpr int MAX_EVENTS = 16;

    int reader_fd = OpenListenerSocket(opts.port, opts.number);

    Msg("Initialize epoll set");
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        throw std::runtime_error("epoll_create1: " + std::string(strerror(errno)));
    }

    // Add listener socket to epoll set.
    std::vector<int> connections;
    connections.reserve(opts.number);
    connections.push_back(reader_fd);

    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLHUP | EPOLLERR,
        .data = {
            .fd = reader_fd
        }
     };
    int err = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, reader_fd, &ev);
    if (err == -1) {
        close(reader_fd);
        close(epoll_fd);
        throw std::runtime_error("epoll_ctl: " + std::string(strerror(errno)));
    };

    Msg("Accept connections");
    auto start_time = std::chrono::steady_clock::now();
    auto stop_time = start_time + std::chrono::seconds(opts.seconds);

    size_t total_bytes = 0;
    size_t total_lines = 0;

    while (std::chrono::steady_clock::now() < stop_time) {
        struct epoll_event events[MAX_EVENTS];
        memset(events, 0, sizeof(events));
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, TIMEOUT_MS);
        if (n == -1) {
            Err("epoll_wait: %s", strerror(errno));
            if (errno == EINTR) {
                continue;   // interrupted by signal, do over
            } else {
                break;      // unrecoverable error, give up
            }
        } else if (n > 0) {
            for (int i = 0; i < n; ++i) {
                if (events[i].data.fd == reader_fd) {
                    AcceptNewConnection(epoll_fd, reader_fd, connections);
                } else if ((events[i].events & EPOLLHUP) != 0) {
                    Err("HUP (%d)", events[i].data.fd);
                    CloseConnection(epoll_fd, events[i].data.fd, connections);
                }
                else if ((events[i].events & EPOLLERR) != 0) {
                    Err("Error (%d)", events[i].data.fd);
                    CloseConnection(epoll_fd, events[i].data.fd, connections);
                } else {
                    try {
                        total_lines++;
                        total_bytes += ReadFromConnection(events[i].data.fd);
                    } catch (const no_data_exception&) {
                        CloseConnection(epoll_fd, events[i].data.fd, connections);
                    } catch (const std::runtime_error& e) {
                        Err("Error (%d): %s", events[i].data.fd, e.what());
                        CloseConnection(epoll_fd, events[i].data.fd, connections);
                    }
                }
            }
        } // else n == 0 -> timed out
    }

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
    PrintSummary(total_bytes, total_lines, duration_ms);

    for (int fd : connections) {
        Msg("Close connection (%d)", fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);    // not necessary
        close(fd);
    }
    close(epoll_fd);

    Msg("Reader offline");
}

static void SocketServerPoll(const Options& opts)
{
    Msg("%s", __func__);

    static constexpr int TIMEOUT_MS = 100;   // 0.1 seconds
    const size_t MAX_CONNS = opts.number > 0 ? static_cast<size_t>(opts.number) : std::numeric_limits<size_t>::max();

    int reader_fd = OpenListenerSocket(opts.port, opts.number);

    Msg("Accepting connections");
    auto start_time = std::chrono::steady_clock::now();
    auto stop_time = start_time + std::chrono::seconds(opts.seconds);

    auto connections = std::unique_ptr<struct pollfd[]>(new struct pollfd[opts.number]);
    size_t num_conns = 0;
    size_t total_bytes = 0;

    connections[num_conns++] = {reader_fd, POLLIN, 0};

    while (std::chrono::steady_clock::now() < stop_time) {
        int n = poll(connections.get(), num_conns, TIMEOUT_MS);
        if (n == -1) {
            Err("poll: %s", strerror(errno));
        } else if (n > 0) {
            for (size_t i = 0; i < num_conns; ++i) {
                if ((connections[i].revents & POLLHUP) != 0) {
                    Err("HUP (%d)", connections[i].fd);
                    close(connections[i].fd);
                    connections[i].fd = -1;
                    connections[i] = connections[num_conns - 1];
                    num_conns--;
                    i--;
                }
                else if ((connections[i].revents & POLLNVAL) != 0) {
                    Err("Invalid (%d)", connections[i].fd);
                    connections[i].fd = -1;
                    connections[i] = connections[num_conns - 1];
                    num_conns--;
                    i--;
                }
                else if ((connections[i].revents & POLLERR) != 0) {
                    Err("Error (%d, %04X)", connections[i].fd, connections[i].revents);
                    close(connections[i].fd);
                    connections[i].fd = -1;
                    connections[i].events = 0;
                    // connections[i] = connections[num_conns - 1];
                    // num_conns--;
                    // i--;
                }
                else if ((connections[i].revents & POLLIN) != 0) {
                    if (connections[i].fd == reader_fd) {
                        // Accept new connection
                        // TODO Grow the connections array as needed.
                        if (num_conns >= MAX_CONNS) {
                            // At capacity, accept and close immediately.
                            Err("Too many connections");
                            struct sockaddr_in remote_addr;
                            memset(&remote_addr, 0, sizeof(remote_addr));
                            socklen_t remote_len = sizeof(remote_addr);
                            int remote_fd = accept(reader_fd, (struct sockaddr *)&remote_addr, &remote_len);
                            if (remote_fd != -1) {
                                close(remote_fd);
                            }
                        } else {
                            // Add the new connection to connections array.
                            struct sockaddr_in remote_addr;
                            memset(&remote_addr, 0, sizeof(remote_addr));
                            socklen_t remote_len = sizeof(remote_addr);
                            int remote_fd = accept(reader_fd, (struct sockaddr *)&remote_addr, &remote_len);
                            if (remote_fd == -1) {
                                Err("accept: %s", strerror(errno));
                            } else {
                                Msg("Accepted new connection (%d)", remote_fd);
                                connections[num_conns] = {remote_fd, POLLIN, 0};
                                num_conns++;
                            }
                        }
                    } else {
                        // Read from existing connection
                        Msg("Ready (%d)", connections[i].fd);
                        char rxbuf[RXBUFLEN];
                        ssize_t n = recv(connections[i].fd, rxbuf, RXBUFLEN, 0);
                        if (n == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                continue;   // skip this one
                            } else {
                                Err("recv: %s", strerror(errno));
                            }
                        } else if (n > 0) {
                            total_bytes += n;
                            std::cout << '[' << connections[i].fd << "] " << std::string(rxbuf, n) << std::endl;
                        } else {
                            // Is this case handled above by POLLHUP?
                            Msg("Connection closed (%d)", connections[i].fd);
                            close(connections[i].fd);
                            connections[i].fd = -1;
                            connections[i] = connections[num_conns - 1];
                            num_conns--;
                            i--;
                        }
                    }
                }
            }
        } // else n == 0 -> timed out
    }

    Msg("Read %zu bytes", total_bytes);
    Msg("Reader offline");
    close(reader_fd);

    for (size_t i = 0; i < num_conns; ++i) {
        if (connections[i].fd > 0) {
            Msg("Close connection (%d)", connections[i].fd);
            close(connections[i].fd);
            connections[i].fd = -1;
        }
    }
}

static void SocketServerSelect(const Options& opts)
{
    Msg("%s", __func__);

    int reader_fd = OpenListenerSocket(opts.port, opts.number);
    Msg("Accepting connections");
    auto start_time = std::chrono::steady_clock::now();
    auto stop_time = start_time + std::chrono::seconds(opts.seconds);

    size_t total_bytes = 0;

    std::vector<int> connections;
    connections.reserve(opts.number);
    connections.push_back(reader_fd);

    while (std::chrono::steady_clock::now() < stop_time) {
        fd_set readfds;
        FD_ZERO(&readfds);
        fd_set exceptfds;
        FD_ZERO(&exceptfds);
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 100000   // 0.1 seconds
        };

        // Set management could be smarter by keeping an active fd_set and a
        // working fd_set and copying the active one to the working one before
        // each select call.  Update the active fd_set and maxfd when a new
        // connection is accepted.
        int maxfd = 0;
        for (int fd : connections) {
            if (fd > 0 && fd < FD_SETSIZE) {
                FD_SET(fd, &readfds);
                FD_SET(fd, &exceptfds);
                if (fd > maxfd) {
                    maxfd = fd;
                }
            } else if (fd >= FD_SETSIZE) {
                Err("File descriptor exceeds FD_SETSIZE: %d", fd);
                // TODO Close this connection and remove from connections array.
            }
        }

        int n = select(maxfd + 1, &readfds, nullptr, &exceptfds, &timeout);
        if (n == -1) {
            Err("select: %s", strerror(errno));
        } else if (n > 0) {
            for (size_t i = 0; i < connections.size(); ++i) {
                int fd = connections[i];
                if (fd < 0 || fd >= FD_SETSIZE) {
                    continue;
                }
                if (FD_ISSET(fd, &exceptfds)) {
                    Err("Exception (%d)", fd);
                    close(fd);
                    connections[i] = -1;
                    //connections.erase(connections.begin() + i);
                    //--i;
                    continue;
                }
                if (FD_ISSET(fd, &readfds)) {
                    if (fd == reader_fd) {
                        // Accept new connection
                        struct sockaddr_in remote_addr;
                        memset(&remote_addr, 0, sizeof(remote_addr));
                        socklen_t remote_len = sizeof(remote_addr);
                        int remote_fd = accept(reader_fd, (struct sockaddr *)&remote_addr, &remote_len);
                        if (remote_fd == -1) {
                            Err("accept: %s", strerror(errno));
                        } else {
                            Msg("Accepted new connection (%d)", remote_fd);
                            connections.push_back(remote_fd);
                        }
                    } else {
                        // Read from existing connection
                        Msg("Ready (%d)", fd);
                        char rxbuf[RXBUFLEN];
                        ssize_t n = recv(fd, rxbuf, RXBUFLEN, 0);
                        if (n == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                continue;   // skip this one
                            } else {
                                Err("recv: %s", strerror(errno));
                            }
                        } else if (n > 0) {
                            total_bytes += n;
                            std::cout << '[' << fd << "] " << std::string(rxbuf, n) << std::endl;
                        } else {
                            Msg("Connection closed (%d)", fd);
                            close(fd);
                            connections[i] = -1;
                        }
                    }
                }
            } // else n == 0 -> timed out
        }
        // TODO Move connections array clean up to here
        // TODO (remove closed connections)
    }

    Msg("Read %zu bytes", total_bytes);
    Msg("Reader offline");
    close(reader_fd);

    for (int fd : connections) {
        if (fd > 0) {
            close(fd);
        }
    }
}
