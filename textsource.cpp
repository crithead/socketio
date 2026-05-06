/// A Source of text.

#include <memory>

#include "textsource.hpp"

TextSource::TextSource(const std::string& filename) :
    input(filename)
{
}

TextSource::~TextSource()
{
    input.close();
}

std::string TextSource::GetText(size_t length)
{
    std::string text;

    if (input.eof()) {
        input.clear();
        input.seekg(0, std::ios::beg);
    }

    if (length == 0) {
        std::getline(input, text);
    } else {
        std::unique_ptr<char[]> buffer(new char[length + 1]);
        input.read(buffer.get(), length);
        buffer[input.gcount()] = '\0';
        text = buffer.get();
    }

    return text;
}
