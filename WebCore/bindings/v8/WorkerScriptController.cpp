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

#include <v8.h>

#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "DOMTimer.h"
#include "V8DOMMap.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"
#include "WorkerObjectProxy.h"
#include "WorkerThread.h"

namespace WebCore {

WorkerScriptController::WorkerScriptController(WorkerContext* workerContext)
    : m_workerContext(workerContext)
    , m_proxy(new WorkerContextExecutionProxy(workerContext))
    , m_executionForbidden(false)
{
}

WorkerScriptController::~WorkerScriptController()
{
    removeAllDOMObjectsInCurrentThread();
}

ScriptValue WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode)
{
    {
        MutexLocker lock(m_sharedDataMutex);
        if (m_executionForbidden)
            return ScriptValue();
    }

    v8::Local<v8::Value> result = m_proxy->evaluate(sourceCode.source(), sourceCode.url().string(), sourceCode.startLine() - 1);
    m_workerContext->thread()->workerObjectProxy()->reportPendingActivity(m_workerContext->hasPendingActivity());
    return ScriptValue();
}

ScriptValue WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode, ScriptValue* /* exception */)
{
    // FIXME: Need to return an exception.
    return evaluate(sourceCode);
}

void WorkerScriptController::forbidExecution()
{
    // This function is called from another thread.
    MutexLocker lock(m_sharedDataMutex);
    m_executionForbidden = true;
}

void WorkerScriptController::setException(ScriptValue /* exception */)
{
    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
