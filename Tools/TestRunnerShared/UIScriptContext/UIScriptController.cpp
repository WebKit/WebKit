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

UIScriptController::UIScriptController(UIScriptContext& context)
    : m_context(&context)
{
}

#if !PLATFORM(IOS)
void UIScriptController::checkForOutstandingCallbacks()
{
}
#endif

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

void simulateAccessibilitySettingsChangeNotification(JSValueRef)
{
}

void UIScriptController::doAfterPresentationUpdate(JSValueRef)
{
}

void UIScriptController::doAfterNextStablePresentationUpdate(JSValueRef)
{
}

void UIScriptController::doAfterVisibleContentRectUpdate(JSValueRef)
{
}
#endif

void UIScriptController::setDidStartFormControlInteractionCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidStartFormControlInteraction);
    platformSetDidStartFormControlInteractionCallback();
}

JSValueRef UIScriptController::didStartFormControlInteractionCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidStartFormControlInteraction);
}

void UIScriptController::setDidEndFormControlInteractionCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidEndFormControlInteraction);
    platformSetDidEndFormControlInteractionCallback();
}

JSValueRef UIScriptController::didEndFormControlInteractionCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidEndFormControlInteraction);
}
    
void UIScriptController::setDidShowForcePressPreviewCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidShowForcePressPreview);
    platformSetDidShowForcePressPreviewCallback();
}

JSValueRef UIScriptController::didShowForcePressPreviewCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidShowForcePressPreview);
}

void UIScriptController::setDidDismissForcePressPreviewCallback(JSValueRef callback)
{
    m_context->registerCallback(callback, CallbackTypeDidDismissForcePressPreview);
    platformSetDidDismissForcePressPreviewCallback();
}

JSValueRef UIScriptController::didDismissForcePressPreviewCallback() const
{
    return m_context->callbackWithID(CallbackTypeDidDismissForcePressPreview);
}

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

#if !PLATFORM(COCOA)
void UIScriptController::zoomToScale(double, JSValueRef)
{
}

void UIScriptController::setViewScale(double)
{
}

void UIScriptController::simulateAccessibilitySettingsChangeNotification(JSValueRef)
{
}

JSObjectRef UIScriptController::contentsOfUserInterfaceItem(JSStringRef interfaceItem) const
{
    return nullptr;
}
#endif

void UIScriptController::playBackEventStream(JSStringRef stream, JSValueRef callback)
{
    platformPlayBackEventStream(stream, callback);
}

#if !PLATFORM(IOS)
void UIScriptController::touchDownAtPoint(long x, long y, long touchCount, JSValueRef)
{
}

void UIScriptController::liftUpAtPoint(long x, long y, long touchCount, JSValueRef)
{
}

void UIScriptController::singleTapAtPoint(long x, long y, JSValueRef)
{
}

void UIScriptController::doubleTapAtPoint(long x, long y, JSValueRef)
{
}

void UIScriptController::dragFromPointToPoint(long startX, long startY, long endX, long endY, double durationSeconds, JSValueRef callback)
{
}
    
void UIScriptController::longPressAtPoint(long x, long y, JSValueRef)
{
}

void UIScriptController::stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
}

void UIScriptController::stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
}

void UIScriptController::stylusUpAtPoint(long x, long y, JSValueRef callback)
{
}

void UIScriptController::stylusTapAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback)
{
}

void UIScriptController::sendEventStream(JSStringRef eventsJSON, JSValueRef callback)
{
}

void UIScriptController::enterText(JSStringRef)
{
}

void UIScriptController::typeCharacterUsingHardwareKeyboard(JSStringRef, JSValueRef)
{
}

void UIScriptController::keyUpUsingHardwareKeyboard(JSStringRef, JSValueRef)
{
}

void UIScriptController::keyDownUsingHardwareKeyboard(JSStringRef, JSValueRef)
{
}

void UIScriptController::dismissFormAccessoryView()
{
}

void UIScriptController::setTimePickerValue(long, long)
{
}

void UIScriptController::selectFormAccessoryPickerRow(long)
{
}

JSRetainPtr<JSStringRef> UIScriptController::textContentType() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> UIScriptController::selectFormPopoverTitle() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> UIScriptController::formInputLabel() const
{
    return nullptr;
}

void UIScriptController::scrollToOffset(long x, long y)
{
}

void UIScriptController::immediateScrollToOffset(long x, long y)
{
}

void UIScriptController::immediateZoomToScale(double scale)
{
}

void UIScriptController::keyboardAccessoryBarNext()
{
}

void UIScriptController::keyboardAccessoryBarPrevious()
{
}

void UIScriptController::applyAutocorrection(JSStringRef, JSStringRef, JSValueRef)
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

std::optional<bool> UIScriptController::stableStateOverride() const
{
    return std::nullopt;
}

void UIScriptController::setStableStateOverride(std::optional<bool>)
{
}

JSObjectRef UIScriptController::contentVisibleRect() const
{
    return nullptr;
}

JSObjectRef UIScriptController::selectionRangeViewRects() const
{
    return nullptr;
}

JSObjectRef UIScriptController::textSelectionCaretRect() const
{
    return nullptr;
}

JSObjectRef UIScriptController::selectionStartGrabberViewRect() const
{
    return nullptr;
}

JSObjectRef UIScriptController::selectionEndGrabberViewRect() const
{
    return nullptr;
}

JSObjectRef UIScriptController::inputViewBounds() const
{
    return nullptr;
}

void UIScriptController::removeAllDynamicDictionaries()
{
}

JSRetainPtr<JSStringRef> UIScriptController::scrollingTreeAsText() const
{
    return nullptr;
}

JSObjectRef UIScriptController::propertiesOfLayerWithID(uint64_t layerID) const
{
    return nullptr;
}

void UIScriptController::platformSetDidStartFormControlInteractionCallback()
{
}

void UIScriptController::platformSetDidEndFormControlInteractionCallback()
{
}
    
void UIScriptController::platformSetDidShowForcePressPreviewCallback()
{
}

void UIScriptController::platformSetDidDismissForcePressPreviewCallback()
{
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

void UIScriptController::retrieveSpeakSelectionContent(JSValueRef)
{
}

JSRetainPtr<JSStringRef> UIScriptController::accessibilitySpeakSelectionContent() const
{
    return nullptr;
}

void UIScriptController::setSafeAreaInsets(double top, double right, double bottom, double left)
{
}

#endif

#if !PLATFORM(COCOA)

void UIScriptController::simulateRotation(DeviceOrientation*, JSValueRef callback)
{
}

void UIScriptController::simulateRotationLikeSafari(DeviceOrientation*, JSValueRef callback)
{
}

void UIScriptController::findString(JSStringRef, unsigned long options, unsigned long maxCount)
{
}

void UIScriptController::removeViewFromWindow(JSValueRef)
{
}

void UIScriptController::addViewToWindow(JSValueRef)
{
}

void UIScriptController::beginBackSwipe(JSValueRef callback)
{
}

void UIScriptController::completeBackSwipe(JSValueRef callback)
{
}

void UIScriptController::setShareSheetCompletesImmediatelyWithResolution(bool)
{
}

#endif // !PLATFORM(COCOA)

#if !PLATFORM(MAC)

void UIScriptController::overridePreference(JSStringRef, JSStringRef)
{
}

void UIScriptController::replaceTextAtRange(JSStringRef, int, int)
{
}

void UIScriptController::platformPlayBackEventStream(JSStringRef, JSValueRef)
{
}

void UIScriptController::firstResponderSuppressionForWebView(bool)
{
}

void UIScriptController::makeWindowContentViewFirstResponder()
{
}

bool UIScriptController::isWindowContentViewFirstResponder() const
{
    return false;
}

bool UIScriptController::isShowingDataListSuggestions() const
{
    return false;
}

#endif

void UIScriptController::uiScriptComplete(JSStringRef result)
{
    m_context->requestUIScriptCompletion(result);
    platformClearAllCallbacks();
}

}
