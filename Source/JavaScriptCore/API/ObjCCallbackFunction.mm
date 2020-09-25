/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#import "JavaScriptCore.h"

#if JSC_OBJC_API_ENABLED

#import "APICallbackFunction.h"
#import "APICast.h"
#import "Error.h"
#import "JSCell.h"
#import "JSCInlines.h"
#import "JSContextInternal.h"
#import "JSWrapperMap.h"
#import "JSValueInternal.h"
#import "ObjCCallbackFunction.h"
#import "ObjcRuntimeExtras.h"
#import "StructureInlines.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

class CallbackArgument {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~CallbackArgument();
    virtual void set(NSInvocation *, NSInteger, JSContext *, JSValueRef, JSValueRef*) = 0;

    std::unique_ptr<CallbackArgument> m_next;
};

CallbackArgument::~CallbackArgument()
{
}

class CallbackArgumentBoolean final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) final
    {
        bool value = JSValueToBoolean([context JSGlobalContextRef], argument);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

template<typename T>
class CallbackArgumentInteger final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        T value = (T)JSC::toInt32(JSValueToNumber([context JSGlobalContextRef], argument, exception));
        if (*exception)
            return;
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

template<typename T>
class CallbackArgumentDouble final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        T value = (T)JSValueToNumber([context JSGlobalContextRef], argument, exception);
        if (*exception)
            return;
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentJSValue final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) final
    {
        JSValue *value = [JSValue valueWithJSValueRef:argument inContext:context];
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentId final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) final
    {
        id value = valueToObject(context, argument);
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentOfClass final : public CallbackArgument {
public:
    CallbackArgumentOfClass(Class cls)
        : m_class(cls)
    {
    }

private:
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        JSGlobalContextRef contextRef = [context JSGlobalContextRef];

        id object = tryUnwrapObjcObject(contextRef, argument);
        if (object && [object isKindOfClass:m_class.get()]) {
            [invocation setArgument:&object atIndex:argumentNumber];
            return;
        }

        if (JSValueIsNull(contextRef, argument) || JSValueIsUndefined(contextRef, argument)) {
            object = nil;
            [invocation setArgument:&object atIndex:argumentNumber];
            return;
        }

        *exception = toRef(JSC::createTypeError(toJS(contextRef), "Argument does not match Objective-C Class"_s));
    }

    RetainPtr<Class> m_class;
};

class CallbackArgumentNSNumber final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        id value = valueToNumber([context JSGlobalContextRef], argument, exception);
        if (*exception)
            return;
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSString final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        id value = valueToString([context JSGlobalContextRef], argument, exception);
        if (*exception)
            return;
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSDate final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        id value = valueToDate([context JSGlobalContextRef], argument, exception);
        if (*exception)
            return;
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSArray final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        id value = valueToArray([context JSGlobalContextRef], argument, exception);
        if (*exception)
            return;
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentNSDictionary final : public CallbackArgument {
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef* exception) final
    {
        ASSERT(exception && !*exception);
        id value = valueToDictionary([context JSGlobalContextRef], argument, exception);
        if (*exception)
            return;
        [invocation setArgument:&value atIndex:argumentNumber];
    }
};

class CallbackArgumentStruct final : public CallbackArgument {
public:
    CallbackArgumentStruct(NSInvocation *conversionInvocation, const char* encodedType)
        : m_conversionInvocation(conversionInvocation)
        , m_buffer(encodedType)
    {
    }
    
private:
    void set(NSInvocation *invocation, NSInteger argumentNumber, JSContext *context, JSValueRef argument, JSValueRef*) final
    {
        JSValue *value = [JSValue valueWithJSValueRef:argument inContext:context];
        [m_conversionInvocation invokeWithTarget:value];
        [m_conversionInvocation getReturnValue:m_buffer];
        [invocation setArgument:m_buffer atIndex:argumentNumber];
    }

    RetainPtr<NSInvocation> m_conversionInvocation;
    StructBuffer m_buffer;
};

class ArgumentTypeDelegate final {
public:
    typedef std::unique_ptr<CallbackArgument> ResultType;

    template<typename T>
    static ResultType typeInteger()
    {
        return makeUnique<CallbackArgumentInteger<T>>();
    }

    template<typename T>
    static ResultType typeDouble()
    {
        return makeUnique<CallbackArgumentDouble<T>>();
    }

    static ResultType typeBool()
    {
        return makeUnique<CallbackArgumentBoolean>();
    }

    static ResultType typeVoid()
    {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    static ResultType typeId()
    {
        return makeUnique<CallbackArgumentId>();
    }

    static ResultType typeOfClass(const char* begin, const char* end)
    {
        StringRange copy(begin, end);
        Class cls = objc_getClass(copy);
        if (!cls)
            return nullptr;

        if (cls == [JSValue class])
            return makeUnique<CallbackArgumentJSValue>();
        if (cls == [NSString class])
            return makeUnique<CallbackArgumentNSString>();
        if (cls == [NSNumber class])
            return makeUnique<CallbackArgumentNSNumber>();
        if (cls == [NSDate class])
            return makeUnique<CallbackArgumentNSDate>();
        if (cls == [NSArray class])
            return makeUnique<CallbackArgumentNSArray>();
        if (cls == [NSDictionary class])
            return makeUnique<CallbackArgumentNSDictionary>();

        return makeUnique<CallbackArgumentOfClass>(cls);
    }

    static ResultType typeBlock(const char*, const char*)
    {
        return nullptr;
    }

    static ResultType typeStruct(const char* begin, const char* end)
    {
        StringRange copy(begin, end);
        if (NSInvocation *invocation = valueToTypeInvocationFor(copy))
            return makeUnique<CallbackArgumentStruct>(invocation, copy);
        return nullptr;
    }
};

class CallbackResult {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~CallbackResult()
    {
    }

    virtual JSValueRef get(NSInvocation *, JSContext *, JSValueRef*) = 0;
};

class CallbackResultVoid final : public CallbackResult {
    JSValueRef get(NSInvocation *, JSContext *context, JSValueRef*) final
    {
        return JSValueMakeUndefined([context JSGlobalContextRef]);
    }
};

class CallbackResultId final : public CallbackResult {
    JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) final
    {
        id value;
        [invocation getReturnValue:&value];
        return objectToValue(context, value);
    }
};

template<typename T>
class CallbackResultNumeric final : public CallbackResult {
    JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) final
    {
        T value;
        [invocation getReturnValue:&value];
        return JSValueMakeNumber([context JSGlobalContextRef], value);
    }
};

class CallbackResultBoolean final : public CallbackResult {
    JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) final
    {
        bool value;
        [invocation getReturnValue:&value];
        return JSValueMakeBoolean([context JSGlobalContextRef], value);
    }
};

class CallbackResultStruct final : public CallbackResult {
public:
    CallbackResultStruct(NSInvocation *conversionInvocation, const char* encodedType)
        : m_conversionInvocation(conversionInvocation)
        , m_buffer(encodedType)
    {
    }
    
private:
    JSValueRef get(NSInvocation *invocation, JSContext *context, JSValueRef*) final
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

class ResultTypeDelegate final {
public:
    typedef std::unique_ptr<CallbackResult> ResultType;

    template<typename T>
    static ResultType typeInteger()
    {
        return makeUnique<CallbackResultNumeric<T>>();
    }

    template<typename T>
    static ResultType typeDouble()
    {
        return makeUnique<CallbackResultNumeric<T>>();
    }

    static ResultType typeBool()
    {
        return makeUnique<CallbackResultBoolean>();
    }

    static ResultType typeVoid()
    {
        return makeUnique<CallbackResultVoid>();
    }

    static ResultType typeId()
    {
        return makeUnique<CallbackResultId>();
    }

    static ResultType typeOfClass(const char*, const char*)
    {
        return makeUnique<CallbackResultId>();
    }

    static ResultType typeBlock(const char*, const char*)
    {
        return makeUnique<CallbackResultId>();
    }

    static ResultType typeStruct(const char* begin, const char* end)
    {
        StringRange copy(begin, end);
        if (NSInvocation *invocation = typeToValueInvocationFor(copy))
            return makeUnique<CallbackResultStruct>(invocation, copy);
        return nullptr;
    }
};

enum CallbackType {
    CallbackInitMethod,
    CallbackInstanceMethod,
    CallbackClassMethod,
    CallbackBlock
};

namespace JSC {

class ObjCCallbackFunctionImpl final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ObjCCallbackFunctionImpl(NSInvocation *invocation, CallbackType type, Class instanceClass, std::unique_ptr<CallbackArgument> arguments, std::unique_ptr<CallbackResult> result)
        : m_type(type)
        , m_instanceClass(instanceClass)
        , m_invocation(invocation)
        , m_arguments(WTFMove(arguments))
        , m_result(WTFMove(result))
    {
        ASSERT((type != CallbackInstanceMethod && type != CallbackInitMethod) || instanceClass);
    }

    void destroy(Heap& heap)
    {
        // We need to explicitly release the target since we didn't call 
        // -retainArguments on m_invocation (and we don't want to do so).
        if (m_type == CallbackBlock || m_type == CallbackClassMethod)
            heap.releaseSoon(adoptNS([m_invocation.get() target]));
        m_instanceClass = nil;
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
            return m_instanceClass.get();
        default:
            return nil;
        }
    }

    CallbackType type() const { return m_type; }

    bool isConstructible()
    {
        return !!wrappedBlock() || m_type == CallbackInitMethod;
    }

    String name();

private:
    CallbackType m_type;
    RetainPtr<Class> m_instanceClass;
    RetainPtr<NSInvocation> m_invocation;
    std::unique_ptr<CallbackArgument> m_arguments;
    std::unique_ptr<CallbackResult> m_result;
};

static JSValueRef objCCallbackFunctionCallAsFunction(JSContextRef callerContext, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT(exception && !*exception);

    // Retake the API lock - we need this for a few reasons:
    // (1) We don't want to support the C-API's confusing drops-locks-once policy - should only drop locks if we can do so recursively.
    // (2) We're calling some JSC internals that require us to be on the 'inside' - e.g. createTypeError.
    // (3) We need to be locked (per context would be fine) against conflicting usage of the ObjCCallbackFunction's NSInvocation.
    JSC::JSLockHolder locker(toJS(callerContext));

    ObjCCallbackFunction* callback = static_cast<ObjCCallbackFunction*>(toJS(function));
    ObjCCallbackFunctionImpl* impl = callback->impl();
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(callback->globalObject())];

    if (impl->type() == CallbackInitMethod) {
        JSGlobalContextRef contextRef = [context JSGlobalContextRef];
        *exception = toRef(JSC::createTypeError(toJS(contextRef), "Cannot call a class constructor without |new|"_s));
        if (*exception)
            return nullptr;
        return JSValueMakeUndefined(contextRef);
    }

    CallbackData callbackData;
    JSValueRef result;
    @autoreleasepool {
        [context beginCallbackWithData:&callbackData calleeValue:function thisValue:thisObject argumentCount:argumentCount arguments:arguments];
        result = impl->call(context, thisObject, argumentCount, arguments, exception);
        if (context.exception)
            *exception = valueInternalValue(context.exception);
        [context endCallbackWithData:&callbackData];
    }
    if (*exception)
        return nullptr;
    return result;
}

static JSObjectRef objCCallbackFunctionCallAsConstructor(JSContextRef callerContext, JSObjectRef constructor, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT(exception && !*exception);
    JSC::JSLockHolder locker(toJS(callerContext));

    ObjCCallbackFunction* callback = static_cast<ObjCCallbackFunction*>(toJS(constructor));
    ObjCCallbackFunctionImpl* impl = callback->impl();
    JSContext *context = [JSContext contextWithJSGlobalContextRef:toGlobalRef(toJS(callerContext))];

    CallbackData callbackData;
    JSValueRef result;
    @autoreleasepool {
        [context beginCallbackWithData:&callbackData calleeValue:constructor thisValue:nullptr argumentCount:argumentCount arguments:arguments];
        result = impl->call(context, nullptr, argumentCount, arguments, exception);
        if (context.exception)
            *exception = valueInternalValue(context.exception);
        [context endCallbackWithData:&callbackData];
    }
    if (*exception)
        return nullptr;

    JSGlobalContextRef contextRef = [context JSGlobalContextRef];
    if (!JSValueIsObject(contextRef, result)) {
        *exception = toRef(JSC::createTypeError(toJS(contextRef), "Objective-C blocks called as constructors must return an object."_s));
        return nullptr;
    }
    ASSERT(!*exception);
    return const_cast<JSObjectRef>(result);
}

const JSC::ClassInfo ObjCCallbackFunction::s_info = { "CallbackFunction", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ObjCCallbackFunction) };

static JSC_DECLARE_HOST_FUNCTION(callObjCCallbackFunction);
static JSC_DECLARE_HOST_FUNCTION(constructObjCCallbackFunction);

JSC_DEFINE_HOST_FUNCTION(callObjCCallbackFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return APICallbackFunction::callImpl<ObjCCallbackFunction>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(constructObjCCallbackFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return APICallbackFunction::constructImpl<ObjCCallbackFunction>(globalObject, callFrame);
}

ObjCCallbackFunction::ObjCCallbackFunction(JSC::VM& vm, JSC::Structure* structure, JSObjectCallAsFunctionCallback functionCallback, JSObjectCallAsConstructorCallback constructCallback, std::unique_ptr<ObjCCallbackFunctionImpl> impl)
    : Base(vm, structure, callObjCCallbackFunction, impl->isConstructible() ? constructObjCCallbackFunction : nullptr)
    , m_functionCallback(functionCallback)
    , m_constructCallback(constructCallback)
    , m_impl(WTFMove(impl))
{
}

ObjCCallbackFunction* ObjCCallbackFunction::create(JSC::VM& vm, JSC::JSGlobalObject* globalObject, const String& name, std::unique_ptr<ObjCCallbackFunctionImpl> impl)
{
    Structure* structure = globalObject->objcCallbackFunctionStructure();
    ObjCCallbackFunction* function = new (NotNull, allocateCell<ObjCCallbackFunction>(vm.heap)) ObjCCallbackFunction(vm, structure, objCCallbackFunctionCallAsFunction, objCCallbackFunctionCallAsConstructor, WTFMove(impl));
    function->finishCreation(vm, 0, name);
    return function;
}

void ObjCCallbackFunction::destroy(JSCell* cell)
{
    ObjCCallbackFunction& function = *static_cast<ObjCCallbackFunction*>(cell);
    function.impl()->destroy(*Heap::heap(cell));
    function.~ObjCCallbackFunction();
}

String ObjCCallbackFunctionImpl::name()
{
    if (m_type == CallbackInitMethod)
        return class_getName(m_instanceClass.get());
    // FIXME: Maybe we could support having the selector as the name of the non-init 
    // functions to make it a bit more user-friendly from the JS side?
    return "";
}

JSValueRef ObjCCallbackFunctionImpl::call(JSContext *context, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT(exception && !*exception);
    JSGlobalContextRef contextRef = [context JSGlobalContextRef];

    id target;
    size_t firstArgument;
    switch (m_type) {
    case CallbackInitMethod: {
        RELEASE_ASSERT(!thisObject);
        target = [m_instanceClass alloc];
        if (!target || ![target isKindOfClass:m_instanceClass.get()]) {
            *exception = toRef(JSC::createTypeError(toJS(contextRef), "self type check failed for Objective-C instance method"_s));
            if (*exception)
                return nullptr;
            return JSValueMakeUndefined(contextRef);
        }
        [m_invocation setTarget:target];
        firstArgument = 2;
        break;
    }
    case CallbackInstanceMethod: {
        target = tryUnwrapObjcObject(contextRef, thisObject);
        if (!target || ![target isKindOfClass:m_instanceClass.get()]) {
            *exception = toRef(JSC::createTypeError(toJS(contextRef), "self type check failed for Objective-C instance method"_s));
            if (*exception)
                return nullptr;
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
            return nullptr;
        ++argumentNumber;
    }

    [m_invocation invoke];

    JSValueRef result = m_result->get(m_invocation.get(), context, exception);
    if (*exception)
        return nullptr;

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
        return _Block_has_signature((__bridge void*)block) && strstr(_Block_signature((__bridge void*)block), "NSString");
    }();
    return containsClass;
}

static inline bool skipNumber(const char*& position)
{
    if (!isASCIIDigit(*position))
        return false;
    while (isASCIIDigit(*++position)) { }
    return true;
}

static JSObjectRef objCCallbackFunctionForInvocation(JSContext *context, NSInvocation *invocation, CallbackType type, Class instanceClass, const char* signatureWithObjcClasses)
{
    if (!signatureWithObjcClasses)
        return nullptr;

    const char* position = signatureWithObjcClasses;

    auto result = parseObjCType<ResultTypeDelegate>(position);
    if (!result || !skipNumber(position))
        return nullptr;

    switch (type) {
    case CallbackInitMethod:
    case CallbackInstanceMethod:
    case CallbackClassMethod:
        // Methods are passed two implicit arguments - (id)self, and the selector.
        if ('@' != *position++ || !skipNumber(position) || ':' != *position++ || !skipNumber(position))
            return nullptr;
        break;
    case CallbackBlock:
        // Blocks are passed one implicit argument - the block, of type "@?".
        if (('@' != *position++) || ('?' != *position++) || !skipNumber(position))
            return nullptr;
        // Only allow arguments of type 'id' if the block signature contains the NS type information.
        if ((!blockSignatureContainsClass() && strchr(position, '@')))
            return nullptr;
        break;
    }

    std::unique_ptr<CallbackArgument> arguments;
    auto* nextArgument = &arguments;
    unsigned argumentCount = 0;
    while (*position) {
        auto argument = parseObjCType<ArgumentTypeDelegate>(position);
        if (!argument || !skipNumber(position))
            return nullptr;

        *nextArgument = WTFMove(argument);
        nextArgument = &(*nextArgument)->m_next;
        ++argumentCount;
    }

    JSC::JSGlobalObject* globalObject = toJS([context JSGlobalContextRef]);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto impl = makeUnique<JSC::ObjCCallbackFunctionImpl>(invocation, type, instanceClass, WTFMove(arguments), WTFMove(result));
    const String& name = impl->name();
    return toRef(JSC::ObjCCallbackFunction::create(vm, globalObject, name, WTFMove(impl)));
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
    if (!isInstanceMethod) {
        [invocation setTarget:cls];
        // We need to retain the target Class because m_invocation doesn't retain it by default (and we don't want it to).
        // FIXME: What releases it?
        CFRetain((__bridge CFTypeRef)cls);
    }
    return objCCallbackFunctionForInvocation(context, invocation, isInstanceMethod ? CallbackInstanceMethod : CallbackClassMethod, isInstanceMethod ? cls : nil, _protocol_getMethodTypeEncoding(protocol, sel, YES, isInstanceMethod));
}

JSObjectRef objCCallbackFunctionForBlock(JSContext *context, id target)
{
    if (!_Block_has_signature((__bridge void*)target))
        return nullptr;
    const char* signature = _Block_signature((__bridge void*)target);
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:signature]];

    // We don't want to use -retainArguments because that leaks memory. Arguments 
    // would be retained indefinitely between invocations of the callback.
    // Additionally, we copy the target because we want the block to stick around
    // until the ObjCCallbackFunctionImpl is destroyed.
    [invocation setTarget:[target copy]];

    return objCCallbackFunctionForInvocation(context, invocation, CallbackBlock, nil, signature);
}

id tryUnwrapConstructor(JSC::VM* vm, JSObjectRef object)
{
    if (!toJS(object)->inherits<JSC::ObjCCallbackFunction>(*vm))
        return nil;
    JSC::ObjCCallbackFunctionImpl* impl = static_cast<JSC::ObjCCallbackFunction*>(toJS(object))->impl();
    if (!impl->isConstructible())
        return nil;
    return impl->wrappedConstructor();
}

#endif
