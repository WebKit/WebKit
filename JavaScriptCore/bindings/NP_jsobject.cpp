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

static NPObject *jsAllocate(NPP npp)
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
    0
};

static NPClass *javascriptClass = &_javascriptClass;
NPClass *NPScriptObjectClass = javascriptClass;

Identifier identiferFromNPIdentifier(const NPUTF8 *name)
{
    NPUTF16 *methodName;
    unsigned int UTF16Length;
    
    convertUTF8ToUTF16 (name, -1, &methodName, &UTF16Length); // requires free() of returned memory.
    Identifier identifier ((const KJS::UChar*)methodName, UTF16Length);
    free ((void *)methodName);
    
    return identifier;
}

NPObject *_NPN_CreateScriptObject (NPP npp, KJS::ObjectImp *imp, KJS::Bindings::RootObject *root)
{
    JavaScriptObject *obj = (JavaScriptObject *)NPN_CreateObject(npp, NPScriptObjectClass);

    obj->imp = imp;
    obj->root = root;    

    addNativeReference (root, imp);
    
    return (NPObject *)obj;
}

bool NPN_Call (NPP npp, NPObject *o, NPIdentifier methodName, const NPVariant *args, unsigned argCount, NPVariant *result)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 
        
        PrivateIdentifier *i = (PrivateIdentifier *)methodName;
        if (!i->isString)
            return false;
            
        // Lookup the function object.
        ExecState *exec = obj->root->interpreter()->globalExec();
        Interpreter::lock();
        Value func = obj->imp->get (exec, identiferFromNPIdentifier(i->value.string));
        Interpreter::unlock();

        if (func.isNull()) {
            NPN_InitializeVariantAsNull(result);
            return false;
        }
        else if ( func.type() == UndefinedType) {
            NPN_InitializeVariantAsUndefined(result);
            return false;
        }
        else {
            // Call the function object.
            ObjectImp *funcImp = static_cast<ObjectImp*>(func.imp());
            Object thisObj = Object(const_cast<ObjectImp*>(obj->imp));
            List argList = listFromVariantArgs(exec, args, argCount);
            Interpreter::lock();
            Value resultV = funcImp->call (exec, thisObj, argList);
            Interpreter::unlock();

            // Convert and return the result of the function call.
            convertValueToNPVariant(exec, resultV, result);
            return true;
        }
    }
    else {
        if (o->_class->invoke) {
            return o->_class->invoke (o, methodName, args, argCount, result);
        }
    }
    
    return true;
}

bool NPN_Evaluate (NPP npp, NPObject *o, NPString *s, NPVariant *variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 

        ExecState *exec = obj->root->interpreter()->globalExec();
        Object thisObj = Object(const_cast<ObjectImp*>(obj->imp));
        
        Interpreter::lock();
        NPUTF16 *scriptString;
        unsigned int UTF16Length;
        convertNPStringToUTF16 (s, &scriptString, &UTF16Length);    // requires free() of returned memory.
        KJS::Value result = obj->root->interpreter()->evaluate(UString(), 0, UString((const UChar *)scriptString,UTF16Length)).value();
        Interpreter::unlock();
        
        free ((void *)scriptString);
        
        convertValueToNPVariant(exec, result, variant);
    
        return true;
    }
    return false;
}

bool NPN_GetProperty (NPP npp, NPObject *o, NPIdentifier propertyName, NPVariant *variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 
        ExecState *exec = obj->root->interpreter()->globalExec();

        PrivateIdentifier *i = (PrivateIdentifier *)propertyName;
        if (i->isString) {
            if (!obj->imp->hasProperty (exec, identiferFromNPIdentifier(i->value.string))) {
                NPN_InitializeVariantAsNull(variant);
                return false;
            }
        }
        else {
            if (!obj->imp->hasProperty (exec, i->value.number)) {
                NPN_InitializeVariantAsNull(variant);
                return false;
            }
        }
        
        Interpreter::lock();
        Value result;
        if (i->isString) {
            result = obj->imp->get (exec, identiferFromNPIdentifier(i->value.string));
        }
        else {
            result = obj->imp->get (exec, i->value.number);
        }
        Interpreter::unlock();

        if (result.isNull()) {
            NPN_InitializeVariantAsNull(variant);
            return false;
        }
        else if (result.type() == UndefinedType) {
            NPN_InitializeVariantAsUndefined(variant);
            return false;
        }
        else {
            convertValueToNPVariant(exec, result, variant);
        }

        return true;
    }
    else if (o->_class->hasProperty && o->_class->getProperty) {
        if (o->_class->hasProperty (o->_class, propertyName)) {
            return o->_class->getProperty (o, propertyName, variant);
        }
        else {
            return false;
        }
    }
    return false;
}

bool NPN_SetProperty (NPP npp, NPObject *o, NPIdentifier propertyName, const NPVariant *variant)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 

        ExecState *exec = obj->root->interpreter()->globalExec();
        Interpreter::lock();
        Value result;
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

bool NPN_RemoveProperty (NPP npp, NPObject *o, NPIdentifier propertyName)
{
    if (o->_class == NPScriptObjectClass) {
        JavaScriptObject *obj = (JavaScriptObject *)o; 
        ExecState *exec = obj->root->interpreter()->globalExec();

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

