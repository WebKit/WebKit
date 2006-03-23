/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "NP_jsobject.h"

#include "c_utility.h"
#include "identifier.h"
#include "interpreter.h"
#include "list.h"
#include "npruntime.h"
#include "npruntime_impl.h"
#include "npruntime_priv.h"
#include "runtime_root.h"

using namespace KJS;
using namespace KJS::Bindings;

static List listFromVariantArgs(ExecState* exec, const NPVariant* args, unsigned argCount)
{
    List aList; 
    for (unsigned i = 0; i < argCount; i++)
        aList.append(convertNPVariantToValue(exec, &args[i]));    
    return aList;
}

static NPObject* jsAllocate(NPP, NPClass*)
{
    return (NPObject*)malloc(sizeof(JavaScriptObject));
}

static void jsDeallocate(NPObject* obj)
{
    free(obj);
}

static NPClass javascriptClass = { 1, jsAllocate, jsDeallocate, 0, 0, 0, 0, 0, 0, 0, 0 };
static NPClass noScriptClass = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

NPClass* NPScriptObjectClass = &javascriptClass;
static NPClass* NPNoScriptObjectClass = &noScriptClass;

static Identifier identifierFromNPIdentifier(const NPUTF8* name)
{
    NPUTF16 *methodName;
    unsigned UTF16Length;
    convertUTF8ToUTF16(name, -1, &methodName, &UTF16Length); // requires free() of returned memory.
    Identifier identifier((const KJS::UChar*)methodName, UTF16Length);
    free(methodName);
    return identifier;
}

static bool _isSafeScript(JavaScriptObject* obj)
{
    if (obj->originExecutionContext) {
	Interpreter* originInterpreter = obj->originExecutionContext->interpreter();
	if (originInterpreter)
	    return originInterpreter->isSafeScript(obj->executionContext->interpreter());
    }
    return true;
}

NPObject *_NPN_CreateScriptObject (NPP npp, JSObject *imp, const RootObject *originExecutionContext, const RootObject *executionContext)
{
    JavaScriptObject* obj = (JavaScriptObject*)_NPN_CreateObject(npp, NPScriptObjectClass);

    obj->imp = imp;
    obj->originExecutionContext = originExecutionContext;    
    obj->executionContext = executionContext;    

    addNativeReference(executionContext, imp);

    return (NPObject *)obj;
}

NPObject *_NPN_CreateNoScriptObject(void)
{
    return _NPN_CreateObject(0, NPNoScriptObjectClass);
}

bool _NPN_InvokeDefault(NPP, NPObject* o, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (o->_class == NPScriptObjectClass)
        // No notion of a default function on JS objects. Just return false, can't handle.
        return false;
    if (o->_class->invokeDefault)
        return o->_class->invokeDefault(o, args, argCount, result);    
    VOID_TO_NPVARIANT(*result);
    return true;
}

bool _NPN_Invoke(NPP npp, NPObject* o, NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 
	if (!_isSafeScript(obj))
	    return false;

        PrivateIdentifier* i = (PrivateIdentifier*)methodName;
        if (!i->isString)
            return false;

	// Special case the "eval" method.
	if (methodName == _NPN_GetStringIdentifier("eval")) {
	    if (argCount != 1)
		return false;
	    if (args[0].type != NPVariantType_String)
		return false;
	    return _NPN_Evaluate(npp, o, (NPString *)&args[0].value.stringValue, result);
	}

        // Lookup the function object.
        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        JSLock lock;
        JSValue* func = obj->imp->get(exec, identifierFromNPIdentifier(i->value.string));
        if (func->isNull()) {
            NULL_TO_NPVARIANT(*result);
            return false;
        } 
        if (func->isUndefined()) {
            VOID_TO_NPVARIANT(*result);
            return false;
        }
        // Call the function object.
        JSObject *funcImp = static_cast<JSObject*>(func);
        JSObject *thisObj = const_cast<JSObject*>(obj->imp);
        List argList = listFromVariantArgs(exec, args, argCount);
        JSValue *resultV = funcImp->call (exec, thisObj, argList);

        // Convert and return the result of the function call.
        convertValueToNPVariant(exec, resultV, result);
        return true;
    }

    if (o->_class->invoke)
        return o->_class->invoke(o, methodName, args, argCount, result);
    
    VOID_TO_NPVARIANT(*result);
    return true;
}

bool _NPN_Evaluate(NPP, NPObject* o, NPString* s, NPVariant* variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 

	if (!_isSafeScript(obj))
	    return false;

        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        
        JSLock lock;
        NPUTF16* scriptString;
        unsigned int UTF16Length;
        convertNPStringToUTF16(s, &scriptString, &UTF16Length); // requires free() of returned memory
        Completion completion = obj->executionContext->interpreter()->evaluate(UString(), 0, UString((const UChar*)scriptString,UTF16Length));
        ComplType type = completion.complType();
        
        JSValue* result;
        if (type == Normal) {
            result = completion.value();
            if (!result)
                result = jsUndefined();
        } else
            result = jsUndefined();

        free(scriptString);

        convertValueToNPVariant(exec, result, variant);
    
        return true;
    }

    VOID_TO_NPVARIANT(*variant);
    return false;
}

bool _NPN_GetProperty(NPP, NPObject* o, NPIdentifier propertyName, NPVariant* variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 
	if (!_isSafeScript(obj))
	    return false;

        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        PrivateIdentifier* i = (PrivateIdentifier*)propertyName;
        
        JSLock lock;
        JSValue *result;
        if (i->isString)
            result = obj->imp->get(exec, identifierFromNPIdentifier(i->value.string));
        else
            result = obj->imp->get(exec, i->value.number);

        if (result->isNull()) {
            NULL_TO_NPVARIANT(*variant);
            return false;
        }
        if (result->isUndefined()) {
            VOID_TO_NPVARIANT(*variant);
            return false;
        }
        convertValueToNPVariant(exec, result, variant);
        return true;
    }

    if (o->_class->hasProperty && o->_class->getProperty) {
        if (o->_class->hasProperty(o, propertyName))
            return o->_class->getProperty(o, propertyName, variant);
        return false;
    }

    VOID_TO_NPVARIANT(*variant);
    return false;
}

bool _NPN_SetProperty(NPP, NPObject* o, NPIdentifier propertyName, const NPVariant* variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 
	if (!_isSafeScript(obj))
	    return false;

        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        JSLock lock;
        PrivateIdentifier* i = (PrivateIdentifier*)propertyName;
        if (i->isString)
            obj->imp->put(exec, identifierFromNPIdentifier(i->value.string), convertNPVariantToValue(exec, variant));
        else
            obj->imp->put(exec, i->value.number, convertNPVariantToValue(exec, variant));
        return true;
    }

    if (o->_class->setProperty)
        return o->_class->setProperty(o, propertyName, variant);

    return false;
}

bool _NPN_RemoveProperty(NPP, NPObject* o, NPIdentifier propertyName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 
	if (!_isSafeScript(obj))
	    return false;

        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        PrivateIdentifier* i = (PrivateIdentifier*)propertyName;
        if (i->isString) {
            if (!obj->imp->hasProperty(exec, identifierFromNPIdentifier(i->value.string)))
                return false;
        } else {
            if (!obj->imp->hasProperty(exec, i->value.number))
                return false;
        }

        JSLock lock;
        if (i->isString)
            obj->imp->deleteProperty(exec, identifierFromNPIdentifier(i->value.string));
        else
            obj->imp->deleteProperty(exec, i->value.number);
        
        return true;
    }
    return false;
}

bool _NPN_HasProperty(NPP, NPObject* o, NPIdentifier propertyName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 
	if (!_isSafeScript(obj))
	    return false;

        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        PrivateIdentifier* i = (PrivateIdentifier*)propertyName;
        JSLock lock;
        if (i->isString)
            return obj->imp->hasProperty(exec, identifierFromNPIdentifier(i->value.string));
        return obj->imp->hasProperty(exec, i->value.number);
    }

    if (o->_class->hasProperty)
        return o->_class->hasProperty(o, propertyName);

    return false;
}

bool _NPN_HasMethod(NPP, NPObject* o, NPIdentifier methodName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 
	if (!_isSafeScript(obj))
	    return false;

        PrivateIdentifier* i = (PrivateIdentifier*)methodName;
        if (!i->isString)
            return false;

        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        JSLock lock;
        JSValue* func = obj->imp->get(exec, identifierFromNPIdentifier(i->value.string));
        return !func->isUndefined();
    }
    
    if (o->_class->hasMethod)
        return o->_class->hasMethod(o, methodName);
    
    return false;
}

void _NPN_SetException(NPObject* o, const NPUTF8* message)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject* obj = (JavaScriptObject*)o; 
        ExecState* exec = obj->executionContext->interpreter()->globalExec();
        JSLock lock;
        throwError(exec, GeneralError, message);
    }
}
