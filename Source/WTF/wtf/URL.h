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

#if USE(FOUNDATION)
OBJC_CLASS NSURL;
#endif

namespace WTF {
class TextStream;

class URLTextEncoding {
public:
    virtual Vector<uint8_t> encodeForURLParsing(StringView) const = 0;
    virtual ~URLTextEncoding() { };
};

struct URLHash;

class WTF_EXPORT_PRIVATE URL {
public:
    // Generates a URL which contains a null string.
    URL() { invalidate(); }

    explicit URL(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return string().isHashTableDeletedValue(); }

    // Resolves the relative URL with the given base URL. If provided, the
    // URLTextEncoding is used to encode non-ASCII characers. The base URL can be
    // null or empty, in which case the relative URL will be interpreted as
    // absolute.
    // FIXME: If the base URL is invalid, this always creates an invalid
    // URL. Instead I think it would be better to treat all invalid base URLs
    // the same way we treate null and empty base URLs.
    URL(const URL& base, const String& relative, const URLTextEncoding* = nullptr);

    static URL fakeURLWithRelativePart(const String&);
    static URL fileURLWithFileSystemPath(const String&);

    String strippedForUseAsReferrer() const;

    // FIXME: The above functions should be harmonized so that passing a
    // base of null or the empty string gives the same result as the
    // standard String constructor.

    // Makes a deep copy. Helpful only if you need to use a URL on another
    // thread. Since the underlying StringImpl objects are immutable, there's
    // no other reason to ever prefer isolatedCopy() over plain old assignment.
    URL isolatedCopy() const;

    bool isNull() const;
    bool isEmpty() const;
    bool isValid() const;

    // Returns true if you can set the host and port for the URL.
    // Non-hierarchical URLs don't have a host and port.
    bool canSetHostOrPort() const { return isHierarchical(); }

    bool canSetPathname() const { return isHierarchical(); }
    bool isHierarchical() const;

    const String& string() const { return m_string; }

    String stringCenterEllipsizedToLength(unsigned length = 1024) const;

    StringView protocol() const;
    StringView host() const;
    std::optional<uint16_t> port() const;
    String hostAndPort() const;
    String protocolHostAndPort() const;
    String user() const;
    String pass() const;
    String path() const;
    String lastPathComponent() const;
    String query() const;
    String fragmentIdentifier() const;
    bool hasFragmentIdentifier() const;

    bool hasUsername() const;
    bool hasPassword() const;
    bool hasQuery() const;
    bool hasFragment() const;
    bool hasPath() const;

    // Unlike user() and pass(), these functions don't decode escape sequences.
    // This is necessary for accurate round-tripping, because encoding doesn't encode '%' characters.
    String encodedUser() const;
    String encodedPass() const;

    String baseAsString() const;

    String fileSystemPath() const;

    // Returns true if the current URL's protocol is the same as the null-
    // terminated ASCII argument. The argument must be lower-case.
    bool protocolIs(const char*) const;
    bool protocolIs(StringView) const;
    bool protocolIsBlob() const { return protocolIs("blob"); }
    bool protocolIsData() const { return protocolIs("data"); }
    bool protocolIsAbout() const;
    bool protocolIsInHTTPFamily() const;
    bool isLocalFile() const;
    bool isBlankURL() const;
    bool cannotBeABaseURL() const { return m_cannotBeABaseURL; }

    bool isMatchingDomain(const String&) const;

    bool setProtocol(const String&);
    void setHost(const String&);

    void removePort();
    void setPort(unsigned short);

    // Input is like "foo.com" or "foo.com:8000".
    void setHostAndPort(const String&);

    void setUser(const String&);
    void setPass(const String&);

    // If you pass an empty path for HTTP or HTTPS URLs, the resulting path
    // will be "/".
    void setPath(const String&);

    // The query may begin with a question mark, or, if not, one will be added
    // for you. Setting the query to the empty string will leave a "?" in the
    // URL (with nothing after it). To clear the query, pass a null string.
    void setQuery(const String&);

    void setFragmentIdentifier(StringView);
    void removeFragmentIdentifier();

    void removeQueryAndFragmentIdentifier();

    static bool hostIsIPAddress(StringView);

    unsigned pathStart() const;
    unsigned pathEnd() const;
    unsigned pathAfterLastSlash() const;

    operator const String&() const { return string(); }

#if USE(CF)
    URL(CFURLRef);
    RetainPtr<CFURLRef> createCFURL() const;
#endif

#if USE(FOUNDATION)
    URL(NSURL*);
    operator NSURL*() const;
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
    void invalidate();
    static bool protocolIs(const String&, const char*);
    void copyToBuffer(Vector<char, 512>& buffer) const;
    unsigned hostStart() const;

    friend WTF_EXPORT_PRIVATE bool equalIgnoringFragmentIdentifier(const URL&, const URL&);
    friend WTF_EXPORT_PRIVATE bool protocolHostAndPortAreEqual(const URL&, const URL&);
    friend WTF_EXPORT_PRIVATE bool hostsAreEqual(const URL&, const URL&);

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

WTF_EXPORT_PRIVATE bool equalIgnoringFragmentIdentifier(const URL&, const URL&);
WTF_EXPORT_PRIVATE bool equalIgnoringQueryAndFragment(const URL&, const URL&);
WTF_EXPORT_PRIVATE bool protocolHostAndPortAreEqual(const URL&, const URL&);
WTF_EXPORT_PRIVATE bool hostsAreEqual(const URL&, const URL&);

WTF_EXPORT_PRIVATE const URL& blankURL();

// Functions to do URL operations on strings.
// These are operations that aren't faster on a parsed URL.
// These are also different from the URL functions in that they don't require the string to be a valid and parsable URL.
// This is especially important because valid javascript URLs are not necessarily considered valid by URL.

WTF_EXPORT_PRIVATE bool protocolIs(const String& url, const char* protocol);
WTF_EXPORT_PRIVATE bool protocolIsJavaScript(const String& url);
WTF_EXPORT_PRIVATE bool protocolIsJavaScript(StringView url);
WTF_EXPORT_PRIVATE bool protocolIsInHTTPFamily(const String& url);

WTF_EXPORT_PRIVATE std::optional<uint16_t> defaultPortForProtocol(StringView protocol);
WTF_EXPORT_PRIVATE bool isDefaultPortForProtocol(uint16_t port, StringView protocol);
WTF_EXPORT_PRIVATE bool portAllowed(const URL&); // Blacklist ports that should never be used for Web resources.

WTF_EXPORT_PRIVATE void registerDefaultPortForProtocolForTesting(uint16_t port, const String& protocol);
WTF_EXPORT_PRIVATE void clearDefaultPortForProtocolMapForTesting();

WTF_EXPORT_PRIVATE bool isValidProtocol(const String&);

WTF_EXPORT_PRIVATE String mimeTypeFromDataURL(const String& url);

// FIXME: This is a wrong concept to expose, different parts of a URL need different escaping per the URL Standard.
WTF_EXPORT_PRIVATE String encodeWithURLEscapeSequences(const String&);

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

WTF_EXPORT_PRIVATE WTF::TextStream& operator<<(WTF::TextStream&, const URL&);

template<> struct DefaultHash<URL>;
template<> struct HashTraits<URL>;

} // namespace WTF
