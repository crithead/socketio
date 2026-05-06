/// A Source of text.

#include <string>
#include <fstream>

/// @brief  A source of text.
class TextSource
{
    public:

    /// @brief  Construct a TextSource object from the specified file.
    /// @param filename A file containing text.
    TextSource(const std::string& filename);

    /// @brief  Destructor.
    ~TextSource();

    /// @brief  Get text from the source.
    /// Reads text froma file. If the end of file is reached, it will wrap
    /// around to the beginning of the file so it never runs out.
    /// @param length Maximum length of text to return.
    ///               If 0, return one line of text.
    /// @return A string of text.
    std::string GetText(size_t length = 0);

    private:
    std::ifstream input;
};
