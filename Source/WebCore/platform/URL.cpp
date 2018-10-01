/*
 * Copyright (C) 2004, 2007-2008, 2011-2013, 2015-2016 Apple Inc. All rights reserved.
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
#include "URL.h"

#include "URLParser.h"
#include <stdio.h>
#include <unicode/uidna.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UUID.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/TextStream.h>

// FIXME: This file makes too much use of the + operator on String.
// We either have to optimize that operator so it doesn't involve
// so many allocations, or change this to use StringBuffer instead.


namespace WebCore {
using namespace WTF;

typedef Vector<char, 512> CharBuffer;
typedef Vector<UChar, 512> UCharBuffer;

static const unsigned invalidPortNumber = 0xFFFF;

// Copies the source to the destination, assuming all the source characters are
// ASCII. The destination buffer must be large enough. Null characters are allowed
// in the source string, and no attempt is made to null-terminate the result.
static void copyASCII(const String& string, char* dest)
{
    if (string.isEmpty())
        return;

    if (string.is8Bit())
        memcpy(dest, string.characters8(), string.length());
    else {
        const UChar* src = string.characters16();
        size_t length = string.length();
        for (size_t i = 0; i < length; i++)
            dest[i] = static_cast<char>(src[i]);
    }
}

void URL::invalidate()
{
    m_isValid = false;
    m_protocolIsInHTTPFamily = false;
    m_cannotBeABaseURL = false;
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

URL::URL(ParsedURLStringTag, const String& url)
{
    URLParser parser(url);
    *this = parser.result();

#if OS(WINDOWS)
    // FIXME(148598): Work around Windows local file handling bug in CFNetwork
    ASSERT(isLocalFile() || url == m_string);
#else
    ASSERT(url == m_string);
#endif
}

URL::URL(const URL& base, const String& relative, const URLTextEncoding* encoding)
{
    URLParser parser(relative, base, encoding);
    *this = parser.result();
}

static bool shouldTrimFromURL(UChar c)
{
    // Browsers ignore leading/trailing whitespace and control
    // characters from URLs.  Note that c is an *unsigned* char here
    // so this comparison should only catch control characters.
    return c <= ' ';
}

URL URL::isolatedCopy() const
{
    URL result = *this;
    result.m_string = result.m_string.isolatedCopy();
    return result;
}

String URL::lastPathComponent() const
{
    if (!hasPath())
        return String();

    unsigned end = m_pathEnd - 1;
    if (m_string[end] == '/')
        --end;

    size_t start = m_string.reverseFind('/', end);
    if (start < static_cast<unsigned>(m_hostEnd + m_portLength))
        return String();
    ++start;

    return m_string.substring(start, end - start + 1);
}

StringView URL::protocol() const
{
    return StringView(m_string).substring(0, m_schemeEnd);
}

StringView URL::host() const
{
    unsigned start = hostStart();
    return StringView(m_string).substring(start, m_hostEnd - start);
}

std::optional<uint16_t> URL::port() const
{
    if (!m_portLength)
        return std::nullopt;

    bool ok = false;
    unsigned number;
    if (m_string.is8Bit())
        number = charactersToUIntStrict(m_string.characters8() + m_hostEnd + 1, m_portLength - 1, &ok);
    else
        number = charactersToUIntStrict(m_string.characters16() + m_hostEnd + 1, m_portLength - 1, &ok);
    if (!ok || number > std::numeric_limits<uint16_t>::max())
        return std::nullopt;
    return number;
}

String URL::hostAndPort() const
{
    if (auto port = this->port())
        return makeString(host(), ':', static_cast<unsigned>(port.value()));
    return host().toString();
}

String URL::protocolHostAndPort() const
{
    String result = m_string.substring(0, m_hostEnd + m_portLength);

    if (m_passwordEnd - m_userStart > 0) {
        const int allowForTrailingAtSign = 1;
        result.remove(m_userStart, m_passwordEnd - m_userStart + allowForTrailingAtSign);
    }

    return result;
}

static String decodeEscapeSequencesFromParsedURL(StringView input)
{
    auto inputLength = input.length();
    if (!inputLength)
        return emptyString();
    Vector<LChar> percentDecoded;
    percentDecoded.reserveInitialCapacity(inputLength);
    for (unsigned i = 0; i < inputLength; ++i) {
        if (input[i] == '%'
            && inputLength > 2
            && i < inputLength - 2
            && isASCIIHexDigit(input[i + 1])
            && isASCIIHexDigit(input[i + 2])) {
            percentDecoded.uncheckedAppend(toASCIIHexValue(input[i + 1], input[i + 2]));
            i += 2;
        } else
            percentDecoded.uncheckedAppend(input[i]);
    }
    return String::fromUTF8(percentDecoded.data(), percentDecoded.size());
}

String URL::user() const
{
    return decodeEscapeSequencesFromParsedURL(StringView(m_string).substring(m_userStart, m_userEnd - m_userStart));
}

String URL::pass() const
{
    if (m_passwordEnd == m_userEnd)
        return String();

    return decodeEscapeSequencesFromParsedURL(StringView(m_string).substring(m_userEnd + 1, m_passwordEnd - m_userEnd - 1));
}

String URL::encodedUser() const
{
    return m_string.substring(m_userStart, m_userEnd - m_userStart);
}

String URL::encodedPass() const
{
    if (m_passwordEnd == m_userEnd)
        return String();

    return m_string.substring(m_userEnd + 1, m_passwordEnd - m_userEnd - 1);
}

String URL::fragmentIdentifier() const
{
    if (!hasFragmentIdentifier())
        return String();

    return m_string.substring(m_queryEnd + 1);
}

bool URL::hasFragmentIdentifier() const
{
    return m_isValid && m_string.length() != m_queryEnd;
}

String URL::baseAsString() const
{
    return m_string.left(m_pathAfterLastSlash);
}

#if !USE(CF)

String URL::fileSystemPath() const
{
    if (!isValid() || !isLocalFile())
        return String();

    return decodeEscapeSequencesFromParsedURL(StringView(path()));
}

#endif

#ifdef NDEBUG

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
static DefaultPortForProtocolMapForTesting*& defaultPortForProtocolMapForTesting()
{
    static DefaultPortForProtocolMapForTesting* defaultPortForProtocolMap;
    return defaultPortForProtocolMap;
}

static DefaultPortForProtocolMapForTesting& ensureDefaultPortForProtocolMapForTesting()
{
    DefaultPortForProtocolMapForTesting*& defaultPortForProtocolMap = defaultPortForProtocolMapForTesting();
    if (!defaultPortForProtocolMap)
        defaultPortForProtocolMap = new DefaultPortForProtocolMapForTesting;
    return *defaultPortForProtocolMap;
}

void registerDefaultPortForProtocolForTesting(uint16_t port, const String& protocol)
{
    auto locker = holdLock(defaultPortForProtocolMapForTestingLock);
    ensureDefaultPortForProtocolMapForTesting().add(protocol, port);
}

void clearDefaultPortForProtocolMapForTesting()
{
    auto locker = holdLock(defaultPortForProtocolMapForTestingLock);
    if (auto* map = defaultPortForProtocolMapForTesting())
        map->clear();
}

std::optional<uint16_t> defaultPortForProtocol(StringView protocol)
{
    if (auto* overrideMap = defaultPortForProtocolMapForTesting()) {
        auto locker = holdLock(defaultPortForProtocolMapForTestingLock);
        ASSERT(overrideMap); // No need to null check again here since overrideMap cannot become null after being non-null.
        auto iterator = overrideMap->find(protocol.toStringWithoutCopying());
        if (iterator != overrideMap->end())
            return iterator->value;
    }
    return URLParser::defaultPortForProtocol(protocol);
}

bool isDefaultPortForProtocol(uint16_t port, StringView protocol)
{
    return defaultPortForProtocol(protocol) == port;
}

bool URL::protocolIs(const char* protocol) const
{
    assertProtocolIsGood(StringView(reinterpret_cast<const LChar*>(protocol), strlen(protocol)));

    // JavaScript URLs are "valid" and should be executed even if URL decides they are invalid.
    // The free function protocolIsJavaScript() should be used instead. 
    ASSERT(!equalLettersIgnoringASCIICase(StringView(protocol), "javascript"));

    if (!m_isValid)
        return false;

    // Do the comparison without making a new string object.
    for (unsigned i = 0; i < m_schemeEnd; ++i) {
        if (!protocol[i] || !isASCIIAlphaCaselessEqual(m_string[i], protocol[i]))
            return false;
    }
    return !protocol[m_schemeEnd]; // We should have consumed all characters in the argument.
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

String URL::query() const
{
    if (m_queryEnd == m_pathEnd)
        return String();

    return m_string.substring(m_pathEnd + 1, m_queryEnd - (m_pathEnd + 1)); 
}

String URL::path() const
{
    unsigned portEnd = m_hostEnd + m_portLength;
    return m_string.substring(portEnd, m_pathEnd - portEnd);
}

bool URL::setProtocol(const String& s)
{
    // Firefox and IE remove everything after the first ':'.
    size_t separatorPosition = s.find(':');
    String newProtocol = s.substring(0, separatorPosition);
    auto canonicalized = URLParser::maybeCanonicalizeScheme(newProtocol);
    if (!canonicalized)
        return false;

    if (!m_isValid) {
        URLParser parser(makeString(*canonicalized, ":", m_string));
        *this = parser.result();
        return true;
    }

    URLParser parser(makeString(*canonicalized, m_string.substring(m_schemeEnd)));
    *this = parser.result();
    return true;
}

static bool isAllASCII(StringView string)
{
    if (string.is8Bit())
        return charactersAreAllASCII(string.characters8(), string.length());
    return charactersAreAllASCII(string.characters16(), string.length());
}
    
// Appends the punycoded hostname identified by the given string and length to
// the output buffer. The result will not be null terminated.
// Return value of false means error in encoding.
static bool appendEncodedHostname(UCharBuffer& buffer, StringView string)
{
    // Needs to be big enough to hold an IDN-encoded name.
    // For host names bigger than this, we won't do IDN encoding, which is almost certainly OK.
    const unsigned hostnameBufferLength = 2048;
    
    if (string.length() > hostnameBufferLength || isAllASCII(string)) {
        append(buffer, string);
        return true;
    }
    
    UChar hostnameBuffer[hostnameBufferLength];
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo processingDetails = UIDNA_INFO_INITIALIZER;
    int32_t numCharactersConverted = uidna_nameToASCII(&URLParser::internationalDomainNameTranscoder(),
        string.upconvertedCharacters(), string.length(), hostnameBuffer, hostnameBufferLength, &processingDetails, &error);
    
    if (U_SUCCESS(error) && !processingDetails.errors) {
        buffer.append(hostnameBuffer, numCharactersConverted);
        return true;
    }
    return false;
}

unsigned URL::hostStart() const
{
    return (m_passwordEnd == m_userStart) ? m_passwordEnd : m_passwordEnd + 1;
}

void URL::setHost(const String& s)
{
    if (!m_isValid)
        return;

    auto colonIndex = s.find(':');
    if (colonIndex != notFound)
        return;

    UCharBuffer encodedHostName;
    if (!appendEncodedHostname(encodedHostName, s))
        return;
    
    bool slashSlashNeeded = m_userStart == static_cast<unsigned>(m_schemeEnd + 1);
    
    StringBuilder builder;
    builder.append(m_string.left(hostStart()));
    if (slashSlashNeeded)
        builder.appendLiteral("//");
    builder.append(StringView(encodedHostName.data(), encodedHostName.size()));
    builder.append(m_string.substring(m_hostEnd));

    URLParser parser(builder.toString());
    *this = parser.result();
}

void URL::removePort()
{
    if (!m_portLength)
        return;
    URLParser parser(makeString(StringView(m_string).left(m_hostEnd), StringView(m_string).substring(m_hostEnd + m_portLength)));
    *this = parser.result();
}

void URL::setPort(unsigned short i)
{
    if (!m_isValid)
        return;

    bool colonNeeded = !m_portLength;
    unsigned portStart = (colonNeeded ? m_hostEnd : m_hostEnd + 1);

    URLParser parser(makeString(StringView(m_string).left(portStart), (colonNeeded ? ":" : ""), String::number(i), StringView(m_string).substring(m_hostEnd + m_portLength)));
    *this = parser.result();
}

void URL::setHostAndPort(const String& hostAndPort)
{
    if (!m_isValid)
        return;

    StringView hostName(hostAndPort);
    StringView port;
    
    auto colonIndex = hostName.find(':');
    if (colonIndex != notFound) {
        port = hostName.substring(colonIndex + 1);
        bool ok;
        int portInt = port.toIntStrict(ok);
        if (!ok || portInt < 0)
            return;
        hostName = hostName.substring(0, colonIndex);
    }

    if (hostName.isEmpty())
        return;

    UCharBuffer encodedHostName;
    if (!appendEncodedHostname(encodedHostName, hostName))
        return;

    bool slashSlashNeeded = m_userStart == static_cast<unsigned>(m_schemeEnd + 1);

    StringBuilder builder;
    builder.append(m_string.left(hostStart()));
    if (slashSlashNeeded)
        builder.appendLiteral("//");
    builder.append(StringView(encodedHostName.data(), encodedHostName.size()));
    if (!port.isEmpty()) {
        builder.appendLiteral(":");
        builder.append(port);
    }
    builder.append(StringView(m_string).substring(m_hostEnd + m_portLength));

    URLParser parser(builder.toString());
    *this = parser.result();
}

static String percentEncodeCharacters(const String& input, bool(*shouldEncode)(UChar))
{
    auto encode = [shouldEncode] (const String& input) {
        CString utf8 = input.utf8();
        auto* data = utf8.data();
        StringBuilder builder;
        auto length = utf8.length();
        for (unsigned j = 0; j < length; j++) {
            auto c = data[j];
            if (shouldEncode(c)) {
                builder.append('%');
                builder.append(upperNibbleToASCIIHexDigit(c));
                builder.append(lowerNibbleToASCIIHexDigit(c));
            } else
                builder.append(c);
        }
        return builder.toString();
    };

    for (size_t i = 0; i < input.length(); ++i) {
        if (UNLIKELY(shouldEncode(input[i])))
            return encode(input);
    }
    return input;
}

void URL::setUser(const String& user)
{
    if (!m_isValid)
        return;

    // FIXME: Non-ASCII characters must be encoded and escaped to match parse() expectations,
    // and to avoid changing more than just the user login.

    unsigned end = m_userEnd;
    if (!user.isEmpty()) {
        String u = percentEncodeCharacters(user, URLParser::isInUserInfoEncodeSet);
        if (m_userStart == static_cast<unsigned>(m_schemeEnd + 1))
            u = "//" + u;
        // Add '@' if we didn't have one before.
        if (end == m_hostEnd || (end == m_passwordEnd && m_string[end] != '@'))
            u.append('@');
        URLParser parser(makeString(StringView(m_string).left(m_userStart), u, StringView(m_string).substring(end)));
        *this = parser.result();
    } else {
        // Remove '@' if we now have neither user nor password.
        if (m_userEnd == m_passwordEnd && end != m_hostEnd && m_string[end] == '@')
            end += 1;
        // We don't want to parse in the extremely common case where we are not going to make a change.
        if (m_userStart != end) {
            URLParser parser(makeString(StringView(m_string).left(m_userStart), StringView(m_string).substring(end)));
            *this = parser.result();
        }
    }
}

void URL::setPass(const String& password)
{
    if (!m_isValid)
        return;

    unsigned end = m_passwordEnd;
    if (!password.isEmpty()) {
        String p = ":" + percentEncodeCharacters(password, URLParser::isInUserInfoEncodeSet) + "@";
        if (m_userEnd == static_cast<unsigned>(m_schemeEnd + 1))
            p = "//" + p;
        // Eat the existing '@' since we are going to add our own.
        if (end != m_hostEnd && m_string[end] == '@')
            end += 1;
        URLParser parser(makeString(StringView(m_string).left(m_userEnd), p, StringView(m_string).substring(end)));
        *this = parser.result();
    } else {
        // Remove '@' if we now have neither user nor password.
        if (m_userStart == m_userEnd && end != m_hostEnd && m_string[end] == '@')
            end += 1;
        // We don't want to parse in the extremely common case where we are not going to make a change.
        if (m_userEnd != end) {
            URLParser parser(makeString(StringView(m_string).left(m_userEnd), StringView(m_string).substring(end)));
            *this = parser.result();
        }
    }
}

void URL::setFragmentIdentifier(StringView identifier)
{
    if (!m_isValid)
        return;

    // FIXME: Optimize the case where the identifier already happens to be equal to what was passed?
    // FIXME: Is it correct to do this without encoding and escaping non-ASCII characters?
    *this = URLParser { makeString(StringView { m_string }.substring(0, m_queryEnd), '#', identifier) }.result();
}

void URL::removeFragmentIdentifier()
{
    if (!m_isValid) {
        ASSERT(!m_queryEnd);
        return;
    }
    if (m_isValid && m_string.length() > m_queryEnd)
        m_string = m_string.left(m_queryEnd);
}

void URL::removeQueryAndFragmentIdentifier()
{
    if (!m_isValid)
        return;

    m_string = m_string.left(m_pathEnd);
    m_queryEnd = m_pathEnd;
}

void URL::setQuery(const String& query)
{
    if (!m_isValid)
        return;

    // FIXME: '#' and non-ASCII characters must be encoded and escaped.
    // Usually, the query is encoded using document encoding, not UTF-8, but we don't have
    // access to the document in this function.
    // https://webkit.org/b/161176
    if ((query.isEmpty() || query[0] != '?') && !query.isNull()) {
        URLParser parser(makeString(StringView(m_string).left(m_pathEnd), "?", query, StringView(m_string).substring(m_queryEnd)));
        *this = parser.result();
    } else {
        URLParser parser(makeString(StringView(m_string).left(m_pathEnd), query, StringView(m_string).substring(m_queryEnd)));
        *this = parser.result();
    }

}

void URL::setPath(const String& s)
{
    if (!m_isValid)
        return;

    String path = s;
    if (path.isEmpty() || path[0] != '/')
        path = "/" + path;

    auto questionMarkOrNumberSign = [] (UChar character) {
        return character == '?' || character == '#';
    };
    URLParser parser(makeString(StringView(m_string).left(m_hostEnd + m_portLength), percentEncodeCharacters(path, questionMarkOrNumberSign), StringView(m_string).substring(m_pathEnd)));
    *this = parser.result();
}

#if PLATFORM(IOS)

static bool shouldCanonicalizeScheme = true;

void enableURLSchemeCanonicalization(bool enableSchemeCanonicalization)
{
    shouldCanonicalizeScheme = enableSchemeCanonicalization;
}

#endif

template<size_t length>
static inline bool equal(const char* a, const char (&b)[length])
{
#if PLATFORM(IOS)
    if (!shouldCanonicalizeScheme) {
        for (size_t i = 0; i < length; ++i) {
            if (toASCIILower(a[i]) != b[i])
                return false;
        }
        return true;
    }
#endif
    for (size_t i = 0; i < length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

template<size_t lengthB>
static inline bool equal(const char* stringA, size_t lengthA, const char (&stringB)[lengthB])
{
    return lengthA == lengthB && equal(stringA, stringB);
}

bool equalIgnoringFragmentIdentifier(const URL& a, const URL& b)
{
    if (a.m_queryEnd != b.m_queryEnd)
        return false;
    unsigned queryLength = a.m_queryEnd;
    for (unsigned i = 0; i < queryLength; ++i)
        if (a.string()[i] != b.string()[i])
            return false;
    return true;
}

bool equalIgnoringQueryAndFragment(const URL& a, const URL& b)
{
    if (a.pathEnd() != b.pathEnd())
        return false;
    unsigned pathEnd = a.pathEnd();
    for (unsigned i = 0; i < pathEnd; ++i) {
        if (a.string()[i] != b.string()[i])
            return false;
    }
    return true;
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
        if (a.string()[i] != b.string()[i])
            return false;
    }

    // And the host
    for (unsigned i = 0; i < hostLengthA; ++i) {
        if (a.string()[hostStartA + i] != b.string()[hostStartB + i])
            return false;
    }

    if (a.port() != b.port())
        return false;

    return true;
}

bool hostsAreEqual(const URL& a, const URL& b)
{
    unsigned hostStartA = a.hostStart();
    unsigned hostLengthA = a.m_hostEnd - hostStartA;
    unsigned hostStartB = b.hostStart();
    unsigned hostLengthB = b.m_hostEnd - hostStartB;
    if (hostLengthA != hostLengthB)
        return false;

    for (unsigned i = 0; i < hostLengthA; ++i) {
        if (a.string()[hostStartA + i] != b.string()[hostStartB + i])
            return false;
    }

    return true;
}

bool URL::isMatchingDomain(const String& domain) const
{
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

String encodeWithURLEscapeSequences(const String& input)
{
    return percentEncodeCharacters(input, URLParser::isInUserInfoEncodeSet);
}

bool URL::isHierarchical() const
{
    if (!m_isValid)
        return false;
    ASSERT(m_string[m_schemeEnd] == ':');
    return m_string[m_schemeEnd + 1] == '/';
}

void URL::copyToBuffer(Vector<char, 512>& buffer) const
{
    // FIXME: This throws away the high bytes of all the characters in the string!
    // That's fine for a valid URL, which is all ASCII, but not for invalid URLs.
    buffer.resize(m_string.length());
    copyASCII(m_string, buffer.data());
}

template<typename StringClass>
bool protocolIsInternal(const StringClass& url, const char* protocol)
{
    // Do the comparison without making a new string object.
    assertProtocolIsGood(StringView(reinterpret_cast<const LChar*>(protocol), strlen(protocol)));
    bool isLeading = true;
    for (unsigned i = 0, j = 0; url[i]; ++i) {
        // Skip leading whitespace and control characters.
        if (isLeading && shouldTrimFromURL(url[i]))
            continue;
        isLeading = false;

        // Skip any tabs and newlines.
        if (url[i] == '\t' || url[i] == '\r' || url[i] == '\n')
            continue;

        if (!protocol[j])
            return url[i] == ':';
        if (!isASCIIAlphaCaselessEqual(url[i], protocol[j]))
            return false;

        ++j;
    }
    
    return false;
}

bool protocolIs(const String& url, const char* protocol)
{
    return protocolIsInternal(url, protocol);
}

inline bool URL::protocolIs(const String& string, const char* protocol)
{
    return WebCore::protocolIsInternal(string, protocol);
}

#ifndef NDEBUG

void URL::print() const
{
    printf("%s\n", m_string.utf8().data());
}

#endif

String URL::strippedForUseAsReferrer() const
{
    URL referrer(*this);
    referrer.setUser(String());
    referrer.setPass(String());
    referrer.removeFragmentIdentifier();
    return referrer.string();
}

bool URL::isLocalFile() const
{
    // Including feed here might be a bad idea since drag and drop uses this check
    // and including feed would allow feeds to potentially let someone's blog
    // read the contents of the clipboard on a drag, even without a drop.
    // Likewise with using the FrameLoader::shouldTreatURLAsLocal() function.
    return protocolIs("file");
}

bool protocolIsJavaScript(const String& url)
{
    return protocolIsInternal(url, "javascript");
}

bool protocolIsJavaScript(StringView url)
{
    return protocolIsInternal(url, "javascript");
}

bool protocolIsInHTTPFamily(const String& url)
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

const URL& blankURL()
{
    static NeverDestroyed<URL> staticBlankURL(ParsedURLString, "about:blank");
    return staticBlankURL;
}

bool URL::isBlankURL() const
{
    return protocolIs("about");
}

bool portAllowed(const URL& url)
{
    std::optional<uint16_t> port = url.port();

    // Since most URLs don't have a port, return early for the "no port" case.
    if (!port)
        return true;

    // This blocked port list matches the port blocking that Mozilla implements.
    // See http://www.mozilla.org/projects/netlib/PortBanning.html for more information.
    static const uint16_t blockedPortList[] = {
        1,    // tcpmux
        7,    // echo
        9,    // discard
        11,   // systat
        13,   // daytime
        15,   // netstat
        17,   // qotd
        19,   // chargen
        20,   // FTP-data
        21,   // FTP-control
        22,   // SSH
        23,   // telnet
        25,   // SMTP
        37,   // time
        42,   // name
        43,   // nicname
        53,   // domain
        77,   // priv-rjs
        79,   // finger
        87,   // ttylink
        95,   // supdup
        101,  // hostriame
        102,  // iso-tsap
        103,  // gppitnp
        104,  // acr-nema
        109,  // POP2
        110,  // POP3
        111,  // sunrpc
        113,  // auth
        115,  // SFTP
        117,  // uucp-path
        119,  // nntp
        123,  // NTP
        135,  // loc-srv / epmap
        139,  // netbios
        143,  // IMAP2
        179,  // BGP
        389,  // LDAP
        465,  // SMTP+SSL
        512,  // print / exec
        513,  // login
        514,  // shell
        515,  // printer
        526,  // tempo
        530,  // courier
        531,  // Chat
        532,  // netnews
        540,  // UUCP
        548,  // afpovertcp [Apple addition]
        556,  // remotefs
        563,  // NNTP+SSL
        587,  // ESMTP
        601,  // syslog-conn
        636,  // LDAP+SSL
        993,  // IMAP+SSL
        995,  // POP3+SSL
        2049, // NFS
        3659, // apple-sasl / PasswordServer [Apple addition]
        4045, // lockd
        4190, // ManageSieve [Apple addition]
        6000, // X11
        6665, // Alternate IRC [Apple addition]
        6666, // Alternate IRC [Apple addition]
        6667, // Standard IRC [Apple addition]
        6668, // Alternate IRC [Apple addition]
        6669, // Alternate IRC [Apple addition]
        6679, // Alternate IRC SSL [Apple addition]
        6697, // IRC+SSL [Apple addition]
        invalidPortNumber, // Used to block all invalid port numbers
    };

    // If the port is not in the blocked port list, allow it.
    ASSERT(std::is_sorted(std::begin(blockedPortList), std::end(blockedPortList)));
    if (!std::binary_search(std::begin(blockedPortList), std::end(blockedPortList), port.value()))
        return true;

    // Allow ports 21 and 22 for FTP URLs, as Mozilla does.
    if ((port.value() == 21 || port.value() == 22) && url.protocolIs("ftp"))
        return true;

    // Allow any port number in a file URL, since the port number is ignored.
    if (url.protocolIs("file"))
        return true;

    return false;
}

String mimeTypeFromDataURL(const String& url)
{
    ASSERT(protocolIsInternal(url, "data"));

    // FIXME: What's the right behavior when the URL has a comma first, but a semicolon later?
    // Currently this code will break at the semicolon in that case. Not sure that's correct.
    auto index = url.find(';', 5);
    if (index == notFound)
        index = url.find(',', 5);
    if (index == notFound) {
        // FIXME: There was an old comment here that made it sound like this should be returning text/plain.
        // But we have been returning empty string here for some time, so not changing its behavior at this time.
        return emptyString();
    }
    if (index == 5)
        return "text/plain"_s;
    ASSERT(index >= 5);
    return url.substring(5, index - 5).convertToASCIILowercase();
}

String URL::stringCenterEllipsizedToLength(unsigned length) const
{
    if (string().length() <= length)
        return string();

    return string().left(length / 2 - 1) + "..." + string().right(length / 2 - 2);
}

URL URL::fakeURLWithRelativePart(const String& relativePart)
{
    return URL(URL(), "webkit-fake-url://" + createCanonicalUUIDString() + '/' + relativePart);
}

URL URL::fileURLWithFileSystemPath(const String& filePath)
{
    return URL(URL(), "file:///" + filePath);
}

TextStream& operator<<(TextStream& ts, const URL& url)
{
    ts << url.string();
    return ts;
}

#if !PLATFORM(COCOA) && !USE(SOUP)
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

static bool isIPv6Address(StringView string)
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

bool URL::hostIsIPAddress(StringView host)
{
    if (host.find(':') == notFound)
        return isIPv4Address(host);

    return isIPv6Address(host);
}
#endif

} // namespace WebCore
