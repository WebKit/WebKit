/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <wtf/CompletionHandler.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/WorkQueue.h>

namespace WTF {

class SuspendableWorkQueue final : public WorkQueue {
public:
    using QOS = WorkQueue::QOS;
    WTF_EXPORT_PRIVATE static Ref<SuspendableWorkQueue> create(const char* name, QOS = QOS::Default);

    WTF_EXPORT_PRIVATE void suspend(Function<void()>&& suspendFunction, CompletionHandler<void()>&& suspensionCompletionHandler);
    WTF_EXPORT_PRIVATE void resume();
    WTF_EXPORT_PRIVATE void dispatch(Function<void()>&&) final;
    WTF_EXPORT_PRIVATE void dispatchAfter(Seconds, Function<void()>&&) final;
    WTF_EXPORT_PRIVATE void dispatchSync(Function<void()>&&) final;

private:
    SuspendableWorkQueue(const char* name, QOS);
    void invokeAllSuspensionCompletionHandlers() WTF_REQUIRES_LOCK(m_suspensionLock);
    void suspendIfNeeded();
#if USE(COCOA_EVENT_LOOP)
    using WorkQueue::dispatchQueue;
#else
    using WorkQueue::runLoop;
#endif

    Lock m_suspensionLock;
    Condition m_suspensionCondition;
    bool m_isOrWillBeSuspended WTF_GUARDED_BY_LOCK(m_suspensionLock) { false };
    Function<void()> m_suspendFunction WTF_GUARDED_BY_LOCK(m_suspensionLock);
    Vector<CompletionHandler<void()>> m_suspensionCompletionHandlers WTF_GUARDED_BY_LOCK(m_suspensionLock);
};

} // namespace WTF

using WTF::SuspendableWorkQueue;
