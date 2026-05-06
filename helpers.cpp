/// Helper Functions

#include <sys/resource.h>
#include <sys/stat.h>

#include <chrono>
#include <cstdarg>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

#include "helpers.hpp"

static bool _messages_on = false;

/// Adjust resource limits for the process.
/// * Set nofile (file descriptor) soft limit to hard limit.
void AdjustLimits(void)
{
    struct rlimit rls;

    if (getrlimit(RLIMIT_NOFILE, &rls) == -1) {
        throw std::runtime_error("Failed to get resource limit");
    }

    rls.rlim_cur = rls.rlim_max;

    if (setrlimit(RLIMIT_NOFILE, &rls) == -1) {
        throw std::runtime_error("Failed to set resource limit");
    }

    Msg("Adjusted file descriptor limit to %lu", rls.rlim_cur);
}

// Create a directory path.
void CreateDirectory(const std::string& dir)
{
    // creates parent directories as needed like "mkdir -p"
    std::filesystem::create_directories(dir);
}

// Check that a directory exists.
bool DirectoryExists(const std::string& dir)
{
    return std::filesystem::exists(dir) && std::filesystem::is_directory(dir);
}

void EnableMessages(bool enable)
{
    _messages_on = enable;
}

void Msg(const char *fmt, ...)
{
    if (_messages_on) {
        static constexpr size_t LINESIZE = 4096;
        static char line[LINESIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(line, LINESIZE, fmt, args);
        va_end(args);
        std::cout << line << std::endl;
    }
}

void Err(const char *fmt, ...)
{
    static constexpr size_t LINESIZE = 4096;
    static char line[LINESIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, LINESIZE, fmt, args);
    va_end(args);
    std::cerr << line << std::endl;
}

// Pause the program for the specified number of milliseconds.
void Pause(size_t milliseconds)
{
    if (milliseconds > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
}

// Print a summary of data processed.
void PrintSummary(size_t bytes, size_t lines, size_t milliseconds)
{
    size_t minutes = 0;
    size_t seconds = 0;
    if (milliseconds >= 60000) {
        minutes = milliseconds / 60000;
        milliseconds = milliseconds % 60000;
    }
    if (milliseconds >= 1000) {
        seconds = milliseconds / 1000;
        milliseconds = milliseconds % 1000;
    }

    if (minutes > 0) {
        std::cout << "Processed " << bytes << " bytes in " << lines << " lines for " << minutes << "m " << seconds << "s " << milliseconds << "ms" << std::endl;
    } else if (seconds > 0) {
        std::cout << "Processed " << bytes << " bytes in " << lines << " lines for " << seconds << "s " << milliseconds << "ms" << std::endl;
    } else {
        std::cout << "Processed " << bytes << " bytes in " << lines << " lines for " << milliseconds << "ms" << std::endl;
    }
}

// Generate a random integer.
int RandInt(int a, int b)
{
    if (a == b) {
        return a;
    } else if (a > b) {
        a = a ^ b;
        b = a ^ b;
        a = a ^ b;
    }
    static std::random_device generator;
    std::uniform_int_distribution<int> distribution(a, b);
    return distribution(generator);
}
