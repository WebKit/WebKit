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
#include "ExceptionOr.h"
#include "GenericTaskQueue.h"
#include "ReducedResolutionSeconds.h"
#include <wtf/ListHashSet.h>
#include <wtf/Variant.h>

namespace JSC {
class JSGlobalObject;
}

namespace WebCore {

class LoadTiming;
class PerformanceUserTiming;
class PerformanceEntry;
class PerformanceMark;
class PerformanceMeasure;
class PerformanceNavigation;
class PerformanceObserver;
class PerformancePaintTiming;
class PerformanceTiming;
class ResourceResponse;
class ResourceTiming;
class ScriptExecutionContext;
struct PerformanceMarkOptions;
struct PerformanceMeasureOptions;

class Performance final : public RefCounted<Performance>, public ContextDestructionObserver, public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED(Performance);
public:
    static Ref<Performance> create(ScriptExecutionContext* context, MonotonicTime timeOrigin) { return adoptRef(*new Performance(context, timeOrigin)); }
    ~Performance();

    DOMHighResTimeStamp now() const;
    ReducedResolutionSeconds nowInReducedResolutionSeconds() const;

    PerformanceNavigation* navigation();
    PerformanceTiming* timing();

    Vector<RefPtr<PerformanceEntry>> getEntries() const;
    Vector<RefPtr<PerformanceEntry>> getEntriesByType(const String& entryType) const;
    Vector<RefPtr<PerformanceEntry>> getEntriesByName(const String& name, const String& entryType) const;
    void appendBufferedEntriesByType(const String& entryType, Vector<RefPtr<PerformanceEntry>>&) const;

    void clearResourceTimings();
    void setResourceTimingBufferSize(unsigned);

    ExceptionOr<Ref<PerformanceMark>> mark(JSC::JSGlobalObject&, const String& markName, Optional<PerformanceMarkOptions>&&);
    void clearMarks(const String& markName);

    using StartOrMeasureOptions = Variant<String, PerformanceMeasureOptions>;
    ExceptionOr<Ref<PerformanceMeasure>> measure(JSC::JSGlobalObject&, const String& measureName, Optional<StartOrMeasureOptions>&&, const String& endMark);
    void clearMeasures(const String& measureName);

    void addResourceTiming(ResourceTiming&&);

    void reportFirstContentfulPaint();

    void removeAllObservers();
    void registerPerformanceObserver(PerformanceObserver&);
    void unregisterPerformanceObserver(PerformanceObserver&);

    static Seconds reduceTimeResolution(Seconds);

    DOMHighResTimeStamp relativeTimeFromTimeOriginInReducedResolution(MonotonicTime) const;

    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    using RefCounted::ref;
    using RefCounted::deref;

private:
    Performance(ScriptExecutionContext*, MonotonicTime timeOrigin);

    void contextDestroyed() override;

    EventTargetInterface eventTargetInterface() const final { return PerformanceEventTargetInterfaceType; }

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool isResourceTimingBufferFull() const;
    void resourceTimingBufferFullTimerFired();

    void queueEntry(PerformanceEntry&);

    mutable RefPtr<PerformanceNavigation> m_navigation;
    mutable RefPtr<PerformanceTiming> m_timing;

    // https://w3c.github.io/resource-timing/#extensions-performance-interface recommends size of 150.
    Vector<RefPtr<PerformanceEntry>> m_resourceTimingBuffer;
    unsigned m_resourceTimingBufferSize { 150 };

    Timer m_resourceTimingBufferFullTimer;
    Vector<RefPtr<PerformanceEntry>> m_backupResourceTimingBuffer;

    // https://w3c.github.io/resource-timing/#dfn-resource-timing-buffer-full-flag
    bool m_resourceTimingBufferFullFlag { false };
    bool m_waitingForBackupBufferToBeProcessed { false };

    MonotonicTime m_timeOrigin;

    RefPtr<PerformancePaintTiming> m_firstContentfulPaint;
    std::unique_ptr<PerformanceUserTiming> m_userTiming;

    GenericTaskQueue<ScriptExecutionContext> m_performanceTimelineTaskQueue;
    ListHashSet<RefPtr<PerformanceObserver>> m_observers;
};

}
