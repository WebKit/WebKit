/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
typedef const struct __CFURL* CFURLRef;
#endif

#if USE(SOUP)
#include "GUniquePtrSoup.h"
#endif

#if USE(FOUNDATION)
OBJC_CLASS NSURL;
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class URLTextEncoding {
public:
    virtual Vector<uint8_t> encodeForURLParsing(StringView) const = 0;
    virtual ~URLTextEncoding() { };
};

struct URLHash;

enum ParsedURLStringTag { ParsedURLString };

class URL {
public:
    // Generates a URL which contains a null string.
    URL() { invalidate(); }

    // The argument is an absolute URL string. The string is assumed to be output of URL::string() called on a valid
    // URL object, or indiscernible from such.
    // It is usually best to avoid repeatedly parsing a string, unless memory saving outweigh the possible slow-downs.
    WEBCORE_EXPORT URL(ParsedURLStringTag, const String&);
    explicit URL(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return string().isHashTableDeletedValue(); }

    // Resolves the relative URL with the given base URL. If provided, the
    // URLTextEncoding is used to encode non-ASCII characers. The base URL can be
    // null or empty, in which case the relative URL will be interpreted as
    // absolute.
    // FIXME: If the base URL is invalid, this always creates an invalid
    // URL. Instead I think it would be better to treat all invalid base URLs
    // the same way we treate null and empty base URLs.
    WEBCORE_EXPORT URL(const URL& base, const String& relative, const URLTextEncoding* = nullptr);

    WEBCORE_EXPORT static URL fakeURLWithRelativePart(const String&);
    WEBCORE_EXPORT static URL fileURLWithFileSystemPath(const String&);

    String strippedForUseAsReferrer() const;

    // FIXME: The above functions should be harmonized so that passing a
    // base of null or the empty string gives the same result as the
    // standard String constructor.

    // Makes a deep copy. Helpful only if you need to use a URL on another
    // thread. Since the underlying StringImpl objects are immutable, there's
    // no other reason to ever prefer isolatedCopy() over plain old assignment.
    WEBCORE_EXPORT URL isolatedCopy() const;

    bool isNull() const;
    bool isEmpty() const;
    bool isValid() const;

    // Returns true if you can set the host and port for the URL.
    // Non-hierarchical URLs don't have a host and port.
    bool canSetHostOrPort() const { return isHierarchical(); }

    bool canSetPathname() const { return isHierarchical(); }
    bool isHierarchical() const;

    const String& string() const { return m_string; }

    WEBCORE_EXPORT String stringCenterEllipsizedToLength(unsigned length = 1024) const;

    WEBCORE_EXPORT StringView protocol() const;
    WEBCORE_EXPORT StringView host() const;
    WEBCORE_EXPORT std::optional<uint16_t> port() const;
    WEBCORE_EXPORT String hostAndPort() const;
    WEBCORE_EXPORT String protocolHostAndPort() const;
    WEBCORE_EXPORT String user() const;
    WEBCORE_EXPORT String pass() const;
    WEBCORE_EXPORT String path() const;
    WEBCORE_EXPORT String lastPathComponent() const;
    WEBCORE_EXPORT String query() const;
    WEBCORE_EXPORT String fragmentIdentifier() const;
    WEBCORE_EXPORT bool hasFragmentIdentifier() const;

    bool hasUsername() const;
    bool hasPassword() const;
    bool hasQuery() const;
    bool hasFragment() const;
    bool hasPath() const;

    // Unlike user() and pass(), these functions don't decode escape sequences.
    // This is necessary for accurate round-tripping, because encoding doesn't encode '%' characters.
    String encodedUser() const;
    String encodedPass() const;

    WEBCORE_EXPORT String baseAsString() const;

    WEBCORE_EXPORT String fileSystemPath() const;

    // Returns true if the current URL's protocol is the same as the null-
    // terminated ASCII argument. The argument must be lower-case.
    WEBCORE_EXPORT bool protocolIs(const char*) const;
    bool protocolIs(StringView) const;
    bool protocolIsBlob() const { return protocolIs("blob"); }
    bool protocolIsData() const { return protocolIs("data"); }
    bool protocolIsInHTTPFamily() const;
    WEBCORE_EXPORT bool isLocalFile() const;
    WEBCORE_EXPORT bool isBlankURL() const;
    bool cannotBeABaseURL() const { return m_cannotBeABaseURL; }

    WEBCORE_EXPORT bool isMatchingDomain(const String&) const;

    WEBCORE_EXPORT bool setProtocol(const String&);
    void setHost(const String&);

    void removePort();
    void setPort(unsigned short);

    // Input is like "foo.com" or "foo.com:8000".
    void setHostAndPort(const String&);

    void setUser(const String&);
    void setPass(const String&);

    // If you pass an empty path for HTTP or HTTPS URLs, the resulting path
    // will be "/".
    WEBCORE_EXPORT void setPath(const String&);

    // The query may begin with a question mark, or, if not, one will be added
    // for you. Setting the query to the empty string will leave a "?" in the
    // URL (with nothing after it). To clear the query, pass a null string.
    WEBCORE_EXPORT void setQuery(const String&);

    WEBCORE_EXPORT void setFragmentIdentifier(StringView);
    WEBCORE_EXPORT void removeFragmentIdentifier();

    WEBCORE_EXPORT void removeQueryAndFragmentIdentifier();

    WEBCORE_EXPORT static bool hostIsIPAddress(StringView);

    unsigned pathStart() const;
    unsigned pathEnd() const;
    unsigned pathAfterLastSlash() const;

    operator const String&() const { return string(); }

#if USE(CF)
    WEBCORE_EXPORT URL(CFURLRef);
    WEBCORE_EXPORT RetainPtr<CFURLRef> createCFURL() const;
#endif

#if USE(SOUP)
    URL(SoupURI*);
    GUniquePtr<SoupURI> createSoupURI() const;
#endif

#if USE(FOUNDATION)
    WEBCORE_EXPORT URL(NSURL*);
    WEBCORE_EXPORT operator NSURL*() const;
#endif
#ifdef __OBJC__
    operator NSString*() const { return string(); }
#endif

#ifndef NDEBUG
    void print() const;
#endif

    template <class Encoder> void encode(Encoder&) const;
    template <class Decoder> static bool decode(Decoder&, URL&);
    template <class Decoder> static std::optional<URL> decode(Decoder&);

private:
    friend class URLParser;
    WEBCORE_EXPORT void invalidate();
    static bool protocolIs(const String&, const char*);
    void copyToBuffer(Vector<char, 512>& buffer) const;
    unsigned hostStart() const;

    WEBCORE_EXPORT friend bool equalIgnoringFragmentIdentifier(const URL&, const URL&);
    WEBCORE_EXPORT friend bool protocolHostAndPortAreEqual(const URL&, const URL&);
    WEBCORE_EXPORT friend bool hostsAreEqual(const URL&, const URL&);

    String m_string;

    unsigned m_isValid : 1;
    unsigned m_protocolIsInHTTPFamily : 1;
    unsigned m_cannotBeABaseURL : 1;

    // This is out of order to align the bits better. The port is after the host.
    unsigned m_portLength : 3;
    static constexpr unsigned maxPortLength = (1 << 3) - 1;

    static constexpr unsigned maxSchemeLength = (1 << 26) - 1;
    unsigned m_schemeEnd : 26;
    unsigned m_userStart;
    unsigned m_userEnd;
    unsigned m_passwordEnd;
    unsigned m_hostEnd;
    unsigned m_pathAfterLastSlash;
    unsigned m_pathEnd;
    unsigned m_queryEnd;
};

static_assert(sizeof(URL) == sizeof(String) + 8 * sizeof(unsigned), "URL should stay small");

template <class Encoder>
void URL::encode(Encoder& encoder) const
{
    encoder << m_string;
}

template <class Decoder>
bool URL::decode(Decoder& decoder, URL& url)
{
    auto optionalURL = URL::decode(decoder);
    if (!optionalURL)
        return false;
    url = WTFMove(*optionalURL);
    return true;
}

template <class Decoder>
std::optional<URL> URL::decode(Decoder& decoder)
{
    String string;
    if (!decoder.decode(string))
        return std::nullopt;
    return URL(URL(), string);
}

bool operator==(const URL&, const URL&);
bool operator==(const URL&, const String&);
bool operator==(const String&, const URL&);
bool operator!=(const URL&, const URL&);
bool operator!=(const URL&, const String&);
bool operator!=(const String&, const URL&);

WEBCORE_EXPORT bool equalIgnoringFragmentIdentifier(const URL&, const URL&);
WEBCORE_EXPORT bool equalIgnoringQueryAndFragment(const URL&, const URL&);
WEBCORE_EXPORT bool protocolHostAndPortAreEqual(const URL&, const URL&);
WEBCORE_EXPORT bool hostsAreEqual(const URL&, const URL&);

WEBCORE_EXPORT const URL& blankURL();

// Functions to do URL operations on strings.
// These are operations that aren't faster on a parsed URL.
// These are also different from the URL functions in that they don't require the string to be a valid and parsable URL.
// This is especially important because valid javascript URLs are not necessarily considered valid by URL.

WEBCORE_EXPORT bool protocolIs(const String& url, const char* protocol);
WEBCORE_EXPORT bool protocolIsJavaScript(const String& url);
bool protocolIsJavaScript(StringView url);
WEBCORE_EXPORT bool protocolIsInHTTPFamily(const String& url);

std::optional<uint16_t> defaultPortForProtocol(StringView protocol);
WEBCORE_EXPORT bool isDefaultPortForProtocol(uint16_t port, StringView protocol);
WEBCORE_EXPORT bool portAllowed(const URL&); // Blacklist ports that should never be used for Web resources.

WEBCORE_EXPORT void registerDefaultPortForProtocolForTesting(uint16_t port, const String& protocol);
WEBCORE_EXPORT void clearDefaultPortForProtocolMapForTesting();

bool isValidProtocol(const String&);

String mimeTypeFromDataURL(const String& url);

// Unescapes the given string using URL escaping rules, given an optional
// encoding (defaulting to UTF-8 otherwise). DANGER: If the URL has "%00"
// in it, the resulting string will have embedded null characters!
WEBCORE_EXPORT String decodeURLEscapeSequences(const String&);
class TextEncoding;
String decodeURLEscapeSequences(const String&, const TextEncoding&);

// FIXME: This is a wrong concept to expose, different parts of a URL need different escaping per the URL Standard.
WEBCORE_EXPORT String encodeWithURLEscapeSequences(const String&);

#if PLATFORM(IOS)
WEBCORE_EXPORT void enableURLSchemeCanonicalization(bool);
#endif

// Inlines.

inline bool operator==(const URL& a, const URL& b)
{
    return a.string() == b.string();
}

inline bool operator==(const URL& a, const String& b)
{
    return a.string() == b;
}

inline bool operator==(const String& a, const URL& b)
{
    return a == b.string();
}

inline bool operator!=(const URL& a, const URL& b)
{
    return a.string() != b.string();
}

inline bool operator!=(const URL& a, const String& b)
{
    return a.string() != b;
}

inline bool operator!=(const String& a, const URL& b)
{
    return a != b.string();
}

// Inline versions of some non-GoogleURL functions so we can get inlining
// without having to have a lot of ugly ifdefs in the class definition.

inline bool URL::isNull() const
{
    return m_string.isNull();
}

inline bool URL::isEmpty() const
{
    return m_string.isEmpty();
}

inline bool URL::isValid() const
{
    return m_isValid;
}

inline bool URL::hasPath() const
{
    return m_pathEnd != m_hostEnd + m_portLength;
}

inline bool URL::hasUsername() const
{
    return m_userEnd > m_userStart;
}

inline bool URL::hasPassword() const
{
    return m_passwordEnd > (m_userEnd + 1);
}

inline bool URL::hasQuery() const
{
    return m_queryEnd > m_pathEnd;
}

inline bool URL::hasFragment() const
{
    return m_isValid && m_string.length() > m_queryEnd;
}

inline bool URL::protocolIsInHTTPFamily() const
{
    return m_protocolIsInHTTPFamily;
}

inline unsigned URL::pathStart() const
{
    return m_hostEnd + m_portLength;
}

inline unsigned URL::pathEnd() const
{
    return m_pathEnd;
}

inline unsigned URL::pathAfterLastSlash() const
{
    return m_pathAfterLastSlash;
}

WTF::TextStream& operator<<(WTF::TextStream&, const URL&);

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::URL>;
template<> struct HashTraits<WebCore::URL>;
} // namespace WTF
