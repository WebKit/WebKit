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

#include "config.h"
#include "JSWebExtensionWrapper.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionWrappable.h"
#include "WebExtensionAPIRuntime.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSWeakObjectMapRefPrivate.h>

namespace WebKit {

Ref<WebExtensionCallbackHandler> WebExtensionCallbackHandler::create(JSContextRef context, JSObjectRef resolveFunction, JSObjectRef rejectFunction)
{
    return adoptRef(*new WebExtensionCallbackHandler(context, resolveFunction, rejectFunction));
}

Ref<WebExtensionCallbackHandler> WebExtensionCallbackHandler::create(JSContextRef context, JSObjectRef callbackFunction, WebExtensionAPIRuntimeBase& runtime)
{
    return adoptRef(*new WebExtensionCallbackHandler(context, callbackFunction, runtime));
}

Ref<WebExtensionCallbackHandler> WebExtensionCallbackHandler::create(JSContextRef context, WebExtensionAPIRuntimeBase& runtime)
{
    return adoptRef(*new WebExtensionCallbackHandler(context, runtime));
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

JSValueRef WebExtensionCallbackHandler::callbackFunction() const
{
    if (!m_globalContext || !m_callbackFunction)
        return nil;

    return m_callbackFunction;
}

template<size_t ArgumentCount>
JSValueRef callWithArguments(JSObjectRef callbackFunction, JSRetainPtr<JSGlobalContextRef>& globalContext, std::array<JSValueRef, ArgumentCount>&& arguments)
{
    if (!globalContext || !callbackFunction)
        return nil;
    return JSObjectCallAsFunction(globalContext.get(), callbackFunction, nullptr, ArgumentCount, arguments.data(), nullptr);
}

void WebExtensionCallbackHandler::reportError(const String& message)
{
    if (!m_globalContext)
        return;

    if (RefPtr runtime = m_runtime) {
        runtime->reportError(message, *this);
        return;
    }

    if (!m_rejectFunction)
        return;

    RELEASE_LOG_ERROR(Extensions, "Promise rejected: %" PUBLIC_LOG_STRING, message.utf8().data());

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSValueRef messageValue = JSValueMakeString(m_globalContext.get(), toJSString(message).get());
    JSValueRef error = JSObjectMakeError(m_globalContext.get(), 1, &messageValue, nullptr);

    callWithArguments<1>(m_rejectFunction, m_globalContext, { error });
}

JSValueRef WebExtensionCallbackHandler::call()
{
    return callWithArguments<0>(m_callbackFunction, m_globalContext, { });
}

JSValueRef WebExtensionCallbackHandler::call(JSValueRef argument)
{
    return callWithArguments<1>(m_callbackFunction, m_globalContext, {
        argument
    });
}

JSValueRef WebExtensionCallbackHandler::call(JSValueRef argumentOne, JSValueRef argumentTwo)
{
    return callWithArguments<2>(m_callbackFunction, m_globalContext, {
        argumentOne,
        argumentTwo
    });
}

JSValueRef WebExtensionCallbackHandler::call(JSValueRef argumentOne, JSValueRef argumentTwo, JSValueRef argumentThree)
{
    return callWithArguments<3>(m_callbackFunction, m_globalContext, {
        argumentOne,
        argumentTwo,
        argumentThree
    });
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

String toString(JSContextRef context, JSValueRef value, NullStringPolicy nullStringPolicy)
{
    ASSERT(context);
    ASSERT(value);

    switch (nullStringPolicy) {
    case NullStringPolicy::NullAndUndefinedAsNullString:
        if (JSValueIsUndefined(context, value))
            return nullString();
        [[fallthrough]];

    case NullStringPolicy::NullAsNullString:
        if (JSValueIsNull(context, value))
            return nullString();
        [[fallthrough]];

    case NullStringPolicy::NoNullString:
        // Don't try to convert other objects into strings.
        if (!JSValueIsString(context, value))
            return nullString();

        JSRetainPtr string(Adopt, JSValueToStringCopy(context, value, 0));
        return toString(string.get());
    }
}

String toString(JSStringRef string)
{
    if (!string)
        return nullString();

    Vector<char> buffer(JSStringGetMaximumUTF8CStringSize(string));
    JSStringGetUTF8CString(string, buffer.mutableSpan().data(), buffer.size());
    return String::fromUTF8(buffer.span().data());
}

JSValueRef toWindowObject(JSContextRef context, WebFrame& frame)
{
    ASSERT(context);

    auto frameContext = frame.jsContext();
    if (!frameContext)
        return JSValueMakeNull(context);

    return JSContextGetGlobalObject(frameContext) ?: JSValueMakeNull(context);
}

JSValueRef toWindowObject(JSContextRef context, WebPage& page)
{
    ASSERT(context);

    return toWindowObject(context, page.mainWebFrame());
}

JSValueRef toJSValueRef(JSContextRef context, const String& string, NullOrEmptyString nullOrEmptyString)
{
    ASSERT(context);

    switch (nullOrEmptyString) {
    case NullOrEmptyString::NullStringAsNull:
        if (string.isEmpty())
            return JSValueMakeNull(context);
        [[fallthrough]];

    case NullOrEmptyString::NullStringAsEmptyString:
        if (JSRetainPtr stringRef = toJSString(string)) {
            // This is a safer cpp false positive (rdar://163760990).
            SUPPRESS_UNCOUNTED_ARG return JSValueMakeString(context, stringRef.get());
        }

        return JSValueMakeNull(context);
    }
}

JSObjectRef toJSError(JSContextRef context, const String& string)
{
    ASSERT(context);

    RELEASE_LOG_ERROR(Extensions, "Exception thrown: %" PUBLIC_LOG_STRING, string.utf8().data());

    JSValueRef messageArgument = toJSValueRef(context, string, NullOrEmptyString::NullStringAsEmptyString);

    return JSObjectMakeError(context, 1, &messageArgument, nullptr);
}

JSValueRef deserializeJSONString(JSContextRef context, const String& jsonString)
{
    ASSERT(context);

    if (jsonString.isEmpty())
        return JSValueMakeNull(context);

    if (JSRetainPtr string = toJSString(jsonString)) {
        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG if (JSValueRef value = JSValueMakeFromJSONString(context, string.get()))
            return value;
    }

    return JSValueMakeNull(context);
}

String serializeJSObject(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
    ASSERT(context);

    if (!value)
        return nullString();

    JSRetainPtr string(Adopt, JSValueCreateJSONString(context, value, 0, exception));

    return toString(string.get());
}

static JSValueRef fromJSONArray(JSContextRef context, const JSON::Array& array)
{
    if (!context)
        return nullptr;

    if (!array)
        return JSValueMakeUndefined(context);

    auto globalContext = JSContextGetGlobalContext(context);
    Vector<Protected<JSValueRef>> retArray;
    for (Ref value : array)
        retArray.append(Protected(globalContext, fromJSON(context, value.get())));

    auto rawValues = retArray.map([](const Protected<JSValueRef>& ptr) {
        return ptr.get();
    });

    return JSObjectMakeArray(context, rawValues.size(), rawValues.span().data(), nullptr);
}

static JSValueRef fromJSONObject(JSContextRef context, const JSON::Object& object)
{
    if (!context)
        return nullptr;

    if (!object)
        return JSValueMakeUndefined(context);

    auto result = JSObjectMake(context, nullptr, nullptr);

    for (auto& key : object.keys()) {
        if (auto value = object.getValue(key)) {
            JSRetainPtr jsKey = toJSString(key);
            // This is a safer cpp false positive (rdar://163760990).
            SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, result, jsKey.get(), fromJSON(context, value), 0, nullptr);
        }
    }

    return result;
}

JSValueRef fromJSON(JSContextRef context, RefPtr<JSON::Value> value)
{
    if (!context)
        return nullptr;

    if (!value)
        return JSValueMakeUndefined(context);

    switch (value->type()) {
    case JSON::Value::Type::Boolean:
        return JSValueMakeBoolean(context, value->asBoolean().value());
    case JSON::Value::Type::String:
        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG return JSValueMakeString(context, toJSString(value->asString()).get());
    case JSON::Value::Type::Integer:
    case JSON::Value::Type::Double:
        return JSValueMakeNumber(context, value->asDouble().value());
    case JSON::Value::Type::Object:
        return fromJSONObject(context, *(value->asObject()));
    case JSON::Value::Type::Array:
        return fromJSONArray(context, *(value->asArray()));
    case JSON::Value::Type::Null:
        return JSValueMakeNull(context);
    }

    return JSValueMakeUndefined(context);
}

JSValueRef fromArray(JSContextRef context, Vector<Protected<JSValueRef>>&& array)
{
    if (!context)
        return nullptr;

    auto rawValues = array.map([](const Protected<JSValueRef>& ptr) {
        return ptr.get();
    });

    return JSObjectMakeArray(context, rawValues.size(), rawValues.span().data(), nullptr);
}

JSValueRef fromArray(JSContextRef context, Vector<size_t>&& array)
{
    if (!context)
        return nullptr;

    return fromArray(context, array.map([&context](auto num) {
        auto globalContext = JSContextGetGlobalContext(context);
        return Protected(globalContext, JSValueMakeNumber(context, num));
    }));
}

JSValueRef fromArray(JSContextRef context, Vector<String>&& array)
{
    if (!context)
        return nullptr;

    return fromArray(context, array.map([&context](auto str) {
        auto globalContext = JSContextGetGlobalContext(context);
        return Protected(globalContext, JSValueMakeString(context, toJSString(str).get()));
    }));
}

JSValueRef fromObject(JSContextRef context, HashMap<String, Protected<JSValueRef>>&& object)
{
    if (!context)
        return nullptr;

    auto result = JSObjectMake(context, nullptr, nullptr);

    for (auto& key : object.keys()) {
        JSRetainPtr jsKey = toJSString(key);
        // This is a safer cpp false positive (rdar://163760990).
        SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, result, jsKey.get(), object.get(key).get(), 0, nullptr);
    }

    return result;
}

static HashMap<JSGlobalContextRef, JSWeakObjectMapRef>& NODELETE wrapperCache()
{
    static NeverDestroyed<HashMap<JSGlobalContextRef, JSWeakObjectMapRef>> wrappers;
    return wrappers;
}

static void cacheMapDestroyed(JSWeakObjectMapRef map, void* context)
{
    wrapperCache().remove(static_cast<JSGlobalContextRef>(context));
}

static inline JSWeakObjectMapRef wrapperCacheMap(JSContextRef context)
{
    auto globalContext = JSContextGetGlobalContext(context);
    return wrapperCache().ensure(globalContext, [&] {
        return JSWeakObjectMapCreate(globalContext, globalContext, cacheMapDestroyed);
    }).iterator->value;
}

static inline JSValueRef getCachedWrapper(JSContextRef context, JSWeakObjectMapRef wrappers, JSWebExtensionWrappable* object)
{
    ASSERT(context);
    ASSERT(wrappers);
    ASSERT(object);

    if (auto wrapper = JSWeakObjectMapGet(context, wrappers, object)) {
        // Check if the wrapper is still valid. Objects invalidated through finalize
        // will not get removed from the map automatically.
        if (JSObjectGetPrivate(wrapper))
            return wrapper;

        // Remove from the map, since the object is invalid.
        JSWeakObjectMapRemove(context, wrappers, object);
    }

    return nullptr;
}

JSValueRef JSWebExtensionWrapper::wrap(JSContextRef context, JSWebExtensionWrappable* object)
{
    ASSERT(context);

    if (!object)
        return JSValueMakeNull(context);

    // JSWeakObjectMapRef is an opaque type that isn't refcounted. Static analysis can see internal refcounting APIs via internal headers, but clients aren't supposed to use those internal APIs.
    SUPPRESS_UNCOUNTED_LOCAL auto wrappers = wrapperCacheMap(context);
    if (auto result = getCachedWrapper(context, wrappers, object))
        return result;

    RefPtr objectClass = object->wrapperClass();
    ASSERT(objectClass);

    auto wrapper = JSObjectMake(context, objectClass.get(), object);
    ASSERT(wrapper);

    JSWeakObjectMapSet(context, wrappers, object, wrapper);

    return wrapper;
}

JSWebExtensionWrappable* JSWebExtensionWrapper::unwrap(JSContextRef context, JSValueRef value)
{
    ASSERT(context);
    ASSERT(value);

    if (!context || !value)
        return nullptr;

    return static_cast<JSWebExtensionWrappable*>(JSObjectGetPrivate(JSValueToObject(context, value, nullptr)));
}

static JSWebExtensionWrappable* unwrapObject(JSObjectRef object)
{
    ASSERT(object);

    ASSERT(JSObjectGetPrivate(object));
    return static_cast<JSWebExtensionWrappable*>(JSObjectGetPrivate(object));
}

void JSWebExtensionWrapper::initialize(JSContextRef, JSObjectRef object)
{
    if (RefPtr wrappable = unwrapObject(object))
        wrappable->ref();
}

void JSWebExtensionWrapper::finalize(JSObjectRef object)
{
    if (RefPtr wrappable = unwrapObject(object)) {
        JSObjectSetPrivate(object, nullptr);
        wrappable->deref();
    }
}

RefPtr<WebFrame> toWebFrame(JSContextRef context)
{
    ASSERT(context);
    return WebFrame::frameForContext(JSContextGetGlobalContext(context));
}

RefPtr<WebPage> toWebPage(JSContextRef context)
{
    ASSERT(context);
    auto frame = toWebFrame(context);
    return frame ? frame->page() : nullptr;
}

String toJSONString(JSContextRef context, JSValueRef value)
{
    if (!context)
        return nullString();

    return serializeJSObject(context, value, nullptr);
}

bool isFunction(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObject(context, value))
        return false;

    JSObjectRef functionRef = JSValueToObject(context, value, nullptr);
    return functionRef && JSObjectIsFunction(context, functionRef);
}

bool isDictionary(JSContextRef context, JSValueRef value)
{
    // Equivalent to JavaScript: this.__proto__ === Object.prototype
    if (!context || !JSValueIsObject(context, value))
        return false;

    if (isThenable(context, value))
        return false;

    JSRetainPtr protoString = toJSString("__proto__"_s);
    JSRetainPtr objectString = toJSString("Object"_s);
    JSRetainPtr prototypeString = toJSString("prototype"_s);

    JSObjectRef thisObject = JSValueToObject(context, value, nullptr);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSValueRef protoObject = JSObjectGetProperty(context, thisObject, protoString.get(), nullptr);
    SUPPRESS_UNCOUNTED_ARG JSObjectRef contextObject = JSValueToObject(context, JSObjectGetProperty(context, globalObject, objectString.get(), nullptr), nullptr);
    SUPPRESS_UNCOUNTED_ARG JSValueRef prototypeObject = JSObjectGetProperty(context, contextObject, prototypeString.get(), nullptr);

    return JSValueIsStrictEqual(context, protoObject, prototypeObject);
}

bool isRegularExpression(JSContextRef context, JSValueRef value)
{
    if (!context || !JSValueIsObject(context, value))
        return false;

    JSRetainPtr regexpString = toJSString("RegExp"_s);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSObjectRef regexpValue = JSValueToObject(context, JSObjectGetProperty(context, globalObject, regexpString.get(), nullptr), nullptr);

    return JSValueIsInstanceOfConstructor(context, value, regexpValue, nullptr);
}

bool isThenable(JSContextRef context, JSValueRef value)
{
    if (!context || !JSValueIsObject(context, value))
        return false;

    JSRetainPtr thenableString = toJSString("then"_s);
    JSObjectRef valueObject = JSValueToObject(context, value, nullptr);
    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSValueRef thenableObject = JSObjectGetProperty(context, valueObject, thenableString.get(), nullptr);

    return isFunction(context, thenableObject);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
