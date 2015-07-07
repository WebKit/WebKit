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

#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "ResourceResponse.h"
#include <wtf/CurrentTime.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace WebCore {

static void parseCacheHeader(const String& header, Vector<std::pair<String, String>>& result);

inline const ResourceResponse& ResourceResponseBase::asResourceResponse() const
{
    return *static_cast<const ResourceResponse*>(this);
}

ResourceResponseBase::ResourceResponseBase()  
    : m_expectedContentLength(0)
    , m_httpStatusCode(0)
    , m_connectionID(0)
    , m_cacheControlMaxAge(0)
    , m_age(0)
    , m_date(0)
    , m_expires(0)
    , m_lastModified(0)
    , m_wasCached(false)
    , m_connectionReused(false)
    , m_isNull(true)
    , m_haveParsedCacheControlHeader(false)
    , m_haveParsedAgeHeader(false)
    , m_haveParsedDateHeader(false)
    , m_haveParsedExpiresHeader(false)
    , m_haveParsedLastModifiedHeader(false)
    , m_cacheControlContainsNoCache(false)
    , m_cacheControlContainsNoStore(false)
    , m_cacheControlContainsMustRevalidate(false)
{
}

ResourceResponseBase::ResourceResponseBase(const URL& url, const String& mimeType, long long expectedLength, const String& textEncodingName, const String& filename)
    : m_url(url)
    , m_mimeType(mimeType)
    , m_expectedContentLength(expectedLength)
    , m_textEncodingName(textEncodingName)
    , m_suggestedFilename(filename)
    , m_httpStatusCode(0)
    , m_connectionID(0)
    , m_cacheControlMaxAge(0)
    , m_age(0)
    , m_date(0)
    , m_expires(0)
    , m_lastModified(0)
    , m_wasCached(false)
    , m_connectionReused(false)
    , m_isNull(false)
    , m_haveParsedCacheControlHeader(false)
    , m_haveParsedAgeHeader(false)
    , m_haveParsedDateHeader(false)
    , m_haveParsedExpiresHeader(false)
    , m_haveParsedLastModifiedHeader(false)
    , m_cacheControlContainsNoCache(false)
    , m_cacheControlContainsNoStore(false)
    , m_cacheControlContainsMustRevalidate(false)
{
}

PassOwnPtr<ResourceResponse> ResourceResponseBase::adopt(PassOwnPtr<CrossThreadResourceResponseData> data)
{
    OwnPtr<ResourceResponse> response = adoptPtr(new ResourceResponse);
    response->setURL(data->m_url);
    response->setMimeType(data->m_mimeType);
    response->setExpectedContentLength(data->m_expectedContentLength);
    response->setTextEncodingName(data->m_textEncodingName);
    response->setSuggestedFilename(data->m_suggestedFilename);

    response->setHTTPStatusCode(data->m_httpStatusCode);
    response->setHTTPStatusText(data->m_httpStatusText);

    response->lazyInit(CommonAndUncommonFields);
    response->m_httpHeaderFields.adopt(WTF::move(data->m_httpHeaders));
    response->m_resourceLoadTiming = data->m_resourceLoadTiming;
    response->doPlatformAdopt(data);
    return response.release();
}

PassOwnPtr<CrossThreadResourceResponseData> ResourceResponseBase::copyData() const
{
    OwnPtr<CrossThreadResourceResponseData> data = adoptPtr(new CrossThreadResourceResponseData);
    data->m_url = url().copy();
    data->m_mimeType = mimeType().isolatedCopy();
    data->m_expectedContentLength = expectedContentLength();
    data->m_textEncodingName = textEncodingName().isolatedCopy();
    data->m_suggestedFilename = suggestedFilename().isolatedCopy();
    data->m_httpStatusCode = httpStatusCode();
    data->m_httpStatusText = httpStatusText().isolatedCopy();
    data->m_httpHeaders = httpHeaderFields().copyData();
    data->m_resourceLoadTiming = m_resourceLoadTiming;
    return asResourceResponse().doPlatformCopyData(data.release());
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

// FIXME should compute this on the fly
const String& ResourceResponseBase::suggestedFilename() const
{
    lazyInit(AllFields);

    return m_suggestedFilename;
}

void ResourceResponseBase::setSuggestedFilename(const String& suggestedName)
{
    lazyInit(AllFields);
    m_isNull = false;

    // FIXME: Suggested file name is calculated based on other headers. There should not be a setter for it.
    m_suggestedFilename = suggestedName; 

    // FIXME: Should invalidate or update platform response if present.
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
    lazyInit(CommonAndUncommonFields);

    return m_httpStatusText; 
}

void ResourceResponseBase::setHTTPStatusText(const String& statusText) 
{
    lazyInit(CommonAndUncommonFields);

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

    lazyInit(CommonAndUncommonFields);

    return m_httpHeaderFields.get(name); 
}

String ResourceResponseBase::httpHeaderField(HTTPHeaderName name) const
{
    lazyInit(CommonFieldsOnly);

    // If we already have the header, just return it instead of consuming memory by grabing all headers.
    String value = m_httpHeaderFields.get(name);
    if (!value.isEmpty())
        return value;

    lazyInit(CommonAndUncommonFields);

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
    lazyInit(CommonAndUncommonFields);

    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        updateHeaderParsedState(headerName);

    m_httpHeaderFields.set(name, value);

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::setHTTPHeaderField(HTTPHeaderName name, const String& value)
{
    lazyInit(CommonAndUncommonFields);

    updateHeaderParsedState(name);

    m_httpHeaderFields.set(name, value);

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::addHTTPHeaderField(const String& name, const String& value)
{
    lazyInit(CommonAndUncommonFields);

    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        updateHeaderParsedState(headerName);

    m_httpHeaderFields.add(name, value);
}

const HTTPHeaderMap& ResourceResponseBase::httpHeaderFields() const
{
    lazyInit(CommonAndUncommonFields);

    return m_httpHeaderFields;
}

void ResourceResponseBase::parseCacheControlDirectives() const
{
    ASSERT(!m_haveParsedCacheControlHeader);

    lazyInit(CommonFieldsOnly);

    m_haveParsedCacheControlHeader = true;

    m_cacheControlContainsMustRevalidate = false;
    m_cacheControlContainsNoCache = false;
    m_cacheControlMaxAge = std::numeric_limits<double>::quiet_NaN();

    String cacheControlValue = m_httpHeaderFields.get(HTTPHeaderName::CacheControl);
    if (!cacheControlValue.isEmpty()) {
        Vector<std::pair<String, String>> directives;
        parseCacheHeader(cacheControlValue, directives);

        size_t directivesSize = directives.size();
        for (size_t i = 0; i < directivesSize; ++i) {
            // RFC2616 14.9.1: A no-cache directive with a value is only meaningful for proxy caches.
            // It should be ignored by a browser level cache.
            if (equalIgnoringCase(directives[i].first, "no-cache") && directives[i].second.isEmpty())
                m_cacheControlContainsNoCache = true;
            else if (equalIgnoringCase(directives[i].first, "no-store"))
                m_cacheControlContainsNoStore = true;
            else if (equalIgnoringCase(directives[i].first, "must-revalidate"))
                m_cacheControlContainsMustRevalidate = true;
            else if (equalIgnoringCase(directives[i].first, "max-age")) {
                if (!std::isnan(m_cacheControlMaxAge)) {
                    // First max-age directive wins if there are multiple ones.
                    continue;
                }
                bool ok;
                double maxAge = directives[i].second.toDouble(&ok);
                if (ok)
                    m_cacheControlMaxAge = maxAge;
            }
        }
    }

    if (!m_cacheControlContainsNoCache) {
        // Handle Pragma: no-cache
        // This is deprecated and equivalent to Cache-control: no-cache
        // Don't bother tokenizing the value, it is not important
        String pragmaValue = m_httpHeaderFields.get(HTTPHeaderName::Pragma);

        m_cacheControlContainsNoCache = pragmaValue.contains("no-cache", false);
    }
}
    
bool ResourceResponseBase::cacheControlContainsNoCache() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlContainsNoCache;
}

bool ResourceResponseBase::cacheControlContainsNoStore() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlContainsNoStore;
}

bool ResourceResponseBase::cacheControlContainsMustRevalidate() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlContainsMustRevalidate;
}

bool ResourceResponseBase::hasCacheValidatorFields() const
{
    lazyInit(CommonFieldsOnly);

    return !m_httpHeaderFields.get(HTTPHeaderName::LastModified).isEmpty() || !m_httpHeaderFields.get(HTTPHeaderName::ETag).isEmpty();
}

double ResourceResponseBase::cacheControlMaxAge() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlMaxAge;
}

static double parseDateValueInHeader(const HTTPHeaderMap& headers, HTTPHeaderName headerName)
{
    String headerValue = headers.get(headerName);
    if (headerValue.isEmpty())
        return std::numeric_limits<double>::quiet_NaN(); 
    // This handles all date formats required by RFC2616:
    // Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
    // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
    // Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
    double dateInMilliseconds = parseDate(headerValue);
    if (!std::isfinite(dateInMilliseconds))
        return std::numeric_limits<double>::quiet_NaN();
    return dateInMilliseconds / 1000;
}

double ResourceResponseBase::date() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedDateHeader) {
        m_date = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::Date);
        m_haveParsedDateHeader = true;
    }
    return m_date;
}

double ResourceResponseBase::age() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedAgeHeader) {
        String headerValue = m_httpHeaderFields.get(HTTPHeaderName::Age);
        bool ok;
        m_age = headerValue.toDouble(&ok);
        if (!ok)
            m_age = std::numeric_limits<double>::quiet_NaN();
        m_haveParsedAgeHeader = true;
    }
    return m_age;
}

double ResourceResponseBase::expires() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedExpiresHeader) {
        m_expires = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::Expires);
        m_haveParsedExpiresHeader = true;
    }
    return m_expires;
}

double ResourceResponseBase::lastModified() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedLastModifiedHeader) {
        m_lastModified = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::LastModified);
        m_haveParsedLastModifiedHeader = true;
    }
    return m_lastModified;
}

bool ResourceResponseBase::isAttachment() const
{
    lazyInit(CommonAndUncommonFields);

    String value = m_httpHeaderFields.get(HTTPHeaderName::ContentDisposition);
    size_t loc = value.find(';');
    if (loc != notFound)
        value = value.left(loc);
    value = value.stripWhiteSpace();

    return equalIgnoringCase(value, "attachment");
}
  
bool ResourceResponseBase::wasCached() const
{
    lazyInit(CommonAndUncommonFields);

    return m_wasCached;
}

void ResourceResponseBase::setWasCached(bool value)
{
    m_wasCached = value;
}

bool ResourceResponseBase::connectionReused() const
{
    lazyInit(CommonAndUncommonFields);

    return m_connectionReused;
}

void ResourceResponseBase::setConnectionReused(bool connectionReused)
{
    lazyInit(CommonAndUncommonFields);

    m_connectionReused = connectionReused;
}

unsigned ResourceResponseBase::connectionID() const
{
    lazyInit(CommonAndUncommonFields);

    return m_connectionID;
}

void ResourceResponseBase::setConnectionID(unsigned connectionID)
{
    lazyInit(CommonAndUncommonFields);

    m_connectionID = connectionID;
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

static bool isCacheHeaderSeparator(UChar c)
{
    // See RFC 2616, Section 2.2
    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
    }
}

static bool isControlCharacter(UChar c)
{
    return c < ' ' || c == 127;
}

static inline String trimToNextSeparator(const String& str)
{
    return str.substring(0, str.find(isCacheHeaderSeparator));
}

static void parseCacheHeader(const String& header, Vector<std::pair<String, String>>& result)
{
    const String safeHeader = header.removeCharacters(isControlCharacter);
    unsigned max = safeHeader.length();
    for (unsigned pos = 0; pos < max; /* pos incremented in loop */) {
        size_t nextCommaPosition = safeHeader.find(',', pos);
        size_t nextEqualSignPosition = safeHeader.find('=', pos);
        if (nextEqualSignPosition != notFound && (nextEqualSignPosition < nextCommaPosition || nextCommaPosition == notFound)) {
            // Get directive name, parse right hand side of equal sign, then add to map
            String directive = trimToNextSeparator(safeHeader.substring(pos, nextEqualSignPosition - pos).stripWhiteSpace());
            pos += nextEqualSignPosition - pos + 1;

            String value = safeHeader.substring(pos, max - pos).stripWhiteSpace();
            if (value[0] == '"') {
                // The value is a quoted string
                size_t nextDoubleQuotePosition = value.find('"', 1);
                if (nextDoubleQuotePosition != notFound) {
                    // Store the value as a quoted string without quotes
                    result.append(std::pair<String, String>(directive, value.substring(1, nextDoubleQuotePosition - 1).stripWhiteSpace()));
                    pos += (safeHeader.find('"', pos) - pos) + nextDoubleQuotePosition + 1;
                    // Move past next comma, if there is one
                    size_t nextCommaPosition2 = safeHeader.find(',', pos);
                    if (nextCommaPosition2 != notFound)
                        pos += nextCommaPosition2 - pos + 1;
                    else
                        return; // Parse error if there is anything left with no comma
                } else {
                    // Parse error; just use the rest as the value
                    result.append(std::pair<String, String>(directive, trimToNextSeparator(value.substring(1, value.length() - 1).stripWhiteSpace())));
                    return;
                }
            } else {
                // The value is a token until the next comma
                size_t nextCommaPosition2 = value.find(',');
                if (nextCommaPosition2 != notFound) {
                    // The value is delimited by the next comma
                    result.append(std::pair<String, String>(directive, trimToNextSeparator(value.substring(0, nextCommaPosition2).stripWhiteSpace())));
                    pos += (safeHeader.find(',', pos) - pos) + 1;
                } else {
                    // The rest is the value; no change to value needed
                    result.append(std::pair<String, String>(directive, trimToNextSeparator(value)));
                    return;
                }
            }
        } else if (nextCommaPosition != notFound && (nextCommaPosition < nextEqualSignPosition || nextEqualSignPosition == notFound)) {
            // Add directive to map with empty string as value
            result.append(std::pair<String, String>(trimToNextSeparator(safeHeader.substring(pos, nextCommaPosition - pos).stripWhiteSpace()), ""));
            pos += nextCommaPosition - pos + 1;
        } else {
            // Add last directive to map with empty string as value
            result.append(std::pair<String, String>(trimToNextSeparator(safeHeader.substring(pos, max - pos).stripWhiteSpace()), ""));
            return;
        }
    }
}

}
