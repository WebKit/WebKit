/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include <stdio.h>
#include <unicode/uidna.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UUID.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/TextStream.h>

namespace WTF {

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

URL::URL(const URL& base, const String& relative, const URLTextEncoding* encoding)
{
    *this = URLParser(relative, base, encoding).result();
}

static bool shouldTrimFromURL(UChar character)
{
    // Ignore leading/trailing whitespace and control characters.
    return character <= ' ';
}

URL URL::isolatedCopy() const
{
    URL result = *this;
    result.m_string = result.m_string.isolatedCopy();
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

StringView URL::protocol() const
{
    if (!m_isValid)
        return { };

    return StringView(m_string).substring(0, m_schemeEnd);
}

StringView URL::host() const
{
    if (!m_isValid)
        return { };

    unsigned start = hostStart();
    return StringView(m_string).substring(start, m_hostEnd - start);
}

Optional<uint16_t> URL::port() const
{
    return m_portLength ? parseUInt16(StringView(m_string).substring(m_hostEnd + 1, m_portLength - 1)) : WTF::nullopt;
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
        return m_string.substring(0, pathStart());

    return makeString(
        StringView(m_string).substring(0, m_userStart),
        StringView(m_string).substring(hostStart(), pathStart() - hostStart())
    );
}

static Optional<LChar> decodeEscapeSequence(StringView input, unsigned index, unsigned length)
{
    if (index + 3 > length || input[index] != '%')
        return WTF::nullopt;
    auto digit1 = input[index + 1];
    auto digit2 = input[index + 2];
    if (!isASCIIHexDigit(digit1) || !isASCIIHexDigit(digit2))
        return WTF::nullopt;
    return toASCIIHexValue(digit1, digit2);
}

static String decodeEscapeSequencesFromParsedURL(StringView input)
{
    ASSERT(input.isAllASCII());

    auto length = input.length();
    if (length < 3 || !input.contains('%'))
        return input.toString();

    // FIXME: This 100 is arbitrary. Should make a histogram of how this function is actually used to choose a better value.
    Vector<LChar, 100> percentDecoded;
    percentDecoded.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ) {
        if (auto decodedCharacter = decodeEscapeSequence(input, i, length)) {
            percentDecoded.uncheckedAppend(*decodedCharacter);
            i += 3;
        } else {
            percentDecoded.uncheckedAppend(input[i]);
            ++i;
        }
    }

    // FIXME: Is UTF-8 always the correct encoding?
    // FIXME: This returns a null string when we encounter an invalid UTF-8 sequence. Is that OK?
    return String::fromUTF8(percentDecoded.data(), percentDecoded.size());
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

URL URL::truncatedForUseAsBase() const
{
    return URL(URL(), m_string.left(m_pathAfterLastSlash));
}

#if !USE(CF)

String URL::fileSystemPath() const
{
    if (!isLocalFile())
        return { };

    return decodeEscapeSequencesFromParsedURL(path());
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

Optional<uint16_t> defaultPortForProtocol(StringView protocol)
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

bool URL::protocolIsJavaScript() const
{
    return WTF::protocolIsJavaScript(string());
}

bool URL::protocolIs(const char* protocol) const
{
    assertProtocolIsGood(protocol);

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
    // Firefox and IE remove everything after the first ':'.
    auto newProtocolPrefix = newProtocol.substring(0, newProtocol.find(':'));
    auto newProtocolCanonicalized = URLParser::maybeCanonicalizeScheme(newProtocolPrefix.toStringWithoutCopying());
    if (!newProtocolCanonicalized)
        return false;

    if (!m_isValid) {
        parse(makeString(*newProtocolCanonicalized, ':', m_string));
        return true;
    }

    if ((m_passwordEnd != m_userStart || port()) && *newProtocolCanonicalized == "file")
        return true;

    if (isLocalFile() && host().isEmpty())
        return true;

    parse(makeString(*newProtocolCanonicalized, StringView(m_string).substring(m_schemeEnd)));
    return true;
}

// Appends the punycoded hostname identified by the given string and length to
// the output buffer. The result will not be null terminated.
// Return value of false means error in encoding.
static bool appendEncodedHostname(Vector<UChar, 512>& buffer, StringView string)
{
    // Needs to be big enough to hold an IDN-encoded name.
    // For host names bigger than this, we won't do IDN encoding, which is almost certainly OK.
    const unsigned hostnameBufferLength = 2048;

    if (string.length() > hostnameBufferLength || string.isAllASCII()) {
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

unsigned URL::credentialsEnd() const
{
    // Include '@' too if we have it.
    unsigned end = m_passwordEnd;
    if (end != m_hostEnd && m_string[end] == '@')
        end += 1;
    return end;
}

void URL::setHost(StringView newHost)
{
    if (!m_isValid)
        return;

    if (newHost.contains(':'))
        return;

    Vector<UChar, 512> encodedHostName;
    if (!appendEncodedHostname(encodedHostName, newHost))
        return;

    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
    parse(makeString(
        StringView(m_string).left(hostStart()),
        slashSlashNeeded ? "//" : "",
        StringView(encodedHostName.data(), encodedHostName.size()),
        StringView(m_string).substring(m_hostEnd)
    ));
}

void URL::setPort(Optional<uint16_t> port)
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

void URL::setHostAndPort(StringView hostAndPort)
{
    if (!m_isValid)
        return;

    auto hostName = hostAndPort;
    StringView portString;
    auto colonIndex = hostName.find(':');
    if (colonIndex != notFound) {
        portString = hostName.substring(colonIndex + 1);
        hostName = hostName.substring(0, colonIndex);
        if (!parseUInt16(portString))
            portString = { };
    }
    if (hostName.isEmpty()) {
        remove(hostStart(), pathStart() - hostStart());
        return;
    }

    Vector<UChar, 512> encodedHostName;
    if (!appendEncodedHostname(encodedHostName, hostName))
        return;

    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
    parse(makeString(
        StringView(m_string).left(hostStart()),
        slashSlashNeeded ? "//" : "",
        StringView(encodedHostName.data(), encodedHostName.size()),
        portString.isEmpty() ? "" : ":",
        portString,
        StringView(m_string).substring(pathStart())
    ));
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

void URL::parse(const String& string)
{
    *this = URLParser(string).result();
}

void URL::remove(unsigned start, unsigned length)
{
    if (!length)
        return;
    ASSERT(start < m_string.length());
    ASSERT(length <= m_string.length() - start);

    auto stringAfterRemoval = WTFMove(m_string);
    stringAfterRemoval.remove(start, length);
    parse(stringAfterRemoval);
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
            slashSlashNeeded ? "//" : "",
            percentEncodeCharacters(newUser.toStringWithoutCopying(), URLParser::isInUserInfoEncodeSet),
            needSeparator ? "@" : "",
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
            needLeadingSlashes ? "//:" : ":",
            percentEncodeCharacters(newPassword.toStringWithoutCopying(), URLParser::isInUserInfoEncodeSet),
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

    parse(makeString(StringView(m_string).left(m_queryEnd), '#', identifier));
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

    parse(makeString(
        StringView(m_string).left(m_pathEnd),
        (!newQuery.startsWith('?') && !newQuery.isNull()) ? "?" : "",
        newQuery,
        StringView(m_string).substring(m_queryEnd)
    ));
}

static String escapePathWithoutCopying(StringView path)
{
    auto questionMarkOrNumberSign = [] (UChar character) {
        return character == '?' || character == '#';
    };
    return percentEncodeCharacters(path.toStringWithoutCopying(), questionMarkOrNumberSign);
}

void URL::setPath(StringView path)
{
    if (!m_isValid)
        return;

    parse(makeString(
        StringView(m_string).left(pathStart()),
        path.startsWith('/') ? "" : "/",
        escapePathWithoutCopying(path),
        StringView(m_string).substring(m_pathEnd)
    ));
}

StringView URL::stringWithoutQueryOrFragmentIdentifier() const
{
    if (!m_isValid)
        return m_string;

    return StringView(m_string).left(pathEnd());
}

StringView URL::stringWithoutFragmentIdentifier() const
{
    if (!m_isValid)
        return m_string;

    return StringView(m_string).left(m_queryEnd);
}

bool equalIgnoringFragmentIdentifier(const URL& a, const URL& b)
{
    return a.stringWithoutFragmentIdentifier() == b.stringWithoutFragmentIdentifier();
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

bool URL::isHierarchical() const
{
    if (!m_isValid)
        return false;
    ASSERT(m_string[m_schemeEnd] == ':');
    return m_string[m_schemeEnd + 1] == '/';
}

static bool protocolIsInternal(StringView string, const char* protocol)
{
    assertProtocolIsGood(protocol);
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

        char expectedCharacter = *protocol++;
        if (!expectedCharacter)
            return codeUnit == ':';
        if (!isASCIIAlphaCaselessEqual(codeUnit, expectedCharacter))
            return false;
    }
    return false;
}

bool protocolIs(StringView string, const char* protocol)
{
    return protocolIsInternal(string, protocol);
}

#ifndef NDEBUG

void URL::print() const
{
    printf("%s\n", m_string.utf8().data());
}

#endif

String URL::strippedForUseAsReferrer() const
{
    if (!m_isValid)
        return m_string;

    unsigned end = credentialsEnd();

    if (m_userStart == end && m_queryEnd == m_string.length())
        return m_string;

    return makeString(
        StringView(m_string).substring(0, m_userStart),
        StringView(m_string).substring(end, m_queryEnd - end)
    );
}

bool URL::isLocalFile() const
{
    // Including feed here might be a bad idea since drag and drop uses this check
    // and including feed would allow feeds to potentially let someone's blog
    // read the contents of the clipboard on a drag, even without a drop.
    // Likewise with using the FrameLoader::shouldTreatURLAsLocal() function.
    return protocolIs("file");
}

bool protocolIsJavaScript(StringView string)
{
    return protocolIsInternal(string, "javascript");
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

const URL& aboutBlankURL()
{
    static NeverDestroyed<URL> staticBlankURL(URL(), "about:blank"_s);
    return staticBlankURL;
}

const URL& aboutSrcDocURL()
{
    static NeverDestroyed<URL> staticAboutSrcDocURL(URL(), "about:srcdoc"_s);
    return staticAboutSrcDocURL;
}

bool URL::protocolIsAbout() const
{
    return protocolIs("about");
}

bool portAllowed(const URL& url)
{
    Optional<uint16_t> port = url.port();

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
        427,  // SLP (Also used by Apple Filing Protocol)
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

String mimeTypeFromDataURL(StringView dataURL)
{
    ASSERT(protocolIsInternal(dataURL, "data"));

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

    return makeString(StringView(m_string).left(length / 2 - 1), "...", StringView(m_string).right(length / 2 - 2));
}

URL URL::fakeURLWithRelativePart(StringView relativePart)
{
    return URL(URL(), makeString("webkit-fake-url://", createCanonicalUUIDString(), '/', relativePart));
}

URL URL::fileURLWithFileSystemPath(StringView path)
{
    return URL(URL(), makeString(
        "file://",
        path.startsWith('/') ? "" : "/",
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
    return protocolIsAbout() && path() == "blank";
}

bool URL::isAboutSrcDoc() const
{
    return protocolIsAbout() && path() == "srcdoc";
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
    return host.contains(':') ? isIPv6Address(host) : isIPv4Address(host);
}

#endif

} // namespace WTF
