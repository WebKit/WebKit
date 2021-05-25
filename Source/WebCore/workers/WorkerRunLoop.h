/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScriptExecutionContext.h"
#include <memory>
#include <wtf/MessageQueue.h>

namespace WebCore {

    class ModePredicate;
    class WorkerOrWorkletGlobalScope;
    class WorkerSharedTimer;

    class WorkerRunLoop {
    public:
        WorkerRunLoop();
        ~WorkerRunLoop();
        
        // Blocking call. Waits for tasks and timers, invokes the callbacks.
        void run(WorkerOrWorkletGlobalScope*);

        enum WaitMode { WaitForMessage, DontWaitForMessage };

        // Waits for a single task and returns.
        MessageQueueWaitResult runInMode(WorkerOrWorkletGlobalScope*, const String& mode, WaitMode = WaitForMessage);
        MessageQueueWaitResult runInDebuggerMode(WorkerOrWorkletGlobalScope&);

        void terminate();
        bool terminated() const { return m_messageQueue.killed(); }

        void postTask(ScriptExecutionContext::Task&&);
        void postTaskAndTerminate(ScriptExecutionContext::Task&&);
        WEBCORE_EXPORT void postTaskForMode(ScriptExecutionContext::Task&&, const String& mode);
        void postDebuggerTask(ScriptExecutionContext::Task&&);

        unsigned long createUniqueId() { return ++m_uniqueId; }

        WEBCORE_EXPORT static String defaultMode();
        class Task {
            WTF_MAKE_NONCOPYABLE(Task); WTF_MAKE_FAST_ALLOCATED;
        public:
            Task(ScriptExecutionContext::Task&&, const String& mode);
            const String& mode() const { return m_mode; }

        private:
            void performTask(WorkerOrWorkletGlobalScope*);

            ScriptExecutionContext::Task m_task;
            String m_mode;

            friend class WorkerRunLoop;
        };

    private:
        friend class RunLoopSetup;
        MessageQueueWaitResult runInMode(WorkerOrWorkletGlobalScope*, const ModePredicate&, WaitMode);

        // Runs any clean up tasks that are currently in the queue and returns.
        // This should only be called when the context is closed or loop has been terminated.
        void runCleanupTasks(WorkerOrWorkletGlobalScope*);

        bool isBeingDebugged() const { return m_debugCount >= 1; }

        MessageQueue<Task> m_messageQueue;
        std::unique_ptr<WorkerSharedTimer> m_sharedTimer;
        int m_nestedCount { 0 };
        int m_debugCount { 0 };
        unsigned long m_uniqueId { 0 };
    };

} // namespace WebCore
