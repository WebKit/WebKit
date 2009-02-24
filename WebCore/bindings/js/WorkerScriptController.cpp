/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"

#if ENABLE(WORKERS)

#include "WorkerScriptController.h"

#include "JSDOMBinding.h"
#include "JSWorkerContext.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "WorkerContext.h"
#include "WorkerObjectProxy.h"
#include "WorkerThread.h"
#include <interpreter/Interpreter.h>
#include <runtime/Completion.h>
#include <runtime/Completion.h>
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

WorkerScriptController::WorkerScriptController(WorkerContext* workerContext)
    : m_globalData(JSGlobalData::create())
    , m_workerContext(workerContext)
    , m_executionForbidden(false)
{
}

WorkerScriptController::~WorkerScriptController()
{
    m_workerContextWrapper = 0; // Unprotect the global object.

    ASSERT(!m_globalData->heap.protectedObjectCount());
    ASSERT(!m_globalData->heap.isBusy());
    m_globalData->heap.destroy();
}

void WorkerScriptController::initScript()
{
    ASSERT(!m_workerContextWrapper);

    JSLock lock(false);

    RefPtr<Structure> prototypeStructure = JSWorkerContextPrototype::createStructure(jsNull());
    RefPtr<Structure> structure = JSWorkerContext::createStructure(new (m_globalData.get()) JSWorkerContextPrototype(prototypeStructure.release()));
    m_workerContextWrapper = new (m_globalData.get()) JSWorkerContext(structure.release(), m_workerContext);
}

ScriptValue WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode)
{
    {
        MutexLocker lock(m_sharedDataMutex);
        if (m_executionForbidden)
            return noValue();
    }

    initScriptIfNeeded();
    JSLock lock(false);

    ExecState* exec = m_workerContextWrapper->globalExec();
    m_workerContextWrapper->globalData()->timeoutChecker.start();
    Completion comp = JSC::evaluate(exec, exec->dynamicGlobalObject()->globalScopeChain(), sourceCode.jsSourceCode(), m_workerContextWrapper);
    m_workerContextWrapper->globalData()->timeoutChecker.stop();

    m_workerContext->thread()->workerObjectProxy()->reportPendingActivity(m_workerContext->hasPendingActivity());

    if (comp.complType() == Normal || comp.complType() == ReturnValue)
        return comp.value();

    if (comp.complType() == Throw)
        reportException(exec, comp.value());
    return noValue();
}

void WorkerScriptController::forbidExecution()
{
    // This function is called from another thread.
    // Mutex protection for m_executionForbidden is needed to guarantee that the value is synchronized between processors, because
    // if it were not, the worker could re-enter JSC::evaluate(), but with timeout already reset.
    // It is not critical for Interpreter::m_timeoutTime to be synchronized, we just rely on it reaching the worker thread's processor sooner or later.
    MutexLocker lock(m_sharedDataMutex);
    m_executionForbidden = true;
    m_globalData->timeoutChecker.setTimeoutInterval(1); // 1ms is the smallest timeout that can be set.
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
