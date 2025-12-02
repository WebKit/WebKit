/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <wtf/URL.h>

#include "URLParser.h"
#include <ranges>
#include <stdio.h>
#include <unicode/uidna.h>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UUID.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextStream.h>

namespace WTF {

void URL::invalidate()
{
    m_isValid = false;
    m_protocolIsInHTTPFamily = false;
    m_hasOpaquePath = false;
    m_schemeEnd = 0;
    m_userStart = 0;
    m_userEnd = 0;
    m_passwordEnd = 0;
    m_hostEnd = 0;
    m_portLength = 0;
    m_pathEnd = 0;
    m_pathAfterLastSlash = 0;
    m_queryEnd = 0;
}

URL::URL(const URL& base, const String& relative, const URLTextEncoding* encoding)
{
    *this = URLParser(String { relative },  base, encoding).result();
}

URL::URL(String&& absoluteURL, const URLTextEncoding* encoding)
{
    *this = URLParser(WTFMove(absoluteURL), URL(), encoding).result();
}

static bool shouldTrimFromURL(char16_t character)
{
    // Ignore leading/trailing whitespace and control characters.
    return character <= ' ';
}

URL URL::isolatedCopy() const &
{
    URL result = *this;
    result.m_string = result.m_string.isolatedCopy();
    return result;
}

URL URL::isolatedCopy() &&
{
    URL result { WTFMove(*this) };
    result.m_string = WTFMove(result.m_string).isolatedCopy();
    return result;
}

StringView URL::lastPathComponent() const
{
    if (!hasPath())
        return { };

    unsigned end = m_pathEnd - 1;
    if (m_string[end] == '/')
        --end;

    size_t start = m_string.reverseFind('/', end);
    if (start < pathStart())
        return { };
    ++start;

    return StringView(m_string).substring(start, end - start + 1);
}

bool URL::hasSpecialScheme() const
{
    // https://url.spec.whatwg.org/#special-scheme
    return protocolIs("ftp"_s)
        || protocolIsFile()
        || protocolIs("http"_s)
        || protocolIs("https"_s)
        || protocolIs("ws"_s)
        || protocolIs("wss"_s);
}

bool URL::hasLocalScheme() const
{
    // https://fetch.spec.whatwg.org/#local-scheme
    return protocolIsAbout()
        || protocolIsBlob()
        || protocolIsData();
}

bool URL::hasFetchScheme() const
{
    // https://fetch.spec.whatwg.org/#fetch-scheme
    return protocolIsInHTTPFamily()
        || protocolIsAbout()
        || protocolIsBlob()
        || protocolIsData()
        || protocolIsFile();
}

bool URL::protocolIsSecure() const
{
    // Note: FTPS is not considered secure for WebKit purposes.
    return protocolIs("https"_s)
        || protocolIs("wss"_s);
}

unsigned URL::pathStart() const
{
    unsigned start = m_hostEnd + m_portLength;
    if (start == m_schemeEnd + 1U
        && start + 1 < m_string.length()
        && m_string[start] == '/' && m_string[start + 1] == '.')
        start += 2;
    return start;
}

StringView URL::protocol() const
{
    if (!m_isValid)
        return { };

    return StringView(m_string).left(m_schemeEnd);
}

StringView URL::host() const
{
    if (!m_isValid)
        return { };

    unsigned start = hostStart();
    return StringView(m_string).substring(start, m_hostEnd - start);
}

std::optional<uint16_t> URL::port() const
{
    return m_portLength ? parseInteger<uint16_t>(StringView(m_string).substring(m_hostEnd + 1, m_portLength - 1)) : std::nullopt;
}

String URL::hostAndPort() const
{
    if (auto port = this->port())
        return makeString(host(), ':', port.value());
    return host().toString();
}

String URL::protocolHostAndPort() const
{
    if (!hasCredentials())
        return m_string.left(pathStart());

    return makeString(
        StringView(m_string).left(m_userStart),
        StringView(m_string).substring(hostStart(), pathStart() - hostStart())
    );
}

static std::optional<Latin1Character> decodeEscapeSequence(StringView input, unsigned index, unsigned length)
{
    if (index + 3 > length || input[index] != '%')
        return std::nullopt;
    auto digit1 = input[index + 1];
    auto digit2 = input[index + 2];
    if (!isASCIIHexDigit(digit1) || !isASCIIHexDigit(digit2))
        return std::nullopt;
    return toASCIIHexValue(digit1, digit2);
}

static String decodeEscapeSequencesFromParsedURL(StringView input)
{
    ASSERT(input.containsOnlyASCII());

    auto length = input.length();
    if (length < 3 || !input.contains('%'))
        return input.toString();

    // FIXME: This 100 is arbitrary. Should make a histogram of how this function is actually used to choose a better value.
    Vector<Latin1Character, 100> percentDecoded;
    percentDecoded.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ) {
        if (auto decodedCharacter = decodeEscapeSequence(input, i, length)) {
            percentDecoded.append(*decodedCharacter);
            i += 3;
        } else {
            percentDecoded.append(input[i]);
            ++i;
        }
    }

    // FIXME: Is UTF-8 always the correct encoding?
    // FIXME: This returns a null string when we encounter an invalid UTF-8 sequence. Is that OK?
    return String::fromUTF8(percentDecoded.span());
}

String URL::user() const
{
    return decodeEscapeSequencesFromParsedURL(encodedUser());
}

String URL::password() const
{
    return decodeEscapeSequencesFromParsedURL(encodedPassword());
}

StringView URL::encodedUser() const
{
    return StringView(m_string).substring(m_userStart, m_userEnd - m_userStart);
}

StringView URL::encodedPassword() const
{
    if (m_passwordEnd == m_userEnd)
        return { };

    return StringView(m_string).substring(m_userEnd + 1, m_passwordEnd - m_userEnd - 1);
}

StringView URL::fragmentIdentifier() const
{
    if (!hasFragmentIdentifier())
        return { };

    return StringView(m_string).substring(m_queryEnd + 1);
}

// https://wicg.github.io/scroll-to-text-fragment/#the-fragment-directive
String URL::consumeFragmentDirective()
{
    ASCIILiteral fragmentDirectiveDelimiter = ":~:"_s;
    auto fragment = fragmentIdentifier();
    
    auto fragmentDirectiveStart = fragment.find(fragmentDirectiveDelimiter);
    
    if (fragmentDirectiveStart == notFound)
        return { };
    
    auto fragmentDirective = fragment.substring(fragmentDirectiveStart + fragmentDirectiveDelimiter.length()).toString();
    
    auto remainingFragment = fragment.left(fragmentDirectiveStart);
    if (remainingFragment.isEmpty())
        removeFragmentIdentifier();
    else
        setFragmentIdentifier(remainingFragment);
    
    return fragmentDirective;
}

URL URL::truncatedForUseAsBase() const
{
    return URL(m_string.left(m_pathAfterLastSlash));
}

#if !USE(CF)

String URL::fileSystemPath() const
{
    if (!protocolIsFile())
        return { };

    auto result = decodeEscapeSequencesFromParsedURL(path());
#if PLATFORM(WIN)
    if (result.startsWith('/'))
        result = result.substring(1);
#endif
    return result;
}

#endif

#if !ASSERT_ENABLED

static inline void assertProtocolIsGood(StringView)
{
}

#else

static void assertProtocolIsGood(StringView protocol)
{
    // FIXME: We probably don't need this function any more.
    // The isASCIIAlphaCaselessEqual function asserts that passed-in characters
    // are ones it can handle; the older code did not and relied on these checks.
    for (auto character : protocol.codeUnits()) {
        ASSERT(isASCII(character));
        ASSERT(character > ' ');
        ASSERT(!isASCIIUpper(character));
        ASSERT(toASCIILowerUnchecked(character) == character);
    }
}

#endif

static Lock defaultPortForProtocolMapForTestingLock;

using DefaultPortForProtocolMapForTesting = HashMap<String, uint16_t>;
static DefaultPortForProtocolMapForTesting*& defaultPortForProtocolMapForTesting() WTF_REQUIRES_LOCK(defaultPortForProtocolMapForTestingLock)
{
    static DefaultPortForProtocolMapForTesting* defaultPortForProtocolMap;
    return defaultPortForProtocolMap;
}

static DefaultPortForProtocolMapForTesting& ensureDefaultPortForProtocolMapForTesting() WTF_REQUIRES_LOCK(defaultPortForProtocolMapForTestingLock)
{
    DefaultPortForProtocolMapForTesting*& defaultPortForProtocolMap = defaultPortForProtocolMapForTesting();
    if (!defaultPortForProtocolMap)
        defaultPortForProtocolMap = new DefaultPortForProtocolMapForTesting;
    return *defaultPortForProtocolMap;
}

void registerDefaultPortForProtocolForTesting(uint16_t port, const String& protocol)
{
    Locker locker { defaultPortForProtocolMapForTestingLock };
    ensureDefaultPortForProtocolMapForTesting().add(protocol, port);
}

void clearDefaultPortForProtocolMapForTesting()
{
    Locker locker { defaultPortForProtocolMapForTestingLock };
    if (auto* map = defaultPortForProtocolMapForTesting())
        map->clear();
}

std::optional<uint16_t> defaultPortForProtocol(StringView protocol)
{
    {
        Locker locker { defaultPortForProtocolMapForTestingLock };
        if (auto* overrideMap = defaultPortForProtocolMapForTesting()) {
            auto iterator = overrideMap->find<StringViewHashTranslator>(protocol);
            if (iterator != overrideMap->end())
                return iterator->value;
        }
    }
    return URLParser::defaultPortForProtocol(protocol);
}

bool isDefaultPortForProtocol(uint16_t port, StringView protocol)
{
    return defaultPortForProtocol(protocol) == port;
}

bool URL::protocolIsJavaScript() const
{
    return WTF::protocolIsJavaScript(string());
}

bool URL::protocolIs(StringView protocol) const
{
    assertProtocolIsGood(protocol);

    if (!m_isValid)
        return false;
    
    if (m_schemeEnd != protocol.length())
        return false;

    // Do the comparison without making a new string object.
    for (unsigned i = 0; i < m_schemeEnd; ++i) {
        if (!isASCIIAlphaCaselessEqual(m_string[i], protocol[i]))
            return false;
    }
    return true;
}

StringView URL::query() const
{
    if (m_queryEnd == m_pathEnd)
        return { };

    return StringView(m_string).substring(m_pathEnd + 1, m_queryEnd - (m_pathEnd + 1));
}

StringView URL::path() const
{
    if (!m_isValid)
        return { };

    return StringView(m_string).substring(pathStart(), m_pathEnd - pathStart());
}

bool URL::setProtocol(StringView newProtocol)
{
    auto newProtocolPrefix = newProtocol.left(newProtocol.find(':'));
    auto newProtocolCanonicalized = URLParser::maybeCanonicalizeScheme(newProtocolPrefix);
    if (!newProtocolCanonicalized)
        return false;

    if (!m_isValid) {
        parse(makeString(*newProtocolCanonicalized, ':', m_string));
        return true;
    }

    if (URLParser::isSpecialScheme(this->protocol()) != URLParser::isSpecialScheme(*newProtocolCanonicalized))
        return true;

    if ((m_passwordEnd != m_userStart || port()) && *newProtocolCanonicalized == "file"_s)
        return true;

    if (protocolIsFile() && host().isEmpty())
        return true;

    parse(makeString(*newProtocolCanonicalized, StringView(m_string).substring(m_schemeEnd)));
    return true;
}

// Appends the punycoded hostname identified by the given string and length to
// the output buffer. The result will not be null terminated.
// Return value of false means error in encoding.
static bool appendEncodedHostname(Vector<char16_t, 512>& buffer, StringView string)
{
    // hostnameBuffer needs to be big enough to hold an IDN-encoded name.
    // For host names bigger than this, we won't do IDN encoding, which is almost certainly OK.
    if (string.length() > URLParser::hostnameBufferLength || string.containsOnlyASCII()) {
        append(buffer, string);
        return true;
    }

    std::array<char16_t, URLParser::hostnameBufferLength> hostnameBuffer;
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo processingDetails = UIDNA_INFO_INITIALIZER;
    // struct UIDNA is forward declared in WTF/icu/unicode/uidna.h.
    SUPPRESS_FORWARD_DECL_ARG int32_t numCharactersConverted = uidna_nameToASCII(&URLParser::internationalDomainNameTranscoder(),
        string.upconvertedCharacters(), string.length(), hostnameBuffer.data(), hostnameBuffer.size(), &processingDetails, &error);

    if (U_SUCCESS(error) && !(processingDetails.errors & ~URLParser::allowedNameToASCIIErrors) && numCharactersConverted) {
        buffer.append(std::span { hostnameBuffer }.first(numCharactersConverted));
        return true;
    }
    return false;
}

unsigned URL::hostStart() const
{
    return (m_passwordEnd == m_userStart) ? m_passwordEnd : m_passwordEnd + 1;
}

unsigned URL::credentialsEnd() const
{
    // Include '@' too if we have it.
    unsigned end = m_passwordEnd;
    if (end != m_hostEnd && m_string[end] == '@')
        end += 1;
    return end;
}

static bool forwardSlashHashOrQuestionMark(char16_t c)
{
    return c == '/'
        || c == '#'
        || c == '?';
}

static bool slashHashOrQuestionMark(char16_t c)
{
    return forwardSlashHashOrQuestionMark(c) || c == '\\';
}

bool URL::setHost(StringView newHost)
{
    if (!m_isValid || hasOpaquePath())
        return false;

    if (auto index = newHost.find(hasSpecialScheme() ? slashHashOrQuestionMark : forwardSlashHashOrQuestionMark); index != notFound)
        newHost = newHost.left(index);

    if (newHost.contains('@'))
        return false;

    if (newHost.contains(':') && !newHost.startsWith('['))
        return false;

    Vector<char16_t, 512> encodedHostName;
    if (hasSpecialScheme() && !appendEncodedHostname(encodedHostName, newHost))
        return false;

    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
    parse(makeString(
        StringView(m_string).left(hostStart()),
        slashSlashNeeded ? "//"_s : ""_s,
        hasSpecialScheme() ? StringView(encodedHostName.span()) : newHost,
        StringView(m_string).substring(m_hostEnd)
    ));

    return m_isValid;
}

void URL::setPort(std::optional<uint16_t> port)
{
    if (!m_isValid)
        return;

    if (!port) {
        remove(m_hostEnd, m_portLength);
        return;
    }

    parse(makeString(
        StringView(m_string).left(m_hostEnd),
        ':',
        static_cast<unsigned>(*port),
        StringView(m_string).substring(pathStart())
    ));
}

static unsigned countASCIIDigits(StringView string)
{
    unsigned length = string.length();
    for (unsigned count = 0; count < length; ++count) {
        if (!isASCIIDigit(string[count]))
            return count;
    }
    return length;
}

void URL::setHostAndPort(StringView hostAndPort)
{
    if (!m_isValid || hasOpaquePath())
        return;

    if (auto index = hostAndPort.find(hasSpecialScheme() ? slashHashOrQuestionMark : forwardSlashHashOrQuestionMark); index != notFound)
        hostAndPort = hostAndPort.left(index);

    auto colonIndex = hostAndPort.reverseFind(':');
    if (!colonIndex)
        return;

    auto ipv6Separator = hostAndPort.reverseFind(']');
    if (colonIndex == notFound || (ipv6Separator != notFound && ipv6Separator > colonIndex)) {
        setHost(hostAndPort);
        return;
    }

    auto portString = hostAndPort.substring(colonIndex + 1);
    auto hostName = hostAndPort.left(colonIndex);
    if (hostName.contains('@'))
        return;
    // Multiple colons are acceptable only in case of IPv6.
    if (hostName.contains(':') && ipv6Separator == notFound)
        return;

    unsigned portLength = countASCIIDigits(portString);
    if (!portLength) {
        setHost(hostName);
        return;
    }
    portString = portString.left(portLength);
    if (!parseInteger<uint16_t>(portString))
        portString = { };

    Vector<char16_t, 512> encodedHostName;
    if (hasSpecialScheme() && !appendEncodedHostname(encodedHostName, hostName))
        return;

    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
    parse(makeString(
        StringView(m_string).left(hostStart()),
        slashSlashNeeded ? "//"_s : ""_s,
        hasSpecialScheme() ? StringView(encodedHostName.span()) : hostName,
        portString.isEmpty() ? ""_s : ":"_s,
        portString,
        StringView(m_string).substring(pathStart())
    ));
}

void URL::removeHostAndPort()
{
    if (m_isValid)
        remove(hostStart(), pathStart() - hostStart());
}

template<typename StringType>
static String percentEncodeCharacters(const StringType& input, bool(*shouldEncode)(char16_t))
{
    auto encode = [shouldEncode] (const StringType& input) {
        auto result = input.tryGetUTF8([&](std::span<const char8_t> span) -> String {
            StringBuilder builder(OverflowPolicy::RecordOverflow);
            for (char c : span) {
                if (shouldEncode(c))
                    builder.append('%', upperNibbleToASCIIHexDigit(c), lowerNibbleToASCIIHexDigit(c));
                else
                    builder.append(c);
                if (builder.hasOverflowed())
                    return { };
            }
            return builder.toString();
        });
        RELEASE_ASSERT(result);
        return result.value();
    };

    for (size_t i = 0; i < input.length(); ++i) {
        if (shouldEncode(input[i])) [[unlikely]]
            return encode(input);
    }
    if constexpr (std::is_same_v<StringType, StringView>)
        return input.toString();
    else
        return input;
}

void URL::parse(String&& string)
{
    *this = URLParser(WTFMove(string)).result();
}

void URL::parseAllowingC0AtEnd(String&& string)
{
    *this = URLParser(WTFMove(string), { }, URLTextEncodingSentinelAllowingC0AtEnd).result();
}

void URL::remove(unsigned start, unsigned length)
{
    if (!length)
        return;
    ASSERT(start < m_string.length());
    ASSERT(length <= m_string.length() - start);

    auto stringAfterRemoval = makeStringByRemoving(std::exchange(m_string, { }), start, length);
    parse(WTFMove(stringAfterRemoval));
}

void URL::setUser(StringView newUser)
{
    if (!m_isValid)
        return;

    unsigned end = m_userEnd;
    if (!newUser.isEmpty()) {
        bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
        bool needSeparator = end == m_hostEnd || (end == m_passwordEnd && m_string[end] != '@');
        parse(makeString(
            StringView(m_string).left(m_userStart),
            slashSlashNeeded ? "//"_s : ""_s,
            percentEncodeCharacters(newUser, URLParser::isInUserInfoEncodeSet),
            needSeparator ? "@"_s : ""_s,
            StringView(m_string).substring(end)
        ));
    } else {
        // Remove '@' if we now have neither user nor password.
        if (m_userEnd == m_passwordEnd && end != m_hostEnd && m_string[end] == '@')
            end += 1;
        remove(m_userStart, end - m_userStart);
    }
}

void URL::setPassword(StringView newPassword)
{
    if (!m_isValid)
        return;

    if (!newPassword.isEmpty()) {
        bool needLeadingSlashes = m_userEnd == m_schemeEnd + 1U;
        parse(makeString(
            StringView(m_string).left(m_userEnd),
            needLeadingSlashes ? "//:"_s : ":"_s,
            percentEncodeCharacters(newPassword, URLParser::isInUserInfoEncodeSet),
            '@',
            StringView(m_string).substring(credentialsEnd())
        ));
    } else {
        unsigned end = m_userStart == m_userEnd ? credentialsEnd() : m_passwordEnd;
        remove(m_userEnd, end - m_userEnd);
    }
}

void URL::removeCredentials()
{
    if (!m_isValid)
        return;

    remove(m_userStart, credentialsEnd() - m_userStart);
}

void URL::setFragmentIdentifier(StringView identifier)
{
    if (!m_isValid)
        return;

    parseAllowingC0AtEnd(makeString(StringView(m_string).left(m_queryEnd), '#', identifier));
}

void URL::removeFragmentIdentifier()
{
    if (!m_isValid)
        return;

    m_string = m_string.left(m_queryEnd);
}

void URL::removeQueryAndFragmentIdentifier()
{
    if (!m_isValid)
        return;

    m_string = m_string.left(m_pathEnd);
    m_queryEnd = m_pathEnd;
}

void URL::setQuery(StringView newQuery)
{
    // FIXME: Consider renaming this function to setEncodedQuery and/or calling percentEncodeCharacters the way setPath does.
    // https://webkit.org/b/161176

    if (!m_isValid)
        return;

    parseAllowingC0AtEnd(makeString(
        StringView(m_string).left(m_pathEnd),
        (!newQuery.startsWith('?') && !newQuery.isNull()) ? "?"_s : ""_s,
        newQuery,
        StringView(m_string).substring(m_queryEnd)
    ));
}

static String escapePathWithoutCopying(StringView path)
{
    auto questionMarkOrNumberSignOrNonASCII = [] (char16_t character) {
        return character == '?' || character == '#' || !isASCII(character);
    };
    return percentEncodeCharacters(path, questionMarkOrNumberSignOrNonASCII);
}

void URL::setPath(StringView path)
{
    if (!m_isValid)
        return;

    parseAllowingC0AtEnd(makeString(
        StringView(m_string).left(pathStart()),
        path.startsWith('/') || (path.startsWith('\\') && (hasSpecialScheme() || protocolIsFile())) || (!hasSpecialScheme() && path.isEmpty() && m_schemeEnd + 1U < pathStart()) ? ""_s : "/"_s,
        !hasSpecialScheme() && host().isEmpty() && path.startsWith("//"_s) && path.length() > 2 ? "/."_s : ""_s,
        escapePathWithoutCopying(path),
        StringView(m_string).substring(m_pathEnd)
    ));
}

StringView URL::viewWithoutQueryOrFragmentIdentifier() const
{
    if (!m_isValid)
        return m_string;

    return StringView(m_string).left(pathEnd());
}

StringView URL::viewWithoutFragmentIdentifier() const
{
    if (!m_isValid)
        return m_string;

    return StringView(m_string).left(m_queryEnd);
}

String URL::stringWithoutFragmentIdentifier() const
{
    if (!m_isValid)
        return m_string;

    return m_string.left(m_queryEnd);
}

bool equalIgnoringFragmentIdentifier(const URL& a, const URL& b)
{
    return a.viewWithoutFragmentIdentifier() == b.viewWithoutFragmentIdentifier();
}

bool protocolHostAndPortAreEqual(const URL& a, const URL& b)
{
    if (a.m_schemeEnd != b.m_schemeEnd)
        return false;

    unsigned hostStartA = a.hostStart();
    unsigned hostLengthA = a.m_hostEnd - hostStartA;
    unsigned hostStartB = b.hostStart();
    unsigned hostLengthB = b.m_hostEnd - b.hostStart();
    if (hostLengthA != hostLengthB)
        return false;

    // Check the scheme
    for (unsigned i = 0; i < a.m_schemeEnd; ++i) {
        if (toASCIILower(a.string()[i]) != toASCIILower(b.string()[i]))
            return false;
    }

    // And the host
    for (unsigned i = 0; i < hostLengthA; ++i) {
        if (toASCIILower(a.string()[hostStartA + i]) != toASCIILower(b.string()[hostStartB + i]))
            return false;
    }

    if (a.port() != b.port())
        return false;

    return true;
}

bool URL::isMatchingDomain(StringView domain) const
{
    // FIXME: Consider moving this to an appropriate place in WebCore's plug-in code; don't want people tempted to use this instead of SecurityOrigin.

    if (isNull())
        return false;

    if (domain.isEmpty())
        return true;

    if (!protocolIsInHTTPFamily())
        return false;

    auto host = this->host();
    if (!host.endsWith(domain))
        return false;

    return host.length() == domain.length() || host[host.length() - domain.length() - 1] == '.';
}

// FIXME: Rename this so it's clear that it does the appropriate escaping for URL query field values.
String encodeWithURLEscapeSequences(const String& input)
{
    return percentEncodeCharacters(input, URLParser::isInUserInfoEncodeSet);
}

String percentEncodeFragmentDirectiveSpecialCharacters(const String& input)
{
    return percentEncodeCharacters(input, URLParser::isSpecialCharacterForFragmentDirective);
}

static bool protocolIsInternal(StringView string, ASCIILiteral protocol)
{
    assertProtocolIsGood(protocol);
    size_t protocolIndex = 0;
    bool isLeading = true;
    for (auto codeUnit : string.codeUnits()) {
        if (isLeading) {
            // Skip leading whitespace and control characters.
            if (shouldTrimFromURL(codeUnit))
                continue;
            isLeading = false;
        } else {
            // Skip tabs and newlines even later in the protocol.
            if (codeUnit == '\t' || codeUnit == '\r' || codeUnit == '\n')
                continue;
        }


        if (protocolIndex == protocol.length())
            return codeUnit == ':';
        char expectedCharacter = protocol[protocolIndex++];
        if (!isASCIIAlphaCaselessEqual(codeUnit, expectedCharacter))
            return false;
    }
    return false;
}

bool protocolIs(StringView string, ASCIILiteral protocol)
{
    return protocolIsInternal(string, protocol);
}

#ifndef NDEBUG

void URL::print() const
{
    SAFE_PRINTF("%s\n", m_string.utf8());
}

#endif

void URL::dump(PrintStream& out) const
{
    out.print(m_string);
}

URL::StripResult URL::strippedForUseAsReferrer() const
{
    if (!m_isValid)
        return { m_string, false };

    unsigned end = credentialsEnd();

    if (m_userStart == end && m_queryEnd == m_string.length())
        return { m_string, false };

    return { makeString(StringView(m_string).left(m_userStart), StringView(m_string).substring(end, m_queryEnd - end)), true };
}

URL::StripResult URL::strippedForUseAsReferrerWithExplicitPort() const
{
    if (!m_isValid)
        return { m_string, false };

    // Custom ports will appear in the URL string:
    if (m_portLength)
        return strippedForUseAsReferrer();

    auto port = defaultPortForProtocol(protocol());
    if (!port)
        return strippedForUseAsReferrer();

    unsigned end = credentialsEnd();

    if (m_userStart == end && m_queryEnd == m_string.length())
        return { makeString(StringView(m_string).left(m_hostEnd), ':', static_cast<unsigned>(*port), StringView(m_string).substring(pathStart())), true };

    return { makeString(StringView(m_string).left(m_hostEnd), ':', static_cast<unsigned>(*port), StringView(m_string).substring(end, m_queryEnd - end)), true };
}

String URL::strippedForUseAsReport() const
{
    if (!m_isValid)
        return m_string;

    unsigned end = credentialsEnd();

    if (m_userStart == end && m_pathEnd == m_string.length())
        return m_string;

    return makeString(StringView(m_string).left(m_userStart), StringView(m_string).substring(end, m_pathEnd - end));
}

bool protocolIsJavaScript(StringView string)
{
    return protocolIsInternal(string, "javascript"_s);
}

bool protocolIsInHTTPFamily(StringView url)
{
    auto length = url.length();
    // Do the comparison without making a new string object.
    return length >= 5
        && isASCIIAlphaCaselessEqual(url[0], 'h')
        && isASCIIAlphaCaselessEqual(url[1], 't')
        && isASCIIAlphaCaselessEqual(url[2], 't')
        && isASCIIAlphaCaselessEqual(url[3], 'p')
        && (url[4] == ':' || (isASCIIAlphaCaselessEqual(url[4], 's') && length >= 6 && url[5] == ':'));
}


static StaticStringImpl aboutBlankString { "about:blank" };
const URL& aboutBlankURL()
{
    static NeverDestroyed<URL> staticBlankURL { &aboutBlankString };
    return staticBlankURL;
}

static StaticStringImpl aboutSrcDocString { "about:srcdoc" };
const URL& aboutSrcDocURL()
{
    static NeverDestroyed<URL> staticSrcDocURL { &aboutSrcDocString };
    return staticSrcDocURL;
}

bool portAllowed(const URL& url)
{
    std::optional<uint16_t> port = url.port();

    // Since most URLs don't have a port, return early for the "no port" case.
    if (!port)
        return true;

    // This blocked port list is defined by the Fetch spec, with the addition of port 0.
    // See https://fetch.spec.whatwg.org/#port-blocking for more information.
    static const uint16_t blockedPortList[] = {
        0, // reserved
        1, // tcpmux
        7, // echo
        9, // discard
        11, // systat
        13, // daytime
        15, // netstat
        17, // qotd
        19, // chargen
        20, // FTP-data
        21, // FTP-control
        22, // SSH
        23, // telnet
        25, // SMTP
        37, // time
        42, // name
        43, // nicname
        53, // domain
        69, // TFTP
        77, // priv-rjs
        79, // finger
        87, // ttylink
        95, // supdup
        101, // hostriame
        102, // iso-tsap
        103, // gppitnp
        104, // acr-nema
        109, // POP2
        110, // POP3
        111, // sunrpc
        113, // auth
        115, // SFTP
        117, // uucp-path
        119, // nntp
        123, // NTP
        135, // loc-srv / epmap
        137, // NetBIOS
        139, // netbios
        143, // IMAP2
        161, // SNMP
        179, // BGP
        389, // LDAP
        427, // SLP (Also used by Apple Filing Protocol)
        465, // SMTP+SSL
        512, // print / exec
        513, // login
        514, // shell
        515, // printer
        526, // tempo
        530, // courier
        531, // Chat
        532, // netnews
        540, // UUCP
        548, // afpovertcp [Apple addition]
        554, // rtsp
        556, // remotefs
        563, // NNTP+SSL
        587, // ESMTP
        601, // syslog-conn
        636, // LDAP+SSL
        989, // ftps-data
        990, // ftps
        993, // IMAP+SSL
        995, // POP3+SSL
        1719, // H323 (RAS)
        1720, // H323 (Q931)
        1723, // H323 (H245)
        2049, // NFS
        3659, // apple-sasl / PasswordServer [Apple addition]
        4045, // lockd
        4190, // ManageSieve [Apple addition]
        5060, // SIP
        5061, // SIPS
        6000, // X11
        6566, // SANE
        6665, // Alternate IRC [Apple addition]
        6666, // Alternate IRC [Apple addition]
        6667, // Standard IRC [Apple addition]
        6668, // Alternate IRC [Apple addition]
        6669, // Alternate IRC [Apple addition]
        6679, // Alternate IRC SSL [Apple addition]
        6697, // IRC+SSL [Apple addition]
        10080, // amanda
    };

    // If the port is not in the blocked port list, allow it.
    ASSERT(std::is_sorted(std::begin(blockedPortList), std::end(blockedPortList)));
    if (!std::binary_search(std::begin(blockedPortList), std::end(blockedPortList), port.value()))
        return true;

    // Allow ports 21 and 22 for FTP URLs, as Mozilla does.
    if ((port.value() == 21 || port.value() == 22) && url.protocolIs("ftp"_s))
        return true;

    // Allow any port number in a file URL, since the port number is ignored.
    if (url.protocolIsFile())
        return true;

    return false;
}

String mimeTypeFromDataURL(StringView dataURL)
{
    ASSERT(protocolIsInternal(dataURL, "data"_s));

    // FIXME: What's the right behavior when the URL has a comma first, but a semicolon later?
    // Currently this code will break at the semicolon in that case; should add a test.
    auto index = dataURL.find(';', 5);
    if (index == notFound)
        index = dataURL.find(',', 5);
    if (index == notFound) {
        // FIXME: There was an old comment here that made it sound like this should be returning text/plain.
        // But we have been returning empty string here for some time, so not changing its behavior at this time.
        return emptyString();
    }
    if (index == 5)
        return "text/plain"_s;
    ASSERT(index >= 5);
    return dataURL.substring(5, index - 5).convertToASCIILowercase();
}

String URL::stringCenterEllipsizedToLength(unsigned length) const
{
    if (m_string.length() <= length)
        return m_string;

    return makeString(StringView(m_string).left(length / 2 - 1), "..."_s, StringView(m_string).right(length / 2 - 2));
}

URL URL::fakeURLWithRelativePart(StringView relativePart)
{
    return URL(makeString("webkit-fake-url://"_s, UUID::createVersion4(), '/', relativePart));
}

URL URL::fileURLWithFileSystemPath(StringView path)
{
    return URL(makeString(
        "file://"_s,
        path.startsWith('/') ? ""_s : "/"_s,
        escapePathWithoutCopying(path)
    ));
}

StringView URL::queryWithLeadingQuestionMark() const
{
    if (m_queryEnd <= m_pathEnd)
        return { };

    return StringView(m_string).substring(m_pathEnd, m_queryEnd - m_pathEnd);
}

StringView URL::fragmentIdentifierWithLeadingNumberSign() const
{
    if (!m_isValid || m_string.length() <= m_queryEnd)
        return { };

    return StringView(m_string).substring(m_queryEnd);
}

bool URL::isAboutBlank() const
{
    return protocolIsAbout() && path() == "blank"_s;
}

bool URL::isAboutSrcDoc() const
{
    return protocolIsAbout() && path() == "srcdoc"_s;
}

TextStream& operator<<(TextStream& ts, const URL& url)
{
    ts << url.string();
    return ts;
}

static bool isIPv4Address(StringView string)
{
    auto count = 0;

    for (const auto octet : string.splitAllowingEmptyEntries('.')) {
        if (count >= 4)
            return false;

        const auto length = octet.length();
        if (!length || length > 3)
            return false;

        auto value = 0;
        for (auto i = 0u; i < length; ++i) {
            const auto digit = octet[i];

            // Prohibit leading zeroes.
            if (digit > '9' || digit < (!i && length > 1 ? '1' : '0'))
                return false;

            value = 10 * value + (digit - '0');
        }

        if (value > 255)
            return false;

        count++;
    }

    return (count == 4);
}

bool URL::isIPv6Address(StringView string)
{
    enum SkipState { None, WillSkip, Skipping, Skipped, Final };
    auto skipState = None;
    auto count = 0;

    for (const auto hextet : string.splitAllowingEmptyEntries(':')) {
        if (count >= 8 || skipState == Final)
            return false;

        const auto length = hextet.length();
        if (!length) {
            // :: may be used anywhere to skip 1 to 8 hextets, but only once.
            if (skipState == Skipped)
                return false;

            if (skipState == None)
                skipState = !count ? WillSkip : Skipping;
            else if (skipState == WillSkip)
                skipState = Skipping;
            else
                skipState = Final;
            continue;
        }

        if (skipState == WillSkip)
            return false;

        if (skipState == Skipping)
            skipState = Skipped;

        if (length > 4) {
            // An IPv4 address may be used in place of the final two hextets.
            if ((skipState == None && count != 6) || (skipState == Skipped && count >= 6) || !isIPv4Address(hextet))
                return false;

            skipState = Final;
            continue;
        }

        for (const auto codeUnit : hextet.codeUnits()) {
            // IPv6 allows leading zeroes.
            if (!isASCIIHexDigit(codeUnit))
                return false;
        }

        count++;
    }

    return (count == 8 && skipState == None) || skipState == Skipped || skipState == Final;
}

#if !PLATFORM(COCOA) && !USE(SOUP)

bool URL::hostIsIPAddress(StringView host)
{
    return host.contains(':') ? isIPv6Address(host) : isIPv4Address(host);
}

#endif

Vector<KeyValuePair<String, String>> queryParameters(const URL& url)
{
    return URLParser::parseURLEncodedForm(url.query());
}

Vector<KeyValuePair<String, String>> differingQueryParameters(const URL& firstURL, const URL& secondURL)
{
    auto firstQueryParameters = URLParser::parseURLEncodedForm(firstURL.query());
    auto secondQueryParameters = URLParser::parseURLEncodedForm(secondURL.query());
    
    if (firstQueryParameters.isEmpty())
        return secondQueryParameters;

    if (secondQueryParameters.isEmpty())
        return firstQueryParameters;
    
    auto compare = [] (const KeyValuePair<String, String>& a, const KeyValuePair<String, String>& b) {
        if (auto result = codePointCompare(a.key, b.key); is_neq(result))
            return result;
        return codePointCompare(a.value, b.value);
        
    };
    auto comparesLessThan = [compare] (const KeyValuePair<String, String>& a, const KeyValuePair<String, String>& b) {
        return compare(a, b) < 0;
    };
    
    std::ranges::sort(firstQueryParameters, comparesLessThan);
    std::ranges::sort(secondQueryParameters, comparesLessThan);
    size_t totalFirstQueryParameters = firstQueryParameters.size();
    size_t totalSecondQueryParameters = secondQueryParameters.size();
    size_t indexInFirstQueryParameters = 0;
    size_t indexInSecondQueryParameters = 0;
    Vector<KeyValuePair<String, String>> differingQueryParameters;
    while (indexInFirstQueryParameters < totalFirstQueryParameters && indexInSecondQueryParameters < totalSecondQueryParameters) {
        auto comparison = compare(firstQueryParameters[indexInFirstQueryParameters], secondQueryParameters[indexInSecondQueryParameters]);
        if (is_lt(comparison)) {
            differingQueryParameters.append(firstQueryParameters[indexInFirstQueryParameters]);
            indexInFirstQueryParameters++;
        } else if (is_gt(comparison)) {
            differingQueryParameters.append(secondQueryParameters[indexInSecondQueryParameters]);
            indexInSecondQueryParameters++;
        } else {
            indexInFirstQueryParameters++;
            indexInSecondQueryParameters++;
        }
    }
    
    while (indexInFirstQueryParameters < totalFirstQueryParameters) {
        differingQueryParameters.append(firstQueryParameters[indexInFirstQueryParameters]);
        indexInFirstQueryParameters++;
    }
    
    while (indexInSecondQueryParameters < totalSecondQueryParameters) {
        differingQueryParameters.append(secondQueryParameters[indexInSecondQueryParameters]);
        indexInSecondQueryParameters++;
    }
    
    return differingQueryParameters;
}

static StringView substringIgnoringQueryAndFragments(const URL& url LIFETIME_BOUND)
{
    if (!url.isValid())
        return StringView(url.string());
    
    return StringView(url.string()).left(url.pathEnd());
}

bool isEqualIgnoringQueryAndFragments(const URL& a, const URL& b)
{
    return substringIgnoringQueryAndFragments(a) == substringIgnoringQueryAndFragments(b);
}

Vector<String> removeQueryParameters(URL& url, const HashSet<String>& keysToRemove)
{
    if (keysToRemove.isEmpty())
        return { };

    return removeQueryParameters(url, [&](auto& key, auto&) {
        return keysToRemove.contains(key);
    });
}

Vector<String> removeQueryParameters(URL& url, NOESCAPE const Function<bool(const String&, const String&)>& shouldRemove)
{
    if (!url.hasQuery())
        return { };

    Vector<String> removedParameters;
    StringBuilder queryWithoutRemovalKeys;
    for (auto bytes : url.query().split('&')) {
        auto nameAndValue = URLParser::parseQueryNameAndValue(bytes);
        if (!nameAndValue)
            continue;

        auto& key = nameAndValue->key;
        if (key.isEmpty())
            continue;

        if (shouldRemove(key, nameAndValue->value)) {
            removedParameters.append(key);
            continue;
        }

        queryWithoutRemovalKeys.append(queryWithoutRemovalKeys.isEmpty() ? ""_s : "&"_s, bytes);
    }

    if (!removedParameters.isEmpty())
        url.setQuery(queryWithoutRemovalKeys);

    return removedParameters;
}

} // namespace WTF
