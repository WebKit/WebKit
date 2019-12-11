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

#pragma once

#include <wtf/CrossThreadQueue.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

namespace WTF {

class RegistrationStore;
class SQLiteDatabase;

class CrossThreadTaskHandler {
public:
    WTF_EXPORT_PRIVATE virtual ~CrossThreadTaskHandler();
    enum class AutodrainedPoolForRunLoop { DoNotUse, Use };

protected:
    WTF_EXPORT_PRIVATE CrossThreadTaskHandler(const char* threadName, AutodrainedPoolForRunLoop = AutodrainedPoolForRunLoop::DoNotUse);

    WTF_EXPORT_PRIVATE void postTask(CrossThreadTask&&);
    WTF_EXPORT_PRIVATE void postTaskReply(CrossThreadTask&&);
    WTF_EXPORT_PRIVATE void suspendAndWait();
    WTF_EXPORT_PRIVATE void resume();

private:
    void handleTaskRepliesOnMainThread();
    void taskRunLoop();

    AutodrainedPoolForRunLoop m_useAutodrainedPool { AutodrainedPoolForRunLoop::DoNotUse };

    Lock m_taskThreadCreationLock;
    Lock m_mainThreadReplyLock;
    bool m_mainThreadReplyScheduled { false };

    bool m_shouldSuspend { false };
    Condition m_shouldSuspendCondition;
    Lock m_shouldSuspendLock;
    
    bool m_suspended { false };
    Condition m_suspendedCondition;
    Lock m_suspendedLock;

    CrossThreadQueue<CrossThreadTask> m_taskQueue;
    CrossThreadQueue<CrossThreadTask> m_taskReplyQueue;
};

} // namespace WTF

using WTF::CrossThreadTaskHandler;

