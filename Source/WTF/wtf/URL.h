/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include <wtf/text/WTFString.h>

#if USE(GLIB) && HAVE(GURI)
#include <wtf/glib/GRefPtr.h>
#endif

#if USE(CF)
typedef const struct __CFURL* CFURLRef;
#endif

#if USE(FOUNDATION)
OBJC_CLASS NSURL;
#endif

namespace WTF {

class PrintStream;
class TextStream;

class URLTextEncoding {
public:
    virtual Vector<uint8_t> encodeForURLParsing(StringView) const = 0;
protected:
    virtual ~URLTextEncoding() = default;
};

class URL {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Generates a URL which contains a null string.
    URL() { invalidate(); }

    explicit URL(HashTableDeletedValueType);
    bool isHashTableDeletedValue() const { return m_string.isHashTableDeletedValue(); }

    // Resolves the relative URL with the given base URL. If provided, the
    // URLTextEncoding is used to encode non-ASCII characters. The base URL can be
    // null or empty, in which case the relative URL will be interpreted as absolute.
    WTF_EXPORT_PRIVATE URL(const URL& base, const String& relative, const URLTextEncoding* = nullptr);

    // Parses the input string as an absolute URL. If you need to parse a relative URL, call the constructor above
    // taking a base URL and a relative URL string.
    WTF_EXPORT_PRIVATE explicit URL(String&& absoluteURL, const URLTextEncoding* = nullptr);
    explicit URL(const String& absoluteURL, const URLTextEncoding* encoding = nullptr)
        : URL(String { absoluteURL }, encoding)
    {
    }

    URL(const URL&) = default;
    URL& operator=(const URL&) = default;

    URL(URL&& other)
        : m_string(WTFMove(other.m_string))
        , m_isValid(other.m_isValid)
        , m_protocolIsInHTTPFamily(other.m_protocolIsInHTTPFamily)
        , m_hasOpaquePath(other.m_hasOpaquePath)
        , m_portLength(other.m_portLength)
        , m_schemeEnd(other.m_schemeEnd)
        , m_userStart(other.m_userStart)
        , m_userEnd(other.m_userEnd)
        , m_passwordEnd(other.m_passwordEnd)
        , m_hostEnd(other.m_hostEnd)
        , m_pathAfterLastSlash(other.m_pathAfterLastSlash)
        , m_pathEnd(other.m_pathEnd)
        , m_queryEnd(other.m_queryEnd)
    {
        other.m_isValid = false;
    }

    URL& operator=(URL&& other)
    {
        m_string = WTFMove(other.m_string);
        m_isValid = other.m_isValid;
        other.m_isValid = false;
        m_protocolIsInHTTPFamily = other.m_protocolIsInHTTPFamily;
        m_hasOpaquePath = other.m_hasOpaquePath;
        m_portLength = other.m_portLength;
        m_schemeEnd = other.m_schemeEnd;
        m_userStart = other.m_userStart;
        m_userEnd = other.m_userEnd;
        m_passwordEnd = other.m_passwordEnd;
        m_hostEnd = other.m_hostEnd;
        m_pathAfterLastSlash = other.m_pathAfterLastSlash;
        m_pathEnd = other.m_pathEnd;
        m_queryEnd = other.m_queryEnd;

        return *this;
    }

    WTF_EXPORT_PRIVATE static URL fakeURLWithRelativePart(StringView);
    WTF_EXPORT_PRIVATE static URL fileURLWithFileSystemPath(StringView);

    WTF_EXPORT_PRIVATE String strippedForUseAsReferrer() const;
    WTF_EXPORT_PRIVATE String strippedForUseAsReferrerWithExplicitPort() const;

    // Similar to strippedForUseAsReferrer except we also remove the query component.
    WTF_EXPORT_PRIVATE String strippedForUseAsReport() const;

    // Makes a deep copy. Helpful only if you need to use a URL on another
    // thread. Since the underlying StringImpl objects are immutable, there's
    // no other reason to ever prefer isolatedCopy() over plain old assignment.
    WTF_EXPORT_PRIVATE URL isolatedCopy() const &;
    WTF_EXPORT_PRIVATE URL isolatedCopy() &&;

    bool isNull() const;
    bool isEmpty() const;
    bool isValid() const;

    // Since we overload operator NSURL * we have this to prevent accidentally using that operator
    // when placing a URL in an if statment.
    operator bool() const = delete;

    const String& string() const { return m_string; }
    WTF_EXPORT_PRIVATE String stringCenterEllipsizedToLength(unsigned length = 1024) const;

    // Unlike user() and password(), encodedUser() and encodedPassword() don't decode escape sequences.
    // This is necessary for accurate round-tripping, because encoding doesn't encode '%' characters.

    WTF_EXPORT_PRIVATE StringView protocol() const;
    WTF_EXPORT_PRIVATE StringView encodedUser() const;
    WTF_EXPORT_PRIVATE StringView encodedPassword() const;
    WTF_EXPORT_PRIVATE StringView host() const;
    WTF_EXPORT_PRIVATE std::optional<uint16_t> port() const;
    WTF_EXPORT_PRIVATE StringView path() const;
    WTF_EXPORT_PRIVATE StringView lastPathComponent() const;
    WTF_EXPORT_PRIVATE StringView query() const;
    WTF_EXPORT_PRIVATE StringView fragmentIdentifier() const;

    WTF_EXPORT_PRIVATE StringView queryWithLeadingQuestionMark() const;
    WTF_EXPORT_PRIVATE StringView fragmentIdentifierWithLeadingNumberSign() const;
    WTF_EXPORT_PRIVATE StringView viewWithoutQueryOrFragmentIdentifier() const;
    WTF_EXPORT_PRIVATE StringView viewWithoutFragmentIdentifier() const;
    WTF_EXPORT_PRIVATE String stringWithoutFragmentIdentifier() const;

    WTF_EXPORT_PRIVATE String protocolHostAndPort() const;
    WTF_EXPORT_PRIVATE String hostAndPort() const;
    WTF_EXPORT_PRIVATE String user() const;
    WTF_EXPORT_PRIVATE String password() const;
    WTF_EXPORT_PRIVATE String fileSystemPath() const;

    WTF_EXPORT_PRIVATE URL truncatedForUseAsBase() const;

    bool hasQuery() const;
    bool hasFragmentIdentifier() const;
    bool hasPath() const;

    bool hasCredentials() const;

    // Returns true if the current URL's protocol is the same as the null-
    // terminated ASCII argument. The argument must be lower-case.
    WTF_EXPORT_PRIVATE bool protocolIs(StringView) const;
    bool protocolIsAbout() const { return protocolIs("about"_s); }
    bool protocolIsBlob() const { return protocolIs("blob"_s); }
    bool protocolIsData() const { return protocolIs("data"_s); }
    bool protocolIsFile() const { return protocolIs("file"_s); }
    WTF_EXPORT_PRIVATE bool protocolIsJavaScript() const;
    bool protocolIsInFTPFamily() const { return protocolIs("ftp"_s) || protocolIs("ftps"_s); }
    bool protocolIsInHTTPFamily() const { return m_protocolIsInHTTPFamily; }
    bool hasOpaquePath() const { return m_hasOpaquePath; }

    WTF_EXPORT_PRIVATE bool isAboutBlank() const;
    WTF_EXPORT_PRIVATE bool isAboutSrcDoc() const;

    WTF_EXPORT_PRIVATE bool isMatchingDomain(StringView) const;

    WTF_EXPORT_PRIVATE bool setProtocol(StringView);
    WTF_EXPORT_PRIVATE void setHost(StringView);

    WTF_EXPORT_PRIVATE void setPort(std::optional<uint16_t>);

    // Input is like "foo.com" or "foo.com:8000".
    WTF_EXPORT_PRIVATE void setHostAndPort(StringView);

    WTF_EXPORT_PRIVATE void setUser(StringView);
    WTF_EXPORT_PRIVATE void setPassword(StringView);
    WTF_EXPORT_PRIVATE void removeCredentials();

    // If you pass an empty path for HTTP or HTTPS URLs, the resulting path will be "/".
    WTF_EXPORT_PRIVATE void setPath(StringView);

    // The query may begin with a question mark, or, if not, one will be added
    // for you. Setting the query to the empty string will leave a "?" in the
    // URL (with nothing after it). To clear the query, pass a null string.
    WTF_EXPORT_PRIVATE void setQuery(StringView);

    WTF_EXPORT_PRIVATE void setFragmentIdentifier(StringView);
    WTF_EXPORT_PRIVATE void removeFragmentIdentifier();
    WTF_EXPORT_PRIVATE String consumefragmentDirective();
    WTF_EXPORT_PRIVATE void removeQueryAndFragmentIdentifier();

    WTF_EXPORT_PRIVATE static bool hostIsIPAddress(StringView);

    WTF_EXPORT_PRIVATE unsigned pathStart() const;
    unsigned pathEnd() const;
    unsigned pathAfterLastSlash() const;

#if USE(CF)
    WTF_EXPORT_PRIVATE URL(CFURLRef);
    WTF_EXPORT_PRIVATE RetainPtr<CFURLRef> createCFURL() const;
#endif

#if USE(FOUNDATION)
    WTF_EXPORT_PRIVATE URL(NSURL *);
    WTF_EXPORT_PRIVATE operator NSURL *() const;
#endif

#if USE(GLIB) && HAVE(GURI)
    WTF_EXPORT_PRIVATE URL(GUri*);
    WTF_EXPORT_PRIVATE GRefPtr<GUri> createGUri() const;
#endif

#ifndef NDEBUG
    void print() const;
#endif

    WTF_EXPORT_PRIVATE void dump(PrintStream&) const;

    WTF_EXPORT_PRIVATE bool hasSpecialScheme() const;
    WTF_EXPORT_PRIVATE bool hasLocalScheme() const;

private:
    friend class URLParser;

    WTF_EXPORT_PRIVATE void invalidate();
    unsigned hostStart() const;
    unsigned credentialsEnd() const;
    void remove(unsigned start, unsigned length);
    void parse(String&&);
    void parseAllowingC0AtEnd(String&&);

    void maybeTrimTrailingSpacesFromOpaquePath();

    friend WTF_EXPORT_PRIVATE bool protocolHostAndPortAreEqual(const URL&, const URL&);

#if USE(CF)
    static RetainPtr<CFURLRef> emptyCFURL();
#endif

    String m_string;

    unsigned m_isValid : 1;
    unsigned m_protocolIsInHTTPFamily : 1;
    unsigned m_hasOpaquePath : 1;

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

bool operator==(const URL&, const URL&);
bool operator==(const URL&, const String&);
bool operator==(const String&, const URL&);

WTF_EXPORT_PRIVATE bool equalIgnoringFragmentIdentifier(const URL&, const URL&);
WTF_EXPORT_PRIVATE bool protocolHostAndPortAreEqual(const URL&, const URL&);
WTF_EXPORT_PRIVATE Vector<KeyValuePair<String, String>> differingQueryParameters(const URL&, const URL&);
WTF_EXPORT_PRIVATE Vector<KeyValuePair<String, String>> queryParameters(const URL&);
WTF_EXPORT_PRIVATE bool isEqualIgnoringQueryAndFragments(const URL&, const URL&);

// Returns the parameters that were removed (including duplicates), in the order that they appear in the URL.
WTF_EXPORT_PRIVATE Vector<String> removeQueryParameters(URL&, const HashSet<String>&);
WTF_EXPORT_PRIVATE Vector<String> removeQueryParameters(URL&, Function<bool(const String&)>&&);

WTF_EXPORT_PRIVATE const URL& aboutBlankURL();
WTF_EXPORT_PRIVATE const URL& aboutSrcDocURL();

// Functions to do URL operations on strings.
// These are operations that aren't faster on a parsed URL.
// These are also different from the WTF::URL functions in that they don't require the string to be a valid and parsable URL.
// This is especially important because valid javascript URLs are not necessarily considered valid by WTF::URL.

WTF_EXPORT_PRIVATE bool protocolIs(StringView url, ASCIILiteral protocol);
WTF_EXPORT_PRIVATE bool protocolIsJavaScript(StringView url);
WTF_EXPORT_PRIVATE bool protocolIsInHTTPFamily(StringView url);

WTF_EXPORT_PRIVATE std::optional<uint16_t> defaultPortForProtocol(StringView protocol);
WTF_EXPORT_PRIVATE bool isDefaultPortForProtocol(uint16_t port, StringView protocol);
WTF_EXPORT_PRIVATE bool portAllowed(const URL&); // Disallow ports that should never be used for Web resources.

WTF_EXPORT_PRIVATE void registerDefaultPortForProtocolForTesting(uint16_t port, const String& protocol);
WTF_EXPORT_PRIVATE void clearDefaultPortForProtocolMapForTesting();

WTF_EXPORT_PRIVATE String mimeTypeFromDataURL(StringView dataURL);

// FIXME: This needs a new, more specific name. The general thing named here can't be implemented correctly, since different parts of a URL need different escaping.
WTF_EXPORT_PRIVATE String encodeWithURLEscapeSequences(const String&);

#ifdef __OBJC__

WTF_EXPORT_PRIVATE RetainPtr<id> makeNSArrayElement(const URL&);
WTF_EXPORT_PRIVATE std::optional<URL> makeVectorElement(const URL*, id);

#endif

WTF_EXPORT_PRIVATE TextStream& operator<<(TextStream&, const URL&);

template<> struct DefaultHash<URL>;
template<> struct HashTraits<URL>;

// Function template and inline function definitions.

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

inline URL::URL(HashTableDeletedValueType)
    : m_string(HashTableDeletedValue)
{
}

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
    return m_pathEnd > pathStart();
}

inline bool URL::hasCredentials() const
{
    return m_passwordEnd > m_userStart;
}

inline bool URL::hasQuery() const
{
    return m_queryEnd > m_pathEnd;
}

inline bool URL::hasFragmentIdentifier() const
{
    return m_isValid && m_string.length() > m_queryEnd;
}

inline unsigned URL::pathEnd() const
{
    return m_pathEnd;
}

inline unsigned URL::pathAfterLastSlash() const
{
    return m_pathAfterLastSlash;
}

} // namespace WTF

using WTF::aboutBlankURL;
using WTF::aboutSrcDocURL;
