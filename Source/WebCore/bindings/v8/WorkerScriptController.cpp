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

#include "config.h"

#if ENABLE(WORKERS)

#include "WorkerScriptController.h"

#include "DOMTimer.h"
#include "ScriptCallStack.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "V8DOMMap.h"
#include "V8Proxy.h"
#include "V8WorkerContext.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"
#include "WorkerObjectProxy.h"
#include "WorkerThread.h"
#include <v8.h>

namespace WebCore {

WorkerScriptController::WorkerScriptController(WorkerContext* workerContext)
    : m_workerContext(workerContext)
    , m_proxy(adoptPtr(new WorkerContextExecutionProxy(workerContext)))
    , m_executionForbidden(false)
{
}

WorkerScriptController::~WorkerScriptController()
{
    removeAllDOMObjectsInCurrentThread();
}

ScriptValue WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode)
{
    return evaluate(sourceCode, 0);
}

ScriptValue WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode, ScriptValue* exception)
{
    if (isExecutionForbidden())
        return ScriptValue();

    WorkerContextExecutionState state;
    ScriptValue result = m_proxy->evaluate(sourceCode.source(), sourceCode.url().string(), WTF::toZeroBasedTextPosition(sourceCode.startPosition()), &state);
    if (state.hadException) {
        if (exception)
            *exception = state.exception;
        else
            m_workerContext->reportException(state.errorMessage, state.lineNumber, state.sourceURL, 0);
    }

    return result;
}

void WorkerScriptController::scheduleExecutionTermination()
{
    v8::V8::TerminateExecution();
}

void WorkerScriptController::forbidExecution()
{
    ASSERT(m_workerContext->isContextThread());
    m_executionForbidden = true;
}

bool WorkerScriptController::isExecutionForbidden() const
{
    ASSERT(m_workerContext->isContextThread());
    return m_executionForbidden;
}

void WorkerScriptController::setException(ScriptValue exception)
{
    throwError(*exception.v8Value());
}

WorkerScriptController* WorkerScriptController::controllerForContext()
{
    // Happens on frame destruction, check otherwise GetCurrent() will crash.
    if (!v8::Context::InContext())
        return 0;
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    v8::Handle<v8::Object> global = context->Global();
    global = V8DOMWrapper::lookupDOMWrapper(V8WorkerContext::GetTemplate(), global);
    // Return 0 if the current executing context is not the worker context.
    if (global.IsEmpty())
        return 0;
    WorkerContext* workerContext = V8WorkerContext::toNative(global);
    return workerContext->script();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
