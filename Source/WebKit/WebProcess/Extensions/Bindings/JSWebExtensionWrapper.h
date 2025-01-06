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

#include <JavaScriptCore/JSRetainPtr.h>
#if PLATFORM(COCOA)
#include <JavaScriptCore/JavaScriptCore.h>
#else
#include <JavaScriptCore/JavaScript.h>
#endif
#include <wtf/WeakPtr.h>

OBJC_CLASS NSString;

#if JSC_OBJC_API_ENABLED && defined(__OBJC__)

@interface JSValue (WebKitExtras)
- (NSString *)_toJSONString;
- (NSString *)_toSortedJSONString;

@property (nonatomic, readonly, getter=_isFunction) BOOL _function;
@property (nonatomic, readonly, getter=_isDictionary) BOOL _dictionary;
@property (nonatomic, readonly, getter=_isRegularExpression) BOOL _regularExpression;
@property (nonatomic, readonly, getter=_isThenable) BOOL _thenable;

- (void)_awaitThenableResolutionWithCompletionHandler:(void (^)(JSValue *result, JSValue *error))completionHandler;
@end

#endif // JSC_OBJC_API_ENABLED && defined(__OBJC__)

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
    template<typename... Args>
    static Ref<WebExtensionCallbackHandler> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionCallbackHandler(std::forward<Args>(args)...));
    }

    ~WebExtensionCallbackHandler();

#if PLATFORM(COCOA)
    JSGlobalContextRef globalContext() const { return m_globalContext.get(); }
    JSValue *callbackFunction() const;

    void reportError(NSString *);

    id call();
    id call(id argument);
    id call(id argumentOne, id argumentTwo);
    id call(id argumentOne, id argumentTwo, id argumentThree);
#endif

private:
    WebExtensionCallbackHandler(JSValue *callbackFunction);
    WebExtensionCallbackHandler(JSContextRef, JSObjectRef resolveFunction, JSObjectRef rejectFunction);
    WebExtensionCallbackHandler(JSContextRef, JSObjectRef callbackFunction, WebExtensionAPIRuntimeBase&);
    WebExtensionCallbackHandler(JSContextRef, WebExtensionAPIRuntimeBase&);

#if PLATFORM(COCOA)
    JSObjectRef m_callbackFunction = nullptr;
    JSObjectRef m_rejectFunction = nullptr;
    JSRetainPtr<JSGlobalContextRef> m_globalContext;
    RefPtr<WebExtensionAPIRuntimeBase> m_runtime;
#endif
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

inline JSRetainPtr<JSStringRef> toJSString(const char* string)
{
    ASSERT(string);
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString(string));
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

#ifdef __OBJC__

id toNSObject(JSContextRef, JSValueRef, Class containingObjectsOfClass = Nil, NullValuePolicy = NullValuePolicy::NotAllowed, ValuePolicy = ValuePolicy::Recursive);
NSString *toNSString(JSContextRef, JSValueRef, NullStringPolicy = NullStringPolicy::NullAndUndefinedAsNullString);
NSDictionary *toNSDictionary(JSContextRef, JSValueRef, NullValuePolicy = NullValuePolicy::NotAllowed, ValuePolicy = ValuePolicy::Recursive);

JSContext *toJSContext(JSContextRef);

NSArray *toNSArray(JSContextRef, JSValueRef, Class containingObjectsOfClass = NSObject.class);
JSValue *toJSValue(JSContextRef, JSValueRef);

JSValue *toWindowObject(JSContextRef, WebFrame&);
JSValue *toWindowObject(JSContextRef, WebPage&);

JSValueRef toJSValueRef(JSContextRef, id);

JSValueRef toJSValueRef(JSContextRef, NSString *, NullOrEmptyString = NullOrEmptyString::NullStringAsEmptyString);
JSValueRef toJSValueRef(JSContextRef, NSURL *, NullOrEmptyString = NullOrEmptyString::NullStringAsEmptyString);

JSValueRef toJSValueRefOrJSNull(JSContextRef, id);

NSString *toNSString(JSStringRef);

JSObjectRef toJSError(JSContextRef, NSString *);

JSRetainPtr<JSStringRef> toJSString(NSString *);

JSValueRef deserializeJSONString(JSContextRef, NSString *jsonString);
NSString *serializeJSObject(JSContextRef, JSValueRef, JSValueRef* exception);

inline bool isDictionary(JSContextRef context, JSValueRef value) { return toJSValue(context, value)._isDictionary; }

#endif // __OBJC__

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
