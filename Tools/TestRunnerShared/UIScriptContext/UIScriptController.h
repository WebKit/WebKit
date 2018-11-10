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

#ifndef UIScriptController_h
#define UIScriptController_h

#include "JSWrappable.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>

namespace WebCore {
class FloatRect;
}

namespace WTR {

class UIScriptContext;

enum class DeviceOrientation {
    Portrait,
    PortraitUpsideDown,
    LandscapeLeft,
    LandscapeRight
};

DeviceOrientation* toDeviceOrientation(JSContextRef, JSValueRef);

class UIScriptController : public JSWrappable {
public:
    static Ref<UIScriptController> create(UIScriptContext& context)
    {
        return adoptRef(*new UIScriptController(context));
    }

    void contextDestroyed();
    void checkForOutstandingCallbacks();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);
    
    void doAsyncTask(JSValueRef callback);
    void doAfterPresentationUpdate(JSValueRef callback);
    void doAfterNextStablePresentationUpdate(JSValueRef callback);
    void doAfterVisibleContentRectUpdate(JSValueRef callback);

    void zoomToScale(double scale, JSValueRef callback);
    void setViewScale(double);

    void resignFirstResponder();

    void simulateAccessibilitySettingsChangeNotification(JSValueRef callback);

    void touchDownAtPoint(long x, long y, long touchCount, JSValueRef callback);
    void liftUpAtPoint(long x, long y, long touchCount, JSValueRef callback);
    void singleTapAtPoint(long x, long y, JSValueRef callback);
    void doubleTapAtPoint(long x, long y, JSValueRef callback);
    void dragFromPointToPoint(long startX, long startY, long endX, long endY, double durationSeconds, JSValueRef callback);

    void stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback);
    void stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback);
    void stylusUpAtPoint(long x, long y, JSValueRef callback);
    void stylusTapAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback);

    void longPressAtPoint(long x, long y, JSValueRef callback);

    void sendEventStream(JSStringRef eventsJSON, JSValueRef callback);

    void enterText(JSStringRef);
    void typeCharacterUsingHardwareKeyboard(JSStringRef character, JSValueRef callback);
    void keyDownUsingHardwareKeyboard(JSStringRef character, JSValueRef callback);
    void keyUpUsingHardwareKeyboard(JSStringRef character, JSValueRef callback);

    void keyboardAccessoryBarNext();
    void keyboardAccessoryBarPrevious();

    void applyAutocorrection(JSStringRef newString, JSStringRef oldString, JSValueRef callback);
    
    void dismissFormAccessoryView();
    void selectFormAccessoryPickerRow(long);
    JSRetainPtr<JSStringRef> textContentType() const;
    JSRetainPtr<JSStringRef> selectFormPopoverTitle() const;
    JSRetainPtr<JSStringRef> formInputLabel() const;
    void setTimePickerValue(long hour, long minute);

    void setShareSheetCompletesImmediatelyWithResolution(bool resolved);

    bool isShowingDataListSuggestions() const;

    JSObjectRef contentsOfUserInterfaceItem(JSStringRef) const;
    void overridePreference(JSStringRef preference, JSStringRef value);
    
    void scrollToOffset(long x, long y);

    void immediateScrollToOffset(long x, long y);
    void immediateZoomToScale(double scale);

    void beginBackSwipe(JSValueRef callback);
    void completeBackSwipe(JSValueRef callback);

    void setDidStartFormControlInteractionCallback(JSValueRef);
    JSValueRef didStartFormControlInteractionCallback() const;

    void setDidEndFormControlInteractionCallback(JSValueRef);
    JSValueRef didEndFormControlInteractionCallback() const;
    
    void setDidShowForcePressPreviewCallback(JSValueRef);
    JSValueRef didShowForcePressPreviewCallback() const;
    
    void setDidDismissForcePressPreviewCallback(JSValueRef);
    JSValueRef didDismissForcePressPreviewCallback() const;

    void setWillBeginZoomingCallback(JSValueRef);
    JSValueRef willBeginZoomingCallback() const;

    void setDidEndZoomingCallback(JSValueRef);
    JSValueRef didEndZoomingCallback() const;

    void setDidShowKeyboardCallback(JSValueRef);
    JSValueRef didShowKeyboardCallback() const;

    void setDidHideKeyboardCallback(JSValueRef);
    JSValueRef didHideKeyboardCallback() const;

    bool isShowingKeyboard() const;

    void setDidEndScrollingCallback(JSValueRef);
    JSValueRef didEndScrollingCallback() const;

    void playBackEventStream(JSStringRef stream, JSValueRef callback);

    double zoomScale() const;
    double minimumZoomScale() const;
    double maximumZoomScale() const;
    
    std::optional<bool> stableStateOverride() const;
    void setStableStateOverride(std::optional<bool>);

    JSObjectRef contentVisibleRect() const;
    
    JSObjectRef selectionRangeViewRects() const;
    JSObjectRef textSelectionCaretRect() const;
    JSObjectRef selectionStartGrabberViewRect() const;
    JSObjectRef selectionEndGrabberViewRect() const;
    JSObjectRef calendarType() const;
    void setDefaultCalendarType(JSStringRef calendarIdentifier);
    JSObjectRef inputViewBounds() const;

    void replaceTextAtRange(JSStringRef, int location, int length);
    void removeAllDynamicDictionaries();
    
    JSRetainPtr<JSStringRef> scrollingTreeAsText() const;

    JSObjectRef propertiesOfLayerWithID(uint64_t layerID) const;

    void uiScriptComplete(JSStringRef result);
    
    void retrieveSpeakSelectionContent(JSValueRef);
    JSRetainPtr<JSStringRef> accessibilitySpeakSelectionContent() const;
    
    void simulateRotation(DeviceOrientation*, JSValueRef);
    void simulateRotationLikeSafari(DeviceOrientation*, JSValueRef);

    void findString(JSStringRef, unsigned long options, unsigned long maxCount);

    // These use a callback to allow the client to know when view visibility state updates get to the web process.
    void removeViewFromWindow(JSValueRef);
    void addViewToWindow(JSValueRef);

    void setSafeAreaInsets(double top, double right, double bottom, double left);

    void firstResponderSuppressionForWebView(bool);
    void makeWindowContentViewFirstResponder();
    bool isWindowContentViewFirstResponder() const;

private:
    UIScriptController(UIScriptContext&);
    
    UIScriptContext* context() { return m_context; }

    void platformSetDidStartFormControlInteractionCallback();
    void platformSetDidEndFormControlInteractionCallback();
    void platformSetDidShowForcePressPreviewCallback();
    void platformSetDidDismissForcePressPreviewCallback();
    void platformSetWillBeginZoomingCallback();
    void platformSetDidEndZoomingCallback();
    void platformSetDidShowKeyboardCallback();
    void platformSetDidHideKeyboardCallback();
    void platformSetDidEndScrollingCallback();
    void platformClearAllCallbacks();
    void platformPlayBackEventStream(JSStringRef, JSValueRef);

    JSClassRef wrapperClass() final;

    JSObjectRef objectFromRect(const WebCore::FloatRect&) const;

    UIScriptContext* m_context;
};

}

#endif // UIScriptController_h
