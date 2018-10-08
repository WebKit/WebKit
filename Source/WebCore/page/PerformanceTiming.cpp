/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PerformanceTiming.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentTiming.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "LoadTiming.h"
#include "NetworkLoadMetrics.h"
#include "Performance.h"
#include "ResourceResponse.h"

namespace WebCore {

PerformanceTiming::PerformanceTiming(DOMWindow* window)
    : DOMWindowProperty(window)
{
}

unsigned long long PerformanceTiming::navigationStart() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->startTime());
}

unsigned long long PerformanceTiming::unloadEventStart() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    if (timing->hasCrossOriginRedirect() || !timing->hasSameOriginAsPreviousDocument())
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->unloadEventStart());
}

unsigned long long PerformanceTiming::unloadEventEnd() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    if (timing->hasCrossOriginRedirect() || !timing->hasSameOriginAsPreviousDocument())
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->unloadEventEnd());
}

unsigned long long PerformanceTiming::redirectStart() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    if (timing->hasCrossOriginRedirect())
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->redirectStart());
}

unsigned long long PerformanceTiming::redirectEnd() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    if (timing->hasCrossOriginRedirect())
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->redirectEnd());
}

unsigned long long PerformanceTiming::fetchStart() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->fetchStart());
}

unsigned long long PerformanceTiming::domainLookupStart() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return fetchStart();
    
    const NetworkLoadMetrics& timing = loader->response().deprecatedNetworkLoadMetrics();
    
    // This will be -1 when a DNS request is not performed.
    // Rather than exposing a special value that indicates no DNS, we "backfill" with fetchStart.
    if (timing.domainLookupStart < 0_ms)
        return fetchStart();

    return resourceLoadTimeRelativeToFetchStart(timing.domainLookupStart);
}

unsigned long long PerformanceTiming::domainLookupEnd() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return domainLookupStart();
    
    const NetworkLoadMetrics& timing = loader->response().deprecatedNetworkLoadMetrics();
    
    // This will be -1 when a DNS request is not performed.
    // Rather than exposing a special value that indicates no DNS, we "backfill" with domainLookupStart.
    if (timing.domainLookupEnd < 0_ms)
        return domainLookupStart();

    return resourceLoadTimeRelativeToFetchStart(timing.domainLookupEnd);
}

unsigned long long PerformanceTiming::connectStart() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return domainLookupEnd();

    const NetworkLoadMetrics& timing = loader->response().deprecatedNetworkLoadMetrics();
    
    // connectStart will be -1 when a network request is not made.
    // Rather than exposing a special value that indicates no new connection, we "backfill" with domainLookupEnd.
    Seconds connectStart = timing.connectStart;
    if (connectStart < 0_ms)
        return domainLookupEnd();

    // NetworkLoadMetrics's connect phase includes DNS, however Navigation Timing's
    // connect phase should not. So if there is DNS time, trim it from the start.
    if (timing.domainLookupEnd >= 0_ms && timing.domainLookupEnd > connectStart)
        connectStart = timing.domainLookupEnd;

    return resourceLoadTimeRelativeToFetchStart(connectStart);
}

unsigned long long PerformanceTiming::connectEnd() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return connectStart();

    const NetworkLoadMetrics& timing = loader->response().deprecatedNetworkLoadMetrics();
    
    // connectEnd will be -1 when a network request is not made.
    // Rather than exposing a special value that indicates no new connection, we "backfill" with connectStart.
    if (timing.connectEnd < 0_ms)
        return connectStart();

    return resourceLoadTimeRelativeToFetchStart(timing.connectEnd);
}

unsigned long long PerformanceTiming::secureConnectionStart() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return 0;

    const NetworkLoadMetrics& timing = loader->response().deprecatedNetworkLoadMetrics();
    
    if (timing.secureConnectionStart < 0_ms)
        return 0;

    return resourceLoadTimeRelativeToFetchStart(timing.secureConnectionStart);
}

unsigned long long PerformanceTiming::requestStart() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return connectEnd();
    
    const NetworkLoadMetrics& timing = loader->response().deprecatedNetworkLoadMetrics();
    
    ASSERT(timing.requestStart >= 0_ms);
    return resourceLoadTimeRelativeToFetchStart(timing.requestStart);
}

unsigned long long PerformanceTiming::responseStart() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return requestStart();

    const NetworkLoadMetrics& timing = loader->response().deprecatedNetworkLoadMetrics();
    
    ASSERT(timing.responseStart >= 0_ms);
    return resourceLoadTimeRelativeToFetchStart(timing.responseStart);
}

unsigned long long PerformanceTiming::responseEnd() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->responseEnd());
}

unsigned long long PerformanceTiming::domLoading() const
{
    const DocumentTiming* timing = documentTiming();
    if (!timing)
        return fetchStart();

    return monotonicTimeToIntegerMilliseconds(timing->domLoading);
}

unsigned long long PerformanceTiming::domInteractive() const
{
    const DocumentTiming* timing = documentTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domInteractive);
}

unsigned long long PerformanceTiming::domContentLoadedEventStart() const
{
    const DocumentTiming* timing = documentTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domContentLoadedEventStart);
}

unsigned long long PerformanceTiming::domContentLoadedEventEnd() const
{
    const DocumentTiming* timing = documentTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domContentLoadedEventEnd);
}

unsigned long long PerformanceTiming::domComplete() const
{
    const DocumentTiming* timing = documentTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domComplete);
}

unsigned long long PerformanceTiming::loadEventStart() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->loadEventStart());
}

unsigned long long PerformanceTiming::loadEventEnd() const
{
    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->loadEventEnd());
}

DocumentLoader* PerformanceTiming::documentLoader() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    return frame->loader().documentLoader();
}

const DocumentTiming* PerformanceTiming::documentTiming() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    Document* document = frame->document();
    if (!document)
        return nullptr;

    return &document->timing();
}

LoadTiming* PerformanceTiming::loadTiming() const
{
    DocumentLoader* loader = documentLoader();
    if (!loader)
        return nullptr;

    return &loader->timing();
}

unsigned long long PerformanceTiming::resourceLoadTimeRelativeToFetchStart(Seconds delta) const
{
    ASSERT(delta >= 0_ms);

    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    WallTime fetchStart = timing->monotonicTimeToPseudoWallTime(timing->fetchStart());
    WallTime combined = fetchStart + delta;
    Seconds reduced = Performance::reduceTimeResolution(combined.secondsSinceEpoch());
    return static_cast<unsigned long long>(reduced.milliseconds());
}

unsigned long long PerformanceTiming::monotonicTimeToIntegerMilliseconds(MonotonicTime timeStamp) const
{
    ASSERT(timeStamp.secondsSinceEpoch().seconds() >= 0);

    LoadTiming* timing = loadTiming();
    if (!timing)
        return 0;

    WallTime wallTime = timing->monotonicTimeToPseudoWallTime(timeStamp);
    Seconds reduced = Performance::reduceTimeResolution(wallTime.secondsSinceEpoch());
    return static_cast<unsigned long long>(reduced.milliseconds());
}

} // namespace WebCore
