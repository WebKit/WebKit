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
#include <JavaScriptCore/OpaqueJSString.h>

namespace WTR {

DeviceOrientation* toDeviceOrientation(JSContextRef context, JSValueRef value)
{
    static DeviceOrientation values[] = {
        DeviceOrientation::Portrait,
        DeviceOrientation::PortraitUpsideDown,
        DeviceOrientation::LandscapeLeft,
        DeviceOrientation::LandscapeRight
    };

    auto option = adopt(JSValueToStringCopy(context, value, nullptr));
    if (option.get()->string() == "portrait")
        return &values[0];
        
    if (option.get()->string() == "portrait-upsidedown")
        return &values[1];
        
    if (option.get()->string() == "landscape-left")
        return &values[2];
        
    if (option.get()->string() == "landscape-right")
        return &values[3];
        
    return nullptr;
}

#if !PLATFORM(GTK) && !PLATFORM(COCOA)
Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptController(context));
}
#endif

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

void UIScriptController::setDidStartFormControlInteractionCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidStartFormControlInteraction);
}

JSValueRef UIScriptController::didStartFormControlInteractionCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidStartFormControlInteraction);
}

void UIScriptController::setDidEndFormControlInteractionCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidEndFormControlInteraction);
}

JSValueRef UIScriptController::didEndFormControlInteractionCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidEndFormControlInteraction);
}
    
void UIScriptController::setDidShowForcePressPreviewCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidShowForcePressPreview);
}

JSValueRef UIScriptController::didShowForcePressPreviewCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidShowForcePressPreview);
}

void UIScriptController::setDidDismissForcePressPreviewCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidDismissForcePressPreview);
}

JSValueRef UIScriptController::didDismissForcePressPreviewCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidDismissForcePressPreview);
}

void UIScriptController::setWillBeginZoomingCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeWillBeginZooming);
}

JSValueRef UIScriptController::willBeginZoomingCallback() const
{
    return m_context->callbackWithID(CallbackTypeWillBeginZooming);
}

void UIScriptController::setDidEndZoomingCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidEndZooming);
}

JSValueRef UIScriptController::didEndZoomingCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidEndZooming);
}

void UIScriptController::setWillCreateNewPageCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeWillCreateNewPage);
}

JSValueRef UIScriptController::willCreateNewPageCallback() const
{
    return m_context->callbackWithID(CallbackTypeWillCreateNewPage);
}

void UIScriptController::setDidEndScrollingCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidEndScrolling);
}

JSValueRef UIScriptController::didEndScrollingCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidEndScrolling);
}

void UIScriptController::setDidShowKeyboardCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidShowKeyboard);
}

JSValueRef UIScriptController::didShowKeyboardCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidShowKeyboard);
}

void UIScriptController::setDidHideKeyboardCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidHideKeyboard);
}

JSValueRef UIScriptController::didHideKeyboardCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidHideKeyboard);
}

void UIScriptController::setDidShowMenuCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidShowMenu);
}

JSValueRef UIScriptController::didShowMenuCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidShowMenu);
}

void UIScriptController::setDidHideMenuCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidHideMenu);
}

JSValueRef UIScriptController::didHideMenuCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidHideMenu);
}

void UIScriptController::setWillPresentPopoverCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeWillPresentPopover);
}

JSValueRef UIScriptController::willPresentPopoverCallback() const
{
    return m_context->callbackWithID(CallbackTypeWillPresentPopover);
}

void UIScriptController::setDidDismissPopoverCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidDismissPopover);
}

JSValueRef UIScriptController::didDismissPopoverCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidDismissPopover);
}

void UIScriptController::uiScriptComplete(JSStringRef result)
{
    m_context->requestUIScriptCompletion(result);
    clearAllCallbacks();
}

}
