/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSDOMGlobalObjectTask.h"
#include "JSDedicatedWorkerGlobalScope.h"
#include "JSWorkerGlobalScope.h"
#include "Language.h"
#include "WorkerGlobalScope.h"
#include <runtime/JSCInlines.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/Microtask.h>

using namespace JSC;

namespace WebCore {

const ClassInfo JSWorkerGlobalScopeBase::s_info = { "WorkerGlobalScope", &JSDOMGlobalObject::s_info, 0, CREATE_METHOD_TABLE(JSWorkerGlobalScopeBase) };

const GlobalObjectMethodTable JSWorkerGlobalScopeBase::s_globalObjectMethodTable = { &allowsAccessFrom, &supportsRichSourceInfo, &shouldInterruptScript, &javaScriptRuntimeFlags, &queueTaskToEventLoop, &shouldInterruptScriptBeforeTimeout, nullptr, nullptr, nullptr, nullptr, nullptr, &defaultLanguage };

JSWorkerGlobalScopeBase::JSWorkerGlobalScopeBase(JSC::VM& vm, JSC::Structure* structure, RefPtr<WorkerGlobalScope>&& impl)
    : JSDOMGlobalObject(vm, structure, normalWorld(vm), &s_globalObjectMethodTable)
    , m_wrapped(WTFMove(impl))
{
}

void JSWorkerGlobalScopeBase::finishCreation(VM& vm, JSProxy* proxy)
{
    m_proxy.set(vm, this, proxy);

    Base::finishCreation(vm, m_proxy.get());
    ASSERT(inherits(info()));
}

void JSWorkerGlobalScopeBase::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSWorkerGlobalScopeBase* thisObject = jsCast<JSWorkerGlobalScopeBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_proxy);
}

void JSWorkerGlobalScopeBase::destroy(JSCell* cell)
{
    static_cast<JSWorkerGlobalScopeBase*>(cell)->JSWorkerGlobalScopeBase::~JSWorkerGlobalScopeBase();
}

ScriptExecutionContext* JSWorkerGlobalScopeBase::scriptExecutionContext() const
{
    return m_wrapped.get();
}

bool JSWorkerGlobalScopeBase::allowsAccessFrom(const JSGlobalObject* object, ExecState* exec)
{
    return JSGlobalObject::allowsAccessFrom(object, exec);
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
    return JSGlobalObject::javaScriptRuntimeFlags(object);
}

void JSWorkerGlobalScopeBase::queueTaskToEventLoop(const JSGlobalObject* object, Ref<JSC::Microtask>&& task)
{
    const JSWorkerGlobalScopeBase* thisObject = static_cast<const JSWorkerGlobalScopeBase*>(object);
    thisObject->scriptExecutionContext()->postTask(JSGlobalObjectTask((JSDOMGlobalObject*)thisObject, WTFMove(task)));
}

JSValue toJS(ExecState* exec, JSDOMGlobalObject*, WorkerGlobalScope& workerGlobalScope)
{
    return toJS(exec, workerGlobalScope);
}

JSValue toJS(ExecState*, WorkerGlobalScope& workerGlobalScope)
{
    WorkerScriptController* script = workerGlobalScope.script();
    if (!script)
        return jsNull();
    JSWorkerGlobalScope* contextWrapper = script->workerGlobalScopeWrapper();
    ASSERT(contextWrapper);
    return contextWrapper->proxy();
}

JSDedicatedWorkerGlobalScope* toJSDedicatedWorkerGlobalScope(JSValue value)
{
    if (!value.isObject())
        return 0;
    const ClassInfo* classInfo = asObject(value)->classInfo();
    if (classInfo == JSDedicatedWorkerGlobalScope::info())
        return jsCast<JSDedicatedWorkerGlobalScope*>(asObject(value));
    if (classInfo == JSProxy::info())
        return jsDynamicCast<JSDedicatedWorkerGlobalScope*>(jsCast<JSProxy*>(asObject(value))->target());
    return 0;
}


JSWorkerGlobalScope* toJSWorkerGlobalScope(JSValue value)
{
    return toJSDedicatedWorkerGlobalScope(value);
}

} // namespace WebCore
