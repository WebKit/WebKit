/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include <NP_jsobject.h>

#include <JavaScriptCore/npruntime.h>
#include <JavaScriptCore/c_utility.h>
#include <JavaScriptCore/npruntime_impl.h>
#include <JavaScriptCore/npruntime_priv.h>

using namespace KJS;
using namespace KJS::Bindings;


static KJS::List listFromVariantArgs(KJS::ExecState *exec, const NPVariant *args, unsigned argCount)
{
    KJS::List aList; 
    unsigned i;
    const NPVariant *v = args;
    
    for (i = 0; i < argCount; i++) {
        aList.append (convertNPVariantToValue (exec, v));
        v++;
    }
    
    return aList;
}

static NPObject *jsAllocate(NPP npp, NPClass *aClass)
{
    return (NPObject *)malloc(sizeof(JavaScriptObject));
}

static void jsDeallocate (JavaScriptObject *obj)
{
    free (obj);
}

static NPClass _javascriptClass = { 
    1,
    jsAllocate, 
    (NPDeallocateFunctionPtr)jsDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

NPClass *NPScriptObjectClass = &_javascriptClass;

Identifier identiferFromNPIdentifier(const NPUTF8 *name)
{
    NPUTF16 *methodName;
    unsigned int UTF16Length;
    
    convertUTF8ToUTF16 (name, -1, &methodName, &UTF16Length); // requires free() of returned memory.
    Identifier identifier ((const KJS::UChar*)methodName, UTF16Length);
    free ((void *)methodName);
    
    return identifier;
}

static bool _isSafeScript(JavaScriptObject *obj)
{
    if (obj->originExecutionContext) {
	Interpreter *originInterpreter = obj->originExecutionContext->interpreter();
	if (originInterpreter) {
	    return originInterpreter->isSafeScript (obj->executionContext->interpreter());
	}
    }
    return true;
}

NPObject *_NPN_CreateScriptObject (NPP npp, KJS::ObjectImp *imp, const KJS::Bindings::RootObject *originExecutionContext, const KJS::Bindings::RootObject *executionContext)
{
    JavaScriptObject *obj = (JavaScriptObject *)_NPN_CreateObject(npp, NPScriptObjectClass);

    obj->imp = imp;
    obj->originExecutionContext = originExecutionContext;    
    obj->executionContext = executionContext;    

    addNativeReference (executionContext, imp);
    
    return (NPObject *)obj;
}

bool _NPN_InvokeDefault (NPP npp, NPObject *o, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    if (o->_class == NPScriptObjectClass) {
        // No notion of a default function on JS objects.  Just return false, can't handle.
        return false;
    }
    else {
        if (o->_class->invokeDefault) {
            return o->_class->invokeDefault (o, args, argCount, result);
        }
    }
    
    return true;
}

bool _NPN_Invoke (NPP npp, NPObject *o, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 
        
	if (!_isSafeScript(obj))
	    return false;
	    
        PrivateIdentifier *i = (PrivateIdentifier *)methodName;
        if (!i->isString)
            return false;
            
	// Special case the "eval" method.
	if (methodName == _NPN_GetStringIdentifier("eval")) {
	    if (argCount != 1)
		return false;
	    if (args[0].type != NPVariantType_String)
		return false;
	    
	    return _NPN_Evaluate (npp, o, (NPString *)&args[0].value.stringValue, result);
	}
	else {
	    // Lookup the function object.
	    ExecState *exec = obj->executionContext->interpreter()->globalExec();
	    Interpreter::lock();
	    ValueImp *func = obj->imp->get (exec, identiferFromNPIdentifier(i->value.string));
	    Interpreter::unlock();

	    if (func->isNull()) {
		NPN_InitializeVariantAsNull(result);
		return false;
	    }
	    else if (func->isUndefined()) {
		NPN_InitializeVariantAsUndefined(result);
		return false;
	    }
	    else {
		// Call the function object.
		ObjectImp *funcImp = static_cast<ObjectImp*>(func);
		ObjectImp *thisObj = const_cast<ObjectImp*>(obj->imp);
		List argList = listFromVariantArgs(exec, args, argCount);
		Interpreter::lock();
		ValueImp *resultV = funcImp->call (exec, thisObj, argList);
		Interpreter::unlock();

		// Convert and return the result of the function call.
		convertValueToNPVariant(exec, resultV, result);
		return true;
	    }
	}
    }
    else {
        if (o->_class->invoke) {
            return o->_class->invoke (o, methodName, args, argCount, result);
        }
    }
    
    return true;
}

bool _NPN_Evaluate (NPP npp, NPObject *o, NPString *s, NPVariant *variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 

	if (!_isSafeScript(obj))
	    return false;

        ExecState *exec = obj->executionContext->interpreter()->globalExec();
        ValueImp *result;
        
        Interpreter::lock();
        NPUTF16 *scriptString;
        unsigned int UTF16Length;
        convertNPStringToUTF16 (s, &scriptString, &UTF16Length);    // requires free() of returned memory.
        Completion completion = obj->executionContext->interpreter()->evaluate(UString(), 0, UString((const UChar *)scriptString,UTF16Length));
        ComplType type = completion.complType();
        
        if (type == Normal) {
            result = completion.value();
            if (!result) {
                result = Undefined();
            }
        }
        else
            result = Undefined();
            
        Interpreter::unlock();
        
        free ((void *)scriptString);
        
        convertValueToNPVariant(exec, result, variant);
    
        return true;
    }
    return false;
}

bool _NPN_GetProperty (NPP npp, NPObject *o, NPIdentifier propertyName, NPVariant *variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 

	if (!_isSafeScript(obj))
	    return false;

        ExecState *exec = obj->executionContext->interpreter()->globalExec();

        PrivateIdentifier *i = (PrivateIdentifier *)propertyName;
        
        Interpreter::lock();
        ValueImp *result;
        if (i->isString) {
            result = obj->imp->get (exec, identiferFromNPIdentifier(i->value.string));
        }
        else {
            result = obj->imp->get (exec, i->value.number);
        }
        Interpreter::unlock();

        if (result->isNull()) {
            NPN_InitializeVariantAsNull(variant);
            return false;
        }
        else if (result->isUndefined()) {
            NPN_InitializeVariantAsUndefined(variant);
            return false;
        }
        else {
            convertValueToNPVariant(exec, result, variant);
        }

        return true;
    }
    else if (o->_class->hasProperty && o->_class->getProperty) {
        if (o->_class->hasProperty (o, propertyName)) {
            return o->_class->getProperty (o, propertyName, variant);
        }
        else {
            return false;
        }
    }
    return false;
}

bool _NPN_SetProperty (NPP npp, NPObject *o, NPIdentifier propertyName, const NPVariant *variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 

	if (!_isSafeScript(obj))
	    return false;

        ExecState *exec = obj->executionContext->interpreter()->globalExec();
        Interpreter::lock();
        PrivateIdentifier *i = (PrivateIdentifier *)propertyName;
        if (i->isString) {
            obj->imp->put (exec, identiferFromNPIdentifier(i->value.string), convertNPVariantToValue(exec, variant));
        }
        else {
            obj->imp->put (exec, i->value.number, convertNPVariantToValue(exec, variant));
        }
        Interpreter::unlock();
        
        return true;
    }
    else if (o->_class->setProperty) {
        return o->_class->setProperty (o, propertyName, variant);
    }
    return false;
}

bool _NPN_RemoveProperty (NPP npp, NPObject *o, NPIdentifier propertyName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 

	if (!_isSafeScript(obj))
	    return false;

        ExecState *exec = obj->executionContext->interpreter()->globalExec();

        PrivateIdentifier *i = (PrivateIdentifier *)propertyName;
        if (i->isString) {
            if (!obj->imp->hasProperty (exec, identiferFromNPIdentifier(i->value.string))) {
                return false;
            }
        }
        else {
            if (!obj->imp->hasProperty (exec, i->value.number)) {
                return false;
            }
        }

        Interpreter::lock();
        if (i->isString) {
            obj->imp->deleteProperty (exec, identiferFromNPIdentifier(i->value.string));
        }
        else {
            obj->imp->deleteProperty (exec, i->value.number);
        }
        Interpreter::unlock();
        
        return true;
    }
    return false;
}

bool _NPN_HasProperty(NPP npp, NPObject *o, NPIdentifier propertyName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 

	if (!_isSafeScript(obj))
	    return false;

        ExecState *exec = obj->executionContext->interpreter()->globalExec();

        PrivateIdentifier *i = (PrivateIdentifier *)propertyName;
        // String identifier?
        if (i->isString) {
            ExecState *exec = obj->executionContext->interpreter()->globalExec();
            Interpreter::lock();
            bool result = obj->imp->hasProperty (exec, identiferFromNPIdentifier(i->value.string));
            Interpreter::unlock();
            return result;
        }
        
        // Numeric identifer
        Interpreter::lock();
        bool result = obj->imp->hasProperty (exec, i->value.number);
        Interpreter::unlock();
        return result;
    }
    else if (o->_class->hasProperty) {
        return o->_class->hasProperty (o, propertyName);
    }
    
    return false;
}

bool _NPN_HasMethod(NPP npp, NPObject *o, NPIdentifier methodName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 
        
	if (!_isSafeScript(obj))
	    return false;

        PrivateIdentifier *i = (PrivateIdentifier *)methodName;
        if (!i->isString)
            return false;
            
        // Lookup the function object.
        ExecState *exec = obj->executionContext->interpreter()->globalExec();
        Interpreter::lock();
        ValueImp *func = obj->imp->get (exec, identiferFromNPIdentifier(i->value.string));
        Interpreter::unlock();

        if (func->isUndefined()) {
            return false;
        }
        
        return true;
    }
    
    else if (o->_class->hasMethod) {
        return o->_class->hasMethod (o, methodName);
    }
    
    return false;
}

void _NPN_SetException (NPObject *o, const NPUTF8 *message)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 
        ExecState *exec = obj->executionContext->interpreter()->globalExec();
        Interpreter::lock();
        ObjectImp *err = Error::create(exec, GeneralError, message);
        exec->setException (err);
        Interpreter::unlock();
    }
}
