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

#include "JSWorkerContext.h"
#include "WorkerContext.h"
#include <kjs/SourceCode.h>
#include <runtime/Completion.h>
#include <runtime/Interpreter.h>
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

WorkerScriptController::WorkerScriptController(WorkerContext* workerContext)
    : m_globalData(JSGlobalData::create())
    , m_workerContext(workerContext)
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

    m_workerContextWrapper = new (m_globalData.get()) JSWorkerContext(m_workerContext);
}

JSValue* WorkerScriptController::evaluate(const String& sourceURL, int baseLine, const String& code)
{
    ExecState* exec = m_workerContextWrapper->globalExec();
    m_workerContextWrapper->startTimeoutCheck();
    Completion comp = Interpreter::evaluate(exec, exec->dynamicGlobalObject()->globalScopeChain(), makeSource(code, sourceURL, baseLine), m_workerContextWrapper);
    m_workerContextWrapper->stopTimeoutCheck();

    if (comp.complType() == Normal || comp.complType() == ReturnValue)
        return comp.value();

    // FIXME: send exceptions to console.
    if (comp.complType() == Throw)
        fprintf(stderr, "%s\n", comp.value()->toString(exec).UTF8String().c_str());
    return noValue();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
