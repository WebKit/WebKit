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

#include "JSBasics.h"
#include "JSUIScriptController.h"
#include "UIScriptContext.h"
#include <JavaScriptCore/JSRetainPtr.h>
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
    auto option = createJSString(context, value);
    if (JSStringIsEqualToUTF8CString(option.get(), "portrait"))
        return &values[0];
    if (JSStringIsEqualToUTF8CString(option.get(), "portrait-upsidedown"))
        return &values[1];
    if (JSStringIsEqualToUTF8CString(option.get(), "landscape-left"))
        return &values[2];
    if (JSStringIsEqualToUTF8CString(option.get(), "landscape-right"))
        return &values[3];
    return nullptr;
}

ScrollToOptions* toScrollToOptions(JSContextRef context, JSValueRef argument)
{
    if (!JSValueIsObject(context, argument))
        return nullptr;

    static ScrollToOptions options;
    options.unconstrained = booleanProperty(context, (JSObjectRef)argument, "unconstrained", false);
    return &options;
}

TextExtractionTestOptions* toTextExtractionTestOptions(JSContextRef context, JSValueRef argument)
{
    if (!JSValueIsObject(context, argument))
        return nullptr;

    static TextExtractionTestOptions options;
    options.clipToBounds = booleanProperty(context, (JSObjectRef)argument, "clipToBounds", false);
    options.includeRects = booleanProperty(context, (JSObjectRef)argument, "includeRects", false);
    options.includeURLs = booleanProperty(context, (JSObjectRef)argument, "includeURLs", false);
    options.includeEventListeners = booleanProperty(context, (JSObjectRef)argument, "includeEventListeners", false);
    options.includeAccessibilityAttributes = booleanProperty(context, (JSObjectRef)argument, "includeAccessibilityAttributes", false);
    options.includeTextInAutoFilledControls = booleanProperty(context, (JSObjectRef)argument, "includeTextInAutoFilledControls", false);
    options.wordLimit = static_cast<unsigned>(numericProperty(context, (JSObjectRef)argument, "wordLimit"));
    options.mergeParagraphs = booleanProperty(context, (JSObjectRef)argument, "mergeParagraphs", false);
    options.skipNearlyTransparentContent = booleanProperty(context, (JSObjectRef)argument, "skipNearlyTransparentContent", false);
    options.nodeIdentifierInclusion = stringProperty(context, (JSObjectRef)argument, "nodeIdentifierInclusion");
    options.outputFormat = stringProperty(context, (JSObjectRef)argument, "outputFormat");
    return &options;
}

TextExtractionInteractionOptions* toTextExtractionInteractionOptions(JSContextRef context, JSValueRef argument)
{
    if (!JSValueIsObject(context, argument))
        return nullptr;

    static TextExtractionInteractionOptions options;
    if (auto nodeIdentifier = property(context, (JSObjectRef)argument, "nodeIdentifier"); isValidValue(context, nodeIdentifier))
        options.nodeIdentifier = createJSString(context, nodeIdentifier);
    else
        options.nodeIdentifier = nullptr;

    if (auto text = property(context, (JSObjectRef)argument, "text"); isValidValue(context, text))
        options.text = createJSString(context, text);
    else
        options.text = nullptr;

    options.replaceAll = booleanProperty(context, (JSObjectRef)argument, "replaceAll");
    options.scrollToVisible = booleanProperty(context, (JSObjectRef)argument, "scrollToVisible");

    if (auto deltaObject = objectProperty(context, (JSObjectRef)argument, "scrollDelta")) {
        options.scrollDelta = {
            numericProperty(context, deltaObject, "x"),
            numericProperty(context, deltaObject, "y")
        };
    } else
        options.scrollDelta = std::nullopt;

    if (auto locationObject = objectProperty(context, (JSObjectRef)argument, "location")) {
        options.location = {
            numericProperty(context, locationObject, "x"),
            numericProperty(context, locationObject, "y")
        };
    } else
        options.location = std::nullopt;

    return &options;
}

UIScriptController::UIScriptController(UIScriptContext& context)
    : m_context(context)
{
}

UIScriptContext* UIScriptController::context()
{
    return m_context.get();
}

void UIScriptController::makeWindowObject(JSContextRef context)
{
    setGlobalObjectProperty(context, "uiController", this);
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
    
void UIScriptController::setDidShowContextMenuCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidShowContextMenu);
}

JSValueRef UIScriptController::didShowContextMenuCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidShowContextMenu);
}

void UIScriptController::setDidDismissContextMenuCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidDismissContextMenu);
}

JSValueRef UIScriptController::didDismissContextMenuCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidDismissContextMenu);
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

void UIScriptController::setWillStartInputSessionCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeWillStartInputSession);
}

JSValueRef UIScriptController::willStartInputSessionCallback() const
{
    return m_context->callbackWithID(CallbackTypeWillStartInputSession);
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

void UIScriptController::setDidPresentViewControllerCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidPresentViewController);
}

JSValueRef UIScriptController::didPresentViewControllerCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidPresentViewController);
}

void UIScriptController::setDidShowContactPickerCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidShowContactPicker);
}

JSValueRef UIScriptController::didShowContactPickerCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidShowContactPicker);
}

void UIScriptController::setDidHideContactPickerCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidHideContactPicker);
}

JSValueRef UIScriptController::didHideContactPickerCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidHideContactPicker);
}

void UIScriptController::uiScriptComplete(JSStringRef result)
{
    m_context->requestUIScriptCompletion(result);
    clearAllCallbacks();
}

void UIScriptController::dismissMenu()
{
}

void UIScriptController::chooseMenuAction(JSStringRef, JSValueRef)
{
}

}
