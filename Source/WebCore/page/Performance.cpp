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

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventCounts.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "ExceptionOr.h"
#include "InspectorInstrumentation.h"
#include "LargestContentfulPaint.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "PerformanceEntry.h"
#include "PerformanceEventTiming.h"
#include "PerformanceMarkOptions.h"
#include "PerformanceMeasureOptions.h"
#include "PerformanceNavigation.h"
#include "PerformanceNavigationTiming.h"
#include "PerformanceObserver.h"
#include "PerformancePaintTiming.h"
#include "PerformanceResourceTiming.h"
#include "PerformanceTiming.h"
#include "PerformanceUserTiming.h"
#include "ResourceResponse.h"
#include "ScriptExecutionContext.h"
#include "dom/DOMHighResTimeStamp.h"
#include <JavaScriptCore/ProfilerSupport.h>
#include <ranges>
#include <wtf/ReducedResolutionSeconds.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Performance);

static Seconds timePrecision { 1_ms };

static bool isSignpostEnabled()
{
    static bool flag = [] {
        if (const char* value = getenv("WebKitPerformanceSignpostEnabled")) {
            if (auto result = parseInteger<int>(StringView::fromLatin1(value)); result && result.value())
                return true;
        }
        return false;
    }();
    return flag;
}

Performance::Performance(ScriptExecutionContext* context, MonotonicTime timeOrigin)
    : ContextDestructionObserver(context)
    , m_resourceTimingBufferFullTimer(*this, &Performance::resourceTimingBufferFullTimerFired) // FIXME: Migrate this to the event loop as well. https://bugs.webkit.org/show_bug.cgi?id=229044
    , m_timeOrigin(timeOrigin)
    , m_continuousTimeOrigin(timeOrigin.approximate<ContinuousTime>())
{
    ASSERT(m_timeOrigin);
    initializeEntryBufferMap();
}

Performance::~Performance() = default;

void Performance::contextDestroyed()
{
    m_resourceTimingBufferFullTimer.stop();
    ContextDestructionObserver::contextDestroyed();
}

// https://w3c.github.io/performance-timeline/#performance-entry-buffer-map
Performance::PerformanceEntryBuffer& Performance::entryBufferTuple(PerformanceEntry::Type type)
{
    return m_entryBufferMap[entryTypeIndex(type)];
}

const Performance::PerformanceEntryBuffer& Performance::entryBufferTuple(PerformanceEntry::Type type) const
{
    return m_entryBufferMap[entryTypeIndex(type)];
}

// https://w3c.github.io/timing-entrytypes-registry/#registry
static constexpr unsigned maxResourceTimingBufferSize = 250;
static constexpr unsigned maxPaintTimingBufferSize = 2;
static constexpr unsigned maxFirstInputBufferSize = 1;
static constexpr unsigned maxEventTimingBufferSize = 150;
static constexpr unsigned maxLargestContentfulPaintBufferSize = 150;

void Performance::initializeEntryBufferMap()
{
    // Default tuple has empty buffer, unlimited maxBufferSize, availableFromTimeline=true, droppedEntriesCount=0.
    entryBufferTuple(PerformanceEntry::Type::Resource).maxBufferSize = maxResourceTimingBufferSize;
    entryBufferTuple(PerformanceEntry::Type::Paint).maxBufferSize = maxPaintTimingBufferSize;
    entryBufferTuple(PerformanceEntry::Type::FirstInput).maxBufferSize = maxFirstInputBufferSize;

    auto& eventTuple = entryBufferTuple(PerformanceEntry::Type::Event);
    eventTuple.maxBufferSize = maxEventTimingBufferSize;
    eventTuple.availableFromTimeline = false;

    auto& lcpTuple = entryBufferTuple(PerformanceEntry::Type::LargestContentfulPaint);
    lcpTuple.maxBufferSize = maxLargestContentfulPaintBufferSize;
    lcpTuple.availableFromTimeline = false;
}

bool Performance::addToEntryBuffer(PerformanceEntry& entry)
{
    auto& tuple = entryBufferTuple(entry.performanceEntryType());
    if (tuple.buffer.size() >= tuple.maxBufferSize) {
        tuple.droppedEntriesCount++;
        return false;
    }
    tuple.buffer.append(entry);
    return true;
}

void Performance::clearEntryBuffer(PerformanceEntry::Type type, const String& name)
{
    auto& buffer = entryBufferTuple(type).buffer;
    if (name.isNull())
        buffer.clear();
    else
        buffer.removeAllMatching([&](auto& entry) { return entry->name() == name; });
}

DOMHighResTimeStamp Performance::now() const
{
    return nowInReducedResolutionSeconds().milliseconds();
}

DOMHighResTimeStamp Performance::timeOrigin() const
{
    return reduceTimeResolution(m_timeOrigin.approximate<WallTime>().secondsSinceEpoch()).milliseconds();
}

ReducedResolutionSeconds Performance::nowInReducedResolutionSeconds() const
{
    return reduceTimeResolution(MonotonicTime::now() - m_timeOrigin);
}

ReducedResolutionSeconds Performance::reduceTimeResolution(Seconds seconds)
{
    return ReducedResolutionSeconds::reduce(seconds, timePrecision);
}

void Performance::allowHighPrecisionTime()
{
    timePrecision = Seconds::highTimePrecision();
}

Seconds Performance::timeResolution()
{
    return timePrecision;
}

ReducedResolutionSeconds Performance::relativeTimeFromTimeOriginInReducedResolutionSeconds(MonotonicTime timestamp) const
{
    return reduceTimeResolution(timestamp - m_timeOrigin);
}

MonotonicTime Performance::monotonicTimeFromOriginRelative(Seconds offset) const
{
    return m_timeOrigin + offset;
}

DOMHighResTimeStamp Performance::relativeTimeFromTimeOriginInReducedResolution(MonotonicTime timestamp) const
{
    return relativeTimeFromTimeOriginInReducedResolutionSeconds(timestamp).milliseconds();
}

MonotonicTime Performance::monotonicTimeFromRelativeTime(DOMHighResTimeStamp relativeTime) const
{
    return m_timeOrigin + Seconds::fromMilliseconds(relativeTime);
}

ScriptExecutionContext* Performance::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

EventCounts& Performance::eventCounts()
{
    ASSERT(is<Document>(scriptExecutionContext()));
    ASSERT(isMainThread());

    // FIXME: stop lazy-initializing m_eventCounts after event
    // timing stops being gated by a flag:
    if (!m_eventCounts)
        lazyInitialize(m_eventCounts, makeUniqueWithoutRefCountedCheck<EventCounts>(this));

    return *m_eventCounts;
}

uint64_t Performance::interactionCount()
{
    ASSERT(is<Document>(scriptExecutionContext()));
    ASSERT(isMainThread());

    return downcast<Document>(*scriptExecutionContext()).window()->interactionCount();
}

PerformanceNavigation& Performance::navigation()
{
    ASSERT(is<Document>(scriptExecutionContext()));
    ASSERT(isMainThread());

    if (!m_navigation)
        m_navigation = PerformanceNavigation::create(protect(downcast<Document>(*scriptExecutionContext()).window()));
    return *m_navigation;
}

PerformanceTiming& Performance::timing()
{
    ASSERT(is<Document>(scriptExecutionContext()));
    ASSERT(isMainThread());

    if (!m_timing)
        m_timing = PerformanceTiming::create(protect(downcast<Document>(*scriptExecutionContext()).window()));
    return *m_timing;
}

Vector<Ref<PerformanceEntry>> Performance::getEntries() const
{
    // https://w3c.github.io/performance-timeline/#dfn-filter-buffer-map-by-name-and-type
    Vector<Ref<PerformanceEntry>> entries;
    for (auto& tuple : m_entryBufferMap) {
        if (!tuple.availableFromTimeline)
            continue;
        entries.appendVector(tuple.buffer);
    }
    std::ranges::stable_sort(entries, PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<Ref<PerformanceEntry>> Performance::getEntriesByType(const String& entryType) const
{
    // https://w3c.github.io/performance-timeline/#dfn-filter-buffer-map-by-name-and-type
    auto type = PerformanceEntry::parseEntryTypeString(entryType);
    if (!type)
        return { };

    auto& tuple = entryBufferTuple(*type);
    if (!tuple.availableFromTimeline)
        return { };

    Vector<Ref<PerformanceEntry>> entries;
    entries.appendVector(tuple.buffer);
    std::ranges::stable_sort(entries, PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

Vector<Ref<PerformanceEntry>> Performance::getEntriesByName(const String& name, const String& entryType) const
{
    // https://w3c.github.io/performance-timeline/#dfn-filter-buffer-map-by-name-and-type
    Vector<Ref<PerformanceEntry>> entries;

    auto filterAndAppend = [&](const PerformanceEntryBuffer& tuple) {
        if (!tuple.availableFromTimeline)
            return;
        for (auto& entry : tuple.buffer) {
            if (entry->name() == name)
                entries.append(entry);
        }
    };

    if (!entryType.isNull()) {
        auto type = PerformanceEntry::parseEntryTypeString(entryType);
        if (!type)
            return entries;
        filterAndAppend(entryBufferTuple(*type));
    } else {
        for (auto& tuple : m_entryBufferMap)
            filterAndAppend(tuple);
    }

    std::ranges::stable_sort(entries, PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

void Performance::appendBufferedEntriesByType(const String& entryType, Vector<Ref<PerformanceEntry>>& entries, PerformanceObserver& observer) const
{
    // https://w3c.github.io/performance-timeline/#observe-method (step 8c)
    // No availableFromTimeline check — buffered observers see all entry types.
    auto type = PerformanceEntry::parseEntryTypeString(entryType);
    if (!type)
        return;

    if (*type == PerformanceEntry::Type::Navigation && observer.hasNavigationTiming())
        return;

    auto& tuple = entryBufferTuple(*type);
    entries.appendVector(tuple.buffer);

    if (*type == PerformanceEntry::Type::Navigation && !tuple.buffer.isEmpty())
        observer.addedNavigationTiming();
}

void Performance::countEvent(EventType type)
{
    ASSERT(isMainThread());
    eventCounts().add(type);
}

void Performance::processEventEntry(const PerformanceEventTimingCandidate& candidate)
{
    // Constants to avoid the need to round to the appropriate resolution
    // (PerformanceEventTiming::durationResolution) when filtering candidates:
    static constexpr Seconds minDurationCutoffBeforeRounding = PerformanceEventTiming::minimumDurationThreshold - (PerformanceEventTiming::durationResolution / 2);
    static constexpr Seconds defaultDurationCutoffBeforeRounding = PerformanceEventTiming::defaultDurationThreshold - (PerformanceEventTiming::durationResolution / 2);

    // The event timing spec requires us to set first-input and call
    // setDispatchedInputEvent() when an entry with nonzero interactionID has
    // its duration set, and only if hasDispatchedInputEvent() is false. This, however,
    // causes inconsistent behavior: a pointerdown event that had its duration assigned
    // before receiving an interactionID wouldn't qualify for first-input.
    //
    // We instead set first-input and call setDispatchedInputEvent() here; ongoing
    // spec discussion at https://github.com/w3c/event-timing/issues/159 :
    if (!m_firstInput && !candidate.interactionID.isUnassigned()) {
        m_firstInput = PerformanceEventTiming::create(candidate, true);
        addToEntryBuffer(*m_firstInput);
        queueEntry(protect(*m_firstInput));
        if (RefPtr document = dynamicDowncast<Document>(*scriptExecutionContext())) {
            if (auto* window = document->window())
                window->setDispatchedInputEvent();
        }
    }

    if (candidate.duration < minDurationCutoffBeforeRounding)
        return;

    // FIXME: early return more often by keeping track of m_observers; we
    // should keep track of the minimum defaultThreshold and whether any
    // observers are interested in 'event' entries:
    if (candidate.duration <= defaultDurationCutoffBeforeRounding && !m_observers.size())
        return;

    auto entry = PerformanceEventTiming::create(candidate);
    if (candidate.duration > defaultDurationCutoffBeforeRounding)
        addToEntryBuffer(entry);

    queueEntry(entry);
}

void Performance::clearResourceTimings()
{
    entryBufferTuple(PerformanceEntry::Type::Resource).buffer.clear();
    m_resourceTimingBufferFullFlag = false;
}

void Performance::setResourceTimingBufferSize(unsigned size)
{
    entryBufferTuple(PerformanceEntry::Type::Resource).maxBufferSize = size;
    m_resourceTimingBufferFullFlag = false;
}

void Performance::reportFirstContentfulPaint(DOMHighResTimeStamp timestamp)
{
    if (RefPtr context = scriptExecutionContext())
        InspectorInstrumentation::didEnqueueFirstContentfulPaint(*context);

    auto entry = PerformancePaintTiming::createFirstContentfulPaint(timestamp);
    addToEntryBuffer(entry);
    queueEntry(entry);
}

void Performance::enqueueLargestContentfulPaint(Ref<LargestContentfulPaint>&& paintEntry)
{
    if (RefPtr context = scriptExecutionContext())
        InspectorInstrumentation::didEnqueueLargestContentfulPaint(*context, paintEntry.get());

    addToEntryBuffer(paintEntry);
    queueEntry(paintEntry);
}

void Performance::addNavigationTiming(DocumentLoader& documentLoader, Document& document, CachedResource& resource, const DocumentLoadTiming& timing, const NetworkLoadMetrics& metrics)
{
    m_navigationTiming = PerformanceNavigationTiming::create(m_timeOrigin, resource, timing, metrics, document.eventTiming(), document.securityOrigin(), documentLoader.triggeringAction().type());
    addToEntryBuffer(*m_navigationTiming);
}

void Performance::documentLoadFinished(const NetworkLoadMetrics& metrics)
{
    if (!m_navigationTiming)
        return;

    protect(m_navigationTiming)->documentLoadFinished(metrics);
}

void Performance::navigationFinished(MonotonicTime loadEventEnd)
{
    if (!m_navigationTiming)
        return;

    m_navigationTiming->documentLoadTiming().setLoadEventEnd(loadEventEnd);
    queueEntry(protect(*m_navigationTiming));
}

void Performance::addResourceTiming(ResourceTiming&& resourceTiming)
{
    ASSERT(scriptExecutionContext());

    auto entry = PerformanceResourceTiming::create(m_timeOrigin, WTF::move(resourceTiming));

    if (m_waitingForBackupBufferToBeProcessed) {
        m_backupResourceTimingBuffer.append(WTF::move(entry));
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
        m_backupResourceTimingBuffer.append(WTF::move(entry));
        m_waitingForBackupBufferToBeProcessed = true;
        m_resourceTimingBufferFullTimer.startOneShot(0_s);
        return;
    }

    queueEntry(entry.get());
    entryBufferTuple(PerformanceEntry::Type::Resource).buffer.append(WTF::move(entry));
}

bool Performance::isResourceTimingBufferFull() const
{
    auto& tuple = entryBufferTuple(PerformanceEntry::Type::Resource);
    return tuple.buffer.size() >= tuple.maxBufferSize;
}

void Performance::resourceTimingBufferFullTimerFired()
{
    ASSERT(scriptExecutionContext());

    auto& resourceBuffer = entryBufferTuple(PerformanceEntry::Type::Resource).buffer;

    while (!m_backupResourceTimingBuffer.isEmpty()) {
        auto beforeCount = m_backupResourceTimingBuffer.size();

        auto backupBuffer = std::exchange(m_backupResourceTimingBuffer, { });
        ASSERT(m_backupResourceTimingBuffer.isEmpty());

        if (isResourceTimingBufferFull()) {
            m_resourceTimingBufferFullFlag = true;
            dispatchEvent(Event::create(eventNames().resourcetimingbufferfullEvent, Event::CanBubble::No, Event::IsCancelable::No));
        }

        if (m_resourceTimingBufferFullFlag) {
            for (auto& entry : backupBuffer)
                queueEntry(entry);
            // Dispatching resourcetimingbufferfull event may have inserted more entries.
            for (auto& entry : std::exchange(m_backupResourceTimingBuffer, { }))
                queueEntry(entry);
            break;
        }

        // More entries may have added while dispatching resourcetimingbufferfull event.
        backupBuffer.appendVector(std::exchange(m_backupResourceTimingBuffer, { }));

        for (auto& entry : backupBuffer) {
            if (!isResourceTimingBufferFull()) {
                resourceBuffer.append(entry.copyRef());
                queueEntry(entry);
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

ExceptionOr<Ref<PerformanceMark>> Performance::mark(JSC::JSGlobalObject& globalObject, const String& markName, std::optional<PerformanceMarkOptions>&& markOptions)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<PerformanceUserTiming>(*this);

    auto mark = m_userTiming->mark(globalObject, markName, WTF::move(markOptions));
    if (mark.hasException())
        return mark.releaseException();

    addToEntryBuffer(mark.returnValue().get());
    queueEntry(mark.returnValue().get());
    return mark.releaseReturnValue();
}

void Performance::clearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<PerformanceUserTiming>(*this);
    m_userTiming->clearMarks(markName);
    clearEntryBuffer(PerformanceEntry::Type::Mark, markName);
}

ExceptionOr<Ref<PerformanceMeasure>> Performance::measure(JSC::JSGlobalObject& globalObject, const String& measureName, StartOrMeasureOptions&& startOrMeasureOptions, const String& endMark)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<PerformanceUserTiming>(*this);

    auto measure = m_userTiming->measure(globalObject, measureName, WTF::move(startOrMeasureOptions), endMark);
    if (measure.hasException())
        return measure.releaseException();

    if (isSignpostEnabled()) {
        Ref entry { measure.returnValue() };
        auto message = measureName.utf8();
#if OS(DARWIN)
        {
            auto startTime = m_continuousTimeOrigin + Seconds::fromMilliseconds(entry->startTime());
            auto endTime = m_continuousTimeOrigin + Seconds::fromMilliseconds(entry->startTime() + entry->duration());
            uint64_t platformStartTime = startTime.toMachContinuousTime();
            uint64_t platformEndTime = endTime.toMachContinuousTime();
            uint64_t correctedStartTime = std::min(platformStartTime, platformEndTime);
            uint64_t correctedEndTime = std::max(platformStartTime, platformEndTime);
            // Because signpost intervals are closed invervals [start, end], we decrease the endTime by 1 if startTime and endTime is not the same.
            if (correctedStartTime != correctedEndTime)
                correctedEndTime -= 1;

            WTFBeginSignpostAlwaysWithSpecificTime(entry.ptr(), WebKitPerformance, correctedStartTime, "%" PUBLIC_LOG_STRING, message.data());
            WTFEndSignpostAlwaysWithSpecificTime(entry.ptr(), WebKitPerformance, correctedEndTime, "%" PUBLIC_LOG_STRING, message.data());
        }
#endif
        {
            auto timeOrigin = m_continuousTimeOrigin.approximate<MonotonicTime>();
            auto startTime = timeOrigin + Seconds::fromMilliseconds(entry->startTime());
            auto endTime = timeOrigin + Seconds::fromMilliseconds(entry->startTime() + entry->duration());
            JSC::ProfilerSupport::markInterval(entry.ptr(), JSC::ProfilerSupport::Category::WebKitPerformanceSignpost, startTime, endTime, WTF::move(message));
        }
    }

    addToEntryBuffer(measure.returnValue().get());
    queueEntry(measure.returnValue().get());
    return measure.releaseReturnValue();
}

void Performance::clearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = makeUnique<PerformanceUserTiming>(*this);
    m_userTiming->clearMeasures(measureName);
    clearEntryBuffer(PerformanceEntry::Type::Measure, measureName);
}

void Performance::removeAllObservers()
{
    for (auto& observer : m_observers)
        observer->disassociate();
    m_observers.clear();
}

void Performance::registerPerformanceObserver(PerformanceObserver& observer)
{
    m_observers.add(observer);
}

void Performance::unregisterPerformanceObserver(PerformanceObserver& observer)
{
    m_observers.remove(observer);
}

void Performance::queueEntry(PerformanceEntry& entry)
{
    bool shouldScheduleTask = false;
    for (auto& observer : m_observers) {
        bool isObserverInterested = observer->typeFilter().contains(entry.performanceEntryType());
        if (entry.performanceEntryType() == PerformanceEntry::Type::Event && entry.duration() < observer->durationThreshold().milliseconds())
            isObserverInterested = false;

        if (entry.performanceEntryType() == PerformanceEntry::Type::Navigation && observer->hasNavigationTiming())
            isObserverInterested = false;

        if (isObserverInterested) {
            observer->queueEntry(entry);
            shouldScheduleTask = true;
        }
    }

    if (!shouldScheduleTask)
        return;

    LOG_WITH_STREAM(PerformanceTimeline, stream << "PerformanceEntry of type " << entry.name() << " dispatched to interested observer at t=" << now());
    scheduleTaskIfNeeded();
}

void Performance::scheduleTaskIfNeeded()
{
    if (m_hasScheduledDeliveryTask)
        return;

    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    m_hasScheduledDeliveryTask = true;
    context->eventLoop().queueTask(TaskSource::PerformanceTimeline, [protectedThis = Ref { *this }, this] {
        RefPtr context = scriptExecutionContext();
        if (!context)
            return;

        m_hasScheduledDeliveryTask = false;
        for (auto& observer : copyToVector(m_observers))
            observer->deliver();
    });
}

} // namespace WebCore
