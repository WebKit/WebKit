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

OBJC_CLASS NSUndoManager;
OBJC_CLASS NSView;
OBJC_CLASS UIView;

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
    static Ref<UIScriptController> create(UIScriptContext&);

    void uiScriptComplete(JSStringRef result);

    void notImplemented() const { RELEASE_ASSERT_NOT_REACHED(); }

    void contextDestroyed();
    virtual void waitForOutstandingCallbacks() { /* notImplemented(); */ }

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);

    // Transaction helpers
    
    virtual void doAsyncTask(JSValueRef callback) { notImplemented(); }
    virtual void doAfterPresentationUpdate(JSValueRef callback) { doAsyncTask(callback); }
    virtual void doAfterNextStablePresentationUpdate(JSValueRef callback) { doAsyncTask(callback); }
    virtual void ensurePositionInformationIsUpToDateAt(long x, long y, JSValueRef callback) { doAsyncTask(callback); }
    virtual void doAfterVisibleContentRectUpdate(JSValueRef callback) { doAsyncTask(callback); }

    virtual void doAfterDoubleTapDelay(JSValueRef callback) { doAsyncTask(callback); }

    // Preferences

    virtual void overridePreference(JSStringRef preference, JSStringRef value) { notImplemented(); }

    // Zooming

    virtual void zoomToScale(double scale, JSValueRef callback) { notImplemented(); }
    virtual void immediateZoomToScale(double scale) { notImplemented(); }
    virtual void setViewScale(double) { notImplemented(); }
    virtual double zoomScale() const { notImplemented(); return 1; }
    virtual double minimumZoomScale() const { notImplemented(); return 1; }
    virtual double maximumZoomScale() const { notImplemented(); return 1; }

    // Viewports

    virtual void setMinimumEffectiveWidth(double) { notImplemented(); }
    virtual void setAllowsViewportShrinkToFit(bool) { notImplemented(); }

    virtual Optional<bool> stableStateOverride() const { notImplemented(); return WTF::nullopt; }
    virtual void setStableStateOverride(Optional<bool>) { notImplemented(); }

    virtual JSObjectRef contentVisibleRect() const { notImplemented(); return nullptr; }
    
    virtual void setSafeAreaInsets(double top, double right, double bottom, double left) { notImplemented(); }

    // View Parenting and Visibility

    virtual void becomeFirstResponder() { notImplemented(); }
    virtual void resignFirstResponder() { notImplemented(); }

    virtual void copyText(JSStringRef) { notImplemented(); }
    virtual void paste() { notImplemented(); }

    virtual void chooseMenuAction(JSStringRef, JSValueRef);
    virtual void dismissMenu();

    virtual void firstResponderSuppressionForWebView(bool) { notImplemented(); }
    virtual void makeWindowContentViewFirstResponder() { notImplemented(); }
    virtual bool isWindowContentViewFirstResponder() const { notImplemented(); return false; }

    virtual void removeViewFromWindow(JSValueRef) { notImplemented(); }
    virtual void addViewToWindow(JSValueRef) { notImplemented(); }

    virtual void installTapGestureOnWindow(JSValueRef) { notImplemented(); }

    // Editable region

    virtual bool mayContainEditableElementsInRect(unsigned, unsigned, unsigned, unsigned) { notImplemented(); return false; }

    // Compositing

    virtual JSObjectRef propertiesOfLayerWithID(uint64_t layerID) const { notImplemented(); return nullptr; }

    // Scrolling

    virtual bool scrollUpdatesDisabled() const { notImplemented(); return false; }
    virtual void setScrollUpdatesDisabled(bool) { notImplemented(); }

    virtual void scrollToOffset(long x, long y) { notImplemented(); }

    virtual void immediateScrollToOffset(long x, long y) { notImplemented(); }
    virtual void immediateScrollElementAtContentPointToOffset(long x, long y, long xScrollOffset, long yScrollOffset) { notImplemented(); }

    virtual double contentOffsetX() const { notImplemented(); return 0; }
    virtual double contentOffsetY() const { notImplemented(); return 0; }

    virtual JSRetainPtr<JSStringRef> scrollingTreeAsText() const { notImplemented(); return nullptr; }

    // Touches

    virtual void touchDownAtPoint(long x, long y, long touchCount, JSValueRef callback) { notImplemented(); }
    virtual void liftUpAtPoint(long x, long y, long touchCount, JSValueRef callback) { notImplemented(); }
    virtual void singleTapAtPoint(long x, long y, JSValueRef callback) { notImplemented(); }
    virtual void singleTapAtPointWithModifiers(long x, long y, JSValueRef modifierArray, JSValueRef callback) { notImplemented(); }
    virtual void twoFingerSingleTapAtPoint(long x, long y, JSValueRef callback) { notImplemented(); }
    virtual void doubleTapAtPoint(long x, long y, float delay, JSValueRef callback) { notImplemented(); }
    virtual void dragFromPointToPoint(long startX, long startY, long endX, long endY, double durationSeconds, JSValueRef callback) { notImplemented(); }
    virtual void longPressAtPoint(long x, long y, JSValueRef callback) { notImplemented(); }

    virtual void activateAtPoint(long x, long y, JSValueRef callback) { notImplemented(); }

    // Keyboard

    virtual void enterText(JSStringRef) { notImplemented(); }
    virtual void typeCharacterUsingHardwareKeyboard(JSStringRef character, JSValueRef callback) { notImplemented(); }

    virtual void keyDown(JSStringRef character, JSValueRef modifierArray) { notImplemented(); }
    virtual void toggleCapsLock(JSValueRef callback) { notImplemented(); }
    virtual void setContinuousSpellCheckingEnabled(bool) { notImplemented(); }
    virtual bool keyboardIsAutomaticallyShifted() const
    {
        notImplemented();
        return false;
    }

    virtual void rawKeyDown(JSStringRef) { notImplemented(); }
    virtual void rawKeyUp(JSStringRef) { notImplemented(); }

    virtual void keyboardAccessoryBarNext() { notImplemented(); }
    virtual void keyboardAccessoryBarPrevious() { notImplemented(); }

    virtual void applyAutocorrection(JSStringRef newString, JSStringRef oldString, JSValueRef callback) { notImplemented(); }

    virtual bool isShowingKeyboard() const { notImplemented(); return false; }
    virtual bool hasInputSession() const { notImplemented(); return false; }

    virtual void setHardwareKeyboardAttached(bool) { }

    virtual void setKeyboardInputModeIdentifier(JSStringRef) { notImplemented(); }

    virtual void replaceTextAtRange(JSStringRef, int location, int length) { notImplemented(); }
    virtual void removeAllDynamicDictionaries() { notImplemented(); }

    virtual bool windowIsKey() const;
    virtual void setWindowIsKey(bool);

    // Stylus

    virtual void stylusDownAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback) { notImplemented(); }
    virtual void stylusMoveToPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback) { notImplemented(); }
    virtual void stylusUpAtPoint(long x, long y, JSValueRef callback) { notImplemented(); }
    virtual void stylusTapAtPoint(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef callback) { notImplemented(); }
    virtual void stylusTapAtPointWithModifiers(long x, long y, float azimuthAngle, float altitudeAngle, float pressure, JSValueRef modifierArray, JSValueRef callback) { notImplemented(); }

    // Event Stream

    virtual void sendEventStream(JSStringRef eventsJSON, JSValueRef callback) { notImplemented(); }
    virtual void playBackEventStream(JSStringRef stream, JSValueRef callback) { notImplemented(); }

    // Form Controls
    
    virtual void dismissFilePicker(JSValueRef callback) { notImplemented(); }
    virtual void dismissFormAccessoryView() { notImplemented(); }
    virtual void selectFormAccessoryPickerRow(long) { notImplemented(); }
    virtual bool selectFormAccessoryHasCheckedItemAtRow(long) const { return false; }
    virtual JSRetainPtr<JSStringRef> textContentType() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> selectFormPopoverTitle() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> formInputLabel() const { notImplemented(); return nullptr; }
    virtual void setTimePickerValue(long hour, long minute) { notImplemented(); }
    virtual double timePickerValueHour() const { notImplemented(); return -1; }
    virtual double timePickerValueMinute() const { notImplemented(); return -1; }
    virtual bool isShowingDataListSuggestions() const { notImplemented(); return false; }
    virtual JSObjectRef calendarType() const { notImplemented(); return nullptr; }
    virtual void setDefaultCalendarType(JSStringRef calendarIdentifier, JSStringRef localeIdentifier) { notImplemented(); }
    virtual JSObjectRef inputViewBounds() const { notImplemented(); return nullptr; }
    virtual void activateDataListSuggestion(unsigned, JSValueRef) { notImplemented(); }

    // Find in Page

    virtual void findString(JSStringRef, unsigned long options, unsigned long maxCount) { notImplemented(); }

    // Accessibility

    virtual void simulateAccessibilitySettingsChangeNotification(JSValueRef callback) { notImplemented(); }
    virtual void retrieveSpeakSelectionContent(JSValueRef) { notImplemented(); }
    virtual JSRetainPtr<JSStringRef> accessibilitySpeakSelectionContent() const { notImplemented(); return nullptr; }
    virtual JSObjectRef contentsOfUserInterfaceItem(JSStringRef) const { notImplemented(); return nullptr; }

    // Swipe

    virtual void beginBackSwipe(JSValueRef callback) { notImplemented(); }
    virtual void completeBackSwipe(JSValueRef callback) { notImplemented(); }

    // Child View Controllers

    virtual bool isShowingPopover() const { notImplemented(); return false; }
    virtual bool isPresentingModally() const { notImplemented(); return false; }

    // Context Menus

    virtual bool isDismissingMenu() const { notImplemented(); return false; }
    virtual bool isShowingMenu() const { notImplemented(); return false; }
    virtual JSObjectRef rectForMenuAction(JSStringRef action) const { notImplemented(); return nullptr; }
    virtual JSObjectRef menuRect() const { notImplemented(); return nullptr; }
    virtual bool isShowingContextMenu() const { notImplemented(); return false; }

    // Selection

    virtual JSObjectRef textSelectionRangeRects() const { notImplemented(); return nullptr; }
    virtual JSObjectRef textSelectionCaretRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionStartGrabberViewRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionEndGrabberViewRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionCaretViewRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionRangeViewRects() const { notImplemented(); return nullptr; }

    // Rotation

    virtual void simulateRotation(DeviceOrientation*, JSValueRef) { notImplemented(); }
    virtual void simulateRotationLikeSafari(DeviceOrientation*, JSValueRef) { notImplemented(); }

    // Editable Images

    virtual void drawSquareInEditableImage() { notImplemented(); }
    virtual long numberOfStrokesInEditableImage() { notImplemented(); return 0; }

    // Undo/Redo

    virtual JSRetainPtr<JSStringRef> lastUndoLabel() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> firstRedoLabel() const { notImplemented(); return nullptr; }

    // Attachment Elements

    virtual JSObjectRef attachmentInfo(JSStringRef attachmentIdentifier) { notImplemented(); return nullptr; }

    // Callbacks
    
    virtual void setDidStartFormControlInteractionCallback(JSValueRef);
    JSValueRef didStartFormControlInteractionCallback() const;

    virtual void setDidEndFormControlInteractionCallback(JSValueRef);
    JSValueRef didEndFormControlInteractionCallback() const;
    
    virtual void setDidShowContextMenuCallback(JSValueRef);
    JSValueRef didShowContextMenuCallback() const;
    
    virtual void setDidDismissContextMenuCallback(JSValueRef);
    JSValueRef didDismissContextMenuCallback() const;

    virtual void setWillBeginZoomingCallback(JSValueRef);
    JSValueRef willBeginZoomingCallback() const;

    virtual void setDidEndZoomingCallback(JSValueRef);
    JSValueRef didEndZoomingCallback() const;

    virtual void setWillCreateNewPageCallback(JSValueRef);
    JSValueRef willCreateNewPageCallback() const;

    virtual void setDidShowKeyboardCallback(JSValueRef);
    JSValueRef didShowKeyboardCallback() const;

    virtual void setDidHideKeyboardCallback(JSValueRef);
    JSValueRef didHideKeyboardCallback() const;

    virtual void setDidHideMenuCallback(JSValueRef);
    JSValueRef didHideMenuCallback() const;
    virtual void setDidShowMenuCallback(JSValueRef);
    JSValueRef didShowMenuCallback() const;

    virtual void setDidDismissPopoverCallback(JSValueRef);
    JSValueRef didDismissPopoverCallback() const;
    virtual void setWillPresentPopoverCallback(JSValueRef);
    JSValueRef willPresentPopoverCallback() const;

    virtual void setDidEndScrollingCallback(JSValueRef);
    JSValueRef didEndScrollingCallback() const;

protected:
    explicit UIScriptController(UIScriptContext&);
    
    UIScriptContext* context() { return m_context; }

    virtual void clearAllCallbacks() { /* notImplemented(); */ }

#if PLATFORM(COCOA)
    virtual NSUndoManager *platformUndoManager() const { notImplemented(); return nullptr; }
#endif

#if PLATFORM(IOS_FAMILY)
    virtual UIView *platformContentView() const { notImplemented(); return nullptr; }
#endif
#if PLATFORM(MAC)
    virtual NSView *platformContentView() const { notImplemented(); return nullptr; }
#endif

    JSClassRef wrapperClass() final;

    JSObjectRef objectFromRect(const WebCore::FloatRect&) const;

    UIScriptContext* m_context;

#if PLATFORM(COCOA)
    bool m_capsLockOn { false };
#endif
};

}

#endif // UIScriptController_h
