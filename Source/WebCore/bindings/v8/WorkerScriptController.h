/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#ifndef WorkerScriptController_h
#define WorkerScriptController_h

#if ENABLE(WORKERS)

#include "ScopedDOMDataStore.h"
#include "V8Binding.h"

#include <v8.h>
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

    class ScriptSourceCode;
    class ScriptValue;
    class WorkerContext;
    class WorkerContextExecutionProxy;

    class WorkerScriptController {
    public:
        WorkerScriptController(WorkerContext*);
        ~WorkerScriptController();

        WorkerContextExecutionProxy* proxy() { return m_proxy.get(); }
        WorkerContext* workerContext() { return m_workerContext; }

        void evaluate(const ScriptSourceCode&);
        void evaluate(const ScriptSourceCode&, ScriptValue* exception);

        void setException(ScriptValue);

        // Async request to terminate a future JS execution. Eventually causes termination
        // exception raised during JS execution, if the worker thread happens to run JS.
        // After JS execution was terminated in this way, the Worker thread has to use
        // forbidExecution()/isExecutionForbidden() to guard against reentry into JS.
        // Can be called from any thread.
        void scheduleExecutionTermination();
        bool isExecutionTerminating() const;

        // Called on Worker thread when JS exits with termination exception caused by forbidExecution() request,
        // or by Worker thread termination code to prevent future entry into JS.
        void forbidExecution();
        bool isExecutionForbidden() const;

        void disableEval();

        // Returns WorkerScriptController for the currently executing context. 0 will be returned if the current executing context is not the worker context.
        static WorkerScriptController* controllerForContext();

    private:
        WorkerContext* m_workerContext;
        OwnPtr<WorkerContextExecutionProxy> m_proxy;
        v8::Isolate* m_isolate;
        ScopedDOMDataStore m_DOMDataStore;
        bool m_executionForbidden;
        bool m_executionScheduledToTerminate;
        mutable Mutex m_scheduledTerminationMutex;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerScriptController_h
