/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "objc_runtime.h"

#import "JSDOMBinding.h"
#import "ObjCRuntimeObject.h"
#import "WebScriptObject.h"
#import "WebScriptObjectProtocol.h"
#import "objc_instance.h"
#import "runtime_array.h"
#import "runtime_object.h"
#import <JavaScriptCore/Error.h>
#import <JavaScriptCore/JSGlobalObject.h>
#import <JavaScriptCore/JSLock.h>
#import <wtf/RetainPtr.h>

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

bool ObjcMethod::isFallbackMethod() const
{
    return _selector == @selector(invokeUndefinedMethodFromWebScript:withArguments:);
}

// ---------------------- ObjcField ----------------------

ObjcField::ObjcField(Ivar ivar) 
    : _ivar(ivar)
    , _name(adoptCF(CFStringCreateWithCString(0, ivar_getName(_ivar), kCFStringEncodingASCII)))
{
}

ObjcField::ObjcField(CFStringRef name)
    : _ivar(0)
    , _name(name)
{
}

JSValue ObjcField::valueFromInstance(ExecState* exec, const Instance* instance) const
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue result = jsUndefined();
    
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();

    JSLock::DropAllLocks dropAllLocks(exec); // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        if (id objcValue = [targetObject valueForKey:(__bridge NSString *)_name.get()])
            result = convertObjcValueToValue(exec, &objcValue, ObjcObjectType, instance->rootObject());
        {
            JSLockHolder lock(exec);
            ObjcInstance::moveGlobalExceptionToExecState(exec);
        }
    } @catch(NSException* localException) {
        JSLockHolder lock(exec);
        throwError(exec, scope, [localException reason]);
    }

    // Work around problem in some versions of GCC where result gets marked volatile and
    // it can't handle copying from a volatile to non-volatile.
    return const_cast<JSValue&>(result);
}

static id convertValueToObjcObject(ExecState* exec, JSValue value)
{
    VM& vm = exec->vm();
    RefPtr<RootObject> rootObject = findRootObject(vm.vmEntryGlobalObject(exec));
    if (!rootObject)
        return nil;
    return [webScriptObjectClass() _convertValueToObjcValue:value originRootObject:rootObject.get() rootObject:rootObject.get()];
}

bool ObjcField::setValueToInstance(ExecState* exec, const Instance* instance, JSValue aValue) const
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id value = convertValueToObjcObject(exec, aValue);

    JSLock::DropAllLocks dropAllLocks(exec); // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        [targetObject setValue:value forKey:(__bridge NSString *)_name.get()];
        {
            JSLockHolder lock(exec);
            ObjcInstance::moveGlobalExceptionToExecState(exec);
        }
        return true;
    } @catch(NSException* localException) {
        JSLockHolder lock(exec);
        throwError(exec, scope, [localException reason]);
        return false;
    }
}

// ---------------------- ObjcArray ----------------------

ObjcArray::ObjcArray(ObjectStructPtr a, RefPtr<RootObject>&& rootObject)
    : Array(WTFMove(rootObject))
    , _array(a)
{
}

bool ObjcArray::setValueAt(ExecState* exec, unsigned int index, JSValue aValue) const
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (![_array.get() respondsToSelector:@selector(insertObject:atIndex:)]) {
        throwTypeError(exec, scope, "Array is not mutable."_s);
        return false;
    }

    if (index > [_array.get() count]) {
        throwException(exec, scope, createRangeError(exec, "Index exceeds array size."));
        return false;
    }
    
    // Always try to convert the value to an ObjC object, so it can be placed in the
    // array.
    ObjcValue oValue = convertValueToObjcValue (exec, aValue, ObjcObjectType);

    @try {
        [_array.get() insertObject:(__bridge id)oValue.objectValue atIndex:index];
        return true;
    } @catch(NSException* localException) {
        throwException(exec, scope, createError(exec, "Objective-C exception."));
        return false;
    }
}

JSValue ObjcArray::valueAt(ExecState* exec, unsigned int index) const
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (index > [_array.get() count])
        return throwException(exec, scope, createRangeError(exec, "Index exceeds array size."));
    @try {
        id obj = [_array.get() objectAtIndex:index];
        if (obj)
            return convertObjcValueToValue (exec, &obj, ObjcObjectType, m_rootObject.get());
    } @catch(NSException* localException) {
        return throwException(exec, scope, createError(exec, "Objective-C exception."));
    }
    return jsUndefined();
}

unsigned int ObjcArray::getLength() const
{
    return [_array.get() count];
}

const ClassInfo ObjcFallbackObjectImp::s_info = { "ObjcFallbackObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ObjcFallbackObjectImp) };

ObjcFallbackObjectImp::ObjcFallbackObjectImp(JSGlobalObject* globalObject, Structure* structure, ObjcInstance* i, const String& propertyName)
    : JSDestructibleObject(globalObject->vm(), structure)
    , _instance(i)
    , m_item(propertyName)
{
}

void ObjcFallbackObjectImp::destroy(JSCell* cell)
{
    static_cast<ObjcFallbackObjectImp*>(cell)->ObjcFallbackObjectImp::~ObjcFallbackObjectImp();
}

void ObjcFallbackObjectImp::finishCreation(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
}

bool ObjcFallbackObjectImp::getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot& slot)
{
    // keep the prototype from getting called instead of just returning false
    slot.setUndefined();
    return true;
}

bool ObjcFallbackObjectImp::put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&)
{
    return false;
}

static EncodedJSValue JSC_HOST_CALL callObjCFallbackObject(ExecState* exec)
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = exec->thisValue();
    if (!thisValue.inherits<ObjCRuntimeObject>(vm))
        return throwVMTypeError(exec, scope);

    JSValue result = jsUndefined();

    ObjCRuntimeObject* runtimeObject = static_cast<ObjCRuntimeObject*>(asObject(thisValue));
    ObjcInstance* objcInstance = runtimeObject->getInternalObjCInstance();

    if (!objcInstance)
        return JSValue::encode(RuntimeObject::throwInvalidAccessError(exec, scope));
    
    objcInstance->begin();

    id targetObject = objcInstance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)]){
        ObjcClass* objcClass = static_cast<ObjcClass*>(objcInstance->getClass());
        std::unique_ptr<ObjcMethod> fallbackMethod(makeUnique<ObjcMethod>(objcClass->isa(), @selector(invokeUndefinedMethodFromWebScript:withArguments:)));
        const String& nameIdentifier = static_cast<ObjcFallbackObjectImp*>(exec->jsCallee())->propertyName();
        fallbackMethod->setJavaScriptName(nameIdentifier.createCFString().get());
        result = objcInstance->invokeObjcMethod(exec, fallbackMethod.get());
    }
            
    objcInstance->end();

    return JSValue::encode(result);
}

CallType ObjcFallbackObjectImp::getCallData(JSCell* cell, CallData& callData)
{
    ObjcFallbackObjectImp* thisObject = jsCast<ObjcFallbackObjectImp*>(cell);
    id targetObject = thisObject->_instance->getObject();
    if (![targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return CallType::None;
    callData.native.function = callObjCFallbackObject;
    return CallType::Host;
}

bool ObjcFallbackObjectImp::deleteProperty(JSCell*, ExecState*, PropertyName)
{
    return false;
}

JSValue ObjcFallbackObjectImp::defaultValue(const JSObject* object, ExecState* exec, PreferredPrimitiveType)
{
    VM& vm = exec->vm();
    const ObjcFallbackObjectImp* thisObject = jsCast<const ObjcFallbackObjectImp*>(object);
    return thisObject->_instance->getValueOfUndefinedField(exec, Identifier::fromString(vm, thisObject->m_item));
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
