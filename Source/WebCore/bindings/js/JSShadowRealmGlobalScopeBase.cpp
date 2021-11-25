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

#include "DOMWrapperWorld.h"
#include "EventLoop.h"
#include "JSDOMGlobalObjectTask.h"
#include "JSDOMGuardedObject.h"
#include "JSShadowRealmGlobalScope.h"
#include "JSMicrotaskCallback.h"
#include "ShadowRealmGlobalScope.h"
#include "ShadowRealmScriptController.h"
#include "WorkerThread.h"
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
    nullptr, // deriveShadowRealmGlobalObject
};

JSShadowRealmGlobalScopeBase::JSShadowRealmGlobalScopeBase(JSC::VM& vm, JSC::Structure* structure, RefPtr<ShadowRealmGlobalScope>&& impl)
    : JSDOMGlobalObject(vm, structure, normalWorld(vm), &s_globalObjectMethodTable)
    , m_wrapped(WTFMove(impl))
{
}

void JSShadowRealmGlobalScopeBase::finishCreation(VM& vm, JSProxy* proxy)
{
    m_proxy.set(vm, this, proxy);

    // TODO(jgriego) definitely _not_ an ideal solution
    ASSERT(inherits<JSShadowRealmGlobalScope>(vm));
    ASSERT(!m_wrapped->m_scriptController);
    m_wrapped->m_scriptController = std::make_unique<ShadowRealmScriptController>(Ref(vm),
                                                                                 jsCast<JSShadowRealmGlobalScope*>(this));


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

void JSShadowRealmGlobalScopeBase::destroy(JSCell* cell)
{
    static_cast<JSShadowRealmGlobalScopeBase*>(cell)->JSShadowRealmGlobalScopeBase::~JSShadowRealmGlobalScopeBase();
}

ScriptExecutionContext* JSShadowRealmGlobalScopeBase::scriptExecutionContext() const
{
    return m_wrapped.get();
}

bool JSShadowRealmGlobalScopeBase::supportsRichSourceInfo(const JSGlobalObject* object)
{
    return JSGlobalObject::supportsRichSourceInfo(object);
}

bool JSShadowRealmGlobalScopeBase::shouldInterruptScript(const JSGlobalObject* object)
{
    return JSGlobalObject::shouldInterruptScript(object);
}

bool JSShadowRealmGlobalScopeBase::shouldInterruptScriptBeforeTimeout(const JSGlobalObject* object)
{
    return JSGlobalObject::shouldInterruptScriptBeforeTimeout(object);
}

RuntimeFlags JSShadowRealmGlobalScopeBase::javaScriptRuntimeFlags(const JSGlobalObject* object)
{
    const JSShadowRealmGlobalScopeBase *thisObject = jsCast<const JSShadowRealmGlobalScopeBase*>(object);
    auto const incubatingGlobalObj = thisObject->m_wrapped->m_incubatingWrapper;

    return incubatingGlobalObj->globalObjectMethodTable()->javaScriptRuntimeFlags(incubatingGlobalObj.get());
}

JSC::ScriptExecutionStatus JSShadowRealmGlobalScopeBase::scriptExecutionStatus(JSC::JSGlobalObject* globalObject, JSC::JSObject* owner)
{
    ASSERT_UNUSED(owner, globalObject == owner);
    return jsCast<JSShadowRealmGlobalScopeBase*>(globalObject)->scriptExecutionContext()->jscScriptExecutionStatus();
}

void JSShadowRealmGlobalScopeBase::reportViolationForUnsafeEval(JSC::JSGlobalObject* globalObject)
{
    return JSGlobalObject::reportViolationForUnsafeEval(globalObject);
}

void JSShadowRealmGlobalScopeBase::queueMicrotaskToEventLoop(JSGlobalObject& object, Ref<JSC::Microtask>&& task)
{
    JSShadowRealmGlobalScopeBase& thisObject = static_cast<JSShadowRealmGlobalScopeBase&>(object);

    auto callback = JSMicrotaskCallback::create(thisObject, WTFMove(task));
    auto context = thisObject.wrapped().enclosingContext();
    context->eventLoop().queueMicrotask([callback = WTFMove(callback)]() mutable {
        callback->call();
    });
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, ShadowRealmGlobalScope& workerGlobalScope)
{
    CRASH();
    //return toJS(lexicalGlobalObject, workerGlobalScope);
}

JSValue toJS(JSGlobalObject*, ShadowRealmGlobalScope& workerGlobalScope)
{
    CRASH();
    //auto* script = workerGlobalScope.script();
    //if (!script)
    //return jsNull();
    //auto* contextWrapper = script->globalScopeWrapper();
    //ASSERT(contextWrapper);
    //return &contextWrapper->proxy();
}

} // namespace WebCore
