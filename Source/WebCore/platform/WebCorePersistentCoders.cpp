/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCorePersistentCoders.h"

#include "AppHighlightRangeData.h"
#include "CertificateInfo.h"
#include "ClientOrigin.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "CrossOriginEmbedderPolicy.h"
#include "FetchOptions.h"
#include "HTTPHeaderMap.h"
#include "ImageResource.h"
#include "NavigationPreloadState.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SWRegistrationDatabase.h"
#include <wtf/persistence/PersistentCoders.h>

#if PLATFORM(COCOA)
#include <wtf/cf/VectorCF.h>
#include <wtf/spi/cocoa/SecuritySPI.h>
#endif

namespace WTF::Persistence {

#if ENABLE(APP_HIGHLIGHTS)
template<> struct Coder<WebCore::AppHighlightRangeData::NodePathComponent> {
    static void encodeForPersistence(Encoder& encoder, const WebCore::AppHighlightRangeData::NodePathComponent& instance)
    {
        encoder << instance.identifier;
        encoder << instance.nodeName;
        encoder << instance.textData;
        encoder << instance.pathIndex;
    }

    static std::optional<WebCore::AppHighlightRangeData::NodePathComponent> decodeForPersistence(Decoder& decoder)
    {
        std::optional<String> identifier;
        decoder >> identifier;
        if (!identifier)
            return std::nullopt;

        std::optional<String> nodeName;
        decoder >> nodeName;
        if (!nodeName)
            return std::nullopt;

        std::optional<String> textData;
        decoder >> textData;
        if (!textData)
            return std::nullopt;

        std::optional<uint32_t> pathIndex;
        decoder >> pathIndex;
        if (!pathIndex)
            return std::nullopt;

        return { { WTFMove(*identifier), WTFMove(*nodeName), WTFMove(*textData), *pathIndex } };
    }
};

constexpr uint64_t highlightFileSignature = 0x4141504832303231; // File Signature  (A)pple(AP)plication(H)ighlights(2021)

void Coder<WebCore::AppHighlightRangeData>::encodeForPersistence(Encoder& encoder, const WebCore::AppHighlightRangeData& instance)
{
    constexpr uint64_t currentAppHighlightVersion = 1;
    
    encoder << highlightFileSignature;
    encoder << currentAppHighlightVersion;
    encoder << instance.identifier();
    encoder << instance.text();
    encoder << instance.startContainer();
    encoder << instance.startOffset();
    encoder << instance.endContainer();
    encoder << instance.endOffset();
}

std::optional<WebCore::AppHighlightRangeData> Coder<WebCore::AppHighlightRangeData>::decodeForPersistence(Decoder& decoder)
{
    std::optional<uint64_t> version;
    
    std::optional<uint64_t> decodedHighlightFileSignature;
    decoder >> decodedHighlightFileSignature;
    if (!decodedHighlightFileSignature)
        return std::nullopt;
    if (decodedHighlightFileSignature != highlightFileSignature) {
        if (!decoder.rewind(sizeof(highlightFileSignature)))
            return std::nullopt;
        version = 0;
    }
    
    std::optional<String> identifier;
    if (version)
        identifier = nullString();
    else {
        decoder >> version;
        if (!version)
            return std::nullopt;
        
        decoder >> identifier;
        if (!identifier)
            return std::nullopt;
    }

    std::optional<String> text;
    decoder >> text;
    if (!text)
        return std::nullopt;

    std::optional<WebCore::AppHighlightRangeData::NodePath> startContainer;
    decoder >> startContainer;
    if (!startContainer)
        return std::nullopt;

    std::optional<uint32_t> startOffset;
    decoder >> startOffset;
    if (!startOffset)
        return std::nullopt;

    std::optional<WebCore::AppHighlightRangeData::NodePath> endContainer;
    decoder >> endContainer;
    if (!endContainer)
        return std::nullopt;

    std::optional<uint32_t> endOffset;
    decoder >> endOffset;
    if (!endOffset)
        return std::nullopt;

    return { { WTFMove(*identifier), WTFMove(*text), WTFMove(*startContainer), *startOffset, WTFMove(*endContainer), *endOffset } };
}
#endif // ENABLE(APP_HIGHLIGHTS)

void Coder<WebCore::ImportedScriptAttributes>::encodeForPersistence(Encoder& encoder, const WebCore::ImportedScriptAttributes& instance)
{
    encoder << instance.responseURL << instance.mimeType;
}

std::optional<WebCore::ImportedScriptAttributes> Coder<WebCore::ImportedScriptAttributes>::decodeForPersistence(Decoder& decoder)
{
    std::optional<URL> responseURL;
    decoder >> responseURL;
    if (!responseURL)
        return std::nullopt;

    std::optional<String> mimeType;
    decoder >> mimeType;
    if (!mimeType)
        return std::nullopt;

    return { {
        WTFMove(*responseURL),
        WTFMove(*mimeType)
    } };
}

void Coder<WebCore::ImageResource>::encodeForPersistence(Encoder& encoder, const WebCore::ImageResource& instance)
{
    encoder << instance.src << instance.sizes << instance.type << instance.label;
}

std::optional<WebCore::ImageResource> Coder<WebCore::ImageResource>::decodeForPersistence(Decoder& decoder)
{
    std::optional<String> src;
    decoder >> src;
    if (!src)
        return std::nullopt;

    std::optional<String> sizes;
    decoder >> sizes;
    if (!sizes)
        return std::nullopt;

    std::optional<String> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<String> label;
    decoder >> label;
    if (!label)
        return std::nullopt;

    return { {
        WTFMove(*src),
        WTFMove(*sizes),
        WTFMove(*type),
        WTFMove(*label)
    } };
}

void Coder<WebCore::ResourceRequest>::encodeForPersistence(Encoder& encoder, const WebCore::ResourceRequest& instance)
{
    ASSERT(!instance.httpBody());
    ASSERT(!instance.platformRequestUpdated());
    encoder << instance.url();
    encoder << instance.timeoutInterval();
    encoder << instance.firstPartyForCookies().string();
    encoder << instance.httpMethod();
    encoder << instance.httpHeaderFields();
    encoder << instance.responseContentDispositionEncodingFallbackArray();
    encoder << instance.cachePolicy();
    encoder << instance.allowCookies();
    encoder << instance.sameSiteDisposition();
    encoder << instance.isTopSite();
    encoder << instance.priority();
    encoder << instance.requester();
    encoder << instance.isAppInitiated();
}

std::optional<WebCore::ResourceRequest> Coder<WebCore::ResourceRequest>::decodeForPersistence(Decoder& decoder)
{
    std::optional<URL> url;
    decoder >> url;
    if (!url)
        return std::nullopt;

    std::optional<double> timeoutInterval;
    decoder >> timeoutInterval;
    if (!timeoutInterval)
        return std::nullopt;

    std::optional<String> firstPartyForCookies;
    decoder >> firstPartyForCookies;
    if (!firstPartyForCookies)
        return std::nullopt;

    std::optional<String> httpMethod;
    decoder >> httpMethod;
    if (!httpMethod)
        return std::nullopt;

    std::optional<WebCore::HTTPHeaderMap> fields;
    decoder >> fields;
    if (!fields)
        return std::nullopt;

    std::optional<Vector<String>> array;
    decoder >> array;
    if (!array)
        return std::nullopt;

    std::optional<WebCore::ResourceRequestCachePolicy> cachePolicy;
    decoder >> cachePolicy;
    if (!cachePolicy)
        return std::nullopt;

    std::optional<bool> allowCookies;
    decoder >> allowCookies;
    if (!allowCookies)
        return std::nullopt;

    std::optional<WebCore::ResourceRequestBase::SameSiteDisposition> sameSiteDisposition;
    decoder >> sameSiteDisposition;
    if (!sameSiteDisposition)
        return std::nullopt;

    std::optional<bool> isTopSite;
    decoder >> isTopSite;
    if (!isTopSite)
        return std::nullopt;

    std::optional<WebCore::ResourceLoadPriority> priority;
    decoder >> priority;
    if (!priority)
        return std::nullopt;

    std::optional<WebCore::ResourceRequestRequester> requester;
    decoder >> requester;
    if (!requester)
        return std::nullopt;

    std::optional<bool> isAppInitiated;
    decoder >> isAppInitiated;
    if (!isAppInitiated)
        return std::nullopt;

    WebCore::ResourceRequest request;
    request.setURL(WTFMove(*url));
    request.setTimeoutInterval(WTFMove(*timeoutInterval));
    request.setFirstPartyForCookies(URL({ }, *firstPartyForCookies));
    request.setHTTPMethod(WTFMove(*httpMethod));
    request.setHTTPHeaderFields(WTFMove(*fields));
    request.setResponseContentDispositionEncodingFallbackArray(WTFMove(*array));
    request.setCachePolicy(*cachePolicy);
    request.setAllowCookies(*allowCookies);
    request.setSameSiteDisposition(*sameSiteDisposition);
    request.setIsTopSite(*isTopSite);
    request.setPriority(*priority);
    request.setRequester(*requester);
    request.setIsAppInitiated(*isAppInitiated);
    return { request };
}

#if PLATFORM(COCOA)

} // namespace WTF::Persistence

namespace WTF::Persistence {

static void encodeCFData(Encoder& encoder, CFDataRef data)
{
    auto dataSpan = span(data);
    encoder << static_cast<uint64_t>(dataSpan.size());
    encoder.encodeFixedLengthData(dataSpan);
}

static std::optional<RetainPtr<CFDataRef>> decodeCFData(Decoder& decoder)
{
    std::optional<uint64_t> size;
    decoder >> size;

    if (UNLIKELY(!isInBounds<size_t>(*size)))
        return std::nullopt;

    auto buffer = decoder.bufferPointerForDirectRead(static_cast<size_t>(*size));
    if (!buffer.data())
        return std::nullopt;

    return toCFData(buffer);
}

static void encodeSecTrustRef(Encoder& encoder, SecTrustRef trust)
{
    auto data = adoptCF(SecTrustSerialize(trust, nullptr));
    if (!data) {
        encoder << false;
        return;
    }

    encoder << true;
    encodeCFData(encoder, data.get());
}

static std::optional<RetainPtr<SecTrustRef>> decodeSecTrustRef(Decoder& decoder)
{
    std::optional<bool> hasTrust;
    decoder >> hasTrust;
    if (!hasTrust)
        return std::nullopt;

    if (!*hasTrust)
        return { nullptr };

    auto trustData = decodeCFData(decoder);
    if (!trustData)
        return std::nullopt;

    auto trust = adoptCF(SecTrustDeserialize(trustData->get(), nullptr));
    if (!trust)
        return std::nullopt;

    return trust;
}

void Coder<WebCore::CertificateInfo>::encodeForPersistence(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    encodeSecTrustRef(encoder, certificateInfo.trust().get());
}

std::optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decodeForPersistence(Decoder& decoder)
{
    auto trust = decodeSecTrustRef(decoder);
    if (!trust)
        return std::nullopt;

    return WebCore::CertificateInfo(WTFMove(*trust));
}

#elif USE(CURL)

void Coder<WebCore::CertificateInfo>::encodeForPersistence(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    auto& certificateChain = certificateInfo.certificateChain();

    encoder << certificateInfo.verificationError();
    encoder << certificateChain.size();
    for (auto& certificate : certificateChain)
        encoder << certificate;
}

std::optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decodeForPersistence(Decoder& decoder)
{
    std::optional<int> verificationError;
    decoder >> verificationError;
    if (!verificationError)
        return std::nullopt;

    std::optional<size_t> numOfCerts;
    decoder >> numOfCerts;
    if (!numOfCerts)
        return std::nullopt;

    WebCore::CertificateInfo::CertificateChain certificateChain;
    for (size_t i = 0; i < numOfCerts.value(); i++) {
        std::optional<WebCore::CertificateInfo::Certificate> certificate;
        decoder >> certificate;
        if (!certificate)
            return std::nullopt;

        certificateChain.append(WTFMove(certificate.value()));
    }

    return WebCore::CertificateInfo(verificationError.value(), WTFMove(certificateChain));
}

#elif USE(SOUP)

template<> struct Coder<GRefPtr<GByteArray>> {
    static void encodeForPersistence(Encoder &encoder, const GRefPtr<GByteArray>& byteArray)
    {
        encoder << static_cast<uint32_t>(byteArray->len);
        encoder.encodeFixedLengthData({ byteArray->data, byteArray->len });
    }

    static std::optional<GRefPtr<GByteArray>> decodeForPersistence(Decoder& decoder)
    {
        std::optional<uint32_t> size;
        decoder >> size;
        if (!size)
            return std::nullopt;

        GRefPtr<GByteArray> byteArray = adoptGRef(g_byte_array_sized_new(*size));
        g_byte_array_set_size(byteArray.get(), *size);
        if (!decoder.decodeFixedLengthData({ byteArray->data, *size }))
            return std::nullopt;
        return byteArray;
    }
};

static Vector<GRefPtr<GByteArray>> certificatesDataListFromCertificateInfo(const WebCore::CertificateInfo &certificateInfo)
{
    auto* certificate = certificateInfo.certificate().get();
    if (!certificate)
        return { };

    Vector<GRefPtr<GByteArray>> certificatesDataList;
    for (; certificate; certificate = g_tls_certificate_get_issuer(certificate)) {
        GByteArray* certificateData = nullptr;
        g_object_get(G_OBJECT(certificate), "certificate", &certificateData, nullptr);

        if (!certificateData) {
            certificatesDataList.clear();
            break;
        }
        certificatesDataList.append(adoptGRef(certificateData));
    }

    // Reverse so that the list starts from the rootmost certificate.
    certificatesDataList.reverse();

    return certificatesDataList;
}

static GRefPtr<GTlsCertificate> certificateFromCertificatesDataList(const Vector<GRefPtr<GByteArray>> &certificatesDataList)
{
    GType certificateType = g_tls_backend_get_certificate_type(g_tls_backend_get_default());
    GRefPtr<GTlsCertificate> certificate;
    for (auto& certificateData : certificatesDataList) {
        certificate = adoptGRef(G_TLS_CERTIFICATE(g_initable_new(
            certificateType, nullptr, nullptr, "certificate", certificateData.get(), "issuer", certificate.get(), nullptr)));
        if (!certificate)
            break;
    }

    return certificate;
}

void Coder<WebCore::CertificateInfo>::encodeForPersistence(Encoder& encoder, const WebCore::CertificateInfo& certificateInfo)
{
    auto certificatesDataList = certificatesDataListFromCertificateInfo(certificateInfo);

    encoder << certificatesDataList;

    if (certificatesDataList.isEmpty())
        return;

    encoder << static_cast<uint32_t>(certificateInfo.tlsErrors());
}

std::optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decodeForPersistence(Decoder& decoder)
{
    std::optional<Vector<GRefPtr<GByteArray>>> certificatesDataList;
    decoder >> certificatesDataList;
    if (!certificatesDataList)
        return std::nullopt;

    WebCore::CertificateInfo certificateInfo;
    if (certificatesDataList->isEmpty())
        return certificateInfo;

    auto certificate = certificateFromCertificatesDataList(certificatesDataList.value());
    if (!certificate)
        return std::nullopt;
    certificateInfo.setCertificate(certificate.get());

    std::optional<uint32_t> tlsErrors;
    decoder >> tlsErrors;
    if (!tlsErrors)
        return std::nullopt;
    certificateInfo.setTLSErrors(static_cast<GTlsCertificateFlags>(*tlsErrors));

    return certificateInfo;
}

#elif PLATFORM(WIN)

void Coder<WebCore::CertificateInfo>::encodeForPersistence(Encoder&, const WebCore::CertificateInfo&)
{
}

std::optional<WebCore::CertificateInfo> Coder<WebCore::CertificateInfo>::decodeForPersistence(Decoder&)
{
    return WebCore::CertificateInfo();
}

#endif

// FIXME: Move persistent coder implementations here and generate IPC coders for these structures.
void Coder<WebCore::NavigationPreloadState>::encodeForPersistence(Encoder& encoder, const WebCore::NavigationPreloadState& instance)
{
    encoder << instance.enabled;
    encoder << instance.headerValue;
}

std::optional<WebCore::NavigationPreloadState> Coder<WebCore::NavigationPreloadState>::decodeForPersistence(Decoder& decoder)
{
    std::optional<bool> enabled;
    decoder >> enabled;
    if (!enabled)
        return { };

    std::optional<String> headerValue;
    decoder >> headerValue;
    if (!headerValue)
        return { };
    return { { *enabled, WTFMove(*headerValue) } };
}

void Coder<WebCore::CrossOriginEmbedderPolicy>::encodeForPersistence(Encoder& encoder, const WebCore::CrossOriginEmbedderPolicy& instance)
{
    instance.encode(encoder);
}

std::optional<WebCore::CrossOriginEmbedderPolicy> Coder<WebCore::CrossOriginEmbedderPolicy>::decodeForPersistence(Decoder& decoder)
{
    return WebCore::CrossOriginEmbedderPolicy::decode(decoder);
}

void Coder<WebCore::ContentSecurityPolicyResponseHeaders>::encodeForPersistence(Encoder& encoder, const WebCore::ContentSecurityPolicyResponseHeaders& instance)
{
    encoder << static_cast<uint64_t>(instance.headers().size());
    for (auto& pair : instance.headers()) {
        encoder << pair.first;
        encoder << pair.second;
    }
    encoder << instance.httpStatusCode();
}

std::optional<WebCore::ContentSecurityPolicyResponseHeaders> Coder<WebCore::ContentSecurityPolicyResponseHeaders>::decodeForPersistence(Decoder& decoder)
{
    WebCore::ContentSecurityPolicyResponseHeaders headers;

    std::optional<uint64_t> headersSize;
    decoder >> headersSize;
    if (!headersSize)
        return std::nullopt;

    Vector<std::pair<String, WebCore::ContentSecurityPolicyHeaderType>> headersVector;
    for (size_t i = 0; i < *headersSize; ++i) {
        std::optional<String> header;
        decoder >> header;
        if (!header)
            return std::nullopt;
        std::optional<WebCore::ContentSecurityPolicyHeaderType> headerType;
        decoder >> headerType;
        if (!headerType)
            return std::nullopt;
        headersVector.append(std::make_pair(WTFMove(*header), WTFMove(*headerType)));
    }
    headersVector.shrinkToFit();
    headers.setHeaders(WTFMove(headersVector));

    std::optional<int> httpStatusCode;
    decoder >> httpStatusCode;
    if (!httpStatusCode)
        return std::nullopt;
    headers.setHTTPStatusCode(*httpStatusCode);

    return headers;
}

void Coder<WebCore::ClientOrigin>::encodeForPersistence(Encoder& encoder, const WebCore::ClientOrigin& instance)
{
    encoder << instance.topOrigin;
    encoder << instance.clientOrigin;
}

std::optional<WebCore::ClientOrigin> Coder<WebCore::ClientOrigin>::decodeForPersistence(Decoder& decoder)
{
    std::optional<WebCore::SecurityOriginData> topOrigin;
    std::optional<WebCore::SecurityOriginData> clientOrigin;
    decoder >> topOrigin;
    if (!topOrigin || topOrigin->isNull())
        return std::nullopt;
    decoder >> clientOrigin;
    if (!clientOrigin || clientOrigin->isNull())
        return std::nullopt;

    return WebCore::ClientOrigin { WTFMove(*topOrigin), WTFMove(*clientOrigin) };
}

void Coder<WebCore::SecurityOriginData>::encodeForPersistence(Encoder& encoder, const WebCore::SecurityOriginData& instance)
{
    encoder << instance.protocol();
    encoder << instance.host();
    encoder << instance.port();
}

std::optional<WebCore::SecurityOriginData> Coder<WebCore::SecurityOriginData>::decodeForPersistence(Decoder& decoder)
{
    std::optional<String> protocol;
    decoder >> protocol;
    if (!protocol)
        return std::nullopt;

    std::optional<String> host;
    decoder >> host;
    if (!host)
        return std::nullopt;

    std::optional<std::optional<uint16_t>> port;
    decoder >> port;
    if (!port)
        return std::nullopt;

    WebCore::SecurityOriginData data { WTFMove(*protocol), WTFMove(*host), WTFMove(*port) };
    if (data.isHashTableDeletedValue())
        return std::nullopt;

    return data;
}

void Coder<WebCore::ResourceResponse>::encodeForPersistence(Encoder& encoder, const WebCore::ResourceResponse& instance)
{
    encoder << instance.m_isNull;
    if (instance.m_isNull)
        return;
    instance.lazyInit(WebCore::ResourceResponseBase::AllFields);

    encoder << instance.m_url;
    encoder << instance.m_mimeType;
    encoder << static_cast<int64_t>(instance.m_expectedContentLength);
    encoder << instance.m_textEncodingName;
    encoder << instance.m_httpStatusText;
    encoder << instance.m_httpVersion;
    encoder << instance.m_httpHeaderFields;

    encoder << instance.m_httpStatusCode;
    encoder << instance.m_certificateInfo;
    encoder << instance.m_source;
    encoder << instance.m_type;
    encoder << instance.m_tainting;
    encoder << instance.m_isRedirected;
    WebCore::UsedLegacyTLS usedLegacyTLS = instance.m_usedLegacyTLS;
    encoder << usedLegacyTLS;
    WebCore::WasPrivateRelayed wasPrivateRelayed = instance.m_wasPrivateRelayed;
    encoder << wasPrivateRelayed;
    encoder << instance.m_isRangeRequested;
}

std::optional<WebCore::ResourceResponse> Coder<WebCore::ResourceResponse>::decodeForPersistence(Decoder& decoder)
{
    WebCore::ResourceResponse response;
    ASSERT(response.m_isNull);
    std::optional<bool> responseIsNull;
    decoder >> responseIsNull;
    if (!responseIsNull)
        return std::nullopt;
    if (*responseIsNull)
        return { WTFMove(response) };

    response.m_isNull = false;

    std::optional<URL> url;
    decoder >> url;
    if (!url)
        return std::nullopt;
    response.m_url = WTFMove(*url);

    std::optional<AtomString> mimeType;
    decoder >> mimeType;
    if (!mimeType)
        return std::nullopt;
    response.m_mimeType = WTFMove(*mimeType);

    std::optional<int64_t> expectedContentLength;
    decoder >> expectedContentLength;
    if (!expectedContentLength)
        return std::nullopt;
    response.m_expectedContentLength = *expectedContentLength;

    std::optional<AtomString> textEncodingName;
    decoder >> textEncodingName;
    if (!textEncodingName)
        return std::nullopt;
    response.m_textEncodingName = WTFMove(*textEncodingName);

    std::optional<AtomString> httpStatusText;
    decoder >> httpStatusText;
    if (!httpStatusText)
        return std::nullopt;
    response.m_httpStatusText = WTFMove(*httpStatusText);

    std::optional<AtomString> httpVersion;
    decoder >> httpVersion;
    if (!httpVersion)
        return std::nullopt;
    response.m_httpVersion = WTFMove(*httpVersion);

    std::optional<WebCore::HTTPHeaderMap> httpHeaderFields;
    decoder >> httpHeaderFields;
    if (!httpHeaderFields)
        return std::nullopt;
    response.m_httpHeaderFields = WTFMove(*httpHeaderFields);

    std::optional<short> httpStatusCode;
    decoder >> httpStatusCode;
    if (!httpStatusCode)
        return std::nullopt;
    response.m_httpStatusCode = WTFMove(*httpStatusCode);

    std::optional<std::optional<WebCore::CertificateInfo>> certificateInfo;
    decoder >> certificateInfo;
    if (!certificateInfo)
        return std::nullopt;
    response.m_certificateInfo = WTFMove(*certificateInfo);

    std::optional<WebCore::ResourceResponseBase::Source> source;
    decoder >> source;
    if (!source)
        return std::nullopt;
    response.m_source = WTFMove(*source);

    std::optional<WebCore::ResourceResponseBase::Type> type;
    decoder >> type;
    if (!type)
        return std::nullopt;
    response.m_type = WTFMove(*type);

    std::optional<WebCore::ResourceResponseBase::Tainting> tainting;
    decoder >> tainting;
    if (!tainting)
        return std::nullopt;
    response.m_tainting = WTFMove(*tainting);

    std::optional<bool> isRedirected;
    decoder >> isRedirected;
    if (!isRedirected)
        return std::nullopt;
    response.m_isRedirected = WTFMove(*isRedirected);

    std::optional<WebCore::UsedLegacyTLS> usedLegacyTLS;
    decoder >> usedLegacyTLS;
    if (!usedLegacyTLS)
        return std::nullopt;
    response.m_usedLegacyTLS = WTFMove(*usedLegacyTLS);

    std::optional<WebCore::WasPrivateRelayed> wasPrivateRelayed;
    decoder >> wasPrivateRelayed;
    if (!wasPrivateRelayed)
        return std::nullopt;
    response.m_wasPrivateRelayed = WTFMove(*wasPrivateRelayed);

    std::optional<bool> isRangeRequested;
    decoder >> isRangeRequested;
    if (!isRangeRequested)
        return std::nullopt;
    response.m_isRangeRequested = WTFMove(*isRangeRequested);

    return { WTFMove(response) };
}

void Coder<WebCore::FetchOptions>::encodeForPersistence(Encoder& encoder, const WebCore::FetchOptions& instance)
{
    instance.encodePersistent(encoder);
}

std::optional<WebCore::FetchOptions> Coder<WebCore::FetchOptions>::decodeForPersistence(Decoder& decoder)
{
    WebCore::FetchOptions options;
    if (!WebCore::FetchOptions::decodePersistent(decoder, options))
        return std::nullopt;
    return options;
}

// Store common HTTP headers as strings instead of using their value in the HTTPHeaderName enumeration
// so that the headers stored in the cache stays valid even after HTTPHeaderName.in gets updated.
void Coder<WebCore::HTTPHeaderMap>::encodeForPersistence(Encoder& encoder, const WebCore::HTTPHeaderMap& headers)
{
    encoder << static_cast<uint64_t>(headers.size());
    for (auto& keyValue : headers) {
        encoder << keyValue.key;
        encoder << keyValue.value;
    }
}

std::optional<WebCore::HTTPHeaderMap> Coder<WebCore::HTTPHeaderMap>::decodeForPersistence(Decoder& decoder)
{
    std::optional<uint64_t> headersSize;
    decoder >> headersSize;
    if (!headersSize)
        return std::nullopt;

    WebCore::HTTPHeaderMap headers;
    for (uint64_t i = 0; i < *headersSize; ++i) {
        std::optional<String> name;
        decoder >> name;
        if (!name)
            return std::nullopt;
        std::optional<String> value;
        decoder >> value;
        if (!value)
            return std::nullopt;
        headers.append(WTFMove(*name), WTFMove(*value));
    }
    return headers;
}

} // namespace WTF::Persistence
