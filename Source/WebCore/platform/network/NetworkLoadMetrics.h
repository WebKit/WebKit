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

#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSDictionary;
#endif

namespace WebCore {

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
        protocol = String();
        complete = false;
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
            && protocol == other.protocol;
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

    // Whether or not all of the properties (0 or otherwise) have been set.
    bool complete { false };

    // ALPN Protocol ID: https://w3c.github.io/resource-timing/#bib-RFC7301
    String protocol;
};

#if PLATFORM(COCOA)
WEBCORE_EXPORT void copyTimingData(NSDictionary *timingData, NetworkLoadMetrics&);
#endif

#if PLATFORM(COCOA) && !HAVE(TIMINGDATAOPTIONS)
WEBCORE_EXPORT void setCollectsTimingData();
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
}

template<class Decoder>
bool NetworkLoadMetrics::decode(Decoder& decoder, NetworkLoadMetrics& timing)
{
    return decoder.decode(timing.domainLookupStart)
        && decoder.decode(timing.domainLookupEnd)
        && decoder.decode(timing.connectStart)
        && decoder.decode(timing.secureConnectionStart)
        && decoder.decode(timing.connectEnd)
        && decoder.decode(timing.requestStart)
        && decoder.decode(timing.responseStart)
        && decoder.decode(timing.responseEnd)
        && decoder.decode(timing.complete)
        && decoder.decode(timing.protocol);
}

}
