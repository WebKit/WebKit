/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 *
 */

#include "config.h"
#include "JSWorkerGlobalScopeBase.h"

#include "DOMWrapperWorld.h"
#include "EventLoop.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGuardedObject.h"
#include "JSMicrotaskCallback.h"
#include "JSTrustedScript.h"
#include "TrustedType.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <JavaScriptCore/GlobalObjectMethodTable.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalProxy.h>
#include <JavaScriptCore/Microtask.h>
#include <wtf/Language.h>

namespace WebCore {
using namespace JSC;

const ClassInfo JSWorkerGlobalScopeBase::s_info = { "WorkerGlobalScope"_s, &JSDOMGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWorkerGlobalScopeBase) };

const GlobalObjectMethodTable* JSWorkerGlobalScopeBase::globalObjectMethodTable()
{
    static constexpr GlobalObjectMethodTable table = {
        &supportsRichSourceInfo,
        &shouldInterruptScript,
        &javaScriptRuntimeFlags,
        &queueMicrotaskToEventLoop,
        &shouldInterruptScriptBeforeTimeout,
        &moduleLoaderImportModule,
        &moduleLoaderResolve,
        &moduleLoaderFetch,
        &moduleLoaderCreateImportMetaProperties,
        &moduleLoaderEvaluate,
        &promiseRejectionTracker,
        &reportUncaughtExceptionAtEventLoop,
        &currentScriptExecutionOwner,
        &scriptExecutionStatus,
        &reportViolationForUnsafeEval,
        [] { return defaultLanguage(); },
#if ENABLE(WEBASSEMBLY)
        &compileStreaming,
        &instantiateStreaming,
#else
        nullptr,
        nullptr,
#endif
        deriveShadowRealmGlobalObject,
        codeForEval,
        canCompileStrings,
        trustedScriptStructure,
    };
    return &table;
};

JSWorkerGlobalScopeBase::JSWorkerGlobalScopeBase(JSC::VM& vm, JSC::Structure* structure, RefPtr<WorkerGlobalScope>&& impl)
    : JSDOMGlobalObject(vm, structure, normalWorld(vm), globalObjectMethodTable())
    , m_wrapped(WTFMove(impl))
{
}

void JSWorkerGlobalScopeBase::finishCreation(VM& vm, JSGlobalProxy* proxy)
{
    m_proxy.set(vm, this, proxy);

    Base::finishCreation(vm, m_proxy.get());
    ASSERT(inherits(info()));
}

template<typename Visitor>
void JSWorkerGlobalScopeBase::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSWorkerGlobalScopeBase* thisObject = jsCast<JSWorkerGlobalScopeBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_proxy);
}

DEFINE_VISIT_CHILDREN(JSWorkerGlobalScopeBase);

void JSWorkerGlobalScopeBase::destroy(JSCell* cell)
{
    static_cast<JSWorkerGlobalScopeBase*>(cell)->JSWorkerGlobalScopeBase::~JSWorkerGlobalScopeBase();
}

ScriptExecutionContext* JSWorkerGlobalScopeBase::scriptExecutionContext() const
{
    return m_wrapped.get();
}

bool JSWorkerGlobalScopeBase::supportsRichSourceInfo(const JSGlobalObject* object)
{
    return JSGlobalObject::supportsRichSourceInfo(object);
}

bool JSWorkerGlobalScopeBase::shouldInterruptScript(const JSGlobalObject* object)
{
    return JSGlobalObject::shouldInterruptScript(object);
}

bool JSWorkerGlobalScopeBase::shouldInterruptScriptBeforeTimeout(const JSGlobalObject* object)
{
    return JSGlobalObject::shouldInterruptScriptBeforeTimeout(object);
}

RuntimeFlags JSWorkerGlobalScopeBase::javaScriptRuntimeFlags(const JSGlobalObject* object)
{
    const JSWorkerGlobalScopeBase *thisObject = jsCast<const JSWorkerGlobalScopeBase*>(object);
    return thisObject->m_wrapped->thread().runtimeFlags();
}

JSC::ScriptExecutionStatus JSWorkerGlobalScopeBase::scriptExecutionStatus(JSC::JSGlobalObject* globalObject, JSC::JSObject* owner)
{
    ASSERT_UNUSED(owner, globalObject == owner);
    return jsCast<JSWorkerGlobalScopeBase*>(globalObject)->scriptExecutionContext()->jscScriptExecutionStatus();
}

void JSWorkerGlobalScopeBase::reportViolationForUnsafeEval(JSC::JSGlobalObject* globalObject, const String& source)
{
    return JSGlobalObject::reportViolationForUnsafeEval(globalObject, source);
}

void JSWorkerGlobalScopeBase::queueMicrotaskToEventLoop(JSGlobalObject& object, Ref<JSC::Microtask>&& task)
{
    JSWorkerGlobalScopeBase& thisObject = static_cast<JSWorkerGlobalScopeBase&>(object);

    auto callback = JSMicrotaskCallback::create(thisObject, WTFMove(task));
    auto& context = thisObject.wrapped();
    context.eventLoop().queueMicrotask([callback = WTFMove(callback)]() mutable {
        callback->call();
    });
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, WorkerGlobalScope& workerGlobalScope)
{
    return toJS(lexicalGlobalObject, workerGlobalScope);
}

JSValue toJS(JSGlobalObject*, WorkerGlobalScope& workerGlobalScope)
{
    auto* script = workerGlobalScope.script();
    if (!script)
        return jsNull();
    auto* contextWrapper = script->globalScopeWrapper();
    ASSERT(contextWrapper);
    return &contextWrapper->proxy();
}

} // namespace WebCore
