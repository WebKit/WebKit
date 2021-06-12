/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include "JSWorkletGlobalScopeBase.h"

#include "DOMWrapperWorld.h"
#include "JSDOMGlobalObjectTask.h"
#include "JSDOMGuardedObject.h"
#include "WorkerOrWorkletScriptController.h"
#include "WorkletGlobalScope.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/Language.h>

namespace WebCore {

using namespace JSC;

const ClassInfo JSWorkletGlobalScopeBase::s_info = { "WorkletGlobalScope", &JSDOMGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWorkletGlobalScopeBase) };

const GlobalObjectMethodTable JSWorkletGlobalScopeBase::s_globalObjectMethodTable = {
    &supportsRichSourceInfo,
    &shouldInterruptScript,
    &javaScriptRuntimeFlags,
    nullptr, // queueMicrotaskToEventLoop
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
    [] { return defaultLanguage(); },
#if ENABLE(WEBASSEMBLY)
    &compileStreaming,
    &instantiateStreaming,
#else
    nullptr,
    nullptr,
#endif
};

JSWorkletGlobalScopeBase::JSWorkletGlobalScopeBase(JSC::VM& vm, JSC::Structure* structure, RefPtr<WorkletGlobalScope>&& impl)
    : JSDOMGlobalObject(vm, structure, normalWorld(vm), &s_globalObjectMethodTable)
    , m_wrapped(WTFMove(impl))
{
}

void JSWorkletGlobalScopeBase::finishCreation(VM& vm, JSProxy* proxy)
{
    m_proxy.set(vm, this, proxy);

    Base::finishCreation(vm, m_proxy.get());
    ASSERT(inherits(vm, info()));
}

template<typename Visitor>
void JSWorkletGlobalScopeBase::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSWorkletGlobalScopeBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_proxy);
}

DEFINE_VISIT_CHILDREN(JSWorkletGlobalScopeBase);

void JSWorkletGlobalScopeBase::destroy(JSCell* cell)
{
    static_cast<JSWorkletGlobalScopeBase*>(cell)->JSWorkletGlobalScopeBase::~JSWorkletGlobalScopeBase();
}

ScriptExecutionContext* JSWorkletGlobalScopeBase::scriptExecutionContext() const
{
    return m_wrapped.get();
}

JSC::ScriptExecutionStatus JSWorkletGlobalScopeBase::scriptExecutionStatus(JSC::JSGlobalObject* globalObject, JSC::JSObject* owner)
{
    ASSERT_UNUSED(owner, globalObject == owner);
    return jsCast<JSWorkletGlobalScopeBase*>(globalObject)->scriptExecutionContext()->jscScriptExecutionStatus();
}

bool JSWorkletGlobalScopeBase::supportsRichSourceInfo(const JSGlobalObject* object)
{
    return JSGlobalObject::supportsRichSourceInfo(object);
}

bool JSWorkletGlobalScopeBase::shouldInterruptScript(const JSGlobalObject* object)
{
    return JSGlobalObject::shouldInterruptScript(object);
}

bool JSWorkletGlobalScopeBase::shouldInterruptScriptBeforeTimeout(const JSGlobalObject* object)
{
    return JSGlobalObject::shouldInterruptScriptBeforeTimeout(object);
}

RuntimeFlags JSWorkletGlobalScopeBase::javaScriptRuntimeFlags(const JSGlobalObject* object)
{
    auto* thisObject = jsCast<const JSWorkletGlobalScopeBase*>(object);
    return thisObject->m_wrapped->jsRuntimeFlags();
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, WorkletGlobalScope& workletGlobalScope)
{
    return toJS(lexicalGlobalObject, workletGlobalScope);
}

JSValue toJS(JSGlobalObject*, WorkletGlobalScope& workletGlobalScope)
{
    if (!workletGlobalScope.script())
        return jsUndefined();
    auto* contextWrapper = workletGlobalScope.script()->globalScopeWrapper();
    if (!contextWrapper)
        return jsUndefined();
    return &contextWrapper->proxy();
}

} // namespace WebCore
