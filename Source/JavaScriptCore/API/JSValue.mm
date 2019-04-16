/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#import "APICast.h"
#import "DateInstance.h"
#import "Error.h"
#import "Exception.h"
#import "JavaScriptCore.h"
#import "JSContextInternal.h"
#import "JSObjectRefPrivate.h"
#import "JSVirtualMachineInternal.h"
#import "JSValueInternal.h"
#import "JSValuePrivate.h"
#import "JSWrapperMap.h"
#import "ObjcRuntimeExtras.h"
#import "JSCInlines.h"
#import "JSCJSValue.h"
#import "Strong.h"
#import "StrongInlines.h"
#import <wtf/Expected.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/Lock.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>
#import <wtf/text/StringHash.h>

#if ENABLE(REMOTE_INSPECTOR)
#import "CallFrame.h"
#import "JSGlobalObject.h"
#import "JSGlobalObjectInspectorController.h"
#endif

#if JSC_OBJC_API_ENABLED

NSString * const JSPropertyDescriptorWritableKey = @"writable";
NSString * const JSPropertyDescriptorEnumerableKey = @"enumerable";
NSString * const JSPropertyDescriptorConfigurableKey = @"configurable";
NSString * const JSPropertyDescriptorValueKey = @"value";
NSString * const JSPropertyDescriptorGetKey = @"get";
NSString * const JSPropertyDescriptorSetKey = @"set";

@implementation JSValue {
    JSValueRef m_value;
}

- (void)dealloc
{
    JSValueUnprotect([_context JSGlobalContextRef], m_value);
    [_context release];
    _context = nil;
    [super dealloc];
}

- (NSString *)description
{
    if (id wrapped = tryUnwrapObjcObject([_context JSGlobalContextRef], m_value))
        return [wrapped description];
    return [self toString];
}

- (JSValueRef)JSValueRef
{
    return m_value;
}

+ (JSValue *)valueWithObject:(id)value inContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:objectToValue(context, value) inContext:context];
}

+ (JSValue *)valueWithBool:(BOOL)value inContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSValueMakeBoolean([context JSGlobalContextRef], value) inContext:context];
}

+ (JSValue *)valueWithDouble:(double)value inContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSValueMakeNumber([context JSGlobalContextRef], value) inContext:context];
}

+ (JSValue *)valueWithInt32:(int32_t)value inContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSValueMakeNumber([context JSGlobalContextRef], value) inContext:context];
}

+ (JSValue *)valueWithUInt32:(uint32_t)value inContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSValueMakeNumber([context JSGlobalContextRef], value) inContext:context];
}

+ (JSValue *)valueWithNewObjectInContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSObjectMake([context JSGlobalContextRef], 0, 0) inContext:context];
}

+ (JSValue *)valueWithNewArrayInContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSObjectMakeArray([context JSGlobalContextRef], 0, NULL, 0) inContext:context];
}

+ (JSValue *)valueWithNewRegularExpressionFromPattern:(NSString *)pattern flags:(NSString *)flags inContext:(JSContext *)context
{
    auto patternString = OpaqueJSString::tryCreate(pattern);
    auto flagsString = OpaqueJSString::tryCreate(flags);
    JSValueRef arguments[2] = { JSValueMakeString([context JSGlobalContextRef], patternString.get()), JSValueMakeString([context JSGlobalContextRef], flagsString.get()) };
    return [JSValue valueWithJSValueRef:JSObjectMakeRegExp([context JSGlobalContextRef], 2, arguments, 0) inContext:context];
}

+ (JSValue *)valueWithNewErrorFromMessage:(NSString *)message inContext:(JSContext *)context
{
    auto string = OpaqueJSString::tryCreate(message);
    JSValueRef argument = JSValueMakeString([context JSGlobalContextRef], string.get());
    return [JSValue valueWithJSValueRef:JSObjectMakeError([context JSGlobalContextRef], 1, &argument, 0) inContext:context];
}

+ (JSValue *)valueWithNullInContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSValueMakeNull([context JSGlobalContextRef]) inContext:context];
}

+ (JSValue *)valueWithUndefinedInContext:(JSContext *)context
{
    return [JSValue valueWithJSValueRef:JSValueMakeUndefined([context JSGlobalContextRef]) inContext:context];
}

+ (JSValue *)valueWithNewSymbolFromDescription:(NSString *)description inContext:(JSContext *)context
{
    auto string = OpaqueJSString::tryCreate(description);
    return [JSValue valueWithJSValueRef:JSValueMakeSymbol([context JSGlobalContextRef], string.get()) inContext:context];
}

+ (JSValue *)valueWithNewPromiseInContext:(JSContext *)context fromExecutor:(void (^)(JSValue *, JSValue *))executor
{
    JSObjectRef resolve;
    JSObjectRef reject;
    JSValueRef exception = nullptr;
    JSObjectRef promise = JSObjectMakeDeferredPromise([context JSGlobalContextRef], &resolve, &reject, &exception);
    if (exception) {
        [context notifyException:exception];
        return [JSValue valueWithUndefinedInContext:context];
    }

    JSValue *result = [JSValue valueWithJSValueRef:promise inContext:context];
    JSValue *rejection = [JSValue valueWithJSValueRef:reject inContext:context];
    CallbackData callbackData;
    const size_t argumentCount = 2;
    JSValueRef arguments[argumentCount];
    arguments[0] = resolve;
    arguments[1] = reject;

    [context beginCallbackWithData:&callbackData calleeValue:nullptr thisValue:promise argumentCount:argumentCount arguments:arguments];
    executor([JSValue valueWithJSValueRef:resolve inContext:context], rejection);
    if (context.exception)
        [rejection callWithArguments:@[context.exception]];
    [context endCallbackWithData:&callbackData];

    return result;
}

+ (JSValue *)valueWithNewPromiseResolvedWithResult:(id)result inContext:(JSContext *)context
{
    return [JSValue valueWithNewPromiseInContext:context fromExecutor:^(JSValue *resolve, JSValue *) {
        [resolve callWithArguments:@[result]];
    }];
}

+ (JSValue *)valueWithNewPromiseRejectedWithReason:(id)reason inContext:(JSContext *)context
{
    return [JSValue valueWithNewPromiseInContext:context fromExecutor:^(JSValue *, JSValue *reject) {
        [reject callWithArguments:@[reason]];
    }];
}

- (id)toObject
{
    return valueToObject(_context, m_value);
}

- (id)toObjectOfClass:(Class)expectedClass
{
    id result = [self toObject];
    return [result isKindOfClass:expectedClass] ? result : nil;
}

- (BOOL)toBool
{
    return JSValueToBoolean([_context JSGlobalContextRef], m_value);
}

- (double)toDouble
{
    JSValueRef exception = 0;
    double result = JSValueToNumber([_context JSGlobalContextRef], m_value, &exception);
    if (exception) {
        [_context notifyException:exception];
        return std::numeric_limits<double>::quiet_NaN();
    }

    return result;
}

- (int32_t)toInt32
{
    return JSC::toInt32([self toDouble]);
}

- (uint32_t)toUInt32
{
    return JSC::toUInt32([self toDouble]);
}

- (NSNumber *)toNumber
{
    JSValueRef exception = 0;
    id result = valueToNumber([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        [_context notifyException:exception];
    return result;
}

- (NSString *)toString
{
    JSValueRef exception = 0;
    id result = valueToString([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        [_context notifyException:exception];
    return result;
}

- (NSDate *)toDate
{
    JSValueRef exception = 0;
    id result = valueToDate([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        [_context notifyException:exception];
    return result;
}

- (NSArray *)toArray
{
    JSValueRef exception = 0;
    id result = valueToArray([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        [_context notifyException:exception];
    return result;
}

- (NSDictionary *)toDictionary
{
    JSValueRef exception = 0;
    id result = valueToDictionary([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        [_context notifyException:exception];
    return result;
}

template<typename Result, typename NSStringFunction, typename JSValueFunction, typename... Types>
inline Expected<Result, JSValueRef> performPropertyOperation(NSStringFunction stringFunction, JSValueFunction jsFunction, JSValue* value, id propertyKey, Types... arguments)
{
    JSContext* context = [value context];
    JSValueRef exception = nullptr;
    JSObjectRef object = JSValueToObject([context JSGlobalContextRef], [value JSValueRef], &exception);
    if (exception)
        return Unexpected<JSValueRef>(exception);

    Result result;
    // If it's a NSString already, reduce indirection and just pass the NSString.
    if ([propertyKey isKindOfClass:[NSString class]]) {
        auto name = OpaqueJSString::tryCreate((NSString *)propertyKey);
        result = stringFunction([context JSGlobalContextRef], object, name.get(), arguments..., &exception);
    } else
        result = jsFunction([context JSGlobalContextRef], object, [[JSValue valueWithObject:propertyKey inContext:context] JSValueRef], arguments..., &exception);
    return Expected<Result, JSValueRef>(result);
}

- (JSValue *)valueForProperty:(id)key
{
    auto result = performPropertyOperation<JSValueRef>(JSObjectGetProperty, JSObjectGetPropertyForKey, self, key);
    if (!result)
        return [_context valueFromNotifyException:result.error()];

    return [JSValue valueWithJSValueRef:result.value() inContext:_context];
}


- (void)setValue:(id)value forProperty:(JSValueProperty)key
{
    // We need Unit business because void can't be assigned to in performPropertyOperation and I don't want to duplicate the code...
    using Unit = std::tuple<>;
    auto stringSetProperty = [] (auto... args) -> Unit {
        JSObjectSetProperty(args...);
        return { };
    };

    auto jsValueSetProperty = [] (auto... args) -> Unit {
        JSObjectSetPropertyForKey(args...);
        return { };
    };

    auto result = performPropertyOperation<Unit>(stringSetProperty, jsValueSetProperty, self, key, objectToValue(_context, value), kJSPropertyAttributeNone);
    if (!result) {
        [_context notifyException:result.error()];
        return;
    }
}

- (BOOL)deleteProperty:(JSValueProperty)key
{
    Expected<BOOL, JSValueRef> result = performPropertyOperation<BOOL>(JSObjectDeleteProperty, JSObjectDeletePropertyForKey, self, key);
    if (!result)
        return [_context boolFromNotifyException:result.error()];
    return result.value();
}

- (BOOL)hasProperty:(JSValueProperty)key
{
    // The C-api doesn't return an exception value for the string version of has property.
    auto stringHasProperty = [] (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef*) -> BOOL {
        return JSObjectHasProperty(ctx, object, propertyName);
    };

    Expected<BOOL, JSValueRef> result = performPropertyOperation<BOOL>(stringHasProperty, JSObjectHasPropertyForKey, self, key);
    if (!result)
        return [_context boolFromNotifyException:result.error()];
    return result.value();
}

- (void)defineProperty:(JSValueProperty)key descriptor:(id)descriptor
{
    [[_context globalObject][@"Object"] invokeMethod:@"defineProperty" withArguments:@[ self, key, descriptor ]];
}

- (JSValue *)valueAtIndex:(NSUInteger)index
{
    // Properties that are higher than an unsigned value can hold are converted to a double then inserted as a normal property.
    // Indices that are bigger than the max allowed index size (UINT_MAX - 1) will be handled internally in get().
    if (index != (unsigned)index)
        return [self valueForProperty:[[JSValue valueWithDouble:index inContext:_context] toString]];

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    JSValueRef result = JSObjectGetPropertyAtIndex([_context JSGlobalContextRef], object, (unsigned)index, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    return [JSValue valueWithJSValueRef:result inContext:_context];
}

- (void)setValue:(id)value atIndex:(NSUInteger)index
{
    // Properties that are higher than an unsigned value can hold are converted to a double, then inserted as a normal property.
    // Indices that are bigger than the max allowed index size (UINT_MAX - 1) will be handled internally in putByIndex().
    if (index != (unsigned)index)
        return [self setValue:value forProperty:[[JSValue valueWithDouble:index inContext:_context] toString]];

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject([_context JSGlobalContextRef], m_value, &exception);
    if (exception) {
        [_context notifyException:exception];
        return;
    }

    JSObjectSetPropertyAtIndex([_context JSGlobalContextRef], object, (unsigned)index, objectToValue(_context, value), &exception);
    if (exception) {
        [_context notifyException:exception];
        return;
    }
}

- (BOOL)isUndefined
{
    return JSValueIsUndefined([_context JSGlobalContextRef], m_value);
}

- (BOOL)isNull
{
    return JSValueIsNull([_context JSGlobalContextRef], m_value);
}

- (BOOL)isBoolean
{
    return JSValueIsBoolean([_context JSGlobalContextRef], m_value);
}

- (BOOL)isNumber
{
    return JSValueIsNumber([_context JSGlobalContextRef], m_value);
}

- (BOOL)isString
{
    return JSValueIsString([_context JSGlobalContextRef], m_value);
}

- (BOOL)isObject
{
    return JSValueIsObject([_context JSGlobalContextRef], m_value);
}

- (BOOL)isSymbol
{
    return JSValueIsSymbol([_context JSGlobalContextRef], m_value);
}

- (BOOL)isArray
{
    return JSValueIsArray([_context JSGlobalContextRef], m_value);
}

- (BOOL)isDate
{
    return JSValueIsDate([_context JSGlobalContextRef], m_value);
}

- (BOOL)isEqualToObject:(id)value
{
    return JSValueIsStrictEqual([_context JSGlobalContextRef], m_value, objectToValue(_context, value));
}

- (BOOL)isEqualWithTypeCoercionToObject:(id)value
{
    JSValueRef exception = 0;
    BOOL result = JSValueIsEqual([_context JSGlobalContextRef], m_value, objectToValue(_context, value), &exception);
    if (exception)
        return [_context boolFromNotifyException:exception];

    return result;
}

- (BOOL)isInstanceOf:(id)value
{
    JSValueRef exception = 0;
    JSObjectRef constructor = JSValueToObject([_context JSGlobalContextRef], objectToValue(_context, value), &exception);
    if (exception)
        return [_context boolFromNotifyException:exception];

    BOOL result = JSValueIsInstanceOfConstructor([_context JSGlobalContextRef], m_value, constructor, &exception);
    if (exception)
        return [_context boolFromNotifyException:exception];

    return result;
}

- (JSValue *)callWithArguments:(NSArray *)argumentArray
{
    NSUInteger argumentCount = [argumentArray count];
    JSValueRef arguments[argumentCount];
    for (unsigned i = 0; i < argumentCount; ++i)
        arguments[i] = objectToValue(_context, [argumentArray objectAtIndex:i]);

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    JSValueRef result = JSObjectCallAsFunction([_context JSGlobalContextRef], object, 0, argumentCount, arguments, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    return [JSValue valueWithJSValueRef:result inContext:_context];
}

- (JSValue *)constructWithArguments:(NSArray *)argumentArray
{
    NSUInteger argumentCount = [argumentArray count];
    JSValueRef arguments[argumentCount];
    for (unsigned i = 0; i < argumentCount; ++i)
        arguments[i] = objectToValue(_context, [argumentArray objectAtIndex:i]);

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    JSObjectRef result = JSObjectCallAsConstructor([_context JSGlobalContextRef], object, argumentCount, arguments, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    return [JSValue valueWithJSValueRef:result inContext:_context];
}

- (JSValue *)invokeMethod:(NSString *)method withArguments:(NSArray *)arguments
{
    NSUInteger argumentCount = [arguments count];
    JSValueRef argumentArray[argumentCount];
    for (unsigned i = 0; i < argumentCount; ++i)
        argumentArray[i] = objectToValue(_context, [arguments objectAtIndex:i]);

    JSValueRef exception = 0;
    JSObjectRef thisObject = JSValueToObject([_context JSGlobalContextRef], m_value, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    auto name = OpaqueJSString::tryCreate(method);
    JSValueRef function = JSObjectGetProperty([_context JSGlobalContextRef], thisObject, name.get(), &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    JSObjectRef object = JSValueToObject([_context JSGlobalContextRef], function, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    JSValueRef result = JSObjectCallAsFunction([_context JSGlobalContextRef], object, thisObject, argumentCount, argumentArray, &exception);
    if (exception)
        return [_context valueFromNotifyException:exception];

    return [JSValue valueWithJSValueRef:result inContext:_context];
}

@end

@implementation JSValue(StructSupport)

- (CGPoint)toPoint
{
    return (CGPoint){
        static_cast<CGFloat>([self[@"x"] toDouble]),
        static_cast<CGFloat>([self[@"y"] toDouble])
    };
}

- (NSRange)toRange
{
    return (NSRange){
        [[self[@"location"] toNumber] unsignedIntegerValue],
        [[self[@"length"] toNumber] unsignedIntegerValue]
    };
}

- (CGRect)toRect
{
    return (CGRect){
        [self toPoint],
        [self toSize]
    };
}

- (CGSize)toSize
{
    return (CGSize){
        static_cast<CGFloat>([self[@"width"] toDouble]),
        static_cast<CGFloat>([self[@"height"] toDouble])
    };
}

+ (JSValue *)valueWithPoint:(CGPoint)point inContext:(JSContext *)context
{
    return [JSValue valueWithObject:@{
        @"x":@(point.x),
        @"y":@(point.y)
    } inContext:context];
}

+ (JSValue *)valueWithRange:(NSRange)range inContext:(JSContext *)context
{
    return [JSValue valueWithObject:@{
        @"location":@(range.location),
        @"length":@(range.length)
    } inContext:context];
}

+ (JSValue *)valueWithRect:(CGRect)rect inContext:(JSContext *)context
{
    return [JSValue valueWithObject:@{
        @"x":@(rect.origin.x),
        @"y":@(rect.origin.y),
        @"width":@(rect.size.width),
        @"height":@(rect.size.height)
    } inContext:context];
}

+ (JSValue *)valueWithSize:(CGSize)size inContext:(JSContext *)context
{
    return [JSValue valueWithObject:@{
        @"width":@(size.width),
        @"height":@(size.height)
    } inContext:context];
}

@end

@implementation JSValue(SubscriptSupport)

- (JSValue *)objectForKeyedSubscript:(id)key
{
    return [self valueForProperty:key];
}

- (JSValue *)objectAtIndexedSubscript:(NSUInteger)index
{
    return [self valueAtIndex:index];
}

- (void)setObject:(id)object forKeyedSubscript:(id)key
{
    [self setValue:object forProperty:key];
}

- (void)setObject:(id)object atIndexedSubscript:(NSUInteger)index
{
    [self setValue:object atIndex:index];
}

@end

inline bool isDate(JSC::VM& vm, JSObjectRef object, JSGlobalContextRef context)
{
    JSC::JSLockHolder locker(toJS(context));
    return toJS(object)->inherits<JSC::DateInstance>(vm);
}

inline bool isArray(JSC::VM& vm, JSObjectRef object, JSGlobalContextRef context)
{
    JSC::JSLockHolder locker(toJS(context));
    return toJS(object)->inherits<JSC::JSArray>(vm);
}

@implementation JSValue(Internal)

enum ConversionType {
    ContainerNone,
    ContainerArray,
    ContainerDictionary
};

class JSContainerConvertor {
public:
    struct Task {
        JSValueRef js;
        id objc;
        ConversionType type;
    };

    JSContainerConvertor(JSGlobalContextRef context)
        : m_context(context)
    {
    }

    id convert(JSValueRef property);
    void add(Task);
    Task take();
    bool isWorkListEmpty() const { return !m_worklist.size(); }

private:
    JSGlobalContextRef m_context;
    HashMap<JSValueRef, __unsafe_unretained id> m_objectMap;
    Vector<Task> m_worklist;
    Vector<JSC::Strong<JSC::Unknown>> m_jsValues;
};

inline id JSContainerConvertor::convert(JSValueRef value)
{
    auto iter = m_objectMap.find(value);
    if (iter != m_objectMap.end())
        return iter->value;

    Task result = valueToObjectWithoutCopy(m_context, value);
    if (result.js)
        add(result);
    return result.objc;
}

void JSContainerConvertor::add(Task task)
{
    JSC::ExecState* exec = toJS(m_context);
    m_jsValues.append(JSC::Strong<JSC::Unknown>(exec->vm(), toJSForGC(exec, task.js)));
    m_objectMap.add(task.js, task.objc);
    if (task.type != ContainerNone)
        m_worklist.append(task);
}

JSContainerConvertor::Task JSContainerConvertor::take()
{
    ASSERT(!isWorkListEmpty());
    Task last = m_worklist.last();
    m_worklist.removeLast();
    return last;
}

#if ENABLE(REMOTE_INSPECTOR)
static void reportExceptionToInspector(JSGlobalContextRef context, JSC::JSValue exceptionValue)
{
    JSC::ExecState* exec = toJS(context);
    JSC::VM& vm = exec->vm();
    JSC::Exception* exception = JSC::Exception::create(vm, exceptionValue);
    vm.vmEntryGlobalObject(exec)->inspectorController().reportAPIException(exec, exception);
}
#endif

static JSContainerConvertor::Task valueToObjectWithoutCopy(JSGlobalContextRef context, JSValueRef value)
{
    JSC::ExecState* exec = toJS(context);
    JSC::VM& vm = exec->vm();

    if (!JSValueIsObject(context, value)) {
        id primitive;
        if (JSValueIsBoolean(context, value))
            primitive = JSValueToBoolean(context, value) ? @YES : @NO;
        else if (JSValueIsNumber(context, value)) {
            // Normalize the number, so it will unique correctly in the hash map -
            // it's nicer not to leak this internal implementation detail!
            value = JSValueMakeNumber(context, JSValueToNumber(context, value, 0));
            primitive = [NSNumber numberWithDouble:JSValueToNumber(context, value, 0)];
        } else if (JSValueIsString(context, value)) {
            // Would be nice to unique strings, too.
            auto jsstring = adoptRef(JSValueToStringCopy(context, value, 0));
            primitive = CFBridgingRelease(JSStringCopyCFString(kCFAllocatorDefault, jsstring.get()));
        } else if (JSValueIsNull(context, value))
            primitive = [NSNull null];
        else {
            ASSERT(JSValueIsUndefined(context, value));
            primitive = nil;
        }
        return { value, primitive, ContainerNone };
    }

    JSObjectRef object = JSValueToObject(context, value, 0);

    if (id wrapped = tryUnwrapObjcObject(context, object))
        return { object, wrapped, ContainerNone };

    if (isDate(vm, object, context))
        return { object, [NSDate dateWithTimeIntervalSince1970:JSValueToNumber(context, object, 0) / 1000.0], ContainerNone };

    if (isArray(vm, object, context))
        return { object, [NSMutableArray array], ContainerArray };

    return { object, [NSMutableDictionary dictionary], ContainerDictionary };
}

static id containerValueToObject(JSGlobalContextRef context, JSContainerConvertor::Task task)
{
    ASSERT(task.type != ContainerNone);
    JSC::JSLockHolder locker(toJS(context));
    JSContainerConvertor convertor(context);
    convertor.add(task);
    ASSERT(!convertor.isWorkListEmpty());
    
    do {
        JSContainerConvertor::Task current = convertor.take();
        ASSERT(JSValueIsObject(context, current.js));
        JSObjectRef js = JSValueToObject(context, current.js, 0);

        if (current.type == ContainerArray) {
            ASSERT([current.objc isKindOfClass:[NSMutableArray class]]);
            NSMutableArray *array = (NSMutableArray *)current.objc;
        
            auto lengthString = OpaqueJSString::tryCreate("length"_s);
            unsigned length = JSC::toUInt32(JSValueToNumber(context, JSObjectGetProperty(context, js, lengthString.get(), 0), 0));

            for (unsigned i = 0; i < length; ++i) {
                id objc = convertor.convert(JSObjectGetPropertyAtIndex(context, js, i, 0));
                [array addObject:objc ? objc : [NSNull null]];
            }
        } else {
            ASSERT([current.objc isKindOfClass:[NSMutableDictionary class]]);
            NSMutableDictionary *dictionary = (NSMutableDictionary *)current.objc;

            JSC::JSLockHolder locker(toJS(context));

            JSPropertyNameArrayRef propertyNameArray = JSObjectCopyPropertyNames(context, js);
            size_t length = JSPropertyNameArrayGetCount(propertyNameArray);

            for (size_t i = 0; i < length; ++i) {
                JSStringRef propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNameArray, i);
                if (id objc = convertor.convert(JSObjectGetProperty(context, js, propertyName, 0)))
                    dictionary[(__bridge NSString *)adoptCF(JSStringCopyCFString(kCFAllocatorDefault, propertyName)).get()] = objc;
            }

            JSPropertyNameArrayRelease(propertyNameArray);
        }

    } while (!convertor.isWorkListEmpty());

    return task.objc;
}

id valueToObject(JSContext *context, JSValueRef value)
{
    JSContainerConvertor::Task result = valueToObjectWithoutCopy([context JSGlobalContextRef], value);
    if (result.type == ContainerNone)
        return result.objc;
    return containerValueToObject([context JSGlobalContextRef], result);
}

id valueToNumber(JSGlobalContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(!*exception);
    if (id wrapped = tryUnwrapObjcObject(context, value)) {
        if ([wrapped isKindOfClass:[NSNumber class]])
            return wrapped;
    }

    if (JSValueIsBoolean(context, value))
        return JSValueToBoolean(context, value) ? @YES : @NO;

    double result = JSValueToNumber(context, value, exception);
    return [NSNumber numberWithDouble:*exception ? std::numeric_limits<double>::quiet_NaN() : result];
}

id valueToString(JSGlobalContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(!*exception);
    if (id wrapped = tryUnwrapObjcObject(context, value)) {
        if ([wrapped isKindOfClass:[NSString class]])
            return wrapped;
    }

    auto jsstring = adoptRef(JSValueToStringCopy(context, value, exception));
    if (*exception) {
        ASSERT(!jsstring);
        return nil;
    }

    return CFBridgingRelease(JSStringCopyCFString(kCFAllocatorDefault, jsstring.get()));
}

id valueToDate(JSGlobalContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(!*exception);
    if (id wrapped = tryUnwrapObjcObject(context, value)) {
        if ([wrapped isKindOfClass:[NSDate class]])
            return wrapped;
    }

    double result = JSValueToNumber(context, value, exception) / 1000.0;
    return *exception ? nil : [NSDate dateWithTimeIntervalSince1970:result];
}

id valueToArray(JSGlobalContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(!*exception);
    if (id wrapped = tryUnwrapObjcObject(context, value)) {
        if ([wrapped isKindOfClass:[NSArray class]])
            return wrapped;
    }

    if (JSValueIsObject(context, value))
        return containerValueToObject(context, { value, [NSMutableArray array], ContainerArray});

    JSC::JSLockHolder locker(toJS(context));
    if (!(JSValueIsNull(context, value) || JSValueIsUndefined(context, value))) {
        JSC::JSObject* exceptionObject = JSC::createTypeError(toJS(context), "Cannot convert primitive to NSArray"_s);
        *exception = toRef(exceptionObject);
#if ENABLE(REMOTE_INSPECTOR)
        reportExceptionToInspector(context, exceptionObject);
#endif
    }
    return nil;
}

id valueToDictionary(JSGlobalContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(!*exception);
    if (id wrapped = tryUnwrapObjcObject(context, value)) {
        if ([wrapped isKindOfClass:[NSDictionary class]])
            return wrapped;
    }

    if (JSValueIsObject(context, value))
        return containerValueToObject(context, { value, [NSMutableDictionary dictionary], ContainerDictionary});

    JSC::JSLockHolder locker(toJS(context));
    if (!(JSValueIsNull(context, value) || JSValueIsUndefined(context, value))) {
        JSC::JSObject* exceptionObject = JSC::createTypeError(toJS(context), "Cannot convert primitive to NSDictionary"_s);
        *exception = toRef(exceptionObject);
#if ENABLE(REMOTE_INSPECTOR)
        reportExceptionToInspector(context, exceptionObject);
#endif
    }
    return nil;
}

class ObjcContainerConvertor {
public:
    struct Task {
        id objc;
        JSValueRef js;
        ConversionType type;
    };

    ObjcContainerConvertor(JSContext *context)
        : m_context(context)
    {
    }

    JSValueRef convert(id object);
    void add(Task);
    Task take();
    bool isWorkListEmpty() const { return !m_worklist.size(); }

private:
    JSContext *m_context;
    HashMap<__unsafe_unretained id, JSValueRef> m_objectMap;
    Vector<Task> m_worklist;
    Vector<JSC::Strong<JSC::Unknown>> m_jsValues;
};

JSValueRef ObjcContainerConvertor::convert(id object)
{
    ASSERT(object);

    auto it = m_objectMap.find(object);
    if (it != m_objectMap.end())
        return it->value;

    ObjcContainerConvertor::Task task = objectToValueWithoutCopy(m_context, object);
    add(task);
    return task.js;
}

void ObjcContainerConvertor::add(ObjcContainerConvertor::Task task)
{
    JSC::ExecState* exec = toJS(m_context.JSGlobalContextRef);
    m_jsValues.append(JSC::Strong<JSC::Unknown>(exec->vm(), toJSForGC(exec, task.js)));
    m_objectMap.add(task.objc, task.js);
    if (task.type != ContainerNone)
        m_worklist.append(task);
}

ObjcContainerConvertor::Task ObjcContainerConvertor::take()
{
    ASSERT(!isWorkListEmpty());
    Task last = m_worklist.last();
    m_worklist.removeLast();
    return last;
}

inline bool isNSBoolean(id object)
{
    ASSERT([@YES class] == [@NO class]);
    ASSERT([@YES class] != [NSNumber class]);
    ASSERT([[@YES class] isSubclassOfClass:[NSNumber class]]);
    return [object isKindOfClass:[@YES class]];
}

static ObjcContainerConvertor::Task objectToValueWithoutCopy(JSContext *context, id object)
{
    JSGlobalContextRef contextRef = [context JSGlobalContextRef];

    if (!object)
        return { object, JSValueMakeUndefined(contextRef), ContainerNone };

    if (!class_conformsToProtocol(object_getClass(object), getJSExportProtocol())) {
        if ([object isKindOfClass:[NSArray class]])
            return { object, JSObjectMakeArray(contextRef, 0, NULL, 0), ContainerArray };

        if ([object isKindOfClass:[NSDictionary class]])
            return { object, JSObjectMake(contextRef, 0, 0), ContainerDictionary };

        if ([object isKindOfClass:[NSNull class]])
            return { object, JSValueMakeNull(contextRef), ContainerNone };

        if ([object isKindOfClass:[JSValue class]])
            return { object, ((JSValue *)object)->m_value, ContainerNone };

        if ([object isKindOfClass:[NSString class]]) {
            auto string = OpaqueJSString::tryCreate((NSString *)object);
            return { object, JSValueMakeString(contextRef, string.get()), ContainerNone };
        }

        if ([object isKindOfClass:[NSNumber class]]) {
            if (isNSBoolean(object))
                return { object, JSValueMakeBoolean(contextRef, [object boolValue]), ContainerNone };
            return { object, JSValueMakeNumber(contextRef, [object doubleValue]), ContainerNone };
        }

        if ([object isKindOfClass:[NSDate class]]) {
            JSValueRef argument = JSValueMakeNumber(contextRef, [object timeIntervalSince1970] * 1000.0);
            JSObjectRef result = JSObjectMakeDate(contextRef, 1, &argument, 0);
            return { object, result, ContainerNone };
        }

        if ([object isKindOfClass:[JSManagedValue class]]) {
            JSValue *value = [static_cast<JSManagedValue *>(object) value];
            if (!value)
                return  { object, JSValueMakeUndefined(contextRef), ContainerNone };
            return { object, value->m_value, ContainerNone };
        }
    }

    return { object, valueInternalValue([context wrapperForObjCObject:object]), ContainerNone };
}

JSValueRef objectToValue(JSContext *context, id object)
{
    JSGlobalContextRef contextRef = [context JSGlobalContextRef];

    ObjcContainerConvertor::Task task = objectToValueWithoutCopy(context, object);
    if (task.type == ContainerNone)
        return task.js;

    JSC::JSLockHolder locker(toJS(contextRef));
    ObjcContainerConvertor convertor(context);
    convertor.add(task);
    ASSERT(!convertor.isWorkListEmpty());

    do {
        ObjcContainerConvertor::Task current = convertor.take();
        ASSERT(JSValueIsObject(contextRef, current.js));
        JSObjectRef js = JSValueToObject(contextRef, current.js, 0);

        if (current.type == ContainerArray) {
            ASSERT([current.objc isKindOfClass:[NSArray class]]);
            NSArray *array = (NSArray *)current.objc;
            NSUInteger count = [array count];
            for (NSUInteger index = 0; index < count; ++index)
                JSObjectSetPropertyAtIndex(contextRef, js, index, convertor.convert([array objectAtIndex:index]), 0);
        } else {
            ASSERT(current.type == ContainerDictionary);
            ASSERT([current.objc isKindOfClass:[NSDictionary class]]);
            NSDictionary *dictionary = (NSDictionary *)current.objc;
            for (id key in [dictionary keyEnumerator]) {
                if ([key isKindOfClass:[NSString class]]) {
                    auto propertyName = OpaqueJSString::tryCreate((NSString *)key);
                    JSObjectSetProperty(contextRef, js, propertyName.get(), convertor.convert([dictionary objectForKey:key]), 0, 0);
                }
            }
        }
    } while (!convertor.isWorkListEmpty());

    return task.js;
}

JSValueRef valueInternalValue(JSValue * value)
{
    return value->m_value;
}

+ (JSValue *)valueWithJSValueRef:(JSValueRef)value inContext:(JSContext *)context
{
    return [context wrapperForJSObject:value];
}

- (JSValue *)init
{
    return nil;
}

- (JSValue *)initWithValue:(JSValueRef)value inContext:(JSContext *)context
{
    if (!value || !context)
        return nil;

    self = [super init];
    if (!self)
        return nil;

    _context = [context retain];
    m_value = value;
    JSValueProtect([_context JSGlobalContextRef], m_value);
    return self;
}

struct StructTagHandler {
    SEL typeToValueSEL;
    SEL valueToTypeSEL;
};
typedef HashMap<String, StructTagHandler> StructHandlers;

static StructHandlers* createStructHandlerMap()
{
    StructHandlers* structHandlers = new StructHandlers();

    size_t valueWithXinContextLength = strlen("valueWithX:inContext:");
    size_t toXLength = strlen("toX");

    // Step 1: find all valueWith<Foo>:inContext: class methods in JSValue.
    forEachMethodInClass(object_getClass([JSValue class]), ^(Method method){
        SEL selector = method_getName(method);
        const char* name = sel_getName(selector);
        size_t nameLength = strlen(name);
        // Check for valueWith<Foo>:context:
        if (nameLength < valueWithXinContextLength || memcmp(name, "valueWith", 9) || memcmp(name + nameLength - 11, ":inContext:", 11))
            return;
        // Check for [ id, SEL, <type>, <contextType> ]
        if (method_getNumberOfArguments(method) != 4)
            return;
        char idType[3];
        // Check 2nd argument type is "@"
        {
            auto secondType = adoptSystem<char[]>(method_copyArgumentType(method, 3));
            if (strcmp(secondType.get(), "@") != 0)
                return;
        }
        // Check result type is also "@"
        method_getReturnType(method, idType, 3);
        if (strcmp(idType, "@") != 0)
            return;
        {
            auto type = adoptSystem<char[]>(method_copyArgumentType(method, 2));
            structHandlers->add(StringImpl::create(type.get()), (StructTagHandler) { selector, 0 });
        }
    });

    // Step 2: find all to<Foo> instance methods in JSValue.
    forEachMethodInClass([JSValue class], ^(Method method){
        SEL selector = method_getName(method);
        const char* name = sel_getName(selector);
        size_t nameLength = strlen(name);
        // Check for to<Foo>
        if (nameLength < toXLength || memcmp(name, "to", 2))
            return;
        // Check for [ id, SEL ]
        if (method_getNumberOfArguments(method) != 2)
            return;
        // Try to find a matching valueWith<Foo>:context: method.
        auto type = adoptSystem<char[]>(method_copyReturnType(method));
        StructHandlers::iterator iter = structHandlers->find(type.get());
        if (iter == structHandlers->end())
            return;
        StructTagHandler& handler = iter->value;

        // check that strlen(<foo>) == strlen(<Foo>)
        const char* valueWithName = sel_getName(handler.typeToValueSEL);
        size_t valueWithLength = strlen(valueWithName);
        if (valueWithLength - valueWithXinContextLength != nameLength - toXLength)
            return;
        // Check that <Foo> == <Foo>
        if (memcmp(valueWithName + 9, name + 2, nameLength - toXLength - 1))
            return;
        handler.valueToTypeSEL = selector;
    });

    // Step 3: clean up - remove entries where we found prospective valueWith<Foo>:inContext: conversions, but no matching to<Foo> methods.
    typedef HashSet<String> RemoveSet;
    RemoveSet removeSet;
    for (StructHandlers::iterator iter = structHandlers->begin(); iter != structHandlers->end(); ++iter) {
        StructTagHandler& handler = iter->value;
        if (!handler.valueToTypeSEL)
            removeSet.add(iter->key);
    }

    for (RemoveSet::iterator iter = removeSet.begin(); iter != removeSet.end(); ++iter)
        structHandlers->remove(*iter);

    return structHandlers;
}

static StructTagHandler* handerForStructTag(const char* encodedType)
{
    static Lock handerForStructTagLock;
    LockHolder lockHolder(&handerForStructTagLock);

    static StructHandlers* structHandlers = createStructHandlerMap();

    StructHandlers::iterator iter = structHandlers->find(encodedType);
    if (iter == structHandlers->end())
        return 0;
    return &iter->value;
}

+ (SEL)selectorForStructToValue:(const char *)structTag
{
    StructTagHandler* handler = handerForStructTag(structTag);
    return handler ? handler->typeToValueSEL : nil;
}

+ (SEL)selectorForValueToStruct:(const char *)structTag
{
    StructTagHandler* handler = handerForStructTag(structTag);
    return handler ? handler->valueToTypeSEL : nil;
}

NSInvocation *typeToValueInvocationFor(const char* encodedType)
{
    SEL selector = [JSValue selectorForStructToValue:encodedType];
    if (!selector)
        return 0;

    const char* methodTypes = method_getTypeEncoding(class_getClassMethod([JSValue class], selector));
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:methodTypes]];
    [invocation setSelector:selector];
    return invocation;
}

NSInvocation *valueToTypeInvocationFor(const char* encodedType)
{
    SEL selector = [JSValue selectorForValueToStruct:encodedType];
    if (!selector)
        return 0;

    const char* methodTypes = method_getTypeEncoding(class_getInstanceMethod([JSValue class], selector));
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:methodTypes]];
    [invocation setSelector:selector];
    return invocation;
}

@end

#endif
