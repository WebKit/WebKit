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
#include "PerformancePaintTiming.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceTiming.h"
#include "PerformanceUserTiming.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Performance);

Performance::Performance(ScriptExecutionContext* context, MonotonicTime timeOrigin)
    : ContextDestructionObserver(context)
    , m_resourceTimingBufferFullTimer(*this, &Performance::resourceTimingBufferFullTimerFired)
    , m_timeOrigin(timeOrigin)
    , m_performanceTimelineTaskQueue(context)
{
    ASSERT(m_timeOrigin);
    ASSERT(context || m_performanceTimelineTaskQueue.isClosed());
}

Performance::~Performance() = default;

void Performance::contextDestroyed()
{
    m_performanceTimelineTaskQueue.close();
    m_resourceTimingBufferFullTimer.stop();
    ContextDestructionObserver::contextDestroyed();
}

DOMHighResTimeStamp Performance::now() const
{
    return nowInReducedResolutionSeconds().milliseconds();
}

ReducedResolutionSeconds Performance::nowInReducedResolutionSeconds() const
{
    Seconds now = MonotonicTime::now() - m_timeOrigin;
    return reduceTimeResolution(now);
}

Seconds Performance::reduceTimeResolution(Seconds seconds)
{
    double resolution = (1000_us).seconds();
    double reduced = std::floor(seconds.seconds() / resolution) * resolution;
    return Seconds(reduced);
}

DOMHighResTimeStamp Performance::relativeTimeFromTimeOriginInReducedResolution(MonotonicTime timestamp) const
{
    Seconds seconds = timestamp - m_timeOrigin;
    return reduceTimeResolution(seconds).milliseconds();
}

PerformanceNavigation* Performance::navigation()
{
    if (!is<Document>(scriptExecutionContext()))
        return nullptr;

    ASSERT(isMainThread());
    if (!m_navigation)
        m_navigation = PerformanceNavigation::create(downcast<Document>(*scriptExecutionContext()).domWindow());
    return m_navigation.get();
}

PerformanceTiming* Performance::timing()
{
    if (!is<Document>(scriptExecutionContext()))
        return nullptr;

    ASSERT(isMainThread());
    if (!m_timing)
        m_timing = PerformanceTiming::create(downcast<Document>(*scriptExecutionContext()).domWindow());
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

    if (m_firstContentfulPaint)
        entries.append(m_firstContentfulPaint);

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<RefPtr<PerformanceEntry>> Performance::getEntriesByType(const String& entryType) const
{
    Vector<RefPtr<PerformanceEntry>> entries;

    if (equalLettersIgnoringASCIICase(entryType, "resource"))
        entries.appendVector(m_resourceTimingBuffer);

    if (m_firstContentfulPaint && equalLettersIgnoringASCIICase(entryType, "paint"))
        entries.append(m_firstContentfulPaint);

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

    if (m_firstContentfulPaint && (entryType.isNull() || equalLettersIgnoringASCIICase(entryType, "paint")) && name == "first-contentful-paint")
        entries.append(m_firstContentfulPaint);

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
    m_resourceTimingBufferFullFlag = false;
}

void Performance::setResourceTimingBufferSize(unsigned size)
{
    m_resourceTimingBufferSize = size;
    m_resourceTimingBufferFullFlag = false;
}

void Performance::reportFirstContentfulPaint()
{
    ASSERT(!m_firstContentfulPaint);
    m_firstContentfulPaint = PerformancePaintTiming::createFirstContentfulPaint(now());
    queueEntry(*m_firstContentfulPaint);
}

void Performance::addResourceTiming(ResourceTiming&& resourceTiming)
{
    ASSERT(scriptExecutionContext());

    auto entry = PerformanceResourceTiming::create(m_timeOrigin, WTFMove(resourceTiming));

    if (m_waitingForBackupBufferToBeProcessed) {
        m_backupResourceTimingBuffer.append(WTFMove(entry));
        return;
    }

    if (m_resourceTimingBufferFullFlag) {
        // We fired resourcetimingbufferfull event but the author script didn't clear the buffer.
        // Notify performance observers but don't add it to the buffer.
        queueEntry(entry.get());
        return;
    }

    if (isResourceTimingBufferFull()) {
        ASSERT(!m_resourceTimingBufferFullTimer.isActive());
        m_backupResourceTimingBuffer.append(WTFMove(entry));
        m_waitingForBackupBufferToBeProcessed = true;
        m_resourceTimingBufferFullTimer.startOneShot(0_s);
        return;
    }

    queueEntry(entry.get());
    m_resourceTimingBuffer.append(WTFMove(entry));
}

bool Performance::isResourceTimingBufferFull() const
{
    return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

void Performance::resourceTimingBufferFullTimerFired()
{
    ASSERT(scriptExecutionContext());

    while (!m_backupResourceTimingBuffer.isEmpty()) {
        auto beforeCount = m_backupResourceTimingBuffer.size();

        auto backupBuffer = WTFMove(m_backupResourceTimingBuffer);
        ASSERT(m_backupResourceTimingBuffer.isEmpty());

        if (isResourceTimingBufferFull()) {
            m_resourceTimingBufferFullFlag = true;
            dispatchEvent(Event::create(eventNames().resourcetimingbufferfullEvent, Event::CanBubble::No, Event::IsCancelable::No));
        }

        if (m_resourceTimingBufferFullFlag) {
            for (auto& entry : backupBuffer)
                queueEntry(*entry);
            // Dispatching resourcetimingbufferfull event may have inserted more entries.
            for (auto& entry : m_backupResourceTimingBuffer)
                queueEntry(*entry);
            m_backupResourceTimingBuffer.clear();
            break;
        }

        // More entries may have added while dispatching resourcetimingbufferfull event.
        backupBuffer.appendVector(m_backupResourceTimingBuffer);
        m_backupResourceTimingBuffer.clear();

        for (auto& entry : backupBuffer) {
            if (!isResourceTimingBufferFull()) {
                m_resourceTimingBuffer.append(entry.copyRef());
                queueEntry(*entry);
            } else
                m_backupResourceTimingBuffer.append(entry.copyRef());
        }

        auto afterCount = m_backupResourceTimingBuffer.size();

        if (beforeCount <= afterCount) {
            m_backupResourceTimingBuffer.clear();
            break;
        }
    }
    m_waitingForBackupBufferToBeProcessed = false;
}

ExceptionOr<void> Performance::mark(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<UserTiming>(*this);

    auto result = m_userTiming->mark(markName);
    if (result.hasException())
        return result.releaseException();

    queueEntry(result.releaseReturnValue());

    return { };
}

void Performance::clearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<UserTiming>(*this);
    m_userTiming->clearMarks(markName);
}

ExceptionOr<void> Performance::measure(const String& measureName, const String& startMark, const String& endMark)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<UserTiming>(*this);

    auto result = m_userTiming->measure(measureName, startMark, endMark);
    if (result.hasException())
        return result.releaseException();

    queueEntry(result.releaseReturnValue());

    return { };
}

void Performance::clearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<UserTiming>(*this);
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
        for (auto& observer : copyToVector(m_observers))
            observer->deliver();
    });
}

} // namespace WebCore
