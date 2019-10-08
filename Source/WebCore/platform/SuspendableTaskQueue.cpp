/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "SuspendableTaskQueue.h"

#include "Document.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

SuspendableTaskQueue::SuspendableTaskQueue(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_taskQueue(context)
    , m_isClosed(!context)
{
}

SuspendableTaskQueue::SuspendableTaskQueue(Document& document)
    : ActiveDOMObject(document)
    , m_taskQueue(document)
{
}

void SuspendableTaskQueue::close()
{
    m_isClosed = true;
    cancelAllTasks();
}

void SuspendableTaskQueue::cancelAllTasks()
{
    m_taskQueue.cancelAllTasks();
    m_pendingTasks.clear();
}

void SuspendableTaskQueue::enqueueTask(TaskFunction&& task)
{
    if (m_isClosed)
        return;

    m_pendingTasks.append(WTFMove(task));

    if (m_isSuspended)
        return;

    m_taskQueue.enqueueTask(std::bind(&SuspendableTaskQueue::runOneTask, this));
}

void SuspendableTaskQueue::runOneTask()
{
    ASSERT(!m_pendingTasks.isEmpty());
    auto task = m_pendingTasks.takeFirst();
    task();
}

const char* SuspendableTaskQueue::activeDOMObjectName() const
{
    return "SuspendableTaskQueue";
}

bool SuspendableTaskQueue::canSuspendForDocumentSuspension() const
{
    return true;
}

void SuspendableTaskQueue::stop()
{
    close();
}

void SuspendableTaskQueue::suspend(ReasonForSuspension)
{
    m_isSuspended = true;
    m_taskQueue.cancelAllTasks();
}

void SuspendableTaskQueue::resume()
{
    m_isSuspended = false;
    for (size_t i = 0; i < m_pendingTasks.size(); ++i)
        m_taskQueue.enqueueTask(std::bind(&SuspendableTaskQueue::runOneTask, this));
}

} // namespace WebCore
