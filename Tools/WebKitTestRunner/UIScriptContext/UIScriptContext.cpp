/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "UIScriptContext.h"

#include "StringFunctions.h"
#include "UIScriptController.h"
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKString.h>
#include <WebKit/WKStringPrivate.h>

using namespace WTR;

UIScriptContext::UIScriptContext(UIScriptContextDelegate& delegate)
    : m_context(Adopt, JSGlobalContextCreate(nullptr))
    , m_delegate(delegate)
{
    m_controller = UIScriptController::create(*this);

    JSObjectRef globalObject = JSContextGetGlobalObject(m_context.get());

    JSValueRef exception = nullptr;
    m_controller->makeWindowObject(m_context.get(), globalObject, &exception);
}

void UIScriptContext::runUIScript(WKStringRef script, unsigned scriptCallbackID)
{
    m_currentScriptCallbackID = scriptCallbackID;

    auto scriptRef = toJS(script);
    
    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(m_context.get(), scriptRef.get(), 0, 0, 1, &exception);
    
    if (!hasOutstandingAsyncTasks()) {
        JSValueRef stringifyException = nullptr;
        JSRetainPtr<JSStringRef> resultString(Adopt, JSValueToStringCopy(m_context.get(), result, &stringifyException));
        uiScriptComplete(resultString.get());
        m_currentScriptCallbackID = 0;
    }
}

unsigned UIScriptContext::nextTaskCallbackID()
{
    return ++m_nextTaskCallbackID;
}

unsigned UIScriptContext::prepareForAsyncTask(JSValueRef callback)
{
    unsigned callbackID = nextTaskCallbackID();
    
    JSValueProtect(m_context.get(), callback);
    Task task;
    task.parentScriptCallbackID = m_currentScriptCallbackID;
    task.callback = callback;

    ASSERT(!m_callbacks.contains(callbackID));
    m_callbacks.add(callbackID, task);
    
    return callbackID;
}

void UIScriptContext::asyncTaskComplete(unsigned callbackID)
{
    Task task = m_callbacks.take(callbackID);
    ASSERT(task.callback);

    JSValueRef exception = nullptr;
    JSObjectRef callbackObject = JSValueToObject(m_context.get(), task.callback, &exception);

    m_currentScriptCallbackID = task.parentScriptCallbackID;

    exception = nullptr;
    JSObjectCallAsFunction(m_context.get(), callbackObject, JSContextGetGlobalObject(m_context.get()), 0, nullptr, &exception);
    JSValueUnprotect(m_context.get(), task.callback);
    
    m_currentScriptCallbackID = 0;
}

void UIScriptContext::uiScriptComplete(JSStringRef result)
{
    WKRetainPtr<WKStringRef> uiScriptResult = WKStringCreateWithJSString(result);
    m_delegate.uiScriptDidComplete(uiScriptResult.get(), m_currentScriptCallbackID);
    m_currentScriptCallbackID = 0;
}

JSObjectRef UIScriptContext::objectFromRect(const WKRect& rect) const
{
    JSObjectRef object = JSObjectMake(m_context.get(), nullptr, nullptr);

    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("left")).get(), JSValueMakeNumber(m_context.get(), rect.origin.x), kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("top")).get(), JSValueMakeNumber(m_context.get(), rect.origin.y), kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("width")).get(), JSValueMakeNumber(m_context.get(), rect.size.width), kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("height")).get(), JSValueMakeNumber(m_context.get(), rect.size.height), kJSPropertyAttributeNone, nullptr);
    
    return object;
}

