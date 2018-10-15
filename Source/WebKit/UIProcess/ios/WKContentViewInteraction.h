/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
#import "WKShareSheet.h"
#endif

#import "AssistedNodeInformation.h"
#import "DragDropInteractionState.h"
#import "EditorState.h"
#import "GestureTypes.h"
#import "InteractionInformationAtPosition.h"
#import "UIKitSPI.h"
#import "WKActionSheetAssistant.h"
#import "WKAirPlayRoutePicker.h"
#import "WKFileUploadPanel.h"
#import "WKFormPeripheral.h"
#import "WKKeyboardScrollingAnimator.h"
#import "WKShareSheet.h"
#import "WKSyntheticClickTapGestureRecognizer.h"
#import "_WKFormInputSession.h"
#import <UIKit/UIView.h>
#import <WebCore/Color.h>
#import <WebCore/FloatQuad.h>
#import <wtf/BlockPtr.h>
#import <wtf/Forward.h>
#import <wtf/Vector.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

namespace API {
class OpenPanelParameters;
}

namespace WTF {
class TextStream;
}

namespace WebCore {
class Color;
class FloatQuad;
class IntSize;
class SelectionRect;
struct PromisedAttachmentInfo;
struct ShareDataWithParsedURL;
enum class RouteSharingPolicy : uint8_t;
}

#if ENABLE(DRAG_SUPPORT)
namespace WebCore {
struct DragItem;
}
#endif

namespace WebKit {
class InputViewUpdateDeferrer;
class NativeWebTouchEvent;
class SmartMagnificationController;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
}

@class _UIHighlightView;
@class _UIWebHighlightLongPressGestureRecognizer;
@class UIHoverGestureRecognizer;
@class WebEvent;
@class WKActionSheetAssistant;
@class WKFocusedFormControlView;
@class WKFormInputSession;
@class WKInspectorNodeSearchGestureRecognizer;

typedef void (^UIWKAutocorrectionCompletionHandler)(UIWKAutocorrectionRects *rectsForInput);
typedef void (^UIWKAutocorrectionContextHandler)(UIWKAutocorrectionContext *autocorrectionContext);
typedef void (^UIWKDictationContextHandler)(NSString *selectedText, NSString *beforeText, NSString *afterText);
typedef void (^UIWKSelectionCompletionHandler)(void);
typedef void (^UIWKSelectionWithDirectionCompletionHandler)(BOOL selectionEndIsMoving);

typedef BlockPtr<void(WebKit::InteractionInformationAtPosition)> InteractionInformationCallback;
typedef std::pair<WebKit::InteractionInformationRequest, InteractionInformationCallback> InteractionInformationRequestAndCallback;

#define FOR_EACH_WKCONTENTVIEW_ACTION(M) \
    M(_addShortcut) \
    M(_define) \
    M(_lookup) \
    M(_promptForReplace) \
    M(_share) \
    M(_showTextStyleOptions) \
    M(_transliterateChinese) \
    M(copy) \
    M(cut) \
    M(paste) \
    M(replace) \
    M(select) \
    M(selectAll) \
    M(toggleBoldface) \
    M(toggleItalics) \
    M(toggleUnderline) \
    M(increaseSize) \
    M(decreaseSize) \
    M(toggleStrikeThrough) \
    M(insertUnorderedList) \
    M(insertOrderedList) \
    M(indent) \
    M(outdent) \
    M(alignLeft) \
    M(alignRight) \
    M(alignCenter) \
    M(alignJustified)

namespace WebKit {

struct WKSelectionDrawingInfo {
    enum class SelectionType { None, Plugin, Range };
    WKSelectionDrawingInfo();
    explicit WKSelectionDrawingInfo(const EditorState&);
    SelectionType type;
    WebCore::IntRect caretRect;
    Vector<WebCore::SelectionRect> selectionRects;
};

WTF::TextStream& operator<<(WTF::TextStream&, const WKSelectionDrawingInfo&);

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

@class WKFocusedElementInfo;

@interface WKFormInputSession : NSObject <_WKFormInputSession>

- (instancetype)initWithContentView:(WKContentView *)view focusedElementInfo:(WKFocusedElementInfo *)elementInfo requiresStrongPasswordAssistance:(BOOL)requiresStrongPasswordAssistance;
- (void)endEditing;
- (void)invalidate;

@end

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

#if PLATFORM(IOSMAC)
    RetainPtr<UIHoverGestureRecognizer> _hoverGestureRecognizer;
#endif

    RetainPtr<UIWKTextInteractionAssistant> _textSelectionAssistant;
    RetainPtr<UIWKSelectionAssistant> _webSelectionAssistant;
    BOOL _suppressAssistantSelectionView;

    RetainPtr<UITextInputTraits> _traits;
    RetainPtr<UIWebFormAccessory> _formAccessoryView;
    RetainPtr<_UIHighlightView> _highlightView;
    RetainPtr<UIView> _interactionViewsContainerView;
    RetainPtr<NSString> _markedText;
    RetainPtr<WKActionSheetAssistant> _actionSheetAssistant;
#if ENABLE(AIRPLAY_PICKER)
    RetainPtr<WKAirPlayRoutePicker> _airPlayRoutePicker;
#endif
    RetainPtr<WKFormInputSession> _formInputSession;
    RetainPtr<WKFileUploadPanel> _fileUploadPanel;
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    RetainPtr<WKShareSheet> _shareSheet;
#endif
    RetainPtr<UIGestureRecognizer> _previewGestureRecognizer;
    RetainPtr<UIGestureRecognizer> _previewSecondaryGestureRecognizer;
    Vector<bool> _focusStateStack;
#if HAVE(LINK_PREVIEW)
    RetainPtr<UIPreviewItemController> _previewItemController;
#endif

    std::unique_ptr<WebKit::SmartMagnificationController> _smartMagnificationController;

    WeakObjCPtr<id <UITextInputDelegate>> _inputDelegate;

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
    BlockPtr<void(::WebEvent *, BOOL)> _keyWebEventHandler;

    CGPoint _lastInteractionLocation;
    uint64_t _layerTreeTransactionIdAtLastTouchStart;

    WebKit::WKSelectionDrawingInfo _lastSelectionDrawingInfo;

    std::optional<WebKit::InteractionInformationRequest> _outstandingPositionInformationRequest;

    uint64_t _positionInformationCallbackDepth;
    Vector<std::optional<InteractionInformationRequestAndCallback>> _pendingPositionInformationHandlers;
    
    std::unique_ptr<WebKit::InputViewUpdateDeferrer> _inputViewUpdateDeferrer;

    RetainPtr<WKKeyboardScrollViewAnimator> _keyboardScrollingAnimator;

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

    BOOL _becomingFirstResponder;
    BOOL _resigningFirstResponder;
    BOOL _needsDeferredEndScrollingSelectionUpdate;
    BOOL _isChangingFocus;
    BOOL _isBlurringFocusedNode;

    BOOL _focusRequiresStrongPasswordAssistance;

#if ENABLE(DATA_INTERACTION)
    WebKit::DragDropInteractionState _dragDropInteractionState;
    RetainPtr<UIDragInteraction> _dragInteraction;
    RetainPtr<UIDropInteraction> _dropInteraction;
    BOOL _shouldRestoreCalloutBarAfterDrop;
    BOOL _isAnimatingConcludeEditDrag;
    RetainPtr<UIView> _visibleContentViewSnapshot;
    RetainPtr<_UITextDragCaretView> _editDropCaretView;
#endif

#if PLATFORM(WATCHOS)
    RetainPtr<WKFocusedFormControlView> _focusedFormControlView;
    RetainPtr<UIViewController> _presentedFullScreenInputViewController;
    RetainPtr<UINavigationController> _inputNavigationViewControllerForFullScreenInputs;

    BOOL _shouldRestoreFirstResponderStatusAfterLosingFocus;
    BlockPtr<void()> _activeFocusedStateRetainBlock;
#endif
}

@end

@interface WKContentView (WKInteraction) <UIGestureRecognizerDelegate, UITextAutoscrolling, UITextInputMultiDocument, UITextInputPrivate, UIWebFormAccessoryDelegate, UIWebTouchEventsGestureRecognizerDelegate, UIWKInteractionViewProtocol, WKActionSheetAssistantDelegate, WKFileUploadPanelDelegate, WKKeyboardScrollViewAnimatorDelegate
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    , WKShareSheetDelegate
#endif
#if ENABLE(DATA_INTERACTION)
    , UIDragInteractionDelegate, UIDropInteractionDelegate
#endif
>

@property (nonatomic, readonly) CGPoint lastInteractionLocation;
@property (nonatomic, readonly) BOOL isEditable;
@property (nonatomic, readonly) BOOL shouldHideSelectionWhenScrolling;
@property (nonatomic, readonly) const WebKit::InteractionInformationAtPosition& positionInformation;
@property (nonatomic, readonly) const WebKit::WKAutoCorrectionData& autocorrectionData;
@property (nonatomic, readonly) const WebKit::AssistedNodeInformation& assistedNodeInformation;
@property (nonatomic, readonly) UIWebFormAccessory *formAccessoryView;
@property (nonatomic) BOOL suppressAssistantSelectionView;

- (void)setupInteraction;
- (void)cleanupInteraction;

- (void)scrollViewWillStartPanOrPinchGesture;

- (BOOL)canBecomeFirstResponderForWebView;
- (BOOL)becomeFirstResponderForWebView;
- (BOOL)resignFirstResponderForWebView;
- (BOOL)canPerformActionForWebView:(SEL)action withSender:(id)sender;
- (id)targetForActionForWebView:(SEL)action withSender:(id)sender;

#define DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW(_action) \
    - (void)_action ## ForWebView:(id)sender;
FOR_EACH_WKCONTENTVIEW_ACTION(DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW)
DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW(_pasteAsQuotation)
#undef DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW

- (void)setFontForWebView:(UIFont *)fontFamily sender:(id)sender;
- (void)setFontSizeForWebView:(CGFloat)fontSize sender:(id)sender;
- (void)setTextColorForWebView:(UIColor *)color sender:(id)sender;

#if ENABLE(TOUCH_EVENTS)
- (void)_webTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsDefault;
#endif
- (void)_commitPotentialTapFailed;
- (void)_didNotHandleTapAsClick:(const WebCore::IntPoint&)point;
- (void)_didCompleteSyntheticClick;
- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius;

- (BOOL)_mayDisableDoubleTapGesturesDuringSingleTap;
- (void)_disableDoubleTapGesturesDuringTapIfNecessary:(uint64_t)requestID;
- (void)_startAssistingNode:(const WebKit::AssistedNodeInformation&)information userIsInteracting:(BOOL)userIsInteracting blurPreviousNode:(BOOL)blurPreviousNode changingActivityState:(BOOL)changingActivityState userObject:(NSObject <NSSecureCoding> *)userObject;
- (void)_stopAssistingNode;
- (void)_selectionChanged;
- (void)_updateChangedSelection;
- (BOOL)_interpretKeyEvent:(::WebEvent *)theEvent isCharEvent:(BOOL)isCharEvent;
- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
- (void)_attemptClickAtLocation:(CGPoint)location;
- (void)_willStartScrollingOrZooming;
- (void)_didScroll;
- (void)_didEndScrollingOrZooming;
- (void)_scrollingNodeScrollingWillBegin;
- (void)_scrollingNodeScrollingDidEnd;
- (void)_showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect routeSharingPolicy:(WebCore::RouteSharingPolicy)policy routingContextUID:(NSString *)contextUID;
- (void)_showRunOpenPanel:(API::OpenPanelParameters*)parameters resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener;
- (void)_showShareSheet:(const WebCore::ShareDataWithParsedURL&)shareData completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler;
- (void)accessoryDone;
- (void)_didHandleKeyEvent:(::WebEvent *)event eventWasHandled:(BOOL)eventWasHandled;
- (Vector<WebKit::OptionItem>&) assistedNodeSelectOptions;
- (void)_enableInspectorNodeSearch;
- (void)_disableInspectorNodeSearch;
- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler;
- (void)_setDoubleTapGesturesEnabled:(BOOL)enabled;
#if ENABLE(DATA_DETECTION)
- (NSArray *)_dataDetectionResults;
#endif
- (NSArray<NSValue *> *)_uiTextSelectionRects;
- (void)accessibilityRetrieveSpeakSelectionContent;
- (void)_accessibilityRetrieveRectsEnclosingSelectionOffset:(NSInteger)offset withGranularity:(UITextGranularity)granularity;
- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text completionHandler:(void (^)(const Vector<WebCore::SelectionRect>& rects))completionHandler;
- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text;
- (void)_accessibilityStoreSelection;
- (void)_accessibilityClearSelection;
- (WKFormInputSession *)_formInputSession;

@property (nonatomic, readonly) WebKit::InteractionInformationAtPosition currentPositionInformation;
- (void)doAfterPositionInformationUpdate:(void (^)(WebKit::InteractionInformationAtPosition))action forRequest:(WebKit::InteractionInformationRequest)request;
- (BOOL)ensurePositionInformationIsUpToDate:(WebKit::InteractionInformationRequest)request;

#if ENABLE(DATA_INTERACTION)
- (void)_didChangeDragInteractionPolicy;
- (void)_didPerformDragOperation:(BOOL)handled;
- (void)_didHandleStartDataInteractionRequest:(BOOL)started;
- (void)_didHandleAdditionalDragItemsRequest:(BOOL)added;
- (void)_startDrag:(RetainPtr<CGImageRef>)image item:(const WebCore::DragItem&)item;
- (void)_didConcludeEditDataInteraction:(std::optional<WebCore::TextIndicatorData>)data;
- (void)_didChangeDataInteractionCaretRect:(CGRect)previousRect currentRect:(CGRect)rect;
#endif

- (void)reloadContextViewForPresentedListViewController;

@end

@interface WKContentView (WKTesting)

- (void)_simulateLongPressActionAtLocation:(CGPoint)location;
- (void)_simulateTextEntered:(NSString *)text;
- (void)selectFormAccessoryPickerRow:(NSInteger)rowIndex;
- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute;
- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem;

@property (nonatomic, readonly) NSString *textContentTypeForTesting;
@property (nonatomic, readonly) NSString *selectFormPopoverTitle;
@property (nonatomic, readonly) NSString *formInputLabel;

@end

#if HAVE(LINK_PREVIEW)
@interface WKContentView (WKInteractionPreview) <UIPreviewItemDelegate>

- (void)_registerPreview;
- (void)_unregisterPreview;
@end
#endif

@interface WKContentView (WKFileUploadPanel)
+ (Class)_fileUploadPanelClass;
@end

#endif // PLATFORM(IOS)
