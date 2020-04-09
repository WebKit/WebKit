/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <wtf/CrossThreadTaskHandler.h>

#include <wtf/AutodrainedPool.h>

namespace WTF {

CrossThreadTaskHandler::CrossThreadTaskHandler(const char* threadName, AutodrainedPoolForRunLoop useAutodrainedPool)
    : m_useAutodrainedPool(useAutodrainedPool)
{
    ASSERT(isMainThread());
    Locker<Lock> locker(m_taskThreadCreationLock);
    Thread::create(threadName, [this] {
        taskRunLoop();
    })->detach();
}

CrossThreadTaskHandler::~CrossThreadTaskHandler()
{
    ASSERT(isMainThread());
}

void CrossThreadTaskHandler::postTask(CrossThreadTask&& task)
{
    m_taskQueue.append(WTFMove(task));
}

void CrossThreadTaskHandler::postTaskReply(CrossThreadTask&& task)
{
    m_taskReplyQueue.append(WTFMove(task));

    Locker<Lock> locker(m_mainThreadReplyLock);
    if (m_mainThreadReplyScheduled)
        return;

    m_mainThreadReplyScheduled = true;
    callOnMainThread([this] {
        handleTaskRepliesOnMainThread();
    });
}

void CrossThreadTaskHandler::taskRunLoop()
{
    ASSERT(!isMainThread());
    {
        Locker<Lock> locker(m_taskThreadCreationLock);
    }

    while (!m_taskQueue.isKilled()) {
        {
            std::unique_ptr<AutodrainedPool> autodrainedPool = (m_useAutodrainedPool == AutodrainedPoolForRunLoop::Use) ? makeUnique<AutodrainedPool>() : nullptr;

            m_taskQueue.waitForMessage().performTask();
        }

        Locker<Lock> shouldSuspendLocker(m_shouldSuspendLock);
        while (m_shouldSuspend) {
            m_suspendedLock.lock();
            if (!m_suspended) {
                m_suspended = true;
                m_suspendedCondition.notifyOne();
            }
            m_suspendedLock.unlock();
            m_shouldSuspendCondition.wait(m_shouldSuspendLock);
        }
    }
}

void CrossThreadTaskHandler::handleTaskRepliesOnMainThread()
{
    {
        Locker<Lock> locker(m_mainThreadReplyLock);
        m_mainThreadReplyScheduled = false;
    }

    while (auto task = m_taskReplyQueue.tryGetMessage())
        task->performTask();
}

void CrossThreadTaskHandler::suspendAndWait()
{
    ASSERT(isMainThread());
    {
        Locker<Lock> locker(m_shouldSuspendLock);
        m_shouldSuspend = true;
    }

    // Post an empty task to ensure database thread knows m_shouldSuspend and sets m_suspended.
    postTask(CrossThreadTask([]() { }));

    Locker<Lock> locker(m_suspendedLock);
    while (!m_suspended)
        m_suspendedCondition.wait(m_suspendedLock);
}

void CrossThreadTaskHandler::resume()
{
    ASSERT(isMainThread());
    Locker<Lock> locker(m_shouldSuspendLock);
    if (m_shouldSuspend) {
        m_suspendedLock.lock();
        if (m_suspended)
            m_suspended = false;
        m_suspendedLock.unlock();
        m_shouldSuspend = false;
        m_shouldSuspendCondition.notifyOne();
    }
}

} // namespace WTF
