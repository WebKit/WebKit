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

#pragma once

#include "ActiveDOMObject.h"
#include "GenericTaskQueue.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

class Document;

class SuspendableTaskQueue : public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static UniqueRef<SuspendableTaskQueue> create(ScriptExecutionContext* context)
    {
        auto taskQueue = makeUniqueRef<SuspendableTaskQueue>(context);
        taskQueue->suspendIfNeeded();
        return taskQueue;
    }

    static UniqueRef<SuspendableTaskQueue> create(Document*) = delete;
    static UniqueRef<SuspendableTaskQueue> create(Document& document)
    {
        auto taskQueue = makeUniqueRef<SuspendableTaskQueue>(document);
        taskQueue->suspendIfNeeded();
        return taskQueue;
    }

    using TaskFunction = WTF::Function<void()>;
    void enqueueTask(TaskFunction&&);
    void cancelAllTasks();

    void close();
    bool isClosed() const { return m_isClosed; }

    bool hasPendingTasks() const { return m_pendingTasks.isEmpty(); }

private:
    friend UniqueRef<SuspendableTaskQueue> WTF::makeUniqueRefWithoutFastMallocCheck<SuspendableTaskQueue, WebCore::ScriptExecutionContext*&>(WebCore::ScriptExecutionContext*&);
    friend UniqueRef<SuspendableTaskQueue> WTF::makeUniqueRefWithoutFastMallocCheck<SuspendableTaskQueue, WebCore::Document&>(WebCore::Document&);
    explicit SuspendableTaskQueue(ScriptExecutionContext*);
    explicit SuspendableTaskQueue(Document&);

    void runOneTask();

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    void stop() final;
    void suspend(ReasonForSuspension) final;
    void resume() final;

    GenericTaskQueue<ScriptExecutionContext> m_taskQueue;
    Deque<TaskFunction> m_pendingTasks;
    bool m_isClosed { false };
    bool m_isSuspended { false };
};

} // namespace WebCore
