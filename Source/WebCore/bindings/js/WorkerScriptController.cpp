/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011, 2012 Google Inc. All Rights Reserved.
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

#include "config.h"
#include "WorkerScriptController.h"

#include "JSDOMBinding.h"
#include "JSDedicatedWorkerGlobalScope.h"
#include "JSEventTarget.h"
#include "JSExecState.h"
#include "JSServiceWorkerGlobalScope.h"
#include "ScriptSourceCode.h"
#include "WebCoreJSClientData.h"
#include "WorkerConsoleClient.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/PromiseDeferredTimer.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {
using namespace JSC;

WorkerScriptController::WorkerScriptController(WorkerGlobalScope* workerGlobalScope)
    : m_vm(VM::create())
    , m_workerGlobalScope(workerGlobalScope)
    , m_workerGlobalScopeWrapper(*m_vm)
{
    m_vm->heap.acquireAccess(); // It's not clear that we have good discipline for heap access, so turn it on permanently.
    JSVMClientData::initNormalWorld(m_vm.get());
}

WorkerScriptController::~WorkerScriptController()
{
    JSLockHolder lock(vm());
    if (m_workerGlobalScopeWrapper) {
        m_workerGlobalScopeWrapper->clearDOMGuardedObjects();
        m_workerGlobalScopeWrapper->setConsoleClient(nullptr);
        m_consoleClient = nullptr;
    }
    m_workerGlobalScopeWrapper.clear();
    m_vm = nullptr;
}

void WorkerScriptController::initScript()
{
    ASSERT(!m_workerGlobalScopeWrapper);

    JSLockHolder lock(m_vm.get());

    // Explicitly protect the global object's prototype so it isn't collected
    // when we allocate the global object. (Once the global object is fully
    // constructed, it can mark its own prototype.)
    if (m_workerGlobalScope->isDedicatedWorkerGlobalScope()) {
        Structure* dedicatedContextPrototypeStructure = JSDedicatedWorkerGlobalScopePrototype::createStructure(*m_vm, nullptr, jsNull());
        Strong<JSDedicatedWorkerGlobalScopePrototype> dedicatedContextPrototype(*m_vm, JSDedicatedWorkerGlobalScopePrototype::create(*m_vm, nullptr, dedicatedContextPrototypeStructure));
        Structure* structure = JSDedicatedWorkerGlobalScope::createStructure(*m_vm, nullptr, dedicatedContextPrototype.get());
        auto* proxyStructure = JSProxy::createStructure(*m_vm, nullptr, jsNull(), PureForwardingProxyType);
        auto* proxy = JSProxy::create(*m_vm, proxyStructure);

        m_workerGlobalScopeWrapper.set(*m_vm, JSDedicatedWorkerGlobalScope::create(*m_vm, structure, static_cast<DedicatedWorkerGlobalScope&>(*m_workerGlobalScope), proxy));
        dedicatedContextPrototypeStructure->setGlobalObject(*m_vm, m_workerGlobalScopeWrapper.get());
        ASSERT(structure->globalObject() == m_workerGlobalScopeWrapper);
        ASSERT(m_workerGlobalScopeWrapper->structure(*m_vm)->globalObject() == m_workerGlobalScopeWrapper);
        dedicatedContextPrototype->structure(*m_vm)->setGlobalObject(*m_vm, m_workerGlobalScopeWrapper.get());
        dedicatedContextPrototype->structure(*m_vm)->setPrototypeWithoutTransition(*m_vm, JSWorkerGlobalScope::prototype(*m_vm, *m_workerGlobalScopeWrapper.get()));

        proxy->setTarget(*m_vm, m_workerGlobalScopeWrapper.get());
        proxy->structure(*m_vm)->setGlobalObject(*m_vm, m_workerGlobalScopeWrapper.get());
#if ENABLE(SERVICE_WORKER)
    } else if (m_workerGlobalScope->isServiceWorkerGlobalScope()) {
        Structure* contextPrototypeStructure = JSServiceWorkerGlobalScopePrototype::createStructure(*m_vm, nullptr, jsNull());
        Strong<JSServiceWorkerGlobalScopePrototype> contextPrototype(*m_vm, JSServiceWorkerGlobalScopePrototype::create(*m_vm, nullptr, contextPrototypeStructure));
        Structure* structure = JSServiceWorkerGlobalScope::createStructure(*m_vm, nullptr, contextPrototype.get());
        auto* proxyStructure = JSProxy::createStructure(*m_vm, nullptr, jsNull(), PureForwardingProxyType);
        auto* proxy = JSProxy::create(*m_vm, proxyStructure);
    
        m_workerGlobalScopeWrapper.set(*m_vm, JSServiceWorkerGlobalScope::create(*m_vm, structure, static_cast<ServiceWorkerGlobalScope&>(*m_workerGlobalScope), proxy));
        contextPrototypeStructure->setGlobalObject(*m_vm, m_workerGlobalScopeWrapper.get());
        ASSERT(structure->globalObject() == m_workerGlobalScopeWrapper);
        ASSERT(m_workerGlobalScopeWrapper->structure()->globalObject() == m_workerGlobalScopeWrapper);
        contextPrototype->structure(*m_vm)->setGlobalObject(*m_vm, m_workerGlobalScopeWrapper.get());
        contextPrototype->structure(*m_vm)->setPrototypeWithoutTransition(*m_vm, JSWorkerGlobalScope::prototype(*m_vm, *m_workerGlobalScopeWrapper.get()));

        proxy->setTarget(*m_vm, m_workerGlobalScopeWrapper.get());
        proxy->structure(*m_vm)->setGlobalObject(*m_vm, m_workerGlobalScopeWrapper.get());
#endif
    }
    
    ASSERT(m_workerGlobalScopeWrapper->globalObject() == m_workerGlobalScopeWrapper);
    ASSERT(asObject(m_workerGlobalScopeWrapper->getPrototypeDirect(*m_vm))->globalObject() == m_workerGlobalScopeWrapper);

    m_consoleClient = std::make_unique<WorkerConsoleClient>(*m_workerGlobalScope);
    m_workerGlobalScopeWrapper->setConsoleClient(m_consoleClient.get());
}

void WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    NakedPtr<JSC::Exception> exception;
    evaluate(sourceCode, exception, returnedExceptionMessage);
    if (exception) {
        JSLockHolder lock(vm());
        reportException(m_workerGlobalScopeWrapper->globalExec(), exception);
    }
}

void WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode, NakedPtr<JSC::Exception>& returnedException, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    initScriptIfNeeded();

    auto& state = *m_workerGlobalScopeWrapper->globalExec();
    VM& vm = state.vm();
    JSLockHolder lock { vm };

    JSExecState::profiledEvaluate(&state, JSC::ProfilingReason::Other, sourceCode.jsSourceCode(), m_workerGlobalScopeWrapper->globalThis(), returnedException);

    if ((returnedException && isTerminatedExecutionException(vm, returnedException)) || isTerminatingExecution()) {
        forbidExecution();
        return;
    }

    if (returnedException) {
        if (m_workerGlobalScope->canIncludeErrorDetails(sourceCode.cachedScript(), sourceCode.url().string())) {
            // FIXME: It's not great that this can run arbitrary code to string-ify the value of the exception.
            // Do we need to do anything to handle that properly, if it, say, raises another exception?
            if (returnedExceptionMessage)
                *returnedExceptionMessage = returnedException->value().toWTFString(&state);
        } else {
            // Overwrite the detailed error with a generic error.
            String genericErrorMessage { "Script error."_s };
            if (returnedExceptionMessage)
                *returnedExceptionMessage = genericErrorMessage;
            returnedException = JSC::Exception::create(vm, createError(&state, genericErrorMessage));
        }
    }
}

void WorkerScriptController::setException(JSC::Exception* exception)
{
    JSC::ExecState* exec = m_workerGlobalScopeWrapper->globalExec();
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwException(exec, scope, exception);
}

void WorkerScriptController::scheduleExecutionTermination()
{
    if (m_isTerminatingExecution)
        return;

    {
        // The mutex provides a memory barrier to ensure that once
        // termination is scheduled, isTerminatingExecution() will
        // accurately reflect that state when called from another thread.
        LockHolder locker(m_scheduledTerminationMutex);
        m_isTerminatingExecution = true;
    }
    m_vm->notifyNeedTermination();
}

bool WorkerScriptController::isTerminatingExecution() const
{
    // See comments in scheduleExecutionTermination regarding mutex usage.
    LockHolder locker(m_scheduledTerminationMutex);
    return m_isTerminatingExecution;
}

void WorkerScriptController::forbidExecution()
{
    ASSERT(m_workerGlobalScope->isContextThread());
    m_executionForbidden = true;
}

bool WorkerScriptController::isExecutionForbidden() const
{
    ASSERT(m_workerGlobalScope->isContextThread());
    return m_executionForbidden;
}

void WorkerScriptController::disableEval(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock{vm()};

    m_workerGlobalScopeWrapper->setEvalEnabled(false, errorMessage);
}

void WorkerScriptController::disableWebAssembly(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock{vm()};

    m_workerGlobalScopeWrapper->setWebAssemblyEnabled(false, errorMessage);
}

void WorkerScriptController::releaseHeapAccess()
{
    m_vm->heap.releaseAccess();
}

void WorkerScriptController::acquireHeapAccess()
{
    m_vm->heap.acquireAccess();
}

void WorkerScriptController::addTimerSetNotification(JSC::JSRunLoopTimer::TimerNotificationCallback callback)
{
    auto processTimer = [&] (JSRunLoopTimer* timer) {
        if (!timer)
            return;
        timer->addTimerSetNotification(callback);
    };

    processTimer(m_vm->heap.fullActivityCallback());
    processTimer(m_vm->heap.edenActivityCallback());
    processTimer(m_vm->promiseDeferredTimer.get());
}

void WorkerScriptController::removeTimerSetNotification(JSC::JSRunLoopTimer::TimerNotificationCallback callback)
{
    auto processTimer = [&] (JSRunLoopTimer* timer) {
        if (!timer)
            return;
        timer->removeTimerSetNotification(callback);
    };

    processTimer(m_vm->heap.fullActivityCallback());
    processTimer(m_vm->heap.edenActivityCallback());
    processTimer(m_vm->promiseDeferredTimer.get());
}

void WorkerScriptController::attachDebugger(JSC::Debugger* debugger)
{
    initScriptIfNeeded();
    debugger->attach(m_workerGlobalScopeWrapper->globalObject());
}

void WorkerScriptController::detachDebugger(JSC::Debugger* debugger)
{
    debugger->detach(m_workerGlobalScopeWrapper->globalObject(), JSC::Debugger::TerminatingDebuggingSession);
}

} // namespace WebCore
