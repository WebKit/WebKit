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
#include "Performance.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "PerformanceEntry.h"
#include "PerformanceNavigation.h"
#include "PerformanceObserver.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceTiming.h"
#include "PerformanceUserTiming.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

Performance::Performance(ScriptExecutionContext& context, MonotonicTime timeOrigin)
    : ContextDestructionObserver(&context)
    , m_timeOrigin(timeOrigin)
    , m_performanceTimelineTaskQueue(context)
{
    ASSERT(m_timeOrigin);
}

Performance::~Performance()
{
}

void Performance::contextDestroyed()
{
    m_performanceTimelineTaskQueue.close();

    ContextDestructionObserver::contextDestroyed();
}

double Performance::now() const
{
    Seconds now = MonotonicTime::now() - m_timeOrigin;
    return reduceTimeResolution(now).milliseconds();
}

Seconds Performance::reduceTimeResolution(Seconds seconds)
{
    double resolution = (100_us).seconds();
    double reduced = std::floor(seconds.seconds() / resolution) * resolution;
    return Seconds(reduced);
}

PerformanceNavigation* Performance::navigation()
{
    if (!is<Document>(scriptExecutionContext()))
        return nullptr;

    ASSERT(isMainThread());
    if (!m_navigation)
        m_navigation = PerformanceNavigation::create(downcast<Document>(*scriptExecutionContext()).frame());
    return m_navigation.get();
}

PerformanceTiming* Performance::timing()
{
    if (!is<Document>(scriptExecutionContext()))
        return nullptr;

    ASSERT(isMainThread());
    if (!m_timing)
        m_timing = PerformanceTiming::create(downcast<Document>(*scriptExecutionContext()).frame());
    return m_timing.get();
}

Vector<RefPtr<PerformanceEntry>> Performance::getEntries() const
{
    Vector<RefPtr<PerformanceEntry>> entries;

    entries.appendVector(m_resourceTimingBuffer);

    if (m_userTiming) {
        entries.appendVector(m_userTiming->getMarks());
        entries.appendVector(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<RefPtr<PerformanceEntry>> Performance::getEntriesByType(const String& entryType) const
{
    Vector<RefPtr<PerformanceEntry>> entries;

    if (equalLettersIgnoringASCIICase(entryType, "resource"))
        entries.appendVector(m_resourceTimingBuffer);

    if (m_userTiming) {
        if (equalLettersIgnoringASCIICase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks());
        else if (equalLettersIgnoringASCIICase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures());
    }

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

    if (m_userTiming) {
        if (entryType.isNull() || equalLettersIgnoringASCIICase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks(name));
        if (entryType.isNull() || equalLettersIgnoringASCIICase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures(name));
    }

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
}

void Performance::addResourceTiming(ResourceTiming&& resourceTiming)
{
    RefPtr<PerformanceResourceTiming> entry = PerformanceResourceTiming::create(m_timeOrigin, WTFMove(resourceTiming));

    queueEntry(*entry);

    if (isResourceTimingBufferFull())
        return;

    m_resourceTimingBuffer.append(entry);

    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(eventNames().resourcetimingbufferfullEvent, true, false));
}

bool Performance::isResourceTimingBufferFull() const
{
    return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

ExceptionOr<void> Performance::mark(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = std::make_unique<UserTiming>(*this);

    auto result = m_userTiming->mark(markName);
    if (result.hasException())
        return result.releaseException();

    queueEntry(result.releaseReturnValue());

    return { };
}

void Performance::clearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = std::make_unique<UserTiming>(*this);
    m_userTiming->clearMarks(markName);
}

ExceptionOr<void> Performance::measure(const String& measureName, const String& startMark, const String& endMark)
{
    if (!m_userTiming)
        m_userTiming = std::make_unique<UserTiming>(*this);

    auto result = m_userTiming->measure(measureName, startMark, endMark);
    if (result.hasException())
        return result.releaseException();

    queueEntry(result.releaseReturnValue());

    return { };
}

void Performance::clearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = std::make_unique<UserTiming>(*this);
    m_userTiming->clearMeasures(measureName);
}

void Performance::removeAllObservers()
{
    for (auto& observer : m_observers)
        observer->disassociate();
    m_observers.clear();
}

void Performance::registerPerformanceObserver(PerformanceObserver& observer)
{
    m_observers.add(&observer);
}

void Performance::unregisterPerformanceObserver(PerformanceObserver& observer)
{
    m_observers.remove(&observer);
}

void Performance::queueEntry(PerformanceEntry& entry)
{
    bool shouldScheduleTask = false;
    for (auto& observer : m_observers) {
        if (observer->typeFilter().contains(entry.type())) {
            observer->queueEntry(entry);
            shouldScheduleTask = true;
        }
    }

    if (!shouldScheduleTask)
        return;

    if (m_performanceTimelineTaskQueue.hasPendingTasks())
        return;

    m_performanceTimelineTaskQueue.enqueueTask([this] () {
        Vector<RefPtr<PerformanceObserver>> observers;
        copyToVector(m_observers, observers);
        for (auto& observer : observers)
            observer->deliver();
    });
}

} // namespace WebCore
