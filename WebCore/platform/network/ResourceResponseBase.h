/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include <memory>

namespace WebCore {

class ResourceResponse;
struct CrossThreadResourceResponseData;

// Do not use this class directly, use the class ResponseResponse instead
class ResourceResponseBase {
public:
    static std::auto_ptr<ResourceResponse> adopt(std::auto_ptr<CrossThreadResourceResponseData>);

    // Gets a copy of the data suitable for passing to another thread.
    std::auto_ptr<CrossThreadResourceResponseData> copyData() const;

    bool isNull() const { return m_isNull; }
    bool isHTTP() const;

    const KURL& url() const;
    void setURL(const KURL& url);

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

    bool isMultipart() const { return mimeType() == "multipart/x-mixed-replace"; }

    bool isAttachment() const;

    void setExpirationDate(time_t);
    time_t expirationDate() const;

    void setLastModifiedDate(time_t);
    time_t lastModifiedDate() const;

    bool cacheControlContainsNoCache() const
    {
        if (!m_haveParsedCacheControl)
            parseCacheControlDirectives();
        return m_cacheControlContainsNoCache;
    }
    bool cacheControlContainsMustRevalidate() const
    {
        if (!m_haveParsedCacheControl)
            parseCacheControlDirectives();
        return m_cacheControlContainsMustRevalidate;
    }

    // The ResourceResponse subclass may "shadow" this method to provide platform-specific memory usage information
    unsigned memoryUsage() const
    {
        // average size, mostly due to URL and Header Map strings
        return 1280;
    }

    static bool compare(const ResourceResponse& a, const ResourceResponse& b);

protected:
    ResourceResponseBase()  
        : m_expectedContentLength(0)
        , m_httpStatusCode(0)
        , m_expirationDate(0)
        , m_lastModifiedDate(0)
        , m_isNull(true)
        , m_haveParsedCacheControl(false)
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
        , m_haveParsedCacheControl(false)
    {
    }

    void lazyInit() const;

    // The ResourceResponse subclass may "shadow" this method to lazily initialize platform specific fields
    void platformLazyInit() { }

    // The ResourceResponse subclass may "shadow" this method to compare platform specific fields
    static bool platformCompare(const ResourceResponse&, const ResourceResponse&) { return true; }

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
    bool m_isNull : 1;

private:
    void parseCacheControlDirectives() const;

    mutable bool m_haveParsedCacheControl : 1;
    mutable bool m_cacheControlContainsMustRevalidate : 1;
    mutable bool m_cacheControlContainsNoCache : 1;
};

inline bool operator==(const ResourceResponse& a, const ResourceResponse& b) { return ResourceResponseBase::compare(a, b); }
inline bool operator!=(const ResourceResponse& a, const ResourceResponse& b) { return !(a == b); }

struct CrossThreadResourceResponseData {
    KURL m_url;
    String m_mimeType;
    long long m_expectedContentLength;
    String m_textEncodingName;
    String m_suggestedFilename;
    int m_httpStatusCode;
    String m_httpStatusText;
    OwnPtr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
    time_t m_expirationDate;
    time_t m_lastModifiedDate;
    bool m_haveParsedCacheControl : 1;
    bool m_cacheControlContainsMustRevalidate : 1;
    bool m_cacheControlContainsNoCache : 1;
};

} // namespace WebCore

#endif // ResourceResponseBase_h
