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

#include "config.h"
#include "ResourceResponseBase.h"

#include "CacheValidation.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ResourceResponse.h"
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace WebCore {

inline const ResourceResponse& ResourceResponseBase::asResourceResponse() const
{
    return *static_cast<const ResourceResponse*>(this);
}

ResourceResponseBase::ResourceResponseBase()
    : m_isNull(true)
    , m_expectedContentLength(0)
    , m_includesCertificateInfo(false)
    , m_httpStatusCode(0)
{
}

ResourceResponseBase::ResourceResponseBase(const URL& url, const String& mimeType, long long expectedLength, const String& textEncodingName)
    : m_isNull(false)
    , m_url(url)
    , m_mimeType(mimeType)
    , m_expectedContentLength(expectedLength)
    , m_textEncodingName(textEncodingName)
    , m_includesCertificateInfo(true) // Empty but valid for synthetic responses.
    , m_httpStatusCode(0)
{
}

std::unique_ptr<ResourceResponse> ResourceResponseBase::adopt(std::unique_ptr<CrossThreadResourceResponseData> data)
{
    auto response = std::make_unique<ResourceResponse>();
    response->setURL(data->m_url);
    response->setMimeType(data->m_mimeType);
    response->setExpectedContentLength(data->m_expectedContentLength);
    response->setTextEncodingName(data->m_textEncodingName);

    response->setHTTPStatusCode(data->m_httpStatusCode);
    response->setHTTPStatusText(data->m_httpStatusText);

    response->lazyInit(AllFields);
    response->m_httpHeaderFields.adopt(WTFMove(data->m_httpHeaders));
    response->m_resourceLoadTiming = data->m_resourceLoadTiming;
    response->doPlatformAdopt(WTFMove(data));
    return response;
}

std::unique_ptr<CrossThreadResourceResponseData> ResourceResponseBase::copyData() const
{
    auto data = std::make_unique<CrossThreadResourceResponseData>();
    data->m_url = url().isolatedCopy();
    data->m_mimeType = mimeType().isolatedCopy();
    data->m_expectedContentLength = expectedContentLength();
    data->m_textEncodingName = textEncodingName().isolatedCopy();
    data->m_httpStatusCode = httpStatusCode();
    data->m_httpStatusText = httpStatusText().isolatedCopy();
    data->m_httpHeaders = httpHeaderFields().copyData();
    data->m_resourceLoadTiming = m_resourceLoadTiming;
    return asResourceResponse().doPlatformCopyData(WTFMove(data));
}

bool ResourceResponseBase::isHTTP() const
{
    lazyInit(CommonFieldsOnly);

    String protocol = m_url.protocol();

    return equalIgnoringCase(protocol, "http")  || equalIgnoringCase(protocol, "https");
}

const URL& ResourceResponseBase::url() const
{
    lazyInit(CommonFieldsOnly);

    return m_url; 
}

void ResourceResponseBase::setURL(const URL& url)
{
    lazyInit(CommonFieldsOnly);
    m_isNull = false;

    m_url = url;

    // FIXME: Should invalidate or update platform response if present.
}

const String& ResourceResponseBase::mimeType() const
{
    lazyInit(CommonFieldsOnly);

    return m_mimeType; 
}

void ResourceResponseBase::setMimeType(const String& mimeType)
{
    lazyInit(CommonFieldsOnly);
    m_isNull = false;

    // FIXME: MIME type is determined by HTTP Content-Type header. We should update the header, so that it doesn't disagree with m_mimeType.
    m_mimeType = mimeType;

    // FIXME: Should invalidate or update platform response if present.
}

long long ResourceResponseBase::expectedContentLength() const 
{
    lazyInit(CommonFieldsOnly);

    return m_expectedContentLength;
}

void ResourceResponseBase::setExpectedContentLength(long long expectedContentLength)
{
    lazyInit(CommonFieldsOnly);
    m_isNull = false;

    // FIXME: Content length is determined by HTTP Content-Length header. We should update the header, so that it doesn't disagree with m_expectedContentLength.
    m_expectedContentLength = expectedContentLength; 

    // FIXME: Should invalidate or update platform response if present.
}

const String& ResourceResponseBase::textEncodingName() const
{
    lazyInit(CommonFieldsOnly);

    return m_textEncodingName;
}

void ResourceResponseBase::setTextEncodingName(const String& encodingName)
{
    lazyInit(CommonFieldsOnly);
    m_isNull = false;

    // FIXME: Text encoding is determined by HTTP Content-Type header. We should update the header, so that it doesn't disagree with m_textEncodingName.
    m_textEncodingName = encodingName;

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::includeCertificateInfo() const
{
    if (m_includesCertificateInfo)
        return;
    m_certificateInfo = static_cast<const ResourceResponse*>(this)->platformCertificateInfo();
    m_includesCertificateInfo = true;
}

CertificateInfo ResourceResponseBase::certificateInfo() const
{
    return m_certificateInfo;
}

String ResourceResponseBase::suggestedFilename() const
{
    return static_cast<const ResourceResponse*>(this)->platformSuggestedFilename();
}

int ResourceResponseBase::httpStatusCode() const
{
    lazyInit(CommonFieldsOnly);

    return m_httpStatusCode;
}

void ResourceResponseBase::setHTTPStatusCode(int statusCode)
{
    lazyInit(CommonFieldsOnly);

    m_httpStatusCode = statusCode;

    // FIXME: Should invalidate or update platform response if present.
}

const String& ResourceResponseBase::httpStatusText() const 
{
    lazyInit(AllFields);

    return m_httpStatusText; 
}

void ResourceResponseBase::setHTTPStatusText(const String& statusText) 
{
    lazyInit(AllFields);

    m_httpStatusText = statusText; 

    // FIXME: Should invalidate or update platform response if present.
}

String ResourceResponseBase::httpHeaderField(const String& name) const
{
    lazyInit(CommonFieldsOnly);

    // If we already have the header, just return it instead of consuming memory by grabing all headers.
    String value = m_httpHeaderFields.get(name);
    if (!value.isEmpty())        
        return value;

    lazyInit(AllFields);

    return m_httpHeaderFields.get(name); 
}

String ResourceResponseBase::httpHeaderField(HTTPHeaderName name) const
{
    lazyInit(CommonFieldsOnly);

    // If we already have the header, just return it instead of consuming memory by grabing all headers.
    String value = m_httpHeaderFields.get(name);
    if (!value.isEmpty())
        return value;

    lazyInit(AllFields);

    return m_httpHeaderFields.get(name); 
}

void ResourceResponseBase::updateHeaderParsedState(HTTPHeaderName name)
{
    switch (name) {
    case HTTPHeaderName::Age:
        m_haveParsedAgeHeader = false;
        break;

    case HTTPHeaderName::CacheControl:
    case HTTPHeaderName::Pragma:
        m_haveParsedCacheControlHeader = false;
        break;

    case HTTPHeaderName::Date:
        m_haveParsedDateHeader = false;
        break;

    case HTTPHeaderName::Expires:
        m_haveParsedExpiresHeader = false;
        break;

    case HTTPHeaderName::LastModified:
        m_haveParsedLastModifiedHeader = false;
        break;

    default:
        break;
    }
}

void ResourceResponseBase::setHTTPHeaderField(const String& name, const String& value)
{
    lazyInit(AllFields);

    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        updateHeaderParsedState(headerName);

    m_httpHeaderFields.set(name, value);

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::setHTTPHeaderField(HTTPHeaderName name, const String& value)
{
    lazyInit(AllFields);

    updateHeaderParsedState(name);

    m_httpHeaderFields.set(name, value);

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::addHTTPHeaderField(const String& name, const String& value)
{
    lazyInit(AllFields);

    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        updateHeaderParsedState(headerName);

    m_httpHeaderFields.add(name, value);
}

const HTTPHeaderMap& ResourceResponseBase::httpHeaderFields() const
{
    lazyInit(AllFields);

    return m_httpHeaderFields;
}

void ResourceResponseBase::parseCacheControlDirectives() const
{
    ASSERT(!m_haveParsedCacheControlHeader);

    lazyInit(CommonFieldsOnly);

    m_cacheControlDirectives = WebCore::parseCacheControlDirectives(m_httpHeaderFields);
    m_haveParsedCacheControlHeader = true;
}
    
bool ResourceResponseBase::cacheControlContainsNoCache() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlDirectives.noCache;
}

bool ResourceResponseBase::cacheControlContainsNoStore() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlDirectives.noStore;
}

bool ResourceResponseBase::cacheControlContainsMustRevalidate() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlDirectives.mustRevalidate;
}

bool ResourceResponseBase::hasCacheValidatorFields() const
{
    lazyInit(CommonFieldsOnly);

    return !m_httpHeaderFields.get(HTTPHeaderName::LastModified).isEmpty() || !m_httpHeaderFields.get(HTTPHeaderName::ETag).isEmpty();
}

Optional<std::chrono::microseconds> ResourceResponseBase::cacheControlMaxAge() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlDirectives.maxAge;
}

static Optional<std::chrono::system_clock::time_point> parseDateValueInHeader(const HTTPHeaderMap& headers, HTTPHeaderName headerName)
{
    String headerValue = headers.get(headerName);
    if (headerValue.isEmpty())
        return { };
    // This handles all date formats required by RFC2616:
    // Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
    // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
    // Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
    return parseHTTPDate(headerValue);
}

Optional<std::chrono::system_clock::time_point> ResourceResponseBase::date() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedDateHeader) {
        m_date = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::Date);
        m_haveParsedDateHeader = true;
    }
    return m_date;
}

Optional<std::chrono::microseconds> ResourceResponseBase::age() const
{
    using namespace std::chrono;

    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedAgeHeader) {
        String headerValue = m_httpHeaderFields.get(HTTPHeaderName::Age);
        bool ok;
        double ageDouble = headerValue.toDouble(&ok);
        if (ok)
            m_age = duration_cast<microseconds>(duration<double>(ageDouble));
        m_haveParsedAgeHeader = true;
    }
    return m_age;
}

Optional<std::chrono::system_clock::time_point> ResourceResponseBase::expires() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedExpiresHeader) {
        m_expires = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::Expires);
        m_haveParsedExpiresHeader = true;
    }
    return m_expires;
}

Optional<std::chrono::system_clock::time_point> ResourceResponseBase::lastModified() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedLastModifiedHeader) {
        m_lastModified = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::LastModified);
#if PLATFORM(COCOA)
        // CFNetwork converts malformed dates into Epoch so we need to treat Epoch as
        // an invalid value (rdar://problem/22352838).
        const std::chrono::system_clock::time_point epoch;
        if (m_lastModified && m_lastModified.value() == epoch)
            m_lastModified = Nullopt;
#endif
        m_haveParsedLastModifiedHeader = true;
    }
    return m_lastModified;
}

bool ResourceResponseBase::isAttachment() const
{
    lazyInit(AllFields);

    String value = m_httpHeaderFields.get(HTTPHeaderName::ContentDisposition);
    size_t loc = value.find(';');
    if (loc != notFound)
        value = value.left(loc);
    value = value.stripWhiteSpace();

    return equalIgnoringCase(value, "attachment");
}
  
ResourceResponseBase::Source ResourceResponseBase::source() const
{
    lazyInit(AllFields);

    return m_source;
}

void ResourceResponseBase::setSource(Source source)
{
    m_source = source;
}

void ResourceResponseBase::lazyInit(InitLevel initLevel) const
{
    const_cast<ResourceResponse*>(static_cast<const ResourceResponse*>(this))->platformLazyInit(initLevel);
}

bool ResourceResponseBase::compare(const ResourceResponse& a, const ResourceResponse& b)
{
    if (a.isNull() != b.isNull())
        return false;  
    if (a.url() != b.url())
        return false;
    if (a.mimeType() != b.mimeType())
        return false;
    if (a.expectedContentLength() != b.expectedContentLength())
        return false;
    if (a.textEncodingName() != b.textEncodingName())
        return false;
    if (a.suggestedFilename() != b.suggestedFilename())
        return false;
    if (a.httpStatusCode() != b.httpStatusCode())
        return false;
    if (a.httpStatusText() != b.httpStatusText())
        return false;
    if (a.httpHeaderFields() != b.httpHeaderFields())
        return false;
    if (a.resourceLoadTiming() != b.resourceLoadTiming())
        return false;
    return ResourceResponse::platformCompare(a, b);
}

}
