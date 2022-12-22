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

#pragma once

#include "CacheValidation.h"
#include "CertificateInfo.h"
#include "HTTPHeaderMap.h"
#include "NetworkLoadMetrics.h"
#include "ParsedContentRange.h"
#include <wtf/ArgumentCoder.h>
#include <wtf/Box.h>
#include <wtf/EnumTraits.h>
#include <wtf/Markable.h>
#include <wtf/Span.h>
#include <wtf/URL.h>
#include <wtf/WallTime.h>

namespace WebCore {

namespace DataURLDecoder {
struct Result;
}

class ResourceResponse;

bool isScriptAllowedByNosniff(const ResourceResponse&);

enum class UsedLegacyTLS : bool { No, Yes };
static constexpr unsigned bitWidthOfUsedLegacyTLS = 1;
static_assert(static_cast<unsigned>(UsedLegacyTLS::Yes) <= ((1U << bitWidthOfUsedLegacyTLS) - 1));

enum class WasPrivateRelayed : bool { No, Yes };
static constexpr unsigned bitWidthOfWasPrivateRelayed = 1;
static_assert(static_cast<unsigned>(WasPrivateRelayed::Yes) <= ((1U << bitWidthOfWasPrivateRelayed) - 1));

// Do not use this class directly, use the class ResourceResponse instead
class ResourceResponseBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type : uint8_t { Basic, Cors, Default, Error, Opaque, Opaqueredirect };
    static constexpr unsigned bitWidthOfType = 3;
    enum class Tainting : uint8_t { Basic, Cors, Opaque, Opaqueredirect };
    static constexpr unsigned bitWidthOfTainting = 2;
    enum class Source : uint8_t { Unknown, Network, DiskCache, DiskCacheAfterValidation, MemoryCache, MemoryCacheAfterValidation, ServiceWorker, ApplicationCache, DOMCache, InspectorOverride };
    static constexpr unsigned bitWidthOfSource = 4;

    static bool isRedirectionStatusCode(int code) { return code == 301 || code == 302 || code == 303 || code == 307 || code == 308; }

    struct CrossThreadData {
        CrossThreadData(const CrossThreadData&) = delete;
        CrossThreadData& operator=(const CrossThreadData&) = delete;
        CrossThreadData() = default;
        CrossThreadData(CrossThreadData&&) = default;
        CrossThreadData& operator=(CrossThreadData&&) = default;
        WEBCORE_EXPORT CrossThreadData copy() const;

        URL url;
        String mimeType;
        long long expectedContentLength;
        String textEncodingName;
        int httpStatusCode;
        String httpStatusText;
        String httpVersion;
        HTTPHeaderMap httpHeaderFields;
        std::optional<NetworkLoadMetrics> networkLoadMetrics;
        Type type;
        Tainting tainting;
        bool isRedirected;
        bool isRangeRequested;
    };
    
    struct ResponseData {
        URL m_url;
        AtomString m_mimeType;
        long long m_expectedContentLength;
        AtomString m_textEncodingName;
        AtomString m_httpStatusText;
        AtomString m_httpVersion;
        HTTPHeaderMap m_httpHeaderFields;
        Box<WebCore::NetworkLoadMetrics> m_networkLoadMetrics;

        short m_httpStatusCode;
        std::optional<CertificateInfo> m_certificateInfo;
        
        ResourceResponseBase::Source m_source;
        ResourceResponseBase::Type m_type;
        ResourceResponseBase::Tainting m_tainting;

        bool m_isRedirected;
        UsedLegacyTLS m_usedLegacyTLS;
        WasPrivateRelayed m_wasPrivateRelayed;
        bool m_isRangeRequested;
    };

    WEBCORE_EXPORT CrossThreadData crossThreadData() const;
    WEBCORE_EXPORT static ResourceResponse fromCrossThreadData(CrossThreadData&&);

    bool isNull() const { return m_isNull; }
    WEBCORE_EXPORT bool isInHTTPFamily() const;
    WEBCORE_EXPORT bool isSuccessful() const;

    WEBCORE_EXPORT const URL& url() const;
    WEBCORE_EXPORT void setURL(const URL&);

    WEBCORE_EXPORT const AtomString& mimeType() const;
    WEBCORE_EXPORT void setMimeType(const AtomString&);

    WEBCORE_EXPORT long long expectedContentLength() const;
    WEBCORE_EXPORT void setExpectedContentLength(long long expectedContentLength);

    WEBCORE_EXPORT const AtomString& textEncodingName() const;
    WEBCORE_EXPORT void setTextEncodingName(AtomString&&);

    WEBCORE_EXPORT int httpStatusCode() const;
    WEBCORE_EXPORT void setHTTPStatusCode(int);
    WEBCORE_EXPORT bool isRedirection() const;

    WEBCORE_EXPORT const AtomString& httpStatusText() const;
    WEBCORE_EXPORT void setHTTPStatusText(const AtomString&);

    WEBCORE_EXPORT const AtomString& httpVersion() const;
    WEBCORE_EXPORT void setHTTPVersion(const AtomString&);
    WEBCORE_EXPORT bool isHTTP09() const;

    WEBCORE_EXPORT const HTTPHeaderMap& httpHeaderFields() const;
    void setHTTPHeaderFields(HTTPHeaderMap&&);

    enum class SanitizationType { Redirection, RemoveCookies, CrossOriginSafe };
    WEBCORE_EXPORT void sanitizeHTTPHeaderFields(SanitizationType);

    String httpHeaderField(StringView name) const;
    WEBCORE_EXPORT String httpHeaderField(HTTPHeaderName) const;
    WEBCORE_EXPORT void setHTTPHeaderField(const String& name, const String& value);
    WEBCORE_EXPORT void setUncommonHTTPHeaderField(const String& name, const String& value);
    WEBCORE_EXPORT void setHTTPHeaderField(HTTPHeaderName, const String& value);

    WEBCORE_EXPORT void addHTTPHeaderField(HTTPHeaderName, const String& value);
    WEBCORE_EXPORT void addHTTPHeaderField(const String& name, const String& value);
    WEBCORE_EXPORT void addUncommonHTTPHeaderField(const String& name, const String& value);

    // Instead of passing a string literal to any of these functions, just use a HTTPHeaderName instead.
    template<size_t length> String httpHeaderField(ASCIILiteral) const = delete;
    template<size_t length> void setHTTPHeaderField(ASCIILiteral, const String&) = delete;
    template<size_t length> void addHTTPHeaderField(ASCIILiteral, const String&) = delete;

    bool isMultipart() const { return mimeType() == "multipart/x-mixed-replace"_s; }

    WEBCORE_EXPORT bool isAttachment() const;
    WEBCORE_EXPORT bool isAttachmentWithFilename() const;
    WEBCORE_EXPORT String suggestedFilename() const;
    WEBCORE_EXPORT static String sanitizeSuggestedFilename(const String&);

    WEBCORE_EXPORT void includeCertificateInfo(Span<const std::byte> = { }) const;
    void setCertificateInfo(CertificateInfo&& info) { m_certificateInfo = WTFMove(info); }
    const std::optional<CertificateInfo>& certificateInfo() const { return m_certificateInfo; };
    bool usedLegacyTLS() const { return m_usedLegacyTLS == UsedLegacyTLS::Yes; }
    void setUsedLegacyTLS(UsedLegacyTLS used) { m_usedLegacyTLS = used; }
    bool wasPrivateRelayed() const { return m_wasPrivateRelayed == WasPrivateRelayed::Yes; }
    void setWasPrivateRelayed(WasPrivateRelayed privateRelayed) { m_wasPrivateRelayed = privateRelayed; }
    
    // These functions return parsed values of the corresponding response headers.
    WEBCORE_EXPORT bool cacheControlContainsNoCache() const;
    WEBCORE_EXPORT bool cacheControlContainsNoStore() const;
    WEBCORE_EXPORT bool cacheControlContainsMustRevalidate() const;
    WEBCORE_EXPORT bool cacheControlContainsImmutable() const;
    WEBCORE_EXPORT bool hasCacheValidatorFields() const;
    WEBCORE_EXPORT std::optional<Seconds> cacheControlMaxAge() const;
    WEBCORE_EXPORT std::optional<Seconds> cacheControlStaleWhileRevalidate() const;
    WEBCORE_EXPORT std::optional<WallTime> date() const;
    WEBCORE_EXPORT std::optional<Seconds> age() const;
    WEBCORE_EXPORT std::optional<WallTime> expires() const;
    WEBCORE_EXPORT std::optional<WallTime> lastModified() const;
    const ParsedContentRange& contentRange() const;

    static_assert(static_cast<unsigned>(Source::InspectorOverride) <= ((1U << bitWidthOfSource) - 1));

    WEBCORE_EXPORT Source source() const;
    void setSource(Source source)
    {
        ASSERT(source != Source::Unknown);
        m_source = source;
    }

    // FIXME: This should be eliminated from ResourceResponse.
    // Network loading metrics should be delivered via didFinishLoad
    // and should not be part of the ResourceResponse.
    const NetworkLoadMetrics* deprecatedNetworkLoadMetricsOrNull() const
    {
        if (m_networkLoadMetrics)
            return m_networkLoadMetrics.get();
        return nullptr;
    }
    void setDeprecatedNetworkLoadMetrics(Box<NetworkLoadMetrics>&& metrics)
    {
        m_networkLoadMetrics = WTFMove(metrics);
    }
    Box<NetworkLoadMetrics> takeNetworkLoadMetrics()
    {
        return std::exchange(m_networkLoadMetrics, nullptr);
    }

    // The ResourceResponse subclass may "shadow" this method to provide platform-specific memory usage information
    unsigned memoryUsage() const
    {
        // average size, mostly due to URL and Header Map strings
        return 1280;
    }

    WEBCORE_EXPORT void setType(Type);
    Type type() const { return m_type; }

    void setRedirected(bool isRedirected) { m_isRedirected = isRedirected; }
    bool isRedirected() const { return m_isRedirected; }

    void setTainting(Tainting tainting) { m_tainting = tainting; }
    Tainting tainting() const { return m_tainting; }

    enum class PerformExposeAllHeadersCheck : uint8_t { Yes, No };
    static ResourceResponse filter(const ResourceResponse&, PerformExposeAllHeadersCheck);

    WEBCORE_EXPORT static ResourceResponse syntheticRedirectResponse(const URL& fromURL, const URL& toURL);

    static bool equalForWebKitLegacyChallengeComparison(const ResourceResponse&, const ResourceResponse&);

    template<class Encoder, typename = std::enable_if_t<!std::is_same_v<Encoder, IPC::Encoder>>>
    void encode(Encoder&) const;
    template<class Decoder, typename = std::enable_if_t<!std::is_same_v<Decoder, IPC::Decoder>>>
    static WARN_UNUSED_RETURN bool decode(Decoder&, ResourceResponseBase&);

    bool isRangeRequested() const { return m_isRangeRequested; }
    void setAsRangeRequested() { m_isRangeRequested = true; }

    bool containsInvalidHTTPHeaders() const;

    WEBCORE_EXPORT static ResourceResponse dataURLResponse(const URL&, const DataURLDecoder::Result&);
    
    WEBCORE_EXPORT ResourceResponseBase(std::optional<ResponseData>);
    
    WEBCORE_EXPORT std::optional<ResponseData> getResponseData() const;

protected:
    enum InitLevel {
        Uninitialized,
        CommonFieldsOnly,
        AllFields
    };

    WEBCORE_EXPORT ResourceResponseBase();
    WEBCORE_EXPORT ResourceResponseBase(const URL&, const String& mimeType, long long expectedLength, const String& textEncodingName);

    WEBCORE_EXPORT void lazyInit(InitLevel) const;

    // The ResourceResponse subclass should shadow these functions to lazily initialize platform specific fields
    void platformLazyInit(InitLevel) { }
    CertificateInfo platformCertificateInfo(Span<const std::byte>) const { return CertificateInfo(); };
    String platformSuggestedFileName() const { return String(); }

    static bool platformCompare(const ResourceResponse&, const ResourceResponse&) { return true; }

private:
    void parseCacheControlDirectives() const;
    void updateHeaderParsedState(HTTPHeaderName);
    void sanitizeHTTPHeaderFieldsAccordingToTainting();

protected:
    URL m_url;
    AtomString m_mimeType;
    long long m_expectedContentLength { 0 };
    AtomString m_textEncodingName;
    AtomString m_httpStatusText;
    AtomString m_httpVersion;
    HTTPHeaderMap m_httpHeaderFields;
    Box<NetworkLoadMetrics> m_networkLoadMetrics;

    mutable std::optional<CertificateInfo> m_certificateInfo;

private:
    mutable Markable<Seconds, Seconds::MarkableTraits> m_age;
    mutable Markable<WallTime, WallTime::MarkableTraits> m_date;
    mutable Markable<WallTime, WallTime::MarkableTraits> m_expires;
    mutable Markable<WallTime, WallTime::MarkableTraits> m_lastModified;
    mutable ParsedContentRange m_contentRange;
    mutable CacheControlDirectives m_cacheControlDirectives;

    mutable bool m_haveParsedCacheControlHeader : 1 { false };
    mutable bool m_haveParsedAgeHeader : 1 { false };
    mutable bool m_haveParsedDateHeader : 1 { false };
    mutable bool m_haveParsedExpiresHeader : 1 { false };
    mutable bool m_haveParsedLastModifiedHeader : 1 { false };
    mutable bool m_haveParsedContentRangeHeader : 1 { false };
    bool m_isRedirected : 1 { false };
    bool m_isRangeRequested : 1 { false };
protected:
    bool m_isNull : 1 { true };
    unsigned m_initLevel : 3; // Controlled by ResourceResponse.
    mutable UsedLegacyTLS m_usedLegacyTLS : bitWidthOfUsedLegacyTLS { UsedLegacyTLS::No };
    mutable WasPrivateRelayed m_wasPrivateRelayed : bitWidthOfWasPrivateRelayed { WasPrivateRelayed::No };
private:
    Tainting m_tainting : bitWidthOfTainting { Tainting::Basic };
    Source m_source : bitWidthOfSource { Source::Unknown };
    Type m_type : bitWidthOfType { Type::Default };
protected:
    short m_httpStatusCode { 0 };
};

template<class Encoder, typename>
void ResourceResponseBase::encode(Encoder& encoder) const
{
    encoder << m_isNull;
    if (m_isNull)
        return;
    lazyInit(AllFields);

    encoder << m_url;
    encoder << m_mimeType;
    encoder << static_cast<int64_t>(m_expectedContentLength);
    encoder << m_textEncodingName;
    encoder << m_httpStatusText;
    encoder << m_httpVersion;
    encoder << m_httpHeaderFields;

    encoder << m_httpStatusCode;
    encoder << m_certificateInfo;
    encoder << m_source;
    encoder << m_type;
    encoder << m_tainting;
    encoder << m_isRedirected;
    UsedLegacyTLS usedLegacyTLS = m_usedLegacyTLS;
    encoder << usedLegacyTLS;
    WasPrivateRelayed wasPrivateRelayed = m_wasPrivateRelayed;
    encoder << wasPrivateRelayed;
    encoder << m_isRangeRequested;
}

template<class Decoder, typename>
bool ResourceResponseBase::decode(Decoder& decoder, ResourceResponseBase& response)
{
    ASSERT(response.m_isNull);
    std::optional<bool> responseIsNull;
    decoder >> responseIsNull;
    if (!responseIsNull)
        return false;
    if (*responseIsNull)
        return true;

    response.m_isNull = false;

    std::optional<URL> url;
    decoder >> url;
    if (!url)
        return false;
    response.m_url = WTFMove(*url);

    std::optional<AtomString> mimeType;
    decoder >> mimeType;
    if (!mimeType)
        return false;
    response.m_mimeType = WTFMove(*mimeType);

    std::optional<int64_t> expectedContentLength;
    decoder >> expectedContentLength;
    if (!expectedContentLength)
        return false;
    response.m_expectedContentLength = *expectedContentLength;

    std::optional<AtomString> textEncodingName;
    decoder >> textEncodingName;
    if (!textEncodingName)
        return false;
    response.m_textEncodingName = WTFMove(*textEncodingName);

    std::optional<AtomString> httpStatusText;
    decoder >> httpStatusText;
    if (!httpStatusText)
        return false;
    response.m_httpStatusText = WTFMove(*httpStatusText);

    std::optional<AtomString> httpVersion;
    decoder >> httpVersion;
    if (!httpVersion)
        return false;
    response.m_httpVersion = WTFMove(*httpVersion);

    std::optional<HTTPHeaderMap> httpHeaderFields;
    decoder >> httpHeaderFields;
    if (!httpHeaderFields)
        return false;
    response.m_httpHeaderFields = WTFMove(*httpHeaderFields);

    std::optional<short> httpStatusCode;
    decoder >> httpStatusCode;
    if (!httpStatusCode)
        return false;
    response.m_httpStatusCode = WTFMove(*httpStatusCode);

    std::optional<std::optional<CertificateInfo>> certificateInfo;
    decoder >> certificateInfo;
    if (!certificateInfo)
        return false;
    response.m_certificateInfo = WTFMove(*certificateInfo);

    std::optional<Source> source;
    decoder >> source;
    if (!source)
        return false;
    response.m_source = WTFMove(*source);

    std::optional<Type> type;
    decoder >> type;
    if (!type)
        return false;
    response.m_type = WTFMove(*type);

    std::optional<Tainting> tainting;
    decoder >> tainting;
    if (!tainting)
        return false;
    response.m_tainting = WTFMove(*tainting);

    std::optional<bool> isRedirected;
    decoder >> isRedirected;
    if (!isRedirected)
        return false;
    response.m_isRedirected = WTFMove(*isRedirected);

    std::optional<UsedLegacyTLS> usedLegacyTLS;
    decoder >> usedLegacyTLS;
    if (!usedLegacyTLS)
        return false;
    response.m_usedLegacyTLS = WTFMove(*usedLegacyTLS);

    std::optional<WasPrivateRelayed> wasPrivateRelayed;
    decoder >> wasPrivateRelayed;
    if (!wasPrivateRelayed)
        return false;
    response.m_wasPrivateRelayed = WTFMove(*wasPrivateRelayed);

    std::optional<bool> isRangeRequested;
    decoder >> isRangeRequested;
    if (!isRangeRequested)
        return false;
    response.m_isRangeRequested = WTFMove(*isRangeRequested);

    return true;
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraitsForPersistence<WebCore::ResourceResponseBase::Type> {
    using values = EnumValues<
        WebCore::ResourceResponseBase::Type,
        WebCore::ResourceResponseBase::Type::Basic,
        WebCore::ResourceResponseBase::Type::Cors,
        WebCore::ResourceResponseBase::Type::Default,
        WebCore::ResourceResponseBase::Type::Error,
        WebCore::ResourceResponseBase::Type::Opaque,
        WebCore::ResourceResponseBase::Type::Opaqueredirect
    >;
};

template<> struct EnumTraitsForPersistence<WebCore::ResourceResponseBase::Tainting> {
    using values = EnumValues<
        WebCore::ResourceResponseBase::Tainting,
        WebCore::ResourceResponseBase::Tainting::Basic,
        WebCore::ResourceResponseBase::Tainting::Cors,
        WebCore::ResourceResponseBase::Tainting::Opaque,
        WebCore::ResourceResponseBase::Tainting::Opaqueredirect
    >;
};

template<> struct EnumTraitsForPersistence<WebCore::ResourceResponseBase::Source> {
    using values = EnumValues<
        WebCore::ResourceResponseBase::Source,
        WebCore::ResourceResponseBase::Source::Unknown,
        WebCore::ResourceResponseBase::Source::Network,
        WebCore::ResourceResponseBase::Source::DiskCache,
        WebCore::ResourceResponseBase::Source::DiskCacheAfterValidation,
        WebCore::ResourceResponseBase::Source::MemoryCache,
        WebCore::ResourceResponseBase::Source::MemoryCacheAfterValidation,
        WebCore::ResourceResponseBase::Source::ServiceWorker,
        WebCore::ResourceResponseBase::Source::ApplicationCache,
        WebCore::ResourceResponseBase::Source::DOMCache,
        WebCore::ResourceResponseBase::Source::InspectorOverride
    >;
};

} // namespace WTF
