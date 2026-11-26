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

#include "JSWebExtensionAPITestEnums.h"
#include "Logging.h"
#include "WebExtensionAPITestEnums.h"
#include "WebExtensionUtilities.h"
#include "WebPage.h"
#include <wtf/GetPtr.h>

namespace WebKit {

WebExtensionAPITestEnums* toWebExtensionAPITestEnums(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSWebExtensionAPITestEnums::testEnumsClassSingleton() || !JSValueIsObjectOfClass(context, value, JSWebExtensionAPITestEnums::testEnumsClassSingleton()))
        return nullptr;
    return downcast<WebExtensionAPITestEnums>(JSWebExtensionWrapper::unwrap(context, value));
}

static_assert(static_cast<size_t>(WebExtensionTestWindowState::Normal) == 0, "WebExtensionTestWindowState::Normal has to be enumerator 0");
static_assert(static_cast<size_t>(WebExtensionTestWindowState::Minimized) == 1, "WebExtensionTestWindowState::Minimized has to be enumerator 1");
static_assert(static_cast<size_t>(WebExtensionTestWindowState::Maximized) == 2, "WebExtensionTestWindowState::Maximized has to be enumerator 2");
static_assert(static_cast<size_t>(WebExtensionTestWindowState::Fullscreen) == 3, "WebExtensionTestWindowState::Fullscreen has to be enumerator 3");

static constexpr std::array<ASCIILiteral, 4> webExtensionTestWindowStateValues { "normal"_s, "minimized"_s, "maximized"_s, "fullscreen"_s };

std::expected<WebExtensionTestWindowState, WebExtensionValueError> WebExtensionValueParser<WebExtensionTestWindowState>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto index = parseWebExtensionEnumValue(context, value, path, webExtensionTestWindowStateValues);
    if (!index)
        return makeUnexpected(WTF::move(index.error()));

    return static_cast<WebExtensionTestWindowState>(*index);
}

static constexpr std::array<ASCIILiteral, 3> webExtensionTestRestrictionValues { "no-restriction"_s, "ask-user"_s, "9-by-9"_s };

std::expected<WebExtensionTestRestriction, WebExtensionValueError> WebExtensionValueParser<WebExtensionTestRestriction>::parse(JSContextRef context, JSValueRef value, const String& path)
{
    auto index = parseWebExtensionEnumValue(context, value, path, webExtensionTestRestrictionValues);
    if (!index)
        return makeUnexpected(WTF::move(index.error()));

    return static_cast<WebExtensionTestRestriction>(*index);
}

JSClassRef JSWebExtensionAPITestEnums::testEnumsClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestEnums";
        definition.parentClass = nullptr;
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

JSClassRef JSWebExtensionAPITestEnums::testEnumsGlobalObjectClassSingleton()
{
    SUPPRESS_UNCOUNTED_LOCAL static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "TestEnums";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

const JSStaticFunction* JSWebExtensionAPITestEnums::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "setState", setState, kJSPropertyAttributeNone },
        { "restrict", restrict, kJSPropertyAttributeNone },
        { nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return functions;
}

const JSStaticValue* JSWebExtensionAPITestEnums::staticValues()
{
    static const JSStaticValue values[] = {
        { "WindowState", WindowState, nullptr, kJSPropertyAttributeNone },
        { "Restriction", Restriction, nullptr, kJSPropertyAttributeNone },
        { nullptr, nullptr, nullptr, kJSPropertyAttributeNone },
    };

    return values;
}

const JSStaticValue* JSWebExtensionAPITestEnums::staticConstants()
{
    return nullptr;
}

bool JSWebExtensionAPITestEnums::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, testEnumsClassSingleton());
}

// Functions

JSValueRef JSWebExtensionAPITestEnums::setState(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestEnums(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testEnums.setState() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testEnums.setState()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestWindowState, WebExtensionValueError> state = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testEnums.setState()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        state = WebExtensionValueParser<WebExtensionTestWindowState>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        state = WebExtensionValueParser<WebExtensionTestWindowState>::parse(context, arguments[0], { });
    }

    if (!state) [[unlikely]] {
        *exception = toJSError(context, "testEnums.setState()"_s, "state"_s, state.error());
        return JSValueMakeUndefined(context);
    }

    if (!callback)
        callback = toJSErrorCallbackHandler(context, protect(impl->runtime()));

    impl->setState(*state, callback.releaseNonNull());

    return JSValueMakeUndefined(context);
}

JSValueRef JSWebExtensionAPITestEnums::restrict(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef unsafeArguments[], JSValueRef* exception)
{
    RefPtr impl = toWebExtensionAPITestEnums(context, thisObject);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called function testEnums.restrict() (%" PUBLIC_LOG "lu %" PUBLIC_LOG_STRING ") in %" PUBLIC_LOG_STRING " world", argumentCount, argumentCount == 1 ? "argument" : "arguments", makeUTF8CString(toDebugString(impl->contentWorldType())));

    auto arguments = unsafeMakeSpan(unsafeArguments, argumentCount);

    constexpr size_t requiredArgumentCount = 1;
    if (argumentCount < requiredArgumentCount) [[unlikely]] {
        *exception = toJSError(context, "testEnums.restrict()"_s, nullString(), "a required argument is missing"_s);
        return JSValueMakeUndefined(context);
    }

    std::expected<WebExtensionTestRestriction, WebExtensionValueError> restriction = makeUnexpected(WebExtensionValueError { "a required argument is missing"_s });
    RefPtr<WebExtensionCallbackHandler> callback;

    if (argumentCount == 2) {
        if (!((JSValueIsObject(context, arguments[1]) && JSObjectIsFunction(context, JSValueToObject(context, arguments[1], nullptr))) || JSValueIsNull(context, arguments[1]) || JSValueIsUndefined(context, arguments[1]))) [[unlikely]] {
            *exception = toJSError(context, "testEnums.restrict()"_s, "callback"_s, "a function is expected"_s);
            return JSValueMakeUndefined(context);
        }

        restriction = WebExtensionValueParser<WebExtensionTestRestriction>::parse(context, arguments[0], { });
        callback = toJSCallbackHandler(context, arguments[1], protect(impl->runtime()));
    } else if (argumentCount == 1) {
        restriction = WebExtensionValueParser<WebExtensionTestRestriction>::parse(context, arguments[0], { });
    }

    if (!restriction) [[unlikely]] {
        *exception = toJSError(context, "testEnums.restrict()"_s, "restriction"_s, restriction.error());
        return JSValueMakeUndefined(context);
    }

    if (!callback)
        callback = toJSErrorCallbackHandler(context, protect(impl->runtime()));

    impl->restrict(*restriction, callback.releaseNonNull());

    return JSValueMakeUndefined(context);
}

// Attributes

JSValueRef JSWebExtensionAPITestEnums::WindowState(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestEnums(context, object);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testEnums.WindowState in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));

    SUPPRESS_UNCOUNTED_ARG return makeEnumObject(context, { { "NORMAL"_s, "normal"_s }, { "MINIMIZED"_s, "minimized"_s }, { "MAXIMIZED"_s, "maximized"_s }, { "FULLSCREEN"_s, "fullscreen"_s } });
}

JSValueRef JSWebExtensionAPITestEnums::Restriction(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UNUSED_PARAM(propertyName);

    RefPtr impl = toWebExtensionAPITestEnums(context, object);
    if (!impl || !impl->isForMainWorld()) [[unlikely]]
        return JSValueMakeUndefined(context);

    RELEASE_LOG_DEBUG(Extensions, "Called getter testEnums.Restriction in %" PUBLIC_LOG_STRING " world", makeUTF8CString(toDebugString(impl->contentWorldType())));

    SUPPRESS_UNCOUNTED_ARG return makeEnumObject(context, { { "NO_RESTRICTION"_s, "no-restriction"_s }, { "ASK_USER"_s, "ask-user"_s }, { "_9_BY_9"_s, "9-by-9"_s } });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
