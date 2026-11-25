/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPIEvent.h"
#include "JSWebExtensionAPITestCPP.h"
#include "Logging.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPITestCPP.h"
#include "WebExtensionUtilities.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <wtf/GetPtr.h>

namespace WebKit {

WebExtensionAPITestCPP* toWebExtensionAPITestCPP(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSWebExtensionAPITestCPP::testCPPClassSingleton() || !JSValueIsObjectOfClass(context, value, JSWebExtensionAPITestCPP::testCPPClassSingleton()))
        return nullptr;
    return downcast<WebExtensionAPITestCPP>(JSWebExtensionWrapper::unwrap(context, value));
}

JSClassRef JSWebExtensionAPITestCPP::testCPPClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestCPP";
        definition.parentClass = nullptr;
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        definition.getPropertyNames = getPropertyNames;
        definition.hasProperty = hasProperty;
        definition.getProperty = getProperty;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

JSClassRef JSWebExtensionAPITestCPP::testCPPGlobalObjectClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestCPP";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

const JSStaticFunction* JSWebExtensionAPITestCPP::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "notifyFail", notifyFail, kJSPropertyAttributeNone },
        { "sendMessage", sendMessage, kJSPropertyAttributeNone },
        { "runWithUserGesture", runWithUserGesture, kJSPropertyAttributeNone },
        { "isProcessingUserGesture", isProcessingUserGesture, kJSPropertyAttributeNone },
        { "assertEq", assertEq, kJSPropertyAttributeNone },
        { "assertRejects", assertRejects, kJSPropertyAttributeNone },
        { "create", create, kJSPropertyAttributeNone },
        { "get", get, kJSPropertyAttributeNone },
        { "getAll", getAll, kJSPropertyAttributeNone },
        { "clear", clear, kJSPropertyAttributeNone },
        { "getItems", getItems, kJSPropertyAttributeNone },
        { "getBytesInUse", getBytesInUse, kJSPropertyAttributeNone },
        { "set", set, kJSPropertyAttributeNone },
        { "getURL", getURL, kJSPropertyAttributeNone },
        { "getManifest", getManifest, kJSPropertyAttributeNone },
        { "sendRuntimeMessage", sendRuntimeMessage, kJSPropertyAttributeNone },
        { nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return functions;
}

const JSStaticValue* JSWebExtensionAPITestCPP::staticValues()
{
    static const JSStaticValue values[] = {
        { "onAlarm", onAlarm, nullptr, kJSPropertyAttributeNone },
        { "QUOTA_BYTES", quotaBytes, nullptr, kJSPropertyAttributeNone },
        { nullptr, nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return values;
}

const JSStaticValue* JSWebExtensionAPITestCPP::staticConstants()
{
    return nullptr;
}

bool JSWebExtensionAPITestCPP::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, testCPPClassSingleton());
}

void JSWebExtensionAPITestCPP::getPropertyNames(JSContextRef context, JSObjectRef thisObject, JSPropertyNameAccumulatorRef propertyNames)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return;

    RefPtr page = toWebPage(context);
    if (impl->isPropertyAllowed("setAccessLevel"_s, page.get()))
        SUPPRESS_UNCOUNTED_ARG JSPropertyNameAccumulatorAddName(propertyNames, toJSString("setAccessLevel"_s).get());

    if (impl->isPropertyAllowed("MAX_ITEMS"_s, page.get()))
        SUPPRESS_UNCOUNTED_ARG JSPropertyNameAccumulatorAddName(propertyNames, toJSString("MAX_ITEMS"_s).get());

}

bool JSWebExtensionAPITestCPP::hasProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return false;

    RefPtr page = toWebPage(context);
    if (JSStringIsEqualToUTF8CString(propertyName, "setAccessLevel"))
        return impl->isPropertyAllowed("setAccessLevel"_s, page.get());

    if (JSStringIsEqualToUTF8CString(propertyName, "MAX_ITEMS"))
        return impl->isPropertyAllowed("MAX_ITEMS"_s, page.get());

    return false;
}

JSValueRef JSWebExtensionAPITestCPP::getProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RefPtr page = toWebPage(context);
    if (JSStringIsEqualToUTF8CString(propertyName, "MAX_ITEMS") && impl->isPropertyAllowed("MAX_ITEMS"_s, page.get()))
        return maxItems(context, thisObject, propertyName, exception);

    if (JSStringIsEqualToUTF8CString(propertyName, "setAccessLevel") && impl->isPropertyAllowed("setAccessLevel"_s, page.get()))
        return JSObjectMakeFunctionWithCallback(context, propertyName, setAccessLevel);

    return JSValueMakeUndefined(context);
}

// Functions

JSValueRef JSWebExtensionAPITestCPP::notifyFail(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.notifyFail() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    String message = nullString();

    if (argumentCount == 1) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.notifyFail()"_s, "message"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        message = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
    }

    impl->notifyFail(context, message);

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.sendMessage() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.sendMessage()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    String message = nullString();
    JSValueRef argument = JSValueMakeUndefined(context);

    if (argumentCount == 2) {
        if (!(JSValueIsString(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.sendMessage()"_s, "message"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        message = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
        argument = arguments[1];
    } else if (argumentCount == 1) {
        if (!(JSValueIsString(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.sendMessage()"_s, "message"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        message = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
    }

    impl->sendMessage(context, message, argument);

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::runWithUserGesture(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.runWithUserGesture() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.runWithUserGesture()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    JSValueRef function = arguments[0];

    RefPtr frame = toWebFrame(context);
    if (!frame) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Frame could not be found for JSContextRef");
        return JSValueMakeUndefined(context);
    }

    SUPPRESS_UNCOUNTED_ARG return toJSValueRefOrJSNull(context, impl->runWithUserGesture(*frame, context, function));
}

JSValueRef JSWebExtensionAPITestCPP::isProcessingUserGesture(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.isProcessingUserGesture() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    SUPPRESS_UNCOUNTED_ARG return JSValueMakeBoolean(context, impl->isProcessingUserGesture());
}

JSValueRef JSWebExtensionAPITestCPP::assertEq(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.assertEq() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 2;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.assertEq()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    JSValueRef actualValue = JSValueMakeUndefined(context);
    JSValueRef expectedValue = JSValueMakeUndefined(context);
    String message = nullString();

    if (argumentCount == 3) {
        if (!(JSValueIsString(context, arguments[2]) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.assertEq()"_s, "message"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        actualValue = arguments[0];
        expectedValue = arguments[1];
        message = toString(context, arguments[2], NullStringPolicy::NullAndUndefinedAsNullString);
    } else if (argumentCount == 2) {
        actualValue = arguments[0];
        expectedValue = arguments[1];
    }

    NSString *exceptionString;
    impl->assertEq(context, actualValue, expectedValue, message, &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testCPP.assertEq()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::assertRejects(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.assertRejects() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    size_t argumentIndex = argumentCount;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.assertRejects()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    JSValueRef promise = JSValueMakeUndefined(context);
    JSValueRef expectedError = JSValueMakeUndefined(context);
    String message = nullString();

    if (argumentCount == 3) {
        if (!(JSValueIsObject(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.assertRejects()"_s, "promise"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!(JSValueIsString(context, arguments[2]) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.assertRejects()"_s, "message"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        promise = arguments[0];
        expectedError = arguments[1];
        message = toString(context, arguments[2], NullStringPolicy::NullAndUndefinedAsNullString);
    } else {
        JSValueRef currentArgument = nullptr;
        size_t allowedOptionalArgumentCount = argumentCount - requiredArgumentCount;
        size_t processedOptionalArgumentCount = 0;
        argumentIndex = 0;

        if (argumentIndex < argumentCount && (currentArgument = arguments[argumentIndex])) {
            if (!(JSValueIsObject(context, currentArgument))) [[unlikely]] {
                *exception = toJSError(context, "testCPP.assertRejects()"_s, "promise"_s, "an object is expected"_s);
                return JSValueMakeUndefined(context);
            }

            promise = currentArgument;
            ++argumentIndex;
        }

        if (argumentIndex < argumentCount && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (currentArgument) {
                expectedError = currentArgument;
                ++processedOptionalArgumentCount;
                ++argumentIndex;
            }
        }

        if (argumentIndex < argumentCount && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (JSValueIsString(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                message = toString(context, currentArgument, NullStringPolicy::NullAndUndefinedAsNullString);
                ++processedOptionalArgumentCount;
                ++argumentIndex;
            }
        }
    }

    if (promise && !JSValueIsObject(context, promise)) [[unlikely]] {
        *exception = toJSError(context, "testCPP.assertRejects()"_s, "promise"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex < argumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.assertRejects()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    SUPPRESS_UNCOUNTED_ARG return toJSValueRefOrJSNull(context, impl->assertRejects(context, promise, expectedError, message));
}

JSValueRef JSWebExtensionAPITestCPP::create(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.create() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    ssize_t argumentIndex = -1;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.create()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    String name = nullString();
    RefPtr<JSON::Value> info;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.create()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!(isDictionary(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.create()"_s, "info"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.create()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        name = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
        info = toJSONValue(context, arguments[1], NullValuePolicy::NotAllowed);
        callback = toJSCallbackHandler(context, arguments[2], protect(impl->runtime()));
    } else {
        JSValueRef currentArgument = nullptr;
        size_t allowedOptionalArgumentCount = argumentCount - requiredArgumentCount;
        size_t processedOptionalArgumentCount = 0;
        argumentIndex = argumentCount - 1;

        if (argumentIndex >= 0 && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if ((JSValueIsObject(context, currentArgument) && JSObjectIsFunction(context, JSValueToObject(context, currentArgument, nullptr))) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                callback = toJSCallbackHandler(context, currentArgument, protect(impl->runtime()));
                ++processedOptionalArgumentCount;
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            if (!(isDictionary(context, currentArgument))) [[unlikely]] {
                *exception = toJSError(context, "testCPP.create()"_s, "info"_s, "an object is expected"_s);
                return JSValueMakeUndefined(context);
            }

            info = toJSONValue(context, currentArgument, NullValuePolicy::NotAllowed);
            --argumentIndex;
        }

        if (argumentIndex >= 0 && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (JSValueIsString(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                name = toString(context, currentArgument, NullStringPolicy::NullAndUndefinedAsNullString);
                ++processedOptionalArgumentCount;
                --argumentIndex;
            }
        }
    }

    if (!info) [[unlikely]] {
        *exception = toJSError(context, "testCPP.create()"_s, "info"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex >= 0) [[unlikely]] {
        *exception = toJSError(context, "testCPP.create()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.create()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    String exceptionString;
    impl->createAlarm(page->webPageProxyIdentifier(), name, info, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testCPP.create()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::get(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.get() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    String name = nullString();
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.get()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.get()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        name = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.get()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        name = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.get()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        callback = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.get()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    impl->get(page->webPageProxyIdentifier(), name, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::getAll(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.getAll() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.getAll()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        callback = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.getAll()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    impl->getAll(page->webPageProxyIdentifier(), callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::clear(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.clear() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    String name = nullString();
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.clear()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.clear()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        name = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.clear()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        name = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.clear()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        callback = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.clear()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    impl->clear(page->webPageProxyIdentifier(), name, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::getItems(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.getItems() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    RefPtr<JSON::Value> items;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.getItems()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        items = fromJSValue(context, arguments[0], NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        items = fromJSValue(context, arguments[0], NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.getItems()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        callback = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.getItems()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    String exceptionString;
    impl->getItems(page->webPageProxyIdentifier(), items, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testCPP.getItems()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::getBytesInUse(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.getBytesInUse() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    RefPtr<JSON::Value> keys;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.getBytesInUse()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        keys = fromJSValue(context, arguments[0], NullValuePolicy::NotAllowed);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        keys = fromJSValue(context, arguments[0], NullValuePolicy::NotAllowed);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.getBytesInUse()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        callback = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.getBytesInUse()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    String exceptionString;
    impl->getBytesInUse(page->webPageProxyIdentifier(), keys, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testCPP.getBytesInUse()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::set(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.set() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.set()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    JSValueRef items = JSValueMakeUndefined(context);
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(JSValueIsObject(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.set()"_s, "items"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.set()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        items = arguments[0];
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        if (!(JSValueIsObject(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.set()"_s, "items"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        items = arguments[0];
    }

    if (items && !JSValueIsObject(context, items)) [[unlikely]] {
        *exception = toJSError(context, "testCPP.set()"_s, "items"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.set()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    String exceptionString;
    impl->set(page->webPageProxyIdentifier(), context, items, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testCPP.set()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::setAccessLevel(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.setAccessLevel() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.setAccessLevel()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr<JSON::Value> accessOptions;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(isDictionary(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.setAccessLevel()"_s, "accessOptions"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.setAccessLevel()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        accessOptions = toJSONValue(context, arguments[0], NullValuePolicy::NotAllowed);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        if (!(isDictionary(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.setAccessLevel()"_s, "accessOptions"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        accessOptions = toJSONValue(context, arguments[0], NullValuePolicy::NotAllowed);
    }

    if (!accessOptions) [[unlikely]] {
        *exception = toJSError(context, "testCPP.setAccessLevel()"_s, "accessOptions"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.setAccessLevel()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    String exceptionString;
    impl->setAccessLevel(page->webPageProxyIdentifier(), accessOptions, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testCPP.setAccessLevel()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestCPP::getURL(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.getURL() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.getURL()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    if (!(JSValueIsString(context, arguments[0]))) [[unlikely]] {
        *exception = toJSError(context, "testCPP.getURL()"_s, "resourcePath"_s, "a string is expected"_s);
        return JSValueMakeUndefined(context);
    }

    String resourcePath = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);

    String exceptionString;
    JSValueRef result = toJSValueRef(context, impl->getURL(resourcePath, exceptionString), NullOrEmptyString::NullStringAsNull);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testCPP.getURL()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return result;
}

JSValueRef JSWebExtensionAPITestCPP::getManifest(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.getManifest() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    SUPPRESS_UNCOUNTED_ARG return toJSValueRefOrJSNull(context, impl->getManifest());
}

JSValueRef JSWebExtensionAPITestCPP::sendRuntimeMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestCPP(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testCPP.sendRuntimeMessage() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", toDebugString(impl->contentWorldType()).utf8());

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    size_t argumentIndex = argumentCount;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    String extensionID = nullString();
    std::optional<WebExtensionMessage> message = std::nullopt;
    RefPtr<JSON::Value> options;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 4) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, "extensionID"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((!JSValueIsNull(context, arguments[1]) && !JSValueIsUndefined(context, arguments[1]) && !JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, "message"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!(isDictionary(context, arguments[2]) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, "options"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[3]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[3], nullptr))) || JSValueIsNull(context, arguments[3]) || JSValueIsUndefined(context, arguments[3]))) [[unlikely]] {
            *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        extensionID = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
        message = serializeMessage(context, arguments[1], impl->usesStructuredCloneMessageSerialization(), exception);
        options = toJSONValue(context, arguments[2], NullValuePolicy::NotAllowed);
        callback = toJSCallbackHandler(context, arguments[3], protect(impl->runtime()));
    } else {
        JSValueRef currentArgument = nullptr;
        size_t allowedOptionalArgumentCount = argumentCount - requiredArgumentCount;
        size_t processedOptionalArgumentCount = 0;
        argumentIndex = 0;

        if (allowedOptionalArgumentCount && (currentArgument = arguments[argumentCount - 1]) && ((JSValueIsObject(context, currentArgument) && JSObjectIsFunction(context, JSValueToObject(context, currentArgument, nullptr))) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument))) {
            callback = toJSCallbackHandler(context, currentArgument, protect(impl->runtime()));
            --allowedOptionalArgumentCount;
            if (argumentCount)
                --argumentCount;
        }

        if (argumentIndex < argumentCount && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (JSValueIsString(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                extensionID = toString(context, currentArgument, NullStringPolicy::NullAndUndefinedAsNullString);
                ++processedOptionalArgumentCount;
                ++argumentIndex;
            }
        }

        if (argumentIndex < argumentCount && (currentArgument = arguments[argumentIndex])) {
            if (!((!JSValueIsNull(context, currentArgument) && !JSValueIsUndefined(context, currentArgument) && !JSObjectIsFunction(context, JSValueToObject(context, currentArgument, nullptr))))) [[unlikely]] {
                *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, "message"_s, "an object is expected"_s);
                return JSValueMakeUndefined(context);
            }

            message = serializeMessage(context, currentArgument, impl->usesStructuredCloneMessageSerialization(), exception);
            ++argumentIndex;
        }

        if (argumentIndex < argumentCount && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (isDictionary(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                options = toJSONValue(context, currentArgument, NullValuePolicy::NotAllowed);
                ++processedOptionalArgumentCount;
                ++argumentIndex;
            }
        }
    }

    if (*exception) [[unlikely]]
        return JSValueMakeUndefined(context);

    if (!message) [[unlikely]] {
        *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, "message"_s, impl->usesStructuredCloneMessageSerialization() ? "a serializable value is expected"_s : "a JSON serializable value is expected"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex < argumentCount) [[unlikely]] {
        *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.sendRuntimeMessage()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    RefPtr frame = toWebFrame(context);
    if (!frame) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Frame could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testCPP.sendRuntimeMessage()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    String exceptionString;
    impl->sendRuntimeMessage(page->webPageProxyIdentifier(), *frame, extensionID, *message, options, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testCPP.sendRuntimeMessage()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

// Attributes

JSValueRef JSWebExtensionAPITestCPP::onAlarm(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestCPP(context, object);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testCPP.onAlarm in %" PUBLIC_LOG_STRING " world", toDebugString(impl->contentWorldType()).utf8());

    SUPPRESS_UNCOUNTED_ARG return toJS(context, getPtr(impl->onAlarm()));
}

JSValueRef JSWebExtensionAPITestCPP::quotaBytes(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestCPP(context, object);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testCPP.QUOTA_BYTES in %" PUBLIC_LOG_STRING " world", toDebugString(impl->contentWorldType()).utf8());

    SUPPRESS_UNCOUNTED_ARG return JSValueMakeNumber(context, impl->quotaBytes());
}

JSValueRef JSWebExtensionAPITestCPP::maxItems(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestCPP(context, object);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testCPP.MAX_ITEMS in %" PUBLIC_LOG_STRING " world", toDebugString(impl->contentWorldType()).utf8());

    SUPPRESS_UNCOUNTED_ARG return JSValueMakeNumber(context, impl->maxItems());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
