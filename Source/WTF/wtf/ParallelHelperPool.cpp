/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include <wtf/ParallelHelperPool.h>

#include <wtf/AutomaticThread.h>

namespace WTF {

ParallelHelperClient::ParallelHelperClient(RefPtr<ParallelHelperPool>&& pool)
    : m_pool(WTFMove(pool))
{
    Locker locker { *m_pool->m_lock };
    RELEASE_ASSERT(!m_pool->m_isDying);
    m_pool->m_clients.append(this);
}

ParallelHelperClient::~ParallelHelperClient()
{
    Locker locker { *m_pool->m_lock };
    finishWithLock();

    for (size_t i = 0; i < m_pool->m_clients.size(); ++i) {
        if (m_pool->m_clients[i] == this) {
            m_pool->m_clients[i] = m_pool->m_clients.last();
            m_pool->m_clients.removeLast();
            break;
        }
    }
}

void ParallelHelperClient::setTask(RefPtr<SharedTask<void ()>>&& task)
{
    Locker locker { *m_pool->m_lock };
    RELEASE_ASSERT(!m_task);
    m_task = WTFMove(task);
    m_pool->didMakeWorkAvailable(locker);
}

void ParallelHelperClient::finish()
{
    Locker locker { *m_pool->m_lock };
    finishWithLock();
}

void ParallelHelperClient::doSomeHelping()
{
    RefPtr<SharedTask<void ()>> task;
    {
        Locker locker { *m_pool->m_lock };
        task = claimTask();
        if (!task)
            return;
    }

    runTask(task);
}

void ParallelHelperClient::runTaskInParallel(RefPtr<SharedTask<void ()>>&& task)
{
    setTask(WTFMove(task));
    doSomeHelping();
    finish();
}

void ParallelHelperClient::finishWithLock()
{
    m_task = nullptr;
    while (m_numActive)
        m_pool->m_workCompleteCondition.wait(*m_pool->m_lock);
}

RefPtr<SharedTask<void ()>> ParallelHelperClient::claimTask()
{
    if (!m_task)
        return nullptr;
    
    m_numActive++;
    return m_task;
}

void ParallelHelperClient::runTask(const RefPtr<SharedTask<void ()>>& task)
{
    RELEASE_ASSERT(m_numActive);
    RELEASE_ASSERT(task);

    task->run();

    {
        Locker locker { *m_pool->m_lock };
        RELEASE_ASSERT(m_numActive);
        // No new task could have been installed, since we were still active.
        RELEASE_ASSERT(!m_task || m_task == task);
        m_task = nullptr;
        m_numActive--;
        if (!m_numActive)
            m_pool->m_workCompleteCondition.notifyAll();
    }
}

ParallelHelperPool::ParallelHelperPool(ASCIILiteral threadName)
    : m_lock(Box<Lock>::create())
    , m_workAvailableCondition(AutomaticThreadCondition::create())
    , m_threadName(threadName)
{
}

ParallelHelperPool::~ParallelHelperPool()
{
    RELEASE_ASSERT(m_clients.isEmpty());
    
    {
        Locker locker { *m_lock };
        m_isDying = true;
        m_workAvailableCondition->notifyAll(locker);
    }

    for (RefPtr<AutomaticThread>& thread : m_threads)
        thread->join();
}

void ParallelHelperPool::ensureThreads(unsigned numThreads)
{
    Locker locker { *m_lock };
    if (numThreads < m_numThreads)
        return;
    m_numThreads = numThreads;
    if (getClientWithTask())
        didMakeWorkAvailable(locker);
}

void ParallelHelperPool::doSomeHelping()
{
    ParallelHelperClient* client;
    RefPtr<SharedTask<void ()>> task;
    {
        Locker locker { *m_lock };
        client = getClientWithTask();
        if (!client)
            return;
        assertIsHeld(*client->m_pool->m_lock);
        task = client->claimTask();
    }

    client->runTask(task);
}

class ParallelHelperPool::Thread final : public AutomaticThread {
public:
    Thread(const AbstractLocker& locker, ParallelHelperPool& pool)
        : AutomaticThread(locker, pool.m_lock, pool.m_workAvailableCondition.copyRef())
        , m_pool(pool)
    {
    }
    
    ASCIILiteral name() const final
    {
        return m_pool.m_threadName;
    }

private:
    PollResult poll(const AbstractLocker&) final
    {
        if (m_pool.m_isDying)
            return PollResult::Stop;
        assertIsHeld(*m_pool.m_lock);
        m_client = m_pool.getClientWithTask();
        if (m_client) {
            assertIsHeld(*m_client->m_pool->m_lock);
            m_task = m_client->claimTask();
            return PollResult::Work;
        }
        return PollResult::Wait;
    }
    
    WorkResult work() final
    {
        m_client->runTask(m_task);
        m_client = nullptr;
        m_task = nullptr;
        return WorkResult::Continue;
    }

    ParallelHelperPool& m_pool;
    ParallelHelperClient* m_client { nullptr };
    RefPtr<SharedTask<void ()>> m_task;
};

void ParallelHelperPool::didMakeWorkAvailable(const AbstractLocker& locker)
{
    while (m_numThreads > m_threads.size())
        m_threads.append(adoptRef(new Thread(locker, *this)));
    m_workAvailableCondition->notifyAll(locker);
}

bool ParallelHelperPool::hasClientWithTask()
{
    return !!getClientWithTask();
}

ParallelHelperClient* ParallelHelperPool::getClientWithTask()
{
    // We load-balance by being random.
    unsigned startIndex = m_random.getUint32(m_clients.size());
    for (unsigned index = startIndex; index < m_clients.size(); ++index) {
        ParallelHelperClient* client = m_clients[index];
        if (client->m_task)
            return client;
    }
    for (unsigned index = 0; index < startIndex; ++index) {
        ParallelHelperClient* client = m_clients[index];
        if (client->m_task)
            return client;
    }

    return nullptr;
}

} // namespace WTF

