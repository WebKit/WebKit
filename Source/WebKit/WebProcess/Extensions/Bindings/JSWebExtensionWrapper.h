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

#pragma once

#include "Logging.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JavaScriptCore.h>
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

class WebPage;

#if ENABLE(WK_WEB_EXTENSIONS)

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
#if PLATFORM(COCOA)
    WebExtensionCallbackHandler(JSValue *callbackFunction);
    WebExtensionCallbackHandler(JSContextRef, JSObjectRef resolveFunction, JSObjectRef rejectFunction);
    WebExtensionCallbackHandler(JSContextRef, JSObjectRef callbackFunction, WebExtensionAPIRuntimeBase&);
    WebExtensionCallbackHandler(JSContextRef, WebExtensionAPIRuntimeBase&);

    JSObjectRef m_callbackFunction = nullptr;
    JSObjectRef m_rejectFunction = nullptr;
    JSRetainPtr<JSGlobalContextRef> m_globalContext;
    RefPtr<WebExtensionAPIRuntimeBase> m_runtime;
#endif
};

#endif // ENABLE(WK_WEB_EXTENSIONS)

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

inline RefPtr<WebFrame> toWebFrame(JSContextRef context)
{
    ASSERT(context);
    return WebFrame::frameForContext(JSContextGetGlobalContext(context));
}

inline RefPtr<WebPage> toWebPage(JSContextRef context)
{
    ASSERT(context);
    auto frame = toWebFrame(context);
    return frame ? frame->page() : nullptr;
}

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

#if ENABLE(WK_WEB_EXTENSIONS)

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

#endif // ENABLE(WK_WEB_EXTENSIONS)

#ifdef __OBJC__

id toNSObject(JSContextRef, JSValueRef, Class containingObjectsOfClass = Nil);
NSString *toNSString(JSContextRef, JSValueRef, NullStringPolicy = NullStringPolicy::NullAndUndefinedAsNullString);
NSDictionary *toNSDictionary(JSContextRef, JSValueRef, NullValuePolicy = NullValuePolicy::NotAllowed, ValuePolicy = ValuePolicy::Recursive);

inline NSArray *toNSArray(JSContextRef context, JSValueRef value, Class containingObjectsOfClass = NSObject.class)
{
    ASSERT(containingObjectsOfClass);
    return toNSObject(context, value, containingObjectsOfClass);
}

inline JSValue *toJSValue(JSContextRef context, JSValueRef value)
{
    ASSERT(context);

    if (!value)
        return nil;

    return [JSValue valueWithJSValueRef:value inContext:[JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(context)]];
}

inline JSValue *toWindowObject(JSContextRef context, WebFrame& frame)
{
    ASSERT(context);

    auto frameContext = frame.jsContext();
    if (!frameContext)
        return nil;

    return toJSValue(context, JSContextGetGlobalObject(frameContext));
}

inline JSValue *toWindowObject(JSContextRef context, WebPage& page)
{
    ASSERT(context);

    return toWindowObject(context, page.mainWebFrame());
}

inline JSValueRef toJSValueRef(JSContextRef context, id object)
{
    ASSERT(context);

    if (!object)
        return JSValueMakeUndefined(context);

    if (JSValue *value = dynamic_objc_cast<JSValue>(object))
        return value.JSValueRef;

    return [JSValue valueWithObject:object inContext:[JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(context)]].JSValueRef;
}

JSValueRef toJSValueRef(JSContextRef, NSString *, NullOrEmptyString = NullOrEmptyString::NullStringAsEmptyString);
JSValueRef toJSValueRef(JSContextRef, NSURL *, NullOrEmptyString = NullOrEmptyString::NullStringAsEmptyString);

NSString *toNSString(JSStringRef);

inline JSObjectRef toJSError(JSContextRef context, NSString *string)
{
    ASSERT(context);

    RELEASE_LOG_ERROR(Extensions, "Exception thrown: %{public}@", string);

    JSValueRef messageArgument = toJSValueRef(context, string, NullOrEmptyString::NullStringAsEmptyString);
    return JSObjectMakeError(context, 1, &messageArgument, nullptr);
}

inline JSRetainPtr<JSStringRef> toJSString(NSString *string)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCFString(string ? (__bridge CFStringRef)string : CFSTR("")));
}

inline JSValueRef toJSValueRefOrJSNull(JSContextRef context, id object)
{
    ASSERT(context);
    return object ? toJSValueRef(context, object) : JSValueMakeNull(context);
}

JSValueRef deserializeJSONString(JSContextRef, NSString *jsonString);
NSString *serializeJSObject(JSContextRef, JSValueRef, JSValueRef* exception);

inline bool isDictionary(JSContextRef context, JSValueRef value) { return toJSValue(context, value)._isDictionary; }

#endif // __OBJC__

} // namespace WebKit
