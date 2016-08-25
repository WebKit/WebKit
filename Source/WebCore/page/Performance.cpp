/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_TIMING)
#include "Performance.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "Frame.h"
#include "PerformanceEntry.h"
#include "PerformanceNavigation.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceTiming.h"
#include "PerformanceUserTiming.h"
#include "ResourceResponse.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static const size_t defaultResourceTimingBufferSize = 150;

Performance::Performance(Frame& frame)
    : DOMWindowProperty(&frame)
    , m_resourceTimingBufferSize(defaultResourceTimingBufferSize)
    , m_referenceTime(frame.document()->loader() ? frame.document()->loader()->timing().referenceMonotonicTime() : monotonicallyIncreasingTime())
#if ENABLE(USER_TIMING)
    , m_userTiming(nullptr)
#endif // ENABLE(USER_TIMING)
{
    ASSERT(m_referenceTime);
}

Performance::~Performance()
{
}

ScriptExecutionContext* Performance::scriptExecutionContext() const
{
    if (!frame())
        return nullptr;
    return frame()->document();
}

PerformanceNavigation* Performance::navigation() const
{
    if (!m_navigation)
        m_navigation = PerformanceNavigation::create(m_frame);

    return m_navigation.get();
}

PerformanceTiming* Performance::timing() const
{
    if (!m_timing)
        m_timing = PerformanceTiming::create(m_frame);

    return m_timing.get();
}

Vector<RefPtr<PerformanceEntry>> Performance::getEntries() const
{
    Vector<RefPtr<PerformanceEntry>> entries;

    entries.appendVector(m_resourceTimingBuffer);

#if ENABLE(USER_TIMING)
    if (m_userTiming) {
        entries.appendVector(m_userTiming->getMarks());
        entries.appendVector(m_userTiming->getMeasures());
    }
#endif // ENABLE(USER_TIMING)

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<RefPtr<PerformanceEntry>> Performance::getEntriesByType(const String& entryType) const
{
    Vector<RefPtr<PerformanceEntry>> entries;

    if (equalLettersIgnoringASCIICase(entryType, "resource")) {
        for (auto& resource : m_resourceTimingBuffer)
            entries.append(resource);
    }

#if ENABLE(USER_TIMING)
    if (m_userTiming) {
        if (equalLettersIgnoringASCIICase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks());
        else if (equalLettersIgnoringASCIICase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures());
    }
#endif

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<RefPtr<PerformanceEntry>> Performance::getEntriesByName(const String& name, const String& entryType) const
{
    Vector<RefPtr<PerformanceEntry>> entries;

    if (entryType.isNull() || equalLettersIgnoringASCIICase(entryType, "resource")) {
        for (auto& resource : m_resourceTimingBuffer) {
            if (resource->name() == name)
                entries.append(resource);
        }
    }

#if ENABLE(USER_TIMING)
    if (m_userTiming) {
        if (entryType.isNull() || equalLettersIgnoringASCIICase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks(name));
        if (entryType.isNull() || equalLettersIgnoringASCIICase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures(name));
    }
#endif

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

void Performance::clearResourceTimings()
{
    m_resourceTimingBuffer.clear();
}

void Performance::setResourceTimingBufferSize(unsigned size)
{
    m_resourceTimingBufferSize = size;
    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(eventNames().resourcetimingbufferfullEvent, false, false));
}

void Performance::addResourceTiming(const String& initiatorName, Document* initiatorDocument, const URL& originalURL, const ResourceResponse& response, LoadTiming loadTiming)
{
    if (isResourceTimingBufferFull())
        return;

    RefPtr<PerformanceEntry> entry = PerformanceResourceTiming::create(initiatorName, originalURL, response, loadTiming, initiatorDocument);

    m_resourceTimingBuffer.append(entry);

    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(eventNames().resourcetimingbufferfullEvent, false, false));
}

bool Performance::isResourceTimingBufferFull()
{
    return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

#if ENABLE(USER_TIMING)
void Performance::webkitMark(const String& markName, ExceptionCode& ec)
{
    ec = 0;
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->mark(markName, ec);
}

void Performance::webkitClearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMarks(markName);
}

void Performance::webkitMeasure(const String& measureName, const String& startMark, const String& endMark, ExceptionCode& ec)
{
    ec = 0;
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->measure(measureName, startMark, endMark, ec);
}

void Performance::webkitClearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMeasures(measureName);
}

#endif // ENABLE(USER_TIMING)

double Performance::now() const
{
    double nowSeconds = monotonicallyIncreasingTime() - m_referenceTime;
    return 1000.0 * reduceTimeResolution(nowSeconds);
}

double Performance::reduceTimeResolution(double seconds)
{
    const double resolutionSeconds = 0.000005;
    return floor(seconds / resolutionSeconds) * resolutionSeconds;
}

} // namespace WebCore

#endif // ENABLE(WEB_TIMING)
