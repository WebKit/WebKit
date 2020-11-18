/*
 * Copyright (C) 2008-2020 Apple Inc. All Rights Reserved.
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
#include "WorkerOrWorkletScriptController.h"

#include "DedicatedWorkerGlobalScope.h"
#include "JSAudioWorkletGlobalScope.h"
#include "JSDOMBinding.h"
#include "JSDedicatedWorkerGlobalScope.h"
#include "JSEventTarget.h"
#include "JSExecState.h"
#include "JSPaintWorkletGlobalScope.h"
#include "JSServiceWorkerGlobalScope.h"
#include "ScriptSourceCode.h"
#include "WebCoreJSClientData.h"
#include "WorkerConsoleClient.h"
#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/DeferredWorkTimer.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {

using namespace JSC;

WorkerOrWorkletScriptController::WorkerOrWorkletScriptController(WorkerThreadType type, Ref<VM>&& vm, WorkerOrWorkletGlobalScope* globalScope)
    : m_vm(WTFMove(vm))
    , m_globalScope(globalScope)
    , m_globalScopeWrapper(*m_vm)
{
    m_vm->heap.acquireAccess(); // It's not clear that we have good discipline for heap access, so turn it on permanently.
    JSVMClientData::initNormalWorld(m_vm.get(), type);
}

WorkerOrWorkletScriptController::WorkerOrWorkletScriptController(WorkerThreadType type, WorkerOrWorkletGlobalScope* globalScope)
    : WorkerOrWorkletScriptController(type, JSC::VM::create(), globalScope)
{
}

WorkerOrWorkletScriptController::~WorkerOrWorkletScriptController()
{
    JSLockHolder lock(vm());
    if (m_globalScopeWrapper) {
        m_globalScopeWrapper->clearDOMGuardedObjects();
        m_globalScopeWrapper->setConsoleClient(nullptr);

    }
    m_globalScopeWrapper.clear();
    m_vm = nullptr;
}

void WorkerOrWorkletScriptController::attachDebugger(JSC::Debugger* debugger)
{
    initScriptIfNeeded();
    debugger->attach(m_globalScopeWrapper.get());
}

void WorkerOrWorkletScriptController::detachDebugger(JSC::Debugger* debugger)
{
    debugger->detach(m_globalScopeWrapper.get(), JSC::Debugger::TerminatingDebuggingSession);
}

void WorkerOrWorkletScriptController::forbidExecution()
{
    ASSERT(m_globalScope->isContextThread());
    m_executionForbidden = true;
}

bool WorkerOrWorkletScriptController::isExecutionForbidden() const
{
    ASSERT(m_globalScope->isContextThread());
    return m_executionForbidden;
}

void WorkerOrWorkletScriptController::scheduleExecutionTermination()
{
    if (m_isTerminatingExecution)
        return;

    {
        // The mutex provides a memory barrier to ensure that once
        // termination is scheduled, isTerminatingExecution() will
        // accurately reflect that lexicalGlobalObject when called from another thread.
        LockHolder locker(m_scheduledTerminationMutex);
        m_isTerminatingExecution = true;
    }
    m_vm->notifyNeedTermination();
}

bool WorkerOrWorkletScriptController::isTerminatingExecution() const
{
    // See comments in scheduleExecutionTermination regarding mutex usage.
    LockHolder locker(m_scheduledTerminationMutex);
    return m_isTerminatingExecution;
}

void WorkerOrWorkletScriptController::releaseHeapAccess()
{
    m_vm->heap.releaseAccess();
}

void WorkerOrWorkletScriptController::acquireHeapAccess()
{
    m_vm->heap.acquireAccess();
}

void WorkerOrWorkletScriptController::addTimerSetNotification(JSC::JSRunLoopTimer::TimerNotificationCallback callback)
{
    auto processTimer = [&] (JSRunLoopTimer* timer) {
        if (!timer)
            return;
        timer->addTimerSetNotification(callback);
    };

    processTimer(m_vm->heap.fullActivityCallback());
    processTimer(m_vm->heap.edenActivityCallback());
    processTimer(m_vm->deferredWorkTimer.ptr());
}

void WorkerOrWorkletScriptController::removeTimerSetNotification(JSC::JSRunLoopTimer::TimerNotificationCallback callback)
{
    auto processTimer = [&] (JSRunLoopTimer* timer) {
        if (!timer)
            return;
        timer->removeTimerSetNotification(callback);
    };

    processTimer(m_vm->heap.fullActivityCallback());
    processTimer(m_vm->heap.edenActivityCallback());
    processTimer(m_vm->deferredWorkTimer.ptr());
}

void WorkerOrWorkletScriptController::setException(JSC::Exception* exception)
{
    JSC::JSGlobalObject* lexicalGlobalObject = m_globalScopeWrapper.get();
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwException(lexicalGlobalObject, scope, exception);
}

void WorkerOrWorkletScriptController::disableEval(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock { vm() };

    m_globalScopeWrapper->setEvalEnabled(false, errorMessage);
}

void WorkerOrWorkletScriptController::disableWebAssembly(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock { vm() };

    m_globalScopeWrapper->setWebAssemblyEnabled(false, errorMessage);
}

void WorkerOrWorkletScriptController::evaluate(const ScriptSourceCode& sourceCode, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    NakedPtr<JSC::Exception> exception;
    evaluate(sourceCode, exception, returnedExceptionMessage);
    if (exception) {
        JSLockHolder lock(vm());
        reportException(m_globalScopeWrapper.get(), exception);
    }
}

void WorkerOrWorkletScriptController::evaluate(const ScriptSourceCode& sourceCode, NakedPtr<JSC::Exception>& returnedException, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    initScriptIfNeeded();

    auto& globalObject = *m_globalScopeWrapper.get();
    VM& vm = globalObject.vm();
    JSLockHolder lock { vm };

    JSExecState::profiledEvaluate(&globalObject, JSC::ProfilingReason::Other, sourceCode.jsSourceCode(), m_globalScopeWrapper->globalThis(), returnedException);

    if ((returnedException && isTerminatedExecutionException(vm, returnedException)) || isTerminatingExecution()) {
        forbidExecution();
        return;
    }

    if (returnedException) {
        if (m_globalScope->canIncludeErrorDetails(sourceCode.cachedScript(), sourceCode.url().string())) {
            // FIXME: It's not great that this can run arbitrary code to string-ify the value of the exception.
            // Do we need to do anything to handle that properly, if it, say, raises another exception?
            if (returnedExceptionMessage)
                *returnedExceptionMessage = returnedException->value().toWTFString(&globalObject);
        } else {
            // Overwrite the detailed error with a generic error.
            String genericErrorMessage { "Script error."_s };
            if (returnedExceptionMessage)
                *returnedExceptionMessage = genericErrorMessage;
            returnedException = JSC::Exception::create(vm, createError(&globalObject, genericErrorMessage));
        }
    }
}

template<typename JSGlobalScopePrototype, typename JSGlobalScope, typename GlobalScope>
void WorkerOrWorkletScriptController::initScriptWithSubclass()
{
    ASSERT(!m_globalScopeWrapper);

    JSLockHolder lock { vm() };

    // Explicitly protect the global object's prototype so it isn't collected
    // when we allocate the global object. (Once the global object is fully
    // constructed, it can mark its own prototype.)
    Structure* contextPrototypeStructure = JSGlobalScopePrototype::createStructure(*m_vm, nullptr, jsNull());
    auto* contextPrototype = JSGlobalScopePrototype::create(*m_vm, nullptr, contextPrototypeStructure);
    Structure* structure = JSGlobalScope::createStructure(*m_vm, nullptr, contextPrototype);
    auto* proxyStructure = JSProxy::createStructure(*m_vm, nullptr, jsNull(), PureForwardingProxyType);
    auto* proxy = JSProxy::create(*m_vm, proxyStructure);

    m_globalScopeWrapper.set(*m_vm, JSGlobalScope::create(*m_vm, structure, static_cast<GlobalScope&>(*m_globalScope), proxy));
    contextPrototypeStructure->setGlobalObject(*m_vm, m_globalScopeWrapper.get());
    ASSERT(structure->globalObject() == m_globalScopeWrapper);
    ASSERT(m_globalScopeWrapper->structure(*m_vm)->globalObject() == m_globalScopeWrapper);
    contextPrototype->structure(*m_vm)->setGlobalObject(*m_vm, m_globalScopeWrapper.get());
    auto* globalScopePrototype = JSGlobalScope::prototype(*m_vm, *m_globalScopeWrapper.get());
    globalScopePrototype->didBecomePrototype();
    contextPrototype->structure(*m_vm)->setPrototypeWithoutTransition(*m_vm, globalScopePrototype);

    proxy->setTarget(*m_vm, m_globalScopeWrapper.get());
    proxy->structure(*m_vm)->setGlobalObject(*m_vm, m_globalScopeWrapper.get());

    ASSERT(m_globalScopeWrapper->globalObject() == m_globalScopeWrapper);
    ASSERT(asObject(m_globalScopeWrapper->getPrototypeDirect(*m_vm))->globalObject() == m_globalScopeWrapper);

    m_consoleClient = makeUnique<WorkerConsoleClient>(*m_globalScope);
    m_globalScopeWrapper->setConsoleClient(m_consoleClient.get());
}

void WorkerOrWorkletScriptController::initScript()
{
    if (is<DedicatedWorkerGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSDedicatedWorkerGlobalScopePrototype, JSDedicatedWorkerGlobalScope, DedicatedWorkerGlobalScope>();
        return;
    }

#if ENABLE(SERVICE_WORKER)
    if (is<ServiceWorkerGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSServiceWorkerGlobalScopePrototype, JSServiceWorkerGlobalScope, ServiceWorkerGlobalScope>();
        return;
    }
#endif

#if ENABLE(CSS_PAINTING_API)
    if (is<PaintWorkletGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSPaintWorkletGlobalScopePrototype, JSPaintWorkletGlobalScope, PaintWorkletGlobalScope>();
        return;
    }
#endif

#if ENABLE(WEB_AUDIO)
    if (is<AudioWorkletGlobalScope>(m_globalScope)) {
        initScriptWithSubclass<JSAudioWorkletGlobalScopePrototype, JSAudioWorkletGlobalScope, AudioWorkletGlobalScope>();
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

} // namespace WebCore
