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
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Page;
class OpportunisticTaskScheduler;

class OpportunisticTaskDeferralScope {
    WTF_MAKE_NONCOPYABLE(OpportunisticTaskDeferralScope); WTF_MAKE_FAST_ALLOCATED;
public:
    OpportunisticTaskDeferralScope(OpportunisticTaskScheduler&);
    OpportunisticTaskDeferralScope(OpportunisticTaskDeferralScope&&);
    ~OpportunisticTaskDeferralScope();

private:
    WeakPtr<OpportunisticTaskScheduler> m_scheduler;
};

class OpportunisticTaskScheduler : public RefCounted<OpportunisticTaskScheduler>, public CanMakeWeakPtr<OpportunisticTaskScheduler> {
public:
    static Ref<OpportunisticTaskScheduler> create(Page& page)
    {
        return adoptRef(*new OpportunisticTaskScheduler(page));
    }

    ~OpportunisticTaskScheduler();

    void reschedule(MonotonicTime deadline);

    WARN_UNUSED_RETURN std::unique_ptr<OpportunisticTaskDeferralScope> makeDeferralScope();

private:
    friend class OpportunisticTaskDeferralScope;

    OpportunisticTaskScheduler(Page&);
    void runLoopObserverFired();

    void incrementDeferralCount();
    void decrementDeferralCount();

    WeakPtr<Page> m_page;
    uint64_t m_taskDeferralCount { 0 };
    MonotonicTime m_currentDeadline;
    std::unique_ptr<RunLoopObserver> m_runLoopObserver;
};

} // namespace WebCore
