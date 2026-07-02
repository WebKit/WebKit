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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Protected.h"
#include <JavaScriptCore/JSRetainPtr.h>
#if PLATFORM(COCOA)
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif
#include <wtf/JSONValues.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebFrame;
class WebPage;

class JSWebExtensionWrappable;
class WebExtensionAPIRuntimeBase;
class WebExtensionCallbackHandler;

class JSWebExtensionWrapper {
public:
    static JSValueRef wrap(JSContextRef, JSWebExtensionWrappable*);
    static JSWebExtensionWrappable* unwrap(JSContextRef, JSValueRef);

    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);
};

class WebExtensionCallbackHandler : public RefCounted<WebExtensionCallbackHandler> {
public:
    static Ref<WebExtensionCallbackHandler> create(JSContextRef, JSObjectRef resolveFunction, JSObjectRef rejectFunction);
    static Ref<WebExtensionCallbackHandler> create(JSContextRef, JSObjectRef callbackFunction, WebExtensionAPIRuntimeBase&);
    static Ref<WebExtensionCallbackHandler> create(JSContextRef, WebExtensionAPIRuntimeBase&);

    ~WebExtensionCallbackHandler();

    JSGlobalContextRef globalContext() const { return m_globalContext.get(); }
    JSValueRef NODELETE callbackFunction() const;

    void reportError(const String&);

    JSValueRef call();
    JSValueRef call(JSValueRef argument);
    JSValueRef call(JSValueRef argumentOne, JSValueRef argumentTwo);
    JSValueRef call(JSValueRef argumentOne, JSValueRef argumentTwo, JSValueRef argumentThree);

private:
    WebExtensionCallbackHandler(JSContextRef, JSObjectRef resolveFunction, JSObjectRef rejectFunction);
    WebExtensionCallbackHandler(JSContextRef, JSObjectRef callbackFunction, WebExtensionAPIRuntimeBase&);
    WebExtensionCallbackHandler(JSContextRef, WebExtensionAPIRuntimeBase&);

    JSObjectRef m_callbackFunction = nullptr;
    JSObjectRef m_rejectFunction = nullptr;
    JSRetainPtr<JSGlobalContextRef> m_globalContext;
    RefPtr<WebExtensionAPIRuntimeBase> m_runtime;
};

enum class NullStringPolicy : uint8_t {
    NoNullString,
    NullAsNullString,
    NullAndUndefinedAsNullString
};

enum class NullOrEmptyString : bool {
    NullStringAsNull,
    NullStringAsEmptyString
};

enum class NullValuePolicy : bool {
    NotAllowed,
    Allowed,
};

enum class ValuePolicy : bool {
    Recursive,
    StopAtTopLevel,
};

RefPtr<WebFrame> toWebFrame(JSContextRef);
RefPtr<WebPage> toWebPage(JSContextRef);

inline JSRetainPtr<JSStringRef> toJSString(const String& string)
{
    return JSRetainPtr(Adopt, JSStringCreateWithUTF8CString(!string.isEmpty() ? string.utf8().data() : ""));
}

inline JSValueRef toJSValueRefOrJSNull(JSContextRef context, JSValueRef value)
{
    ASSERT(context);
    return value ? value : JSValueMakeNull(context);
}

inline JSValueRef toJS(JSContextRef context, JSWebExtensionWrappable* impl)
{
    return JSWebExtensionWrapper::wrap(context, impl);
}

inline Ref<WebExtensionCallbackHandler> toJSPromiseCallbackHandler(JSContextRef context, JSObjectRef resolveFunction, JSObjectRef rejectFunction)
{
    return WebExtensionCallbackHandler::create(context, resolveFunction, rejectFunction);
}

inline Ref<WebExtensionCallbackHandler> toJSErrorCallbackHandler(JSContextRef context, WebExtensionAPIRuntimeBase& runtime)
{
    return WebExtensionCallbackHandler::create(context, runtime);
}

RefPtr<WebExtensionCallbackHandler> toJSCallbackHandler(JSContextRef, JSValueRef callback, WebExtensionAPIRuntimeBase&);

String toString(JSContextRef, JSValueRef, NullStringPolicy = NullStringPolicy::NullAndUndefinedAsNullString);

String toString(JSStringRef);

JSObjectRef toJSError(JSContextRef, const String&);

JSValueRef deserializeJSONString(JSContextRef, const String& jsonString);
String serializeJSObject(JSContextRef, JSValueRef, JSValueRef* exception);

String toJSONString(JSContextRef, JSValueRef);
String toSortedJSONString(JSContextRef, JSValueRef);
bool isFunction(JSContextRef, JSValueRef);
bool isDictionary(JSContextRef, JSValueRef);
bool isRegularExpression(JSContextRef, JSValueRef);
bool isThenable(JSContextRef, JSValueRef);

JSValueRef fromArray(JSContextRef, Vector<Protected<JSValueRef>>&&);
JSValueRef fromArray(JSContextRef, Vector<size_t>&&);
JSValueRef fromArray(JSContextRef, Vector<String>&&);

JSValueRef fromJSON(JSContextRef, RefPtr<JSON::Value>);
JSValueRef fromObject(JSContextRef, HashMap<String, Protected<JSValueRef>>&&);

JSValueRef toJSValueRef(JSContextRef, const String&, NullOrEmptyString = NullOrEmptyString::NullStringAsEmptyString);

JSValueRef toWindowObject(JSContextRef, WebFrame&);
JSValueRef toWindowObject(JSContextRef, WebPage&);

#ifdef __OBJC__

id toNSObject(JSContextRef, JSValueRef, Class containingObjectsOfClass = Nil, NullValuePolicy = NullValuePolicy::NotAllowed, ValuePolicy = ValuePolicy::Recursive);
NSDictionary *toNSDictionary(JSContextRef, JSValueRef, NullValuePolicy = NullValuePolicy::NotAllowed, ValuePolicy = ValuePolicy::Recursive);

JSContext *toJSContext(JSContextRef);

NSArray *toNSArray(JSContextRef, JSValueRef, Class containingObjectsOfClass = NSObject.class);
JSValue *toJSValue(JSContextRef, JSValueRef);

JSValueRef toJSValueRef(JSContextRef, id);

JSValueRef toJSValueRef(JSContextRef, NSURL *, NullOrEmptyString = NullOrEmptyString::NullStringAsEmptyString);

JSValueRef toJSValueRefOrJSNull(JSContextRef, id);

#endif // __OBJC__

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
