/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebAutomationSessionProxy.h"

#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebAutomationSessionProxyScriptSource.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRefPrivate.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/UUID.h>

namespace WebKit {

static inline JSRetainPtr<JSStringRef> toJSString(const String& string)
{
    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::create(string).leakRef());
}

static inline JSValueRef toJSValue(JSContextRef context, const String& string)
{
    return JSValueMakeString(context, toJSString(string).get());
}

WebAutomationSessionProxy::WebAutomationSessionProxy(const String& sessionIdentifier)
    : m_sessionIdentifier(sessionIdentifier)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebAutomationSessionProxy::messageReceiverName(), *this);
}

WebAutomationSessionProxy::~WebAutomationSessionProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebAutomationSessionProxy::messageReceiverName());
}

static JSValueRef evaluate(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT_ARG(argumentCount, argumentCount == 1);
    ASSERT_ARG(arguments, JSValueIsString(context, arguments[0]));

    if (argumentCount != 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> script(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    return JSEvaluateScript(context, script.get(), nullptr, nullptr, 0, exception);
}

static JSValueRef createUUID(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return toJSValue(context, WebCore::createCanonicalUUIDString().convertToASCIIUppercase());
}

JSObjectRef WebAutomationSessionProxy::scriptObjectForFrame(WebFrame& frame)
{
    if (JSObjectRef scriptObject = m_webFrameScriptObjectMap.get(frame.frameID()))
        return scriptObject;

    JSValueRef exception = nullptr;
    JSGlobalContextRef context = frame.jsContext();

    JSValueRef sessionIdentifier = toJSValue(context, m_sessionIdentifier);
    JSObjectRef evaluateFunction = JSObjectMakeFunctionWithCallback(context, nullptr, evaluate);
    JSObjectRef createUUIDFunction = JSObjectMakeFunctionWithCallback(context, nullptr, createUUID);

    String script = StringImpl::createWithoutCopying(WebAutomationSessionProxyScriptSource, sizeof(WebAutomationSessionProxyScriptSource));

    JSObjectRef scriptObjectFunction = const_cast<JSObjectRef>(JSEvaluateScript(context, toJSString(script).get(), nullptr, nullptr, 0, &exception));
    ASSERT(JSValueIsObject(context, scriptObjectFunction));

    JSValueRef arguments[] = { sessionIdentifier, evaluateFunction, createUUIDFunction };
    JSObjectRef scriptObject = const_cast<JSObjectRef>(JSObjectCallAsFunction(context, scriptObjectFunction, nullptr, WTF_ARRAY_LENGTH(arguments), arguments, &exception));
    ASSERT(JSValueIsObject(context, scriptObject));

    JSValueProtect(context, scriptObject);
    m_webFrameScriptObjectMap.add(frame.frameID(), scriptObject);

    return scriptObject;
}

void WebAutomationSessionProxy::didClearWindowObjectForFrame(WebFrame& frame)
{
    if (JSObjectRef scriptObject = m_webFrameScriptObjectMap.take(frame.frameID()))
        JSValueUnprotect(frame.jsContext(), scriptObject);
}

} // namespace WebKit
