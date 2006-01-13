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
#include "config.h"
#include <Foundation/Foundation.h>


#include <JavaScriptCore/internal.h>

#include <objc_instance.h>
#include <WebScriptObjectPrivate.h>

#include <runtime_array.h>
#include <runtime_object.h>


using namespace KJS;
using namespace KJS::Bindings;

// ---------------------- ObjcMethod ----------------------

ObjcMethod::ObjcMethod(ClassStructPtr aClass, const char *name)
{
    _objcClass = aClass;
    _selector = name;   // Assume ObjC runtime keeps these around forever.
    _javaScriptName = 0;
}

const char *ObjcMethod::name() const
{
    return (const char *)_selector;
}

int ObjcMethod::numParameters() const
{
    return [getMethodSignature() numberOfArguments] - 2;
}


NSMethodSignature *ObjcMethod::getMethodSignature() const
{
    return [(id)_objcClass instanceMethodSignatureForSelector:(SEL)_selector];
}

void ObjcMethod::setJavaScriptName (CFStringRef n)
{
    if (n != _javaScriptName) {
        if (_javaScriptName != 0)
            CFRelease (_javaScriptName);
        _javaScriptName = (CFStringRef)CFRetain (n);
    }
}

// ---------------------- ObjcField ----------------------


ObjcField::ObjcField(Ivar ivar) 
{
    _ivar = ivar;    // Assume ObjectiveC runtime will keep this alive forever
    _name = 0;
}

ObjcField::ObjcField(CFStringRef name) 
{
    _ivar = 0;
    _name = (CFStringRef)CFRetain(name);
}

const char *ObjcField::name() const 
{
    if (_ivar)
        return _ivar->ivar_name;
    return [(NSString *)_name UTF8String];
}

RuntimeType ObjcField::type() const 
{ 
    if (_ivar)
        return _ivar->ivar_type;
    
    // Type is irrelevant if we use KV to set/get the value.
    return "";
}

JSValue *ObjcField::valueFromInstance(ExecState *exec, const Instance *instance) const
{
    JSValue *aValue;
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id objcValue = nil;

    NS_DURING
    
        NSString *key = [NSString stringWithCString:name()];
        objcValue = [targetObject valueForKey:key];
        
    NS_HANDLER
        
        throwError(exec, GeneralError, [localException reason]);
        
    NS_ENDHANDLER

    if (objcValue)
        aValue = convertObjcValueToValue (exec, &objcValue, ObjcObjectType);
    else
        aValue = jsUndefined();

    return aValue;
}

static id convertValueToObjcObject (ExecState *exec, JSValue *value)
{
    const Bindings::RootObject *root = rootForInterpreter(exec->interpreter());
    if (!root) {
        Bindings::RootObject *newRoot = new Bindings::RootObject(0);
        newRoot->setInterpreter (exec->interpreter());
        root = newRoot;
    }
    return [WebScriptObject _convertValueToObjcValue:value originExecutionContext:root executionContext:root ];
}


void ObjcField::setValueToInstance(ExecState *exec, const Instance *instance, JSValue *aValue) const
{
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id value = convertValueToObjcObject(exec, aValue);
    
    NS_DURING
    
        NSString *key = [NSString stringWithCString:name()];
        [targetObject setValue:value forKey:key];

    NS_HANDLER
        
        throwError(exec, GeneralError, [localException reason]);
        
    NS_ENDHANDLER
}

// ---------------------- ObjcArray ----------------------

ObjcArray::ObjcArray (ObjectStructPtr a) 
{
    _array = (id)CFRetain(a);
}

ObjcArray::~ObjcArray () 
{
    CFRelease(_array);
}


ObjcArray::ObjcArray (const ObjcArray &other) : Array() 
{
    _array = other._array;
    CFRetain(_array);
}

ObjcArray &ObjcArray::operator=(const ObjcArray &other)
{
    ObjectStructPtr _oldArray = _array;
    _array = other._array;
    CFRetain(_array);
    CFRelease(_oldArray);
    return *this;
}

void ObjcArray::setValueAt(ExecState *exec, unsigned int index, JSValue *aValue) const
{
    if (![_array respondsToSelector:@selector(insertObject:atIndex:)]) {
        throwError(exec, TypeError, "Array is not mutable.");
        return;
    }

    if (index > [_array count]) {
        throwError(exec, RangeError, "Index exceeds array size.");
        return;
    }
    
    // Always try to convert the value to an ObjC object, so it can be placed in the
    // array.
    ObjcValue oValue = convertValueToObjcValue (exec, aValue, ObjcObjectType);

NS_DURING

    [_array insertObject:oValue.objectValue atIndex:index];

NS_HANDLER
    
    throwError(exec, GeneralError, "Objective-C exception.");
    
NS_ENDHANDLER
}


JSValue *ObjcArray::valueAt(ExecState *exec, unsigned int index) const
{
    if (index > [_array count])
        return throwError(exec, RangeError, "Index exceeds array size.");
    
    ObjectStructPtr obj = 0;
    JSObject * volatile error;
    volatile bool haveError = false;

NS_DURING

    obj = [_array objectAtIndex:index];
    
NS_HANDLER
    
    error = throwError(exec, GeneralError, "Objective-C exception.");
    haveError = true;
    
NS_ENDHANDLER

    if (haveError)
        return error;
        
    return convertObjcValueToValue (exec, &obj, ObjcObjectType);
}

unsigned int ObjcArray::getLength() const
{
    return [_array count];
}


const ClassInfo ObjcFallbackObjectImp::info = {"ObjcFallbackObject", 0, 0, 0};

ObjcFallbackObjectImp::ObjcFallbackObjectImp(JSObject *proto)
  : JSObject(proto)
{
    _instance = 0;
}

ObjcFallbackObjectImp::ObjcFallbackObjectImp(ObjcInstance *i, const KJS::Identifier propertyName)
{
    _instance = i;
    _item = propertyName;
}

bool ObjcFallbackObjectImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    // keep the prototype from getting called instead of just returning false
    slot.setUndefined(this);
    return true;
}

void ObjcFallbackObjectImp::put(ExecState *exec, const Identifier &propertyName,
                 JSValue *value, int attr)
{
}

bool ObjcFallbackObjectImp::canPut(ExecState *exec, const Identifier &propertyName) const
{
    return false;
}


Type ObjcFallbackObjectImp::type() const
{
    id targetObject = _instance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return ObjectType;
    
    return UndefinedType;
}

bool ObjcFallbackObjectImp::implementsCall() const
{
    id targetObject = _instance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return true;
    
    return false;
}

JSValue *ObjcFallbackObjectImp::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
    JSValue *result = jsUndefined();
    
    RuntimeObjectImp *imp = static_cast<RuntimeObjectImp*>(thisObj);
    if (imp) {
        Instance *instance = imp->getInternalInstance();
        
        instance->begin();

        ObjcInstance *objcInstance = static_cast<ObjcInstance*>(instance);
        id targetObject = objcInstance->getObject();
        
        if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)]){
            MethodList methodList;
            ObjcClass *objcClass = static_cast<ObjcClass*>(instance->getClass());
            ObjcMethod *fallbackMethod = new ObjcMethod (objcClass->isa(), (const char *)@selector(invokeUndefinedMethodFromWebScript:withArguments:));
            fallbackMethod->setJavaScriptName((CFStringRef)[NSString stringWithCString:_item.ascii()]);
            methodList.addMethod ((Method *)fallbackMethod);
            result = instance->invokeMethod(exec, methodList, args);
            delete fallbackMethod;
        }
                
        instance->end();
    }

    return result;
}

bool ObjcFallbackObjectImp::deleteProperty(ExecState *exec,
                            const Identifier &propertyName)
{
    return false;
}

JSValue *ObjcFallbackObjectImp::defaultValue(ExecState *exec, Type hint) const
{
    return _instance->getValueOfUndefinedField(exec, _item, hint);
}

bool ObjcFallbackObjectImp::toBoolean(ExecState *exec) const
{
    id targetObject = _instance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return true;
    
    return false;
}
