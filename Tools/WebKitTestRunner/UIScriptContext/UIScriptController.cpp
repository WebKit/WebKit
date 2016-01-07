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
#include "UIScriptController.h"

#include "JSUIScriptController.h"
#include "UIScriptContext.h"
#include <JavaScriptCore/JSValueRef.h>

namespace WTR {

UIScriptController::UIScriptController(UIScriptContext& context)
    : m_context(&context)
{
}

void UIScriptController::contextDestroyed()
{
    m_context = nullptr;
}

void UIScriptController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "uiController", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef UIScriptController::wrapperClass()
{
    return JSUIScriptController::uIScriptControllerClass();
}

#if !PLATFORM(COCOA)
void UIScriptController::doAsyncTask(JSValueRef)
{
}
#endif

void UIScriptController::setWillBeginZoomingCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeWillBeginZooming);
    platformSetWillBeginZoomingCallback();
}

JSValueRef UIScriptController::willBeginZoomingCallback() const
{
    return m_context->callbackWithID(CallbackTypeWillBeginZooming);
}

void UIScriptController::setDidEndZoomingCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidEndZooming);
    platformSetDidEndZoomingCallback();
}

JSValueRef UIScriptController::didEndZoomingCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidEndZooming);
}

void UIScriptController::setDidEndScrollingCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidEndScrolling);
    platformSetDidEndScrollingCallback();
}

JSValueRef UIScriptController::didEndScrollingCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidEndScrolling);
}

void UIScriptController::setDidShowKeyboardCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidShowKeyboard);
    platformSetDidShowKeyboardCallback();
}

JSValueRef UIScriptController::didShowKeyboardCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidShowKeyboard);
}

void UIScriptController::setDidHideKeyboardCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidHideKeyboard);
    platformSetDidHideKeyboardCallback();
}

JSValueRef UIScriptController::didHideKeyboardCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidHideKeyboard);
}

#if !PLATFORM(IOS)
void UIScriptController::zoomToScale(double, JSValueRef)
{
}

void UIScriptController::singleTapAtPoint(long x, long y, JSValueRef)
{
}

void UIScriptController::doubleTapAtPoint(long x, long y, JSValueRef)
{
}

void UIScriptController::typeCharacterUsingHardwareKeyboard(JSStringRef, JSValueRef)
{
}

double UIScriptController::zoomScale() const
{
    return 1;
}

double UIScriptController::minimumZoomScale() const
{
    return 1;
}

double UIScriptController::maximumZoomScale() const
{
    return 1;
}

JSObjectRef UIScriptController::contentVisibleRect() const
{
    return nullptr;
}

void UIScriptController::platformSetWillBeginZoomingCallback()
{
}

void UIScriptController::platformSetDidEndZoomingCallback()
{
}

void UIScriptController::platformSetDidEndScrollingCallback()
{
}

void UIScriptController::platformSetDidShowKeyboardCallback()
{
}

void UIScriptController::platformSetDidHideKeyboardCallback()
{
}

void UIScriptController::platformClearAllCallbacks()
{
}
#endif

void UIScriptController::uiScriptComplete(JSStringRef result)
{
    m_context->requestUIScriptCompletion(result);
    platformClearAllCallbacks();
}

}
