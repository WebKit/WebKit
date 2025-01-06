/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#import "JSWebExtensionWrappable.h"
#import "WebExtensionAPIRuntime.h"

namespace WebKit {

WebExtensionCallbackHandler::WebExtensionCallbackHandler(JSValue *callbackFunction)
    : m_callbackFunction(JSValueToObject(callbackFunction.context.JSGlobalContextRef, callbackFunction.JSValueRef, nullptr))
    , m_globalContext(callbackFunction.context.JSGlobalContextRef)
{
    ASSERT(callbackFunction);
    ASSERT(callbackFunction._isFunction);

    JSValueProtect(m_globalContext.get(), m_callbackFunction);
}

WebExtensionCallbackHandler::WebExtensionCallbackHandler(JSContextRef context, JSObjectRef callbackFunction, WebExtensionAPIRuntimeBase& runtime)
    : m_callbackFunction(callbackFunction)
    , m_globalContext(JSContextGetGlobalContext(context))
    , m_runtime(&runtime)
{
    ASSERT(context);
    ASSERT(callbackFunction);

    JSValueProtect(m_globalContext.get(), m_callbackFunction);
}

WebExtensionCallbackHandler::WebExtensionCallbackHandler(JSContextRef context, WebExtensionAPIRuntimeBase& runtime)
    : m_globalContext(JSContextGetGlobalContext(context))
    , m_runtime(&runtime)
{
    ASSERT(context);
}

WebExtensionCallbackHandler::WebExtensionCallbackHandler(JSContextRef context, JSObjectRef resolveFunction, JSObjectRef rejectFunction)
    : m_callbackFunction(resolveFunction)
    , m_rejectFunction(rejectFunction)
    , m_globalContext(JSContextGetGlobalContext(context))
{
    ASSERT(context);
    ASSERT(resolveFunction);
    ASSERT(rejectFunction);

    JSValueProtect(m_globalContext.get(), m_callbackFunction);
    JSValueProtect(m_globalContext.get(), m_rejectFunction);
}

WebExtensionCallbackHandler::~WebExtensionCallbackHandler()
{
    if (m_callbackFunction)
        JSValueUnprotect(m_globalContext.get(), m_callbackFunction);

    if (m_rejectFunction)
        JSValueUnprotect(m_globalContext.get(), m_rejectFunction);
}

JSValue *WebExtensionCallbackHandler::callbackFunction() const
{
    if (!m_globalContext || !m_callbackFunction)
        return nil;

    return [JSValue valueWithJSValueRef:m_callbackFunction inContext:[JSContext contextWithJSGlobalContextRef:m_globalContext.get()]];
}

template<size_t ArgumentCount>
id callWithArguments(JSObjectRef callbackFunction, JSRetainPtr<JSGlobalContextRef>& globalContext, std::array<JSValueRef, ArgumentCount>&& arguments)
{
    if (!globalContext || !callbackFunction)
        return nil;
    return toNSObject(globalContext.get(), JSObjectCallAsFunction(globalContext.get(), callbackFunction, nullptr, ArgumentCount, arguments.data(), nullptr));
}

void WebExtensionCallbackHandler::reportError(NSString *message)
{
    if (!m_globalContext)
        return;

    if (m_runtime) {
        m_runtime->reportError(message, *this);
        return;
    }

    if (!m_rejectFunction)
        return;

    RELEASE_LOG_ERROR(Extensions, "Promise rejected: %{public}@", message);

    JSValue *error = [JSValue valueWithNewErrorFromMessage:message inContext:[JSContext contextWithJSGlobalContextRef:m_globalContext.get()]];

    callWithArguments<1>(m_rejectFunction, m_globalContext, {
        toJSValueRef(m_globalContext.get(), error)
    });
}

id WebExtensionCallbackHandler::call()
{
    return callWithArguments<0>(m_callbackFunction, m_globalContext, { });
}

id WebExtensionCallbackHandler::call(id argument)
{
    return callWithArguments<1>(m_callbackFunction, m_globalContext, {
        toJSValueRef(m_globalContext.get(), argument)
    });
}

id WebExtensionCallbackHandler::call(id argumentOne, id argumentTwo)
{
    return callWithArguments<2>(m_callbackFunction, m_globalContext, {
        toJSValueRef(m_globalContext.get(), argumentOne),
        toJSValueRef(m_globalContext.get(), argumentTwo)
    });
}

id WebExtensionCallbackHandler::call(id argumentOne, id argumentTwo, id argumentThree)
{
    return callWithArguments<3>(m_callbackFunction, m_globalContext, {
        toJSValueRef(m_globalContext.get(), argumentOne),
        toJSValueRef(m_globalContext.get(), argumentTwo),
        toJSValueRef(m_globalContext.get(), argumentThree)
    });
}

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
            if (valuePolicy == ValuePolicy::StopAtTopLevel && (itemValue.isArray || itemValue._isDictionary)) {
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

    if (value._isDictionary)
        return toNSDictionary(context, valueRef, nullPolicy, valuePolicy);

    if (value.isObject && !value.isDate && !value.isNull)
        return value;

    return [value toObject];
}

NSString *toNSString(JSContextRef context, JSValueRef value, NullStringPolicy nullStringPolicy)
{
    ASSERT(context);
    ASSERT(value);

    switch (nullStringPolicy) {
    case NullStringPolicy::NullAndUndefinedAsNullString:
        if (JSValueIsUndefined(context, value))
            return nil;
        FALLTHROUGH;

    case NullStringPolicy::NullAsNullString:
        if (JSValueIsNull(context, value))
            return nil;
        FALLTHROUGH;

    case NullStringPolicy::NoNullString:
        // Don't try to convert other objects into strings.
        if (!JSValueIsString(context, value))
            return nil;

        JSRetainPtr<JSStringRef> string(Adopt, JSValueToStringCopy(context, value, 0));
        return CFBridgingRelease(JSStringCopyCFString(nullptr, string.get()));
    }
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
    if (!value._isDictionary)
        return nil;

    JSPropertyNameArrayRef propertyNames = JSObjectCopyPropertyNames(context, object);
    size_t propertyNameCount = JSPropertyNameArrayGetCount(propertyNames);

    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:propertyNameCount];

    for (size_t i = 0; i < propertyNameCount; ++i) {
        JSRetainPtr<JSStringRef> propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNames, i);
        JSValueRef item = JSObjectGetProperty(context, object, propertyName.get(), 0);

        // Chrome does not include null values in dictionaries for web extensions.
        if (nullPolicy == NullValuePolicy::NotAllowed && JSValueIsNull(context, item))
            continue;

        auto *key = toNSString(propertyName.get());
        auto *itemValue = toJSValue(context, item);

        if (valuePolicy == ValuePolicy::StopAtTopLevel) {
            if (itemValue)
                result[key] = itemValue;
            continue;
        }

        if (itemValue._isDictionary) {
            if (auto *itemDictionary = toNSDictionary(context, item, nullPolicy))
                result[key] = itemDictionary;
        } else if (id value = toNSObject(context, item))
            result[key] = value;
    }

    JSPropertyNameArrayRelease(propertyNames);

    return [result copy];
}

JSValueRef toJSValueRef(JSContextRef context, NSString *string, NullOrEmptyString nullOrEmptyString)
{
    ASSERT(context);

    switch (nullOrEmptyString) {
    case NullOrEmptyString::NullStringAsNull:
        if (!string)
            return JSValueMakeNull(context);
        FALLTHROUGH;

    case NullOrEmptyString::NullStringAsEmptyString:
        return JSValueMakeString(context, toJSString(string).get());
    }
}

JSValueRef toJSValueRef(JSContextRef context, NSURL *url, NullOrEmptyString nullOrEmptyString)
{
    ASSERT(context);

    return toJSValueRef(context, url.absoluteURL.absoluteString, nullOrEmptyString);
}

RefPtr<WebExtensionCallbackHandler> toJSCallbackHandler(JSContextRef context, JSValueRef callbackValue, WebExtensionAPIRuntimeBase& runtime)
{
    ASSERT(context);

    if (!callbackValue)
        return nullptr;

    JSObjectRef callbackFunction = JSValueToObject(context, callbackValue, nullptr);
    if (!callbackFunction)
        return nullptr;

    if (!JSObjectIsFunction(context, callbackFunction))
        return nullptr;

    return WebExtensionCallbackHandler::create(context, callbackFunction, runtime);
}

NSString *toNSString(JSStringRef string)
{
    return string ? CFBridgingRelease(JSStringCopyCFString(nullptr, string)) : nil;
}

JSValueRef deserializeJSONString(JSContextRef context, NSString *jsonString)
{
    ASSERT(context);

    if (!jsonString)
        return JSValueMakeNull(context);

    if (JSValueRef value = JSValueMakeFromJSONString(context, toJSString(jsonString).get()))
        return value;

    return JSValueMakeNull(context);
}

NSString *serializeJSObject(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(context);

    if (!value)
        return nil;

    JSRetainPtr<JSStringRef> string(Adopt, JSValueCreateJSONString(context, value, 0, exception));

    return toNSString(string.get());
}

JSObjectRef toJSError(JSContextRef context, NSString *string)
{
    ASSERT(context);

    RELEASE_LOG_ERROR(Extensions, "Exception thrown: %{public}@", string);

    JSValueRef messageArgument = toJSValueRef(context, string, NullOrEmptyString::NullStringAsEmptyString);
    return JSObjectMakeError(context, 1, &messageArgument, nullptr);
}

JSRetainPtr<JSStringRef> toJSString(NSString *string)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCFString(string ? (__bridge CFStringRef)string : CFSTR("")));
}

JSValueRef toJSValueRefOrJSNull(JSContextRef context, id object)
{
    ASSERT(context);
    return object ? toJSValueRef(context, object) : JSValueMakeNull(context);
}

NSArray *toNSArray(JSContextRef context, JSValueRef value, Class containingObjectsOfClass)
{
    ASSERT(containingObjectsOfClass);
    return toNSObject(context, value, containingObjectsOfClass);
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

JSValue *toWindowObject(JSContextRef context, WebFrame& frame)
{
    ASSERT(context);

    auto frameContext = frame.jsContext();
    if (!frameContext)
        return nil;

    return toJSValue(context, JSContextGetGlobalObject(frameContext));
}

JSValue *toWindowObject(JSContextRef context, WebPage& page)
{
    ASSERT(context);

    return toWindowObject(context, page.mainWebFrame());
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

} // namespace WebKit

using namespace WebKit;

#if JSC_OBJC_API_ENABLED

@implementation JSValue (WebKitExtras)

- (NSString *)_toJSONString
{
    return serializeJSObject(self.context.JSGlobalContextRef, self.JSValueRef, nullptr);
}

- (NSString *)_toSortedJSONString
{
    // This double-JSON approach works best since it avoids JSC's Cocoa object conversion, which can produce JSValue's that NSJSONSerialization can't convert.
    auto* data = [self._toJSONString dataUsingEncoding:NSUTF8StringEncoding];
    if (!data)
        return nil;

    id object = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:nullptr];
    if (!object)
        return nil;

    data = [NSJSONSerialization dataWithJSONObject:object options:NSJSONWritingFragmentsAllowed | NSJSONWritingSortedKeys error:nullptr];
    if (!data)
        return nil;

    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

- (BOOL)_isFunction
{
    JSGlobalContextRef context = self.context.JSGlobalContextRef;
    JSValueRef valueRef = self.JSValueRef;

    if (!valueRef || !JSValueIsObject(context, valueRef))
        return NO;

    JSObjectRef functionRef = JSValueToObject(context, valueRef, nullptr);
    return functionRef && JSObjectIsFunction(context, functionRef);
}

- (BOOL)_isDictionary
{
    // Equivalent to JavaScript: this.__proto__ === Object.prototype
    // Using isInstanceOf: is too permissive here since all built-in objects inherit from Object.
    return self.isObject && [self[@"__proto__"] isEqualToObject:self.context[@"Object"][@"prototype"]] && !self._isThenable;
}

- (BOOL)_isRegularExpression
{
    return self.isObject && [self isInstanceOf:self.context[@"RegExp"]];
}

- (BOOL)_isThenable
{
    return self.isObject && dynamic_objc_cast<JSValue>(self[@"then"])._isFunction;
}

- (void)_awaitThenableResolutionWithCompletionHandler:(void (^)(JSValue *result, JSValue *error))completionHandler
{
    ASSERT(self._isThenable);

    auto resolveBlock = ^(JSValue *result) {
        completionHandler(result, nil);
    };

    auto rejectBlock = ^(JSValue *error) {
        completionHandler(nil, error);
    };

    [self invokeMethod:@"then" withArguments:@[ resolveBlock, rejectBlock ]];
}

@end

#endif // JSC_OBJC_API_ENABLED
#endif // ENABLE(WK_WEB_EXTENSIONS)
