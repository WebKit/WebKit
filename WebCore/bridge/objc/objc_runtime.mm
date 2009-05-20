/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#include "JSDOMBinding.h"
#include "WebScriptObject.h"
#include "objc_instance.h"
#include "runtime_array.h"
#include "runtime_object.h"
#include <runtime/Error.h>
#include <runtime/JSGlobalObject.h>
#include <runtime/JSLock.h>
#include <runtime/ObjectPrototype.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

namespace JSC {
namespace Bindings {

ClassStructPtr webScriptObjectClass()
{
    static ClassStructPtr<WebScriptObject> webScriptObjectClass = NSClassFromString(@"WebScriptObject");
    return webScriptObjectClass;
}

ClassStructPtr webUndefinedClass()
{
    static ClassStructPtr<WebUndefined> webUndefinedClass = NSClassFromString(@"WebUndefined");
    return webUndefinedClass;
}

// ---------------------- ObjcMethod ----------------------

ObjcMethod::ObjcMethod(ClassStructPtr aClass, SEL selector)
    : _objcClass(aClass)
    , _selector(selector)
{
}

int ObjcMethod::numParameters() const
{
    return [getMethodSignature() numberOfArguments] - 2;
}

NSMethodSignature* ObjcMethod::getMethodSignature() const
{
    return [_objcClass instanceMethodSignatureForSelector:_selector];
}

// ---------------------- ObjcField ----------------------

ObjcField::ObjcField(Ivar ivar) 
    : _ivar(ivar)
#if defined(OBJC_API_VERSION) && OBJC_API_VERSION >= 2
    , _name(AdoptCF, CFStringCreateWithCString(0, ivar_getName(_ivar), kCFStringEncodingASCII))
#else
    , _name(AdoptCF, CFStringCreateWithCString(0, _ivar->ivar_name, kCFStringEncodingASCII))
#endif
{
}

ObjcField::ObjcField(CFStringRef name)
    : _ivar(0)
    , _name(name)
{
}

JSValue ObjcField::valueFromInstance(ExecState* exec, const Instance* instance) const
{
    JSValue result = jsUndefined();
    
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();

    JSLock::DropAllLocks dropAllLocks(false); // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        if (id objcValue = [targetObject valueForKey:(NSString *)_name.get()])
            result = convertObjcValueToValue(exec, &objcValue, ObjcObjectType, instance->rootObject());
    } @catch(NSException* localException) {
        JSLock::lock(false);
        throwError(exec, GeneralError, [localException reason]);
        JSLock::unlock(false);
    }

    // Work around problem in some versions of GCC where result gets marked volatile and
    // it can't handle copying from a volatile to non-volatile.
    return const_cast<JSValue&>(result);
}

static id convertValueToObjcObject(ExecState* exec, JSValue value)
{
    RefPtr<RootObject> rootObject = findRootObject(exec->dynamicGlobalObject());
    if (!rootObject)
        return nil;
    return [webScriptObjectClass() _convertValueToObjcValue:value originRootObject:rootObject.get() rootObject:rootObject.get()];
}

void ObjcField::setValueToInstance(ExecState* exec, const Instance* instance, JSValue aValue) const
{
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id value = convertValueToObjcObject(exec, aValue);

    JSLock::DropAllLocks dropAllLocks(false); // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        [targetObject setValue:value forKey:(NSString *)_name.get()];
    } @catch(NSException* localException) {
        JSLock::lock(false);
        throwError(exec, GeneralError, [localException reason]);
        JSLock::unlock(false);
    }
}

// ---------------------- ObjcArray ----------------------

ObjcArray::ObjcArray(ObjectStructPtr a, PassRefPtr<RootObject> rootObject)
    : Array(rootObject)
    , _array(a)
{
}

void ObjcArray::setValueAt(ExecState* exec, unsigned int index, JSValue aValue) const
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

JSValue ObjcArray::valueAt(ExecState* exec, unsigned int index) const
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

const ClassInfo ObjcFallbackObjectImp::s_info = { "ObjcFallbackObject", 0, 0, 0 };

ObjcFallbackObjectImp::ObjcFallbackObjectImp(ExecState* exec, ObjcInstance* i, const Identifier& propertyName)
    : JSObject(getDOMStructure<ObjcFallbackObjectImp>(exec))
    , _instance(i)
    , _item(propertyName)
{
}

bool ObjcFallbackObjectImp::getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot& slot)
{
    // keep the prototype from getting called instead of just returning false
    slot.setUndefined();
    return true;
}

void ObjcFallbackObjectImp::put(ExecState*, const Identifier&, JSValue, PutPropertySlot&)
{
}

static JSValue JSC_HOST_CALL callObjCFallbackObject(ExecState* exec, JSObject* function, JSValue thisValue, const ArgList& args)
{
    if (!thisValue.isObject(&RuntimeObjectImp::s_info))
        return throwError(exec, TypeError);

    JSValue result = jsUndefined();

    RuntimeObjectImp* imp = static_cast<RuntimeObjectImp*>(asObject(thisValue));
    Instance* instance = imp->getInternalInstance();

    if (!instance)
        return RuntimeObjectImp::throwInvalidAccessError(exec);
    
    instance->begin();

    ObjcInstance* objcInstance = static_cast<ObjcInstance*>(instance);
    id targetObject = objcInstance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)]){
        ObjcClass* objcClass = static_cast<ObjcClass*>(instance->getClass());
        OwnPtr<ObjcMethod> fallbackMethod(new ObjcMethod(objcClass->isa(), @selector(invokeUndefinedMethodFromWebScript:withArguments:)));
        const Identifier& nameIdentifier = static_cast<ObjcFallbackObjectImp*>(function)->propertyName();
        RetainPtr<CFStringRef> name(AdoptCF, CFStringCreateWithCharacters(0, nameIdentifier.data(), nameIdentifier.size()));
        fallbackMethod->setJavaScriptName(name.get());
        MethodList methodList;
        methodList.append(fallbackMethod.get());
        result = instance->invokeMethod(exec, methodList, args);
    }
            
    instance->end();

    return result;
}

CallType ObjcFallbackObjectImp::getCallData(CallData& callData)
{
    id targetObject = _instance->getObject();
    if (![targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return CallTypeNone;
    callData.native.function = callObjCFallbackObject;
    return CallTypeHost;
}

bool ObjcFallbackObjectImp::deleteProperty(ExecState*, const Identifier&)
{
    return false;
}

JSValue ObjcFallbackObjectImp::defaultValue(ExecState* exec, PreferredPrimitiveType) const
{
    return _instance->getValueOfUndefinedField(exec, _item);
}

bool ObjcFallbackObjectImp::toBoolean(ExecState *) const
{
    id targetObject = _instance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return true;
    
    return false;
}

}
}
