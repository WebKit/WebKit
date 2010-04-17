/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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


#ifndef WorkerContextExecutionProxy_h
#define WorkerContextExecutionProxy_h

#if ENABLE(WORKERS)

#include "ScriptValue.h"
#include <v8.h>
#include <wtf/OwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

    class Event;
    class EventTarget;
    class WorkerContext;

    struct WorkerContextExecutionState {
        WorkerContextExecutionState() : hadException(false), lineNumber(0) { }

        bool hadException;
        ScriptValue exception;
        String errorMessage;
        int lineNumber;
        String sourceURL;
    };

    class WorkerContextExecutionProxy {
    public:
        WorkerContextExecutionProxy(WorkerContext*);
        ~WorkerContextExecutionProxy();

        // Track the event so that we can detach it from the JS wrapper when a worker
        // terminates. This is needed because we need to be able to dispose these
        // events and releases references to their event targets: WorkerContext.
        void trackEvent(Event*);

        // Evaluate a script file in the current execution environment.
        ScriptValue evaluate(const String& script, const String& fileName, int baseLine, WorkerContextExecutionState*);

        // Returns a local handle of the context.
        v8::Local<v8::Context> context() { return v8::Local<v8::Context>::New(m_context); }

    private:
        void initV8IfNeeded();
        bool initContextIfNeeded();
        void dispose();

        // Run an already compiled script.
        v8::Local<v8::Value> runScript(v8::Handle<v8::Script>);

        static bool forgetV8EventObject(Event*);

        static const int kWorkerMaxStackSize = 500 * 1024;

        WorkerContext* m_workerContext;
        v8::Persistent<v8::Context> m_context;
        int m_recursion;

        Vector<Event*> m_events;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerContextExecutionProxy_h
