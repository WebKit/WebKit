/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WKContentView.h"

#import "AssistedNodeInformation.h"
#import "EditorState.h"
#import "GestureTypes.h"
#import "InteractionInformationAtPosition.h"
#import "UIKitSPI.h"
#import "WKActionSheetAssistant.h"
#import "WKAirPlayRoutePicker.h"
#import "WKFileUploadPanel.h"
#import "WKFormPeripheral.h"
#import "WKSyntheticClickTapGestureRecognizer.h"
#import <UIKit/UIView.h>
#import <WebCore/Color.h>
#import <WebCore/FloatQuad.h>
#import <wtf/Forward.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

namespace WebCore {
class Color;
class FloatQuad;
class IntSize;
}

namespace WebKit {
class NativeWebTouchEvent;
class SmartMagnificationController;
class WebOpenPanelParameters;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
}

@class WKActionSheetAssistant;
@class WKFormInputSession;
@class WKInspectorNodeSearchGestureRecognizer;
@class WebIOSEvent;
@class _UIHighlightView;
@class _UIWebHighlightLongPressGestureRecognizer;

typedef void (^UIWKAutocorrectionCompletionHandler)(UIWKAutocorrectionRects *rectsForInput);
typedef void (^UIWKAutocorrectionContextHandler)(UIWKAutocorrectionContext *autocorrectionContext);
typedef void (^UIWKDictationContextHandler)(NSString *selectedText, NSString *beforeText, NSString *afterText);
typedef void (^UIWKSelectionCompletionHandler)(void);
typedef void (^UIWKSelectionWithDirectionCompletionHandler)(BOOL selectionEndIsMoving);
typedef void (^UIWKKeyWebEventCompletionHandler)(WebIOSEvent *theEvent, BOOL wasHandled);

namespace WebKit {
struct WKSelectionDrawingInfo {
    enum class SelectionType { None, Plugin, Range };
    WKSelectionDrawingInfo();
    explicit WKSelectionDrawingInfo(const EditorState&);
    SelectionType type;
    WebCore::IntRect caretRect;
    Vector<WebCore::SelectionRect> selectionRects;
};
struct WKAutoCorrectionData {
    String fontName;
    CGFloat fontSize;
    uint64_t fontTraits;
    CGRect textFirstRect;
    CGRect textLastRect;
    UIWKAutocorrectionCompletionHandler autocorrectionHandler;
    UIWKAutocorrectionContextHandler autocorrectionContextHandler;
};

}

@interface WKContentView () {
    RetainPtr<UIWebTouchEventsGestureRecognizer> _touchEventGestureRecognizer;

    BOOL _canSendTouchEventsAsynchronously;

    RetainPtr<WKSyntheticClickTapGestureRecognizer> _singleTapGestureRecognizer;
    RetainPtr<_UIWebHighlightLongPressGestureRecognizer> _highlightLongPressGestureRecognizer;
    RetainPtr<UILongPressGestureRecognizer> _longPressGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _doubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _nonBlockingDoubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _twoFingerDoubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _twoFingerSingleTapGestureRecognizer;
    RetainPtr<WKInspectorNodeSearchGestureRecognizer> _inspectorNodeSearchGestureRecognizer;

    RetainPtr<UIWKTextInteractionAssistant> _textSelectionAssistant;
    RetainPtr<UIWKSelectionAssistant> _webSelectionAssistant;

    RetainPtr<UITextInputTraits> _traits;
    RetainPtr<UIWebFormAccessory> _formAccessoryView;
    RetainPtr<_UIHighlightView> _highlightView;
    RetainPtr<UIView> _interactionViewsContainerView;
    RetainPtr<NSString> _markedText;
    RetainPtr<WKActionSheetAssistant> _actionSheetAssistant;
    RetainPtr<WKAirPlayRoutePicker> _airPlayRoutePicker;
    RetainPtr<WKFormInputSession> _formInputSession;
    RetainPtr<WKFileUploadPanel> _fileUploadPanel;
    RetainPtr<UIGestureRecognizer> _previewGestureRecognizer;
    RetainPtr<UIGestureRecognizer> _previewSecondaryGestureRecognizer;
#if HAVE(LINK_PREVIEW)
    RetainPtr<UIPreviewItemController> _previewItemController;
#endif

    std::unique_ptr<WebKit::SmartMagnificationController> _smartMagnificationController;

    id <UITextInputDelegate> _inputDelegate;

    uint64_t _latestTapID;
    struct TapHighlightInformation {
        WebCore::Color color;
        Vector<WebCore::FloatQuad> quads;
        WebCore::IntSize topLeftRadius;
        WebCore::IntSize topRightRadius;
        WebCore::IntSize bottomLeftRadius;
        WebCore::IntSize bottomRightRadius;
    };
    TapHighlightInformation _tapHighlightInformation;

    WebKit::WKAutoCorrectionData _autocorrectionData;
    WebKit::InteractionInformationAtPosition _positionInformation;
    WebKit::AssistedNodeInformation _assistedNodeInformation;
    RetainPtr<NSObject<WKFormPeripheral>> _inputPeripheral;
    RetainPtr<UIEvent> _uiEventBeingResent;
    UIWKKeyWebEventCompletionHandler _keyWebEventHandler;

    CGPoint _lastInteractionLocation;

    WebKit::WKSelectionDrawingInfo _lastSelectionDrawingInfo;

    BOOL _isEditable;
    BOOL _showingTextStyleOptions;
    BOOL _hasValidPositionInformation;
    BOOL _isTapHighlightIDValid;
    BOOL _potentialTapInProgress;
    BOOL _isDoubleTapPending;
    BOOL _highlightLongPressCanClick;
    BOOL _hasTapHighlightForPotentialTap;
    BOOL _selectionNeedsUpdate;
    BOOL _shouldRestoreSelection;
    BOOL _usingGestureForSelection;
    BOOL _inspectorNodeSearchEnabled;
    BOOL _didAccessoryTabInitiateFocus;
    BOOL _isExpectingFastSingleTapCommit;
    BOOL _showDebugTapHighlightsForFastClicking;

    BOOL _resigningFirstResponder;
}

@end

@interface WKContentView (WKInteraction) <UIGestureRecognizerDelegate, UIWebTouchEventsGestureRecognizerDelegate, UITextInputPrivate, UIWebFormAccessoryDelegate, UIWKInteractionViewProtocol, WKFileUploadPanelDelegate, WKActionSheetAssistantDelegate>

@property (nonatomic, readonly) CGPoint lastInteractionLocation;
@property (nonatomic, readonly) BOOL isEditable;
@property (nonatomic, readonly) const WebKit::InteractionInformationAtPosition& positionInformation;
@property (nonatomic, readonly) const WebKit::WKAutoCorrectionData& autocorrectionData;
@property (nonatomic, readonly) const WebKit::AssistedNodeInformation& assistedNodeInformation;
@property (nonatomic, readonly) UIWebFormAccessory *formAccessoryView;

- (void)setupInteraction;
- (void)cleanupInteraction;

- (void)scrollViewWillStartPanOrPinchGesture;

#if ENABLE(TOUCH_EVENTS)
- (void)_webTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsDefault;
#endif
- (void)_commitPotentialTapFailed;
- (void)_didNotHandleTapAsClick:(const WebCore::IntPoint&)point;
- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius;

- (BOOL)_mayDisableDoubleTapGesturesDuringSingleTap;
- (void)_disableDoubleTapGesturesDuringTapIfNecessary:(uint64_t)requestID;
- (void)_startAssistingNode:(const WebKit::AssistedNodeInformation&)information userIsInteracting:(BOOL)userIsInteracting blurPreviousNode:(BOOL)blurPreviousNode userObject:(NSObject <NSSecureCoding> *)userObject;
- (void)_stopAssistingNode;
- (void)_selectionChanged;
- (void)_updateChangedSelection;
- (BOOL)_interpretKeyEvent:(WebIOSEvent *)theEvent isCharEvent:(BOOL)isCharEvent;
- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
- (void)_attemptClickAtLocation:(CGPoint)location;
- (void)_willStartScrollingOrZooming;
- (void)_didScroll;
- (void)_didEndScrollingOrZooming;
- (void)_overflowScrollingWillBegin;
- (void)_overflowScrollingDidEnd;
- (void)_didUpdateBlockSelectionWithTouch:(WebKit::SelectionTouch)touch withFlags:(WebKit::SelectionFlags)flags growThreshold:(CGFloat)growThreshold shrinkThreshold:(CGFloat)shrinkThreshold;
- (void)_showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect;
- (void)_showRunOpenPanel:(WebKit::WebOpenPanelParameters*)parameters resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener;
- (void)accessoryDone;
- (void)_didHandleKeyEvent:(WebIOSEvent *)event eventWasHandled:(BOOL)eventWasHandled;
- (Vector<WebKit::OptionItem>&) assistedNodeSelectOptions;
- (void)_enableInspectorNodeSearch;
- (void)_disableInspectorNodeSearch;
- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler;
- (void)_setDoubleTapGesturesEnabled:(BOOL)enabled;
- (NSArray *)_dataDetectionResults;
@end

#if HAVE(LINK_PREVIEW)
@interface WKContentView (WKInteractionPreview) <UIPreviewItemDelegate>

- (void)_registerPreview;
- (void)_unregisterPreview;
@end
#endif

#endif // PLATFORM(IOS)
