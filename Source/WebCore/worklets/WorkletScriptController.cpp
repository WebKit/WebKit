/*
 * Copyright (C) 2018 Apple Inc. All Rights Reserved.
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
#include "WorkletScriptController.h"

#if ENABLE(CSS_PAINTING_API)

#include "JSDOMBinding.h"
#include "JSEventTarget.h"
#include "JSExecState.h"
#include "JSPaintWorkletGlobalScope.h"
#include "ScriptSourceCode.h"
#include "WebCoreJSClientData.h"
#include "WorkletConsoleClient.h"

#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/GCActivityCallback.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/PromiseDeferredTimer.h>
#include <JavaScriptCore/StrongInlines.h>

namespace WebCore {
using namespace JSC;

WorkletScriptController::WorkletScriptController(WorkletGlobalScope* workletGlobalScope)
    : m_vm(VM::create())
    , m_workletGlobalScope(workletGlobalScope)
    , m_workletGlobalScopeWrapper(*m_vm)
{
    m_vm->heap.acquireAccess(); // It's not clear that we have good discipline for heap access, so turn it on permanently.
    JSVMClientData::initNormalWorld(m_vm.get());
}

WorkletScriptController::~WorkletScriptController()
{
    JSLockHolder lock(vm());
    m_workletGlobalScopeWrapper.clear();
    m_vm = nullptr;
    if (m_workletGlobalScopeWrapper) {
        m_workletGlobalScopeWrapper->clearDOMGuardedObjects();
        m_workletGlobalScopeWrapper->setConsoleClient(nullptr);
        m_consoleClient = nullptr;
    }
}

void WorkletScriptController::forbidExecution()
{
    ASSERT(m_workletGlobalScope->isContextThread());
    m_executionForbidden = true;
}

bool WorkletScriptController::isExecutionForbidden() const
{
    ASSERT(m_workletGlobalScope->isContextThread());
    return m_executionForbidden;
}

void WorkletScriptController::disableEval(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock { vm() };

    m_workletGlobalScopeWrapper->setEvalEnabled(false, errorMessage);
}

void WorkletScriptController::disableWebAssembly(const String& errorMessage)
{
    initScriptIfNeeded();
    JSLockHolder lock { vm() };

    m_workletGlobalScopeWrapper->setWebAssemblyEnabled(false, errorMessage);
}


template<typename JSGlobalScopePrototype, typename JSGlobalScope, typename GlobalScope>
void WorkletScriptController::initScriptWithSubclass()
{
    ASSERT(!m_workletGlobalScopeWrapper);

    JSLockHolder lock { vm() };

    // Explicitly protect the global object's prototype so it isn't collected
    // when we allocate the global object. (Once the global object is fully
    // constructed, it can mark its own prototype.)
    Structure* contextPrototypeStructure = JSGlobalScopePrototype::createStructure(*m_vm, nullptr, jsNull());
    auto* contextPrototype = JSGlobalScopePrototype::create(*m_vm, nullptr, contextPrototypeStructure);
    Structure* structure = JSGlobalScope::createStructure(*m_vm, nullptr, contextPrototype);
    auto* proxyStructure = JSProxy::createStructure(*m_vm, nullptr, jsNull(), PureForwardingProxyType);
    auto* proxy = JSProxy::create(*m_vm, proxyStructure);

    m_workletGlobalScopeWrapper.set(*m_vm, JSGlobalScope::create(*m_vm, structure, static_cast<GlobalScope&>(*m_workletGlobalScope), proxy));
    contextPrototypeStructure->setGlobalObject(*m_vm, m_workletGlobalScopeWrapper.get());
    ASSERT(structure->globalObject() == m_workletGlobalScopeWrapper);
    ASSERT(m_workletGlobalScopeWrapper->structure(*m_vm)->globalObject() == m_workletGlobalScopeWrapper);
    contextPrototype->structure(*m_vm)->setGlobalObject(*m_vm, m_workletGlobalScopeWrapper.get());
    contextPrototype->structure(*m_vm)->setPrototypeWithoutTransition(*m_vm, JSGlobalScope::prototype(*m_vm, *m_workletGlobalScopeWrapper.get()));

    proxy->setTarget(*m_vm, m_workletGlobalScopeWrapper.get());
    proxy->structure(*m_vm)->setGlobalObject(*m_vm, m_workletGlobalScopeWrapper.get());

    ASSERT(m_workletGlobalScopeWrapper->globalObject() == m_workletGlobalScopeWrapper);
    ASSERT(asObject(m_workletGlobalScopeWrapper->getPrototypeDirect(*m_vm))->globalObject() == m_workletGlobalScopeWrapper);

    m_consoleClient = std::make_unique<WorkletConsoleClient>(*m_workletGlobalScope);
    m_workletGlobalScopeWrapper->setConsoleClient(m_consoleClient.get());
}

void WorkletScriptController::initScript()
{
    if (is<PaintWorkletGlobalScope>(m_workletGlobalScope)) {
        initScriptWithSubclass<JSPaintWorkletGlobalScopePrototype, JSPaintWorkletGlobalScope, PaintWorkletGlobalScope>();
        return;
    }

    ASSERT_NOT_REACHED();
}

void WorkletScriptController::evaluate(const ScriptSourceCode& sourceCode, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    NakedPtr<JSC::Exception> exception;
    evaluate(sourceCode, exception, returnedExceptionMessage);
    if (exception) {
        JSLockHolder lock(vm());
        reportException(m_workletGlobalScopeWrapper->globalExec(), exception);
    }
}

// Important: The caller of this function must verify that the returned exception message does not violate CORS if it is going to be passed back to JS.
void WorkletScriptController::evaluate(const ScriptSourceCode& sourceCode, NakedPtr<JSC::Exception>& returnedException, String* returnedExceptionMessage)
{
    if (isExecutionForbidden())
        return;

    initScriptIfNeeded();

    auto& state = *m_workletGlobalScopeWrapper->globalExec();
    VM& vm = state.vm();
    JSLockHolder lock { vm };

    JSExecState::profiledEvaluate(&state, JSC::ProfilingReason::Other, sourceCode.jsSourceCode(), m_workletGlobalScopeWrapper->globalThis(), returnedException);

    if (returnedException && returnedExceptionMessage) {
        // This FIXME is from WorkerScriptController.
        // FIXME: It's not great that this can run arbitrary code to string-ify the value of the exception.
        // Do we need to do anything to handle that properly, if it, say, raises another exception?
        *returnedExceptionMessage = returnedException->value().toWTFString(&state);
    }
}

void WorkletScriptController::setException(JSC::Exception* exception)
{
    auto* exec = m_workletGlobalScopeWrapper->globalExec();
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwException(exec, scope, exception);
}

} // namespace WebCore
#endif
