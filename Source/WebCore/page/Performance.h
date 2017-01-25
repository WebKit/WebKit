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

#if ENABLE(WEB_TIMING)

#include "DOMWindowProperty.h"
#include "EventTarget.h"
#include "ExceptionOr.h"

namespace WebCore {

class Document;
class LoadTiming;
class PerformanceEntry;
class PerformanceNavigation;
class PerformanceTiming;
class ResourceResponse;
class URL;
class UserTiming;

class Performance final : public RefCounted<Performance>, public DOMWindowProperty, public EventTargetWithInlineData {
public:
    static Ref<Performance> create(Frame& frame) { return adoptRef(*new Performance(frame)); }
    ~Performance();

    PerformanceNavigation& navigation();
    PerformanceTiming& timing();
    double now() const;

    Vector<RefPtr<PerformanceEntry>> getEntries() const;
    Vector<RefPtr<PerformanceEntry>> getEntriesByType(const String& entryType) const;
    Vector<RefPtr<PerformanceEntry>> getEntriesByName(const String& name, const String& entryType) const;

    void clearResourceTimings();
    void setResourceTimingBufferSize(unsigned);

    void addResourceTiming(const String& initiatorName, Document*, const URL& originalURL, const ResourceResponse&, const LoadTiming&);

    using RefCounted::ref;
    using RefCounted::deref;

    ExceptionOr<void> mark(const String& markName);
    void clearMarks(const String& markName);

    ExceptionOr<void> measure(const String& measureName, const String& startMark, const String& endMark);
    void clearMeasures(const String& measureName);

    static double reduceTimeResolution(double seconds);

private:
    explicit Performance(Frame&);

    EventTargetInterface eventTargetInterface() const final { return PerformanceEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool isResourceTimingBufferFull() const;

    mutable RefPtr<PerformanceNavigation> m_navigation;
    mutable RefPtr<PerformanceTiming> m_timing;

    // https://w3c.github.io/resource-timing/#extensions-performance-interface recommends size of 150.
    Vector<RefPtr<PerformanceEntry>> m_resourceTimingBuffer;
    unsigned m_resourceTimingBufferSize { 150 };

    double m_referenceTime;

    std::unique_ptr<UserTiming> m_userTiming;
};

}

#endif // ENABLE(WEB_TIMING)
