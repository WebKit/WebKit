/*
 * Copyright (C) 2021 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSShadowRealmGlobalScopeBase.h"

#include "EventLoop.h"
#include "JSMicrotaskCallback.h"
#include "JSShadowRealmGlobalScope.h"
#include "ScriptModuleLoader.h"
#include "ShadowRealmGlobalScope.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSProxy.h>
#include <JavaScriptCore/Microtask.h>
#include <wtf/Language.h>

namespace WebCore {

using namespace JSC;

const ClassInfo JSShadowRealmGlobalScopeBase::s_info = { "ShadowRealmGlobalScope", &JSDOMGlobalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSShadowRealmGlobalScopeBase) };

const GlobalObjectMethodTable JSShadowRealmGlobalScopeBase::s_globalObjectMethodTable = {
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
    &deriveShadowRealmGlobalObject,
};

JSShadowRealmGlobalScopeBase::JSShadowRealmGlobalScopeBase(JSC::VM& vm, JSC::Structure* structure, RefPtr<ShadowRealmGlobalScope>&& impl)
    : JSDOMGlobalObject(vm, structure, normalWorld(vm), &s_globalObjectMethodTable)
    , m_wrapped(WTFMove(impl))
{
}

void JSShadowRealmGlobalScopeBase::finishCreation(VM& vm, JSProxy* proxy)
{
    m_proxy.set(vm, this, proxy);
    m_wrapped->m_wrapper = JSC::Weak(this);
    Base::finishCreation(vm, m_proxy.get());
    ASSERT(inherits(vm, info()));
}

template<typename Visitor>
void JSShadowRealmGlobalScopeBase::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSShadowRealmGlobalScopeBase* thisObject = jsCast<JSShadowRealmGlobalScopeBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_proxy);
}

DEFINE_VISIT_CHILDREN(JSShadowRealmGlobalScopeBase);

ScriptExecutionContext* JSShadowRealmGlobalScopeBase::scriptExecutionContext() const
{
    return incubatingRealm()->scriptExecutionContext();
}

const JSDOMGlobalObject* JSShadowRealmGlobalScopeBase::incubatingRealm() const
{
    auto incubatingWrapper = m_wrapped->m_incubatingWrapper.get();
    ASSERT(incubatingWrapper);
    return incubatingWrapper;
}

void JSShadowRealmGlobalScopeBase::destroy(JSCell* cell)
{
    static_cast<JSShadowRealmGlobalScopeBase*>(cell)->JSShadowRealmGlobalScopeBase::~JSShadowRealmGlobalScopeBase();
}

bool JSShadowRealmGlobalScopeBase::supportsRichSourceInfo(const JSGlobalObject* object)
{
    auto incubating = jsCast<const JSShadowRealmGlobalScopeBase*>(object)->incubatingRealm();
    return incubating->globalObjectMethodTable()->supportsRichSourceInfo(incubating);
}

bool JSShadowRealmGlobalScopeBase::shouldInterruptScript(const JSGlobalObject* object)
{
    auto incubating = jsCast<const JSShadowRealmGlobalScopeBase*>(object)->incubatingRealm();
    return incubating->globalObjectMethodTable()->shouldInterruptScript(incubating);
}

bool JSShadowRealmGlobalScopeBase::shouldInterruptScriptBeforeTimeout(const JSGlobalObject* object)
{
    auto incubating = jsCast<const JSShadowRealmGlobalScopeBase*>(object)->incubatingRealm();
    return incubating->globalObjectMethodTable()->shouldInterruptScriptBeforeTimeout(incubating);
}

RuntimeFlags JSShadowRealmGlobalScopeBase::javaScriptRuntimeFlags(const JSGlobalObject* object)
{
    auto incubating = jsCast<const JSShadowRealmGlobalScopeBase*>(object)->incubatingRealm();
    return incubating->globalObjectMethodTable()->javaScriptRuntimeFlags(incubating);
}

JSC::ScriptExecutionStatus JSShadowRealmGlobalScopeBase::scriptExecutionStatus(JSC::JSGlobalObject* globalObject, JSC::JSObject* owner)
{
    auto incubating = jsCast<JSShadowRealmGlobalScopeBase*>(globalObject)->incubatingRealm();
    return incubating->globalObjectMethodTable()->scriptExecutionStatus(incubating, owner);
}

void JSShadowRealmGlobalScopeBase::reportViolationForUnsafeEval(JSC::JSGlobalObject* globalObject, JSC::JSString* msg)
{
    auto incubating = jsCast<JSShadowRealmGlobalScopeBase*>(globalObject)->incubatingRealm();
    incubating->globalObjectMethodTable()->reportViolationForUnsafeEval(incubating, msg);
}

void JSShadowRealmGlobalScopeBase::queueMicrotaskToEventLoop(JSGlobalObject& object, Ref<JSC::Microtask>&& task)
{
    auto incubating = jsCast<JSShadowRealmGlobalScopeBase*>(&object)->incubatingRealm();
    incubating->globalObjectMethodTable()->queueMicrotaskToEventLoop(*incubating, WTFMove(task));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, ShadowRealmGlobalScope& realmGlobalScope)
{
    return toJS(lexicalGlobalObject, realmGlobalScope);
}

JSValue toJS(JSGlobalObject*, ShadowRealmGlobalScope& realmGlobalScope)
{
    ASSERT(realmGlobalScope.wrapper());
    return &realmGlobalScope.wrapper()->proxy();
}

} // namespace WebCore
