/// @file helpers.hpp
/// @brief Common functions used by both reader and writer.

/// @brief Adjust resource limits for this process.
/// * Set nofile (file descriptor) soft limit to hard limit.
/// @throws std::runtime_error
extern void AdjustLimits(void);

/// @brief Create a directory path.
/// @param dir The directory path to create.
extern void CreateDirectory(const std::string& dir);

/// @brief Check that a directory exists.
/// @param dir The directory path to check
/// @retval true The directory exists
/// @retval false The directory does not exist
extern bool DirectoryExists(const std::string& dir);

/// @brief Enable or disable messages.
/// @param enable Set true to enable messages, false to disable.
extern void EnableMessages(bool enable);

/// @brief Print a message if enabled.
/// @param fmt Printf-style format string
/// @param ... Variable arguments for the format string
extern void Msg(const char *fmt, ...);

/// @brief Print an error message.
/// @param fmt Printf-style format string
/// @param ... Variable arguments for the format string
extern void Err(const char *fmt, ...);

/// @brief  Pause the program for the specified number of milliseconds.
/// @param milliseconds Number of milliseconds to sleep.
extern void Pause(size_t milliseconds);

/// @brief Print a summary of data processed.
/// @param bytes Number of bytes
/// @param lines Number of lines
/// @param milliseconds Number of milliseconds
extern void PrintSummary(size_t bytes, size_t lines, size_t milliseconds);

/// @brief Get a random integer in [a, b].
/// @param a The minimum value.
/// @param b The maximum value.
/// @return The random integer.
extern int RandInt(int a, int b);
