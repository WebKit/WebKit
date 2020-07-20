/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_PAINTING_API)

#include "DOMWrapperWorld.h"
#include "JSDOMGlobalObjectTask.h"
#include "JSDOMGuardedObject.h"
#include "JSWorkletGlobalScope.h"
#include "WorkletGlobalScope.h"
#include "WorkletScriptController.h"
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
    nullptr, // moduleLoaderImportModule
    nullptr, // moduleLoaderResolve
    nullptr, // moduleLoaderFetch
    nullptr, // moduleLoaderCreateImportMetaProperties
    nullptr, // moduleLoaderEvaluate
    &promiseRejectionTracker,
    &reportUncaughtExceptionAtEventLoop,
    &defaultLanguage,
    nullptr, // compileStreaming
    nullptr, // instantiateStreaming
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

void JSWorkletGlobalScopeBase::clearDOMGuardedObjects()
{
    auto guardedObjects = m_guardedObjects;
    for (auto& guarded : guardedObjects)
        guarded->clear();
}

void JSWorkletGlobalScopeBase::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSWorkletGlobalScopeBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_proxy);
}

void JSWorkletGlobalScopeBase::destroy(JSCell* cell)
{
    static_cast<JSWorkletGlobalScopeBase*>(cell)->JSWorkletGlobalScopeBase::~JSWorkletGlobalScopeBase();
}

ScriptExecutionContext* JSWorkletGlobalScopeBase::scriptExecutionContext() const
{
    return m_wrapped.get();
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
    auto* contextWrapper = workletGlobalScope.script()->workletGlobalScopeWrapper();
    if (!contextWrapper)
        return jsUndefined();
    return contextWrapper->proxy();
}

JSWorkletGlobalScope* toJSWorkletGlobalScope(VM& vm, JSValue value)
{
    if (!value.isObject())
        return nullptr;
    auto* classInfo = asObject(value)->classInfo(vm);

    if (classInfo == JSProxy::info())
        return jsDynamicCast<JSWorkletGlobalScope*>(vm, jsCast<JSProxy*>(asObject(value))->target());

    return nullptr;
}

} // namespace WebCore
#endif
