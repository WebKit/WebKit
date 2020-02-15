/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2014-2017 Apple, Inc. All Rights Reserved.
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
#include <wtf/Optional.h>
#include <wtf/Seconds.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSDictionary;
#endif

namespace WebCore {

enum class NetworkLoadPriority : uint8_t {
    Low,
    Medium,
    High,
    Unknown,
};

class NetworkLoadMetrics {
public:
    NetworkLoadMetrics()
    {
        reset();
    }

    NetworkLoadMetrics isolatedCopy() const
    {
        NetworkLoadMetrics copy;

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

        copy.remoteAddress = remoteAddress.isolatedCopy();
        copy.connectionIdentifier = connectionIdentifier.isolatedCopy();
        copy.tlsProtocol = tlsProtocol.isolatedCopy();
        copy.tlsCipher = tlsCipher.isolatedCopy();
        copy.priority = priority;
        copy.requestHeaders = requestHeaders.isolatedCopy();

        copy.requestHeaderBytesSent = requestHeaderBytesSent;
        copy.requestBodyBytesSent = requestBodyBytesSent;
        copy.responseHeaderBytesReceived = responseHeaderBytesReceived;
        copy.responseBodyBytesReceived = responseBodyBytesReceived;
        copy.responseBodyDecodedSize = responseBodyDecodedSize;

        return copy;
    }

    void reset()
    {
        domainLookupStart = Seconds(-1);
        domainLookupEnd = Seconds(-1);
        connectStart = Seconds(-1);
        secureConnectionStart = Seconds(-1);
        connectEnd = Seconds(-1);
        requestStart = Seconds(0);
        responseStart = Seconds(0);
        responseEnd = Seconds(0);
        complete = false;
        protocol = String();
        clearNonTimingData();
    }

    void clearNonTimingData()
    {
        remoteAddress = String();
        connectionIdentifier = String();
        tlsProtocol = String();
        tlsCipher = String();
        priority = NetworkLoadPriority::Unknown;
        requestHeaders.clear();
        requestHeaderBytesSent = std::numeric_limits<uint32_t>::max();
        requestBodyBytesSent = std::numeric_limits<uint64_t>::max();
        responseHeaderBytesReceived = std::numeric_limits<uint32_t>::max();
        responseBodyBytesReceived = std::numeric_limits<uint64_t>::max();
        responseBodyDecodedSize = std::numeric_limits<uint64_t>::max();
    }

    bool operator==(const NetworkLoadMetrics& other) const
    {
        return domainLookupStart == other.domainLookupStart
            && domainLookupEnd == other.domainLookupEnd
            && connectStart == other.connectStart
            && secureConnectionStart == other.secureConnectionStart
            && connectEnd == other.connectEnd
            && requestStart == other.requestStart
            && responseStart == other.responseStart
            && responseEnd == other.responseEnd
            && complete == other.complete
            && protocol == other.protocol
            && remoteAddress == other.remoteAddress
            && connectionIdentifier == other.connectionIdentifier
            && tlsProtocol == other.tlsProtocol
            && tlsCipher == other.tlsCipher
            && priority == other.priority
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

    bool isComplete() const { return complete; }
    void markComplete() { complete = true; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, NetworkLoadMetrics&);

    // These should be treated as deltas to LoadTiming's fetchStart.
    // They should be in ascending order as listed here.
    Seconds domainLookupStart;     // -1 if no DNS.
    Seconds domainLookupEnd;       // -1 if no DNS.
    Seconds connectStart;          // -1 if reused connection.
    Seconds secureConnectionStart; // -1 if no secure connection.
    Seconds connectEnd;            // -1 if reused connection.
    Seconds requestStart;
    Seconds responseStart;
    Seconds responseEnd;

    // ALPN Protocol ID: https://w3c.github.io/resource-timing/#bib-RFC7301
    String protocol;

    String remoteAddress;
    String connectionIdentifier;

    String tlsProtocol;
    String tlsCipher;

    // Whether or not all of the properties (0 or otherwise) have been set.
    NetworkLoadPriority priority;
    bool complete { false };

    HTTPHeaderMap requestHeaders;

    uint64_t requestHeaderBytesSent;
    uint64_t responseHeaderBytesReceived;
    uint64_t requestBodyBytesSent;
    uint64_t responseBodyBytesReceived;
    uint64_t responseBodyDecodedSize;
};

#if PLATFORM(COCOA)
WEBCORE_EXPORT void copyTimingData(NSDictionary *timingData, NetworkLoadMetrics&);
#endif

template<class Encoder>
void NetworkLoadMetrics::encode(Encoder& encoder) const
{
    encoder << domainLookupStart;
    encoder << domainLookupEnd;
    encoder << connectStart;
    encoder << secureConnectionStart;
    encoder << connectEnd;
    encoder << requestStart;
    encoder << responseStart;
    encoder << responseEnd;
    encoder << complete;
    encoder << protocol;
    encoder << remoteAddress;
    encoder << connectionIdentifier;
    encoder << tlsProtocol;
    encoder << tlsCipher;
    encoder << priority;
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
    return decoder.decode(metrics.domainLookupStart)
        && decoder.decode(metrics.domainLookupEnd)
        && decoder.decode(metrics.connectStart)
        && decoder.decode(metrics.secureConnectionStart)
        && decoder.decode(metrics.connectEnd)
        && decoder.decode(metrics.requestStart)
        && decoder.decode(metrics.responseStart)
        && decoder.decode(metrics.responseEnd)
        && decoder.decode(metrics.complete)
        && decoder.decode(metrics.protocol)
        && decoder.decode(metrics.remoteAddress)
        && decoder.decode(metrics.connectionIdentifier)
        && decoder.decode(metrics.tlsProtocol)
        && decoder.decode(metrics.tlsCipher)
        && decoder.decode(metrics.priority)
        && decoder.decode(metrics.requestHeaders)
        && decoder.decode(metrics.requestHeaderBytesSent)
        && decoder.decode(metrics.requestBodyBytesSent)
        && decoder.decode(metrics.responseHeaderBytesReceived)
        && decoder.decode(metrics.responseBodyBytesReceived)
        && decoder.decode(metrics.responseBodyDecodedSize);
}

} // namespace WebCore

// NetworkLoadMetrics should not be stored by the WTF::Persistence::Decoder.
namespace WTF {
namespace Persistence {

template<> struct Coder<Optional<WebCore::NetworkLoadPriority>> {
    static NO_RETURN_DUE_TO_ASSERT void encode(Encoder&, const Optional<WebCore::NetworkLoadPriority>&)
    {
        ASSERT_NOT_REACHED();
    }

    static bool decode(Decoder&, Optional<WebCore::NetworkLoadPriority>&)
    {
        ASSERT_NOT_REACHED();
        return false;
    }
};

}} // namespace WTF::Persistence
