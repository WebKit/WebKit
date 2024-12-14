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

#include <utility>
#include <wtf/threads/BinarySemaphore.h>

namespace WTF {

static std::pair<Ref<RunLoop>, uint32_t> createQueueRunLoop(ASCIILiteral name, QOS qos)
{
    Ref runLoop = RunLoop::create(name, ThreadType::Unknown, qos);
    uint32_t threadID = 0;
    BinarySemaphore semaphore;
    runLoop->dispatch([&] {
        threadID = Thread::current().uid();
        semaphore.signal();
    });
    semaphore.wait();
    return { WTFMove(runLoop), threadID };
}

WorkQueue::WorkQueueBase(MainTag)
    : WorkQueueBase(&RunLoop::main(), mainThreadID)
{
}

WorkQueueBase::~WorkQueueBase()
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

Ref<WorkQueue> WorkQueue::create(ASCIILiteral name, QOS qos)
{
    auto [workQueue, threadID] = createQueueRunLoop(name, qos);
    return adoptRef(*new WorkQueue(WTFMove(workQueue), threadID));
}

Ref<ConcurrentWorkQueue> ConcurrentWorkQueue::create(ASCIILiteral name, QOS qos)
{
    auto [workQueue, threadID] = createQueueRunLoop(name, qos);
    return adoptRef(*new ConcurrentWorkQueue(WTFMove(workQueue), threadID))
}

Ref<ConcurrentWorkQueue> ConcurrentWorkQueue::createToDefault(ASCIILiteral, QOS)
{
    // Minimally implemented.
    static Ref defaultQueue = create("Default concurrent queue", QOS::Default);
    return defaultQueue;
}

void ConcurrentWorkQueue::dispatchBarrierSync(WTF::Function<void()>&& function)
{
    dispatchSync(WTFMove(function));
}

}
