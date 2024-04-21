/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include <wtf/WorkQueue.h>

#include <mutex>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/NumberOfCores.h>
#include <wtf/Ref.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WTF {

WorkQueueBase::WorkQueueBase(ASCIILiteral name, Type type, QOS qos)
{
    platformInitialize(name, type, qos);
}

WorkQueueBase::~WorkQueueBase()
{
    platformInvalidate();
}

Ref<ConcurrentWorkQueue> ConcurrentWorkQueue::create(ASCIILiteral name, QOS qos)
{
    return adoptRef(*new ConcurrentWorkQueue(name, qos));
}

void ConcurrentWorkQueue::dispatch(Function<void()>&& function)
{
    WorkQueueBase::dispatch(WTFMove(function));
}

#if !PLATFORM(COCOA)
void WorkQueueBase::dispatchSync(Function<void()>&& function)
{
    BinarySemaphore semaphore;
    dispatch([&semaphore, function = WTFMove(function)]() mutable {
        function();
        semaphore.signal();
    });
    semaphore.wait();
}

void WorkQueueBase::dispatchWithQOS(Function<void()>&& function, QOS)
{
    dispatch(WTFMove(function));
}

void ConcurrentWorkQueue::apply(size_t iterations, WTF::Function<void(size_t index)>&& function)
{
    if (!iterations)
        return;

    if (iterations == 1) {
        function(0);
        return;
    }

    class ThreadPool {
    public:
        ThreadPool()
            // We don't need a thread for the current core.
            : m_workers(numberOfProcessorCores() - 1, [this](size_t) {
                return Thread::create("ThreadPool Worker"_s, [this] {
                    threadBody();
                });
            })
        {
        }

        size_t workerCount() const { return m_workers.size(); }

        void dispatch(const WTF::Function<void ()>* function)
        {
            Locker locker { m_lock };
            m_queue.append(function);
            m_condition.notifyOne();
        }

    private:
        NO_RETURN void threadBody()
        {
            while (true) {
                const WTF::Function<void ()>* function;

                {
                    Locker locker { m_lock };
                    m_condition.wait(m_lock, [this] {
                        assertIsHeld(m_lock);
                        return !m_queue.isEmpty();
                    });

                    function = m_queue.takeFirst();
                }

                (*function)();
            }
        }

        Lock m_lock;
        Condition m_condition;
        Deque<const Function<void()>*> m_queue WTF_GUARDED_BY_LOCK(m_lock);

        Vector<Ref<Thread>> m_workers;
    };

    static LazyNeverDestroyed<ThreadPool> threadPool;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        threadPool.construct();
    });

    // Cap the worker count to the number of iterations (excluding this thread)
    const size_t workerCount = std::min(iterations - 1, threadPool->workerCount());

    std::atomic<size_t> currentIndex(0);
    std::atomic<size_t> activeThreads(workerCount + 1);

    Condition condition;
    Lock lock;

    Function<void ()> applier = [&, function = WTFMove(function)] {
        size_t index;

        // Call the function for as long as there are iterations left.
        while ((index = currentIndex++) < iterations)
            function(index);

        // If there are no active threads left, signal the caller.
        if (!--activeThreads) {
            Locker locker { lock };
            condition.notifyOne();
        }
    };

    for (size_t i = 0; i < workerCount; ++i)
        threadPool->dispatch(&applier);
    applier();

    Locker locker { lock };
    condition.wait(lock, [&] { return !activeThreads; });
}
#endif

WorkQueue& WorkQueue::main()
{
    static NeverDestroyed<RefPtr<WorkQueue>> mainWorkQueue;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        mainWorkQueue.get() = adoptRef(*new WorkQueue(CreateMain));
    });
    return *mainWorkQueue.get();
}

Ref<WorkQueue> WorkQueue::create(ASCIILiteral name, QOS qos)
{
    return adoptRef(*new WorkQueue(name, qos));
}

WorkQueue::WorkQueue(ASCIILiteral name, QOS qos)
    : WorkQueueBase(name, Type::Serial, qos)
{
}

void WorkQueue::dispatch(Function<void()>&& function)
{
    WorkQueueBase::dispatch(WTFMove(function));
}

bool WorkQueue::isCurrent() const
{
    return currentSequence() == m_threadID;
}

void WorkQueue::ref() const
{
    ThreadSafeRefCounted::ref();
}

void WorkQueue::deref() const
{
    ThreadSafeRefCounted::deref();
}

ConcurrentWorkQueue::ConcurrentWorkQueue(ASCIILiteral name, QOS qos)
    : WorkQueueBase(name, Type::Concurrent, qos)
{
}

}
