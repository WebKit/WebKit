/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ResourceResponseBase_h
#define ResourceResponseBase_h

#include "HTTPHeaderMap.h"
#include "KURL.h"

namespace WebCore {

typedef HashMap<String, HashSet<String>, CaseFoldingHash> CacheControlDirectiveMap;
typedef HashMap<String, String, CaseFoldingHash> PragmaDirectiveMap;

class ResourceResponse;

// Do not use this class directly, use the class ResponseResponse instead
class ResourceResponseBase {
 public:

    bool isNull() const { return m_isNull; }
    bool isHTTP() const;

    const KURL& url() const;
    void setUrl(const KURL& url);

    const String& mimeType() const;
    void setMimeType(const String& mimeType);

    long long expectedContentLength() const;
    void setExpectedContentLength(long long expectedContentLength);

    const String& textEncodingName() const;
    void setTextEncodingName(const String& name);

    // FIXME should compute this on the fly
    const String& suggestedFilename() const;
    void setSuggestedFilename(const String&);

    int httpStatusCode() const;
    void setHTTPStatusCode(int);
    
    const String& httpStatusText() const;
    void setHTTPStatusText(const String&);
    
    String httpHeaderField(const AtomicString& name) const;
    void setHTTPHeaderField(const AtomicString& name, const String& value);
    const HTTPHeaderMap& httpHeaderFields() const;

    const PragmaDirectiveMap& parsePragmaDirectives() const;
    const CacheControlDirectiveMap& parseCacheControlDirectives() const;

    bool isMultipart() const { return mimeType() == "multipart/x-mixed-replace"; }

    bool isAttachment() const;

    void setExpirationDate(time_t);
    time_t expirationDate() const;

    void setLastModifiedDate(time_t);
    time_t lastModifiedDate() const;

    static bool compare(const ResourceResponse& a, const ResourceResponse& b);

 protected:
    ResourceResponseBase()  
        : m_expectedContentLength(0)
        , m_httpStatusCode(0)
        , m_expirationDate(0)
        , m_lastModifiedDate(0)
        , m_isNull(true)
        , m_haveParsedCacheControlHeader(false)
        , m_haveParsedPragmaHeader(false)
    {
    }

    ResourceResponseBase(const KURL& url, const String& mimeType, long long expectedLength, const String& textEncodingName, const String& filename)
        : m_url(url)
        , m_mimeType(mimeType)
        , m_expectedContentLength(expectedLength)
        , m_textEncodingName(textEncodingName)
        , m_suggestedFilename(filename)
        , m_httpStatusCode(0)
        , m_expirationDate(0)
        , m_lastModifiedDate(0)
        , m_isNull(false)
        , m_haveParsedCacheControlHeader(false)
        , m_haveParsedPragmaHeader(false)
    {
    }

    void lazyInit() const;

    // The ResourceResponse subclass may "shadow" this method to lazily initialize platform specific fields
    void platformLazyInit() {}

    // The ResourceResponse subclass may "shadow" this method to compare platform specific fields
    static bool platformCompare(const ResourceResponse& a, const ResourceResponse& b) { return true; }

    KURL m_url;
    String m_mimeType;
    long long m_expectedContentLength;
    String m_textEncodingName;
    String m_suggestedFilename;
    int m_httpStatusCode;
    String m_httpStatusText;
    HTTPHeaderMap m_httpHeaderFields;
    time_t m_expirationDate;
    time_t m_lastModifiedDate;
    bool m_isNull:1;
    mutable bool m_haveParsedCacheControlHeader:1;
    mutable bool m_haveParsedPragmaHeader:1;
    mutable CacheControlDirectiveMap m_cacheControlDirectiveMap;
    mutable PragmaDirectiveMap m_pragmaDirectiveMap;
};

inline bool operator==(const ResourceResponse& a, const ResourceResponse& b) { return ResourceResponseBase::compare(a, b); }
inline bool operator!=(const ResourceResponse& a, const ResourceResponse& b) { return !(a == b); }

} // namespace WebCore

#endif // ResourceResponseBase_h
