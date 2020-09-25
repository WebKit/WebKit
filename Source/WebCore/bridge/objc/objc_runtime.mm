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
#import <JavaScriptCore/IsoSubspacePerVM.h>
#import <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
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

JSValue ObjcField::valueFromInstance(JSGlobalObject* lexicalGlobalObject, const Instance* instance) const
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue result = jsUndefined();
    
    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();

    JSLock::DropAllLocks dropAllLocks(lexicalGlobalObject); // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        if (id objcValue = [targetObject valueForKey:(__bridge NSString *)_name.get()])
            result = convertObjcValueToValue(lexicalGlobalObject, &objcValue, ObjcObjectType, instance->rootObject());
        {
            JSLockHolder lock(lexicalGlobalObject);
            ObjcInstance::moveGlobalExceptionToExecState(lexicalGlobalObject);
        }
    } @catch(NSException* localException) {
        JSLockHolder lock(lexicalGlobalObject);
        throwError(lexicalGlobalObject, scope, [localException reason]);
    }

    // Work around problem in some versions of GCC where result gets marked volatile and
    // it can't handle copying from a volatile to non-volatile.
    return const_cast<JSValue&>(result);
}

static id convertValueToObjcObject(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject->vm();
    RefPtr<RootObject> rootObject = findRootObject(vm.deprecatedVMEntryGlobalObject(lexicalGlobalObject));
    if (!rootObject)
        return nil;
    return [webScriptObjectClass() _convertValueToObjcValue:value originRootObject:rootObject.get() rootObject:rootObject.get()];
}

bool ObjcField::setValueToInstance(JSGlobalObject* lexicalGlobalObject, const Instance* instance, JSValue aValue) const
{
    JSC::VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    id targetObject = (static_cast<const ObjcInstance*>(instance))->getObject();
    id value = convertValueToObjcObject(lexicalGlobalObject, aValue);

    JSLock::DropAllLocks dropAllLocks(lexicalGlobalObject); // Can't put this inside the @try scope because it unwinds incorrectly.

    @try {
        [targetObject setValue:value forKey:(__bridge NSString *)_name.get()];
        {
            JSLockHolder lock(lexicalGlobalObject);
            ObjcInstance::moveGlobalExceptionToExecState(lexicalGlobalObject);
        }
        return true;
    } @catch(NSException* localException) {
        JSLockHolder lock(lexicalGlobalObject);
        throwError(lexicalGlobalObject, scope, [localException reason]);
        return false;
    }
}

// ---------------------- ObjcArray ----------------------

ObjcArray::ObjcArray(ObjectStructPtr a, RefPtr<RootObject>&& rootObject)
    : Array(WTFMove(rootObject))
    , _array(a)
{
}

bool ObjcArray::setValueAt(JSGlobalObject* lexicalGlobalObject, unsigned int index, JSValue aValue) const
{
    JSC::VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (![_array.get() respondsToSelector:@selector(insertObject:atIndex:)]) {
        throwTypeError(lexicalGlobalObject, scope, "Array is not mutable."_s);
        return false;
    }

    if (index > [_array.get() count]) {
        throwException(lexicalGlobalObject, scope, createRangeError(lexicalGlobalObject, "Index exceeds array size."));
        return false;
    }
    
    // Always try to convert the value to an ObjC object, so it can be placed in the
    // array.
    ObjcValue oValue = convertValueToObjcValue (lexicalGlobalObject, aValue, ObjcObjectType);

    @try {
        [_array.get() insertObject:(__bridge id)oValue.objectValue atIndex:index];
        return true;
    } @catch(NSException* localException) {
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Objective-C exception."));
        return false;
    }
}

JSValue ObjcArray::valueAt(JSGlobalObject* lexicalGlobalObject, unsigned int index) const
{
    JSC::VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (index > [_array.get() count])
        return throwException(lexicalGlobalObject, scope, createRangeError(lexicalGlobalObject, "Index exceeds array size."));
    @try {
        id obj = [_array.get() objectAtIndex:index];
        if (obj)
            return convertObjcValueToValue (lexicalGlobalObject, &obj, ObjcObjectType, m_rootObject.get());
    } @catch(NSException* localException) {
        return throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, "Objective-C exception."));
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

bool ObjcFallbackObjectImp::getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot& slot)
{
    // keep the prototype from getting called instead of just returning false
    slot.setUndefined();
    return true;
}

bool ObjcFallbackObjectImp::put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&)
{
    return false;
}

static JSC_DECLARE_HOST_FUNCTION(callObjCFallbackObject);

JSC_DEFINE_HOST_FUNCTION(callObjCFallbackObject, (JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame))
{
    JSC::VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    if (!thisValue.inherits<ObjCRuntimeObject>(vm))
        return throwVMTypeError(lexicalGlobalObject, scope);

    JSValue result = jsUndefined();

    ObjCRuntimeObject* runtimeObject = static_cast<ObjCRuntimeObject*>(asObject(thisValue));
    ObjcInstance* objcInstance = runtimeObject->getInternalObjCInstance();

    if (!objcInstance)
        return JSValue::encode(throwRuntimeObjectInvalidAccessError(lexicalGlobalObject, scope));
    
    objcInstance->begin();

    id targetObject = objcInstance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)]){
        ObjcClass* objcClass = static_cast<ObjcClass*>(objcInstance->getClass());
        std::unique_ptr<ObjcMethod> fallbackMethod(makeUnique<ObjcMethod>(objcClass->isa(), @selector(invokeUndefinedMethodFromWebScript:withArguments:)));
        const String& nameIdentifier = static_cast<ObjcFallbackObjectImp*>(callFrame->jsCallee())->propertyName();
        fallbackMethod->setJavaScriptName(nameIdentifier.createCFString().get());
        result = objcInstance->invokeObjcMethod(lexicalGlobalObject, callFrame, fallbackMethod.get());
    }
            
    objcInstance->end();

    return JSValue::encode(result);
}

CallData ObjcFallbackObjectImp::getCallData(JSCell* cell)
{
    CallData callData;

    ObjcFallbackObjectImp* thisObject = jsCast<ObjcFallbackObjectImp*>(cell);
    id targetObject = thisObject->_instance->getObject();
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)]) {
        callData.type = CallData::Type::Native;
        callData.native.function = callObjCFallbackObject;
    }

    return callData;
}

bool ObjcFallbackObjectImp::deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&)
{
    return false;
}

JSValue ObjcFallbackObjectImp::defaultValue(const JSObject* object, JSGlobalObject* lexicalGlobalObject, PreferredPrimitiveType)
{
    VM& vm = lexicalGlobalObject->vm();
    const ObjcFallbackObjectImp* thisObject = jsCast<const ObjcFallbackObjectImp*>(object);
    return thisObject->_instance->getValueOfUndefinedField(lexicalGlobalObject, Identifier::fromString(vm, thisObject->m_item));
}

bool ObjcFallbackObjectImp::toBoolean(JSGlobalObject*) const
{
    id targetObject = _instance->getObject();
    
    if ([targetObject respondsToSelector:@selector(invokeUndefinedMethodFromWebScript:withArguments:)])
        return true;
    
    return false;
}

JSC::IsoSubspace* ObjcFallbackObjectImp::subspaceForImpl(JSC::VM& vm)
{
    static NeverDestroyed<JSC::IsoSubspacePerVM> perVM([] (JSC::VM& vm) { return ISO_SUBSPACE_PARAMETERS(vm.destructibleObjectHeapCellType.get(), ObjcFallbackObjectImp); });
    return &perVM.get().forVM(vm);
}

}
}
