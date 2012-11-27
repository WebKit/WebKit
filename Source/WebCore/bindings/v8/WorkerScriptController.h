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

#include "ScriptValue.h"
#include "V8Binding.h"
#include <v8.h>
#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {

    class ScriptSourceCode;
    class ScriptValue;
    class WorkerContext;

    struct WorkerContextExecutionState {
        WorkerContextExecutionState()
            : hadException(false)
            , lineNumber(0)
        {
        }

        bool hadException;
        ScriptValue exception;
        String errorMessage;
        int lineNumber;
        String sourceURL;
    };

    class WorkerScriptController {
    public:
        WorkerScriptController(WorkerContext*);
        ~WorkerScriptController();

        WorkerContext* workerContext() { return m_workerContext; }

        void evaluate(const ScriptSourceCode&, ScriptValue* = 0);

        void setException(const ScriptValue&);

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

        void disableEval(const String&);

        // Returns WorkerScriptController for the currently executing context. 0 will be returned if the current executing context is not the worker context.
        static WorkerScriptController* controllerForContext();

        // Evaluate a script file in the current execution environment.
        ScriptValue evaluate(const String& script, const String& fileName, const TextPosition& scriptStartPosition, WorkerContextExecutionState*);

        // Returns a local handle of the context.
        v8::Local<v8::Context> context() { return v8::Local<v8::Context>::New(m_context.get()); }

    private:
        bool initializeContextIfNeeded();
        void disposeContext();

        WorkerContext* m_workerContext;
        v8::Isolate* m_isolate;
        ScopedPersistent<v8::Context> m_context;
        OwnPtr<V8PerContextData> m_perContextData;
        String m_disableEvalPending;
        OwnPtr<DOMDataStore> m_domDataStore;
        bool m_executionForbidden;
        bool m_executionScheduledToTerminate;
        mutable Mutex m_scheduledTerminationMutex;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerScriptController_h
