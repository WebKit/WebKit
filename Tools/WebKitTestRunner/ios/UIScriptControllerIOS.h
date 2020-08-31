/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#import "UIScriptControllerCocoa.h"
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS_FAMILY)

namespace WebCore {
class FloatPoint;
class FloatRect;
}

namespace WTR {

class UIScriptControllerIOS : public UIScriptControllerCocoa {
public:
    explicit UIScriptControllerIOS(UIScriptContext& context)
        : UIScriptControllerCocoa(context)
    {
    }

    void waitForOutstandingCallbacks() override;
    void doAfterPresentationUpdate(JSValueRef) override;
    void doAfterNextStablePresentationUpdate(JSValueRef) override;
    void ensurePositionInformationIsUpToDateAt(long x, long y, JSValueRef) override;
    void doAfterVisibleContentRectUpdate(JSValueRef) override;
    void doAfterDoubleTapDelay(JSValueRef) override;
    void zoomToScale(double scale, JSValueRef) override;
    void retrieveSpeakSelectionContent(JSValueRef) override;
    JSRetainPtr<JSStringRef> accessibilitySpeakSelectionContent() const override;
    void simulateAccessibilitySettingsChangeNotification(JSValueRef) override;
    double zoomScale() const override;
    void touchDownAtPoint(long x, long y, long touchCount, JSValueRef) override;
    void liftUpAtPoint(long x, long y, long touchCount, JSValueRef) override;
    void singleTapAtPoint(long x, long y, JSValueRef) override;
    void singleTapAtPointWithModifiers(long x, long y, JSValueRef modifierArray, JSValueRef) override;
    void twoFingerSingleTapAtPoint(long x, long y, JSValueRef callback) override;
    void doubleTapAtPoint(long x, long y, float delay, JSValueRef) override;
    void stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef) override;
    void stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef) override;
    void stylusUpAtPoint(long x, long y, JSValueRef) override;
    void stylusTapAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef) override;
    void stylusTapAtPointWithModifiers(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef modifierArray, JSValueRef) override;
    void sendEventStream(JSStringRef eventsJSON, JSValueRef) override;
    void dragFromPointToPoint(long startX, long startY, long endX, long endY, double durationSeconds, JSValueRef) override;
    void longPressAtPoint(long x, long y, JSValueRef) override;
    void enterText(JSStringRef text) override;
    void typeCharacterUsingHardwareKeyboard(JSStringRef character, JSValueRef) override;
    void keyDown(JSStringRef character, JSValueRef modifierArray) override;

    void activateAtPoint(long x, long y, JSValueRef callback) override;

    void rawKeyDown(JSStringRef) override;
    void rawKeyUp(JSStringRef) override;

    void dismissFormAccessoryView() override;
    void dismissFilePicker(JSValueRef) override;
    JSRetainPtr<JSStringRef> selectFormPopoverTitle() const override;
    JSRetainPtr<JSStringRef> textContentType() const override;
    JSRetainPtr<JSStringRef> formInputLabel() const override;
    void selectFormAccessoryPickerRow(long rowIndex) override;
    bool selectFormAccessoryHasCheckedItemAtRow(long rowIndex) const override;
    void setTimePickerValue(long hour, long minute) override;
    double timePickerValueHour() const override;
    double timePickerValueMinute() const override;
    bool isPresentingModally() const override;
    double contentOffsetX() const override;
    double contentOffsetY() const override;
    bool scrollUpdatesDisabled() const override;
    void setScrollUpdatesDisabled(bool) override;
    void scrollToOffset(long x, long y) override;
    void immediateScrollToOffset(long x, long y) override;
    void immediateScrollElementAtContentPointToOffset(long x, long y, long xScrollOffset, long yScrollOffset) override;
    void immediateZoomToScale(double scale) override;
    void keyboardAccessoryBarNext() override;
    void keyboardAccessoryBarPrevious() override;
    bool isShowingKeyboard() const override;
    bool hasInputSession() const override;
    void applyAutocorrection(JSStringRef newString, JSStringRef oldString, JSValueRef) override;
    double minimumZoomScale() const override;
    double maximumZoomScale() const override;
    Optional<bool> stableStateOverride() const override;
    void setStableStateOverride(Optional<bool> overrideValue) override;
    JSObjectRef contentVisibleRect() const override;
    JSObjectRef textSelectionRangeRects() const override;
    JSObjectRef textSelectionCaretRect() const override;
    JSObjectRef selectionStartGrabberViewRect() const override;
    JSObjectRef selectionEndGrabberViewRect() const override;
    JSObjectRef selectionCaretViewRect() const override;
    JSObjectRef selectionRangeViewRects() const override;
    JSObjectRef inputViewBounds() const override;
    JSRetainPtr<JSStringRef> scrollingTreeAsText() const override;
    JSObjectRef propertiesOfLayerWithID(uint64_t layerID) const override;
    void simulateRotation(DeviceOrientation*, JSValueRef) override;
    void simulateRotationLikeSafari(DeviceOrientation*, JSValueRef) override;
    bool isShowingPopover() const override;
    JSObjectRef rectForMenuAction(JSStringRef) const override;
    JSObjectRef menuRect() const override;
    bool isDismissingMenu() const override;
    void chooseMenuAction(JSStringRef, JSValueRef) override;
    void setSafeAreaInsets(double top, double right, double bottom, double left) override;
    void beginBackSwipe(JSValueRef) override;
    void completeBackSwipe(JSValueRef) override;
    bool isShowingDataListSuggestions() const override;
    void activateDataListSuggestion(unsigned, JSValueRef) override;
    void setKeyboardInputModeIdentifier(JSStringRef) override;
    void toggleCapsLock(JSValueRef) override;
    bool keyboardIsAutomaticallyShifted() const override;
    JSObjectRef attachmentInfo(JSStringRef) override;
    UIView *platformContentView() const override;
    JSObjectRef calendarType() const override;
    void setHardwareKeyboardAttached(bool) override;
    void setAllowsViewportShrinkToFit(bool) override;
    void copyText(JSStringRef) override;
    void installTapGestureOnWindow(JSValueRef) override;
    bool isShowingContextMenu() const override;
    void setSpellCheckerResults(JSValueRef) override { }

    bool mayContainEditableElementsInRect(unsigned x, unsigned y, unsigned width, unsigned height) override;

    void setDidStartFormControlInteractionCallback(JSValueRef) override;
    void setDidEndFormControlInteractionCallback(JSValueRef) override;
    void setDidShowContextMenuCallback(JSValueRef) override;
    void setDidDismissContextMenuCallback(JSValueRef) override;
    void setWillBeginZoomingCallback(JSValueRef) override;
    void setDidEndZoomingCallback(JSValueRef) override;
    void setDidShowKeyboardCallback(JSValueRef) override;
    void setDidHideKeyboardCallback(JSValueRef) override;
    void setWillPresentPopoverCallback(JSValueRef) override;
    void setDidDismissPopoverCallback(JSValueRef) override;
    void setDidEndScrollingCallback(JSValueRef) override;
    void clearAllCallbacks() override;

private:
    void waitForModalTransitionToFinish() const;
    void waitForSingleTapToReset() const;
    WebCore::FloatRect rectForMenuAction(CFStringRef) const;
    void singleTapAtPointWithModifiers(WebCore::FloatPoint location, Vector<String>&& modifierFlags, BlockPtr<void()>&&);
};

}

#endif // PLATFORM(IOS_FAMILY)
