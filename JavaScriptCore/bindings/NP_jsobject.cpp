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
#include <NP_runtime.h>

#include <c_utility.h>

#include <runtime.h>
#include <runtime_object.h>
#include <runtime_root.h>

using namespace KJS;
using namespace KJS::Bindings;

static KJS::List listFromNPArray(KJS::ExecState *exec, NP_Object **args, unsigned argCount)
{
    KJS::List aList;    
    return aList;
}

typedef struct
{
    NP_Object object;
    KJS::ObjectImp *imp;
    KJS::Bindings::RootObject *root;
} JavaScriptObject;

static NP_Object *jsAllocate()
{
    return (NP_Object *)malloc(sizeof(JavaScriptObject));
}

static void jsDeallocate (JavaScriptObject *obj)
{
    free (obj);
}

static NP_Class _javascriptClass = { 
    1,
    jsAllocate, 
    (NP_DeallocateInterface)jsDeallocate, 
    0,
    0,
    0,
    0,
    0,
    0,
};

static NP_Class *javascriptClass = &_javascriptClass;
NP_Class *NP_JavaScriptObjectClass = javascriptClass;

Identifier identiferFromNPIdentifier(NP_Identifier ident)
{
    NP_String *string = NP_CreateStringWithUTF8 (NP_UTF8FromIdentifier (ident));
    NP_UTF16 *methodName = NP_UTF16FromString (string);
    int32_t length = NP_StringLength (string);
    NP_ReleaseObject (string);
    Identifier identifier ((const KJS::UChar*)methodName, length);
    free ((void *)methodName);
    return identifier;
}

void NP_Call (NP_JavaScriptObject *o, NP_Identifier ident, NP_Object **args, unsigned argCount, NP_JavaScriptResultInterface resultCallback)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    // Lookup the function object.
    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    
    Value func = obj->imp->get (exec, identiferFromNPIdentifier(ident));
    Interpreter::unlock();
    if (func.isNull()) {
        resultCallback (NP_GetNull());
        return;
    }
    else if ( func.type() == UndefinedType) {
        resultCallback (NP_GetUndefined());
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
    NP_Object *npresult = convertValueToNPValueType(exec, result);

    resultCallback (npresult);
}

void NP_Evaluate (NP_JavaScriptObject *o, NP_String *s, NP_JavaScriptResultInterface resultCallback)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Object thisObj = Object(const_cast<ObjectImp*>(obj->imp));
    Interpreter::lock();
    NP_UTF16 *script = NP_UTF16FromString (s);
    int32_t length = NP_StringLength (s);
    KJS::Value result = obj->root->interpreter()->evaluate(UString((const UChar *)script,length)).value();
    Interpreter::unlock();
    free ((void *)script);
    
    NP_Object *npresult = convertValueToNPValueType(exec, result);

    resultCallback (npresult);
}

void NP_GetProperty (NP_JavaScriptObject *o, NP_Identifier propertyName, NP_JavaScriptResultInterface resultCallback)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = obj->imp->get (exec, identiferFromNPIdentifier(propertyName));
    Interpreter::unlock();
    
    NP_Object *npresult = convertValueToNPValueType(exec, result);
    
    resultCallback (npresult);
}

void NP_SetProperty (NP_JavaScriptObject *o, NP_Identifier propertyName, NP_Object *value)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    obj->imp->put (exec, identiferFromNPIdentifier(propertyName), convertNPValueTypeToValue(exec, value));
    Interpreter::unlock();
}

void NP_RemoveProperty (NP_JavaScriptObject *o, NP_Identifier propertyName)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    obj->imp->deleteProperty (exec, identiferFromNPIdentifier(propertyName));
    Interpreter::unlock();
}

void NP_ToString (NP_JavaScriptObject *o, NP_JavaScriptResultInterface resultCallback)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 
    
    Interpreter::lock();
    Object thisObj = Object(const_cast<ObjectImp*>(obj->imp));
    ExecState *exec = obj->root->interpreter()->globalExec();
    
    NP_String *value = (NP_String *)coerceValueToNPValueType(exec, thisObj, NP_StringValueType);

    Interpreter::unlock();
    
    resultCallback (value);
}

void NP_GetPropertyAtIndex (NP_JavaScriptObject *o, int32_t index, NP_JavaScriptResultInterface resultCallback)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    Value result = obj->imp->get (exec, (unsigned)index);
    Interpreter::unlock();

    NP_Object *npresult = convertValueToNPValueType(exec, result);
    
    resultCallback (npresult);
}

void NP_SetPropertyAtIndex (NP_JavaScriptObject *o, unsigned index, NP_Object value)
{
    JavaScriptObject *obj = (JavaScriptObject *)o; 

    ExecState *exec = obj->root->interpreter()->globalExec();
    Interpreter::lock();
    obj->imp->put (exec, (unsigned)index, convertNPValueTypeToValue(exec, &value));
    Interpreter::unlock();
}

