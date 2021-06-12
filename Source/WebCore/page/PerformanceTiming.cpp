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
#include "DocumentEventTiming.h"
#include "DocumentLoadTiming.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
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
    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->startTime());
}

unsigned long long PerformanceTiming::unloadEventStart() const
{
    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    auto* metrics = networkLoadMetrics();
    if (!metrics)
        return 0;

    if (metrics->hasCrossOriginRedirect || !timing->hasSameOriginAsPreviousDocument())
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->unloadEventStart());
}

unsigned long long PerformanceTiming::unloadEventEnd() const
{
    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    auto* metrics = networkLoadMetrics();
    if (!metrics)
        return 0;

    if (metrics->hasCrossOriginRedirect || !timing->hasSameOriginAsPreviousDocument())
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->unloadEventEnd());
}

unsigned long long PerformanceTiming::redirectStart() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics
        || metrics->hasCrossOriginRedirect
        || !metrics->redirectCount)
        return 0;

    return monotonicTimeToIntegerMilliseconds(metrics->redirectStart);
}

unsigned long long PerformanceTiming::redirectEnd() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics
        || metrics->hasCrossOriginRedirect
        || !metrics->redirectCount)
        return 0;

    return monotonicTimeToIntegerMilliseconds(metrics->fetchStart);
}

unsigned long long PerformanceTiming::fetchStart() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics)
        return 0;

    return monotonicTimeToIntegerMilliseconds(metrics->fetchStart);
}

unsigned long long PerformanceTiming::domainLookupStart() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->domainLookupStart)
        return fetchStart();

    return monotonicTimeToIntegerMilliseconds(metrics->domainLookupStart);
}

unsigned long long PerformanceTiming::domainLookupEnd() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->domainLookupEnd)
        return domainLookupStart();

    return monotonicTimeToIntegerMilliseconds(metrics->domainLookupEnd);
}

unsigned long long PerformanceTiming::connectStart() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->connectStart
        || metrics->domainLookupEnd.secondsSinceEpoch() > metrics->connectStart.secondsSinceEpoch())
        return domainLookupEnd();

    return monotonicTimeToIntegerMilliseconds(metrics->connectStart);
}

unsigned long long PerformanceTiming::connectEnd() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->connectEnd)
        return connectStart();

    return monotonicTimeToIntegerMilliseconds(metrics->connectEnd);
}

unsigned long long PerformanceTiming::secureConnectionStart() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics)
        return connectEnd();

    if (!metrics->secureConnectionStart
        || metrics->secureConnectionStart == reusedTLSConnectionSentinel)
        return 0;

    return monotonicTimeToIntegerMilliseconds(metrics->secureConnectionStart);
}

unsigned long long PerformanceTiming::requestStart() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->requestStart)
        return connectEnd();
    
    return monotonicTimeToIntegerMilliseconds(metrics->requestStart);
}

unsigned long long PerformanceTiming::responseStart() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->responseStart)
        return requestStart();

    return monotonicTimeToIntegerMilliseconds(metrics->responseStart);
}

unsigned long long PerformanceTiming::responseEnd() const
{
    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->responseEnd)
        return responseStart();

    return monotonicTimeToIntegerMilliseconds(metrics->responseEnd);
}

unsigned long long PerformanceTiming::domLoading() const
{
    auto* timing = documentEventTiming();
    if (!timing)
        return fetchStart();

    return monotonicTimeToIntegerMilliseconds(timing->domLoading);
}

unsigned long long PerformanceTiming::domInteractive() const
{
    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domInteractive);
}

unsigned long long PerformanceTiming::domContentLoadedEventStart() const
{
    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domContentLoadedEventStart);
}

unsigned long long PerformanceTiming::domContentLoadedEventEnd() const
{
    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domContentLoadedEventEnd);
}

unsigned long long PerformanceTiming::domComplete() const
{
    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->domComplete);
}

unsigned long long PerformanceTiming::loadEventStart() const
{
    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->loadEventStart());
}

unsigned long long PerformanceTiming::loadEventEnd() const
{
    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    return monotonicTimeToIntegerMilliseconds(timing->loadEventEnd());
}

const DocumentLoader* PerformanceTiming::documentLoader() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    return frame->loader().documentLoader();
}

const DocumentEventTiming* PerformanceTiming::documentEventTiming() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    auto* document = frame->document();
    if (!document)
        return nullptr;

    return &document->eventTiming();
}

const DocumentLoadTiming* PerformanceTiming::documentLoadTiming() const
{
    auto* loader = documentLoader();
    if (!loader)
        return nullptr;

    return &loader->timing();
}

const NetworkLoadMetrics* PerformanceTiming::networkLoadMetrics() const
{
    auto* loader = documentLoader();
    if (!loader)
        return nullptr;
    return loader->response().deprecatedNetworkLoadMetricsOrNull();
}

unsigned long long PerformanceTiming::monotonicTimeToIntegerMilliseconds(MonotonicTime timeStamp) const
{
    ASSERT(timeStamp.secondsSinceEpoch().seconds() >= 0);
    if (!timeStamp)
        return 0;
    Seconds reduced = Performance::reduceTimeResolution(timeStamp.approximateWallTime().secondsSinceEpoch());
    return static_cast<unsigned long long>(reduced.milliseconds());
}

} // namespace WebCore
