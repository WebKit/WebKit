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

#include "JSWebExtensionAPITestDictionaries.h"
#include "Logging.h"
#include "WebExtensionAPITestDictionaries.h"
#include "WebExtensionUtilities.h"
#include "WebPage.h"
#include <wtf/GetPtr.h>

namespace WebKit {

WebExtensionAPITestDictionaries* toWebExtensionAPITestDictionaries(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSWebExtensionAPITestDictionaries::testDictionariesClassSingleton() || !JSValueIsObjectOfClass(context, value, JSWebExtensionAPITestDictionaries::testDictionariesClassSingleton()))
        return nullptr;
    return downcast<WebExtensionAPITestDictionaries>(JSWebExtensionWrapper::unwrap(context, value));
}

static constexpr std::array<ASCIILiteral, 2> webExtensionTestModeValues { "fast"_s, "slow-mode"_s };

std::expected<WebExtensionTestMode, WebExtensionValueError> WebExtensionValueParser<WebExtensionTestMode>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto index = parseWebExtensionEnumValue(context, value, path, webExtensionTestModeValues);
    if (!index)
        return makeUnexpected(WTF::move(index.error()));

    return static_cast<WebExtensionTestMode>(*index);
}

std::expected<WebExtensionTestToggle, WebExtensionValueError> parseWebExtensionTestToggle(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);

    auto pathRead = readDictionaryMember<String>(context, object, "path"_s, path);
    if (!pathRead)
        return makeUnexpected(WTF::move(pathRead.error()));

    std::optional<String> pathValue = WTF::move(*pathRead);

    auto enabledRead = readDictionaryMember<bool>(context, object, "enabled"_s, path);
    if (!enabledRead)
        return makeUnexpected(WTF::move(enabledRead.error()));

    std::optional<bool> enabledValue = WTF::move(*enabledRead);

    if (!pathValue && !enabledValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it must specify either 'path' or 'enabled'"_s });

    return WebExtensionTestToggle { .path = WTF::move(pathValue), .enabled = WTF::move(enabledValue) };
}

std::expected<WebExtensionTestExclusiveBooleans, WebExtensionValueError> parseWebExtensionTestExclusiveBooleans(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);

    auto mutedRead = readDictionaryMember<bool>(context, object, "muted"_s, path);
    if (!mutedRead)
        return makeUnexpected(WTF::move(mutedRead.error()));

    std::optional<bool> mutedValue = WTF::move(*mutedRead);

    auto audibleRead = readDictionaryMember<bool>(context, object, "audible"_s, path);
    if (!audibleRead)
        return makeUnexpected(WTF::move(audibleRead.error()));

    std::optional<bool> audibleValue = WTF::move(*audibleRead);

    if (mutedValue && audibleValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both 'muted' and 'audible'"_s });

    return WebExtensionTestExclusiveBooleans { .muted = WTF::move(mutedValue), .audible = WTF::move(audibleValue) };
}

std::expected<WebExtensionTestTwoGroups, WebExtensionValueError> parseWebExtensionTestTwoGroups(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);

    auto allFramesRead = readDictionaryMember<bool>(context, object, "allFrames"_s, path);
    if (!allFramesRead)
        return makeUnexpected(WTF::move(allFramesRead.error()));

    std::optional<bool> allFramesValue = WTF::move(*allFramesRead);

    auto fileRead = readDictionaryMember<String>(context, object, "file"_s, path);
    if (!fileRead)
        return makeUnexpected(WTF::move(fileRead.error()));

    std::optional<String> fileValue = WTF::move(*fileRead);

    auto codeRead = readDictionaryMember<String>(context, object, "code"_s, path);
    if (!codeRead)
        return makeUnexpected(WTF::move(codeRead.error()));

    std::optional<String> codeValue = WTF::move(*codeRead);

    auto frameIdRead = readDictionaryMember<double>(context, object, "frameId"_s, path);
    if (!frameIdRead)
        return makeUnexpected(WTF::move(frameIdRead.error()));

    std::optional<double> frameIdValue = WTF::move(*frameIdRead);

    auto documentIdRead = readDictionaryMember<String>(context, object, "documentId"_s, path);
    if (!documentIdRead)
        return makeUnexpected(WTF::move(documentIdRead.error()));

    std::optional<String> documentIdValue = WTF::move(*documentIdRead);

    if (allFramesValue.value_or(false) && frameIdValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both 'allFrames' and 'frameId'"_s });

    if (allFramesValue.value_or(false) && documentIdValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both 'allFrames' and 'documentId'"_s });

    if (frameIdValue && documentIdValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both 'frameId' and 'documentId'"_s });

    if (fileValue && codeValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both 'file' and 'code'"_s });

    return WebExtensionTestTwoGroups { .allFrames = WTF::move(allFramesValue), .file = WTF::move(fileValue), .code = WTF::move(codeValue), .frameId = WTF::move(frameIdValue), .documentId = WTF::move(documentIdValue) };
}

std::expected<WebExtensionTestBase, WebExtensionValueError> parseWebExtensionTestBase(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);

    auto labelRead = readDictionaryMember<String>(context, object, "label"_s, path);
    if (!labelRead)
        return makeUnexpected(WTF::move(labelRead.error()));

    std::optional<String> labelValue = WTF::move(*labelRead);

    return WebExtensionTestBase { .label = WTF::move(labelValue) };
}

std::expected<WebExtensionTestOptions, WebExtensionValueError> parseWebExtensionTestOptions(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);

    auto labelRead = readDictionaryMember<String>(context, object, "label"_s, path);
    if (!labelRead)
        return makeUnexpected(WTF::move(labelRead.error()));

    std::optional<String> labelValue = WTF::move(*labelRead);

    auto activeRead = readDictionaryMember<bool>(context, object, "active"_s, path);
    if (!activeRead)
        return makeUnexpected(WTF::move(activeRead.error()));

    std::optional<bool> activeValue = WTF::move(*activeRead);

    auto basesRead = readDictionaryMember<Vector<WebExtensionTestBase>>(context, object, "bases"_s, path);
    if (!basesRead)
        return makeUnexpected(WTF::move(basesRead.error()));

    std::optional<Vector<WebExtensionTestBase>> basesValue = WTF::move(*basesRead);

    auto filesRead = readDictionaryMember<Vector<String>>(context, object, "files"_s, path);
    if (!filesRead)
        return makeUnexpected(WTF::move(filesRead.error()));

    std::optional<Vector<String>> filesValue = WTF::move(*filesRead);

#if ENABLE(WK_WEB_EXTENSIONS_TEST_FEATURE)
    auto experimentalRead = readDictionaryMember<bool>(context, object, "experimental"_s, path);
    if (!experimentalRead)
        return makeUnexpected(WTF::move(experimentalRead.error()));

    std::optional<bool> experimentalValue = WTF::move(*experimentalRead);

#endif
    auto extraRead = readDictionaryMember<Protected<JSValueRef>>(context, object, "extra"_s, path);
    if (!extraRead)
        return makeUnexpected(WTF::move(extraRead.error()));

    std::optional<Protected<JSValueRef>> extraValue = WTF::move(*extraRead);

    auto firstRead = readDictionaryMember<double>(context, object, "first"_s, path);
    if (!firstRead)
        return makeUnexpected(WTF::move(firstRead.error()));

    std::optional<double> firstValue = WTF::move(*firstRead);

    auto funcRead = readDictionaryFunctionMember(context, object, "func"_s, path);
    if (!funcRead)
        return makeUnexpected(WTF::move(funcRead.error()));

    std::optional<Protected<JSValueRef>> funcValue = WTF::move(*funcRead);

    auto functionRead = readDictionaryFunctionMember(context, object, "function"_s, path);
    if (!functionRead)
        return makeUnexpected(WTF::move(functionRead.error()));

    std::optional<Protected<JSValueRef>> funcAliasValue = WTF::move(*functionRead);

    auto limitRead = readDictionaryMember<uint32_t>(context, object, "limit"_s, path);
    if (!limitRead)
        return makeUnexpected(WTF::move(limitRead.error()));

    std::optional<uint32_t> limitValue = WTF::move(*limitRead);

    auto modeRead = readDictionaryMember<WebExtensionTestMode>(context, object, "mode"_s, path);
    if (!modeRead)
        return makeUnexpected(WTF::move(modeRead.error()));

    std::optional<WebExtensionTestMode> modeValue = WTF::move(*modeRead);

    auto nameRead = readDictionaryMember<String>(context, object, "name"_s, path);
    if (!nameRead)
        return makeUnexpected(WTF::move(nameRead.error()));

    std::optional<String> nameValue = WTF::move(*nameRead);

    auto qualityRead = readDictionaryMember<int32_t>(context, object, "quality"_s, path);
    if (!qualityRead)
        return makeUnexpected(WTF::move(qualityRead.error()));

    std::optional<int32_t> qualityValue = WTF::move(*qualityRead);

    auto argsRead = readDictionaryMember<Vector<Protected<JSValueRef>>>(context, object, "args"_s, path);
    if (!argsRead)
        return makeUnexpected(WTF::move(argsRead.error()));

    std::optional<Vector<Protected<JSValueRef>>> argsValue = WTF::move(*argsRead);

    auto secondRead = readDictionaryMember<double>(context, object, "second"_s, path);
    if (!secondRead)
        return makeUnexpected(WTF::move(secondRead.error()));

    std::optional<double> secondValue = WTF::move(*secondRead);

    auto thirdRead = readDictionaryMember<double>(context, object, "third"_s, path);
    if (!thirdRead)
        return makeUnexpected(WTF::move(thirdRead.error()));

    std::optional<double> thirdValue = WTF::move(*thirdRead);

    if (funcValue && funcAliasValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both 'func' and 'function'. Please use 'func'"_s });

    if (!funcValue)
        funcValue = WTF::move(funcAliasValue);

    if (firstValue && secondValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it cannot specify both 'first' and 'second'"_s });

    if (!firstValue && !secondValue && !thirdValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it must specify either 'first', 'second', or 'third'"_s });

    if (argsValue && !funcValue)
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), "it must specify both 'func' and 'args'"_s });

    if (filesValue && filesValue->isEmpty())
        return makeUnexpected(WebExtensionValueError { "files"_s, "it must not be empty"_s });

    if (nameValue && makeUTF8CString(*nameValue).length() > 16)
        return makeUnexpected(WebExtensionValueError { "name"_s, "it exceeded the maximum allowed length"_s });

    if (qualityValue && (*qualityValue < 0 || *qualityValue > 100))
        return makeUnexpected(WebExtensionValueError { "quality"_s, makeString("it must specify a value between "_s, 0, " and "_s, 100) });

    return WebExtensionTestOptions {
        .label = WTF::move(labelValue),
        .active = activeValue.value_or(true),
        .bases = WTF::move(basesValue),
        .files = WTF::move(filesValue),
#if ENABLE(WK_WEB_EXTENSIONS_TEST_FEATURE)
        .experimental = WTF::move(experimentalValue),
#endif
        .extra = WTF::move(extraValue),
        .first = WTF::move(firstValue),
        .func = WTF::move(funcValue),
        .limit = limitValue.value_or(5),
        .mode = modeValue.value_or(WebExtensionTestMode::Fast),
        .name = WTF::move(nameValue),
        .quality = WTF::move(qualityValue),
        .args = WTF::move(argsValue),
        .second = WTF::move(secondValue),
        .third = WTF::move(thirdValue),
    };
}

std::expected<WebExtensionTestNode, WebExtensionValueError> parseWebExtensionTestNode(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);
    Vector<String> missingRequiredKeys;

    auto childrenRead = readDictionaryMember<Vector<WebExtensionTestNode>>(context, object, "children"_s, path);
    if (!childrenRead)
        return makeUnexpected(WTF::move(childrenRead.error()));

    std::optional<Vector<WebExtensionTestNode>> childrenValue = WTF::move(*childrenRead);

    auto idRead = readDictionaryMember<String>(context, object, "id"_s, path);
    if (!idRead)
        return makeUnexpected(WTF::move(idRead.error()));

    std::optional<String> idValue = WTF::move(*idRead);

    if (!idValue)
        missingRequiredKeys.append("id"_s);

    if (!missingRequiredKeys.isEmpty())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString("it is missing required keys: "_s, formatList(missingRequiredKeys)) });

    return WebExtensionTestNode { .children = WTF::move(childrenValue), .id = WTF::move(*idValue) };
}

std::expected<WebExtensionTestReport, WebExtensionValueError> parseWebExtensionTestReport(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);
    Vector<String> missingRequiredKeys;

    auto titleRead = readDictionaryMember<String>(context, object, "title"_s, path);
    if (!titleRead)
        return makeUnexpected(WTF::move(titleRead.error()));

    std::optional<String> titleValue = WTF::move(*titleRead);

    if (!titleValue)
        missingRequiredKeys.append("title"_s);

    auto modeRead = readDictionaryMember<WebExtensionTestMode>(context, object, "mode"_s, path);
    if (!modeRead)
        return makeUnexpected(WTF::move(modeRead.error()));

    std::optional<WebExtensionTestMode> modeValue = WTF::move(*modeRead);

    if (!missingRequiredKeys.isEmpty())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString("it is missing required keys: "_s, formatList(missingRequiredKeys)) });

    return WebExtensionTestReport { .title = WTF::move(*titleValue), .mode = WTF::move(modeValue) };
}

JSClassRef JSWebExtensionAPITestDictionaries::testDictionariesClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestDictionaries";
        definition.parentClass = nullptr;
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

JSClassRef JSWebExtensionAPITestDictionaries::testDictionariesGlobalObjectClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestDictionaries";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

const JSStaticFunction* JSWebExtensionAPITestDictionaries::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "toggle", toggle, kJSPropertyAttributeNone },
        { "mute", mute, kJSPropertyAttributeNone },
        { "inject", inject, kJSPropertyAttributeNone },
        { "open", open, kJSPropertyAttributeNone },
        { "query", query, kJSPropertyAttributeNone },
        { "inspect", inspect, kJSPropertyAttributeNone },
        { "visit", visit, kJSPropertyAttributeNone },
        { "set", set, kJSPropertyAttributeNone },
        { "choose", choose, kJSPropertyAttributeNone },
        { "build", build, kJSPropertyAttributeNone },
        { "seek", seek, kJSPropertyAttributeNone },
        { nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return functions;
}

const JSStaticValue* JSWebExtensionAPITestDictionaries::staticValues()
{
    return nullptr;
}

const JSStaticValue* JSWebExtensionAPITestDictionaries::staticConstants()
{
    return nullptr;
}

bool JSWebExtensionAPITestDictionaries::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, testDictionariesClassSingleton());
}

// Functions

JSValueRef JSWebExtensionAPITestDictionaries::toggle(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.toggle() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.toggle()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestToggle, WebExtensionValueError> options = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.toggle()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = WebExtensionValueParser<WebExtensionTestToggle>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        options = WebExtensionValueParser<WebExtensionTestToggle>::parse(context, arguments[0], { });
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.toggle()"_s, "options"_s, options.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->toggle(*options, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::mute(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.mute() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.mute()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestExclusiveBooleans, WebExtensionValueError> options = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.mute()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = WebExtensionValueParser<WebExtensionTestExclusiveBooleans>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        options = WebExtensionValueParser<WebExtensionTestExclusiveBooleans>::parse(context, arguments[0], { });
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.mute()"_s, "options"_s, options.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->mute(*options, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::inject(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.inject() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.inject()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestTwoGroups, WebExtensionValueError> details = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.inject()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        details = WebExtensionValueParser<WebExtensionTestTwoGroups>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        details = WebExtensionValueParser<WebExtensionTestTwoGroups>::parse(context, arguments[0], { });
    }

    if (!details) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.inject()"_s, "details"_s, details.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->inject(*details, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::open(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.open() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.open()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestOptions, WebExtensionValueError> options = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.open()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        options = WebExtensionValueParser<WebExtensionTestOptions>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        options = WebExtensionValueParser<WebExtensionTestOptions>::parse(context, arguments[0], { });
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.open()"_s, "options"_s, options.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->open(*options, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::query(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.query() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    std::expected<std::optional<WebExtensionTestOptions>, WebExtensionValueError> info;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.query()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        info = parseOptionalWebExtensionValue<WebExtensionTestOptions>(context, arguments[0]);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1 && !((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) {
        info = parseOptionalWebExtensionValue<WebExtensionTestOptions>(context, arguments[0]);
    } else if (argumentCount == 1) {
        if (!((JSValueIsObject(context, arguments[0]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[0], nullptr))) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.query()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        callback = toJSCallbackHandler(context, arguments[0], protect(impl->runtime()));
    }

    if (!info) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.query()"_s, "info"_s, info.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->query(*info, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::inspect(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.inspect() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    ssize_t argumentIndex = -1;
    bool optionsMatched = false;
    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.inspect()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    double id = std::numeric_limits<double>::quiet_NaN();
    std::expected<std::optional<WebExtensionTestOptions>, WebExtensionValueError> options;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!(JSValueIsNumber(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.inspect()"_s, "id"_s, "a number is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.inspect()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        id = JSValueToNumber(context, arguments[0], nullptr);
        options = parseOptionalWebExtensionValue<WebExtensionTestOptions>(context, arguments[1]);
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
            if ((WebExtensionValueParser<WebExtensionTestOptions>::matches(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) || argumentIndex == 1) {
                options = parseOptionalWebExtensionValue<WebExtensionTestOptions>(context, currentArgument);
                ++processedOptionalArgumentCount;
                optionsMatched = true;
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            if (!(JSValueIsNumber(context, currentArgument))) [[unlikely]] {
                *exception = toJSError(context, "testDictionaries.inspect()"_s, "id"_s, "a number is expected"_s);
                return JSValueMakeUndefined(context);
            }

            id = JSValueToNumber(context, currentArgument, nullptr);
            --argumentIndex;
        }
    }

    if (!std::isfinite(id)) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.inspect()"_s, "id"_s, "a number is expected"_s);
        return JSValueMakeUndefined(context);
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.inspect()"_s, "options"_s, options.error());
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex >= 0) [[unlikely]] {
        if (argumentIndex == 1 && !optionsMatched) {
            *exception = toJSError(context, "testDictionaries.inspect()"_s, "options"_s, makeString("an object is expected, but "_s, describeValueType(context, arguments[argumentIndex]), " was provided"_s));
            return JSValueMakeUndefined(context);
        }

        if (!optionsMatched) {
            *exception = toJSError(context, "testDictionaries.inspect()"_s, "options"_s, makeString("an object is expected, but "_s, describeValueType(context, arguments[argumentIndex]), " was provided"_s));
            return JSValueMakeUndefined(context);
        }

        *exception = toJSError(context, "testDictionaries.inspect()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->inspect(id, *options, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::visit(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.visit() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    ssize_t argumentIndex = -1;
    bool optionsMatched = false;
    double tab = std::numeric_limits<double>::quiet_NaN();
    std::expected<std::optional<WebExtensionTestOptions>, WebExtensionValueError> options;
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!(JSValueIsNumber(context, arguments[0]) || JSValueIsNull(context, arguments[0]) || JSValueIsUndefined(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.visit()"_s, "tab"_s, "a number is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.visit()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        tab = JSValueToNumber(context, arguments[0], nullptr);
        options = parseOptionalWebExtensionValue<WebExtensionTestOptions>(context, arguments[1]);
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
            if ((WebExtensionValueParser<WebExtensionTestOptions>::matches(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) || argumentIndex == 1) {
                options = parseOptionalWebExtensionValue<WebExtensionTestOptions>(context, currentArgument);
                optionsMatched = true;
                --argumentIndex;
            }
        }

        if (argumentIndex >= 0 && (currentArgument = arguments[argumentIndex])) {
            if (JSValueIsNumber(context, currentArgument) || JSValueIsNull(context, currentArgument) || JSValueIsUndefined(context, currentArgument)) {
                tab = JSValueToNumber(context, currentArgument, nullptr);
                --argumentIndex;
            }
        }
    }

    if (!options) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.visit()"_s, "options"_s, options.error());
        return JSValueMakeUndefined(context);
    }

    if (argumentIndex >= 0) [[unlikely]] {
        if (argumentIndex == 1 && !optionsMatched) {
            *exception = toJSError(context, "testDictionaries.visit()"_s, "options"_s, makeString("an object is expected, but "_s, describeValueType(context, arguments[argumentIndex]), " was provided"_s));
            return JSValueMakeUndefined(context);
        }

        if (!optionsMatched) {
            *exception = toJSError(context, "testDictionaries.visit()"_s, "options"_s, makeString("an object is expected, but "_s, describeValueType(context, arguments[argumentIndex]), " was provided"_s));
            return JSValueMakeUndefined(context);
        }

        *exception = toJSError(context, "testDictionaries.visit()"_s, nullString(), "an unknown argument was provided"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->visit(tab, *options, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::set(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.set() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.set()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<Vector<WebExtensionTestBase>, WebExtensionValueError> items = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.set()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        items = WebExtensionValueParser<Vector<WebExtensionTestBase>>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        items = WebExtensionValueParser<Vector<WebExtensionTestBase>>::parse(context, arguments[0], { });
    }

    if (!items) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.set()"_s, "items"_s, items.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->set(*items, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::choose(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.choose() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.choose()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestMode, WebExtensionValueError> mode = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.choose()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        mode = WebExtensionValueParser<WebExtensionTestMode>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        mode = WebExtensionValueParser<WebExtensionTestMode>::parse(context, arguments[0], { });
    }

    if (!mode) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.choose()"_s, "mode"_s, mode.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->choose(*mode, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::build(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.build() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.build()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestNode, WebExtensionValueError> root = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.build()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        root = WebExtensionValueParser<WebExtensionTestNode>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        root = WebExtensionValueParser<WebExtensionTestNode>::parse(context, arguments[0], { });
    }

    if (!root) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.build()"_s, "root"_s, root.error());
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->build(*root, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestDictionaries::seek(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestDictionaries(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testDictionaries.seek() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.seek()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    double position = std::numeric_limits<double>::quiet_NaN();
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!(JSValueIsNumber(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.seek()"_s, "position"_s, "a number is expected"_s);
            return JSValueMakeUndefined(context);
        }

        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.seek()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        position = JSValueToNumber(context, arguments[0], nullptr);
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        if (!(JSValueIsNumber(context, arguments[0]))) [[unlikely]] {
            *exception = toJSError(context, "testDictionaries.seek()"_s, "position"_s, "a number is expected"_s);
            return JSValueMakeUndefined(context);
        }

        position = JSValueToNumber(context, arguments[0], nullptr);
    }

    if (position < 0 || position > 100) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.seek()"_s, "position"_s, makeString("it must specify a value between "_s, 0, " and "_s, 100));
        return JSValueMakeUndefined(context);
    }

    if (!std::isfinite(position)) [[unlikely]] {
        *exception = toJSError(context, "testDictionaries.seek()"_s, "position"_s, "a number is expected"_s);
        return JSValueMakeUndefined(context);
    }

    JSObjectRef promiseResult = nullptr;
    if (!callback) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        callback = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

    impl->seek(position, callback.releaseNonNull());

    return promiseResult ?: JSValueMakeUndefined(context);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
