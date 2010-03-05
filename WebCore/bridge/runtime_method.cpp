/*
 * Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "runtime_object.h"
#include <runtime/Error.h>
#include <runtime/FunctionPrototype.h>

using namespace WebCore;

namespace JSC {

using namespace Bindings;

ASSERT_CLASS_FITS_IN_CELL(RuntimeMethod);

const ClassInfo RuntimeMethod::s_info = { "RuntimeMethod", &InternalFunction::info, 0, 0 };

RuntimeMethod::RuntimeMethod(ExecState* exec, const Identifier& ident, Bindings::MethodList& m)
    // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object
    // exec-globalData() is also likely wrong.
    // Callers will need to pass in the right global object corresponding to this native object "m".
    : InternalFunction(&exec->globalData(), deprecatedGetDOMStructure<RuntimeMethod>(exec), ident)
    , _methodList(new MethodList(m))
{
}

JSValue RuntimeMethod::lengthGetter(ExecState* exec, JSValue slotBase, const Identifier&)
{
    RuntimeMethod* thisObj = static_cast<RuntimeMethod*>(asObject(slotBase));

    // Ick!  There may be more than one method with this name.  Arbitrarily
    // just pick the first method.  The fundamental problem here is that 
    // JavaScript doesn't have the notion of method overloading and
    // Java does.
    // FIXME: a better solution might be to give the maximum number of parameters
    // of any method
    return jsNumber(exec, thisObj->_methodList->at(0)->numParameters());
}

bool RuntimeMethod::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setCacheableCustom(this, lengthGetter);
        return true;
    }
    
    return InternalFunction::getOwnPropertySlot(exec, propertyName, slot);
}

bool RuntimeMethod::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor &descriptor)
{
    if (propertyName == exec->propertyNames().length) {
        PropertySlot slot;
        slot.setCustom(this, lengthGetter);
        descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
        return true;
    }
    
    return InternalFunction::getOwnPropertyDescriptor(exec, propertyName, descriptor);
}

static JSValue JSC_HOST_CALL callRuntimeMethod(ExecState* exec, JSObject* function, JSValue thisValue, const ArgList& args)
{
    RuntimeMethod* method = static_cast<RuntimeMethod*>(function);

    if (method->methods()->isEmpty())
        return jsUndefined();

    RefPtr<Instance> instance;

    if (thisValue.inherits(&RuntimeObject::s_info)) {
        RuntimeObject* runtimeObject = static_cast<RuntimeObject*>(asObject(thisValue));
        instance = runtimeObject->getInternalInstance();
        if (!instance) 
            return RuntimeObject::throwInvalidAccessError(exec);
    } else {
        // Calling a runtime object of a plugin element?
        if (thisValue.inherits(&JSHTMLElement::s_info)) {
            HTMLElement* element = static_cast<JSHTMLElement*>(asObject(thisValue))->impl();
            instance = pluginInstance(element);
        }
        if (!instance)
            return throwError(exec, TypeError);
    }
    ASSERT(instance);

    instance->begin();
    JSValue result = instance->invokeMethod(exec, method, args);
    instance->end();
    return result;
}

CallType RuntimeMethod::getCallData(CallData& callData)
{
    callData.native.function = callRuntimeMethod;
    return CallTypeHost;
}

}
