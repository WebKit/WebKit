/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef KURL_h
#define KURL_h

#include "DeprecatedString.h"
#include <wtf/Platform.h>

#if PLATFORM(CF)
typedef const struct __CFURL * CFURLRef;
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSURL;
#else
class NSURL;
#endif
#endif

namespace WebCore {

    class KURL;
    class String;
    class TextEncoding;

    bool operator==(const KURL&, const KURL&);
    inline bool operator!=(const KURL &a, const KURL &b) { return !(a == b); }

    bool equalIgnoringRef(const KURL&, const KURL&);

class KURL {
public:
    KURL();
    KURL(const char*);
    KURL(const KURL&, const DeprecatedString&);
    KURL(const KURL&, const DeprecatedString&, const TextEncoding&);
    KURL(const DeprecatedString&);
#if PLATFORM(MAC)
    KURL(NSURL*);
#endif
#if PLATFORM(CF)
    KURL(CFURLRef);
#endif
    bool isEmpty() const { return urlString.isEmpty(); } 
    bool hasPath() const;

    DeprecatedString url() const { return urlString; }

    DeprecatedString protocol() const;
    DeprecatedString host() const;
    unsigned short int port() const;
    DeprecatedString user() const;
    DeprecatedString pass() const;
    DeprecatedString path() const;
    DeprecatedString lastPathComponent() const;
    DeprecatedString query() const;
    DeprecatedString ref() const;
    bool hasRef() const;

    DeprecatedString encodedHtmlRef() const { return ref(); }

    void setProtocol(const DeprecatedString &);
    void setHost(const DeprecatedString &);
    void setPort(unsigned short int);
    void setHostAndPort(const DeprecatedString&);
    void setUser(const DeprecatedString &);
    void setPass(const DeprecatedString &);
    void setPath(const DeprecatedString &);
    void setQuery(const DeprecatedString &);
    void setRef(const DeprecatedString &);

    DeprecatedString prettyURL() const;

#if PLATFORM(CF)
    CFURLRef createCFURL() const;
#endif
#if PLATFORM(MAC)
    NSURL *getNSURL() const;
#endif

    bool isLocalFile() const;

    static DeprecatedString decode_string(const DeprecatedString&);
    static DeprecatedString decode_string(const DeprecatedString&, const TextEncoding&);
    static DeprecatedString encode_string(const DeprecatedString&);
    
    friend bool operator==(const KURL &, const KURL &);

#ifndef NDEBUG
    void print() const;
#endif

private:
    bool isHierarchical() const;
    void init(const KURL&, const DeprecatedString&, const TextEncoding&);
    void parse(const char *url, const DeprecatedString *originalString);

    DeprecatedString urlString;
    bool m_isValid;
    int schemeEndPos;
    int userStartPos;
    int userEndPos;
    int passwordEndPos;
    int hostEndPos;
    int portEndPos;
    int pathEndPos;
    int queryEndPos;
    int fragmentEndPos;
    
    friend bool equalIgnoringRef(const KURL& a, const KURL& b);
};

}

#endif // KURL_h
