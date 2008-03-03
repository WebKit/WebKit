/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "ExecState.h"
#include "JSGlobalObject.h"
#include "runtime_object.h"
#include "function_object.h"

using namespace KJS::Bindings;
using namespace KJS;

RuntimeMethod::RuntimeMethod(ExecState *exec, const Identifier &ident, Bindings::MethodList &m) 
    : InternalFunctionImp(exec->lexicalGlobalObject()->functionPrototype(), ident)
    , _methodList(new MethodList(m))
{
}

JSValue *RuntimeMethod::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    RuntimeMethod *thisObj = static_cast<RuntimeMethod *>(slot.slotBase());

    // Ick!  There may be more than one method with this name.  Arbitrarily
    // just pick the first method.  The fundamental problem here is that 
    // JavaScript doesn't have the notion of method overloading and
    // Java does.
    // FIXME: a better solution might be to give the maximum number of parameters
    // of any method
    return jsNumber(thisObj->_methodList->at(0)->numParameters());
}

bool RuntimeMethod::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot &slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }
    
    return InternalFunctionImp::getOwnPropertySlot(exec, propertyName, slot);
}

JSValue *RuntimeMethod::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    if (_methodList->isEmpty())
        return jsUndefined();
    
    RuntimeObjectImp *imp = 0;

    if (thisObj->classInfo() == &KJS::RuntimeObjectImp::info) {
        imp = static_cast<RuntimeObjectImp*>(thisObj);
    } else {
        // If thisObj is the DOM object for a plugin, get the corresponding
        // runtime object from the DOM object.
        JSValue* value = thisObj->get(exec, "__apple_runtime_object");
        if (value->isObject(&KJS::RuntimeObjectImp::info))    
            imp = static_cast<RuntimeObjectImp*>(value);
    }

    if (!imp)
        return throwError(exec, TypeError);

    RefPtr<Instance> instance = imp->getInternalInstance();
    if (!instance) 
        return RuntimeObjectImp::throwInvalidAccessError(exec);
        
    instance->begin();
    JSValue *aValue = instance->invokeMethod(exec, *_methodList, args);
    instance->end();
    return aValue;
}
