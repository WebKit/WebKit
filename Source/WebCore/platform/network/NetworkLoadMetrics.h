/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2014-2021 Apple, Inc. All Rights Reserved.
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

#include "HTTPHeaderMap.h"
#include <wtf/Box.h>
#include <wtf/MonotonicTime.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSURLConnection;
OBJC_CLASS NSURLSessionTaskMetrics;
#endif

namespace WebCore {

class ResourceHandle;

enum class NetworkLoadPriority : uint8_t {
    Low,
    Medium,
    High,
    Unknown,
};

enum class PrivacyStance : uint8_t {
    Unknown,
    NotEligible,
    Proxied,
    Failed,
    Direct,
};

constexpr MonotonicTime reusedTLSConnectionSentinel { MonotonicTime::fromRawSeconds(-1) };

struct AdditionalNetworkLoadMetricsForWebInspector;

class NetworkLoadMetrics {
    WTF_MAKE_FAST_ALLOCATED(NetworkLoadMetrics);
public:
    WEBCORE_EXPORT NetworkLoadMetrics();

    WEBCORE_EXPORT static const NetworkLoadMetrics& emptyMetrics();

    WEBCORE_EXPORT NetworkLoadMetrics isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<NetworkLoadMetrics> decode(Decoder&);

    bool isComplete() const { return complete; }
    void markComplete() { complete = true; }

    void updateFromFinalMetrics(const NetworkLoadMetrics&);

    // https://www.w3.org/TR/resource-timing-2/#attribute-descriptions
    MonotonicTime redirectStart;
    MonotonicTime fetchStart;
    MonotonicTime domainLookupStart;
    MonotonicTime domainLookupEnd;
    MonotonicTime connectStart;
    MonotonicTime secureConnectionStart;
    MonotonicTime connectEnd;
    MonotonicTime requestStart;
    MonotonicTime responseStart;
    MonotonicTime responseEnd;
    MonotonicTime workerStart;

    // ALPN Protocol ID: https://w3c.github.io/resource-timing/#bib-RFC7301
    String protocol;

    uint16_t redirectCount { 0 };

    bool complete : 1 { false };
    bool cellular : 1 { false };
    bool expensive : 1 { false };
    bool constrained : 1 { false };
    bool multipath : 1 { false };
    bool isReusedConnection : 1 { false };
    bool failsTAOCheck : 1 { false };
    bool hasCrossOriginRedirect : 1 { false };

    PrivacyStance privacyStance { PrivacyStance::Unknown };

    uint64_t responseBodyBytesReceived { std::numeric_limits<uint64_t>::max() };
    uint64_t responseBodyDecodedSize { std::numeric_limits<uint64_t>::max() };

    RefPtr<AdditionalNetworkLoadMetricsForWebInspector> additionalNetworkLoadMetricsForWebInspector;
};

struct AdditionalNetworkLoadMetricsForWebInspector : public RefCounted<AdditionalNetworkLoadMetricsForWebInspector> {

    static Ref<AdditionalNetworkLoadMetricsForWebInspector> create() { return adoptRef(*new AdditionalNetworkLoadMetricsForWebInspector()); }
    Ref<AdditionalNetworkLoadMetricsForWebInspector> isolatedCopy() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static RefPtr<AdditionalNetworkLoadMetricsForWebInspector> decode(Decoder&);
    Ref<AdditionalNetworkLoadMetricsForWebInspector> isolatedCopy();

    NetworkLoadPriority priority { NetworkLoadPriority::Unknown };

    String remoteAddress;
    String connectionIdentifier;

    String tlsProtocol;
    String tlsCipher;

    HTTPHeaderMap requestHeaders;

    uint64_t requestHeaderBytesSent { std::numeric_limits<uint64_t>::max() };
    uint64_t responseHeaderBytesReceived { std::numeric_limits<uint64_t>::max() };
    uint64_t requestBodyBytesSent { std::numeric_limits<uint64_t>::max() };

    bool isProxyConnection { false };
private:
    AdditionalNetworkLoadMetricsForWebInspector() { }
};

#if PLATFORM(COCOA)
Box<NetworkLoadMetrics> copyTimingData(NSURLConnection *, const ResourceHandle&);
WEBCORE_EXPORT Box<NetworkLoadMetrics> copyTimingData(NSURLSessionTaskMetrics *incompleteMetrics, const NetworkLoadMetrics&);
#endif

template<class Encoder>
void NetworkLoadMetrics::encode(Encoder& encoder) const
{
    static_assert(Encoder::isIPCEncoder, "NetworkLoadMetrics should not be stored by the WTF::Persistence::Encoder");

    encoder << redirectStart;
    encoder << fetchStart;
    encoder << domainLookupStart;
    encoder << domainLookupEnd;
    encoder << connectStart;
    encoder << secureConnectionStart;
    encoder << connectEnd;
    encoder << requestStart;
    encoder << responseStart;
    encoder << responseEnd;
    encoder << workerStart;

    encoder << protocol;

    encoder << redirectCount;

    encoder << complete;
    encoder << cellular;
    encoder << expensive;
    encoder << constrained;
    encoder << multipath;
    encoder << isReusedConnection;
    encoder << failsTAOCheck;
    encoder << hasCrossOriginRedirect;

    encoder << privacyStance;

    encoder << responseBodyBytesReceived;
    encoder << responseBodyDecodedSize;
    
    if (additionalNetworkLoadMetricsForWebInspector) {
        encoder << true;
        encoder << *additionalNetworkLoadMetricsForWebInspector;
    } else
        encoder << false;
}

template<class Decoder>
std::optional<NetworkLoadMetrics> NetworkLoadMetrics::decode(Decoder& decoder)
{
    static_assert(Decoder::isIPCDecoder, "NetworkLoadMetrics should not be stored by the WTF::Persistence::Encoder");

    NetworkLoadMetrics metrics;
    if (!(decoder.decode(metrics.redirectStart)
        && decoder.decode(metrics.fetchStart)
        && decoder.decode(metrics.domainLookupStart)
        && decoder.decode(metrics.domainLookupEnd)
        && decoder.decode(metrics.connectStart)
        && decoder.decode(metrics.secureConnectionStart)
        && decoder.decode(metrics.connectEnd)
        && decoder.decode(metrics.requestStart)
        && decoder.decode(metrics.responseStart)
        && decoder.decode(metrics.responseEnd)
        && decoder.decode(metrics.workerStart)
        && decoder.decode(metrics.protocol)
        && decoder.decode(metrics.redirectCount)))
        return std::nullopt;
    
    std::optional<bool> complete;
    decoder >> complete;
    if (!complete)
        return std::nullopt;
    metrics.complete = WTFMove(*complete);

    std::optional<bool> cellular;
    decoder >> cellular;
    if (!cellular)
        return std::nullopt;
    metrics.cellular = WTFMove(*cellular);

    std::optional<bool> expensive;
    decoder >> expensive;
    if (!expensive)
        return std::nullopt;
    metrics.expensive = WTFMove(*expensive);

    std::optional<bool> constrained;
    decoder >> constrained;
    if (!constrained)
        return std::nullopt;
    metrics.constrained = WTFMove(*constrained);

    std::optional<bool> multipath;
    decoder >> multipath;
    if (!multipath)
        return std::nullopt;
    metrics.multipath = WTFMove(*multipath);

    std::optional<bool> isReusedConnection;
    decoder >> isReusedConnection;
    if (!isReusedConnection)
        return std::nullopt;
    metrics.isReusedConnection = WTFMove(*isReusedConnection);

    std::optional<bool> failsTAOCheck;
    decoder >> failsTAOCheck;
    if (!failsTAOCheck)
        return std::nullopt;
    metrics.failsTAOCheck = WTFMove(*failsTAOCheck);

    std::optional<bool> hasCrossOriginRedirect;
    decoder >> hasCrossOriginRedirect;
    if (!hasCrossOriginRedirect)
        return std::nullopt;
    metrics.hasCrossOriginRedirect = WTFMove(*hasCrossOriginRedirect);

    if (!(decoder.decode(metrics.privacyStance)
        && decoder.decode(metrics.responseBodyBytesReceived)
        && decoder.decode(metrics.responseBodyDecodedSize)))
        return std::nullopt;

    std::optional<bool> hasAdditionalNetworkLoadMetricsForWebInspector;
    decoder >> hasAdditionalNetworkLoadMetricsForWebInspector;
    if (!hasAdditionalNetworkLoadMetricsForWebInspector)
        return std::nullopt;
    if (*hasAdditionalNetworkLoadMetricsForWebInspector) {
        metrics.additionalNetworkLoadMetricsForWebInspector = AdditionalNetworkLoadMetricsForWebInspector::decode(decoder);
        if (!metrics.additionalNetworkLoadMetricsForWebInspector)
            return std::nullopt;
    }
    return metrics;
}

template<class Encoder>
void AdditionalNetworkLoadMetricsForWebInspector::encode(Encoder& encoder) const
{
    encoder << priority;
    encoder << remoteAddress;
    encoder << connectionIdentifier;

    encoder << tlsProtocol;
    encoder << tlsCipher;

    encoder << requestHeaders;

    encoder << requestHeaderBytesSent;
    encoder << responseHeaderBytesReceived;
    encoder << requestBodyBytesSent;

    encoder << isProxyConnection;
}

template<class Decoder>
RefPtr<AdditionalNetworkLoadMetricsForWebInspector> AdditionalNetworkLoadMetricsForWebInspector::decode(Decoder& decoder)
{
    std::optional<NetworkLoadPriority> priority;
    decoder >> priority;
    if (!priority)
        return nullptr;
    
    std::optional<String> remoteAddress;
    decoder >> remoteAddress;
    if (!remoteAddress)
        return nullptr;

    std::optional<String> connectionIdentifier;
    decoder >> connectionIdentifier;
    if (!connectionIdentifier)
        return nullptr;

    std::optional<String> tlsProtocol;
    decoder >> tlsProtocol;
    if (!tlsProtocol)
        return nullptr;

    std::optional<String> tlsCipher;
    decoder >> tlsCipher;
    if (!tlsCipher)
        return nullptr;

    std::optional<HTTPHeaderMap> requestHeaders;
    decoder >> requestHeaders;
    if (!requestHeaders)
        return nullptr;

    std::optional<uint64_t> requestHeaderBytesSent;
    decoder >> requestHeaderBytesSent;
    if (!requestHeaderBytesSent)
        return nullptr;

    std::optional<uint64_t> responseHeaderBytesReceived;
    decoder >> responseHeaderBytesReceived;
    if (!responseHeaderBytesReceived)
        return nullptr;

    std::optional<uint64_t> requestBodyBytesSent;
    decoder >> requestBodyBytesSent;
    if (!requestBodyBytesSent)
        return nullptr;

    std::optional<bool> isProxyConnection;
    decoder >> isProxyConnection;
    if (!isProxyConnection)
        return nullptr;

    auto decoded = AdditionalNetworkLoadMetricsForWebInspector::create();
    decoded->priority = WTFMove(*priority);
    decoded->remoteAddress = WTFMove(*remoteAddress);
    decoded->connectionIdentifier = WTFMove(*connectionIdentifier);
    decoded->tlsProtocol = WTFMove(*tlsProtocol);
    decoded->tlsCipher = WTFMove(*tlsCipher);
    decoded->requestHeaders = WTFMove(*requestHeaders);
    decoded->requestHeaderBytesSent = WTFMove(*requestHeaderBytesSent);
    decoded->responseHeaderBytesReceived = WTFMove(*responseHeaderBytesReceived);
    decoded->requestBodyBytesSent = WTFMove(*requestBodyBytesSent);
    decoded->isProxyConnection = WTFMove(*isProxyConnection);
    return decoded;
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::PrivacyStance> {
    using values = EnumValues<
        WebCore::PrivacyStance,
        WebCore::PrivacyStance::Unknown,
        WebCore::PrivacyStance::NotEligible,
        WebCore::PrivacyStance::Proxied,
        WebCore::PrivacyStance::Failed,
        WebCore::PrivacyStance::Direct
    >;
};

template<> struct EnumTraits<WebCore::NetworkLoadPriority> {
    using values = EnumValues<
        WebCore::NetworkLoadPriority,
        WebCore::NetworkLoadPriority::Low,
        WebCore::NetworkLoadPriority::Medium,
        WebCore::NetworkLoadPriority::High,
        WebCore::NetworkLoadPriority::Unknown
    >;
};

}
