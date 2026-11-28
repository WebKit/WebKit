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

#include "JSWebExtensionAPITestImport.h"
#include "Logging.h"
#include "WebExtensionAPITestImport.h"
#include "WebExtensionUtilities.h"
#include "WebPage.h"
#include <wtf/GetPtr.h>

namespace WebKit {

WebExtensionAPITestImport* toWebExtensionAPITestImport(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSWebExtensionAPITestImport::testImportClassSingleton() || !JSValueIsObjectOfClass(context, value, JSWebExtensionAPITestImport::testImportClassSingleton()))
        return nullptr;
    return downcast<WebExtensionAPITestImport>(JSWebExtensionWrapper::unwrap(context, value));
}

std::expected<WebExtensionTestForest, WebExtensionValueError> parseWebExtensionTestForest(JSContextRef context, JSValueRef value, const String& path)
{
    if (!isDictionary(context, value))
        return makeUnexpected(WebExtensionValueError { constructExpectedMessage(path, "an object"_s, describeValueType(context, value)) });

    auto object = JSValueToObject(context, value, nullptr);
    Vector<String> missingRequiredKeys;

    auto modeRead = readDictionaryMember<WebExtensionTestMode>(context, object, "mode"_s, path);
    if (!modeRead)
        return makeUnexpected(WTF::move(modeRead.error()));

    std::optional<WebExtensionTestMode> modeValue = WTF::move(*modeRead);

    auto rootRead = readDictionaryMember<WebExtensionTestNode>(context, object, "root"_s, path);
    if (!rootRead)
        return makeUnexpected(WTF::move(rootRead.error()));

    std::optional<WebExtensionTestNode> rootValue = WTF::move(*rootRead);

    if (!rootValue)
        missingRequiredKeys.append("root"_s);

    auto othersRead = readDictionaryMember<Vector<WebExtensionTestNode>>(context, object, "others"_s, path);
    if (!othersRead)
        return makeUnexpected(WTF::move(othersRead.error()));

    std::optional<Vector<WebExtensionTestNode>> othersValue = WTF::move(*othersRead);

    if (!missingRequiredKeys.isEmpty())
        return makeUnexpected(WebExtensionValueError { valuePathLeaf(path), makeString("it is missing required keys: "_s, formatList(missingRequiredKeys)) });

    return WebExtensionTestForest { .mode = WTF::move(modeValue), .root = WTF::move(*rootValue), .others = WTF::move(othersValue) };
}

JSClassRef JSWebExtensionAPITestImport::testImportClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestImport";
        definition.parentClass = nullptr;
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

JSClassRef JSWebExtensionAPITestImport::testImportGlobalObjectClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestImport";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

const JSStaticFunction* JSWebExtensionAPITestImport::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "plant", plant, kJSPropertyAttributeNone },
        { "report", report, kJSPropertyAttributeNone },
        { "graft", graft, kJSPropertyAttributeNone },
        { nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return functions;
}

const JSStaticValue* JSWebExtensionAPITestImport::staticValues()
{
    return nullptr;
}

const JSStaticValue* JSWebExtensionAPITestImport::staticConstants()
{
    return nullptr;
}

bool JSWebExtensionAPITestImport::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, testImportClassSingleton());
}

// Functions

JSValueRef JSWebExtensionAPITestImport::plant(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestImport(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testImport.plant() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testImport.plant()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestForest, WebExtensionValueError> forest = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testImport.plant()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        forest = WebExtensionValueParser<WebExtensionTestForest>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        forest = WebExtensionValueParser<WebExtensionTestForest>::parse(context, arguments[0], { });
    }

    if (!forest) [[unlikely]] {
        *exception = toJSError(context, "testImport.plant()"_s, "forest"_s, forest.error());
        return JSValueMakeUndefined(context);
    }

    if (!callback)
        callback = toJSErrorCallbackHandler(context, protect(impl->runtime()));

    impl->plant(*forest, callback.releaseNonNull());

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestImport::report(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestImport(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testImport.report() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testImport.report()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestReport, WebExtensionValueError> report = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testImport.report()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        report = WebExtensionValueParser<WebExtensionTestReport>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        report = WebExtensionValueParser<WebExtensionTestReport>::parse(context, arguments[0], { });
    }

    if (!report) [[unlikely]] {
        *exception = toJSError(context, "testImport.report()"_s, "report"_s, report.error());
        return JSValueMakeUndefined(context);
    }

    if (!callback)
        callback = toJSErrorCallbackHandler(context, protect(impl->runtime()));

    impl->report(*report, callback.releaseNonNull());

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestImport::graft(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestImport(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testImport.graft() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 2;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testImport.graft()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestNode, WebExtensionValueError> node = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    std::expected<WebExtensionTestMode, WebExtensionValueError> mode = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 3) {
        if (!((JSValueIsObject(context, arguments[2]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[2], nullptr))) || JSValueIsNull(context, arguments[2]) || JSValueIsUndefined(context, arguments[2]))) [[unlikely]] {
            *exception = toJSError(context, "testImport.graft()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        node = WebExtensionValueParser<WebExtensionTestNode>::parse(context, arguments[0], { });
        mode = WebExtensionValueParser<WebExtensionTestMode>::parse(context, arguments[1], { });
        callback = toJSCallbackHandler(context, arguments[2], protect(impl->runtime()));
    } else if (argumentCount == 2) {
        node = WebExtensionValueParser<WebExtensionTestNode>::parse(context, arguments[0], { });
        mode = WebExtensionValueParser<WebExtensionTestMode>::parse(context, arguments[1], { });
    }

    if (!node) [[unlikely]] {
        *exception = toJSError(context, "testImport.graft()"_s, "node"_s, node.error());
        return JSValueMakeUndefined(context);
    }

    if (!mode) [[unlikely]] {
        *exception = toJSError(context, "testImport.graft()"_s, "mode"_s, mode.error());
        return JSValueMakeUndefined(context);
    }

    if (!callback)
        callback = toJSErrorCallbackHandler(context, protect(impl->runtime()));

    impl->graft(*node, *mode, callback.releaseNonNull());

    return JSValueMakeUndefined(context);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
