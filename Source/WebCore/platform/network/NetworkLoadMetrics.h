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
#include <wtf/persistence/PersistentCoder.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSURLConnection;
OBJC_CLASS NSURLResponse;
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

class NetworkLoadMetricsWithoutNonTimingData {
    WTF_MAKE_FAST_ALLOCATED(NetworkLoadMetricsWithoutNonTimingData);
public:
    bool isComplete() const { return complete; }
    void markComplete() { complete = true; }

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
    
    // ALPN Protocol ID: https://w3c.github.io/resource-timing/#bib-RFC7301
    String protocol;

    uint16_t redirectCount { 0 };

    // FIXME: These could all be made bit fields.
    bool complete { false };
    bool cellular { false };
    bool expensive { false };
    bool constrained { false };
    bool multipath { false };
    bool isReusedConnection { false };
    bool failsTAOCheck { false };
    bool hasCrossOriginRedirect { false };
};

class NetworkLoadMetrics : public NetworkLoadMetricsWithoutNonTimingData {
public:
    NetworkLoadMetrics()
        : NetworkLoadMetricsWithoutNonTimingData()
    {
    }

    NetworkLoadMetrics isolatedCopy() const
    {
        NetworkLoadMetrics copy;

        copy.redirectStart = redirectStart;
        copy.fetchStart = fetchStart;

        copy.domainLookupStart = domainLookupStart;
        copy.domainLookupEnd = domainLookupEnd;
        copy.connectStart = connectStart;
        copy.secureConnectionStart = secureConnectionStart;
        copy.connectEnd = connectEnd;
        copy.requestStart = requestStart;
        copy.responseStart = responseStart;
        copy.responseEnd = responseEnd;
        copy.complete = complete;
        copy.protocol = protocol.isolatedCopy();
        copy.redirectCount = redirectCount;
        copy.cellular = cellular;
        copy.expensive = expensive;
        copy.constrained = constrained;
        copy.multipath = multipath;
        copy.isReusedConnection = isReusedConnection;
        copy.failsTAOCheck = failsTAOCheck;
        copy.hasCrossOriginRedirect = hasCrossOriginRedirect;

        copy.remoteAddress = remoteAddress.isolatedCopy();
        copy.connectionIdentifier = connectionIdentifier.isolatedCopy();
        copy.tlsProtocol = tlsProtocol.isolatedCopy();
        copy.tlsCipher = tlsCipher.isolatedCopy();
        copy.priority = priority;
        copy.privacyStance = privacyStance;
        copy.requestHeaders = requestHeaders.isolatedCopy();

        copy.requestHeaderBytesSent = requestHeaderBytesSent;
        copy.requestBodyBytesSent = requestBodyBytesSent;
        copy.responseHeaderBytesReceived = responseHeaderBytesReceived;
        copy.responseBodyBytesReceived = responseBodyBytesReceived;
        copy.responseBodyDecodedSize = responseBodyDecodedSize;

        return copy;
    }

    bool operator==(const NetworkLoadMetrics& other) const
    {
        return redirectStart == other.redirectStart
            && fetchStart == other.fetchStart
            && domainLookupStart == other.domainLookupStart
            && domainLookupEnd == other.domainLookupEnd
            && connectStart == other.connectStart
            && secureConnectionStart == other.secureConnectionStart
            && connectEnd == other.connectEnd
            && requestStart == other.requestStart
            && responseStart == other.responseStart
            && responseEnd == other.responseEnd
            && complete == other.complete
            && cellular == other.cellular
            && expensive == other.expensive
            && constrained == other.constrained
            && multipath == other.multipath
            && isReusedConnection == other.isReusedConnection
            && failsTAOCheck == other.failsTAOCheck
            && hasCrossOriginRedirect == other.hasCrossOriginRedirect
            && protocol == other.protocol
            && redirectCount == other.redirectCount
            && remoteAddress == other.remoteAddress
            && connectionIdentifier == other.connectionIdentifier
            && tlsProtocol == other.tlsProtocol
            && tlsCipher == other.tlsCipher
            && priority == other.priority
            && privacyStance == other.privacyStance
            && requestHeaders == other.requestHeaders
            && requestHeaderBytesSent == other.requestHeaderBytesSent
            && requestBodyBytesSent == other.requestBodyBytesSent
            && responseHeaderBytesReceived == other.responseHeaderBytesReceived
            && responseBodyBytesReceived == other.responseBodyBytesReceived
            && responseBodyDecodedSize == other.responseBodyDecodedSize;
    }

    bool operator!=(const NetworkLoadMetrics& other) const
    {
        return !(*this == other);
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, NetworkLoadMetrics&);

    String remoteAddress;
    String connectionIdentifier;

    String tlsProtocol;
    String tlsCipher;

    NetworkLoadPriority priority { NetworkLoadPriority::Unknown };
    PrivacyStance privacyStance { PrivacyStance::Unknown };

    HTTPHeaderMap requestHeaders;

    uint64_t requestHeaderBytesSent { std::numeric_limits<uint64_t>::max() };
    uint64_t responseHeaderBytesReceived { std::numeric_limits<uint64_t>::max() };
    uint64_t requestBodyBytesSent { std::numeric_limits<uint64_t>::max() };
    uint64_t responseBodyBytesReceived { std::numeric_limits<uint64_t>::max() };
    uint64_t responseBodyDecodedSize { std::numeric_limits<uint64_t>::max() };
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
    encoder << complete;
    encoder << cellular;
    encoder << expensive;
    encoder << constrained;
    encoder << multipath;
    encoder << isReusedConnection;
    encoder << failsTAOCheck;
    encoder << hasCrossOriginRedirect;
    encoder << protocol;
    encoder << redirectCount;
    encoder << remoteAddress;
    encoder << connectionIdentifier;
    encoder << tlsProtocol;
    encoder << tlsCipher;
    encoder << priority;
    encoder << privacyStance;
    encoder << requestHeaders;
    encoder << requestHeaderBytesSent;
    encoder << requestBodyBytesSent;
    encoder << responseHeaderBytesReceived;
    encoder << responseBodyBytesReceived;
    encoder << responseBodyDecodedSize;
}

template<class Decoder>
bool NetworkLoadMetrics::decode(Decoder& decoder, NetworkLoadMetrics& metrics)
{
    static_assert(Decoder::isIPCDecoder, "NetworkLoadMetrics should not be stored by the WTF::Persistence::Encoder");

    return decoder.decode(metrics.redirectStart)
        && decoder.decode(metrics.fetchStart)
        && decoder.decode(metrics.domainLookupStart)
        && decoder.decode(metrics.domainLookupEnd)
        && decoder.decode(metrics.connectStart)
        && decoder.decode(metrics.secureConnectionStart)
        && decoder.decode(metrics.connectEnd)
        && decoder.decode(metrics.requestStart)
        && decoder.decode(metrics.responseStart)
        && decoder.decode(metrics.responseEnd)
        && decoder.decode(metrics.complete)
        && decoder.decode(metrics.cellular)
        && decoder.decode(metrics.expensive)
        && decoder.decode(metrics.constrained)
        && decoder.decode(metrics.multipath)
        && decoder.decode(metrics.isReusedConnection)
        && decoder.decode(metrics.failsTAOCheck)
        && decoder.decode(metrics.hasCrossOriginRedirect)
        && decoder.decode(metrics.protocol)
        && decoder.decode(metrics.redirectCount)
        && decoder.decode(metrics.remoteAddress)
        && decoder.decode(metrics.connectionIdentifier)
        && decoder.decode(metrics.tlsProtocol)
        && decoder.decode(metrics.tlsCipher)
        && decoder.decode(metrics.priority)
        && decoder.decode(metrics.privacyStance)
        && decoder.decode(metrics.requestHeaders)
        && decoder.decode(metrics.requestHeaderBytesSent)
        && decoder.decode(metrics.requestBodyBytesSent)
        && decoder.decode(metrics.responseHeaderBytesReceived)
        && decoder.decode(metrics.responseBodyBytesReceived)
        && decoder.decode(metrics.responseBodyDecodedSize);
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

}
