/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "JSWebExtensionWrapper.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "WebFrame.h"
#import "WebPage.h"
#import <JavaScriptCore/JSObjectRef.h>
#import <JavaScriptCore/OpaqueJSString.h>

#import "JSWebExtensionWrappable.h"
#import "WebExtensionAPIRuntime.h"

namespace WebKit {

id toNSObject(JSContextRef context, JSValueRef valueRef, Class containingObjectsOfClass, NullValuePolicy nullPolicy, ValuePolicy valuePolicy)
{
    ASSERT(context);

    if (!valueRef)
        return nil;

    JSValue *value = [JSValue valueWithJSValueRef:valueRef inContext:toJSContext(context)];

    if (value.isArray) {
        NSUInteger length = [value[@"length"] toUInt32];
        NSMutableArray *mutableArray = [NSMutableArray arrayWithCapacity:length];

        for (NSUInteger i = 0; i < length; ++i) {
            JSValue *itemValue = [value valueAtIndex:i];
            if (valuePolicy == ValuePolicy::StopAtTopLevel && (itemValue.isArray || isDictionary(context, itemValue.JSValueRef))) {
                if (itemValue)
                    [mutableArray addObject:itemValue];
            } else if (id convertedItem = toNSObject(context, itemValue.JSValueRef, Nil, nullPolicy))
                [mutableArray addObject:convertedItem];
        }

        NSArray *resultArray = [mutableArray copy];
        if (!containingObjectsOfClass || containingObjectsOfClass == NSObject.class)
            return resultArray;

        return filterObjects(resultArray, ^bool(id, id value) {
            return [value isKindOfClass:containingObjectsOfClass];
        });
    }

    if (isDictionary(context, value.JSValueRef))
        return toNSDictionary(context, valueRef, nullPolicy, valuePolicy);

    if (value.isObject && !value.isDate && !value.isNull)
        return value;

    return [value toObject];
}

NSDictionary *toNSDictionary(JSContextRef context, JSValueRef valueRef, NullValuePolicy nullPolicy, ValuePolicy valuePolicy)
{
    ASSERT(context);

    if (!JSValueIsObject(context, valueRef))
        return nil;

    JSObjectRef object = JSValueToObject(context, valueRef, nullptr);
    if (!object)
        return nil;

    JSValue *value = [JSValue valueWithJSValueRef:valueRef inContext:toJSContext(context)];
    if (!isDictionary(context, value.JSValueRef))
        return nil;

    JSPropertyNameArrayRef propertyNames = JSObjectCopyPropertyNames(context, object);
    size_t propertyNameCount = JSPropertyNameArrayGetCount(propertyNames);

    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:propertyNameCount];

    for (size_t i = 0; i < propertyNameCount; ++i) {
        JSRetainPtr propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNames, i);
        if (!propertyName)
            continue;
        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG JSValueRef item = JSObjectGetProperty(context, object, propertyName.get(), 0);

        // Chrome does not include null values in dictionaries for web extensions.
        if (nullPolicy == NullValuePolicy::NotAllowed && JSValueIsNull(context, item))
            continue;

        auto *key = toString(propertyName.get()).createNSString().get();
        auto *itemValue = toJSValue(context, item);

        if (valuePolicy == ValuePolicy::StopAtTopLevel) {
            if (itemValue)
                result[key] = itemValue;
            continue;
        }

        if (isDictionary(context, itemValue.JSValueRef)) {
            if (auto *itemDictionary = toNSDictionary(context, item, nullPolicy))
                result[key] = itemDictionary;
        } else if (id value = toNSObject(context, item))
            result[key] = value;
    }

    JSPropertyNameArrayRelease(propertyNames);

    return [result copy];
}

JSValueRef toJSValueRef(JSContextRef context, NSURL *url, NullOrEmptyString nullOrEmptyString)
{
    ASSERT(context);

    return toJSValueRef(context, url.absoluteURL.absoluteString, nullOrEmptyString);
}

JSValueRef toJSValueRefOrJSNull(JSContextRef context, id object)
{
    ASSERT(context);
    return object ? toJSValueRef(context, object) : JSValueMakeNull(context);
}

NSArray *toNSArray(JSContextRef context, JSValueRef value, Class containingObjectsOfClass)
{
    ASSERT(containingObjectsOfClass);
    return dynamic_objc_cast<NSArray>(toNSObject(context, value, containingObjectsOfClass));
}

JSContext *toJSContext(JSContextRef context)
{
    ASSERT(context);
    return [JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(context)];
}

JSValue *toJSValue(JSContextRef context, JSValueRef value)
{
    ASSERT(context);

    if (!value)
        return nil;

    return [JSValue valueWithJSValueRef:value inContext:toJSContext(context)];
}

JSValueRef toJSValueRef(JSContextRef context, id object)
{
    ASSERT(context);

    if (!object)
        return JSValueMakeUndefined(context);

    if (JSValue *value = dynamic_objc_cast<JSValue>(object))
        return value.JSValueRef;

    return [JSValue valueWithObject:object inContext:toJSContext(context)].JSValueRef;
}

// This function lexicographically sorts the JSON output. The WTF JSON implementation does not yet support sorting keys,
// so this function has to stay on Cocoa for now.
String toSortedJSONString(JSContextRef context, JSValueRef value)
{
    // This double-JSON approach works best since it avoids JSC's Cocoa object conversion, which can produce JSValue's that NSJSONSerialization can't convert.
    auto* data = [nsStringNilIfEmpty(toJSONString(context, value)).autorelease() dataUsingEncoding:NSUTF8StringEncoding];
    if (!data)
        return nullString();

    id object = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:nullptr];
    if (!object)
        return nullString();

    data = [NSJSONSerialization dataWithJSONObject:object options:NSJSONWritingFragmentsAllowed | NSJSONWritingSortedKeys error:nullptr];
    if (!data)
        return nullString();

    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
