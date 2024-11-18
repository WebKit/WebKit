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

#pragma once

#include "JSWrappable.h"
#include <JavaScriptCore/JSRetainPtr.h>
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

struct ScrollToOptions {
    bool unconstrained { false };
};

ScrollToOptions* toScrollToOptions(JSContextRef, JSValueRef);

struct TextExtractionOptions {
    bool clipToBounds { false };
    bool includeRects { false };
};

TextExtractionOptions* toTextExtractionOptions(JSContextRef, JSValueRef);

class UIScriptController : public JSWrappable {
public:
    static Ref<UIScriptController> create(UIScriptContext&);

    void uiScriptComplete(JSStringRef result);

    void notImplemented() const { RELEASE_ASSERT_NOT_REACHED(); }

    void contextDestroyed();
    virtual void waitForOutstandingCallbacks() { /* notImplemented(); */ }

    void makeWindowObject(JSContextRef);

    // Transaction helpers
    
    virtual void doAsyncTask(JSValueRef) { notImplemented(); }
    virtual void doAfterPresentationUpdate(JSValueRef callback) { doAsyncTask(callback); }
    virtual void doAfterNextStablePresentationUpdate(JSValueRef callback) { doAfterPresentationUpdate(callback); }
    virtual void ensurePositionInformationIsUpToDateAt(long, long, JSValueRef callback) { doAsyncTask(callback); }
    virtual void doAfterVisibleContentRectUpdate(JSValueRef callback) { doAsyncTask(callback); }
    virtual void doAfterNextVisibleContentRectAndStablePresentationUpdate(JSValueRef callback) { doAsyncTask(callback); }

    virtual void doAfterDoubleTapDelay(JSValueRef callback) { doAsyncTask(callback); }

    // Preferences

    virtual void overridePreference(JSStringRef, JSStringRef) { notImplemented(); }

    // Zooming

    virtual void zoomToScale(double, JSValueRef) { notImplemented(); }
    virtual void immediateZoomToScale(double) { notImplemented(); }
    virtual void setViewScale(double) { notImplemented(); }
    virtual double zoomScale() const { notImplemented(); return 1; }
    virtual double minimumZoomScale() const { notImplemented(); return 1; }
    virtual double maximumZoomScale() const { notImplemented(); return 1; }

    // Viewports

    virtual void setMinimumEffectiveWidth(double) { notImplemented(); }
    virtual void setAllowsViewportShrinkToFit(bool) { notImplemented(); }

    virtual void setScrollViewKeyboardAvoidanceEnabled(bool) { notImplemented(); }

    virtual std::optional<bool> stableStateOverride() const { notImplemented(); return std::nullopt; }
    virtual void setStableStateOverride(std::optional<bool>) { notImplemented(); }

    virtual JSObjectRef contentVisibleRect() const { notImplemented(); return nullptr; }
    
    virtual void setSafeAreaInsets(double, double, double, double) { notImplemented(); }

    virtual void beginInteractiveObscuredInsetsChange() { notImplemented(); }
    virtual void endInteractiveObscuredInsetsChange() { notImplemented(); }
    virtual void setObscuredInsets(double, double, double, double) { notImplemented(); }

    // View Parenting and Visibility

    virtual void becomeFirstResponder() { notImplemented(); }
    virtual void resignFirstResponder() { notImplemented(); }

    virtual void copyText(JSStringRef) { notImplemented(); }
    virtual void paste() { notImplemented(); }
    virtual int64_t pasteboardChangeCount() const { return 0; }

    virtual void chooseMenuAction(JSStringRef, JSValueRef);
    virtual void dismissMenu();

    virtual void firstResponderSuppressionForWebView(bool) { notImplemented(); }
    virtual void makeWindowContentViewFirstResponder() { notImplemented(); }
    virtual bool isWindowContentViewFirstResponder() const { notImplemented(); return false; }
    virtual bool isWebContentFirstResponder() const { notImplemented(); return false; }

    virtual void setInlinePrediction(JSStringRef, unsigned) { notImplemented(); }
    virtual void acceptInlinePrediction() { notImplemented(); }

    virtual void removeViewFromWindow(JSValueRef) { notImplemented(); }
    virtual void addViewToWindow(JSValueRef) { notImplemented(); }

    virtual void resizeWindowTo(unsigned /* width */, unsigned /* height */) { notImplemented(); }

    virtual void installTapGestureOnWindow(JSValueRef) { notImplemented(); }

    // Editable region

    virtual bool mayContainEditableElementsInRect(unsigned, unsigned, unsigned, unsigned) { notImplemented(); return false; }

    // Compositing

    virtual JSObjectRef propertiesOfLayerWithID(uint64_t) const { notImplemented(); return nullptr; }
    virtual unsigned long countOfUpdatesWithLayerChanges() const { notImplemented(); return 0; }

    // Scrolling

    virtual bool scrollUpdatesDisabled() const { notImplemented(); return false; }
    virtual void setScrollUpdatesDisabled(bool) { notImplemented(); }

    virtual bool isZoomingOrScrolling() const { notImplemented(); return false; }

    virtual void scrollToOffset(long, long, ScrollToOptions*) { notImplemented(); }

    virtual void immediateScrollToOffset(long, long, ScrollToOptions*) { notImplemented(); }
    virtual void immediateScrollElementAtContentPointToOffset(long, long, long, long) { notImplemented(); }

    virtual double contentOffsetX() const { notImplemented(); return 0; }
    virtual double contentOffsetY() const { notImplemented(); return 0; }

    virtual JSObjectRef adjustedContentInset() const { notImplemented(); return nullptr; }

    virtual JSRetainPtr<JSStringRef> scrollingTreeAsText() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> uiViewTreeAsText() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> caLayerTreeAsText() const { notImplemented(); return nullptr; }
    
    virtual JSRetainPtr<JSStringRef> scrollbarStateForScrollingNodeID(unsigned long long, unsigned long long, bool) const { notImplemented(); return nullptr; }

    // Touches

    virtual void touchDownAtPoint(long, long, long, JSValueRef) { notImplemented(); }
    virtual void liftUpAtPoint(long, long, long, JSValueRef) { notImplemented(); }
    virtual void singleTapAtPoint(long, long, JSValueRef) { notImplemented(); }
    virtual void singleTapAtPointWithModifiers(long, long, JSValueRef, JSValueRef) { notImplemented(); }
    virtual void twoFingerSingleTapAtPoint(long, long, JSValueRef) { notImplemented(); }
    virtual void doubleTapAtPoint(long, long, float, JSValueRef) { notImplemented(); }
    virtual void dragFromPointToPoint(long, long, long, long, double, JSValueRef) { notImplemented(); }
    virtual void longPressAtPoint(long, long, JSValueRef) { notImplemented(); }

    virtual void activateAtPoint(long, long, JSValueRef) { notImplemented(); }

    // Keyboard

    virtual void enterText(JSStringRef) { notImplemented(); }
    virtual void typeCharacterUsingHardwareKeyboard(JSStringRef, JSValueRef) { notImplemented(); }

    virtual void keyDown(JSStringRef, JSValueRef) { notImplemented(); }
    virtual void toggleCapsLock(JSValueRef) { notImplemented(); }
    virtual void setContinuousSpellCheckingEnabled(bool) { notImplemented(); }
    virtual void setSpellCheckerResults(JSValueRef) { notImplemented(); }
    virtual unsigned keyboardWillHideCount() const
    {
        notImplemented();
        return 0;
    }
    virtual bool keyboardIsAutomaticallyShifted() const
    {
        notImplemented();
        return false;
    }

    virtual bool isAnimatingDragCancel() const
    {
        notImplemented();
        return false;
    }

    virtual JSRetainPtr<JSStringRef> selectionCaretBackgroundColor() const
    {
        notImplemented();
        return { };
    }

    virtual JSObjectRef tapHighlightViewRect() const
    {
        notImplemented();
        return nullptr;
    }

    virtual void setWebViewEditable(bool) { }

    virtual void setWebViewAllowsMagnification(bool) { }

    virtual void rawKeyDown(JSStringRef) { notImplemented(); }
    virtual void rawKeyUp(JSStringRef) { notImplemented(); }

    virtual void keyboardAccessoryBarNext() { notImplemented(); }
    virtual void keyboardAccessoryBarPrevious() { notImplemented(); }

    virtual void selectWordForReplacement() { notImplemented(); }
    virtual void applyAutocorrection(JSStringRef, JSStringRef, JSValueRef, bool) { notImplemented(); }

    virtual bool isShowingKeyboard() const { notImplemented(); return false; }
    virtual bool hasInputSession() const { notImplemented(); return false; }

    virtual void setHardwareKeyboardAttached(bool) { }

    virtual void setKeyboardInputModeIdentifier(JSStringRef) { notImplemented(); }

    virtual void replaceTextAtRange(JSStringRef, int, int) { notImplemented(); }

    virtual bool windowIsKey() const { notImplemented(); return false; }
    virtual void setWindowIsKey(bool) { notImplemented(); }

    virtual bool suppressSoftwareKeyboard() const { notImplemented(); return false; }
    virtual void setSuppressSoftwareKeyboard(bool) { notImplemented(); }

    // Stylus

    virtual void stylusDownAtPoint(long, long, float, float, float, JSValueRef) { notImplemented(); }
    virtual void stylusMoveToPoint(long, long, float, float, float, JSValueRef) { notImplemented(); }
    virtual void stylusUpAtPoint(long, long, JSValueRef) { notImplemented(); }
    virtual void stylusTapAtPoint(long, long, float, float, float, JSValueRef) { notImplemented(); }
    virtual void stylusTapAtPointWithModifiers(long, long, float, float, float, JSValueRef, JSValueRef) { notImplemented(); }

    // Event Stream

    virtual void sendEventStream(JSStringRef, JSValueRef) { notImplemented(); }
    virtual void playBackEventStream(JSStringRef, JSValueRef) { notImplemented(); }

    // Form Controls
    
    virtual JSObjectRef filePickerAcceptedTypeIdentifiers() { notImplemented(); return nullptr; }
    virtual void dismissFilePicker(JSValueRef) { notImplemented(); }
    virtual void dismissFormAccessoryView() { notImplemented(); }
    virtual void selectFormAccessoryPickerRow(long) { notImplemented(); }
    virtual bool selectFormAccessoryHasCheckedItemAtRow(long) const { return false; }
    virtual JSRetainPtr<JSStringRef> textContentType() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> selectFormPopoverTitle() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> formInputLabel() const { notImplemented(); return nullptr; }
    virtual void setTimePickerValue(long, long) { notImplemented(); }
    virtual double timePickerValueHour() const { notImplemented(); return -1; }
    virtual double timePickerValueMinute() const { notImplemented(); return -1; }
    virtual bool isShowingDateTimePicker() const { notImplemented(); return false; }
    virtual double dateTimePickerValue() const { notImplemented(); return 0; }
    virtual void chooseDateTimePickerValue() { notImplemented(); }
    virtual bool isShowingDataListSuggestions() const { notImplemented(); return false; }
    virtual JSObjectRef calendarType() const { notImplemented(); return nullptr; }
    virtual void setDefaultCalendarType(JSStringRef, JSStringRef) { notImplemented(); }
    virtual JSObjectRef inputViewBounds() const { notImplemented(); return nullptr; }
    virtual void activateDataListSuggestion(unsigned, JSValueRef) { notImplemented(); }
    virtual void setSelectedColorForColorPicker(double, double, double) { notImplemented(); }
    virtual void setAppAccentColor(unsigned short, unsigned short, unsigned short) { notImplemented(); }

    // Find in Page

    virtual void findString(JSStringRef, unsigned long, unsigned long) { notImplemented(); }

    virtual void presentFindNavigator() { notImplemented(); }
    virtual void dismissFindNavigator() { notImplemented(); }

    // Accessibility

    virtual void simulateAccessibilitySettingsChangeNotification(JSValueRef) { notImplemented(); }
    virtual void retrieveSpeakSelectionContent(JSValueRef) { notImplemented(); }
    virtual JSRetainPtr<JSStringRef> accessibilitySpeakSelectionContent() const { notImplemented(); return nullptr; }
    virtual JSObjectRef contentsOfUserInterfaceItem(JSStringRef) const { notImplemented(); return nullptr; }

    // Swipe

    virtual void beginBackSwipe(JSValueRef) { notImplemented(); }
    virtual void completeBackSwipe(JSValueRef) { notImplemented(); }

    // Child View Controllers

    virtual bool isShowingFormValidationBubble() const { notImplemented(); return false; }
    virtual bool isShowingPopover() const { notImplemented(); return false; }
    virtual bool isPresentingModally() const { notImplemented(); return false; }

    // Context Menus

    virtual bool isDismissingMenu() const { notImplemented(); return false; }
    virtual bool isShowingMenu() const { notImplemented(); return false; }
    virtual JSObjectRef rectForMenuAction(JSStringRef) const { notImplemented(); return nullptr; }
    virtual JSObjectRef menuRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef contextMenuPreviewRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef contextMenuRect() const { notImplemented(); return nullptr; }
    virtual bool isShowingContextMenu() const { notImplemented(); return false; }

    // Selection

    virtual JSObjectRef textSelectionRangeRects() const { notImplemented(); return nullptr; }
    virtual JSObjectRef textSelectionCaretRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionStartGrabberViewRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionEndGrabberViewRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionEndGrabberViewShapePathDescription() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionCaretViewRect() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionCaretViewRectInGlobalCoordinates() const { notImplemented(); return nullptr; }
    virtual JSObjectRef selectionRangeViewRects() const { notImplemented(); return nullptr; }

    // Rotation

    virtual void simulateRotation(DeviceOrientation*, JSValueRef) { notImplemented(); }
    virtual void simulateRotationLikeSafari(DeviceOrientation*, JSValueRef) { notImplemented(); }

    // Undo/Redo

    virtual JSRetainPtr<JSStringRef> lastUndoLabel() const { notImplemented(); return nullptr; }
    virtual JSRetainPtr<JSStringRef> firstRedoLabel() const { notImplemented(); return nullptr; }

    // Attachment Elements

    virtual JSObjectRef attachmentInfo(JSStringRef) { notImplemented(); return nullptr; }
    virtual void insertAttachmentForFilePath(JSStringRef, JSStringRef, JSValueRef) { notImplemented(); }

    // Contact Picker

    virtual void setDidShowContactPickerCallback(JSValueRef);
    JSValueRef didShowContactPickerCallback() const;
    virtual bool isShowingContactPicker() const { notImplemented(); return false; }

    virtual void setDidHideContactPickerCallback(JSValueRef);
    JSValueRef didHideContactPickerCallback() const;

    virtual void dismissContactPickerWithContacts(JSValueRef) { notImplemented(); }

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

    virtual void setWillStartInputSessionCallback(JSValueRef);
    JSValueRef willStartInputSessionCallback() const;

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

    // Image Analysis

    virtual uint64_t currentImageAnalysisRequestID() const { return 0; }
    virtual void installFakeMachineReadableCodeResultsForImageAnalysis() { }

    // Text Extraction
    virtual void requestTextExtraction(JSValueRef, TextExtractionOptions*) { notImplemented(); }

    // Element Targeting
    virtual void requestRenderedTextForFrontmostTarget(int, int, JSValueRef) { notImplemented(); }
    virtual void adjustVisibilityForFrontmostTarget(int, int, JSValueRef) { notImplemented(); }
    virtual void resetVisibilityAdjustments(JSValueRef) { notImplemented(); }

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
