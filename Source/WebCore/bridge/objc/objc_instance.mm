/*
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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
#import "objc_instance.h"

#import "JSDOMBinding.h"
#import "ObjCRuntimeObject.h"
#import "WebScriptObject.h"
#import "WebScriptObjectProtocol.h"
#import "runtime_method.h"
#import <JavaScriptCore/Error.h>
#import <JavaScriptCore/FunctionPrototype.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/ObjectPrototype.h>
#import <wtf/Assertions.h>
#import <wtf/HashMap.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ThreadSpecific.h>
#import <wtf/spi/cocoa/objcSPI.h>

#ifdef NDEBUG
#define OBJC_LOG(formatAndArgs...) ((void)0)
#else
#define OBJC_LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:%d -- %s:  ", __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}
#endif

using namespace JSC::Bindings;
using namespace JSC;

static NSString *s_exception;
static JSGlobalObject* s_exceptionEnvironment; // No need to protect this value, since we just use it for a pointer comparison.

static HashMap<CFTypeRef, ObjcInstance*>& wrapperCache()
{
    static NeverDestroyed<HashMap<CFTypeRef, ObjcInstance*>> map;
    return map;
}

RuntimeObject* ObjcInstance::newRuntimeObject(ExecState* exec)
{
    // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object.
    return ObjCRuntimeObject::create(exec->vm(), WebCore::deprecatedGetDOMStructure<ObjCRuntimeObject>(exec), this);
}

void ObjcInstance::setGlobalException(NSString* exception, JSGlobalObject* exceptionEnvironment)
{
    NSString *oldException = s_exception;
    s_exception = [exception copy];
    [oldException release];

    s_exceptionEnvironment = exceptionEnvironment;
}

void ObjcInstance::moveGlobalExceptionToExecState(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!s_exception) {
        ASSERT(!s_exceptionEnvironment);
        return;
    }

    if (!s_exceptionEnvironment || s_exceptionEnvironment == vm.vmEntryGlobalObject(exec)) {
        JSLockHolder lock(vm);
        throwError(exec, scope, s_exception);
    }

    [s_exception release];
    s_exception = nil;
    s_exceptionEnvironment = 0;
}

ObjcInstance::ObjcInstance(id instance, RefPtr<RootObject>&& rootObject) 
    : Instance(WTFMove(rootObject))
    , _instance(instance)
{
}

Ref<ObjcInstance> ObjcInstance::create(id instance, RefPtr<RootObject>&& rootObject)
{
    auto result = wrapperCache().add((__bridge CFTypeRef)instance, nullptr);
    if (result.isNewEntry) {
        auto wrapper = adoptRef(*new ObjcInstance(instance, WTFMove(rootObject)));
        result.iterator->value = wrapper.ptr();
        return wrapper;
    }

    ASSERT(result.iterator->value);
    return *result.iterator->value;
}

ObjcInstance::~ObjcInstance() 
{
    // Both -finalizeForWebScript and -dealloc/-finalize of _instance may require autorelease pools.
    @autoreleasepool {
        ASSERT(_instance);
        wrapperCache().remove((__bridge CFTypeRef)_instance.get());

        if ([_instance.get() respondsToSelector:@selector(finalizeForWebScript)])
            [_instance.get() performSelector:@selector(finalizeForWebScript)];
        _instance = 0;
    }
}

void ObjcInstance::virtualBegin()
{
    if (!m_autoreleasePool)
        m_autoreleasePool = objc_autoreleasePoolPush();
    _beginCount++;
}

void ObjcInstance::virtualEnd()
{
    _beginCount--;
    ASSERT(_beginCount >= 0);
    if (!_beginCount) {
        ASSERT(m_autoreleasePool);
        objc_autoreleasePoolPop(m_autoreleasePool);
        m_autoreleasePool = nullptr;
    }
}

Bindings::Class* ObjcInstance::getClass() const 
{
    if (!_instance)
        return 0;
    if (!_class)
        _class = ObjcClass::classForIsA(object_getClass(_instance.get()));
    return static_cast<Bindings::Class*>(_class);
}

bool ObjcInstance::supportsInvokeDefaultMethod() const
{
    return [_instance.get() respondsToSelector:@selector(invokeDefaultMethodWithArguments:)];
}

class ObjCRuntimeMethod : public RuntimeMethod {
public:
    static ObjCRuntimeMethod* create(ExecState* exec, JSGlobalObject* globalObject, const String& name, Bindings::Method* method)
    {
        VM& vm = globalObject->vm();
        // FIXME: deprecatedGetDOMStructure uses the prototype off of the wrong global object
        // We need to pass in the right global object for "i".
        Structure* domStructure = WebCore::deprecatedGetDOMStructure<ObjCRuntimeMethod>(exec);
        ObjCRuntimeMethod* runtimeMethod = new (NotNull, allocateCell<ObjCRuntimeMethod>(vm.heap)) ObjCRuntimeMethod(globalObject, domStructure, method);
        runtimeMethod->finishCreation(vm, name);
        return runtimeMethod;
    }

    static Structure* createStructure(VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), &s_info);
    }

    DECLARE_INFO;

private:
    typedef RuntimeMethod Base;

    ObjCRuntimeMethod(JSGlobalObject* globalObject, Structure* structure, Bindings::Method* method)
        : RuntimeMethod(globalObject, structure, method)
    {
    }

    void finishCreation(VM& vm, const String& name)
    {
        Base::finishCreation(vm, name);
        ASSERT(inherits(vm, info()));
    }
};

const ClassInfo ObjCRuntimeMethod::s_info = { "ObjCRuntimeMethod", &RuntimeMethod::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ObjCRuntimeMethod) };

JSC::JSValue ObjcInstance::getMethod(ExecState* exec, PropertyName propertyName)
{
    Method* method = getClass()->methodNamed(propertyName, this);
    return ObjCRuntimeMethod::create(exec, exec->lexicalGlobalObject(), propertyName.publicName(), method);
}

JSC::JSValue ObjcInstance::invokeMethod(ExecState* exec, RuntimeMethod* runtimeMethod)
{
    JSC::VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!asObject(runtimeMethod)->inherits<ObjCRuntimeMethod>(vm))
        return throwTypeError(exec, scope, "Attempt to invoke non-plug-in method on plug-in object."_s);

    ObjcMethod *method = static_cast<ObjcMethod*>(runtimeMethod->method());
    ASSERT(method);

    return invokeObjcMethod(exec, method);
}

JSC::JSValue ObjcInstance::invokeObjcMethod(ExecState* exec, ObjcMethod* method)
{
    JSValue result = jsUndefined();
    
    JSLock::DropAllLocks dropAllLocks(exec); // Can't put this inside the @try scope because it unwinds incorrectly.

    setGlobalException(nil);
    
@try {
    NSMethodSignature* signature = method->getMethodSignature();
    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:method->selector()];
    [invocation setTarget:_instance.get()];

    if (method->isFallbackMethod()) {
        if (objcValueTypeForType([signature methodReturnType]) != ObjcObjectType) {
            NSLog(@"Incorrect signature for invokeUndefinedMethodFromWebScript:withArguments: -- return type must be object.");
            return result;
        }

        // Invoke invokeUndefinedMethodFromWebScript:withArguments:, pass JavaScript function
        // name as first (actually at 2) argument and array of args as second.
        NSString* jsName = (__bridge NSString *)method->javaScriptName();
        [invocation setArgument:&jsName atIndex:2];

        NSMutableArray* objcArgs = [NSMutableArray array];
        int count = exec->argumentCount();
        for (int i = 0; i < count; i++) {
            ObjcValue value = convertValueToObjcValue(exec, exec->uncheckedArgument(i), ObjcObjectType);
            [objcArgs addObject:(__bridge id)value.objectValue];
        }
        [invocation setArgument:&objcArgs atIndex:3];
    } else {
        unsigned count = [signature numberOfArguments];
        for (unsigned i = 2; i < count; ++i) {
            const char* type = [signature getArgumentTypeAtIndex:i];
            ObjcValueType objcValueType = objcValueTypeForType(type);

            // Must have a valid argument type.  This method signature should have
            // been filtered already to ensure that it has acceptable argument
            // types.
            ASSERT(objcValueType != ObjcInvalidType && objcValueType != ObjcVoidType);

            ObjcValue value = convertValueToObjcValue(exec, exec->argument(i - 2), objcValueType);

            switch (objcValueType) {
                case ObjcObjectType:
                    [invocation setArgument:&value.objectValue atIndex:i];
                    break;
                case ObjcCharType:
                case ObjcUnsignedCharType:
                    [invocation setArgument:&value.charValue atIndex:i];
                    break;
                case ObjcShortType:
                case ObjcUnsignedShortType:
                    [invocation setArgument:&value.shortValue atIndex:i];
                    break;
                case ObjcBoolType:
                    [invocation setArgument:&value.booleanValue atIndex:i];
                    break;
                case ObjcIntType:
                case ObjcUnsignedIntType:
                    [invocation setArgument:&value.intValue atIndex:i];
                    break;
                case ObjcLongType:
                case ObjcUnsignedLongType:
                    [invocation setArgument:&value.longValue atIndex:i];
                    break;
                case ObjcLongLongType:
                case ObjcUnsignedLongLongType:
                    [invocation setArgument:&value.longLongValue atIndex:i];
                    break;
                case ObjcFloatType:
                    [invocation setArgument:&value.floatValue atIndex:i];
                    break;
                case ObjcDoubleType:
                    [invocation setArgument:&value.doubleValue atIndex:i];
                    break;
                default:
                    // Should never get here.  Argument types are filtered (and
                    // the assert above should have fired in the impossible case
                    // of an invalid type anyway).
                    fprintf(stderr, "%s: invalid type (%d)\n", __PRETTY_FUNCTION__, (int)objcValueType);
                    ASSERT_NOT_REACHED();
            }
        }
    }

    [invocation invoke];

    // Get the return value type.
    const char* type = [signature methodReturnType];
    ObjcValueType objcValueType = objcValueTypeForType(type);

    // Must have a valid return type.  This method signature should have
    // been filtered already to ensure that it have an acceptable return
    // type.
    ASSERT(objcValueType != ObjcInvalidType);

    // Get the return value and convert it to a JavaScript value. Length
    // of return value will never exceed the size of largest scalar
    // or a pointer.
    char buffer[1024];
    ASSERT([signature methodReturnLength] < 1024);

    if (*type != 'v') {
        [invocation getReturnValue:buffer];
        result = convertObjcValueToValue(exec, buffer, objcValueType, m_rootObject.get());
    }
} @catch(NSException* localException) {
}
    moveGlobalExceptionToExecState(exec);

    // Work around problem in some versions of GCC where result gets marked volatile and
    // it can't handle copying from a volatile to non-volatile.
    return const_cast<JSValue&>(result);
}

JSC::JSValue ObjcInstance::invokeDefaultMethod(ExecState* exec)
{
    JSValue result = jsUndefined();

    JSLock::DropAllLocks dropAllLocks(exec); // Can't put this inside the @try scope because it unwinds incorrectly.
    setGlobalException(nil);
    
@try {
    if (![_instance.get() respondsToSelector:@selector(invokeDefaultMethodWithArguments:)])
        return result;

    NSMethodSignature* signature = [_instance.get() methodSignatureForSelector:@selector(invokeDefaultMethodWithArguments:)];
    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:@selector(invokeDefaultMethodWithArguments:)];
    [invocation setTarget:_instance.get()];

    if (objcValueTypeForType([signature methodReturnType]) != ObjcObjectType) {
        NSLog(@"Incorrect signature for invokeDefaultMethodWithArguments: -- return type must be object.");
        return result;
    }

    NSMutableArray* objcArgs = [NSMutableArray array];
    unsigned count = exec->argumentCount();
    for (unsigned i = 0; i < count; i++) {
        ObjcValue value = convertValueToObjcValue(exec, exec->uncheckedArgument(i), ObjcObjectType);
        [objcArgs addObject:(__bridge id)value.objectValue];
    }
    [invocation setArgument:&objcArgs atIndex:2];

    [invocation invoke];

    // Get the return value type, should always be "@" because of
    // check above.
    const char* type = [signature methodReturnType];
    ObjcValueType objcValueType = objcValueTypeForType(type);

    // Get the return value and convert it to a JavaScript value. Length
    // of return value will never exceed the size of a pointer, so we're
    // OK with 32 here.
    char buffer[32];
    [invocation getReturnValue:buffer];
    result = convertObjcValueToValue(exec, buffer, objcValueType, m_rootObject.get());
} @catch(NSException* localException) {
}
    moveGlobalExceptionToExecState(exec);

    // Work around problem in some versions of GCC where result gets marked volatile and
    // it can't handle copying from a volatile to non-volatile.
    return const_cast<JSValue&>(result);
}

bool ObjcInstance::setValueOfUndefinedField(ExecState* exec, PropertyName propertyName, JSValue aValue)
{
    String name(propertyName.publicName());
    if (name.isNull())
        return false;

    id targetObject = getObject();
    if (![targetObject respondsToSelector:@selector(setValue:forUndefinedKey:)])
        return false;

    JSLock::DropAllLocks dropAllLocks(exec); // Can't put this inside the @try scope because it unwinds incorrectly.

    // This check is not really necessary because NSObject implements
    // setValue:forUndefinedKey:, and unfortunately the default implementation
    // throws an exception.
    if ([targetObject respondsToSelector:@selector(setValue:forUndefinedKey:)]){
        setGlobalException(nil);
    
        ObjcValue objcValue = convertValueToObjcValue(exec, aValue, ObjcObjectType);

        @try {
            [targetObject setValue:(__bridge id)objcValue.objectValue forUndefinedKey:[NSString stringWithCString:name.ascii().data() encoding:NSASCIIStringEncoding]];
        } @catch(NSException* localException) {
            // Do nothing.  Class did not override valueForUndefinedKey:.
        }

        moveGlobalExceptionToExecState(exec);
    }
    
    return true;
}

JSC::JSValue ObjcInstance::getValueOfUndefinedField(ExecState* exec, PropertyName propertyName) const
{
    String name(propertyName.publicName());
    if (name.isNull())
        return jsUndefined();

    JSValue result = jsUndefined();
    
    id targetObject = getObject();

    JSLock::DropAllLocks dropAllLocks(exec); // Can't put this inside the @try scope because it unwinds incorrectly.

    // This check is not really necessary because NSObject implements
    // valueForUndefinedKey:, and unfortunately the default implementation
    // throws an exception.
    if ([targetObject respondsToSelector:@selector(valueForUndefinedKey:)]){
        setGlobalException(nil);
    
        @try {
            id objcValue = [targetObject valueForUndefinedKey:[NSString stringWithCString:name.ascii().data() encoding:NSASCIIStringEncoding]];
            result = convertObjcValueToValue(exec, &objcValue, ObjcObjectType, m_rootObject.get());
        } @catch(NSException* localException) {
            // Do nothing.  Class did not override valueForUndefinedKey:.
        }

        moveGlobalExceptionToExecState(exec);
    }

    // Work around problem in some versions of GCC where result gets marked volatile and
    // it can't handle copying from a volatile to non-volatile.
    return const_cast<JSValue&>(result);
}

JSC::JSValue ObjcInstance::defaultValue(ExecState* exec, PreferredPrimitiveType hint) const
{
    if (hint == PreferString)
        return stringValue(exec);
    if (hint == PreferNumber)
        return numberValue(exec);
    if ([_instance.get() isKindOfClass:[NSString class]])
        return stringValue(exec);
    if ([_instance.get() isKindOfClass:[NSNumber class]])
        return numberValue(exec);
    return valueOf(exec);
}

static WTF::ThreadSpecific<uint32_t>* s_descriptionDepth;

@interface NSObject (WebDescriptionCategory)
- (NSString *)_web_description;
@end

@implementation NSObject (WebDescriptionCategory)

- (NSString *)_web_description
{
    ASSERT(s_descriptionDepth);
    if (s_descriptionDepth->isSet() && **s_descriptionDepth)
        return [NSString stringWithFormat:@"<%@>", NSStringFromClass([self class])];
    // We call _web_description here since this method should only be called
    // once we have already swizzled this method with the one on NSObject.
    // Thus, _web_description is actually the original description method.
    return [self _web_description];
}

@end

JSC::JSValue ObjcInstance::stringValue(ExecState* exec) const
{
    static std::once_flag initializeDescriptionDepthOnceFlag;
    std::call_once(initializeDescriptionDepthOnceFlag, [] {
        s_descriptionDepth = new WTF::ThreadSpecific<uint32_t>();
        **s_descriptionDepth = 0;

        auto descriptionMethod = class_getInstanceMethod([NSObject class], @selector(description));
        auto webDescriptionMethod = class_getInstanceMethod([NSObject class], @selector(_web_description));
        method_exchangeImplementations(descriptionMethod, webDescriptionMethod);
    });

    (**s_descriptionDepth)++;
    JSC::JSValue result = convertNSStringToString(exec, [getObject() description]);
    (**s_descriptionDepth)--;
    return result;
}

JSC::JSValue ObjcInstance::numberValue(ExecState*) const
{
    return jsNumber(0);
}

JSC::JSValue ObjcInstance::booleanValue() const
{
    return jsBoolean(false);
}

JSC::JSValue ObjcInstance::valueOf(ExecState* exec) const 
{
    return stringValue(exec);
}
