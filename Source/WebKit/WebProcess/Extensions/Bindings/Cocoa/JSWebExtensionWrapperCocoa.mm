/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "JSWebExtensionWrappable.h"
#import "WebExtensionAPIRuntime.h"
#import <JavaScriptCore/JSObjectRef.h>

namespace WebKit {

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
        m_runtime->reportErrorForCallbackHandler(*this, message, m_globalContext.get());
        return;
    }

    if (!m_rejectFunction)
        return;

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

id toNSObject(JSContextRef context, JSValueRef valueRef, Class requiredClass)
{
    ASSERT(context);

    if (!valueRef)
        return nil;

    JSValue *value = [JSValue valueWithJSValueRef:valueRef inContext:[JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(context)]];

    // For functions and promises ("thenable" objects), return the JSValue instead of calling toObject since that will convert it to an empty NSDictionary and be useless.
    if (JSValueIsObject(context, valueRef)) {
        JSObjectRef objectRef = JSValueToObject(context, valueRef, nullptr);
        if (JSObjectIsFunction(context, objectRef))
            return value;

        if (value._thenable)
            return value;
    }

    id result = [value toObject];
    NSArray *resultArray = dynamic_objc_cast<NSArray>(result);
    if (!requiredClass || !resultArray)
        return result;

    return filterObjects(resultArray, ^bool (id, id value) {
        return [value isKindOfClass:requiredClass];
    });
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
        JSRetainPtr<JSStringRef> string(Adopt, JSValueToStringCopy(context, value, 0));
        return CFBridgingRelease(JSStringCopyCFString(nullptr, string.get()));
    }
}

NSDictionary *toNSDictionary(JSContextRef context, JSValueRef value)
{
    ASSERT(context);

    if (!JSValueIsObject(context, value))
        return nil;

    JSObjectRef object = JSValueToObject(context, value, nullptr);
    if (!object)
        return nil;

    JSPropertyNameArrayRef propertyNames = JSObjectCopyPropertyNames(context, object);
    size_t propertyNameCount = JSPropertyNameArrayGetCount(propertyNames);

    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:propertyNameCount];

    for (size_t i = 0; i < propertyNameCount; ++i) {
        JSRetainPtr<JSStringRef> propertyName = JSPropertyNameArrayGetNameAtIndex(propertyNames, i);
        JSValueRef item = JSObjectGetProperty(context, object, propertyName.get(), 0);

        // Chrome does not include null values in dictionaries for web extensions.
        if (JSValueIsNull(context, item))
            continue;

        if (id value = toNSObject(context, item))
            [result setObject:value forKey:toNSString(propertyName.get())];
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

NSString *toNSString(JSStringRef string)
{
    return string ? CFBridgingRelease(JSStringCopyCFString(nullptr, string)) : nil;
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

} // namespace WebKit

using namespace WebKit;

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

- (BOOL)_isRegularExpression
{
    return self.isObject && dynamic_objc_cast<JSValue>(self[@"test"])._function;
}

- (BOOL)_isThenable
{
    return self.isObject && dynamic_objc_cast<JSValue>(self[@"then"])._function;
}

- (void)_awaitThenableResolutionWithCompletionHandler:(void (^)(JSValue *result, JSValue *error))completionHandler
{
    ASSERT(self._thenable);

    auto resolveBlock = ^(JSValue *result) {
        completionHandler(result, nil);
    };

    auto rejectBlock = ^(JSValue *error) {
        completionHandler(nil, error);
    };

    [self invokeMethod:@"then" withArguments:@[ resolveBlock, rejectBlock ]];
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
