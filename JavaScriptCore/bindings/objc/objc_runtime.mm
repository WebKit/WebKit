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
}

const char *ObjcMethod::name() const
{
    return (const char *)_selector;
}

long ObjcMethod::numParameters() const
{
    return [getMethodSignature() numberOfArguments] - 2;
}


NSMethodSignature *ObjcMethod::getMethodSignature() const
{
    return [(id)_objcClass instanceMethodSignatureForSelector:(SEL)_selector];
}

// ---------------------- ObjcField ----------------------


ObjcField::ObjcField(Ivar ivar) 
{
    _ivar = ivar;    // Assume ObjectiveC runtime will keep this alive forever
}

const char *ObjcField::name() const 
{
    return _ivar->ivar_name;
}

RuntimeType ObjcField::type() const 
{
    return _ivar->ivar_type;
}

Value ObjcField::valueFromInstance(KJS::ExecState *exec, const Instance *instance) const
{
    Value aValue;
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id objcValue = nil;

    NS_DURING
    
        NSString *key = [NSString stringWithCString:_ivar->ivar_name];
        objcValue = [targetObject valueForKey:key];
        
    NS_HANDLER
        
        Value exceptionValue = Error::create(exec, GeneralError, [[localException reason] lossyCString]);
        exec->setException(exceptionValue);
        
    NS_ENDHANDLER

    if (objcValue)
        aValue = convertObjcValueToValue (exec, &objcValue, ObjcObjectType);
    else
        aValue = Undefined();

    return aValue;
}

static id convertValueToObjcObject (KJS::ExecState *exec, const KJS::Value &value)
{
    const Bindings::RootObject *root = rootForInterpreter(exec->interpreter());
    if (!root) {
        Bindings::RootObject *newRoot = new KJS::Bindings::RootObject(0);
        newRoot->setInterpreter (exec->interpreter());
        root = newRoot;
    }
    return [WebScriptObject _convertValueToObjcValue:value root:root];
}


void ObjcField::setValueToInstance(KJS::ExecState *exec, const Instance *instance, const KJS::Value &aValue) const
{
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id value = convertValueToObjcObject(exec, aValue);
    
    NS_DURING
    
        NSString *key = [NSString stringWithCString:_ivar->ivar_name];
        [targetObject setValue:value forKey:key];
        
    NS_HANDLER
        
        Value aValue = Error::create(exec, GeneralError, [[localException reason] lossyCString]);
        exec->setException(aValue);
        
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

void ObjcArray::setValueAt(KJS::ExecState *exec, unsigned int index, const KJS::Value &aValue) const
{
    if (![_array respondsToSelector:@selector(insertObject:atIndex:)]) {
        Object error = Error::create(exec, TypeError, "Array is not mutable.");
        exec->setException(error);
        return;
    }

    if (index > [_array count]) {
        Object error = Error::create(exec, RangeError, "Index exceeds array size.");
        exec->setException(error);
        return;
    }
    
    // Always try to convert the value to an ObjC object, so it can be placed in the
    // array.
    ObjcValue oValue = convertValueToObjcValue (exec, aValue, ObjcObjectType);

NS_DURING

    [_array insertObject:oValue.objectValue atIndex:index];

NS_HANDLER
    
    Object error = Error::create(exec, GeneralError, "ObjectiveC exception.");
    exec->setException(error);
    
NS_ENDHANDLER
}


KJS::Value ObjcArray::valueAt(KJS::ExecState *exec, unsigned int index) const
{
    ObjectStructPtr obj = 0;
    Object error;
    volatile bool haveError = false;
    
    if (index > [_array count]) {
        Object error = Error::create(exec, RangeError, "Index exceeds array size.");
        exec->setException(error);
        return error;
    }
    
NS_DURING

    obj = [_array objectAtIndex:index];
    
NS_HANDLER
    
    Object error = Error::create(exec, GeneralError, "ObjectiveC exception.");
    exec->setException(error);
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
