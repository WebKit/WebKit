/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include <wtf/WorkerPool.h>

namespace WTF {

class WorkerPool::Worker final : public AutomaticThread {
public:
    friend class WorkerPool;

    Worker(const AbstractLocker& locker, WorkerPool& pool, Box<Lock> lock, Ref<AutomaticThreadCondition>&& condition, Seconds timeout)
        : AutomaticThread(locker, lock, WTFMove(condition), timeout)
        , m_pool(pool)
    {
    }

    PollResult poll(const AbstractLocker&) final
    {
        if (m_pool.m_tasks.isEmpty())
            return PollResult::Wait;
        m_task = m_pool.m_tasks.takeFirst();
        if (!m_task)
            return PollResult::Stop;
        return PollResult::Work;
    }

    WorkResult work() final
    {
        m_task();
        m_task = nullptr;
        return WorkResult::Continue;
    }

    void threadDidStart() final
    {
        LockHolder locker(*m_pool.m_lock);
        m_pool.m_numberOfActiveWorkers++;
    }

    void threadIsStopping(const AbstractLocker&) final
    {
        m_pool.m_numberOfActiveWorkers--;
    }

    bool shouldSleep(const AbstractLocker& locker) final
    {
        return m_pool.shouldSleep(locker);
    }

    const char* name() const final
    {
        return m_pool.name();
    }

private:
    WorkerPool& m_pool;
    Function<void()> m_task;
};

WorkerPool::WorkerPool(ASCIILiteral name, unsigned numberOfWorkers, Seconds timeout)
    : m_lock(Box<Lock>::create())
    , m_condition(AutomaticThreadCondition::create())
    , m_timeout(timeout)
    , m_name(name)
{
    LockHolder locker(*m_lock);
    for (unsigned i = 0; i < numberOfWorkers; ++i)
        m_workers.append(adoptRef(*new Worker(locker, *this, m_lock, m_condition.copyRef(), timeout)));
}

WorkerPool::~WorkerPool()
{
    {
        LockHolder locker(*m_lock);
        for (unsigned i = m_workers.size(); i--;)
            m_tasks.append(nullptr); // Use null task to indicate that we want the thread to terminate.
        m_condition->notifyAll(locker);
    }
    for (auto& worker : m_workers)
        worker->join();
    ASSERT(!m_numberOfActiveWorkers);
}

bool WorkerPool::shouldSleep(const AbstractLocker&)
{
    if (m_timeout > 0_s && std::isinf(m_timeout))
        return false;

    MonotonicTime currentTime = MonotonicTime::now();
    if (std::isnan(m_lastTimeoutTime) || (currentTime >= (m_lastTimeoutTime  + m_timeout))) {
        m_lastTimeoutTime = currentTime;
        return true;
    }
    return false;
}

void WorkerPool::postTask(Function<void()>&& task)
{
    LockHolder locker(*m_lock);
    m_tasks.append(WTFMove(task));
    m_condition->notifyOne(locker);
}

}
