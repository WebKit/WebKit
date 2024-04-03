/*
 * Copyright (C) 2006, 2008, 2016 Apple Inc. All rights reserved.
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
#include "DataURLDecoder.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "ParsedContentRange.h"
#include "ResourceResponse.h"
#include "WebCorePersistentCoders.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/StringView.h>

namespace WebCore {

bool isScriptAllowedByNosniff(const ResourceResponse& response)
{
    if (parseContentTypeOptionsHeader(response.httpHeaderField(HTTPHeaderName::XContentTypeOptions)) != ContentTypeOptionsDisposition::Nosniff)
        return true;
    String mimeType = extractMIMETypeFromMediaType(response.httpHeaderField(HTTPHeaderName::ContentType));
    return MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType);
}

ResourceResponseBase::ResourceResponseBase()
{
}

ResourceResponseBase::ResourceResponseBase(const URL& url, const String& mimeType, long long expectedLength, const String& textEncodingName)
    : m_url(url)
    , m_mimeType(mimeType)
    , m_expectedContentLength(expectedLength)
    , m_textEncodingName(textEncodingName)
    , m_certificateInfo(CertificateInfo()) // Empty but valid for synthetic responses.
    , m_isNull(false)
{
}

ResourceResponseBase::ResourceResponseBase(std::optional<ResourceResponseData> data)
    : m_url(data ? data->url : URL { })
    , m_mimeType(data ? data->mimeType : AtomString { })
    , m_expectedContentLength(data ? data->expectedContentLength : 0)
    , m_textEncodingName(data ? data->textEncodingName : String { })
    , m_httpStatusText(data ? data->httpStatusText : String { })
    , m_httpVersion(data ? data->httpVersion : String { })
    , m_httpHeaderFields(data ? data->httpHeaderFields : HTTPHeaderMap { })
    , m_networkLoadMetrics(data && data->networkLoadMetrics ? Box<NetworkLoadMetrics>::create(*data->networkLoadMetrics) : Box<NetworkLoadMetrics> { })
    , m_certificateInfo(data ? data->certificateInfo : std::nullopt)
    , m_httpStatusCode(data ? data->httpStatusCode : 0)
    , m_isNull(!data)
    , m_usedLegacyTLS(data ? data->usedLegacyTLS : UsedLegacyTLS::No)
    , m_wasPrivateRelayed(data ? data->wasPrivateRelayed : WasPrivateRelayed::No)
    , m_isRedirected(data ? data->isRedirected : false)
    , m_isRangeRequested(data ? data->isRangeRequested : false)
    , m_tainting(data ? data->tainting : Tainting::Basic)
    , m_source(data ? data->source : Source::Unknown)
    , m_type(data ? data->type : Type::Default)
{
}

ResourceResponseData ResourceResponseData::isolatedCopy() const
{
    ResourceResponseData result;
    result.url = url.isolatedCopy();
    result.mimeType = mimeType.isolatedCopy();
    result.expectedContentLength = expectedContentLength;
    result.textEncodingName = textEncodingName.isolatedCopy();
    result.httpStatusCode = httpStatusCode;
    result.httpStatusText = httpStatusText.isolatedCopy();
    result.httpVersion = httpVersion.isolatedCopy();
    result.httpHeaderFields = httpHeaderFields.isolatedCopy();
    if (networkLoadMetrics)
        result.networkLoadMetrics = networkLoadMetrics->isolatedCopy();
    result.source = source;
    result.type = type;
    result.tainting = tainting;
    result.isRedirected = isRedirected;
    result.usedLegacyTLS = usedLegacyTLS;
    result.wasPrivateRelayed = wasPrivateRelayed;
    result.isRangeRequested = isRangeRequested;
    if (certificateInfo)
        result.certificateInfo = certificateInfo->isolatedCopy();
    return result;
}

ResourceResponseData ResourceResponseBase::crossThreadData() const
{
    CrossThreadData data;
    data.url = url().isolatedCopy();
    data.mimeType = mimeType().isolatedCopy();
    data.expectedContentLength = expectedContentLength();
    data.textEncodingName = textEncodingName().isolatedCopy();
    data.httpStatusCode = httpStatusCode();
    data.httpStatusText = httpStatusText().isolatedCopy();
    data.httpVersion = httpVersion().isolatedCopy();

    data.httpHeaderFields = httpHeaderFields().isolatedCopy();
    if (m_networkLoadMetrics)
        data.networkLoadMetrics = m_networkLoadMetrics->isolatedCopy();
    data.source = m_source;
    data.type = m_type;
    data.tainting = m_tainting;
    data.isRedirected = m_isRedirected;
    data.usedLegacyTLS = m_usedLegacyTLS;
    data.wasPrivateRelayed = m_wasPrivateRelayed;
    data.isRangeRequested = m_isRangeRequested;
    if (m_certificateInfo)
        data.certificateInfo = m_certificateInfo->isolatedCopy();

    return data;
}

ResourceResponse ResourceResponseBase::fromCrossThreadData(CrossThreadData&& data)
{
    ResourceResponse response;

    response.setURL(data.url);
    response.setMimeType(WTFMove(data.mimeType));
    response.setExpectedContentLength(data.expectedContentLength);
    response.setTextEncodingName(WTFMove(data.textEncodingName));

    response.setHTTPStatusCode(data.httpStatusCode);
    response.setHTTPStatusText(WTFMove(data.httpStatusText));
    response.setHTTPVersion(WTFMove(data.httpVersion));

    response.m_httpHeaderFields = WTFMove(data.httpHeaderFields);
    if (data.networkLoadMetrics)
        response.m_networkLoadMetrics = Box<NetworkLoadMetrics>::create(WTFMove(data.networkLoadMetrics.value()));
    else
        response.m_networkLoadMetrics = nullptr;
    response.m_source = data.source;
    response.m_type = data.type;
    response.m_tainting = data.tainting;
    response.m_isRedirected = data.isRedirected;
    response.m_usedLegacyTLS =  data.usedLegacyTLS;
    response.m_wasPrivateRelayed = data.wasPrivateRelayed;
    response.m_isRangeRequested = data.isRangeRequested;
    response.m_certificateInfo = WTFMove(data.certificateInfo);

    return response;
}

ResourceResponse ResourceResponseBase::syntheticRedirectResponse(const URL& fromURL, const URL& toURL)
{
    ResourceResponse redirectResponse;
    redirectResponse.setURL(fromURL);
    redirectResponse.setHTTPStatusCode(302);
    redirectResponse.setHTTPVersion("HTTP/1.1"_s);
    redirectResponse.setHTTPHeaderField(HTTPHeaderName::Location, toURL.string());
    redirectResponse.setHTTPHeaderField(HTTPHeaderName::CacheControl, "no-store"_s);

    return redirectResponse;
}

ResourceResponse ResourceResponseBase::dataURLResponse(const URL& url, const DataURLDecoder::Result& result)
{
    ResourceResponse dataResponse { url, result.mimeType, static_cast<long long>(result.data.size()), result.charset };
    dataResponse.setHTTPStatusCode(200);
    dataResponse.setHTTPStatusText("OK"_s);
    dataResponse.setHTTPHeaderField(HTTPHeaderName::ContentType, result.contentType);
    dataResponse.setSource(ResourceResponse::Source::Network);

    return dataResponse;
}

ResourceResponse ResourceResponseBase::filter(const ResourceResponse& response, PerformExposeAllHeadersCheck performCheck)
{
    if (response.tainting() == Tainting::Opaque) {
        ResourceResponse opaqueResponse;
        opaqueResponse.setTainting(Tainting::Opaque);
        opaqueResponse.setType(Type::Opaque);
        return opaqueResponse;
    }

    if (response.tainting() == Tainting::Opaqueredirect) {
        ResourceResponse opaqueResponse;
        opaqueResponse.setTainting(Tainting::Opaqueredirect);
        opaqueResponse.setType(Type::Opaqueredirect);
        opaqueResponse.setURL(response.url());
        return opaqueResponse;
    }

    ResourceResponse filteredResponse = response;
    // Let's initialize filteredResponse to remove some header fields.
    filteredResponse.lazyInit(AllFields);

    filteredResponse.m_httpHeaderFields.remove(HTTPHeaderName::SetCookie);
    filteredResponse.m_httpHeaderFields.remove(HTTPHeaderName::SetCookie2);

    if (response.tainting() == Tainting::Basic) {
        filteredResponse.setType(Type::Basic);
        return filteredResponse;
    }

    ASSERT(response.tainting() == Tainting::Cors);
    filteredResponse.setType(Type::Cors);

    auto accessControlExposeHeaderSet = valueOrDefault(parseAccessControlAllowList<ASCIICaseInsensitiveHash>(response.httpHeaderField(HTTPHeaderName::AccessControlExposeHeaders)));
    if (performCheck == PerformExposeAllHeadersCheck::Yes && accessControlExposeHeaderSet.contains<HashTranslatorASCIILiteral>("*"_s))
        return filteredResponse;

    filteredResponse.m_httpHeaderFields.uncommonHeaders().removeAllMatching([&](auto& entry) {
        return !isCrossOriginSafeHeader(entry.key, accessControlExposeHeaderSet);
    });
    filteredResponse.m_httpHeaderFields.commonHeaders().removeAllMatching([&](auto& entry) {
        return !isCrossOriginSafeHeader(entry.key, accessControlExposeHeaderSet);
    });

    return filteredResponse;
}

bool ResourceResponseBase::isInHTTPFamily() const
{
    lazyInit(CommonFieldsOnly);

    return m_url.protocolIsInHTTPFamily();
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

void ResourceResponseBase::setMimeType(String&& mimeType)
{
    lazyInit(CommonFieldsOnly);
    m_isNull = false;

    // FIXME: MIME type is determined by HTTP Content-Type header. We should update the header, so that it doesn't disagree with m_mimeType.
    m_mimeType = WTFMove(mimeType);

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

void ResourceResponseBase::setTextEncodingName(String&& encodingName)
{
    lazyInit(CommonFieldsOnly);
    m_isNull = false;

    // FIXME: Text encoding is determined by HTTP Content-Type header. We should update the header, so that it doesn't disagree with m_textEncodingName.
    m_textEncodingName = WTFMove(encodingName);

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::setType(Type type)
{
    m_isNull = false;
    m_type = type;
}

void ResourceResponseBase::includeCertificateInfo(std::span<const std::byte> auditToken) const
{
    if (m_certificateInfo)
        return;
    m_certificateInfo = static_cast<const ResourceResponse*>(this)->platformCertificateInfo(auditToken);
}

String ResourceResponseBase::suggestedFilename() const
{
    return static_cast<const ResourceResponse*>(this)->platformSuggestedFilename();
}

String ResourceResponseBase::sanitizeSuggestedFilename(const String& suggestedFilename)
{
    if (suggestedFilename.isEmpty())
        return suggestedFilename;

    ResourceResponse response { { { }, "http://example.com/"_s }, { }, -1, { } };
    response.setHTTPStatusCode(200);
    String escapedSuggestedFilename = makeStringByReplacingAll(suggestedFilename, '\\', "\\\\"_s);
    escapedSuggestedFilename = makeStringByReplacingAll(escapedSuggestedFilename, '"', "\\\""_s);
    response.setHTTPHeaderField(HTTPHeaderName::ContentDisposition, makeString("attachment; filename=\"", escapedSuggestedFilename, '"'));
    return response.suggestedFilename();
}

bool ResourceResponseBase::isSuccessful() const
{
    int code = httpStatusCode();
    return code >= 200 && code < 300;
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
    m_isNull = false;

    // FIXME: Should invalidate or update platform response if present.
}

bool ResourceResponseBase::isRedirection() const
{
    return isRedirectionStatusCode(m_httpStatusCode);
}

const String& ResourceResponseBase::httpStatusText() const
{
    lazyInit(AllFields);

    return m_httpStatusText; 
}

void ResourceResponseBase::setHTTPStatusText(String&& statusText)
{
    lazyInit(AllFields);

    m_httpStatusText = WTFMove(statusText);

    // FIXME: Should invalidate or update platform response if present.
}

const String& ResourceResponseBase::httpVersion() const
{
    lazyInit(AllFields);
    
    return m_httpVersion;
}

void ResourceResponseBase::setHTTPVersion(String&& versionText)
{
    lazyInit(AllFields);
    
    m_httpVersion = versionText;
    
    // FIXME: Should invalidate or update platform response if present.
}

static bool isSafeRedirectionResponseHeader(HTTPHeaderName name)
{
    // WebCore needs to keep location and cache related headers as it does caching.
    // We also keep CORS/ReferrerPolicy headers until CORS checks/Referrer computation are done in NetworkProcess.
    return name == HTTPHeaderName::Location
        || name == HTTPHeaderName::ReferrerPolicy
        || name == HTTPHeaderName::CacheControl
        || name == HTTPHeaderName::Date
        || name == HTTPHeaderName::Expires
        || name == HTTPHeaderName::ETag
        || name == HTTPHeaderName::LastModified
        || name == HTTPHeaderName::Age
        || name == HTTPHeaderName::Pragma
        || name == HTTPHeaderName::ReferrerPolicy
        || name == HTTPHeaderName::Refresh
        || name == HTTPHeaderName::Vary
        || name == HTTPHeaderName::CrossOriginOpenerPolicy
        || name == HTTPHeaderName::CrossOriginOpenerPolicyReportOnly
        || name == HTTPHeaderName::CrossOriginEmbedderPolicy
        || name == HTTPHeaderName::CrossOriginEmbedderPolicyReportOnly
        || name == HTTPHeaderName::AccessControlAllowCredentials
        || name == HTTPHeaderName::AccessControlAllowHeaders
        || name == HTTPHeaderName::AccessControlAllowMethods
        || name == HTTPHeaderName::AccessControlAllowOrigin
        || name == HTTPHeaderName::AccessControlExposeHeaders
        || name == HTTPHeaderName::AccessControlMaxAge
        || name == HTTPHeaderName::CrossOriginResourcePolicy
        || name == HTTPHeaderName::TimingAllowOrigin;
}

static bool isSafeCrossOriginResponseHeader(HTTPHeaderName name)
{
    // All known response headers used in WebProcesses.
    return name == HTTPHeaderName::AcceptRanges
        || name == HTTPHeaderName::AccessControlAllowCredentials
        || name == HTTPHeaderName::AccessControlAllowHeaders
        || name == HTTPHeaderName::AccessControlAllowMethods
        || name == HTTPHeaderName::AccessControlAllowOrigin
        || name == HTTPHeaderName::AccessControlExposeHeaders
        || name == HTTPHeaderName::AccessControlMaxAge
        || name == HTTPHeaderName::AccessControlRequestHeaders
        || name == HTTPHeaderName::AccessControlRequestMethod
        || name == HTTPHeaderName::Age
        || name == HTTPHeaderName::CacheControl
        || name == HTTPHeaderName::ContentDisposition
        || name == HTTPHeaderName::ContentEncoding
        || name == HTTPHeaderName::ContentLanguage
        || name == HTTPHeaderName::ContentLength
        || name == HTTPHeaderName::ContentRange
        || name == HTTPHeaderName::ContentSecurityPolicy
        || name == HTTPHeaderName::ContentSecurityPolicyReportOnly
        || name == HTTPHeaderName::ContentType
        || name == HTTPHeaderName::CrossOriginResourcePolicy
        || name == HTTPHeaderName::Date
        || name == HTTPHeaderName::ETag
        || name == HTTPHeaderName::Expires
        || name == HTTPHeaderName::IcyMetaInt
        || name == HTTPHeaderName::IcyMetadata
        || name == HTTPHeaderName::LastEventID
        || name == HTTPHeaderName::LastModified
        || name == HTTPHeaderName::Link
        || name == HTTPHeaderName::Location
        || name == HTTPHeaderName::Pragma
        || name == HTTPHeaderName::Range
        || name == HTTPHeaderName::ReferrerPolicy
        || name == HTTPHeaderName::Refresh
        || name == HTTPHeaderName::ServerTiming
        || name == HTTPHeaderName::SourceMap
        || name == HTTPHeaderName::XSourceMap
        || name == HTTPHeaderName::TimingAllowOrigin
        || name == HTTPHeaderName::Trailer
        || name == HTTPHeaderName::Vary
        || name == HTTPHeaderName::XContentTypeOptions
        || name == HTTPHeaderName::XDNSPrefetchControl
        || name == HTTPHeaderName::XFrameOptions
        || name == HTTPHeaderName::XXSSProtection;
}

void ResourceResponseBase::sanitizeHTTPHeaderFieldsAccordingToTainting()
{
    switch (m_tainting) {
    case ResourceResponse::Tainting::Basic:
        break;
    case ResourceResponse::Tainting::Cors: {
        auto corsSafeHeaderSet = valueOrDefault(parseAccessControlAllowList<ASCIICaseInsensitiveHash>(httpHeaderField(HTTPHeaderName::AccessControlExposeHeaders)));
        if (corsSafeHeaderSet.contains<HashTranslatorASCIILiteral>("*"_s))
            return;

        m_httpHeaderFields.commonHeaders().removeAllMatching([&corsSafeHeaderSet](auto& header) {
            return !isSafeCrossOriginResponseHeader(header.key) && !corsSafeHeaderSet.contains<HashTranslatorASCIILiteralCaseInsensitive>(httpHeaderNameString(header.key));
        });
        m_httpHeaderFields.uncommonHeaders().removeAllMatching([&corsSafeHeaderSet](auto& header) { return !corsSafeHeaderSet.contains(header.key); });
        break;
    }
    case ResourceResponse::Tainting::Opaque:
    case ResourceResponse::Tainting::Opaqueredirect:
        m_httpHeaderFields.commonHeaders().removeAllMatching([](auto& header) { return !isSafeCrossOriginResponseHeader(header.key); });
        m_httpHeaderFields.uncommonHeaders().clear();
        break;
    }
}

void ResourceResponseBase::sanitizeHTTPHeaderFields(SanitizationType type)
{
    lazyInit(AllFields);

    m_httpHeaderFields.remove(HTTPHeaderName::SetCookie);
    m_httpHeaderFields.remove(HTTPHeaderName::SetCookie2);

    switch (type) {
    case SanitizationType::RemoveCookies:
        return;
    case SanitizationType::Redirection: {
        m_httpHeaderFields.commonHeaders().removeAllMatching([](auto& header) { return !isSafeRedirectionResponseHeader(header.key); });
        m_httpHeaderFields.uncommonHeaders().clear();
        return;
    }
    case SanitizationType::CrossOriginSafe:
        sanitizeHTTPHeaderFieldsAccordingToTainting();
    }
}

bool ResourceResponseBase::isHTTP09() const
{
    lazyInit(AllFields);

    return m_httpVersion.startsWith("HTTP/0.9"_s);
}

String ResourceResponseBase::httpHeaderField(StringView name) const
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

    case HTTPHeaderName::ContentRange:
        m_haveParsedContentRangeHeader = false;
        break;

    default:
        break;
    }
}

void ResourceResponseBase::setHTTPHeaderField(const String& name, const String& value)
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        setHTTPHeaderField(headerName, value);
    else
        setUncommonHTTPHeaderField(name, value);
}

void ResourceResponseBase::setUncommonHTTPHeaderField(const String& name, const String& value)
{
    lazyInit(AllFields);

    m_httpHeaderFields.setUncommonHeader(name, value);

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::setHTTPHeaderFields(HTTPHeaderMap&& headerFields)
{
    lazyInit(AllFields);

    m_httpHeaderFields = WTFMove(headerFields);
}

void ResourceResponseBase::setHTTPHeaderField(HTTPHeaderName name, const String& value)
{
    lazyInit(AllFields);

    updateHeaderParsedState(name);

    m_httpHeaderFields.set(name, value);

    // FIXME: Should invalidate or update platform response if present.
}

void ResourceResponseBase::addHTTPHeaderField(HTTPHeaderName name, const String& value)
{
    lazyInit(AllFields);
    updateHeaderParsedState(name);
    m_httpHeaderFields.add(name, value);
}

void ResourceResponseBase::addHTTPHeaderField(const String& name, const String& value)
{
    HTTPHeaderName headerName;
    if (findHTTPHeaderName(name, headerName))
        addHTTPHeaderField(headerName, value);
    else
        addUncommonHTTPHeaderField(name, value);
}

void ResourceResponseBase::addUncommonHTTPHeaderField(const String& name, const String& value)
{
    lazyInit(AllFields);
    m_httpHeaderFields.addUncommonHeader(name, value);
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
    
bool ResourceResponseBase::cacheControlContainsImmutable() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlDirectives.immutable;
}

bool ResourceResponseBase::hasCacheValidatorFields() const
{
    lazyInit(CommonFieldsOnly);

    return !m_httpHeaderFields.get(HTTPHeaderName::LastModified).isEmpty() || !m_httpHeaderFields.get(HTTPHeaderName::ETag).isEmpty();
}

std::optional<Seconds> ResourceResponseBase::cacheControlMaxAge() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlDirectives.maxAge;
}

std::optional<Seconds> ResourceResponseBase::cacheControlStaleWhileRevalidate() const
{
    if (!m_haveParsedCacheControlHeader)
        parseCacheControlDirectives();
    return m_cacheControlDirectives.staleWhileRevalidate;
}

static std::optional<WallTime> parseDateValueInHeader(const HTTPHeaderMap& headers, HTTPHeaderName headerName)
{
    String headerValue = headers.get(headerName);
    if (headerValue.isEmpty())
        return std::nullopt;
    // This handles all date formats required by RFC2616:
    // Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
    // Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
    // Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
    return parseHTTPDate(headerValue);
}

std::optional<WallTime> ResourceResponseBase::date() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedDateHeader) {
        m_date = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::Date);
        m_haveParsedDateHeader = true;
    }
    return m_date;
}

std::optional<Seconds> ResourceResponseBase::age() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedAgeHeader) {
        String headerValue = m_httpHeaderFields.get(HTTPHeaderName::Age);
        bool ok;
        double ageDouble = headerValue.toDouble(&ok);
        if (ok)
            m_age = Seconds { ageDouble };
        m_haveParsedAgeHeader = true;
    }
    return m_age;
}

std::optional<WallTime> ResourceResponseBase::expires() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedExpiresHeader) {
        m_expires = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::Expires);
        m_haveParsedExpiresHeader = true;
    }
    return m_expires;
}

std::optional<WallTime> ResourceResponseBase::lastModified() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedLastModifiedHeader) {
        m_lastModified = parseDateValueInHeader(m_httpHeaderFields, HTTPHeaderName::LastModified);
#if PLATFORM(COCOA)
        // CFNetwork converts malformed dates into Epoch so we need to treat Epoch as
        // an invalid value (rdar://problem/22352838).
        const WallTime epoch = WallTime::fromRawSeconds(0);
        if (m_lastModified && m_lastModified.value() == epoch)
            m_lastModified = std::nullopt;
#endif
        m_haveParsedLastModifiedHeader = true;
    }
    return m_lastModified;
}

static ParsedContentRange parseContentRangeInHeader(const HTTPHeaderMap& headers)
{
    String contentRangeValue = headers.get(HTTPHeaderName::ContentRange);
    if (contentRangeValue.isEmpty())
        return ParsedContentRange();

    return ParsedContentRange(contentRangeValue);
}

const ParsedContentRange& ResourceResponseBase::contentRange() const
{
    lazyInit(CommonFieldsOnly);

    if (!m_haveParsedContentRangeHeader) {
        m_contentRange = parseContentRangeInHeader(m_httpHeaderFields);
        m_haveParsedContentRangeHeader = true;
    }

    return m_contentRange;
}

bool ResourceResponseBase::isAttachment() const
{
    lazyInit(AllFields);

    auto value = m_httpHeaderFields.get(HTTPHeaderName::ContentDisposition);
    return equalLettersIgnoringASCIICase(StringView(value).left(value.find(';')).trim(isUnicodeCompatibleASCIIWhitespace<UChar>), "attachment"_s);
}

bool ResourceResponseBase::isAttachmentWithFilename() const
{
    lazyInit(AllFields);

    String contentDisposition = m_httpHeaderFields.get(HTTPHeaderName::ContentDisposition);
    if (contentDisposition.isNull())
        return false;

    StringView contentDispositionView { contentDisposition };
    if (!equalLettersIgnoringASCIICase(contentDispositionView.left(contentDispositionView.find(';')).trim(isUnicodeCompatibleASCIIWhitespace<UChar>), "attachment"_s))
        return false;

    return !filenameFromHTTPContentDisposition(contentDispositionView).isNull();
}

ResourceResponseBase::Source ResourceResponseBase::source() const
{
    lazyInit(AllFields);

    return m_source;
}

void ResourceResponseBase::lazyInit(InitLevel initLevel) const
{
    const_cast<ResourceResponse*>(static_cast<const ResourceResponse*>(this))->platformLazyInit(initLevel);
}

bool ResourceResponseBase::equalForWebKitLegacyChallengeComparison(const ResourceResponse& a, const ResourceResponse& b)
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
    return ResourceResponse::platformCompare(a, b);
}

bool ResourceResponseBase::containsInvalidHTTPHeaders() const
{
    for (auto& header : httpHeaderFields()) {
        if (!isValidHTTPHeaderValue(header.value.trim(isASCIIWhitespaceWithoutFF<UChar>)))
            return true;
    }
    return false;
}

std::optional<ResourceResponseData> ResourceResponseBase::getResponseData() const
{
    if (m_isNull)
        return std::nullopt;
    lazyInit(AllFields);
    
    return { ResourceResponseData {
        URL { m_url },
        String { m_mimeType },
        m_expectedContentLength,
        String { m_textEncodingName },
        m_httpStatusCode,
        String { m_httpStatusText },
        String { m_httpVersion },
        HTTPHeaderMap { m_httpHeaderFields },
        m_networkLoadMetrics ? std::optional(*m_networkLoadMetrics) : std::nullopt,
        m_source,
        m_type,
        m_tainting,
        m_isRedirected,
        m_usedLegacyTLS,
        m_wasPrivateRelayed,
        m_isRangeRequested,
        m_certificateInfo
    } };
}

} // namespace WebCore

namespace WTF::Persistence {

void Coder<WebCore::ResourceResponseData>::encodeForPersistence(Encoder& encoder, const WebCore::ResourceResponseData& data)
{
    encoder << data.url;
    encoder << data.mimeType;
    encoder << static_cast<int64_t>(data.expectedContentLength);
    encoder << data.textEncodingName;
    encoder << data.httpStatusText;
    encoder << data.httpVersion;
    encoder << data.httpHeaderFields;
    encoder << data.httpStatusCode;
    encoder << data.certificateInfo;
    encoder << data.source;
    encoder << data.type;
    encoder << data.tainting;
    encoder << data.isRedirected;
    encoder << data.usedLegacyTLS;
    encoder << data.wasPrivateRelayed;
    encoder << data.isRangeRequested;
}

std::optional<WebCore::ResourceResponseData> Coder<WebCore::ResourceResponseData>::decodeForPersistence(Decoder& decoder)
{
    std::optional<URL> url;
    decoder >> url;
    if (!url)
        return std::nullopt;

    std::optional<String> mimeType;
    decoder >> mimeType;
    if (!mimeType)
        return std::nullopt;

    std::optional<int64_t> expectedContentLength;
    decoder >> expectedContentLength;
    if (!expectedContentLength)
        return std::nullopt;

    std::optional<String> textEncodingName;
    decoder >> textEncodingName;
    if (!textEncodingName)
        return std::nullopt;

    std::optional<String> httpStatusText;
    decoder >> httpStatusText;
    if (!httpStatusText)
        return std::nullopt;

    std::optional<String> httpVersion;
    decoder >> httpVersion;
    if (!httpVersion)
        return std::nullopt;

    std::optional<WebCore::HTTPHeaderMap> httpHeaderFields;
    decoder >> httpHeaderFields;
    if (!httpHeaderFields)
        return std::nullopt;

    std::optional<short> httpStatusCode;
    decoder >> httpStatusCode;
    if (!httpStatusCode)
        return std::nullopt;

    std::optional<std::optional<WebCore::CertificateInfo>> certificateInfo;
    decoder >> certificateInfo;
    if (!certificateInfo)
        return std::nullopt;

    std::optional<WebCore::ResourceResponseBase::Source> source;
    decoder >> source;
    if (!source)
        return std::nullopt;

    std::optional<WebCore::ResourceResponseBase::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<WebCore::ResourceResponseBase::Tainting> tainting;
    decoder >> tainting;
    if (!tainting)
        return std::nullopt;

    std::optional<bool> isRedirected;
    decoder >> isRedirected;
    if (!isRedirected)
        return std::nullopt;

    std::optional<WebCore::UsedLegacyTLS> usedLegacyTLS;
    decoder >> usedLegacyTLS;
    if (!usedLegacyTLS)
        return std::nullopt;

    std::optional<WebCore::WasPrivateRelayed> wasPrivateRelayed;
    decoder >> wasPrivateRelayed;
    if (!wasPrivateRelayed)
        return std::nullopt;

    std::optional<bool> isRangeRequested;
    decoder >> isRangeRequested;
    if (!isRangeRequested)
        return std::nullopt;

    return WebCore::ResourceResponseData {
        WTFMove(*url),
        WTFMove(*mimeType),
        *expectedContentLength,
        WTFMove(*textEncodingName),
        *httpStatusCode,
        WTFMove(*httpStatusText),
        WTFMove(*httpVersion),
        WTFMove(*httpHeaderFields),
        std::nullopt,
        *source,
        *type,
        *tainting,
        *isRedirected,
        *usedLegacyTLS,
        *wasPrivateRelayed,
        *isRangeRequested,
        WTFMove(*certificateInfo)
    };
}

} // namespace WTF::Persistence
