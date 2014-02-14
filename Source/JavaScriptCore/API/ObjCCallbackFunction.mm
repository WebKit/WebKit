/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#import "JavaScriptCore.h"

#if JSC_OBJC_API_ENABLED

#import "APICallbackFunction.h"
#import "APICast.h"
#import "APIShims.h"
#import "DelayedReleaseScope.h"
#import "Error.h"
#import "JSCJSValueInlines.h"
#import "JSCell.h"
#import "JSCellInlines.h"
#import "JSContextInternal.h"
#import "JSWrapperMap.h"
#import "JSValueInternal.h"
#import "ObjCCallbackFunction.h"
#import "ObjcRuntimeExtras.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

class CallbackArgument {
public:
    virtual ~CallbackArgument();
    virtual void set(NSInvocation *, NSInteger, JSContext *, JSValueRef, JSValueRef*) = 0;

    OwnPtr<CallbackArgument> m_next;
};

CallbackArgument::~CallbackArgument()
{
}

class CallbackArgumentBoolean : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) override
    {
        bool value = JSValueToBoolean([context JSGlobalContextRef], argument);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

template<typename T>
class CallbackArgumentInteger : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        T value = (T)JSC::toInt32(JSValueToNumber([context JSGlobalContextRef], argument, exception));
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

template<typename T>
class CallbackArgumentDouble : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        T value = (T)JSValueToNumber([context JSGlobalContextRef], argument, exception);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentJSValue : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) override
    {
        JSValue *value = [JSValue valueWithJSValueRef:argument inContext:context];
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentId : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) override
    {
        id value = valueToObject(context, argument);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentOfClass : public CallbackArgument {
public:
    CallbackArgumentOfClass(Class cls)
        : CallbackArgument()
        , m_class(cls)
    {
        [m_class retain];
    }

private:
    virtual ~CallbackArgumentOfClass()
    {
        [m_class release];
    }

    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        JSGlobalContextRef contextRef = [context JSGlobalContextRef];

        id object = tryUnwrapObjcObject(contextRef, argument);
        if (object && [object isKindOfClass:m_class]) {
            [invocation setArgument:&object atIndex:argumentNumber];
            return;
        }

        if (JSValueIsNull(contextRef, argument) || JSValueIsUndefined(contextRef, argument)) {
            object = nil;
            [invocation setArgument:&object atIndex:argumentNumber];
            return;
        }

        *exception = toRef(JSC::createTypeError(toJS(contextRef), ASCIILiteral("Argument does not match Objective-C Class")));
    }

    Class m_class;
};

class CallbackArgumentNSNumber : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        id value = valueToNumber([context JSGlobalContextRef], argument, exception);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSString : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        id value = valueToString([context JSGlobalContextRef], argument, exception);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSDate : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        id value = valueToDate([context JSGlobalContextRef], argument, exception);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSArray : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        id value = valueToArray([context JSGlobalContextRef], argument, exception);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSDictionary : public CallbackArgument {
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) override
    {
        id value = valueToDictionary([context JSGlobalContextRef], argument, exception);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentStruct : public CallbackArgument {
public:
    CallbackArgumentStruct(NSInvocation *conversionInvocation, const char* encodedType)
        : m_conversionInvocation(conversionInvocation)
        , m_buffer(encodedType)
    {
    }
    
private:
    virtual void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) override
    {
        JSValue *value = [JSValue valueWithJSValueRef:argument inContext:context];
        [m_conversionInvocation invokeWithTarget:value];
        [m_conversionInvocation getReturnValue:m_buffer];
        [invocation setArgument:m_buffer atIndex:argumentNumber];
    }

    RetainPtr<NSInvocation> m_conversionInvocation;
    StructBuffer m_buffer;
};

class ArgumentTypeDelegate {
public:
    typedef CallbackArgument* ResultType;

    template<typename T>
    static ResultType typeInteger()
    {
        return new CallbackArgumentInteger<T>;
    }

    template<typename T>
    static ResultType typeDouble()
    {
        return new CallbackArgumentDouble<T>;
    }

    static ResultType typeBool()
    {
        return new CallbackArgumentBoolean;
    }

    static ResultType typeVoid()
    {
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }

    static ResultType typeId()
    {
        return new CallbackArgumentId;
    }

    static ResultType typeOfClass(const char* begin, const char* end)
    {
        StringRange copy(begin, end);
        Class cls = objc_getClass(copy);
        if (!cls)
            return 0;

        if (cls == [JSValue class])
            return new CallbackArgumentJSValue;
        if (cls == [NSString class])
            return new CallbackArgumentNSString;
        if (cls == [NSNumber class])
            return new CallbackArgumentNSNumber;
        if (cls == [NSDate class])
            return new CallbackArgumentNSDate;
        if (cls == [NSArray class])
            return new CallbackArgumentNSArray;
        if (cls == [NSDictionary class])
            return new CallbackArgumentNSDictionary;

        return new CallbackArgumentOfClass(cls);
    }

    static ResultType typeBlock(const char*, const char*)
    {
        return nil;
    }

    static ResultType typeStruct(const char* begin, const char* end)
    {
        StringRange copy(begin, end);
        if (NSInvocation *invocation = valueToTypeInvocationFor(copy))
            return new CallbackArgumentStruct(invocation, copy);
        return 0;
    }
};

class CallbackResult {
public:
    virtual ~CallbackResult()
    {
    }

    virtual JSValueRef get(NSInvocation *, JSContext *, JSValueRef*) = 0;
};

class CallbackResultVoid : public CallbackResult {
    virtual JSValueRef get(NSInvocation *, JSContext *context, JSValueRef*) override
    {
        return JSValueMakeUndefined([context JSGlobalContextRef]);
    }
};

class CallbackResultId : public CallbackResult {
    virtual JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) override
    {
        id value;
        [invocation getReturnValue:&value];
        return objectToValue(context, value);
    }
};

template<typename T>
class CallbackResultNumeric : public CallbackResult {
    virtual JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) override
    {
        T value;
        [invocation getReturnValue:&value];
        return JSValueMakeNumber([context JSGlobalContextRef], value);
    }
};

class CallbackResultBoolean : public CallbackResult {
    virtual JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) override
    {
        bool value;
        [invocation getReturnValue:&value];
        return JSValueMakeBoolean([context JSGlobalContextRef], value);
    }
};

class CallbackResultStruct : public CallbackResult {
public:
    CallbackResultStruct(NSInvocation *conversionInvocation, const char* encodedType)
        : m_conversionInvocation(conversionInvocation)
        , m_buffer(encodedType)
    {
    }
    
private:
    virtual JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) override
    {
        [invocation getReturnValue:m_buffer];

        [m_conversionInvocation setArgument:m_buffer atIndex:2];
        [m_conversionInvocation setArgument:&context atIndex:3];
        [m_conversionInvocation invokeWithTarget:[JSValue class]];

        JSValue *value;
        [m_conversionInvocation getReturnValue:&value];
        return valueInternalValue(value);
    }

    RetainPtr<NSInvocation> m_conversionInvocation;
    StructBuffer m_buffer;
};

class ResultTypeDelegate {
public:
    typedef CallbackResult* ResultType;

    template<typename T>
    static ResultType typeInteger()
    {
        return new CallbackResultNumeric<T>;
    }

    template<typename T>
    static ResultType typeDouble()
    {
        return new CallbackResultNumeric<T>;
    }

    static ResultType typeBool()
    {
        return new CallbackResultBoolean;
    }

    static ResultType typeVoid()
    {
        return new CallbackResultVoid;
    }

    static ResultType typeId()
    {
        return new CallbackResultId();
    }

    static ResultType typeOfClass(const char*, const char*)
    {
        return new CallbackResultId();
    }

    static ResultType typeBlock(const char*, const char*)
    {
        return new CallbackResultId();
    }

    static ResultType typeStruct(const char* begin, const char* end)
    {
        StringRange copy(begin, end);
        if (NSInvocation *invocation = typeToValueInvocationFor(copy))
            return new CallbackResultStruct(invocation, copy);
        return 0;
    }
};

enum CallbackType {
    CallbackInitMethod,
    CallbackInstanceMethod,
    CallbackClassMethod,
    CallbackBlock
};

namespace JSC {

class ObjCCallbackFunctionImpl {
public:
    ObjCCallbackFunctionImpl(NSInvocation *invocation, CallbackType type, Class instanceClass, PassOwnPtr<CallbackArgument> arguments, PassOwnPtr<CallbackResult> result)
        : m_type(type)
        , m_instanceClass([instanceClass retain])
        , m_invocation(invocation)
        , m_arguments(arguments)
        , m_result(result)
    {
        ASSERT((type != CallbackInstanceMethod && type != CallbackInitMethod) || instanceClass);
    }

    void destroy(Heap& heap)
    {
        // We need to explicitly release the target since we didn't call 
        // -retainArguments on m_invocation (and we don't want to do so).
        if (m_type == CallbackBlock || m_type == CallbackClassMethod)
            heap.releaseSoon(adoptNS([m_invocation.get() target]));
        [m_instanceClass release];
    }

    JSValueRef call(JSContext *context, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    id wrappedBlock()
    {
        return m_type == CallbackBlock ? [m_invocation target] : nil;
    }

    id wrappedConstructor()
    {
        switch (m_type) {
        case CallbackBlock:
            return [m_invocation target];
        case CallbackInitMethod:
            return m_instanceClass;
        default:
            return nil;
        }
    }

    bool isConstructible()
    {
        return !!wrappedBlock() || m_type == CallbackInitMethod;
    }

    String name();

private:
    CallbackType m_type;
    Class m_instanceClass;
    RetainPtr<NSInvocation> m_invocation;
    OwnPtr<CallbackArgument> m_arguments;
    OwnPtr<CallbackResult> m_result;
};

static JSValueRef objCCallbackFunctionCallAsFunction(JSContextRef callerContext, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Retake the API lock - we need this for a few reasons:
    // (1) We don't want to support the C-API's confusing drops-locks-once policy - should only drop locks if we can do so recursively.
    // (2) We're calling some JSC internals that require us to be on the 'inside' - e.g. createTypeError.
    // (3) We need to be locked (per context would be fine) against conflicting usage of the ObjCCallbackFunction's NSInvocation.
    JSC::APIEntryShim entryShim(toJS(callerContext));

    ObjCCallbackFunction* callback = static_cast<ObjCCallbackFunction*>(toJS(function));
    ObjCCallbackFunctionImpl* impl = callback->impl();
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(callback->globalObject()->globalExec())];

    CallbackData callbackData;
    JSValueRef result;
    @autoreleasepool {
        [context beginCallbackWithData:&callbackData calleeValue:function thisValue:thisObject argumentCount:argumentCount arguments:arguments];
        result = impl->call(context, thisObject, argumentCount, arguments, exception);
        if (context.exception)
            *exception = valueInternalValue(context.exception);
        [context endCallbackWithData:&callbackData];
    }
    return result;
}

static JSObjectRef objCCallbackFunctionCallAsConstructor(JSContextRef callerContext, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSC::APIEntryShim entryShim(toJS(callerContext));

    ObjCCallbackFunction* callback = static_cast<ObjCCallbackFunction*>(toJS(constructor));
    ObjCCallbackFunctionImpl* impl = callback->impl();
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(toJS(callerContext)->lexicalGlobalObject()->globalExec())];

    CallbackData callbackData;
    JSValueRef result;
    @autoreleasepool {
        [context beginCallbackWithData:&callbackData calleeValue:constructor thisValue:nil argumentCount:argumentCount arguments:arguments];
        result = impl->call(context, NULL, argumentCount, arguments, exception);
        if (context.exception)
            *exception = valueInternalValue(context.exception);
        [context endCallbackWithData:&callbackData];
    }

    JSGlobalContextRef contextRef = [context JSGlobalContextRef];
    if (*exception)
        return 0;

    if (!JSValueIsObject(contextRef, result)) {
        *exception = toRef(JSC::createTypeError(toJS(contextRef), ASCIILiteral("Objective-C blocks called as constructors must return an object.")));
        return 0;
    }
    return (JSObjectRef)result;
}

const JSC::ClassInfo ObjCCallbackFunction::s_info = { "CallbackFunction", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(ObjCCallbackFunction) };

ObjCCallbackFunction::ObjCCallbackFunction(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSObjectCallAsFunctionCallback functionCallback, JSObjectCallAsConstructorCallback constructCallback, PassOwnPtr<ObjCCallbackFunctionImpl> impl)
    : Base(vm, globalObject->objcCallbackFunctionStructure())
    , m_functionCallback(functionCallback)
    , m_constructCallback(constructCallback)
    , m_impl(impl)
{
}

ObjCCallbackFunction* ObjCCallbackFunction::create(JSC::VM& vm, JSC::JSGlobalObject* globalObject, const String& name, PassOwnPtr<ObjCCallbackFunctionImpl> impl)
{
    ObjCCallbackFunction* function = new (NotNull, allocateCell<ObjCCallbackFunction>(vm.heap)) ObjCCallbackFunction(vm, globalObject, objCCallbackFunctionCallAsFunction, objCCallbackFunctionCallAsConstructor, impl);
    function->finishCreation(vm, name);
    return function;
}

void ObjCCallbackFunction::destroy(JSCell* cell)
{
    ObjCCallbackFunction& function = *jsCast<ObjCCallbackFunction*>(cell);
    function.impl()->destroy(*Heap::heap(cell));
    function.~ObjCCallbackFunction();
}


CallType ObjCCallbackFunction::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = APICallbackFunction::call<ObjCCallbackFunction>;
    return CallTypeHost;
}

ConstructType ObjCCallbackFunction::getConstructData(JSCell* cell, ConstructData& constructData)
{
    ObjCCallbackFunction* callback = jsCast<ObjCCallbackFunction*>(cell);
    if (!callback->impl()->isConstructible())
        return Base::getConstructData(cell, constructData);
    constructData.native.function = APICallbackFunction::construct<ObjCCallbackFunction>;
    return ConstructTypeHost;
}

String ObjCCallbackFunctionImpl::name()
{
    if (m_type == CallbackInitMethod)
        return class_getName(m_instanceClass);
    // FIXME: Maybe we could support having the selector as the name of the non-init 
    // functions to make it a bit more user-friendly from the JS side?
    return "";
}

JSValueRef ObjCCallbackFunctionImpl::call(JSContext *context, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSGlobalContextRef contextRef = [context JSGlobalContextRef];

    id target;
    size_t firstArgument;
    switch (m_type) {
    case CallbackInitMethod: {
        RELEASE_ASSERT(!thisObject);
        target = [m_instanceClass alloc];
        if (!target || ![target isKindOfClass:m_instanceClass]) {
            *exception = toRef(JSC::createTypeError(toJS(contextRef), ASCIILiteral("self type check failed for Objective-C instance method")));
            return JSValueMakeUndefined(contextRef);
        }
        [m_invocation setTarget:target];
        firstArgument = 2;
        break;
    }
    case CallbackInstanceMethod: {
        target = tryUnwrapObjcObject(contextRef, thisObject);
        if (!target || ![target isKindOfClass:m_instanceClass]) {
            *exception = toRef(JSC::createTypeError(toJS(contextRef), ASCIILiteral("self type check failed for Objective-C instance method")));
            return JSValueMakeUndefined(contextRef);
        }
        [m_invocation setTarget:target];
        firstArgument = 2;
        break;
    }
    case CallbackClassMethod:
        firstArgument = 2;
        break;
    case CallbackBlock:
        firstArgument = 1;
    }

    size_t argumentNumber = 0;
    for (CallbackArgument* argument = m_arguments.get(); argument; argument = argument->m_next.get()) {
        JSValueRef value = argumentNumber < argumentCount ? arguments[argumentNumber] : JSValueMakeUndefined(contextRef);
        argument->set(m_invocation.get(), argumentNumber + firstArgument, context, value, exception);
        if (*exception)
            return JSValueMakeUndefined(contextRef);
        ++argumentNumber;
    }

    [m_invocation invoke];

    JSValueRef result = m_result->get(m_invocation.get(), context, exception);

    // Balance our call to -alloc with a call to -autorelease. We have to do this after calling -init
    // because init family methods are allowed to release the allocated object and return something 
    // else in its place.
    if (m_type == CallbackInitMethod) {
        id objcResult = tryUnwrapObjcObject(contextRef, result);
        if (objcResult)
            [objcResult autorelease];
    }

    return result;
}

} // namespace JSC

static bool blockSignatureContainsClass()
{
    static bool containsClass = ^{
        id block = ^(NSString *string){ return string; };
        return _Block_has_signature(block) && strstr(_Block_signature(block), "NSString");
    }();
    return containsClass;
}

inline bool skipNumber(const char*& position)
{
    if (!isASCIIDigit(*position))
        return false;
    while (isASCIIDigit(*++position)) { }
    return true;
}

static JSObjectRef objCCallbackFunctionForInvocation(JSContext *context, NSInvocation *invocation, CallbackType type, Class instanceClass, const char* signatureWithObjcClasses)
{
    const char* position = signatureWithObjcClasses;

    OwnPtr<CallbackResult> result = adoptPtr(parseObjCType<ResultTypeDelegate>(position));
    if (!result || !skipNumber(position))
        return nil;

    switch (type) {
    case CallbackInitMethod:
    case CallbackInstanceMethod:
    case CallbackClassMethod:
        // Methods are passed two implicit arguments - (id)self, and the selector.
        if ('@' != *position++ || !skipNumber(position) || ':' != *position++ || !skipNumber(position))
            return nil;
        break;
    case CallbackBlock:
        // Blocks are passed one implicit argument - the block, of type "@?".
        if (('@' != *position++) || ('?' != *position++) || !skipNumber(position))
            return nil;
        // Only allow arguments of type 'id' if the block signature contains the NS type information.
        if ((!blockSignatureContainsClass() && strchr(position, '@')))
            return nil;
        break;
    }

    OwnPtr<CallbackArgument> arguments = 0;
    OwnPtr<CallbackArgument>* nextArgument = &arguments;
    unsigned argumentCount = 0;
    while (*position) {
        OwnPtr<CallbackArgument> argument = adoptPtr(parseObjCType<ArgumentTypeDelegate>(position));
        if (!argument || !skipNumber(position))
            return nil;

        *nextArgument = argument.release();
        nextArgument = &(*nextArgument)->m_next;
        ++argumentCount;
    }

    JSC::ExecState* exec = toJS([context JSGlobalContextRef]);
    JSC::APIEntryShim shim(exec);
    OwnPtr<JSC::ObjCCallbackFunctionImpl> impl = adoptPtr(new JSC::ObjCCallbackFunctionImpl(invocation, type, instanceClass, arguments.release(), result.release()));
    return toRef(JSC::ObjCCallbackFunction::create(exec->vm(), exec->lexicalGlobalObject(), impl->name(), impl.release()));
}

JSObjectRef objCCallbackFunctionForInit(JSContext *context, Class cls, Protocol *protocol, SEL sel, const char* types)
{
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:types]];
    [invocation setSelector:sel];
    return objCCallbackFunctionForInvocation(context, invocation, CallbackInitMethod, cls, _protocol_getMethodTypeEncoding(protocol, sel, YES, YES));
}

JSObjectRef objCCallbackFunctionForMethod(JSContext *context, Class cls, Protocol *protocol, BOOL isInstanceMethod, SEL sel, const char* types)
{
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:types]];
    [invocation setSelector:sel];
    // We need to retain the target Class because m_invocation doesn't retain it
    // by default (and we don't want it to).
    if (!isInstanceMethod)
        [invocation setTarget:[cls retain]];
    return objCCallbackFunctionForInvocation(context, invocation, isInstanceMethod ? CallbackInstanceMethod : CallbackClassMethod, isInstanceMethod ? cls : nil, _protocol_getMethodTypeEncoding(protocol, sel, YES, isInstanceMethod));
}

JSObjectRef objCCallbackFunctionForBlock(JSContext *context, id target)
{
    if (!_Block_has_signature(target))
        return 0;
    const char* signature = _Block_signature(target);
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:signature]];

    // We don't want to use -retainArguments because that leaks memory. Arguments 
    // would be retained indefinitely between invocations of the callback.
    // Additionally, we copy the target because we want the block to stick around
    // until the ObjCCallbackFunctionImpl is destroyed.
    [invocation setTarget:[target copy]];

    return objCCallbackFunctionForInvocation(context, invocation, CallbackBlock, nil, signature);
}

id tryUnwrapConstructor(JSObjectRef object)
{
    if (!toJS(object)->inherits(JSC::ObjCCallbackFunction::info()))
        return nil;
    JSC::ObjCCallbackFunctionImpl* impl = static_cast<JSC::ObjCCallbackFunction*>(toJS(object))->impl();
    if (!impl->isConstructible())
        return nil;
    return impl->wrappedConstructor();
}

#endif
