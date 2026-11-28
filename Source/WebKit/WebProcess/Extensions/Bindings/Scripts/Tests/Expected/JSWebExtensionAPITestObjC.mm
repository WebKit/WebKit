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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#include "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionAPIAction.h"
#include "JSWebExtensionAPIEvent.h"
#include "JSWebExtensionAPITestObjC.h"
#include "Logging.h"
#include "WebExtensionAPIAction.h"
#include "WebExtensionAPIEvent.h"
#include "WebExtensionAPITestObjC.h"
#include "WebExtensionUtilities.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <wtf/GetPtr.h>

namespace WebKit {

WebExtensionAPITestObjC* toWebExtensionAPITestObjC(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSWebExtensionAPITestObjC::testObjCClassSingleton() || !JSValueIsObjectOfClass(context, value, JSWebExtensionAPITestObjC::testObjCClassSingleton()))
        return nullptr;
    return downcast<WebExtensionAPITestObjC>(JSWebExtensionWrapper::unwrap(context, value));
}

std::expected<WebExtensionTestObjCOptions, WebExtensionValueError> parseWebExtensionTestObjCOptions(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);
    Vector<String> missingRequiredKeys;

    auto nameRead = readDictionaryMember<String>(context, object, "name"_s, path);
    if (!nameRead)
        return makeUnexpected(WTF::move(nameRead.error()));

    std::optional<String> nameValue = WTF::move(*nameRead);

    if (!nameValue)
        missingRequiredKeys.append("name"_s);

    auto countRead = readDictionaryMember<double>(context, object, "count"_s, path);
    if (!countRead)
        return makeUnexpected(WTF::move(countRead.error()));

    std::optional<double> countValue = WTF::move(*countRead);

    auto enabledRead = readDictionaryMember<bool>(context, object, "enabled"_s, path);
    if (!enabledRead)
        return makeUnexpected(WTF::move(enabledRead.error()));

    std::optional<bool> enabledValue = WTF::move(*enabledRead);

    auto tagsRead = readDictionaryMember<Vector<String>>(context, object, "tags"_s, path);
    if (!tagsRead)
        return makeUnexpected(WTF::move(tagsRead.error()));

    std::optional<Vector<String>> tagsValue = WTF::move(*tagsRead);

    if (!missingRequiredKeys.isEmpty())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString("it is missing required keys: "_s, formatList(missingRequiredKeys)) });

    return WebExtensionTestObjCOptions { .name = WTF::move(*nameValue), .count = WTF::move(countValue), .enabled = enabledValue.value_or(false), .tags = WTF::move(tagsValue) };
}

JSClassRef JSWebExtensionAPITestObjC::testObjCClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestObjC";
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

JSClassRef JSWebExtensionAPITestObjC::testObjCGlobalObjectClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestObjC";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

const JSStaticFunction* JSWebExtensionAPITestObjC::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "setDictionary", setDictionary, kJSPropertyAttributeNone },
        { "getNullableDictionary", getNullableDictionary, kJSPropertyAttributeNone },
        { "enable", enable, kJSPropertyAttributeNone },
        { "registerScripts", registerScripts, kJSPropertyAttributeNone },
        { "create", create, kJSPropertyAttributeNone },
        { "get", get, kJSPropertyAttributeNone },
        { "getManifest", getManifest, kJSPropertyAttributeNone },
        { "postMessage", postMessage, kJSPropertyAttributeNone },
        { "getMessage", getMessage, kJSPropertyAttributeNone },
        { "runTests", runTests, kJSPropertyAttributeNone },
        { "configure", configure, kJSPropertyAttributeNone },
        { "openPopup", openPopup, kJSPropertyAttributeNone },
        { "createMenu", createMenu, kJSPropertyAttributeNone },
        #if ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)
        { "notify", notify, kJSPropertyAttributeNone },
        #endif
        { "addListener", addListener, kJSPropertyAttributeNone },
        { "removeListener", removeListener, kJSPropertyAttributeNone },
        { "hasListener", hasListener, kJSPropertyAttributeNone },
        { nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return functions;
}

const JSStaticValue* JSWebExtensionAPITestObjC::staticValues()
{
    static const JSStaticValue values[] = {
        { "onChanged", onChanged, nullptr, kJSPropertyAttributeNone },
        { "tabId", tabId, nullptr, kJSPropertyAttributeNone },
        { "ROOT_NODE_ID", rootNodeIdentifier, nullptr, kJSPropertyAttributeNone },
        { nullptr, nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return values;
}

const JSStaticValue* JSWebExtensionAPITestObjC::staticConstants()
{
    return nullptr;
}

bool JSWebExtensionAPITestObjC::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, testObjCClassSingleton());
}

void JSWebExtensionAPITestObjC::getPropertyNames(JSContextRef context, JSObjectRef thisObject, JSPropertyNameAccumulatorRef propertyNames)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl) [[unlikely]]
        return;

    RefPtr page = toWebPage(context);
    bool isForMainWorld = impl->isForMainWorld();

    if (impl->isPropertyAllowed("getURL"_s, page.get()))
        SUPPRESS_UNCOUNTED_ARG JSPropertyNameAccumulatorAddName(propertyNames, toJSString("getURL"_s).get());

    if (isForMainWorld)
        SUPPRESS_UNCOUNTED_ARG JSPropertyNameAccumulatorAddName(propertyNames, toJSString("getBackgroundPage"_s).get());

#if PLATFORM(COCOA)
    if (isForMainWorld && impl->isPropertyAllowed("action"_s, page.get()))
        SUPPRESS_UNCOUNTED_ARG JSPropertyNameAccumulatorAddName(propertyNames, toJSString("action"_s).get());
#endif // PLATFORM(COCOA)

}

bool JSWebExtensionAPITestObjC::hasProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl) [[unlikely]]
        return false;

    RefPtr page = toWebPage(context);
    bool isForMainWorld = impl->isForMainWorld();

    if (JSStringIsEqualToUTF8CString(propertyName, "getURL"))
        return impl->isPropertyAllowed("getURL"_s, page.get());

    if (JSStringIsEqualToUTF8CString(propertyName, "getBackgroundPage"))
        return isForMainWorld;

#if PLATFORM(COCOA)
    if (JSStringIsEqualToUTF8CString(propertyName, "action"))
        return isForMainWorld && impl->isPropertyAllowed("action"_s, page.get());
#endif // PLATFORM(COCOA)

    return false;
}

JSValueRef JSWebExtensionAPITestObjC::getProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl) [[unlikely]]
        return JSValueMakeUndefined(context);

    RefPtr page = toWebPage(context);
    bool isForMainWorld = impl->isForMainWorld();

#if PLATFORM(COCOA)
    if (isForMainWorld && JSStringIsEqualToUTF8CString(propertyName, "action") && impl->isPropertyAllowed("action"_s, page.get()))
        return action(context, thisObject, propertyName, exception);
#endif // PLATFORM(COCOA)

    if (JSStringIsEqualToUTF8CString(propertyName, "getURL") && impl->isPropertyAllowed("getURL"_s, page.get()))
        return JSObjectMakeFunctionWithCallback(context, propertyName, getURL);

    if (isForMainWorld && JSStringIsEqualToUTF8CString(propertyName, "getBackgroundPage"))
        return JSObjectMakeFunctionWithCallback(context, propertyName, getBackgroundPage);

    return JSValueMakeUndefined(context);
}

// Functions

JSValueRef JSWebExtensionAPITestObjC::setDictionary(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.setDictionary() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.setDictionary()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    NSDictionary *options;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(isDictionary(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.setDictionary()"_s, "options"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.setDictionary()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = toNSDictionary(context, arguments[0], NullValuePolicy::NotAllowed);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        if (!(isDictionary(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.setDictionary()"_s, "options"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = toNSDictionary(context, arguments[0], NullValuePolicy::NotAllowed);
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testObjC.setDictionary()"_s, "options"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    NSString *exceptionString;
    impl->setDictionary(options, callback.releaseNonNull(), &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.setDictionary()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::getNullableDictionary(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.getNullableDictionary() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    NSDictionary *details;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(isDictionary(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.getNullableDictionary()"_s, "details"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.getNullableDictionary()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        details = toNSDictionary(context, arguments[0], NullValuePolicy::Allowed);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        if (!(isDictionary(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.getNullableDictionary()"_s, "details"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        details = toNSDictionary(context, arguments[0], NullValuePolicy::Allowed);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.getNullableDictionary()"_s, "callback"_s, "a function is expected"_s);
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

    String exceptionString;
    impl->getNullableDictionary(details, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testObjC.getNullableDictionary()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::enable(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.enable() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    NSObject *details;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.enable()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        details = toNSObject(context, arguments[0], Nil, NullValuePolicy::Allowed);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        details = toNSObject(context, arguments[0], Nil, NullValuePolicy::Allowed);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.enable()"_s, "callback"_s, "a function is expected"_s);
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

    NSString *exceptionString;
    impl->enable(details, callback.releaseNonNull(), &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.enable()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::registerScripts(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.registerScripts() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.registerScripts()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    NSArray *scripts;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(JSValueIsArray(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.registerScripts()"_s, "scripts"_s, "an array is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.registerScripts()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        scripts = toNSArray(context, arguments[0], NSDictionary.class);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        if (!(JSValueIsArray(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.registerScripts()"_s, "scripts"_s, "an array is expected"_s);
            return JSValueMakeUndefined(context);
        }

        scripts = toNSArray(context, arguments[0], NSDictionary.class);
    }

    if (!scripts) [[unlikely]] {
        *exception = toJSError(context, "testObjC.registerScripts()"_s, "scripts"_s, "an array is expected"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    NSString *exceptionString;
    impl->registerScripts(scripts, callback.releaseNonNull(), &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.registerScripts()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::create(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.create() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    ssize_t argumentIndex = -1;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.create()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    String name = nullString();
    RefPtr<JSON::Value> info;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.create()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!(isDictionary(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.create()"_s, "info"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.create()"_s, "callback"_s, "a function is expected"_s);
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
                *exception = toJSError(context, "testObjC.create()"_s, "info"_s, "an object is expected"_s);
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
        *exception = toJSError(context, "testObjC.create()"_s, "info"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex >= 0) [[unlikely]] {
        *exception = toJSError(context, "testObjC.create()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    String exceptionString;
    impl->createItem(name.createNSString().get(), info, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testObjC.create()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::get(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.get() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    RefPtr<JSON::Value> items;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.get()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        items = fromJSValue(context, arguments[0], NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        items = fromJSValue(context, arguments[0], NullValuePolicy::Allowed, ValuePolicy::StopAtTopLevel);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.get()"_s, "callback"_s, "a function is expected"_s);
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

    String exceptionString;
    impl->get(items, callback.releaseNonNull(), exceptionString);

    if (!exceptionString.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testObjC.get()"_s, nullString(), exceptionString);
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::getManifest(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.getManifest() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    SUPPRESS_UNCOUNTED_ARG return toJSValueRefOrJSNull(context, impl->getManifest());
}

JSValueRef JSWebExtensionAPITestObjC::postMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.postMessage() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.postMessage()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    if (!((!JSValueIsNull(context, arguments[0]) && !JSValueIsUndefined(context, arguments[0]) && !JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))))) [[unlikely]] {
        *exception = toJSError(context, "testObjC.postMessage()"_s, "message"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    std::optional<WebExtensionMessage> message = serializeMessage(context, arguments[0], impl->usesStructuredCloneMessageSerialization(), exception);

    if (*exception) [[unlikely]]
        return JSValueMakeUndefined(context);

    if (!message) [[unlikely]] {
        *exception = toJSError(context, "testObjC.postMessage()"_s, "message"_s, impl->usesStructuredCloneMessageSerialization() ? "a serializable value is expected"_s : "a JSON serializable value is expected"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr frame = toWebFrame(context);
    if (!frame) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Frame could not be found for JSContextRef");
        return JSValueMakeUndefined(context);
    }

    NSString *exceptionString;
    impl->postMessage(*frame, *message, &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.postMessage()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::getMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.getMessage() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.getMessage()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    String name = nullString();
    JSValue *substitutions;

    if (argumentCount == 2) {
        if (!(JSValueIsString(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.getMessage()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        name = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
        substitutions = toJSValue(context, arguments[1]);
    } else if (argumentCount == 1) {
        if (!(JSValueIsString(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.getMessage()"_s, "name"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        name = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
    }

    if (name.isEmpty()) [[unlikely]] {
        *exception = toJSError(context, "testObjC.getMessage()"_s, "name"_s, "it cannot be empty"_s);
        return JSValueMakeUndefined(context);
    }

    SUPPRESS_UNCOUNTED_ARG return toJSValueRef(context, impl->getMessage(name.createNSString().get(), substitutions), NullOrEmptyString::NullStringAsEmptyString);
}

JSValueRef JSWebExtensionAPITestObjC::getURL(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.getURL() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.getURL()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    if (!(JSValueIsString(context, arguments[0]))) [[unlikely]] {
        *exception = toJSError(context, "testObjC.getURL()"_s, "resourcePath"_s, "a string is expected"_s);
        return JSValueMakeUndefined(context);
    }

    String resourcePath = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);

    NSString *exceptionString;
    JSValueRef result = toJSValueRef(context, impl->getURL(resourcePath.createNSString().get(), &exceptionString), NullOrEmptyString::NullStringAsNull);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.getURL()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return result;
}

JSValueRef JSWebExtensionAPITestObjC::runTests(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.runTests() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.runTests()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    if (!(JSValueIsArray(context, arguments[0]))) [[unlikely]] {
        *exception = toJSError(context, "testObjC.runTests()"_s, "tests"_s, "an array is expected"_s);
        return JSValueMakeUndefined(context);
    }

    NSArray tests = toNSArray(context, arguments[0]);

    SUPPRESS_UNCOUNTED_ARG return toJSValueRefOrJSNull(context, impl->runTests(context, tests));
}

JSValueRef JSWebExtensionAPITestObjC::configure(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.configure() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.configure()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestObjCOptions, WebExtensionValueError> options = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.configure()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = WebExtensionValueParser<WebExtensionTestObjCOptions>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        options = WebExtensionValueParser<WebExtensionTestObjCOptions>::parse(context, arguments[0], { });
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testObjC.configure()"_s, "options"_s, options.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    NSString *exceptionString;
    impl->configure(*options, callback.releaseNonNull(), &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.configure()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::openPopup(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.openPopup() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    NSDictionary *options;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(isDictionary(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.openPopup()"_s, "options"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.openPopup()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = toNSDictionary(context, arguments[0], NullValuePolicy::Allowed);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        if (!(isDictionary(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.openPopup()"_s, "options"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = toNSDictionary(context, arguments[0], NullValuePolicy::Allowed);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.openPopup()"_s, "callback"_s, "a function is expected"_s);
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
            promiseResult = toJSRejectedPromise(context, "testObjC.openPopup()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    NSString *exceptionString;
    impl->openPopup(page->webPageProxyIdentifier(), options, callback.releaseNonNull(), &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.openPopup()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::createMenu(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.createMenu() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.createMenu()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    NSDictionary *properties;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(isDictionary(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.createMenu()"_s, "properties"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.createMenu()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        properties = toNSDictionary(context, arguments[0], NullValuePolicy::Allowed);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        if (!(isDictionary(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.createMenu()"_s, "properties"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        properties = toNSDictionary(context, arguments[0], NullValuePolicy::Allowed);
    }

    if (!properties) [[unlikely]] {
        *exception = toJSError(context, "testObjC.createMenu()"_s, "properties"_s, "an object is expected"_s);
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
            promiseResult = toJSRejectedPromise(context, "testObjC.createMenu()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    RefPtr frame = toWebFrame(context);
    if (!frame) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Frame could not be found for JSContextRef");
        if (promiseResult)
            promiseResult = toJSRejectedPromise(context, "testObjC.createMenu()"_s, nullString(), "an unknown error occurred"_s);
        return promiseResult ?: JSValueMakeUndefined(context);
    }

    NSString *exceptionString;
    impl->createMenu(*page, *frame, properties, callback.releaseNonNull(), &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.createMenu()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}

#if ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)
JSValueRef JSWebExtensionAPITestObjC::notify(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.notify() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    size_t argumentIndex = argumentCount;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.notify()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    String notificationId = nullString();
    NSDictionary *options;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!(JSValueIsString(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.notify()"_s, "notificationId"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!(isDictionary(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.notify()"_s, "options"_s, "an object is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testObjC.notify()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        notificationId = toString(context, arguments[0], NullStringPolicy::NullAndUndefinedAsNullString);
        options = toNSDictionary(context, arguments[1], NullValuePolicy::NotAllowed);
        callback = toJSCallbackHandler(context, arguments[2], protect(impl->runtime()));
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
                notificationId = toString(context, currentArgument, NullStringPolicy::NullAndUndefinedAsNullString);
                ++processedOptionalArgumentCount;
                ++argumentIndex;
            }
        }

        if (argumentIndex < argumentCount && (currentArgument = arguments[argumentIndex])) {
            if (!(isDictionary(context, currentArgument))) [[unlikely]] {
                *exception = toJSError(context, "testObjC.notify()"_s, "options"_s, "an object is expected"_s);
                return JSValueMakeUndefined(context);
            }

            options = toNSDictionary(context, currentArgument, NullValuePolicy::NotAllowed);
            ++argumentIndex;
        }
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testObjC.notify()"_s, "options"_s, "an object is expected"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex < argumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.notify()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    NSString *exceptionString;
    impl->notify(notificationId.createNSString().get(), options, callback.releaseNonNull(), &exceptionString);

    if (exceptionString) [[unlikely]] {
        *exception = toJSError(context, "testObjC.notify()"_s, nullString(), String(exceptionString));
        return JSValueMakeUndefined(context);
    }

    return promiseResult ?: JSValueMakeUndefined(context);
}
#endif // ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)

JSValueRef JSWebExtensionAPITestObjC::getBackgroundPage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.getBackgroundPage() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    SUPPRESS_UNCOUNTED_ARG return toJSValueRefOrJSNull(context, impl->getBackgroundPage(context));
}

JSValueRef JSWebExtensionAPITestObjC::addListener(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.addListener() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.addListener()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))))) [[unlikely]] {
        *exception = toJSError(context, "testObjC.addListener()"_s, "listener"_s, "a function is expected"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr<WebExtensionCallbackHandler> listener = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));

    if (!listener) [[unlikely]] {
        *exception = toJSError(context, "testObjC.addListener()"_s, "listener"_s, "a function is expected"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr frame = toWebFrame(context);
    if (!frame) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Frame could not be found for JSContextRef");
        return JSValueMakeUndefined(context);
    }

    impl->addListener(frame->frameID(), listener);

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::removeListener(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.removeListener() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.removeListener()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))))) [[unlikely]] {
        *exception = toJSError(context, "testObjC.removeListener()"_s, "listener"_s, "a function is expected"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr<WebExtensionCallbackHandler> listener = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));

    if (!listener) [[unlikely]] {
        *exception = toJSError(context, "testObjC.removeListener()"_s, "listener"_s, "a function is expected"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr frame = toWebFrame(context);
    if (!frame) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Frame could not be found for JSContextRef");
        return JSValueMakeUndefined(context);
    }

    impl->removeListener(frame->frameID(), listener);

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestObjC::hasListener(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestObjC(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testObjC.hasListener() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testObjC.hasListener()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))))) [[unlikely]] {
        *exception = toJSError(context, "testObjC.hasListener()"_s, "listener"_s, "a function is expected"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr<WebExtensionCallbackHandler> listener = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));

    if (!listener) [[unlikely]] {
        *exception = toJSError(context, "testObjC.hasListener()"_s, "listener"_s, "a function is expected"_s);
        return JSValueMakeUndefined(context);
    }

    SUPPRESS_UNCOUNTED_ARG return JSValueMakeBoolean(context, impl->hasListener(listener));
}

// Attributes

JSValueRef JSWebExtensionAPITestObjC::onChanged(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestObjC(context, object);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testObjC.onChanged in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));

    SUPPRESS_UNCOUNTED_ARG return toJS(context, getPtr(impl->onChanged()));
}

JSValueRef JSWebExtensionAPITestObjC::tabId(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestObjC(context, object);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testObjC.tabId in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));

    RefPtr page = toWebPage(context);
    if (!page) [[unlikely]] {
        RELEASE_LOG_ERROR(Extensions, "Page could not be found for JSContextRef");
        return JSValueMakeUndefined(context);
    }

    SUPPRESS_UNCOUNTED_ARG return JSValueMakeNumber(context, impl->tabId(*page));
}

JSValueRef JSWebExtensionAPITestObjC::rootNodeIdentifier(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestObjC(context, object);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testObjC.ROOT_NODE_ID in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));

    SUPPRESS_UNCOUNTED_ARG return toJSValueRef(context, impl->rootNodeIdentifier(), NullOrEmptyString::NullStringAsEmptyString);
}

#if PLATFORM(COCOA)
JSValueRef JSWebExtensionAPITestObjC::action(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestObjC(context, object);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testObjC.action in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));

    SUPPRESS_UNCOUNTED_ARG return toJS(context, getPtr(impl->action()));
}
#endif // PLATFORM(COCOA)

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
