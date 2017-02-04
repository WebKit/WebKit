/*
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

#if ENABLE(WEB_TIMING)

#include "Document.h"
#include "DocumentLoader.h"
#include "HTTPHeaderNames.h"
#include "LoadTiming.h"
#include "URL.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include <wtf/Vector.h>

namespace WebCore {

static double monotonicTimeToDOMHighResTimeStamp(MonotonicTime timeOrigin, MonotonicTime timeStamp)
{
    ASSERT(timeStamp.secondsSinceEpoch().seconds() >= 0);
    if (!timeStamp || !timeOrigin)
        return 0;

    Seconds seconds = timeStamp - timeOrigin;
    return Performance::reduceTimeResolution(seconds).milliseconds();
}

static bool passesTimingAllowCheck(const ResourceResponse& response, const SecurityOrigin& initiatorSecurityOrigin)
{
    Ref<SecurityOrigin> resourceOrigin = SecurityOrigin::create(response.url());
    if (resourceOrigin->isSameSchemeHostPort(initiatorSecurityOrigin))
        return true;

    const String& timingAllowOriginString = response.httpHeaderField(HTTPHeaderName::TimingAllowOrigin);
    if (timingAllowOriginString.isEmpty() || equalLettersIgnoringASCIICase(timingAllowOriginString, "null"))
        return false;

    if (timingAllowOriginString == "*")
        return true;

    const String& securityOrigin = initiatorSecurityOrigin.toString();
    Vector<String> timingAllowOrigins;
    timingAllowOriginString.split(' ', timingAllowOrigins);
    for (auto& origin : timingAllowOrigins) {
        if (origin == securityOrigin)
            return true;
    }

    return false;
}

PerformanceResourceTiming::PerformanceResourceTiming(const AtomicString& initiatorType, const URL& originalURL, MonotonicTime timeOrigin, const ResourceResponse& response, const SecurityOrigin& initiatorSecurityOrigin, LoadTiming loadTiming)
    : PerformanceEntry(PerformanceEntry::Type::Resource, originalURL.string(), ASCIILiteral("resource"), monotonicTimeToDOMHighResTimeStamp(timeOrigin, loadTiming.startTime()), monotonicTimeToDOMHighResTimeStamp(timeOrigin, loadTiming.responseEnd()))
    , m_initiatorType(initiatorType)
    , m_timeOrigin(timeOrigin)
    , m_timing(response.networkLoadTiming())
    , m_loadTiming(loadTiming)
    , m_shouldReportDetails(passesTimingAllowCheck(response, initiatorSecurityOrigin))
{
}

PerformanceResourceTiming::~PerformanceResourceTiming()
{
}

double PerformanceResourceTiming::workerStart() const
{
    return 0.0;
}

double PerformanceResourceTiming::redirectStart() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_loadTiming.redirectStart());
}

double PerformanceResourceTiming::redirectEnd() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_loadTiming.redirectEnd());
}

double PerformanceResourceTiming::fetchStart() const
{
    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_loadTiming.fetchStart());
}

double PerformanceResourceTiming::domainLookupStart() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    if (m_timing.domainLookupStart <= 0)
        return fetchStart();

    return networkLoadTimeToDOMHighResTimeStamp(m_timing.domainLookupStart);
}

double PerformanceResourceTiming::domainLookupEnd() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    if (m_timing.domainLookupEnd <= 0)
        return domainLookupStart();

    return networkLoadTimeToDOMHighResTimeStamp(m_timing.domainLookupEnd);
}

double PerformanceResourceTiming::connectStart() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    // connectStart will be -1 when a network request is not made.
    if (m_timing.connectStart <= 0)
        return domainLookupEnd();

    // connectStart includes any DNS time, so we may need to trim that off.
    double connectStart = m_timing.connectStart;
    if (m_timing.domainLookupEnd >= 0)
        connectStart = m_timing.domainLookupEnd;

    return networkLoadTimeToDOMHighResTimeStamp(connectStart);
}

double PerformanceResourceTiming::connectEnd() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    // connectStart will be -1 when a network request is not made.
    if (m_timing.connectEnd <= 0)
        return connectStart();

    return networkLoadTimeToDOMHighResTimeStamp(m_timing.connectEnd);
}

double PerformanceResourceTiming::secureConnectionStart() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    if (m_timing.secureConnectionStart < 0) // Secure connection not negotiated.
        return 0.0;

    return networkLoadTimeToDOMHighResTimeStamp(m_timing.secureConnectionStart);
}

double PerformanceResourceTiming::requestStart() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    return networkLoadTimeToDOMHighResTimeStamp(m_timing.requestStart);
}

double PerformanceResourceTiming::responseStart() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    return networkLoadTimeToDOMHighResTimeStamp(m_timing.responseStart);
}

double PerformanceResourceTiming::responseEnd() const
{
    if (!m_shouldReportDetails)
        return 0.0;

    return monotonicTimeToDOMHighResTimeStamp(m_timeOrigin, m_loadTiming.responseEnd());
}

double PerformanceResourceTiming::networkLoadTimeToDOMHighResTimeStamp(double deltaMilliseconds) const
{
    if (!deltaMilliseconds)
        return 0.0;

    MonotonicTime combined = m_loadTiming.fetchStart() + Seconds::fromMilliseconds(deltaMilliseconds);
    Seconds delta = combined - m_timeOrigin;
    return Performance::reduceTimeResolution(delta).milliseconds();
}

} // namespace WebCore

#endif // ENABLE(WEB_TIMING)
