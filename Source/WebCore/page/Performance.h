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

#pragma once

#include "ContextDestructionObserver.h"
#include "DOMHighResTimeStamp.h"
#include "EventTarget.h"
#include "EventTargetInterfaces.h"
#include "ReducedResolutionSeconds.h"
#include "ScriptExecutionContext.h"
#include "Timer.h"
#include <memory>
#include <wtf/ContinuousTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/MonotonicTime.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class CachedResource;
class Document;
class DocumentLoadTiming;
class DocumentLoader;
class EventCounts;
class LargestContentfulPaint;
class NetworkLoadMetrics;
class PerformanceUserTiming;
class PerformanceEntry;
class PerformanceMark;
class PerformanceMeasure;
class PerformanceNavigation;
class PerformanceNavigationTiming;
class PerformanceObserver;
class PerformancePaintTiming;
class PerformanceTiming;
class ResourceResponse;
class ResourceTiming;
class ScriptExecutionContext;
enum class EventType : uint16_t;
struct PerformanceEventTimingCandidate;
struct PerformanceMarkOptions;
struct PerformanceMeasureOptions;
template<typename> class ExceptionOr;

class Performance final : public RefCounted<Performance>, public ContextDestructionObserver, public EventTarget {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(Performance);
public:
    static Ref<Performance> create(ScriptExecutionContext* context, MonotonicTime timeOrigin) { return adoptRef(*new Performance(context, timeOrigin)); }
    ~Performance();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    DOMHighResTimeStamp now() const;
    DOMHighResTimeStamp timeOrigin() const;
    ReducedResolutionSeconds nowInReducedResolutionSeconds() const;

    PerformanceNavigation* navigation();
    PerformanceTiming* timing();
    EventCounts* eventCounts();

    uint64_t interactionCount();

    Vector<Ref<PerformanceEntry>> getEntries() const;
    Vector<Ref<PerformanceEntry>> getEntriesByType(const String& entryType) const;
    Vector<Ref<PerformanceEntry>> getEntriesByName(const String& name, const String& entryType) const;
    void appendBufferedEntriesByType(const String& entryType, Vector<Ref<PerformanceEntry>>&, PerformanceObserver&) const;

    void countEvent(EventType);
    void processEventEntry(const PerformanceEventTimingCandidate&);

    void clearResourceTimings();
    void setResourceTimingBufferSize(unsigned);

    ExceptionOr<Ref<PerformanceMark>> mark(JSC::JSGlobalObject&, const String& markName, std::optional<PerformanceMarkOptions>&&);
    void clearMarks(const String& markName);

    using StartOrMeasureOptions = Variant<String, PerformanceMeasureOptions>;
    ExceptionOr<Ref<PerformanceMeasure>> measure(JSC::JSGlobalObject&, const String& measureName, std::optional<StartOrMeasureOptions>&&, const String& endMark);
    void clearMeasures(const String& measureName);

    void addNavigationTiming(DocumentLoader&, Document&, CachedResource&, const DocumentLoadTiming&, const NetworkLoadMetrics&);
    void documentLoadFinished(const NetworkLoadMetrics&);
    void navigationFinished(MonotonicTime loadEventEnd);
    void addResourceTiming(ResourceTiming&&);

    void reportFirstContentfulPaint(DOMHighResTimeStamp);
    void enqueueLargestContentfulPaint(Ref<LargestContentfulPaint>&&);

    void removeAllObservers();
    void registerPerformanceObserver(PerformanceObserver&);
    void unregisterPerformanceObserver(PerformanceObserver&);

    static void allowHighPrecisionTime();
    static Seconds timeResolution();
    static Seconds reduceTimeResolution(Seconds);

    Seconds relativeTimeFromTimeOriginInReducedResolutionSeconds(MonotonicTime) const;
    DOMHighResTimeStamp relativeTimeFromTimeOriginInReducedResolution(MonotonicTime) const;
    MonotonicTime monotonicTimeFromRelativeTime(DOMHighResTimeStamp) const;

    ScriptExecutionContext* scriptExecutionContext() const final;

    using RefCounted::ref;
    using RefCounted::deref;

    void scheduleTaskIfNeeded();

    PerformanceNavigationTiming* navigationTiming() { return m_navigationTiming.get(); }

private:
    Performance(ScriptExecutionContext*, MonotonicTime timeOrigin);

    void contextDestroyed() override;

    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::Performance; }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool isResourceTimingBufferFull() const;
    void resourceTimingBufferFullTimerFired();

    void queueEntry(PerformanceEntry&);

    const std::unique_ptr<EventCounts> m_eventCounts;
    mutable RefPtr<PerformanceNavigation> m_navigation;
    mutable RefPtr<PerformanceTiming> m_timing;

    // https://w3c.github.io/resource-timing/#sec-extensions-performance-interface recommends initial buffer size of 250.
    Vector<Ref<PerformanceEntry>> m_resourceTimingBuffer;

    Timer m_resourceTimingBufferFullTimer;
    Vector<Ref<PerformanceEntry>> m_backupResourceTimingBuffer;

    RefPtr<PerformanceEntry> m_firstInput;
    Vector<Ref<PerformanceEntry>> m_eventTimingBuffer;

    // Sizes recommended by https://w3c.github.io/timing-entrytypes-registry/#registry:
    unsigned m_eventTimingBufferSize { 150 };
    unsigned m_resourceTimingBufferSize { 250 };

    // https://w3c.github.io/resource-timing/#dfn-resource-timing-buffer-full-flag
    bool m_resourceTimingBufferFullFlag { false };
    bool m_waitingForBackupBufferToBeProcessed { false };
    bool m_hasScheduledDeliveryTask { false };

    MonotonicTime m_timeOrigin;
    ContinuousTime m_continuousTimeOrigin;

    RefPtr<PerformanceNavigationTiming> m_navigationTiming;
    RefPtr<PerformancePaintTiming> m_firstContentfulPaint;
    RefPtr<PerformanceEntry> m_largestContentfulPaint;
    std::unique_ptr<PerformanceUserTiming> m_userTiming;

    ListHashSet<RefPtr<PerformanceObserver>> m_observers;
};

}
