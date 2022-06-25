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
    if (m_navigationStart)
        return m_navigationStart;

    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    m_navigationStart = monotonicTimeToIntegerMilliseconds(timing->startTime());
    return m_navigationStart;
}

unsigned long long PerformanceTiming::unloadEventStart() const
{
    if (m_unloadEventStart)
        return m_unloadEventStart;

    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    auto* metrics = networkLoadMetrics();
    if (!metrics)
        return 0;

    if (metrics->failsTAOCheck || !timing->hasSameOriginAsPreviousDocument())
        return 0;

    m_unloadEventStart = monotonicTimeToIntegerMilliseconds(timing->unloadEventStart());
    return m_unloadEventStart;
}

unsigned long long PerformanceTiming::unloadEventEnd() const
{
    if (m_unloadEventEnd)
        return m_unloadEventEnd;

    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    auto* metrics = networkLoadMetrics();
    if (!metrics)
        return 0;

    if (metrics->failsTAOCheck || !timing->hasSameOriginAsPreviousDocument())
        return 0;

    m_unloadEventEnd = monotonicTimeToIntegerMilliseconds(timing->unloadEventEnd());
    return m_unloadEventEnd;
}

unsigned long long PerformanceTiming::redirectStart() const
{
    if (m_redirectStart)
        return m_redirectStart;

    auto* metrics = networkLoadMetrics();
    if (!metrics
        || metrics->failsTAOCheck
        || !metrics->redirectCount)
        return 0;

    m_redirectStart = monotonicTimeToIntegerMilliseconds(metrics->redirectStart);
    return m_redirectStart;
}

unsigned long long PerformanceTiming::redirectEnd() const
{
    if (m_redirectEnd)
        return m_redirectEnd;

    auto* metrics = networkLoadMetrics();
    if (!metrics
        || metrics->failsTAOCheck
        || !metrics->redirectCount)
        return 0;

    m_redirectEnd = monotonicTimeToIntegerMilliseconds(metrics->fetchStart);
    return m_redirectEnd;
}

unsigned long long PerformanceTiming::fetchStart() const
{
    if (m_fetchStart)
        return m_fetchStart;

    auto* metrics = networkLoadMetrics();
    if (metrics)
        m_fetchStart = monotonicTimeToIntegerMilliseconds(metrics->fetchStart);

    if (!m_fetchStart) {
        if (auto* timing = documentLoadTiming())
            m_fetchStart = monotonicTimeToIntegerMilliseconds(timing->startTime());
    }

    return m_fetchStart;
}

unsigned long long PerformanceTiming::domainLookupStart() const
{
    if (m_domainLookupStart)
        return m_domainLookupStart;

    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->domainLookupStart)
        return fetchStart();

    m_domainLookupStart = monotonicTimeToIntegerMilliseconds(metrics->domainLookupStart);
    return m_domainLookupStart;
}

unsigned long long PerformanceTiming::domainLookupEnd() const
{
    if (m_domainLookupEnd)
        return m_domainLookupEnd;

    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->domainLookupEnd)
        return domainLookupStart();

    m_domainLookupEnd = monotonicTimeToIntegerMilliseconds(metrics->domainLookupEnd);
    return m_domainLookupEnd;
}

unsigned long long PerformanceTiming::connectStart() const
{
    if (m_connectStart)
        return m_connectStart;

    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->connectStart
        || metrics->domainLookupEnd.secondsSinceEpoch() > metrics->connectStart.secondsSinceEpoch())
        return domainLookupEnd();

    m_connectStart = monotonicTimeToIntegerMilliseconds(metrics->connectStart);
    return m_connectStart;
}

unsigned long long PerformanceTiming::connectEnd() const
{
    if (m_connectEnd)
        return m_connectEnd;

    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->connectEnd)
        return connectStart();

    m_connectEnd = monotonicTimeToIntegerMilliseconds(metrics->connectEnd);
    return m_connectEnd;
}

unsigned long long PerformanceTiming::secureConnectionStart() const
{
    if (m_secureConnectionStart)
        return m_secureConnectionStart;

    auto* metrics = networkLoadMetrics();
    if (!metrics)
        return connectEnd();

    if (!metrics->secureConnectionStart
        || metrics->secureConnectionStart == reusedTLSConnectionSentinel)
        return 0;

    m_secureConnectionStart = monotonicTimeToIntegerMilliseconds(metrics->secureConnectionStart);
    return m_secureConnectionStart;
}

unsigned long long PerformanceTiming::requestStart() const
{
    if (m_requestStart)
        return m_requestStart;

    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->requestStart)
        return connectEnd();
    
    m_requestStart = monotonicTimeToIntegerMilliseconds(metrics->requestStart);
    return m_requestStart;
}

unsigned long long PerformanceTiming::responseStart() const
{
    if (m_responseStart)
        return m_responseStart;

    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->responseStart)
        return requestStart();

    m_responseStart = monotonicTimeToIntegerMilliseconds(metrics->responseStart);
    return m_responseStart;
}

unsigned long long PerformanceTiming::responseEnd() const
{
    if (m_responseEnd)
        return m_responseEnd;

    auto* metrics = networkLoadMetrics();
    if (!metrics || !metrics->responseEnd)
        return responseStart();

    m_responseEnd = monotonicTimeToIntegerMilliseconds(metrics->responseEnd);
    return m_responseEnd;
}

unsigned long long PerformanceTiming::domLoading() const
{
    if (m_domLoading)
        return m_domLoading;

    auto* timing = documentEventTiming();
    if (!timing)
        return fetchStart();

    m_domLoading = monotonicTimeToIntegerMilliseconds(timing->domLoading);
    return m_domLoading;
}

unsigned long long PerformanceTiming::domInteractive() const
{
    if (m_domInteractive)
        return m_domInteractive;

    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    m_domInteractive = monotonicTimeToIntegerMilliseconds(timing->domInteractive);
    return m_domInteractive;
}

unsigned long long PerformanceTiming::domContentLoadedEventStart() const
{
    if (m_domContentLoadedEventStart)
        return m_domContentLoadedEventStart;

    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    m_domContentLoadedEventStart = monotonicTimeToIntegerMilliseconds(timing->domContentLoadedEventStart);
    return m_domContentLoadedEventStart;
}

unsigned long long PerformanceTiming::domContentLoadedEventEnd() const
{
    if (m_domContentLoadedEventEnd)
        return m_domContentLoadedEventEnd;

    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    m_domContentLoadedEventEnd = monotonicTimeToIntegerMilliseconds(timing->domContentLoadedEventEnd);
    return m_domContentLoadedEventEnd;
}

unsigned long long PerformanceTiming::domComplete() const
{
    if (m_domComplete)
        return m_domComplete;

    auto* timing = documentEventTiming();
    if (!timing)
        return 0;

    m_domComplete = monotonicTimeToIntegerMilliseconds(timing->domComplete);
    return m_domComplete;
}

unsigned long long PerformanceTiming::loadEventStart() const
{
    if (m_loadEventStart)
        return m_loadEventStart;

    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    m_loadEventStart = monotonicTimeToIntegerMilliseconds(timing->loadEventStart());
    return m_loadEventStart;
}

unsigned long long PerformanceTiming::loadEventEnd() const
{
    if (m_loadEventEnd)
        return m_loadEventEnd;

    auto* timing = documentLoadTiming();
    if (!timing)
        return 0;

    m_loadEventEnd = monotonicTimeToIntegerMilliseconds(timing->loadEventEnd());
    return m_loadEventEnd;
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
