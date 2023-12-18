/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RunLoopObserver.h"
#include <JavaScriptCore/EdenGCActivityCallback.h>
#include <JavaScriptCore/FullGCActivityCallback.h>
#include <JavaScriptCore/MarkedSpace.h>
#include <wtf/CheckedPtr.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class OpportunisticTaskScheduler;
class Page;

class ImminentlyScheduledWorkScope : public RefCounted<ImminentlyScheduledWorkScope> {
public:
    static Ref<ImminentlyScheduledWorkScope> create(OpportunisticTaskScheduler& scheduler)
    {
        return adoptRef(*new ImminentlyScheduledWorkScope(scheduler));
    }

    ~ImminentlyScheduledWorkScope();

private:
    ImminentlyScheduledWorkScope(OpportunisticTaskScheduler&);

    WeakPtr<OpportunisticTaskScheduler> m_scheduler;
};

class OpportunisticTaskScheduler final : public RefCounted<OpportunisticTaskScheduler>, public CanMakeWeakPtr<OpportunisticTaskScheduler> {
public:
    static Ref<OpportunisticTaskScheduler> create(Page& page)
    {
        return adoptRef(*new OpportunisticTaskScheduler(page));
    }

    ~OpportunisticTaskScheduler();

    void willQueueIdleCallback() { m_mayHavePendingIdleCallbacks = true; }

    void rescheduleIfNeeded(MonotonicTime deadline);
    bool hasImminentlyScheduledWork() const { return m_imminentlyScheduledWorkCount; }

    WARN_UNUSED_RETURN Ref<ImminentlyScheduledWorkScope> makeScheduledWorkScope();

    class FullGCActivityCallback final : public JSC::FullGCActivityCallback {
    public:
        using Base = JSC::FullGCActivityCallback;

        static Ref<FullGCActivityCallback> create(JSC::Heap& heap)
        {
            return adoptRef(*new FullGCActivityCallback(heap));
        }

        void doCollection(JSC::VM&) final;

    private:
        FullGCActivityCallback(JSC::Heap&);

        JSC::VM& m_vm;
        std::unique_ptr<RunLoopObserver> m_runLoopObserver;
        JSC::HeapVersion m_version { 0 };
        unsigned m_deferCount { 0 };
    };

    class EdenGCActivityCallback final : public JSC::EdenGCActivityCallback {
    public:
        using Base = JSC::EdenGCActivityCallback;

        static Ref<EdenGCActivityCallback> create(JSC::Heap& heap)
        {
            return adoptRef(*new EdenGCActivityCallback(heap));
        }

        void doCollection(JSC::VM&) final;

    private:
        EdenGCActivityCallback(JSC::Heap&);

        JSC::VM& m_vm;
        std::unique_ptr<RunLoopObserver> m_runLoopObserver;
        JSC::HeapVersion m_version { 0 };
        unsigned m_deferCount { 0 };
    };

private:
    friend class ImminentlyScheduledWorkScope;

    OpportunisticTaskScheduler(Page&);
    void runLoopObserverFired();

    bool isPageInactiveOrLoading() const;

    bool shouldAllowOpportunisticallyScheduledTasks() const;

    SingleThreadWeakPtr<Page> m_page;
    uint64_t m_imminentlyScheduledWorkCount { 0 };
    uint64_t m_runloopCountAfterBeingScheduled { 0 };
    MonotonicTime m_currentDeadline;
    std::unique_ptr<RunLoopObserver> m_runLoopObserver;
    bool m_mayHavePendingIdleCallbacks { false };
};

} // namespace WebCore
