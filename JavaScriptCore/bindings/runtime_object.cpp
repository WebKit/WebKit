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

#include "error_object.h"
#include "function.h"
#include "interpreter.h"
#include "object.h"
#include "operations.h"
#include "runtime_method.h"
#include "runtime_object.h"
#include "types.h"
#include "value.h"


#include <assert.h>

using namespace KJS;
using namespace Bindings;

const ClassInfo RuntimeObjectImp::info = {"RuntimeObject", 0, 0, 0};

RuntimeObjectImp::RuntimeObjectImp(ObjectImp *proto)
  : ObjectImp(proto)
{
    instance = 0;
}

RuntimeObjectImp::~RuntimeObjectImp()
{
    if (ownsInstance) {
        delete instance;
    }
}

RuntimeObjectImp::RuntimeObjectImp(Bindings::Instance *i, bool oi) : ObjectImp ((ObjectImp *)0)
{
    ownsInstance = oi;
    instance = i;
}

RuntimeObjectImp::RuntimeObjectImp(Bindings::Instance *i, const Value &fb, bool oi) : ObjectImp ((ObjectImp *)0)
{
    ownsInstance = oi;
    instance = i;
    fallback = fb;
}

Value RuntimeObjectImp::get(ExecState *exec, const Identifier &propertyName) const
{
    Value result = Undefined();

    instance->begin();
    
    Class *aClass = instance->getClass();
    
    if (aClass) {
        
        // See if the instance have a field with the specified name.
        Field *aField = aClass->fieldNamed(propertyName.ascii(), instance);
        if (aField) {
            result = instance->getValueOfField (exec, aField); 
        }
        else {
            // Now check if a method with specified name exists, if so return a function object for
            // that method.
            MethodList methodList = aClass->methodsNamed(propertyName.ascii(), instance);
            if (methodList.length() > 0) {
                result = Object (new RuntimeMethodImp(exec, propertyName, methodList));
            }
	    else if (!fallback.isNull() && fallback.type() == ObjectType){
		ObjectImp *imp = static_cast<ObjectImp*>(fallback.imp());
		imp->setForwardingScriptMessage(true);
		result = imp->get (exec, propertyName);
		imp->setForwardingScriptMessage(false);
	    }
        }
	
        if (result.type() == UndefinedType) {
            // Try a fallback object.
            result = aClass->fallbackObject (exec, instance, propertyName);
        }
    }
        
    instance->end();

    
    return result;
}

void RuntimeObjectImp::put(ExecState *exec, const Identifier &propertyName,
                    const Value &value, int attr)
{
    instance->begin();

    // Set the value of the property.
    Field *aField = instance->getClass()->fieldNamed(propertyName.ascii(), instance);
    if (aField) {
        getInternalInstance()->setValueOfField(exec, aField, value);
    }
    else {
	bool domHasProperty = false;
	if (!fallback.isNull() && fallback.type() == ObjectType){
	    ObjectImp *imp = static_cast<ObjectImp*>(fallback.imp());
	    imp->setForwardingScriptMessage(true);
	    domHasProperty = imp->hasProperty(exec, propertyName);
	    imp->setForwardingScriptMessage(false);
	}
	
	// If the DOM has the property, give it a crack first (even if it read-only).
	if (domHasProperty || !getInternalInstance()->supportsSetValueOfUndefinedField()) {
	    ObjectImp *imp = static_cast<ObjectImp*>(fallback.imp());
	    imp->setForwardingScriptMessage(true);
	    imp->put(exec, propertyName, value, attr);
	    imp->setForwardingScriptMessage(false);
	}
	// Now let the runtime object attempt to handle the undefined field.
	else if (getInternalInstance()->supportsSetValueOfUndefinedField()){
	    getInternalInstance()->setValueOfUndefinedField(exec, propertyName, value);
	}
    }

    instance->end();
}

bool RuntimeObjectImp::canPut(ExecState *exec, const Identifier &propertyName) const
{
    bool result = false;

    instance->begin();

    Field *aField = instance->getClass()->fieldNamed(propertyName.ascii(), instance);

    instance->end();

    if (aField)
	return true;
    
    if (!fallback.isNull() && fallback.type() == ObjectType) {
	ObjectImp *imp = static_cast<ObjectImp*>(fallback.imp());
	imp->setForwardingScriptMessage(true);
	result = imp->canPut (exec, propertyName);
	imp->setForwardingScriptMessage(false);
    }
	
    return result;
}

bool RuntimeObjectImp::hasProperty(ExecState *exec,
                            const Identifier &propertyName) const
{
    bool result = false;
    
    instance->begin();

    Field *aField = instance->getClass()->fieldNamed(propertyName.ascii(), instance);
    if (aField) {
        instance->end();
        return true;
    }
        
    MethodList methodList = instance->getClass()->methodsNamed(propertyName.ascii(), instance);

    instance->end();

    if (methodList.length() > 0)
        return true;

    if (!fallback.isNull() && fallback.type() == ObjectType) {
	ObjectImp *imp = static_cast<ObjectImp*>(fallback.imp());
	imp->setForwardingScriptMessage(true);
	result = imp->hasProperty (exec, propertyName);
	imp->setForwardingScriptMessage(false);
    }
        
    return result;
}

bool RuntimeObjectImp::deleteProperty(ExecState *exec,
                            const Identifier &propertyName)
{
    // Can never remove a property of a RuntimeObject.
    return false;
}

Value RuntimeObjectImp::defaultValue(ExecState *exec, Type hint) const
{
    Value result;
    
    instance->begin();

    if (!fallback.isNull() && fallback.type() == ObjectType) {
	ObjectImp *imp = static_cast<ObjectImp*>(fallback.imp());
	imp->setForwardingScriptMessage(true);
	result = imp->defaultValue (exec, hint);
	imp->setForwardingScriptMessage(false);
    }
    else {
	result = getInternalInstance()->defaultValue(hint);
    }
    
    instance->end();
    
    return result;
}
    
bool RuntimeObjectImp::implementsCall() const
{
    // Only true for default functions.
    return true;
}

Value RuntimeObjectImp::call(ExecState *exec, Object &thisObj, const List &args)
{
    instance->begin();

    Value aValue = getInternalInstance()->invokeDefaultMethod(exec, args);
    
    instance->end();
    
    return aValue;
}

