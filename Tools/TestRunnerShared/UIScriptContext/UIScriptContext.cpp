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

#include "UIScriptController.h"
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <WebCore/FloatRect.h>

using namespace WTR;

static inline bool isPersistentCallbackID(unsigned callbackID)
{
    return callbackID < firstNonPersistentCallbackID;
}

UIScriptContext::UIScriptContext(UIScriptContextDelegate& delegate, UIScriptControllerFactory factory)
    : m_context(adopt(JSGlobalContextCreate(nullptr)))
    , m_delegate(delegate)
{
    m_controller = factory(*this);

    JSObjectRef globalObject = JSContextGetGlobalObject(m_context.get());

    JSValueRef exception = nullptr;
    m_controller->makeWindowObject(m_context.get(), globalObject, &exception);
}

UIScriptContext::~UIScriptContext()
{
    m_controller->waitForOutstandingCallbacks();
    m_controller->contextDestroyed();
}

void UIScriptContext::runUIScript(const String& script, unsigned scriptCallbackID)
{
    m_currentScriptCallbackID = scriptCallbackID;

    auto stringRef = adopt(JSStringCreateWithUTF8CString(script.utf8().data()));

    JSValueRef exception = nullptr;
    JSValueRef result = JSEvaluateScript(m_context.get(), stringRef.get(), 0, 0, 1, &exception);
    
    if (!hasOutstandingAsyncTasks()) {
        JSValueRef stringifyException = nullptr;
        auto stringified = adopt(JSValueToStringCopy(m_context.get(), result, &stringifyException));
        requestUIScriptCompletion(stringified.get());
        tryToCompleteUIScriptForCurrentParentCallback();
    }
}

unsigned UIScriptContext::nextTaskCallbackID(CallbackType type)
{
    if (type == CallbackTypeNonPersistent)
        return ++m_nextTaskCallbackID + firstNonPersistentCallbackID;

    return type;
}

unsigned UIScriptContext::prepareForAsyncTask(JSValueRef callback, CallbackType type)
{
    unsigned callbackID = nextTaskCallbackID(type);
    
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
    
    tryToCompleteUIScriptForCurrentParentCallback();
    m_currentScriptCallbackID = 0;
}

unsigned UIScriptContext::registerCallback(JSValueRef taskCallback, CallbackType type)
{
    if (m_callbacks.contains(type))
        unregisterCallback(type);

    if (JSValueIsUndefined(m_context.get(), taskCallback))
        return 0;

    return prepareForAsyncTask(taskCallback, type);
}

void UIScriptContext::unregisterCallback(unsigned callbackID)
{
    Task task = m_callbacks.take(callbackID);
    ASSERT(task.callback);
    JSValueUnprotect(m_context.get(), task.callback);
}

JSValueRef UIScriptContext::callbackWithID(unsigned callbackID)
{
    Task task = m_callbacks.get(callbackID);
    return task.callback;
}

void UIScriptContext::fireCallback(unsigned callbackID)
{
    Task task = m_callbacks.get(callbackID);
    ASSERT(task.callback);

    JSValueRef exception = nullptr;
    JSObjectRef callbackObject = JSValueToObject(m_context.get(), task.callback, &exception);

    m_currentScriptCallbackID = task.parentScriptCallbackID;

    exception = nullptr;
    JSObjectCallAsFunction(m_context.get(), callbackObject, JSContextGetGlobalObject(m_context.get()), 0, nullptr, &exception);
    
    tryToCompleteUIScriptForCurrentParentCallback();
    m_currentScriptCallbackID = 0;
}

void UIScriptContext::requestUIScriptCompletion(JSStringRef result)
{
    ASSERT(m_currentScriptCallbackID);
    if (currentParentCallbackIsPendingCompletion())
        return;

    // This request for the UI script to complete is not fulfilled until the last non-persistent task for the parent callback is finished.
    m_uiScriptResultsPendingCompletion.add(m_currentScriptCallbackID, result ? JSStringRetain(result) : nullptr);
}

void UIScriptContext::tryToCompleteUIScriptForCurrentParentCallback()
{
    if (!currentParentCallbackIsPendingCompletion() || currentParentCallbackHasOutstandingAsyncTasks())
        return;

    JSStringRef result = m_uiScriptResultsPendingCompletion.take(m_currentScriptCallbackID);
    String scriptResult(reinterpret_cast<const UChar*>(JSStringGetCharactersPtr(result)), JSStringGetLength(result));

    m_delegate.uiScriptDidComplete(scriptResult, m_currentScriptCallbackID);
    
    // Unregister tasks associated with this callback
    m_callbacks.removeIf([&](auto& keyAndValue) {
        return keyAndValue.value.parentScriptCallbackID == m_currentScriptCallbackID;
    });
    
    m_currentScriptCallbackID = 0;
    if (result)
        JSStringRelease(result);
}

JSObjectRef UIScriptContext::objectFromRect(const WebCore::FloatRect& rect) const
{
    JSObjectRef object = JSObjectMake(m_context.get(), nullptr, nullptr);

    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("left")).get(), JSValueMakeNumber(m_context.get(), rect.x()), kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("top")).get(), JSValueMakeNumber(m_context.get(), rect.y()), kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("width")).get(), JSValueMakeNumber(m_context.get(), rect.width()), kJSPropertyAttributeNone, nullptr);
    JSObjectSetProperty(m_context.get(), object, adopt(JSStringCreateWithUTF8CString("height")).get(), JSValueMakeNumber(m_context.get(), rect.height()), kJSPropertyAttributeNone, nullptr);
    
    return object;
}

bool UIScriptContext::currentParentCallbackHasOutstandingAsyncTasks() const
{
    for (auto entry : m_callbacks) {
        unsigned callbackID = entry.key;
        Task task = entry.value;
        if (task.parentScriptCallbackID == m_currentScriptCallbackID && !isPersistentCallbackID(callbackID))
            return true;
    }

    return false;
}

