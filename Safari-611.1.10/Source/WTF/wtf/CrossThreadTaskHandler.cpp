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

        if (m_completionCallback)
            m_completionCallback();
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
        std::unique_ptr<AutodrainedPool> autodrainedPool = (m_useAutodrainedPool == AutodrainedPoolForRunLoop::Use) ? makeUnique<AutodrainedPool>() : nullptr;

        m_taskQueue.waitForMessage().performTask();
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

void CrossThreadTaskHandler::setCompletionCallback(Function<void ()>&& completionCallback)
{
    m_completionCallback = WTFMove(completionCallback);
}

void CrossThreadTaskHandler::kill()
{
    m_taskQueue.kill();
    m_taskReplyQueue.kill();
}

} // namespace WTF
