/*
 * Wii U address-bar navigation helper.
 *
 * Kept in WKC so the behaviour remains isolated from upstream WebKit files.
 * Plain text is sent to DuckDuckGo's HTML results endpoint to avoid depending
 * on a JavaScript-heavy search-results page.
 */

#include "config.h"
#include "WKCWebView.h"

#include <cctype>
#include <string>

namespace WKC {
namespace {

bool isASCIIWhitespace(unsigned char character)
{
    return character == ' ' || character == '\t' || character == '\n'
        || character == '\r' || character == '\f' || character == '\v';
}

std::string trimInput(const char* input)
{
    std::string result(input ? input : "");
    size_t start = 0;
    while (start < result.size() && isASCIIWhitespace(static_cast<unsigned char>(result[start])))
        ++start;

    size_t end = result.size();
    while (end > start && isASCIIWhitespace(static_cast<unsigned char>(result[end - 1])))
        --end;

    return result.substr(start, end - start);
}

char lowerASCII(char character)
{
    if (character >= 'A' && character <= 'Z')
        return static_cast<char>(character - 'A' + 'a');
    return character;
}

bool startsWithIgnoringASCIICase(const std::string& value, const char* prefix)
{
    size_t length = 0;
    while (prefix[length])
        ++length;

    if (value.size() < length)
        return false;

    for (size_t i = 0; i < length; ++i) {
        if (lowerASCII(value[i]) != lowerASCII(prefix[i]))
            return false;
    }
    return true;
}

bool hasNavigableScheme(const std::string& value)
{
    static const char* schemes[] = {
        "http:", "https:", "ftp:", "file:", "about:", "data:",
        "javascript:", "mailto:", "ws:", "wss:"
    };

    for (const char* scheme : schemes) {
        if (startsWithIgnoringASCIICase(value, scheme))
            return true;
    }
    return false;
}

bool containsWhitespace(const std::string& value)
{
    for (unsigned char character : value) {
        if (isASCIIWhitespace(character))
            return true;
    }
    return false;
}

bool looksLikeAddress(const std::string& value)
{
    if (containsWhitespace(value))
        return false;

    // Most public host names and IPv4 addresses contain a dot. Treat local
    // development hosts separately because "localhost:port" has no dot.
    if (value.find('.') != std::string::npos)
        return true;

    if (startsWithIgnoringASCIICase(value, "localhost")
        && (value.size() == 9 || value[9] == ':' || value[9] == '/'))
        return true;

    // Accept bracketed IPv6 literals such as [::1] and [2001:db8::1]:443.
    return !value.empty() && value[0] == '['
        && value.find(':') != std::string::npos
        && value.find(']') != std::string::npos;
}

std::string encodeQueryComponent(const std::string& query)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(query.size() * 3);

    for (unsigned char character : query) {
        bool unreserved = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '-' || character == '.' || character == '_'
            || character == '~';

        if (unreserved) {
            encoded.push_back(static_cast<char>(character));
            continue;
        }

        encoded.push_back('%');
        encoded.push_back(hex[(character >> 4) & 0x0F]);
        encoded.push_back(hex[character & 0x0F]);
    }

    return encoded;
}

} // anonymous namespace

bool WKCWebView::loadURLOrSearch(const char* input, const char* referrer)
{
    std::string value = trimInput(input);
    if (value.empty())
        return false;

    std::string target;
    if (hasNavigableScheme(value))
        target = value;
    else if (looksLikeAddress(value))
        target = "https://" + value;
    else
        target = "https://html.duckduckgo.com/html/?q=" + encodeQueryComponent(value);

    loadURI(target.c_str(), referrer);
    return true;
}

} // namespace WKC
