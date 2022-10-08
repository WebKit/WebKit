/*
 * Copyright (C) 2016 Konstantin Tokavev <annulen@yandex.ru>
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/WorkQueue.h>

#include <wtf/threads/BinarySemaphore.h>

namespace WTF {

WorkQueueBase::WorkQueueBase(RunLoop& runLoop)
    : m_runLoop(&runLoop)
{
}

void WorkQueueBase::platformInitialize(const char* name, Type, QOS qos)
{
    m_runLoop = RunLoop::create(name, ThreadType::Unknown, qos).ptr();
#if ASSERT_ENABLED
    BinarySemaphore semaphore;
    m_runLoop->dispatch([&] {
        m_threadID = Thread::current().uid();
        semaphore.signal();
    });
    semaphore.wait();
#endif
}

void WorkQueueBase::platformInvalidate()
{
    if (m_runLoop) {
        Ref<RunLoop> protector(*m_runLoop);
        protector->stop();
        protector->dispatch([] {
            RunLoop::current().stop();
        });
    }
}

void WorkQueueBase::dispatch(Function<void()>&& function)
{
    m_runLoop->dispatch([protectedThis = Ref { *this }, function = WTFMove(function)] {
        function();
    });
}

void WorkQueueBase::dispatchAfter(Seconds delay, Function<void()>&& function)
{
    m_runLoop->dispatchAfter(delay, [protectedThis = Ref { *this }, function = WTFMove(function)] {
        function();
    });
}

WorkQueue::WorkQueue(RunLoop& loop)
    : WorkQueueBase(loop)
{
}

Ref<WorkQueue> WorkQueue::constructMainWorkQueue()
{
    return adoptRef(*new WorkQueue(RunLoop::main()));
}

#if ASSERT_ENABLED
ThreadLikeAssertion WorkQueue::threadLikeAssertion() const
{
    return createThreadLikeAssertion(m_threadID);
}
#endif

}
