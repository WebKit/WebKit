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
#include "objc_runtime.h"

#include "objc_instance.h"
#include "runtime_array.h"
#include "runtime_object.h"
#include "WebScriptObject.h"

using namespace KJS;
using namespace KJS::Bindings;

extern ClassStructPtr KJS::Bindings::webScriptObjectClass()
{
    static ClassStructPtr<WebScriptObject> webScriptObjectClass = NSClassFromString(@"WebScriptObject");
    return webScriptObjectClass;
}

extern ClassStructPtr KJS::Bindings::webUndefinedClass()
{
    static ClassStructPtr<WebUndefined> webUndefinedClass = NSClassFromString(@"WebUndefined");
    return webUndefinedClass;
}

// ---------------------- ObjcMethod ----------------------

ObjcMethod::ObjcMethod(ClassStructPtr aClass, const char* name)
{
    _objcClass = aClass;
    _selector = name;   // Assume ObjC runtime keeps these around forever.
    _javaScriptName = 0;
}

const char* ObjcMethod::name() const
{
    return _selector;
}

int ObjcMethod::numParameters() const
{
    return [getMethodSignature() numberOfArguments] - 2;
}

NSMethodSignature* ObjcMethod::getMethodSignature() const
{
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
    return [_objcClass instanceMethodSignatureForSelector:sel_registerName(_selector)];
#else
    return [_objcClass instanceMethodSignatureForSelector:(SEL)_selector];
#endif
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

const char* ObjcField::name() const 
{
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
    if (_ivar)
        return ivar_getName(_ivar);
#else
    if (_ivar)
        return _ivar->ivar_name;
#endif
    return [(NSString*)_name.get() UTF8String];
}

JSValue* ObjcField::valueFromInstance(ExecState* exec, const Instance* instance) const
{
    JSValue* result = jsUndefined();
    
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();

   JSLock::DropAllLocks dropAllLocks; // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        NSString* key = [NSString stringWithCString:name() encoding:NSASCIIStringEncoding];
        if (id objcValue = [targetObject valueForKey:key])
            result = convertObjcValueToValue(exec, &objcValue, ObjcObjectType, instance->rootObject());
    } @catch(NSException* localException) {
        JSLock::lock();
        throwError(exec, GeneralError, [localException reason]);
        JSLock::unlock();
    }

    return result;
}

static id convertValueToObjcObject(ExecState* exec, JSValue* value)
{
    RefPtr<RootObject> rootObject = findRootObject(exec->dynamicGlobalObject());
    if (!rootObject)
        return nil;
    return [webScriptObjectClass() _convertValueToObjcValue:value originRootObject:rootObject.get() rootObject:rootObject.get()];
}

void ObjcField::setValueToInstance(ExecState* exec, const Instance* instance, JSValue* aValue) const
{
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id value = convertValueToObjcObject(exec, aValue);

   JSLock::DropAllLocks dropAllLocks; // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        NSString* key = [NSString stringWithCString:name() encoding:NSASCIIStringEncoding];
        [targetObject setValue:value forKey:key];
    } @catch(NSException* localException) {
        JSLock::lock();
        throwError(exec, GeneralError, [localException reason]);
        JSLock::unlock();
    }
}

// ---------------------- ObjcArray ----------------------

ObjcArray::ObjcArray(ObjectStructPtr a, PassRefPtr<RootObject> rootObject)
    : Array(rootObject)
    , _array(a)
{
}

void ObjcArray::setValueAt(ExecState* exec, unsigned int index, JSValue* aValue) const
{
    if (![_array.get() respondsToSelector:@selector(insertObject:atIndex:)]) {
        throwError(exec, TypeError, "Array is not mutable.");
        return;
    }

    if (index > [_array.get() count]) {
        throwError(exec, RangeError, "Index exceeds array size.");
        return;
    }
    
    // Always try to convert the value to an ObjC object, so it can be placed in the
    // array.
    ObjcValue oValue = convertValueToObjcValue (exec, aValue, ObjcObjectType);

    @try {
        [_array.get() insertObject:oValue.objectValue atIndex:index];
    } @catch(NSException* localException) {
        throwError(exec, GeneralError, "Objective-C exception.");
    }
}

JSValue* ObjcArray::valueAt(ExecState* exec, unsigned int index) const
{
    if (index > [_array.get() count])
        return throwError(exec, RangeError, "Index exceeds array size.");
    @try {
        id obj = [_array.get() objectAtIndex:index];
        if (obj)
            return convertObjcValueToValue (exec, &obj, ObjcObjectType, _rootObject.get());
    } @catch(NSException* localException) {
        return throwError(exec, GeneralError, "Objective-C exception.");
    }
    return jsUndefined();
}

unsigned int ObjcArray::getLength() const
{
    return [_array.get() count];
}

const ClassInfo ObjcFallbackObjectImp::info = { "ObjcFallbackObject", 0, 0 };

ObjcFallbackObjectImp::ObjcFallbackObjectImp(ObjcInstance* i, const KJS::Identifier propertyName)
: _instance(i)
, _item(propertyName)
{
}

bool ObjcFallbackObjectImp::getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot& slot)
{
    // keep the prototype from getting called instead of just returning false
    slot.setUndefined(this);
    return true;
}

void ObjcFallbackObjectImp::put(ExecState*, const Identifier&, JSValue*, int)
{
}

bool ObjcFallbackObjectImp::canPut(ExecState*, const Identifier&) const
{
    return false;
}


JSType ObjcFallbackObjectImp::type() const
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

JSValue* ObjcFallbackObjectImp::callAsFunction(ExecState* exec, JSObject* thisObj, const List &args)
{
    if (thisObj->classInfo() != &KJS::RuntimeObjectImp::info)
        return throwError(exec, TypeError);

    JSValue* result = jsUndefined();

    RuntimeObjectImp* imp = static_cast<RuntimeObjectImp*>(thisObj);
    Instance* instance = imp->getInternalInstance();

    if (!instance)
        return RuntimeObjectImp::throwInvalidAccessError(exec);
    
    instance->begin();

    ObjcInstance* objcInstance = static_cast<ObjcInstance*>(instance);
    id targetObject = objcInstance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)]){
        MethodList methodList;
        ObjcClass* objcClass = static_cast<ObjcClass*>(instance->getClass());
        ObjcMethod* fallbackMethod = new ObjcMethod (objcClass->isa(), sel_getName(@selector(invokeUndefinedMethodFromWebScript:withArguments:)));
        fallbackMethod->setJavaScriptName((CFStringRef)[NSString stringWithCString:_item.ascii() encoding:NSASCIIStringEncoding]);
        methodList.append(fallbackMethod);
        result = instance->invokeMethod(exec, methodList, args);
        delete fallbackMethod;
    }
            
    instance->end();

    return result;
}

bool ObjcFallbackObjectImp::deleteProperty(ExecState*, const Identifier&)
{
    return false;
}

JSValue* ObjcFallbackObjectImp::defaultValue(ExecState* exec, JSType hint) const
{
    return _instance->getValueOfUndefinedField(exec, _item, hint);
}

bool ObjcFallbackObjectImp::toBoolean(ExecState *) const
{
    id targetObject = _instance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return true;
    
    return false;
}
