/*
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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
#include "runtime_method.h"

#include "JSDOMBinding.h"
#include "JSHTMLElement.h"
#include "JSPluginElementFunctions.h"
#include "WebCoreJSClientData.h"
#include "runtime_object.h"
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/FunctionPrototype.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>

using namespace WebCore;

namespace JSC {

using namespace Bindings;

WEBCORE_EXPORT const ClassInfo RuntimeMethod::s_info = { "RuntimeMethod", &InternalFunction::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RuntimeMethod) };

static EncodedJSValue JSC_HOST_CALL callRuntimeMethod(JSGlobalObject*, CallFrame*);

RuntimeMethod::RuntimeMethod(VM& vm, Structure* structure, Method* method)
    // Callers will need to pass in the right global object corresponding to this native object "method".
    : InternalFunction(vm, structure, callRuntimeMethod, nullptr)
    , m_method(method)
{
}

void RuntimeMethod::finishCreation(VM& vm, const String& ident)
{
    Base::finishCreation(vm, ident);
    ASSERT(inherits(vm, info()));
}

EncodedJSValue RuntimeMethod::lengthGetter(JSGlobalObject* exec, EncodedJSValue thisValue, PropertyName)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeMethod* thisObject = jsDynamicCast<RuntimeMethod*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(exec, scope);
    return JSValue::encode(jsNumber(thisObject->m_method->numParameters()));
}

bool RuntimeMethod::getOwnPropertySlot(JSObject* object, JSGlobalObject* exec, PropertyName propertyName, PropertySlot &slot)
{
    VM& vm = exec->vm();
    RuntimeMethod* thisObject = jsCast<RuntimeMethod*>(object);
    if (propertyName == vm.propertyNames->length) {
        slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, thisObject->lengthGetter);
        return true;
    }
    
    return InternalFunction::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

IsoSubspace* RuntimeMethod::subspaceForImpl(VM& vm)
{
    return &static_cast<JSVMClientData*>(vm.clientData)->runtimeMethodSpace();
}

static EncodedJSValue JSC_HOST_CALL callRuntimeMethod(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeMethod* method = static_cast<RuntimeMethod*>(callFrame->jsCallee());

    if (!method->method())
        return JSValue::encode(jsUndefined());

    RefPtr<Instance> instance;

    JSValue thisValue = callFrame->thisValue();
    if (thisValue.inherits<RuntimeObject>(vm)) {
        RuntimeObject* runtimeObject = static_cast<RuntimeObject*>(asObject(thisValue));
        instance = runtimeObject->getInternalInstance();
        if (!instance) 
            return JSValue::encode(RuntimeObject::throwInvalidAccessError(globalObject, scope));
    } else {
        // Calling a runtime object of a plugin element?
        if (thisValue.inherits<JSHTMLElement>(vm))
            instance = pluginInstance(jsCast<JSHTMLElement*>(asObject(thisValue))->wrapped());
        if (!instance)
            return throwVMTypeError(globalObject, scope);
    }
    ASSERT(instance);

    instance->begin();
    JSValue result = instance->invokeMethod(globalObject, callFrame, method);
    instance->end();
    return JSValue::encode(result);
}

}
