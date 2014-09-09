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

#ifndef ResourceResponseBase_h
#define ResourceResponseBase_h

#include "CertificateInfo.h"
#include "HTTPHeaderMap.h"
#include "URL.h"
#include "ResourceLoadTiming.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

#if OS(SOLARIS)
#include <sys/time.h> // For time_t structure.
#endif

namespace WebCore {

class ResourceResponse;
struct CrossThreadResourceResponseData;

// Do not use this class directly, use the class ResponseResponse instead
class ResourceResponseBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ResourceResponse> adopt(PassOwnPtr<CrossThreadResourceResponseData>);

    // Gets a copy of the data suitable for passing to another thread.
    PassOwnPtr<CrossThreadResourceResponseData> copyData() const;

    bool isNull() const { return m_isNull; }
    WEBCORE_EXPORT bool isHTTP() const;

    WEBCORE_EXPORT const URL& url() const;
    WEBCORE_EXPORT void setURL(const URL& url);

    WEBCORE_EXPORT const String& mimeType() const;
    WEBCORE_EXPORT void setMimeType(const String& mimeType);

    WEBCORE_EXPORT long long expectedContentLength() const;
    WEBCORE_EXPORT void setExpectedContentLength(long long expectedContentLength);

    WEBCORE_EXPORT const String& textEncodingName() const;
    WEBCORE_EXPORT void setTextEncodingName(const String& name);

    WEBCORE_EXPORT int httpStatusCode() const;
    WEBCORE_EXPORT void setHTTPStatusCode(int);
    
    WEBCORE_EXPORT const String& httpStatusText() const;
    WEBCORE_EXPORT void setHTTPStatusText(const String&);

    WEBCORE_EXPORT const HTTPHeaderMap& httpHeaderFields() const;

    String httpHeaderField(const String& name) const;
    WEBCORE_EXPORT String httpHeaderField(HTTPHeaderName) const;
    WEBCORE_EXPORT void setHTTPHeaderField(const String& name, const String& value);
    void setHTTPHeaderField(HTTPHeaderName, const String& value);

    void addHTTPHeaderField(const String& name, const String& value);

    // Instead of passing a string literal to any of these functions, just use a HTTPHeaderName instead.
    template<size_t length> String httpHeaderField(const char (&)[length]) const = delete;
    template<size_t length> void setHTTPHeaderField(const char (&)[length], const String&) = delete;
    template<size_t length> void addHTTPHeaderField(const char (&)[length], const String&) = delete;

    bool isMultipart() const { return mimeType() == "multipart/x-mixed-replace"; }

    WEBCORE_EXPORT bool isAttachment() const;
    WEBCORE_EXPORT String suggestedFilename() const;

    void includeCertificateInfo() const;
    CertificateInfo certificateInfo() const;
    
    // These functions return parsed values of the corresponding response headers.
    // NaN means that the header was not present or had invalid value.
    bool cacheControlContainsNoCache() const;
    bool cacheControlContainsNoStore() const;
    bool cacheControlContainsMustRevalidate() const;
    bool hasCacheValidatorFields() const;
    double cacheControlMaxAge() const;
    double date() const;
    double age() const;
    double expires() const;
    WEBCORE_EXPORT double lastModified() const;

    unsigned connectionID() const;
    void setConnectionID(unsigned);

    bool connectionReused() const;
    void setConnectionReused(bool);

    bool wasCached() const;
    void setWasCached(bool);

    ResourceLoadTiming& resourceLoadTiming() const { return m_resourceLoadTiming; }

    // The ResourceResponse subclass may "shadow" this method to provide platform-specific memory usage information
    unsigned memoryUsage() const
    {
        // average size, mostly due to URL and Header Map strings
        return 1280;
    }

    static bool compare(const ResourceResponse&, const ResourceResponse&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, ResourceResponseBase&);

protected:
    enum InitLevel {
        Uninitialized,
        CommonFieldsOnly,
        AllFields
    };

    WEBCORE_EXPORT ResourceResponseBase();
    ResourceResponseBase(const URL&, const String& mimeType, long long expectedLength, const String& textEncodingName);

    void lazyInit(InitLevel) const;

    // The ResourceResponse subclass should shadow these functions to lazily initialize platform specific fields
    void platformLazyInit(InitLevel) { }
    CertificateInfo platformCertificateInfo() const { return CertificateInfo(); };
    String platformSuggestedFileName() const { return String(); }

    static bool platformCompare(const ResourceResponse&, const ResourceResponse&) { return true; }

    URL m_url;
    AtomicString m_mimeType;
    long long m_expectedContentLength;
    AtomicString m_textEncodingName;
    AtomicString m_httpStatusText;
    HTTPHeaderMap m_httpHeaderFields;
    mutable ResourceLoadTiming m_resourceLoadTiming;

    mutable bool m_includesCertificateInfo;
    mutable CertificateInfo m_certificateInfo;

    int m_httpStatusCode;
    unsigned m_connectionID;

private:
    mutable double m_cacheControlMaxAge;
    mutable double m_age;
    mutable double m_date;
    mutable double m_expires;
    mutable double m_lastModified;

public:
    bool m_wasCached : 1;
    bool m_connectionReused : 1;

    bool m_isNull : 1;
    
private:
    const ResourceResponse& asResourceResponse() const;
    void parseCacheControlDirectives() const;
    void updateHeaderParsedState(HTTPHeaderName);

    mutable bool m_haveParsedCacheControlHeader : 1;
    mutable bool m_haveParsedAgeHeader : 1;
    mutable bool m_haveParsedDateHeader : 1;
    mutable bool m_haveParsedExpiresHeader : 1;
    mutable bool m_haveParsedLastModifiedHeader : 1;

    mutable bool m_cacheControlContainsNoCache : 1;
    mutable bool m_cacheControlContainsNoStore : 1;
    mutable bool m_cacheControlContainsMustRevalidate : 1;
};

inline bool operator==(const ResourceResponse& a, const ResourceResponse& b) { return ResourceResponseBase::compare(a, b); }
inline bool operator!=(const ResourceResponse& a, const ResourceResponse& b) { return !(a == b); }

template<class Encoder>
void ResourceResponseBase::encode(Encoder& encoder) const
{
    encoder << m_isNull;
    if (m_isNull)
        return;
    lazyInit(AllFields);

    encoder << m_url.string();
    encoder << m_mimeType;
    encoder << static_cast<int64_t>(m_expectedContentLength);
    encoder << m_textEncodingName;
    encoder << m_httpStatusText;
    encoder << m_httpHeaderFields;
    encoder << m_resourceLoadTiming;
    encoder << m_httpStatusCode;
    encoder << m_connectionID;
    encoder << m_includesCertificateInfo;
    if (m_includesCertificateInfo)
        encoder << m_certificateInfo;
}

template<class Decoder>
bool ResourceResponseBase::decode(Decoder& decoder, ResourceResponseBase& response)
{
    ASSERT(response.m_isNull);
    bool responseIsNull;
    if (!decoder.decode(responseIsNull))
        return false;
    if (responseIsNull)
        return true;

    String url;
    if (!decoder.decode(url))
        return false;
    response.m_url = URL(URL(), url);
    if (!decoder.decode(response.m_mimeType))
        return false;
    int64_t expectedContentLength;
    if (!decoder.decode(expectedContentLength))
        return false;
    response.m_expectedContentLength = expectedContentLength;
    if (!decoder.decode(response.m_textEncodingName))
        return false;
    if (!decoder.decode(response.m_httpStatusText))
        return false;
    if (!decoder.decode(response.m_httpHeaderFields))
        return false;
    if (!decoder.decode(response.m_resourceLoadTiming))
        return false;
    if (!decoder.decode(response.m_httpStatusCode))
        return false;
    if (!decoder.decode(response.m_connectionID))
        return false;
    if (!decoder.decode(response.m_includesCertificateInfo))
        return false;
    if (response.m_includesCertificateInfo) {
        if (!decoder.decode(response.m_certificateInfo))
            return false;
    }
    response.m_isNull = false;

    return true;
}

struct CrossThreadResourceResponseDataBase {
    WTF_MAKE_NONCOPYABLE(CrossThreadResourceResponseDataBase); WTF_MAKE_FAST_ALLOCATED;
public:
    CrossThreadResourceResponseDataBase() { }
    URL m_url;
    String m_mimeType;
    long long m_expectedContentLength;
    String m_textEncodingName;
    int m_httpStatusCode;
    String m_httpStatusText;
    std::unique_ptr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
    ResourceLoadTiming m_resourceLoadTiming;
};

} // namespace WebCore

#endif // ResourceResponseBase_h
