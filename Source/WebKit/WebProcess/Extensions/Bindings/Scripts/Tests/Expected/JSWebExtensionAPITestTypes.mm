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

#include "JSWebExtensionAPITestTypes.h"
#include "Logging.h"
#include "WebExtensionAPITestTypes.h"
#include "WebExtensionUtilities.h"
#include "WebPage.h"
#include <wtf/GetPtr.h>

namespace WebKit {

WebExtensionAPITestTypes* toWebExtensionAPITestTypes(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSWebExtensionAPITestTypes::testTypesClassSingleton() || !JSValueIsObjectOfClass(context, value, JSWebExtensionAPITestTypes::testTypesClassSingleton()))
        return nullptr;
    return downcast<WebExtensionAPITestTypes>(JSWebExtensionWrapper::unwrap(context, value));
}

static constexpr std::array<ASCIILiteral, 2> webExtensionTestItemKindValues { "bookmark"_s, "folder"_s };

std::expected<WebExtensionTestItemKind, WebExtensionValueError> WebExtensionValueParser<WebExtensionTestItemKind>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto index = parseWebExtensionEnumValue(context, value, path, webExtensionTestItemKindValues);
    if (!index)
        return makeUnexpected(WTF::move(index.error()));

    return static_cast<WebExtensionTestItemKind>(*index);
}

std::expected<WebExtensionTestItemProperties, WebExtensionValueError> parseWebExtensionTestItemProperties(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);
    Vector<String> missingRequiredKeys;

    auto urlsRead = readDictionaryMember<Variant<String, Vector<String>>>(context, object, "urls"_s, path);
    if (!urlsRead)
        return makeUnexpected(WTF::move(urlsRead.error()));

    std::optional<Variant<String, Vector<String>>> urlsValue = WTF::move(*urlsRead);

    auto indexRead = readDictionaryNullableMember<double>(context, object, "index"_s, path);
    if (!indexRead)
        return makeUnexpected(WTF::move(indexRead.error()));

    std::optional<std::optional<double>> indexValue = WTF::move(*indexRead);

    auto positionRead = readDictionaryMember<Variant<double, String>>(context, object, "position"_s, path);
    if (!positionRead)
        return makeUnexpected(WTF::move(positionRead.error()));

    std::optional<Variant<double, String>> positionValue = WTF::move(*positionRead);

    if (!positionValue)
        missingRequiredKeys.append("position"_s);

    auto payloadRead = readDictionaryMemberKeepingNull<WebExtensionJSONValue>(context, object, "payload"_s, path);
    if (!payloadRead)
        return makeUnexpected(WTF::move(payloadRead.error()));

    std::optional<WebExtensionJSONValue> payloadValue = WTF::move(*payloadRead);

    auto headersRead = readDictionaryMember<HashMap<String, String>>(context, object, "headers"_s, path);
    if (!headersRead)
        return makeUnexpected(WTF::move(headersRead.error()));

    std::optional<HashMap<String, String>> headersValue = WTF::move(*headersRead);

    auto formDataRead = readDictionaryMember<HashMap<String, Vector<String>>>(context, object, "formData"_s, path);
    if (!formDataRead)
        return makeUnexpected(WTF::move(formDataRead.error()));

    std::optional<HashMap<String, Vector<String>>> formDataValue = WTF::move(*formDataRead);

    auto extrasRead = readDictionaryMember<HashMap<String, Protected<JSValueRef>>>(context, object, "extras"_s, path);
    if (!extrasRead)
        return makeUnexpected(WTF::move(extrasRead.error()));

    std::optional<HashMap<String, Protected<JSValueRef>>> extrasValue = WTF::move(*extrasRead);

    auto tabIdRead = readDictionaryMember<WebExtensionTabIdentifier>(context, object, "tabId"_s, path);
    if (!tabIdRead)
        return makeUnexpected(WTF::move(tabIdRead.error()));

    std::optional<WebExtensionTabIdentifier> tabIdValue = WTF::move(*tabIdRead);

    auto windowIdRead = readDictionaryNullableMember<WebExtensionWindowIdentifier>(context, object, "windowId"_s, path);
    if (!windowIdRead)
        return makeUnexpected(WTF::move(windowIdRead.error()));

    std::optional<std::optional<WebExtensionWindowIdentifier>> windowIdValue = WTF::move(*windowIdRead);

    auto frameIdRead = readDictionaryMember<WebExtensionFrameIdentifier>(context, object, "frameId"_s, path);
    if (!frameIdRead)
        return makeUnexpected(WTF::move(frameIdRead.error()));

    std::optional<WebExtensionFrameIdentifier> frameIdValue = WTF::move(*frameIdRead);

    auto documentIdRead = readDictionaryMember<UUID>(context, object, "documentId"_s, path);
    if (!documentIdRead)
        return makeUnexpected(WTF::move(documentIdRead.error()));

    std::optional<UUID> documentIdValue = WTF::move(*documentIdRead);

    auto storeIdRead = readDictionaryMember<WebExtensionCookieStoreIdentifier>(context, object, "storeId"_s, path);
    if (!storeIdRead)
        return makeUnexpected(WTF::move(storeIdRead.error()));

    std::optional<WebExtensionCookieStoreIdentifier> storeIdValue = WTF::move(*storeIdRead);

    auto patternRead = readDictionaryMember<Ref<WebExtensionMatchPattern>>(context, object, "pattern"_s, path);
    if (!patternRead)
        return makeUnexpected(WTF::move(patternRead.error()));

    std::optional<Ref<WebExtensionMatchPattern>> patternValue = WTF::move(*patternRead);

    auto ruleRead = readDictionaryMember<WebExtensionJSONObject>(context, object, "rule"_s, path);
    if (!ruleRead)
        return makeUnexpected(WTF::move(ruleRead.error()));

    std::optional<WebExtensionJSONObject> ruleValue = WTF::move(*ruleRead);

    auto addressRead = readDictionaryMember<URL>(context, object, "address"_s, path);
    if (!addressRead)
        return makeUnexpected(WTF::move(addressRead.error()));

    std::optional<URL> addressValue = WTF::move(*addressRead);

    auto idRead = readDictionaryMember<String>(context, object, "id"_s, path);
    if (!idRead)
        return makeUnexpected(WTF::move(idRead.error()));

    std::optional<String> idValue = WTF::move(*idRead);

    auto kindRead = readDictionaryMember<WebExtensionTestItemKind>(context, object, "kind"_s, path);
    if (!kindRead)
        return makeUnexpected(WTF::move(kindRead.error()));

    std::optional<WebExtensionTestItemKind> kindValue = WTF::move(*kindRead);

    if (!missingRequiredKeys.isEmpty())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString("it is missing required keys: "_s, formatList(missingRequiredKeys)) });

    if (addressValue && addressValue->isEmpty())
        return makeUnexpected(WebExtensionValueError { "address"_s, "it must not be empty"_s });

    return WebExtensionTestItemProperties { .urls = WTF::move(urlsValue), .index = WTF::move(indexValue), .position = WTF::move(*positionValue), .payload = WTF::move(payloadValue), .headers = WTF::move(headersValue), .formData = WTF::move(formDataValue), .extras = WTF::move(extrasValue), .tabId = WTF::move(tabIdValue), .windowId = WTF::move(windowIdValue), .frameId = WTF::move(frameIdValue), .documentId = WTF::move(documentIdValue), .storeId = WTF::move(storeIdValue), .pattern = WTF::move(patternValue), .rule = WTF::move(ruleValue), .address = WTF::move(addressValue), .identifier = WTF::move(idValue), .kind = kindValue.value_or(WebExtensionTestItemKind::Folder) };
}

JSClassRef JSWebExtensionAPITestTypes::testTypesClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestTypes";
        definition.parentClass = nullptr;
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

JSClassRef JSWebExtensionAPITestTypes::testTypesGlobalObjectClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestTypes";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

const JSStaticFunction* JSWebExtensionAPITestTypes::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "add", add, kJSPropertyAttributeNone },
        { "find", find, kJSPropertyAttributeNone },
        { "tag", tag, kJSPropertyAttributeNone },
        { "select", select, kJSPropertyAttributeNone },
        { "forget", forget, kJSPropertyAttributeNone },
        { "sync", sync, kJSPropertyAttributeNone },
        { "use", use, kJSPropertyAttributeNone },
        { "open", open, kJSPropertyAttributeNone },
        { nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return functions;
}

const JSStaticValue* JSWebExtensionAPITestTypes::staticValues()
{
    return nullptr;
}

const JSStaticValue* JSWebExtensionAPITestTypes::staticConstants()
{
    return nullptr;
}

bool JSWebExtensionAPITestTypes::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, testTypesClassSingleton());
}

// Functions

JSValueRef JSWebExtensionAPITestTypes::add(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.add() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    ssize_t argumentIndex = -1;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testTypes.add()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestItemProperties, WebExtensionValueError> item = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    String parentId = nullString();
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!(JSValueIsString(context, arguments[1]) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.add()"_s, "parentId"_s, "a string is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.add()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        item = WebExtensionValueParser<WebExtensionTestItemProperties>::parse(context, arguments[0], { });
        parentId = toString(context, arguments[1], NullStringPolicy::NullAndUndefinedAsNullString);
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

        if (argumentIndex >= 0 && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (JSValueIsString(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                parentId = toString(context, currentArgument, NullStringPolicy::NullAndUndefinedAsNullString);
                ++processedOptionalArgumentCount;
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            item = WebExtensionValueParser<WebExtensionTestItemProperties>::parse(context, currentArgument, { });
            --argumentIndex;
        }
    }

    if (!item) [[unlikely]] {
        *exception = toJSError(context, "testTypes.add()"_s, "item"_s, item.error());
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex >= 0) [[unlikely]] {
        *exception = toJSError(context, "testTypes.add()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->add(*item, parentId.createNSString().get(), callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestTypes::find(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.find() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    std::expected<std::optional<WebExtensionTestItemProperties>, WebExtensionValueError> query;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.find()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        query = parseOptionalWebExtensionValue<WebExtensionTestItemProperties>(context, arguments[0]);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        query = parseOptionalWebExtensionValue<WebExtensionTestItemProperties>(context, arguments[0]);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.find()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        callback = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));
    }

    if (!query) [[unlikely]] {
        *exception = toJSError(context, "testTypes.find()"_s, "query"_s, query.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->find(*query, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestTypes::tag(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.tag() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testTypes.tag()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<Variant<String, Vector<String>>, WebExtensionValueError> names = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.tag()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        names = WebExtensionValueParser<Variant<String, Vector<String>>>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        names = WebExtensionValueParser<Variant<String, Vector<String>>>::parse(context, arguments[0], { });
    }

    if (!names) [[unlikely]] {
        *exception = toJSError(context, "testTypes.tag()"_s, "names"_s, names.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->tag(*names, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestTypes::select(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.select() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    ssize_t argumentIndex = -1;
    std::expected<std::optional<WebExtensionTabIdentifier>, WebExtensionValueError> tabId;
    std::expected<std::optional<Variant<double, String>>, WebExtensionValueError> target;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.select()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        tabId = parseOptionalWebExtensionValue<WebExtensionTabIdentifier>(context, arguments[0]);
        target = parseOptionalWebExtensionValue<Variant<double, String>>(context, arguments[1]);
        callback = toJSCallbackHandler(context, arguments[2], protect(impl->runtime()));
    } else {
        JSValueRef currentArgument = nullptr;
        argumentIndex = argumentCount - 1;

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            if ((JSValueIsObject(context, currentArgument) && JSObjectIsFunction(context, JSValueToObject(context, currentArgument, nullptr))) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                callback = toJSCallbackHandler(context, currentArgument, protect(impl->runtime()));
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            if (WebExtensionValueParser<Variant<double, String>>::matches(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                target = parseOptionalWebExtensionValue<Variant<double, String>>(context, currentArgument);
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            if (WebExtensionValueParser<WebExtensionTabIdentifier>::matches(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                tabId = parseOptionalWebExtensionValue<WebExtensionTabIdentifier>(context, currentArgument);
                --argumentIndex;
            }
        }
    }

    if (!tabId) [[unlikely]] {
        *exception = toJSError(context, "testTypes.select()"_s, "tabId"_s, tabId.error());
        return JSValueMakeUndefined(context);
    }

    if (!target) [[unlikely]] {
        *exception = toJSError(context, "testTypes.select()"_s, "target"_s, target.error());
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex >= 0) [[unlikely]] {
        *exception = toJSError(context, "testTypes.select()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->select(*tabId, *target, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestTypes::forget(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.forget() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testTypes.forget()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<Vector<Ref<WebExtensionMatchPattern>>, WebExtensionValueError> patterns = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.forget()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        patterns = WebExtensionValueParser<Vector<Ref<WebExtensionMatchPattern>>>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        patterns = WebExtensionValueParser<Vector<Ref<WebExtensionMatchPattern>>>::parse(context, arguments[0], { });
    }

    if (!patterns) [[unlikely]] {
        *exception = toJSError(context, "testTypes.forget()"_s, "patterns"_s, patterns.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->forget(*patterns, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestTypes::sync(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.sync() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testTypes.sync()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<HashMap<String, Vector<String>>, WebExtensionValueError> headers = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.sync()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        headers = WebExtensionValueParser<HashMap<String, Vector<String>>>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        headers = WebExtensionValueParser<HashMap<String, Vector<String>>>::parse(context, arguments[0], { });
    }

    if (!headers) [[unlikely]] {
        *exception = toJSError(context, "testTypes.sync()"_s, "headers"_s, headers.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->sync(*headers, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestTypes::use(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.use() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    ssize_t argumentIndex = -1;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testTypes.use()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionCookieStoreIdentifier, WebExtensionValueError> storeId = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    std::expected<std::optional<WebExtensionFrameIdentifier>, WebExtensionValueError> frameId;
    std::expected<std::optional<UUID>, WebExtensionValueError> documentId;

    if (argumentCount == 3) {
        storeId = WebExtensionValueParser<WebExtensionCookieStoreIdentifier>::parse(context, arguments[0], { });
        frameId = parseOptionalWebExtensionValue<WebExtensionFrameIdentifier>(context, arguments[1]);
        documentId = parseOptionalWebExtensionValue<UUID>(context, arguments[2]);
    } else {
        JSValueRef currentArgument = nullptr;
        size_t allowedOptionalArgumentCount = argumentCount - requiredArgumentCount;
        size_t processedOptionalArgumentCount = 0;
        argumentIndex = argumentCount - 1;

        if (argumentIndex >= 0 && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (WebExtensionValueParser<UUID>::matches(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                documentId = parseOptionalWebExtensionValue<UUID>(context, currentArgument);
                ++processedOptionalArgumentCount;
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && processedOptionalArgumentCount < allowedOptionalArgumentCount && (currentArgument = arguments[argumentIndex])) {
            if (WebExtensionValueParser<WebExtensionFrameIdentifier>::matches(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                frameId = parseOptionalWebExtensionValue<WebExtensionFrameIdentifier>(context, currentArgument);
                ++processedOptionalArgumentCount;
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            storeId = WebExtensionValueParser<WebExtensionCookieStoreIdentifier>::parse(context, currentArgument, { });
            --argumentIndex;
        }
    }

    if (!storeId) [[unlikely]] {
        *exception = toJSError(context, "testTypes.use()"_s, "storeId"_s, storeId.error());
        return JSValueMakeUndefined(context);
    }

    if (!frameId) [[unlikely]] {
        *exception = toJSError(context, "testTypes.use()"_s, "frameId"_s, frameId.error());
        return JSValueMakeUndefined(context);
    }

    if (!documentId) [[unlikely]] {
        *exception = toJSError(context, "testTypes.use()"_s, "documentId"_s, documentId.error());
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex >= 0) [[unlikely]] {
        *exception = toJSError(context, "testTypes.use()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    impl->use(*storeId, storeId.createNSString().get(), *frameId, *documentId, documentId.createNSString().get());

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestTypes::open(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestTypes(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testTypes.open() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testTypes.open()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<URL, WebExtensionValueError> address = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testTypes.open()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        address = WebExtensionValueParser<URL>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        address = WebExtensionValueParser<URL>::parse(context, arguments[0], { });
    }

    if (!address) [[unlikely]] {
        *exception = toJSError(context, "testTypes.open()"_s, "address"_s, address.error());
        return JSValueMakeUndefined(context);
    }

    if (webExtensionValueIsEmpty(*address)) [[unlikely]] {
        *exception = toJSError(context, "testTypes.open()"_s, "address"_s, "it must not be empty"_s);
        return JSValueMakeUndefined(context);
    }

    if (webExtensionValueHasEmptyString(*address)) [[unlikely]] {
        *exception = toJSError(context, "testTypes.open()"_s, "address"_s, "it must not contain an empty string"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->open(*address, address.createNSString().get(), callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
