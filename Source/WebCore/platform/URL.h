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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef URL_h
#define URL_h

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
typedef const struct __CFURL* CFURLRef;
#endif

#if USE(SOUP)
#include "GUniquePtrSoup.h"
#endif

#if PLATFORM(MAC)
OBJC_CLASS NSURL;
#endif

namespace WebCore {

class TextEncoding;
struct URLHash;

enum ParsedURLStringTag { ParsedURLString };

class URL {
public:
    // Generates a URL which contains a null string.
    URL() { invalidate(); }

    // The argument is an absolute URL string. The string is assumed to be output of URL::string() called on a valid
    // URL object, or indiscernible from such.
    // It is usually best to avoid repeatedly parsing a string, unless memory saving outweigh the possible slow-downs.
    URL(ParsedURLStringTag, const String&);
    explicit URL(WTF::HashTableDeletedValueType) : m_string(WTF::HashTableDeletedValue) { }
    bool isHashTableDeletedValue() const { return string().isHashTableDeletedValue(); }

    // Resolves the relative URL with the given base URL. If provided, the
    // TextEncoding is used to encode non-ASCII characers. The base URL can be
    // null or empty, in which case the relative URL will be interpreted as
    // absolute.
    // FIXME: If the base URL is invalid, this always creates an invalid
    // URL. Instead I think it would be better to treat all invalid base URLs
    // the same way we treate null and empty base URLs.
    URL(const URL& base, const String& relative);
    URL(const URL& base, const String& relative, const TextEncoding&);

    String strippedForUseAsReferrer() const;

    // FIXME: The above functions should be harmonized so that passing a
    // base of null or the empty string gives the same result as the
    // standard String constructor.

    // Makes a deep copy. Helpful only if you need to use a URL on another
    // thread.  Since the underlying StringImpl objects are immutable, there's
    // no other reason to ever prefer copy() over plain old assignment.
    URL copy() const;

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

    String protocol() const;
    String host() const;
    unsigned short port() const;
    bool hasPort() const;
    String user() const;
    String pass() const;
    String path() const;
    String lastPathComponent() const;
    String query() const;
    String fragmentIdentifier() const;
    bool hasFragmentIdentifier() const;

    String baseAsString() const;

    String fileSystemPath() const;

    // Returns true if the current URL's protocol is the same as the null-
    // terminated ASCII argument. The argument must be lower-case.
    bool protocolIs(const char*) const;
    bool protocolIsData() const { return protocolIs("data"); }
    bool protocolIsInHTTPFamily() const;
    bool isLocalFile() const;
    bool isBlankURL() const;

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

    void setFragmentIdentifier(const String&);
    void removeFragmentIdentifier();

    friend bool equalIgnoringFragmentIdentifier(const URL&, const URL&);

    friend bool protocolHostAndPortAreEqual(const URL&, const URL&);

    unsigned hostStart() const;
    unsigned hostEnd() const;

    unsigned pathStart() const;
    unsigned pathEnd() const;
    unsigned pathAfterLastSlash() const;

    operator const String&() const { return string(); }

#if USE(CF)
    URL(CFURLRef);
    RetainPtr<CFURLRef> createCFURL() const;
#endif

#if USE(SOUP)
    URL(SoupURI*);
    GUniquePtr<SoupURI> createSoupURI() const;
#endif

#if PLATFORM(MAC)
    URL(NSURL*);
    operator NSURL*() const;
#endif
#ifdef __OBJC__
    operator NSString*() const { return string(); }
#endif

    const URL* innerURL() const { return 0; }

#ifndef NDEBUG
    void print() const;
#endif

    bool isSafeToSendToAnotherThread() const;

private:
    void invalidate();
    static bool protocolIs(const String&, const char*);
    void init(const URL&, const String&, const TextEncoding&);
    void copyToBuffer(Vector<char, 512>& buffer) const;

    // Parses the given URL. The originalString parameter allows for an
    // optimization: When the source is the same as the fixed-up string,
    // it will use the passed-in string instead of allocating a new one.
    void parse(const String&);
    void parse(const char* url, const String* originalString = 0);

    bool hasPath() const;

    String m_string;
    bool m_isValid : 1;
    bool m_protocolIsInHTTPFamily : 1;

    int m_schemeEnd;
    int m_userStart;
    int m_userEnd;
    int m_passwordEnd;
    int m_hostEnd;
    int m_portEnd;
    int m_pathAfterLastSlash;
    int m_pathEnd;
    int m_queryEnd;
    int m_fragmentEnd;
};

bool operator==(const URL&, const URL&);
bool operator==(const URL&, const String&);
bool operator==(const String&, const URL&);
bool operator!=(const URL&, const URL&);
bool operator!=(const URL&, const String&);
bool operator!=(const String&, const URL&);

bool equalIgnoringFragmentIdentifier(const URL&, const URL&);
bool protocolHostAndPortAreEqual(const URL&, const URL&);

const URL& blankURL();

// Functions to do URL operations on strings.
// These are operations that aren't faster on a parsed URL.
// These are also different from the URL functions in that they don't require the string to be a valid and parsable URL.
// This is especially important because valid javascript URLs are not necessarily considered valid by URL.

bool protocolIs(const String& url, const char* protocol);
bool protocolIsJavaScript(const String& url);
bool protocolIsInHTTPFamily(const String& url);

bool isDefaultPortForProtocol(unsigned short port, const String& protocol);
bool portAllowed(const URL&); // Blacklist ports that should never be used for Web resources.

bool isValidProtocol(const String&);

String mimeTypeFromDataURL(const String& url);
String mimeTypeFromURL(const URL&);

// Unescapes the given string using URL escaping rules, given an optional
// encoding (defaulting to UTF-8 otherwise). DANGER: If the URL has "%00"
// in it, the resulting string will have embedded null characters!
String decodeURLEscapeSequences(const String&);
String decodeURLEscapeSequences(const String&, const TextEncoding&);

String encodeWithURLEscapeSequences(const String&);

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
    return m_pathEnd != m_portEnd;
}

inline bool URL::hasPort() const
{
    return m_hostEnd < m_portEnd;
}

inline bool URL::protocolIsInHTTPFamily() const
{
    return m_protocolIsInHTTPFamily;
}

inline unsigned URL::hostStart() const
{
    return (m_passwordEnd == m_userStart) ? m_passwordEnd : m_passwordEnd + 1;
}

inline unsigned URL::hostEnd() const
{
    return m_hostEnd;
}

inline unsigned URL::pathStart() const
{
    return m_portEnd;
}

inline unsigned URL::pathEnd() const
{
    return m_pathEnd;
}

inline unsigned URL::pathAfterLastSlash() const
{
    return m_pathAfterLastSlash;
}

#if PLATFORM(IOS)
void enableURLSchemeCanonicalization(bool);
#endif

} // namespace WebCore

namespace WTF {

    // URLHash is the default hash for String
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::URL> {
        typedef WebCore::URLHash Hash;
    };

} // namespace WTF

#endif // URL_h
