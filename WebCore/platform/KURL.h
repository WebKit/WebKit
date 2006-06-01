/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef KURL_H_
#define KURL_H_

#include "TextEncoding.h"

#if __APPLE__
#ifdef __OBJC__
@class NSData;
@class NSURL;
#else
class NSData;
class NSURL;
#endif
#endif

namespace WebCore {
    class String;
}

class KURL {
public:
    KURL();
    KURL(const char*);
    KURL(const KURL&, const DeprecatedString&, const WebCore::TextEncoding& encoding = WebCore::TextEncoding(WebCore::UTF8Encoding));
    KURL(const DeprecatedString&);
#if __APPLE__
    KURL(NSURL*);
#endif
    
    bool isEmpty() const { return urlString.isEmpty(); } 
    bool isMalformed() const { return !m_isValid; }
    bool isValid() const { return m_isValid; }
    bool hasPath() const;

    DeprecatedString url() const { return urlString; }

    DeprecatedString protocol() const;
    DeprecatedString host() const;
    unsigned short int port() const;
    DeprecatedString user() const;
    DeprecatedString pass() const;
    DeprecatedString path() const;
    DeprecatedString query() const;
    DeprecatedString ref() const;
    bool hasRef() const;

    DeprecatedString encodedHtmlRef() const { return ref(); }

    void setProtocol(const DeprecatedString &);
    void setHost(const DeprecatedString &);
    void setPort(unsigned short int);
    void setUser(const DeprecatedString &);
    void setPass(const DeprecatedString &);
    void setPath(const DeprecatedString &);
    void setQuery(const DeprecatedString &);
    void setRef(const DeprecatedString &);

    DeprecatedString prettyURL() const;

#if __APPLE__
    CFURLRef createCFURL() const;
    NSURL *getNSURL() const;
#endif

    bool isLocalFile() const;

    static DeprecatedString decode_string(const DeprecatedString &, const WebCore::TextEncoding& encoding = WebCore::TextEncoding(WebCore::UTF8Encoding));
    static DeprecatedString encode_string(const DeprecatedString &);
    
    friend bool operator==(const KURL &, const KURL &);

private:
    bool isHierarchical() const;
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

#endif
