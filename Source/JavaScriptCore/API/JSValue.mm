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
#import "APICast.h"
#import "APIShims.h"
#import "DateInstance.h"
#import "Error.h"
#import "JavaScriptCore.h"
#import "JSContextInternal.h"
#import "JSVirtualMachineInternal.h"
#import "JSValueInternal.h"
#import "JSWrapperMap.h"
#import "ObjcRuntimeExtras.h"
#import "JSValue.h"
#import "wtf/HashMap.h"
#import "wtf/HashSet.h"
#import "wtf/Vector.h"
#import <wtf/TCSpinLock.h>
#import "wtf/text/WTFString.h"
#import <wtf/text/StringHash.h>

#if JS_OBJC_API_ENABLED

NSString * const JSPropertyDescriptorWritableKey = @"writable";
NSString * const JSPropertyDescriptorEnumerableKey = @"enumerable";
NSString * const JSPropertyDescriptorConfigurableKey = @"configurable";
NSString * const JSPropertyDescriptorValueKey = @"value";
NSString * const JSPropertyDescriptorGetKey = @"get";
NSString * const JSPropertyDescriptorSetKey = @"set";

@implementation JSValue {
    JSValueRef m_value;
    JSContext *m_weakContext;
}

+ (JSValue *)valueWithObject:(id)value inContext:(JSContext *)context
{
    return [JSValue valueWithValue:objectToValue(context, value) inContext:context];
}

+ (JSValue *)valueWithBool:(BOOL)value inContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSValueMakeBoolean(contextInternalContext(context), value) inContext:context];
}

+ (JSValue *)valueWithDouble:(double)value inContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSValueMakeNumber(contextInternalContext(context), value) inContext:context];
}

+ (JSValue *)valueWithInt32:(int32_t)value inContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSValueMakeNumber(contextInternalContext(context), value) inContext:context];
}

+ (JSValue *)valueWithUInt32:(uint32_t)value inContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSValueMakeNumber(contextInternalContext(context), value) inContext:context];
}

+ (JSValue *)valueWithNewObjectInContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSObjectMake(contextInternalContext(context), 0, 0) inContext:context];
}

+ (JSValue *)valueWithNewArrayInContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSObjectMakeArray(contextInternalContext(context), 0, NULL, 0) inContext:context];
}

+ (JSValue *)valueWithNewRegularExpressionFromPattern:(NSString *)pattern flags:(NSString *)flags inContext:(JSContext *)context
{
    JSStringRef patternString = JSStringCreateWithCFString((CFStringRef)pattern);
    JSStringRef flagsString = JSStringCreateWithCFString((CFStringRef)flags);
    JSValueRef arguments[2] = { JSValueMakeString(contextInternalContext(context), patternString), JSValueMakeString(contextInternalContext(context), flagsString) };
    JSStringRelease(patternString);
    JSStringRelease(flagsString);

    return [JSValue valueWithValue:JSObjectMakeRegExp(contextInternalContext(context), 2, arguments, 0) inContext:context];
}

+ (JSValue *)valueWithNewErrorFromMessage:(NSString *)message inContext:(JSContext *)context
{
    JSStringRef string = JSStringCreateWithCFString((CFStringRef)message);
    JSValueRef argument = JSValueMakeString(contextInternalContext(context), string);
    JSStringRelease(string);

    return [JSValue valueWithValue:JSObjectMakeError(contextInternalContext(context), 1, &argument, 0) inContext:context];
}

+ (JSValue *)valueWithNullInContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSValueMakeNull(contextInternalContext(context)) inContext:context];
}

+ (JSValue *)valueWithUndefinedInContext:(JSContext *)context
{
    return [JSValue valueWithValue:JSValueMakeUndefined(contextInternalContext(context)) inContext:context];
}

- (id)toObject
{
    JSContext *context = [self context];
    if (!context)
        return nil;
    return valueToObject(context, m_value);
}

- (id)toObjectOfClass:(Class)expectedClass
{
    id result = [self toObject];
    return [result isKindOfClass:expectedClass] ? result : nil;
}

- (BOOL)toBool
{
    JSContext *context = [self context];
    return (context && JSValueToBoolean(contextInternalContext(context), m_value));
}

- (double)toDouble
{
    JSContext *context = [self context];
    if (!context)
        return std::numeric_limits<double>::quiet_NaN();

    JSValueRef exception = 0;
    double result = JSValueToNumber(contextInternalContext(context), m_value, &exception);
    if (exception) {
        [context notifyException:exception];
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
    JSContext *context = [self context];
    if (!context)
        return nil;

    JSValueRef exception = 0;
    id result = valueToNumber(contextInternalContext(context), m_value, &exception);
    if (exception)
        [context notifyException:exception];
    return result;
}

- (NSString *)toString
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    JSValueRef exception = 0;
    id result = valueToString(contextInternalContext(context), m_value, &exception);
    if (exception)
        [context notifyException:exception];
    return result;
}

- (NSDate *)toDate
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    JSValueRef exception = 0;
    id result = valueToDate(contextInternalContext(context), m_value, &exception);
    if (exception)
        [context notifyException:exception];
    return result;
}

- (NSArray *)toArray
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    JSValueRef exception = 0;
    id result = valueToArray(contextInternalContext(context), m_value, &exception);
    if (exception)
        [context notifyException:exception];
    return result;
}

- (NSDictionary *)toDictionary
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    JSValueRef exception = 0;
    id result = valueToDictionary(contextInternalContext(context), m_value, &exception);
    if (exception)
        [context notifyException:exception];
    return result;
}

- (JSValue *)valueForProperty:(NSString *)propertyName
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    JSStringRef name = JSStringCreateWithCFString((CFStringRef)propertyName);
    JSValueRef result = JSObjectGetProperty(contextInternalContext(context), object, name, &exception);
    JSStringRelease(name);
    if (exception)
        return [context valueFromNotifyException:exception];

    return [JSValue valueWithValue:result inContext:context];
}

- (void)setValue:(id)value forProperty:(NSString *)propertyName
{
    JSContext *context = [self context];
    if (!context)
        return;

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception) {
        [context notifyException:exception];
        return;
    }

    JSStringRef name = JSStringCreateWithCFString((CFStringRef)propertyName);
    JSObjectSetProperty(contextInternalContext(context), object, name, objectToValue(context, value), 0, &exception);
    JSStringRelease(name);
    if (exception) {
        [context notifyException:exception];
        return;
    }
}

- (BOOL)deleteProperty:(NSString *)propertyName
{
    JSContext *context = [self context];
    if (!context)
        return NO;

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception)
        return [context boolFromNotifyException:exception];

    JSStringRef name = JSStringCreateWithCFString((CFStringRef)propertyName);
    BOOL result = JSObjectDeleteProperty(contextInternalContext(context), object, name, &exception);
    JSStringRelease(name);
    if (exception)
        return [context boolFromNotifyException:exception];

    return result;
}

- (BOOL)hasProperty:(NSString *)propertyName
{
    JSContext *context = [self context];
    if (!context)
        return NO;

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception)
        return [context boolFromNotifyException:exception];

    JSStringRef name = JSStringCreateWithCFString((CFStringRef)propertyName);
    BOOL result = JSObjectHasProperty(contextInternalContext(context), object, name);
    JSStringRelease(name);
    return result;
}

- (void)defineProperty:(NSString *)property descriptor:(id)descriptor
{
    JSContext *context = [self context];
    if (!context)
        return;
    [[context globalObject][@"Object"] invokeMethod:@"defineProperty" withArguments:@[ self, property, descriptor ]];
}

- (JSValue *)valueAtIndex:(NSUInteger)index
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    if (index != (unsigned)index)
        [self valueForProperty:[[JSValue valueWithDouble:index inContext:context] toString]];

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    JSValueRef result = JSObjectGetPropertyAtIndex(contextInternalContext(context), object, (unsigned)index, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    return [JSValue valueWithValue:result inContext:context];
}

- (void)setValue:(id)value atIndex:(NSUInteger)index
{
    JSContext *context = [self context];
    if (!context)
        return;

    if (index != (unsigned)index)
        return [self setValue:value forProperty:[[JSValue valueWithDouble:index inContext:context] toString]];

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception) {
        [context notifyException:exception];
        return;
    }

    JSObjectSetPropertyAtIndex(contextInternalContext(context), object, (unsigned)index, objectToValue(context, value), &exception);
    if (exception) {
        [context notifyException:exception];
        return;
    }
}

- (BOOL)isUndefined
{
    JSContext *context = [self context];
    return (context && JSValueIsUndefined(contextInternalContext(context), m_value));
}

- (BOOL)isNull
{
    JSContext *context = [self context];
    return (context && JSValueIsNull(contextInternalContext(context), m_value));
}

- (BOOL)isBoolean
{
    JSContext *context = [self context];
    return (context && JSValueIsBoolean(contextInternalContext(context), m_value));
}

- (BOOL)isNumber
{
    JSContext *context = [self context];
    return (context && JSValueIsNumber(contextInternalContext(context), m_value));
}

- (BOOL)isString
{
    JSContext *context = [self context];
    return (context && JSValueIsString(contextInternalContext(context), m_value));
}

- (BOOL)isObject
{
    JSContext *context = [self context];
    return (context && JSValueIsObject(contextInternalContext(context), m_value));
}

- (BOOL)isEqualToObject:(id)value
{
    JSContext *context = [self context];
    if (!context)
        return NO;

    return JSValueIsStrictEqual(contextInternalContext(context), m_value, objectToValue(context, value));
}

- (BOOL)isEqualWithTypeCoercionToObject:(id)value
{
    JSContext *context = [self context];
    if (!context)
        return NO;

    JSValueRef exception = 0;
    BOOL result = JSValueIsEqual(contextInternalContext(context), m_value, objectToValue(context, value), &exception);
    if (exception)
        return [context boolFromNotifyException:exception];

    return result;
}

- (BOOL)isInstanceOf:(id)value
{
    JSContext *context = [self context];
    if (!context)
        return NO;

    JSValueRef exception = 0;
    JSObjectRef constructor = JSValueToObject(contextInternalContext(context), objectToValue(context, value), &exception);
    if (exception)
        return [context boolFromNotifyException:exception];

    BOOL result = JSValueIsInstanceOfConstructor(contextInternalContext(context), m_value, constructor, &exception);
    if (exception)
        return [context boolFromNotifyException:exception];

    return result;
}

- (JSValue *)callWithArguments:(NSArray *)argumentArray
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    NSUInteger argumentCount = [argumentArray count];
    JSValueRef arguments[argumentCount];
    for (unsigned i = 0; i < argumentCount; ++i)
        arguments[i] = objectToValue(context, [argumentArray objectAtIndex:i]);

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    JSValueRef result = JSObjectCallAsFunction(contextInternalContext(context), object, 0, argumentCount, arguments, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    return [JSValue valueWithValue:result inContext:context];
}

- (JSValue *)constructWithArguments:(NSArray *)argumentArray
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    NSUInteger argumentCount = [argumentArray count];
    JSValueRef arguments[argumentCount];
    for (unsigned i = 0; i < argumentCount; ++i)
        arguments[i] = objectToValue(context, [argumentArray objectAtIndex:i]);

    JSValueRef exception = 0;
    JSObjectRef object = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    JSObjectRef result = JSObjectCallAsConstructor(contextInternalContext(context), object, argumentCount, arguments, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    return [JSValue valueWithValue:result inContext:context];
}

- (JSValue *)invokeMethod:(NSString *)method withArguments:(NSArray *)arguments
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    NSUInteger argumentCount = [arguments count];
    JSValueRef argumentArray[argumentCount];
    for (unsigned i = 0; i < argumentCount; ++i)
        argumentArray[i] = objectToValue(context, [arguments objectAtIndex:i]);

    JSValueRef exception = 0;
    JSObjectRef thisObject = JSValueToObject(contextInternalContext(context), m_value, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    JSStringRef name = JSStringCreateWithCFString((CFStringRef)method);
    JSValueRef function = JSObjectGetProperty(contextInternalContext(context), thisObject, name, &exception);
    JSStringRelease(name);
    if (exception)
        return [context valueFromNotifyException:exception];

    JSObjectRef object = JSValueToObject(contextInternalContext(context), function, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    JSValueRef result = JSObjectCallAsFunction(contextInternalContext(context), object, thisObject, argumentCount, argumentArray, &exception);
    if (exception)
        return [context valueFromNotifyException:exception];

    return [JSValue valueWithValue:result inContext:context];
}

- (JSContext *)context
{
    return objc_loadWeak(&m_weakContext);
}

@end

@implementation JSValue(StructSupport)

- (CGPoint)toPoint
{
    return (CGPoint){
        [self[@"x"] toDouble],
        [self[@"y"] toDouble]
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
        [self[@"width"] toDouble],
        [self[@"height"] toDouble]
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
    JSContext *context = [self context];
    if (!context)
        return nil;

    if (![key isKindOfClass:[NSString class]]) {
        key = [[JSValue valueWithObject:key inContext:context] toString];
        if (!key)
            return [JSValue valueWithUndefinedInContext:context];
    }

    return [self valueForProperty:(NSString *)key];
}

- (JSValue *)objectAtIndexedSubscript:(NSUInteger)index
{
    return [self valueAtIndex:index];
}

- (void)setObject:(id)object forKeyedSubscript:(NSObject <NSCopying> *)key
{
    JSContext *context = [self context];
    if (!context)
        return;

    if (![key isKindOfClass:[NSString class]]) {
        key = [[JSValue valueWithObject:key inContext:context] toString];
        if (!key)
            return;
    }

    [self setValue:object forProperty:(NSString *)key];
}

- (void)setObject:(id)object atIndexedSubscript:(NSUInteger)index
{
    [self setValue:object atIndex:index];
}

@end

inline bool isDate(JSObjectRef object, JSGlobalContextRef context)
{
    JSC::APIEntryShim entryShim(toJS(context));
    return toJS(object)->inherits(&JSC::DateInstance::s_info);
}

inline bool isArray(JSObjectRef object, JSGlobalContextRef context)
{
    JSC::APIEntryShim entryShim(toJS(context));
    return toJS(object)->inherits(&JSC::JSArray::s_info);
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
    HashMap<JSValueRef, id> m_objectMap;
    Vector<Task> m_worklist;
};

inline id JSContainerConvertor::convert(JSValueRef value)
{
    HashMap<JSValueRef, id>::iterator iter = m_objectMap.find(value);
    if (iter != m_objectMap.end())
        return iter->value;

    Task result = valueToObjectWithoutCopy(m_context, value);
    if (result.js)
        add(result);
    return result.objc;
}

void JSContainerConvertor::add(Task task)
{
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

static JSContainerConvertor::Task valueToObjectWithoutCopy(JSGlobalContextRef context, JSValueRef value)
{
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
            JSStringRef jsstring = JSValueToStringCopy(context, value, 0);
            NSString * stringNS = (NSString *)JSStringCopyCFString(kCFAllocatorDefault, jsstring);
            JSStringRelease(jsstring);
            primitive = [stringNS autorelease];
        } else if (JSValueIsNull(context, value))
            primitive = [NSNull null];
        else {
            ASSERT(JSValueIsUndefined(context, value));
            primitive = nil;
        }
        return (JSContainerConvertor::Task){ value, primitive, ContainerNone };
    }

    JSObjectRef object = JSValueToObject(context, value, 0);

    if (id wrapped = tryUnwrapObjcObject(context, object))
        return (JSContainerConvertor::Task){ object, wrapped, ContainerNone };

    if (isDate(object, context))
        return (JSContainerConvertor::Task){ object, [NSDate dateWithTimeIntervalSince1970:JSValueToNumber(context, object, 0)], ContainerNone };

    if (isArray(object, context))
        return (JSContainerConvertor::Task){ object, [NSMutableArray array], ContainerArray };

    return (JSContainerConvertor::Task){ object, [NSMutableDictionary dictionary], ContainerDictionary };
}

static id containerValueToObject(JSGlobalContextRef context, JSContainerConvertor::Task task)
{
    ASSERT(task.type != ContainerNone);
    JSContainerConvertor convertor(context);
    convertor.add(task);
    ASSERT(!convertor.isWorkListEmpty());
    
    do {
        JSContainerConvertor::Task current = task = convertor.take();
        ASSERT(JSValueIsObject(context, current.js));
        JSObjectRef js = JSValueToObject(context, current.js, 0);

        if (current.type == ContainerArray) {
            ASSERT([current.objc isKindOfClass:[NSMutableArray class]]);
            NSMutableArray *array = (NSMutableArray *)current.objc;
        
            JSStringRef lengthString = JSStringCreateWithUTF8CString("length");
            unsigned length = JSC::toUInt32(JSValueToNumber(context, JSObjectGetProperty(context, js, lengthString, 0), 0));
            JSStringRelease(lengthString);

            for (unsigned i = 0; i < length; ++i) {
                id objc = convertor.convert(JSObjectGetPropertyAtIndex(context, js, i, 0));
                [array addObject:objc ? objc : [NSNull null]];
            }
        } else {
            ASSERT([current.objc isKindOfClass:[NSMutableDictionary class]]);
            NSMutableDictionary *dictionary = (NSMutableDictionary *)current.objc;

            JSPropertyNameArrayRef propertyNameArray = JSObjectCopyPropertyNames(context, js);
            size_t length = JSPropertyNameArrayGetCount(propertyNameArray);

            for (size_t i = 0; i < length; ++i) {
                JSStringRef propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNameArray, i);
                if (id objc = convertor.convert(JSObjectGetProperty(context, js, propertyName, 0)))
                    dictionary[[(NSString *)JSStringCopyCFString(kCFAllocatorDefault, propertyName) autorelease]] = objc;
            }

            JSPropertyNameArrayRelease(propertyNameArray);
        }

    } while (!convertor.isWorkListEmpty());

    return task.objc;
}

id valueToObject(JSContext *context, JSValueRef value)
{
    JSContainerConvertor::Task result = valueToObjectWithoutCopy(contextInternalContext(context), value);
    if (result.type == ContainerNone)
        return result.objc;
    return containerValueToObject(contextInternalContext(context), result);
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

    JSStringRef jsstring = JSValueToStringCopy(context, value, exception);
    if (*exception) {
        ASSERT(!jsstring);
        return nil;
    }

    NSString* stringNS = [(NSString *)JSStringCopyCFString(kCFAllocatorDefault, jsstring) autorelease];
    JSStringRelease(jsstring);
    return stringNS;
}

id valueToDate(JSGlobalContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(!*exception);
    if (id wrapped = tryUnwrapObjcObject(context, value)) {
        if ([wrapped isKindOfClass:[NSDate class]])
            return wrapped;
    }

    double result = JSValueToNumber(context, value, exception);
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
        return containerValueToObject(context, (JSContainerConvertor::Task){ value, [NSMutableArray array], ContainerArray});

    if (!(JSValueIsNull(context, value) || JSValueIsUndefined(context, value)))
        *exception = toRef(JSC::createTypeError(toJS(context), "Cannot convert primitive to NSArray"));
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
        return containerValueToObject(context, (JSContainerConvertor::Task){ value, [NSMutableDictionary dictionary], ContainerDictionary});

    if (!(JSValueIsNull(context, value) || JSValueIsUndefined(context, value)))
        *exception = toRef(JSC::createTypeError(toJS(context), "Cannot convert primitive to NSDictionary"));
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
    HashMap<id, JSValueRef> m_objectMap;
    Vector<Task> m_worklist;
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

static ObjcContainerConvertor::Task objectToValueWithoutCopy(JSContext *context, id object)
{
    if (!object)
        return (ObjcContainerConvertor::Task){ object, JSValueMakeUndefined(contextInternalContext(context)), ContainerNone };

    if (!class_conformsToProtocol(object_getClass(object), getJSExportProtocol())) {
        if ([object isKindOfClass:[NSArray class]])
            return (ObjcContainerConvertor::Task){ object, JSObjectMakeArray(contextInternalContext(context), 0, NULL, 0), ContainerArray };

        if ([object isKindOfClass:[NSDictionary class]])
            return (ObjcContainerConvertor::Task){ object, JSObjectMake(contextInternalContext(context), 0, 0), ContainerDictionary };

        if ([object isKindOfClass:[NSNull class]])
            return (ObjcContainerConvertor::Task){ object, JSValueMakeNull(contextInternalContext(context)), ContainerNone };

        if ([object isKindOfClass:[JSValue class]])
            return (ObjcContainerConvertor::Task){ object, ((JSValue *)object)->m_value, ContainerNone };

        if ([object isKindOfClass:[NSArray class]])
            return (ObjcContainerConvertor::Task){ object, JSObjectMakeArray(contextInternalContext(context), 0, NULL, 0), ContainerArray };

        if ([object isKindOfClass:[NSDictionary class]])
            return (ObjcContainerConvertor::Task){ object, JSObjectMake(contextInternalContext(context), 0, 0), ContainerDictionary };

        if ([object isKindOfClass:[NSString class]]) {
            JSStringRef string = JSStringCreateWithCFString((CFStringRef)object);
            JSValueRef js = JSValueMakeString(contextInternalContext(context), string);
            JSStringRelease(string);
            return (ObjcContainerConvertor::Task){ object, js, ContainerNone };
        }

        if ([object isKindOfClass:[NSNumber class]]) {
            id literalTrue = @YES;
            if (object == literalTrue)
                return (ObjcContainerConvertor::Task){ object, JSValueMakeBoolean(contextInternalContext(context), true), ContainerNone };
            id literalFalse = @NO;
            if (object == literalFalse)
                return (ObjcContainerConvertor::Task){ object, JSValueMakeBoolean(contextInternalContext(context), false), ContainerNone };
            return (ObjcContainerConvertor::Task){ object, JSValueMakeNumber(contextInternalContext(context), [(NSNumber*)object doubleValue]), ContainerNone };
        }

        if ([object isKindOfClass:[NSDate class]]) {
            JSValueRef argument = JSValueMakeNumber(contextInternalContext(context), [(NSDate *)object timeIntervalSince1970]);
            JSObjectRef result = JSObjectMakeDate(contextInternalContext(context), 1, &argument, 0);
            return (ObjcContainerConvertor::Task){ object, result, ContainerNone };
        }
    }

    return (ObjcContainerConvertor::Task){ object, valueInternalValue([context wrapperForObject:object]), ContainerNone };
}

JSValueRef objectToValue(JSContext *context, id object)
{
    ObjcContainerConvertor::Task task = objectToValueWithoutCopy(context, object);
    if (task.type == ContainerNone)
        return task.js;

    ObjcContainerConvertor convertor(context);
    convertor.add(task);
    ASSERT(!convertor.isWorkListEmpty());

    do {
        ObjcContainerConvertor::Task current = convertor.take();
        ASSERT(JSValueIsObject(contextInternalContext(context), current.js));
        JSObjectRef js = JSValueToObject(contextInternalContext(context), current.js, 0);

        if (current.type == ContainerArray) {
            ASSERT([current.objc isKindOfClass:[NSArray class]]);
            NSArray *array = (NSArray *)current.objc;
            NSUInteger count = [array count];
            for (NSUInteger index = 0; index < count; ++index)
                JSObjectSetPropertyAtIndex(contextInternalContext(context), js, index, convertor.convert([array objectAtIndex:index]), 0);
        } else {
            ASSERT(current.type == ContainerDictionary);
            ASSERT([current.objc isKindOfClass:[NSDictionary class]]);
            NSDictionary* dictionary = (NSDictionary*)current.objc;
            for (id key in [dictionary keyEnumerator]) {
                if ([key isKindOfClass:[NSString class]]) {
                    JSStringRef propertyName = JSStringCreateWithCFString((CFStringRef)key);
                    JSObjectSetProperty(contextInternalContext(context), js, propertyName, convertor.convert([dictionary objectForKey:key]), 0, 0);
                    JSStringRelease(propertyName);
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

+ (JSValue *)valueWithValue:(JSValueRef)value inContext:(JSContext*)context
{
    return [[[JSValue alloc] initWithValue:value inContext:context] autorelease];
}

- (JSValue *)initWithValue:(JSValueRef)value inContext:(JSContext*)context
{
    self = [super init];
    if (!self)
        return nil;

    ASSERT(value);
    objc_initWeak(&m_weakContext, context);
    [context protect:value];
    m_value = value;
    return self;
}

struct StructTagHandler {
    SEL typeToValueSEL;
    SEL valueToTypeSEL;
};

static StructTagHandler* getStructTagHandler(const char* encodedType)
{
    static SpinLock getStructTagHandlerLock = SPINLOCK_INITIALIZER;
    SpinLockHolder lockHolder(&getStructTagHandlerLock);

    typedef HashMap<RefPtr<StringImpl>, StructTagHandler> StructHandlers;
    static StructHandlers* structHandlers = 0;

    if (!structHandlers) {
        structHandlers = new StructHandlers();

        // Step 1: find all valueWith<Foo>:inContext: class methods in JSValue.
        size_t valueWithMinLength = strlen("valueWithX:inContext:");
        forEachMethodInClass(object_getClass([JSValue class]), ^(Method method){
            SEL selector = method_getName(method);
            const char* name = sel_getName(selector);
            size_t nameLength = strlen(name);
            // Check for valueWith<Foo>:context:
            if (nameLength < valueWithMinLength || strncmp(name, "valueWith", 9) || strncmp(name + nameLength - 11, ":inContext:", 11))
                return;
            // Check for [ id, SEL, <type>, <contextType> ]
            if (method_getNumberOfArguments(method) != 4)
                return;
            char idType[3];
            // Check 2nd argument type is "@"
            char* secondType = method_copyArgumentType(method, 3);
            if (strcmp(secondType, "@")) {
                free(secondType);
                return;
            }
            free(secondType);
            // Check result type is also "@"
            method_getReturnType(method, idType, 3);
            if (strcmp(idType, "@"))
                return;
            char* type = method_copyArgumentType(method, 2);
            structHandlers->add(String(type).impl(), (StructTagHandler){ selector, 0 });
            free(type);
        });

        // Step 2: find all to<Foo> instance methods in JSValue.
        size_t minLenValue = strlen("toX");
        forEachMethodInClass([JSValue class], ^(Method method){
            SEL selector = method_getName(method);
            const char* name = sel_getName(selector);
            size_t nameLength = strlen(name);
            // Check for to<Foo>
            if (nameLength < minLenValue || strncmp(name, "to", 2))
                return;
            // Check for [ id, SEL ]
            if (method_getNumberOfArguments(method) != 2)
                return;
            // Try to find a matching valueWith<Foo>:context: method.
            char* type = method_copyReturnType(method);

            StructHandlers::iterator iter = structHandlers->find(String(type).impl());
            free(type);
            if (iter == structHandlers->end())
                return;
            StructTagHandler& handler = iter->value;

            // check that strlen(<foo>) == strlen(<Foo>)
            const char* valueWithName = sel_getName(handler.typeToValueSEL);
            size_t valueWithLen = strlen(valueWithName);
            if (valueWithLen - valueWithMinLength != nameLength - minLenValue)
                return;
            // Check that <Foo> == <Foo>
            if (strncmp(valueWithName + 9, name + 2, nameLength - minLenValue - 1))
                return;
            handler.valueToTypeSEL = selector;
        });

        // Step 3: clean up - remove entries where we found prospective valueWith<Foo>:inContext: conversions, but no matching to<Foo> methods.
        typedef HashSet<RefPtr<StringImpl> > RemoveSet;
        RemoveSet removeSet;
        for (StructHandlers::iterator iter = structHandlers->begin(); iter != structHandlers->end(); ++iter) {
            StructTagHandler& handler = iter->value;
            if (!handler.valueToTypeSEL)
                removeSet.add(iter->key);
        }

        for (RemoveSet::iterator iter = removeSet.begin(); iter != removeSet.end(); ++iter)
            structHandlers->remove(*iter);
    }

    StructHandlers::iterator iter = structHandlers->find(String(encodedType).impl());
    if (iter == structHandlers->end())
        return 0;
    return &iter->value;
}

+ (SEL)selectorForStructToValue:(const char *)structTag
{
    StructTagHandler* handler = getStructTagHandler(structTag);
    return handler ? handler->typeToValueSEL : nil;
}

+ (SEL)selectorForValueToStruct:(const char *)structTag
{
    StructTagHandler* handler = getStructTagHandler(structTag);
    return handler ? handler->valueToTypeSEL : nil;
}

- (void)dealloc
{
    JSContext *context = [self context];
    if (context)
        [context unprotect:m_value];
    objc_destroyWeak(&m_weakContext);
    [super dealloc];
}

- (NSString *)description
{
    JSContext *context = [self context];
    if (!context)
        return nil;

    if (id wrapped = tryUnwrapObjcObject(contextInternalContext(context), m_value))
        return [wrapped description];
    return [self toString];
}

NSInvocation* typeToValueInvocationFor(const char* encodedType)
{
    SEL selector = [JSValue selectorForStructToValue:encodedType];
    if (!selector)
        return 0;

    const char* methodTypes = method_getTypeEncoding(class_getClassMethod([JSValue class], selector));
    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:methodTypes]];
    [invocation setSelector:selector];
    return invocation;
}

NSInvocation* valueToTypeInvocationFor(const char* encodedType)
{
    SEL selector = [JSValue selectorForValueToStruct:encodedType];
    if (!selector)
        return 0;

    const char* methodTypes = method_getTypeEncoding(class_getInstanceMethod([JSValue class], selector));
    NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:[NSMethodSignature signatureWithObjCTypes:methodTypes]];
    [invocation setSelector:selector];
    return invocation;
}

@end

#endif
