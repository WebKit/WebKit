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

using namespace KJS;
using namespace KJS::Bindings;


static KJS::List listFromNPArray(KJS::ExecState *exec, NPObject **args, unsigned argCount)
{
    KJS::List aList;    
    return aList;
}

static NPObject *jsAllocate()
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
};

static NPClass *javascriptClass = &_javascriptClass;
NPClass *NPScriptObjectClass = javascriptClass;

Identifier identiferFromNPIdentifier(NPIdentifier ident)
{
    const NPUTF8 *name = NPN_UTF8FromIdentifier (ident);
    NPString *string = NPN_CreateStringWithUTF8 (name, strlen(name));
    NPUTF16 *methodName = NPN_UTF16FromString (string);
    int32_t length = NPN_StringLength (string);
    NPN_ReleaseObject (string);
    Identifier identifier ((const KJS::UChar*)methodName, length);
    free ((void *)methodName);
    return identifier;
}

void NPN_Call (NPScriptObject *o, NPIdentifier ident, NPObject **args, unsigned argCount, NPScriptResultFunctionPtr resultCallback, void *resultContext)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    // Lookup the function object.
    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    
    Value func = obj->imp->get (exec, identiferFromNPIdentifier(ident));
    Interpreter::unlock();
    if (func.isNull()) {
        resultCallback (NPN_GetNull(), resultContext);
        return;
    }
    else if ( func.type() == UndefinedType) {
        resultCallback (NPN_GetUndefined(), resultContext);
        return;
    }

    // Call the function object.
    ObjectImp *funcImp = static_cast<ObjectImp*>(func.imp());
    Object thisObj = Object(const_cast<ObjectImp*>(obj->imp));
    List argList = listFromNPArray(exec, args, argCount);
    Interpreter::lock();
    Value result = funcImp->call (exec, thisObj, argList);
    Interpreter::unlock();

    // Convert and return the result of the function call.
    NPObject *npresult = convertValueToNPValueType(exec, result);

    resultCallback (npresult, resultContext);
}

void NPN_Evaluate (NPScriptObject *o, NPString *s, NPScriptResultFunctionPtr resultCallback, void *resultContext)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Object thisObj = Object(const_cast<ObjectImp*>(obj->imp));
    Interpreter::lock();
    NPUTF16 *script = NPN_UTF16FromString (s);
    int32_t length = NPN_StringLength (s);
    KJS::Value result = obj->root->interpreter()->evaluate(UString((const UChar *)script,length)).value();
    Interpreter::unlock();
    free ((void *)script);
    
    NPObject *npresult = convertValueToNPValueType(exec, result);

    resultCallback (npresult, resultContext);
}

void NPN_GetProperty (NPScriptObject *o, NPIdentifier propertyName, NPScriptResultFunctionPtr resultCallback, void *resultContext)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = obj->imp->get (exec, identiferFromNPIdentifier(propertyName));
    Interpreter::unlock();
    
    NPObject *npresult = convertValueToNPValueType(exec, result);
    
    resultCallback (npresult, resultContext);
}

void NPN_SetProperty (NPScriptObject *o, NPIdentifier propertyName, NPObject *value)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    obj->imp->put (exec, identiferFromNPIdentifier(propertyName), convertNPValueTypeToValue(exec, value));
    Interpreter::unlock();
}

void NPN_RemoveProperty (NPScriptObject *o, NPIdentifier propertyName)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    obj->imp->deleteProperty (exec, identiferFromNPIdentifier(propertyName));
    Interpreter::unlock();
}

void NPN_ToString (NPScriptObject *o, NPScriptResultFunctionPtr resultCallback, void *resultContext)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 
    
    Interpreter::lock();
    Object thisObj = Object(const_cast<ObjectImp*>(obj->imp));
    ExecState *exec = obj->root->interpreter()->globalExec();
    
    NPString *value = (NPString *)coerceValueToNPString(exec, thisObj);

    Interpreter::unlock();
    
    resultCallback (value, resultContext);
}

void NPN_GetPropertyAtIndex (NPScriptObject *o, int32_t index, NPScriptResultFunctionPtr resultCallback, void *resultContext)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = obj->imp->get (exec, (unsigned)index);
    Interpreter::unlock();

    NPObject *npresult = convertValueToNPValueType(exec, result);
    
    resultCallback (npresult, resultContext);
}

void NPN_SetPropertyAtIndex (NPScriptObject *o, unsigned index, NPObject value)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    obj->imp->put (exec, (unsigned)index, convertNPValueTypeToValue(exec, &value));
    Interpreter::unlock();
}

