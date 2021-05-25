/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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
#include "PerformanceResourceTiming.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "LoadTiming.h"
#include "PerformanceServerTiming.h"
#include "ResourceResponse.h"
#include "ResourceTiming.h"
#include <wtf/URL.h>

namespace WebCore {

static double monotonicTimeToDOMHighResTimeStamp(MonotonicTime timeOrigin, MonotonicTime timeStamp)
{
    ASSERT(timeStamp.secondsSinceEpoch().seconds() >= 0);
    if (!timeStamp || !timeOrigin)
        return 0;

    Seconds seconds = timeStamp - timeOrigin;
    return Performance::reduceTimeResolution(seconds).milliseconds();
}

static double entryStartTime(MonotonicTime timeOrigin, const ResourceTiming& resourceTiming)
{
    if (!resourceTiming.allowTimingDetails())
        return monotonicTimeToDOMHighResTimeStamp(timeOrigin, resourceTiming.loadTiming().fetchStart());

    return monotonicTimeToDOMHighResTimeStamp(timeOrigin, resourceTiming.loadTiming().startTime());
}

static double entryEndTime(MonotonicTime timeOrigin, const ResourceTiming& resourceTiming)
{
    if (!resourceTiming.allowTimingDetails())
        return entryStartTime(timeOrigin, resourceTiming);

    if (resourceTiming.networkLoadMetrics().isComplete()) {
        Seconds endTime = (resourceTiming.loadTiming().fetchStart() + resourceTiming.networkLoadMetrics().responseEnd) - timeOrigin;
        return Performance::reduceTimeResolution(endTime).milliseconds();
    }

    return monotonicTimeToDOMHighResTimeStamp(timeOrigin, resourceTiming.loadTiming().responseEnd());
}

Ref<PerformanceResourceTiming> PerformanceResourceTiming::create(MonotonicTime timeOrigin, ResourceTiming&& resourceTiming)
{
    return adoptRef(*new PerformanceResourceTiming(timeOrigin, WTFMove(resourceTiming)));
}

PerformanceResourceTiming::PerformanceResourceTiming(MonotonicTime timeOrigin, ResourceTiming&& resourceTiming)
    : PerformanceEntry(resourceTiming.url().string(), entryStartTime(timeOrigin, resourceTiming), entryEndTime(timeOrigin, resourceTiming))
    , m_timeOrigin(timeOrigin)
    , m_resourceTiming(WTFMove(resourceTiming))
    , m_serverTiming(m_resourceTiming.populateServerTiming())
{
}

PerformanceResourceTiming::~PerformanceResourceTiming() = default;

const String& PerformanceResourceTiming::nextHopProtocol() const
{
    return m_resourceTiming.networkLoadMetrics().protocol;
}

double PerformanceResourceTiming::workerStart() const
{
    // FIXME: <https://webkit.org/b/179377> Implement PerformanceResourceTiming.workerStart in ServiceWorkers
    return 0.0;
}

double PerformanceResourceTiming::redirectStart() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_resourceTiming.loadTiming().redirectStart());
}

double PerformanceResourceTiming::redirectEnd() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_resourceTiming.loadTiming().redirectEnd());
}

double PerformanceResourceTiming::fetchStart() const
{
    // fetchStart is a required property.
    ASSERT(m_resourceTiming.loadTiming().fetchStart());

    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_resourceTiming.loadTiming().fetchStart());
}

double PerformanceResourceTiming::domainLookupStart() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    if (m_resourceTiming.networkLoadMetrics().domainLookupStart <= 0_ms)
        return fetchStart();

    return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().domainLookupStart);
}

double PerformanceResourceTiming::domainLookupEnd() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    if (m_resourceTiming.networkLoadMetrics().domainLookupEnd <= 0_ms)
        return domainLookupStart();

    return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().domainLookupEnd);
}

double PerformanceResourceTiming::connectStart() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    if (m_resourceTiming.networkLoadMetrics().connectStart <= 0_ms)
        return domainLookupEnd();

    return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().connectStart);
}

double PerformanceResourceTiming::connectEnd() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    if (m_resourceTiming.networkLoadMetrics().connectEnd <= 0_ms)
        return connectStart();

    return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().connectEnd);
}

double PerformanceResourceTiming::secureConnectionStart() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    if (m_resourceTiming.networkLoadMetrics().secureConnectionStart == reusedTLSConnectionSentinel)
        return fetchStart();

    if (m_resourceTiming.networkLoadMetrics().secureConnectionStart <= 0_ms)
        return 0.0;

    return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().secureConnectionStart);
}

double PerformanceResourceTiming::requestStart() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    // requestStart is 0 when a network request is not made.
    if (m_resourceTiming.networkLoadMetrics().requestStart <= 0_ms)
        return connectEnd();

    return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().requestStart);
}

double PerformanceResourceTiming::responseStart() const
{
    if (!m_resourceTiming.allowTimingDetails())
        return 0.0;

    // responseStart is 0 when a network request is not made.
    if (m_resourceTiming.networkLoadMetrics().responseStart <= 0_ms)
        return requestStart();

    return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().responseStart);
}

double PerformanceResourceTiming::responseEnd() const
{
    // responseEnd is a required property.
    ASSERT(m_resourceTiming.networkLoadMetrics().isComplete() || m_resourceTiming.loadTiming().responseEnd());

    if (m_resourceTiming.networkLoadMetrics().isComplete()) {
        // responseEnd is 0 when a network request is not made.
        // This should mean all other properties are empty.
        if (m_resourceTiming.networkLoadMetrics().responseEnd <= 0_ms) {
            ASSERT(m_resourceTiming.networkLoadMetrics().responseStart <= 0_ms);
            ASSERT(m_resourceTiming.networkLoadMetrics().requestStart <= 0_ms);
            ASSERT(m_resourceTiming.networkLoadMetrics().requestStart <= 0_ms);
            ASSERT(m_resourceTiming.networkLoadMetrics().secureConnectionStart <= 0_ms);
            ASSERT(m_resourceTiming.networkLoadMetrics().connectEnd <= 0_ms);
            ASSERT(m_resourceTiming.networkLoadMetrics().connectStart <= 0_ms);
            ASSERT(m_resourceTiming.networkLoadMetrics().domainLookupEnd <= 0_ms);
            ASSERT(m_resourceTiming.networkLoadMetrics().domainLookupStart <= 0_ms);
            return responseStart();
        }

        return networkLoadTimeToDOMHighResTimeStamp(m_resourceTiming.networkLoadMetrics().responseEnd);
    }

    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_resourceTiming.loadTiming().responseEnd());
}

double PerformanceResourceTiming::networkLoadTimeToDOMHighResTimeStamp(Seconds delta) const
{
    ASSERT(delta);
    Seconds final = (m_resourceTiming.loadTiming().fetchStart() + delta) - m_timeOrigin;
    return Performance::reduceTimeResolution(final).milliseconds();
}

} // namespace WebCore
