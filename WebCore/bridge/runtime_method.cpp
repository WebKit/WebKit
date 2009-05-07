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
#include "runtime_object.h"
#include <runtime/Error.h>
#include <runtime/FunctionPrototype.h>

using namespace WebCore;

namespace JSC {

using namespace Bindings;

ASSERT_CLASS_FITS_IN_CELL(RuntimeMethod);

const ClassInfo RuntimeMethod::s_info = { "RuntimeMethod", 0, 0, 0 };

RuntimeMethod::RuntimeMethod(ExecState* exec, const Identifier& ident, Bindings::MethodList& m) 
    : InternalFunction(&exec->globalData(), getDOMStructure<RuntimeMethod>(exec), ident)
    , _methodList(new MethodList(m))
{
}

JSValue RuntimeMethod::lengthGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    RuntimeMethod* thisObj = static_cast<RuntimeMethod*>(asObject(slot.slotBase()));

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
        slot.setCustom(this, lengthGetter);
        return true;
    }
    
    return InternalFunction::getOwnPropertySlot(exec, propertyName, slot);
}

static JSValue JSC_HOST_CALL callRuntimeMethod(ExecState* exec, JSObject* function, JSValue thisValue, const ArgList& args)
{
    RuntimeMethod* method = static_cast<RuntimeMethod*>(function);

    if (method->methods()->isEmpty())
        return jsUndefined();
    
    RuntimeObjectImp* imp;

    if (thisValue.isObject(&RuntimeObjectImp::s_info)) {
        imp = static_cast<RuntimeObjectImp*>(asObject(thisValue));
    } else {
        // If thisObj is the DOM object for a plugin, get the corresponding
        // runtime object from the DOM object.
        JSValue value = thisValue.get(exec, Identifier(exec, "__apple_runtime_object"));
        if (value.isObject(&RuntimeObjectImp::s_info))    
            imp = static_cast<RuntimeObjectImp*>(asObject(value));
        else
            return throwError(exec, TypeError);
    }

    RefPtr<Instance> instance = imp->getInternalInstance();
    if (!instance) 
        return RuntimeObjectImp::throwInvalidAccessError(exec);
        
    instance->begin();
    JSValue result = instance->invokeMethod(exec, *method->methods(), args);
    instance->end();
    return result;
}

CallType RuntimeMethod::getCallData(CallData& callData)
{
    callData.native.function = callRuntimeMethod;
    return CallTypeHost;
}

}
