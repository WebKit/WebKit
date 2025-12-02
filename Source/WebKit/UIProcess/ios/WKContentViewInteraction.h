/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WKContentView.h"

#import "DragDropInteractionState.h"
#import "EditorState.h"
#import "FocusedElementInformation.h"
#import "FrameInfoData.h"
#import "GestureRecognizerConsistencyEnforcer.h"
#import "GestureTypes.h"
#import "IdentifierTypes.h"
#import "ImageAnalysisUtilities.h"
#import "InteractionInformationAtPosition.h"
#import "PasteboardAccessIntent.h"
#import "RevealFocusedElementDeferrer.h"
#if ENABLE(MODEL_PROCESS)
#import "StageModeInteractionState.h"
#endif
#import "SyntheticEditingCommandType.h"
#import "TextCheckingController.h"
#import "TransactionID.h"
#import "UIKitSPI.h"
#import "WKBrowserEngineDefinitions.h"
#import "WKMouseInteraction.h"
#import "WKTextAnimationManagerIOS.h"
#import "WKTextSelectionRect.h"
#import <WebKit/WKActionSheetAssistant.h>
#import <WebKit/WKAirPlayRoutePicker.h>
#import <WebKit/WKContactPicker.h>
#import <WebKit/WKDeferringGestureRecognizer.h>
#import <WebKit/WKFileUploadPanel.h>
#import <WebKit/WKFormAccessoryView.h>
#import <WebKit/WKFormPeripheral.h>
#import <WebKit/WKKeyboardScrollingAnimator.h>
#import <WebKit/WKShareSheet.h>
#import <WebKit/WKSyntheticTapGestureRecognizer.h>
#import <WebKit/WKTouchActionGestureRecognizer.h>
#import <WebKit/WKTouchEventsGestureRecognizer.h>
#import "WebAutocorrectionContext.h"
#import "_WKElementAction.h"
#import "_WKFormInputSession.h"
#import <UIKit/UIView.h>
#import <WebCore/ActivityState.h>
#import <WebCore/Color.h>
#import <WebCore/DataOwnerType.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/MediaControlsContextMenuItem.h>
#import <WebCore/PointerID.h>
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <pal/spi/ios/BrowserEngineKitSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/Forward.h>
#import <wtf/Function.h>
#import <wtf/HashSet.h>
#import <wtf/ObjectIdentifier.h>
#import <wtf/OptionSet.h>
#import <wtf/Scope.h>
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
class FloatRect;
class IntSize;
class SelectionRect;
struct ContactInfo;
struct ContactsRequestData;
struct DigitalCredentialsRequestData;
struct MediaPlayerClientIdentifierType;
struct PromisedAttachmentInfo;
struct ShareDataWithParsedURL;
struct TextRecognitionResult;
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
enum class DOMPasteRequiresInteraction : bool;
struct NodeIdentifierType;
enum class MouseEventPolicy : uint8_t;
enum class RouteSharingPolicy : uint8_t;
enum class TextIndicatorDismissalAnimation : uint8_t;

#if ENABLE(DRAG_SUPPORT)
struct DragItem;
#endif

using NodeIdentifier = ObjectIdentifier<NodeIdentifierType>;
using HTMLMediaElementIdentifier = ObjectIdentifier<MediaPlayerClientIdentifierType>;

}

namespace WebKit {
class WebTouchEvent;
class SmartMagnificationController;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
struct KeyEventInterpretationContext;
enum class PickerDismissalReason : uint8_t;
}

@class AVPlayerViewController;
@class QLPreviewController;
@class VKCImageAnalysisInteraction;
@class WebEvent;
@class WebTextIndicatorLayer;
@class WKActionSheetAssistant;
@class WKContextMenuElementInfo;
@class WKDataListSuggestionsControl;
@class WKFocusedFormControlView;
@class WKFormInputSession;
@class WKFormSelectControl;
@class WKHighlightLongPressGestureRecognizer;
@class WKImageAnalysisGestureRecognizer;
@class WKInspectorNodeSearchGestureRecognizer;
@class WKTapHighlightView;
@class WKTargetedPreviewContainer;
@class WKTextInteractionWrapper;
@class WKTextRange;
@class _WKTextInputContext;

@class WKTextAnimationManager;

#if HAVE(DIGITAL_CREDENTIALS_UI)
@class WKDigitalCredentialsPicker;
#endif

#if !PLATFORM(WATCHOS)
@class WKDateTimeInputControl;
#endif

@class UIPointerInteraction;
@class UIPointerRegion;
@class UITargetedPreview;
@class _UILookupGestureRecognizer;

#if HAVE(PEPPER_UI_CORE)
@class PUICQuickboardViewController;
#if HAVE(QUICKBOARD_CONTROLLER)
@class PUICQuickboardController;
#endif
#endif

typedef BlockPtr<void(WebKit::InteractionInformationAtPosition)> InteractionInformationCallback;
typedef std::pair<WebKit::InteractionInformationRequest, InteractionInformationCallback> InteractionInformationRequestAndCallback;

#if ENABLE(IMAGE_ANALYSIS)
#define FOR_EACH_INSERT_TEXT_FROM_CAMERA_WKCONTENTVIEW_ACTION(M) \
    M(captureTextFromCamera)
#else
#define FOR_EACH_INSERT_TEXT_FROM_CAMERA_WKCONTENTVIEW_ACTION(M)
#endif

#if HAVE(UIFINDINTERACTION)
#define FOR_EACH_FIND_WKCONTENTVIEW_ACTION(M) \
    M(useSelectionForFind) \
    M(findSelected) \
    M(_findSelected)
#else
#define FOR_EACH_FIND_WKCONTENTVIEW_ACTION(M)
#endif

#define FOR_EACH_WKCONTENTVIEW_ACTION(M) \
    FOR_EACH_INSERT_TEXT_FROM_CAMERA_WKCONTENTVIEW_ACTION(M) \
    FOR_EACH_FIND_WKCONTENTVIEW_ACTION(M) \
    M(addShortcut) \
    M(_addShortcut) \
    M(define) \
    M(_define) \
    M(lookup) \
    M(_lookup) \
    M(translate) \
    M(_translate) \
    M(promptForReplace) \
    M(_promptForReplace) \
    M(share) \
    M(_share) \
    M(transliterateChinese) \
    M(_transliterateChinese) \
    M(_nextAccessoryTab) \
    M(_previousAccessoryTab) \
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
    M(pasteAndMatchStyle) \
    M(makeTextWritingDirectionNatural) \
    M(makeTextWritingDirectionLeftToRight) \
    M(makeTextWritingDirectionRightToLeft) \
    M(alignCenter) \
    M(alignJustified) \
    M(alignLeft) \
    M(alignRight)

#define FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(M) \
    M(_indent) \
    M(_outdent) \
    M(_toggleStrikeThrough) \
    M(_insertOrderedList) \
    M(_insertUnorderedList) \
    M(_insertNestedOrderedList) \
    M(_insertNestedUnorderedList) \
    M(_increaseListLevel) \
    M(_decreaseListLevel) \
    M(_changeListType) \
    M(_pasteAsQuotation) \
    M(_pasteAndMatchStyle)

namespace WebKit {

enum SuppressSelectionAssistantReason : uint8_t {
    SelectionIsTransparentOrFullyClipped = 1 << 0,
    FocusedElementIsTooSmall = 1 << 1,
    InteractionIsHappening = 1 << 2,
    ShowingFullscreenVideo = 1 << 3,
};

enum class InputViewUpdateDeferralSource : uint8_t {
    BecomeFirstResponder = 1 << 0,
    TapGesture = 1 << 1,
    ChangingFocusedElement = 1 << 2,
};

enum class TargetedPreviewPositioning : uint8_t {
    Default,
    LeadingOrTrailingEdge,
};

using InputViewUpdateDeferralSources = OptionSet<InputViewUpdateDeferralSource>;

struct WKSelectionDrawingInfo {
    enum class SelectionType : bool { None, Range };
    WKSelectionDrawingInfo();
    explicit WKSelectionDrawingInfo(const EditorState&);
    SelectionType type { SelectionType::None };
    WebCore::IntRect caretRect;
    WebCore::Color caretColor;
    Vector<WebCore::SelectionGeometry> selectionGeometries;
    WebCore::IntRect selectionClipRect;
    std::optional<WebCore::PlatformLayerIdentifier> enclosingLayerID;

    enum class ComparisonResult : uint8_t {
        VisuallyDistinct,
        EquivalentExceptForEnclosingLayer,
        EquivalentIncludingEnclosingLayer,
    };
    ComparisonResult compare(const WKSelectionDrawingInfo&) const;
};

WTF::TextStream& operator<<(WTF::TextStream&, const WKSelectionDrawingInfo&);

struct WKAutoCorrectionData {
    RetainPtr<UIFont> font;
    CGRect textFirstRect;
    CGRect textLastRect;
};

struct KeyEventAndCompletionBlock {
    RetainPtr<::WebEvent> event;
    BlockPtr<void(::WebEvent *, BOOL)> completionBlock;
};

enum class RequestAutocorrectionContextResult : bool { Empty, LastContext };

struct RemoveBackgroundData {
    WebCore::ElementContext element;
    RetainPtr<CGImageRef> image;
    String preferredMIMEType;
};

enum class ProceedWithTextSelectionInImage : bool { No, Yes };

enum class DynamicImageAnalysisContextMenuState : uint8_t {
    NotWaiting,
    WaitingForImageAnalysis,
    WaitingForVisibleMenu,
};

enum class ImageAnalysisRequestIdentifierType { };
using ImageAnalysisRequestIdentifier = ObjectIdentifier<ImageAnalysisRequestIdentifierType>;

struct ImageAnalysisContextMenuActionData {
    bool hasSelectableText { false };
    bool hasVisualSearchResults { false };
    RetainPtr<CGImageRef> copySubjectResult;
    RetainPtr<UIMenu> machineReadableCodeMenu;
};

} // namespace WebKit

@class WKExtendedTextInputTraits;
@class WKFocusedElementInfo;
@protocol UIMenuBuilder;
@protocol WKFormControl;

@interface WKFormInputSession : NSObject <_WKFormInputSession>

- (instancetype)initWithContentView:(WKContentView *)view focusedElementInfo:(WKFocusedElementInfo *)elementInfo requiresStrongPasswordAssistance:(BOOL)requiresStrongPasswordAssistance;
- (void)endEditing;
- (void)invalidate;

@end

@interface WKContentView () {
    RetainPtr<WKDeferringGestureRecognizer> _touchStartDeferringGestureRecognizerForImmediatelyResettableGestures;
    RetainPtr<WKDeferringGestureRecognizer> _touchStartDeferringGestureRecognizerForDelayedResettableGestures;
    RetainPtr<WKDeferringGestureRecognizer> _touchStartDeferringGestureRecognizerForSyntheticTapGestures;
    RetainPtr<WKDeferringGestureRecognizer> _touchEndDeferringGestureRecognizerForImmediatelyResettableGestures;
    RetainPtr<WKDeferringGestureRecognizer> _touchEndDeferringGestureRecognizerForDelayedResettableGestures;
    RetainPtr<WKDeferringGestureRecognizer> _touchEndDeferringGestureRecognizerForSyntheticTapGestures;
    RetainPtr<WKDeferringGestureRecognizer> _touchMoveDeferringGestureRecognizer;
    std::optional<HashSet<RetainPtr<WKDeferringGestureRecognizer>>> _failedTouchStartDeferringGestures;
#if ENABLE(IMAGE_ANALYSIS)
    RetainPtr<WKDeferringGestureRecognizer> _imageAnalysisDeferringGestureRecognizer;
#endif
    const std::unique_ptr<WebKit::GestureRecognizerConsistencyEnforcer> _gestureRecognizerConsistencyEnforcer;
    RetainPtr<WKTouchEventsGestureRecognizer> _touchEventGestureRecognizer;

    BOOL _touchEventsCanPreventNativeGestures;
    BOOL _preventsPanningInXAxis;
    BOOL _preventsPanningInYAxis;

    RetainPtr<WKSyntheticTapGestureRecognizer> _singleTapGestureRecognizer;
    RetainPtr<WKHighlightLongPressGestureRecognizer> _highlightLongPressGestureRecognizer;
    RetainPtr<UILongPressGestureRecognizer> _longPressGestureRecognizer;
    RetainPtr<WKSyntheticTapGestureRecognizer> _doubleTapGestureRecognizer;
#if ENABLE(MODEL_PROCESS)
    RetainPtr<UIPanGestureRecognizer> _modelInteractionPanGestureRecognizer;
#endif
    RetainPtr<UITapGestureRecognizer> _nonBlockingDoubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _doubleTapGestureRecognizerForDoubleClick;
    RetainPtr<UITapGestureRecognizer> _twoFingerDoubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _twoFingerSingleTapGestureRecognizer;
    RetainPtr<WKScrollViewTrackingTapGestureRecognizer> _keyboardDismissalGestureRecognizer;
    RetainPtr<WKInspectorNodeSearchGestureRecognizer> _inspectorNodeSearchGestureRecognizer;

    RetainPtr<WKTouchActionGestureRecognizer> _touchActionGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionLeftSwipeGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionRightSwipeGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionUpSwipeGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionDownSwipeGestureRecognizer;

#if ENABLE(GAMEPAD)
    RetainPtr<UITapGestureRecognizer> _gamepadInteractionGestureRecognizer;
#endif

#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
    RetainPtr<_UILookupGestureRecognizer> _lookupGestureRecognizer;
#endif

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    RetainPtr<WKMouseInteraction> _mouseInteraction;
    WebCore::MouseEventPolicy _mouseEventPolicy;
#endif

#if HAVE(PENCILKIT_TEXT_INPUT)
    RetainPtr<UIIndirectScribbleInteraction> _scribbleInteraction;
#endif

#if HAVE(UI_POINTER_INTERACTION)
    RetainPtr<UIPointerInteraction> _pointerInteraction;
    RetainPtr<UIPointerRegion> _lastPointerRegion;
    BOOL _pointerInteractionRegionNeedsUpdate;
#endif

#if HAVE(UITOOLTIPINTERACTION)
    RetainPtr<UIToolTipInteraction> _toolTip;
#endif

    RetainPtr<WKTextInteractionWrapper> _textInteractionWrapper;
    OptionSet<WebKit::SuppressSelectionAssistantReason> _suppressSelectionAssistantReasons;

    RetainPtr<UITextInputTraits> _legacyTextInputTraits;
    RetainPtr<WKExtendedTextInputTraits> _extendedTextInputTraits;
    RetainPtr<WKFormAccessoryView> _formAccessoryView;
    RetainPtr<WKTapHighlightView> _tapHighlightView;
    RetainPtr<UIView> _interactionViewsContainerView;
    RetainPtr<WKTargetedPreviewContainer> _contextMenuHintContainerView;
    WeakObjCPtr<UIScrollView> _scrollViewForTargetedPreview;
    CGPoint _scrollViewForTargetedPreviewInitialOffset;
    RetainPtr<WKTargetedPreviewContainer> _dragPreviewContainerView;
    RetainPtr<WKTargetedPreviewContainer> _dropPreviewContainerView;
    RetainPtr<NSString> _markedText;
    RetainPtr<WKActionSheetAssistant> _actionSheetAssistant;
#if ENABLE(AIRPLAY_PICKER)
    RetainPtr<WKAirPlayRoutePicker> _airPlayRoutePicker;
#endif
    RetainPtr<WKFormInputSession> _formInputSession;
    RetainPtr<WKFileUploadPanel> _fileUploadPanel;
    std::optional<WebKit::FrameInfoData> _frameInfoForFileUploadPanel;
#if HAVE(SHARE_SHEET_UI)
    RetainPtr<WKShareSheet> _shareSheet;
#endif
#if HAVE(CONTACTSUI)
    RetainPtr<WKContactPicker> _contactPicker;
#endif
#if HAVE(DIGITAL_CREDENTIALS_UI)
    RetainPtr<WKDigitalCredentialsPicker> _digitalCredentialsPicker;
#endif
    RetainPtr<UIGestureRecognizer> _previewGestureRecognizer;
    RetainPtr<UIGestureRecognizer> _previewSecondaryGestureRecognizer;
    Vector<bool> _focusStateStack;
#if HAVE(LINK_PREVIEW)
#if USE(UICONTEXTMENU)
    RetainPtr<WKContextMenuElementInfo> _contextMenuElementInfo;
    BOOL _showLinkPreviews;
    RetainPtr<UIViewController> _contextMenuLegacyPreviewController;
    RetainPtr<UIMenu> _contextMenuLegacyMenu;
    BOOL _contextMenuHasRequestedLegacyData;
    BOOL _contextMenuActionProviderDelegateNeedsOverride;
    BOOL _contextMenuIsUsingAlternateURLForImage;
    BOOL _useContextMenuInteractionDismissalPreview;
#endif
    RetainPtr<UIPreviewItemController> _previewItemController;
#endif

    __weak UIGestureRecognizer *_cachedTextInteractionLoupeGestureRecognizer;
    __weak UIGestureRecognizer *_cachedTextInteractionTapGestureRecognizer;

    RefPtr<WebCore::TextIndicator> _textIndicator;
    RetainPtr<WebTextIndicatorLayer> _textIndicatorLayer;

#if USE(UICONTEXTMENU)
    BOOL _isDisplayingContextMenuWithAnimation;
    RetainPtr<UITargetedPreview> _contextMenuInteractionTargetedPreview;
#endif

#if ENABLE(WRITING_TOOLS)
    BOOL _isPresentingWritingTools;

    RetainPtr<WKTextAnimationManager> _textAnimationManager;
#endif

    enum class SelectionInteractionType : uint8_t {
        None,
        Touch,
        ExtentPoint,
        ExtentPointAndBoundary,
    };
    SelectionInteractionType _selectionInteractionType;

    struct LastSelectionTouch {
        CGPoint point;
        BOOL baseIsStart;
        WKBESelectionFlags flags;
    };
    LastSelectionTouch _lastSelectionTouch;

    struct LastSelectionExtentPoint {
        CGPoint point;
        WebKit::RespectSelectionAnchor respectSelectionAnchor;
    };
    LastSelectionExtentPoint _lastSelectionExtentPoint;

    struct LastSelectionExtentPointAndBoundary {
        CGPoint point;
        WebCore::TextGranularity granularity;
        WebKit::TextInteractionSource interactionSource;
    };
    LastSelectionExtentPointAndBoundary _lastSelectionExtentPointAndBoundary;

    RefPtr<WebKit::SmartMagnificationController> _smartMagnificationController;

    WeakObjCPtr<id <UITextInputDelegate>> _inputDelegate;

    Markable<WebKit::TapIdentifier> _latestTapID;
    struct TapHighlightInformation {
        BOOL nodeHasBuiltInClickHandling { false };
        WebCore::Color color;
        Vector<WebCore::FloatQuad> quads;
        WebCore::IntSize topLeftRadius;
        WebCore::IntSize topRightRadius;
        WebCore::IntSize bottomLeftRadius;
        WebCore::IntSize bottomRightRadius;
    };
    TapHighlightInformation _tapHighlightInformation;

    WebKit::WebAutocorrectionContext _lastAutocorrectionContext;
    WebKit::WKAutoCorrectionData _autocorrectionData;
    WebKit::InteractionInformationAtPosition _positionInformation;
    RefPtr<WebCore::TextIndicator> _positionInformationLinkIndicator;
    WebKit::FocusedElementInformation _focusedElementInformation;
    std::optional<WebKit::FocusedElementInformationIdentifier> _pendingFocusedElementIdentifier;
    RetainPtr<NSObject<WKFormPeripheral>> _inputPeripheral;

    Vector<WebKit::KeyEventAndCompletionBlock, 1> _keyWebEventHandlers;

    CGPoint _lastInteractionLocation;
    std::optional<WebKit::TransactionID> _layerTreeTransactionIdAtLastInteractionStart;

    __weak UIView *_cachedSelectionContainerView;
    __weak UIView *_lastSiblingBeforeSelectionHighlight;
    WebKit::WKSelectionDrawingInfo _lastSelectionDrawingInfo;
    RetainPtr<WKTextRange> _cachedSelectedTextRange;

    std::optional<WebKit::InteractionInformationRequest> _lastOutstandingPositionInformationRequest;

    uint64_t _positionInformationCallbackDepth;
    Vector<std::optional<InteractionInformationRequestAndCallback>> _pendingPositionInformationHandlers;
    
    WebKit::InputViewUpdateDeferralSources _inputViewUpdateDeferralSources;

    RetainPtr<WKKeyboardScrollViewAnimator> _keyboardScrollingAnimator;

    Vector<BlockPtr<void()>> _actionsToPerformAfterEditorStateUpdate;

    RetainPtr<UIView <WKFormControl>> _dataListTextSuggestionsInputView;
    RetainPtr<NSArray<WKBETextSuggestion *>> _dataListTextSuggestions;
    WeakObjCPtr<WKDataListSuggestionsControl> _dataListSuggestionsControl;

    RefPtr<WebKit::RevealFocusedElementDeferrer> _revealFocusedElementDeferrer;

    BOOL _isEditable;
    BOOL _hasValidPositionInformation;
    BOOL _isTapHighlightIDValid;
    BOOL _isTapHighlightFading;
    BOOL _potentialTapInProgress;
    BOOL _isDoubleTapPending;
    BOOL _longPressCanClick;
    BOOL _hasTapHighlightForPotentialTap;
    BOOL _selectionNeedsUpdate;
    BOOL _shouldRestoreSelection;
    BOOL _usingGestureForSelection;
    BOOL _usingMouseDragForSelection;
    BOOL _inspectorNodeSearchEnabled;
    BOOL _isChangingFocusUsingAccessoryTab;
    BOOL _didAccessoryTabInitiateFocus;
    BOOL _isExpectingFastSingleTapCommit;
    BOOL _showDebugTapHighlightsForFastClicking;
    BOOL _textInteractionDidChangeFocusedElement;
    BOOL _treatAsContentEditableUntilNextEditorStateUpdate;
    BOOL _isWaitingOnPositionInformation;
    BOOL _autocorrectionContextNeedsUpdate;

    WebCore::PointerID _commitPotentialTapPointerId;

    BOOL _keyboardDidRequestDismissal;
    BOOL _isKeyboardScrollingAnimationRunning;
    BOOL _keyboardDismissedInCurrentPresentationUpdate;
    BOOL _editingEndedByUser;

    BOOL _candidateViewNeedsUpdate;
    BOOL _seenHardwareKeyDownInNonEditableElement;

    BOOL _becomingFirstResponder;
    BOOL _resigningFirstResponder;
    BOOL _needsDeferredEndScrollingSelectionUpdate;
    BOOL _isChangingFocus;
    BOOL _isFocusingElementWithKeyboard;
    BOOL _isBlurringFocusedElement;
    BOOL _isRelinquishingFirstResponderToFocusedElement;
    BOOL _unsuppressSoftwareKeyboardAfterNextAutocorrectionContextUpdate;
    BOOL _isUnsuppressingSoftwareKeyboardUsingLastAutocorrectionContext;
    BOOL _waitingForKeyboardAppearanceAnimationToStart;
    BOOL _isHidingKeyboard;
    BOOL _isInterpretingKeyEvent;
    BOOL _isPresentingEditMenu;
    BOOL _isHandlingActiveKeyEvent;
    BOOL _isHandlingActivePressesEvent;
    BOOL _isDeferringKeyEventsToInputMethod;
    BOOL _isUpdatingAccessoryView;
    BOOL _doubleTapGesturesAreDisabledTemporarilyForFastTap;

    BOOL _focusRequiresStrongPasswordAssistance;
    BOOL _waitingForEditDragSnapshot;
    std::optional<BOOL> _cachedRequiresLegacyTextInputTraits;
    NSInteger _dropAnimationCount;

    BOOL _waitingForEditorStateAfterScrollingSelectionContainer;
    std::optional<WebCore::IntPoint> _lastSelectionChildScrollViewContentOffset;
    std::optional<CGPoint> _lastSelectionContainerViewOrigin;

    BOOL _hasSetUpInteractions;
    std::optional<BOOL> _cachedHasCustomTintColor;
    NSUInteger _ignoreSelectionCommandFadeCount;
    NSUInteger _activeTextInteractionCount;
    NSInteger _suppressNonEditableSingleTapTextInteractionCount;
    CompletionHandler<void(WebCore::DOMPasteAccessResponse)> _domPasteRequestHandler;
    std::optional<WebCore::DOMPasteAccessCategory> _domPasteRequestCategory;
    CompletionHandler<void(WebKit::RequestAutocorrectionContextResult)> _pendingAutocorrectionContextHandler;
    CompletionHandler<void()> _pendingRunModalJavaScriptDialogCallback;

    RetainPtr<NSDictionary> _additionalContextForStrongPasswordAssistance;

    std::optional<char32_t> _lastInsertedCharacterToOverrideCharacterBeforeSelection;
    unsigned _selectionChangeNestingLevel;

#if ENABLE(DRAG_SUPPORT)
    WebKit::DragDropInteractionState _dragDropInteractionState;
    RetainPtr<UIDragInteraction> _dragInteraction;
    RetainPtr<UIDropInteraction> _dropInteraction;
    BOOL _isAnimatingDragCancel;
    BOOL _shouldRestoreEditMenuAfterDrop;
    RetainPtr<UIView> _visibleContentViewSnapshot;
    RetainPtr<UIView> _unselectedContentSnapshot;
    RetainPtr<_UITextDragCaretView> _editDropCaretView;
    BlockPtr<void()> _actionToPerformAfterReceivingEditDragSnapshot;
#endif

#if ENABLE(MODEL_PROCESS)
    std::optional<WebKit::StageModeSession> _stageModeSession;
#endif

#if HAVE(UI_TEXT_CURSOR_DROP_POSITION_ANIMATOR)
    RetainPtr<UIView<UITextCursorView>> _editDropTextCursorView;
    RetainPtr<UITextCursorDropPositionAnimator> _editDropCaretAnimator;
#endif
    RetainPtr<NSMutableDictionary<NSUUID *, NSUUID *>> _sourceAnimationIDtoDestinationAnimationID;

#if HAVE(PEPPER_UI_CORE)
    RetainPtr<WKFocusedFormControlView> _focusedFormControlView;
#if HAVE(QUICKBOARD_CONTROLLER)
    RetainPtr<PUICQuickboardController> _presentedQuickboardController;
#endif // HAVE(QUICKBOARD_CONTROLLER)
    RetainPtr<PUICQuickboardViewController> _presentedFullScreenInputViewController;
    RetainPtr<UINavigationController> _inputNavigationViewControllerForFullScreenInputs;

    BOOL _shouldRestoreFirstResponderStatusAfterLosingFocus;
    BlockPtr<void()> _activeFocusedStateRetainBlock;
#endif // HAVE(PEPPER_UI_CORE)

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    std::unique_ptr<WebKit::TextCheckingController> _textCheckingController;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    RetainPtr<WKImageAnalysisGestureRecognizer> _imageAnalysisGestureRecognizer;
    std::optional<WebKit::ImageAnalysisRequestIdentifier> _pendingImageAnalysisRequestIdentifier;
    std::optional<WebCore::ElementContext> _elementPendingImageAnalysis;
    Vector<BlockPtr<void(WebKit::ProceedWithTextSelectionInImage)>> _actionsToPerformAfterPendingImageAnalysis;
    BOOL _isProceedingWithTextSelectionInImage;
    RetainPtr<CocoaImageAnalyzer> _imageAnalyzer;
#if USE(QUICK_LOOK)
    RetainPtr<QLPreviewController> _visualSearchPreviewController;
    RetainPtr<UIImage> _visualSearchPreviewImage;
    RetainPtr<NSURL> _visualSearchPreviewImageURL;
    RetainPtr<NSString> _visualSearchPreviewTitle;
    CGRect _visualSearchPreviewImageBounds;
    std::optional<CGRect> _cachedVisualSearchPreviewImageBoundsInWindowCoordinates;
#endif // USE(QUICK_LOOK)
    WebKit::DynamicImageAnalysisContextMenuState _dynamicImageAnalysisContextMenuState;
    std::optional<WebKit::ImageAnalysisContextMenuActionData> _imageAnalysisContextMenuActionData;
#endif // ENABLE(IMAGE_ANALYSIS)
    uint32_t _fullscreenVideoImageAnalysisRequestIdentifier;
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    RetainPtr<VKCImageAnalysisInteraction> _imageAnalysisInteraction;
    RetainPtr<NSMutableSet<UIButton *>> _imageAnalysisActionButtons;
    WebCore::FloatRect _imageAnalysisInteractionBounds;
    std::optional<WebKit::RemoveBackgroundData> _removeBackgroundData;
#endif
#if USE(BROWSERENGINEKIT)
    __weak id<BETextInputDelegate> _asyncInputDelegate;
#endif
}

@end

@interface WKContentView (WKInteraction) <UIGestureRecognizerDelegate, UITextInput, WKFormAccessoryViewDelegate, WKActionSheetAssistantDelegate, WKFileUploadPanelDelegate, WKKeyboardScrollViewAnimatorDelegate, WKDeferringGestureRecognizerDelegate, WKTextSelectionRectDelegate
#if HAVE(CONTACTSUI)
    , WKContactPickerDelegate
#endif
#if HAVE(SHARE_SHEET_UI)
    , WKShareSheetDelegate
#endif
#if ENABLE(DRAG_SUPPORT)
    , UIDropInteractionDelegate
#endif
    , WKTouchActionGestureRecognizerDelegate
#if HAVE(UIFINDINTERACTION)
    , UITextSearching
#endif
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    , WKMouseInteractionDelegate
#endif
#if USE(BROWSERENGINEKIT)
    , BEDragInteractionDelegate
    , BETextInteractionDelegate
#elif ENABLE(DRAG_SUPPORT)
    , UIDragInteractionDelegate
#endif
#if ENABLE(WRITING_TOOLS)
    , WTWritingToolsDelegate
    , WKTextAnimationSourceDelegate
#endif
#if HAVE(UITOOLTIPINTERACTION)
    , UIToolTipInteractionDelegate
#endif
>

@property (nonatomic, readonly) CGPoint lastInteractionLocation;
@property (nonatomic, readonly) BOOL isEditable;
@property (nonatomic, readonly) BOOL shouldHideSelectionInFixedPositionWhenScrolling;
@property (nonatomic, readonly) BOOL shouldIgnoreKeyboardWillHideNotification;
@property (nonatomic, readonly) const WebKit::InteractionInformationAtPosition& positionInformation;
@property (nonatomic, readonly) const WebKit::WKAutoCorrectionData& autocorrectionData;
@property (nonatomic, readonly) const WebKit::FocusedElementInformation& focusedElementInformation;
@property (nonatomic, readonly) WKFormAccessoryView *formAccessoryView;
@property (nonatomic, readonly) UITextInputAssistantItem *inputAssistantItemForWebView;
@property (nonatomic, readonly) UIView *inputViewForWebView;
@property (nonatomic, readonly) UIView *inputAccessoryViewForWebView;
@property (nonatomic, readonly) UITextInputTraits *textInputTraitsForWebView;
@property (nonatomic, readonly) BOOL preventsPanningInXAxis;
@property (nonatomic, readonly) BOOL preventsPanningInYAxis;
@property (nonatomic, readonly) WKTouchEventsGestureRecognizer *touchEventGestureRecognizer;
@property (nonatomic, readonly) NSArray<WKDeferringGestureRecognizer *> *deferringGestures;
@property (nonatomic, readonly) WebKit::GestureRecognizerConsistencyEnforcer& gestureRecognizerConsistencyEnforcer;
@property (nonatomic, readonly) CGRect tapHighlightViewRect;
@property (nonatomic, readonly) UIGestureRecognizer *imageAnalysisGestureRecognizer;
@property (nonatomic, readonly, getter=isKeyboardScrollingAnimationRunning) BOOL keyboardScrollingAnimationRunning;
@property (nonatomic, readonly) UIView *unscaledView;
@property (nonatomic, readonly) BOOL isPresentingEditMenu;
@property (nonatomic, readonly) CGSize sizeForLegacyFormControlPickerViews;
@property (nonatomic, readonly, getter=isPotentialTapInProgress) BOOL potentialTapInProgress;
@property (nonatomic, strong) UIView <WKFormControl> *dataListTextSuggestionsInputView;
@property (nonatomic, strong) NSArray<WKBETextSuggestion *> *dataListTextSuggestions;

- (void)setUpInteraction;
- (void)cleanUpInteraction;
- (void)cleanUpInteractionPreviewContainers;

- (void)dismissPickersIfNeededWithReason:(WebKit::PickerDismissalReason)reason;

- (void)scrollViewWillStartPanOrPinchGesture;

- (void)buildMenuForWebViewWithBuilder:(id <UIMenuBuilder>)builder;

- (BOOL)canBecomeFirstResponderForWebView;
- (BOOL)becomeFirstResponderForWebView;
- (BOOL)resignFirstResponderForWebView;
- (BOOL)canPerformActionForWebView:(SEL)action withSender:(id)sender;
- (id)targetForActionForWebView:(SEL)action withSender:(id)sender;

- (void)_selectPositionAtPoint:(CGPoint)point stayingWithinFocusedElement:(BOOL)stayingWithinFocusedElement completionHandler:(void (^)(void))completionHandler;

- (void)_startSuppressingSelectionAssistantForReason:(WebKit::SuppressSelectionAssistantReason)reason;
- (void)_stopSuppressingSelectionAssistantForReason:(WebKit::SuppressSelectionAssistantReason)reason;

- (BOOL)_hasFocusedElement;
- (void)_zoomToRevealFocusedElement;

- (void)_keyboardWillShow;
- (void)_keyboardDidShow;

- (void)cancelPointersForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer;
- (std::optional<unsigned>)activeTouchIdentifierForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer;

- (void)updateSelection;

#define DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW(_action) \
    - (void)_action ## ForWebView:(id)sender;
FOR_EACH_WKCONTENTVIEW_ACTION(DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW)
FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW)
#undef DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW

- (void)_setFontForWebView:(UIFont *)fontFamily sender:(id)sender;
- (void)_setFontSizeForWebView:(CGFloat)fontSize sender:(id)sender;
- (void)_setTextColorForWebView:(UIColor *)color sender:(id)sender;

#if ENABLE(TOUCH_EVENTS)
- (void)_touchEvent:(const WebKit::WebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsDefault;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
- (void)_doneDeferringTouchStart:(BOOL)preventNativeGestures;
- (void)_doneDeferringTouchMove:(BOOL)preventNativeGestures;
- (void)_doneDeferringTouchEnd:(BOOL)preventNativeGestures;
#endif
- (void)_commitPotentialTapFailed;
- (void)_didNotHandleTapAsClick:(const WebCore::IntPoint&)point;
- (void)_didHandleTapAsHover;
- (void)_didCompleteSyntheticClick;
- (void)_provideSuggestionsToInputDelegate:(NSArray<WKBETextSuggestion *> *)suggestions;

- (void)_didGetTapHighlightForRequest:(WebKit::TapIdentifier)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling;

- (BOOL)_mayDisableDoubleTapGesturesDuringSingleTap;
- (void)_disableDoubleTapGesturesDuringTapIfNecessary:(WebKit::TapIdentifier)requestID;
- (void)_handleSmartMagnificationInformationForPotentialTap:(WebKit::TapIdentifier)requestID renderRect:(const WebCore::FloatRect&)renderRect fitEntireRect:(BOOL)fitEntireRect viewportMinimumScale:(double)viewportMinimumScale viewportMaximumScale:(double)viewportMaximumScale nodeIsRootLevel:(BOOL)nodeIsRootLevel nodeIsPluginElement:(BOOL)nodeIsPluginElement;
- (void)_elementDidFocus:(const WebKit::FocusedElementInformation&)information userIsInteracting:(BOOL)userIsInteracting blurPreviousNode:(BOOL)blurPreviousNode activityStateChanges:(OptionSet<WebCore::ActivityState>)activityStateChanges userObject:(NSObject <NSSecureCoding> *)userObject;
- (void)_updateInputContextAfterBlurringAndRefocusingElement;
- (void)_didProgrammaticallyClearFocusedElement:(WebCore::ElementContext&&)context;
- (void)_updateFocusedElementInformation:(const WebKit::FocusedElementInformation&)information;
- (void)_elementDidBlur;
- (void)_didUpdateInputMode:(WebCore::InputMode)mode;
- (void)_didUpdateEditorState;
- (void)_reconcileEnclosingScrollViewContentOffset:(WebKit::EditorState&)state;
- (void)_hardwareKeyboardAvailabilityChanged;
- (void)_selectionChanged;
- (void)_updateChangedSelection;
- (BOOL)_interpretKeyEvent:(::WebEvent *)event withContext:(WebKit::KeyEventInterpretationContext&&)context;
- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
- (BOOL)_currentPositionInformationIsValidForRequest:(const WebKit::InteractionInformationRequest&)request;
- (void)_attemptSyntheticClickAtLocation:(CGPoint)location modifierFlags:(UIKeyModifierFlags)modifierFlags;
- (void)_willStartScrollingOrZooming;
- (void)_didScroll;
- (void)_didEndScrollingOrZooming;
- (void)_scrollingNodeScrollingWillBegin:(std::optional<WebCore::ScrollingNodeID>)nodeID;
- (void)_scrollingNodeScrollingDidEnd:(std::optional<WebCore::ScrollingNodeID>)nodeID;
- (void)_showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect routeSharingPolicy:(WebCore::RouteSharingPolicy)policy routingContextUID:(NSString *)contextUID;
- (void)_showRunOpenPanel:(API::OpenPanelParameters*)parameters frameInfo:(const WebKit::FrameInfoData&)frameInfo resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener;
- (void)_showShareSheet:(const WebCore::ShareDataWithParsedURL&)shareData inRect:(std::optional<WebCore::FloatRect>)rect completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler;
- (void)_showContactPicker:(const WebCore::ContactsRequestData&)requestData completionHandler:(WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&)completionHandler;

#if HAVE(DIGITAL_CREDENTIALS_UI)
- (void)_showDigitalCredentialsPicker:(const WebCore::DigitalCredentialsRequestData&)requestData completionHandler:(WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&)completionHandler;
- (void)_dismissDigitalCredentialsPicker:(CompletionHandler<void(bool)>&&)completionHandler;
#endif

- (NSArray<NSString *> *)filePickerAcceptedTypeIdentifiers;
- (void)dismissFilePicker;
- (void)_didHandleKeyEvent:(::WebEvent *)event eventWasHandled:(BOOL)eventWasHandled;
- (Vector<WebKit::OptionItem>&) focusedSelectElementOptions;
- (void)_enableInspectorNodeSearch;
- (void)_disableInspectorNodeSearch;
- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler;
- (void)_setDoubleTapGesturesEnabled:(BOOL)enabled;
#if ENABLE(DATA_DETECTION)
- (NSArray *)_dataDetectionResults;
#endif
- (void)accessibilityRetrieveSpeakSelectionContent;
- (void)_accessibilityRetrieveRectsEnclosingSelectionOffset:(NSInteger)offset withGranularity:(UITextGranularity)granularity;
- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text completionHandler:(void (^)(const Vector<WebCore::SelectionGeometry>& rects))completionHandler;
- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text;
- (void)_accessibilityStoreSelection;
- (void)_accessibilityClearSelection;
- (WKFormInputSession *)_formInputSession;
- (void)_didChangeWebViewEditability;
- (NSDictionary *)dataDetectionContextForPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation;
- (void)_showDataDetectorsUIForPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation;

- (void)willFinishIgnoringCalloutBarFadeAfterPerformingAction;

- (BOOL)hasHiddenContentEditable;
- (void)generateSyntheticEditingCommand:(WebKit::SyntheticEditingCommandType)command;

- (NSString *)inputLabelText;

- (void)startRelinquishingFirstResponderToFocusedElement;
- (void)stopRelinquishingFirstResponderToFocusedElement;

- (void)accessoryDone;
- (void)accessoryOpen;
- (void)accessoryTab:(BOOL)isNext;

- (void)updateFocusedElementValueAsColor:(UIColor *)value;
- (void)updateFocusedElementValue:(NSString *)value;
- (void)updateFocusedElementSelectedIndex:(uint32_t)index allowsMultipleSelection:(bool)allowsMultipleSelection;
- (void)updateFocusedElementFocusedWithDataListDropdown:(BOOL)value;

- (void)_requestDOMPasteAccessForCategory:(WebCore::DOMPasteAccessCategory)pasteAccessCategory requiresInteraction:(WebCore::DOMPasteRequiresInteraction)requiresInteraction elementRect:(const WebCore::IntRect&)elementRect originIdentifier:(const String&)originIdentifier completionHandler:(CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&)completionHandler;

- (void)doAfterPositionInformationUpdate:(void (^)(WebKit::InteractionInformationAtPosition))action forRequest:(WebKit::InteractionInformationRequest)request;
- (BOOL)ensurePositionInformationIsUpToDate:(WebKit::InteractionInformationRequest)request;

- (void)doAfterEditorStateUpdateAfterFocusingElement:(dispatch_block_t)block;
- (void)runModalJavaScriptDialog:(CompletionHandler<void()>&&)callback;

#if HAVE(UITOOLTIPINTERACTION)
- (void)_toolTipChanged:(NSString *)newToolTip;
- (UIToolTipConfiguration *)toolTipInteraction:(UIToolTipInteraction *)interaction configurationAtPoint:(CGPoint)point;
#endif

#if ENABLE(DRAG_SUPPORT)
- (void)_didChangeDragInteractionPolicy;
- (void)_didPerformDragOperation:(BOOL)handled;
- (void)_didHandleDragStartRequest:(BOOL)started;
- (void)_didHandleAdditionalDragItemsRequest:(BOOL)added;
- (void)_startDrag:(RetainPtr<CGImageRef>)image item:(const WebCore::DragItem&)item nodeID:(std::optional<WebCore::NodeIdentifier>)nodeID;
- (void)_willReceiveEditDragSnapshot;
- (void)_didReceiveEditDragSnapshot:(RefPtr<WebCore::TextIndicator>&&)textIndicator;
- (void)_didChangeDragCaretRect:(CGRect)previousRect currentRect:(CGRect)rect;
#endif

#if ENABLE(MODEL_PROCESS)
- (void)modelInteractionPanGestureDidBeginAtPoint:(CGPoint)inputPoint;
- (void)modelInteractionPanGestureDidUpdateWithPoint:(CGPoint)inputPoint;
- (void)modelInteractionPanGestureDidEnd;
- (void)didReceiveInteractiveModelElement:(std::optional<WebCore::NodeIdentifier>)nodeID;
#endif

- (void)reloadContextViewForPresentedListViewController;

- (void)updateTextSuggestionsForInputDelegate;

- (void)_updateRuntimeProtocolConformanceIfNeeded;

- (void)_requestTextInputContextsInRect:(CGRect)rect completionHandler:(void (^)(NSArray<_WKTextInputContext *> *))completionHandler;
- (void)_focusTextInputContext:(_WKTextInputContext *)context placeCaretAt:(CGPoint)point completionHandler:(void (^)(UIResponder<UITextInput> *))completionHandler;
- (void)_willBeginTextInteractionInTextInputContext:(_WKTextInputContext *)context;
- (void)_didFinishTextInteractionInTextInputContext:(_WKTextInputContext *)context;

- (void)_handleAutocorrectionContext:(const WebKit::WebAutocorrectionContext&)context;

- (void)applyAutocorrection:(NSString *)correction toString:(NSString *)input isCandidate:(BOOL)isCandidate withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForCorrection))completionHandler;

- (void)_didStartProvisionalLoadForMainFrame;
- (void)_didCommitLoadForMainFrame;

- (void)setUpTextIndicator:(Ref<WebCore::TextIndicator>)textIndicator;
- (void)updateTextIndicator:(Ref<WebCore::TextIndicator>)textIndicator;
- (void)setTextIndicatorAnimationProgress:(float)NSAnimationProgress;
- (void)clearTextIndicator:(WebCore::TextIndicatorDismissalAnimation)animation;
- (CALayer *)textIndicatorInstallationLayer;

#if ENABLE(WRITING_TOOLS)
- (void)addTextAnimationForAnimationID:(NSUUID *)uuid withData:(const WebCore::TextAnimationData&)data;
- (void)removeTextAnimationForAnimationID:(NSUUID *)uuid;
#endif

@property (nonatomic, readonly) BOOL _shouldUseContextMenus;
@property (nonatomic, readonly) BOOL _shouldUseContextMenusForFormControls;
@property (nonatomic, readonly) BOOL _shouldAvoidResizingWhenInputViewBoundsChange;
@property (nonatomic, readonly) BOOL _shouldAvoidScrollingWhenFocusedContentIsVisible;
@property (nonatomic, readonly) BOOL _shouldUseLegacySelectPopoverDismissalBehavior;
@property (nonatomic, readonly) BOOL _shouldAvoidSecurityHeuristicScoreUpdates;
@property (nonatomic, readonly) BOOL _canStartNavigationSwipeAtLastInteractionLocation;

- (void)_didChangeLinkPreviewAvailability;
- (void)setContinuousSpellCheckingEnabled:(BOOL)enabled;
- (void)setGrammarCheckingEnabled:(BOOL)enabled;

- (void)updateSoftwareKeyboardSuppressionStateFromWebView;

#if USE(UICONTEXTMENU)
- (UIView *)containerForContextMenuHintPreviews;
- (UIView *)textEffectsWindow;
- (UITargetedPreview *)_createTargetedPreviewFromTextIndicator:(RefPtr<WebCore::TextIndicator>&&)textIndicator previewContainer:(UIView *)previewContainer;
- (UITargetedPreview *)_createTargetedContextMenuHintPreviewForFocusedElement:(WebKit::TargetedPreviewPositioning)positioning;
- (UITargetedPreview *)_createTargetedContextMenuHintPreviewIfPossible;
- (void)_removeContextMenuHintContainerIfPossible;
- (void)_targetedPreviewContainerDidRemoveLastSubview:(WKTargetedPreviewContainer *)containerView;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
- (void)_writePromisedAttachmentToPasteboard:(WebCore::PromisedAttachmentInfo&&)info;
#endif

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
- (void)_setMouseEventPolicy:(WebCore::MouseEventPolicy)policy;
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
- (void)_showMediaControlsContextMenu:(WebCore::FloatRect&&)targetFrame items:(Vector<WebCore::MediaControlsContextMenuItem>&&)items frameInfo:(const WebKit::FrameInfoData&)frameInfo identifier:(WebCore::HTMLMediaElementIdentifier)identifier completionHandler:(CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&)completionHandler;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(VIDEO) && USE(UICONTEXTMENU)
- (void)showCaptionDisplaySettingsMenu:(WebCore::HTMLMediaElementIdentifier)identifier withOptions:(const WebCore::ResolvedCaptionDisplaySettingsOptions&)options completionHandler:(CompletionHandler<void(Expected<void, WebCore::ExceptionData>)>&&)completionHandler;
#endif

- (BOOL)_formControlRefreshEnabled;

- (WebCore::DataOwnerType)_dataOwnerForPasteboard:(WebKit::PasteboardAccessIntent)intent;

#if ENABLE(IMAGE_ANALYSIS)
- (void)_endImageAnalysisGestureDeferral:(WebKit::ShouldPreventGestures)shouldPreventGestures;
- (void)requestTextRecognition:(NSURL *)imageURL imageData:(WebCore::ShareableBitmap::Handle&&)imageData sourceLanguageIdentifier:(NSString *)sourceLanguageIdentifier targetLanguageIdentifier:(NSString *)targetLanguageIdentifier completionHandler:(CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&)completion;
#endif

#if HAVE(UIFINDINTERACTION)
@property (nonatomic, readonly) BOOL supportsTextReplacementForWebView;
- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition inDocument:(UITextSearchDocumentIdentifier)document;
- (void)didBeginTextSearchOperation;
- (void)didEndTextSearchOperation;

- (void)requestRectForFoundTextRange:(UITextRange *)range completionHandler:(void (^)(CGRect))completionHandler;
#endif

- (void)beginTextRecognitionForFullscreenVideo:(WebCore::ShareableBitmap::Handle&&)imageHandle playerViewController:(AVPlayerViewController *)playerViewController;
- (void)cancelTextRecognitionForFullscreenVideo:(AVPlayerViewController *)controller;
@property (nonatomic, readonly) BOOL isTextRecognitionInFullscreenVideoEnabled;

- (void)beginTextRecognitionForVideoInElementFullscreen:(WebCore::ShareableBitmap::Handle&&)bitmapHandle bounds:(WebCore::FloatRect)bounds;
- (void)cancelTextRecognitionForVideoInElementFullscreen;

- (BOOL)_tryToHandlePressesEvent:(UIPressesEvent *)event;
- (void)_closeCurrentTypingCommand;

- (BOOL)_shouldIgnoreTouchEvent:(UIEvent *)event;
- (void)_touchEventsRecognized;

- (BOOL)_hasEnclosingScrollView:(UIView *)firstView matchingCriteria:(Function<BOOL(UIScrollView *)>&&)matchFunction;

- (ScopeExit<Function<void()>>)makeTextSelectionViewsNonInteractiveForScope;

- (BOOL)_shouldHideSelectionDuringOverflowScroll:(UIScrollView *)scrollView;

- (void)_updateDoubleTapGestureRecognizerEnablement;

@property (nonatomic, readonly) BOOL shouldUseAsyncInteractions;
@property (nonatomic, readonly) NSArray<UIView *> *allViewsIntersectingSelectionRange;

#if HAVE(UI_CONVERSATION_CONTEXT)
@property (strong, nonatomic, setter=_setConversationContext:) UIConversationContext *_conversationContext;
#endif

#if ENABLE(MODEL_PROCESS)
- (void)_willInvalidateDraggedModelWithContainerView:(UIView *)containerView;
#endif

@end

@interface WKContentView (WKTesting)

- (void)selectWordBackwardForTesting;
- (void)_simulateElementAction:(_WKElementActionType)actionType atLocation:(CGPoint)location;
- (void)_simulateLongPressActionAtLocation:(CGPoint)location;
- (void)_simulateTextEntered:(NSString *)text;
- (void)selectFormAccessoryPickerRow:(NSInteger)rowIndex;
- (BOOL)selectFormAccessoryHasCheckedItemAtRow:(long)rowIndex;
- (void)setSelectedColorForColorPicker:(UIColor *)color;
- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute;
- (double)timePickerValueHour;
- (double)timePickerValueMinute;
- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem;
- (void)_doAfterReceivingEditDragSnapshotForTesting:(dispatch_block_t)action;
- (void)_dismissContactPickerWithContacts:(NSArray *)contacts;
- (void)_simulateSelectionStart;

#if ENABLE(MODEL_PROCESS)
- (void)_simulateModelInteractionPanGestureBeginAtPoint:(CGPoint)hitPoint;
- (void)_simulateModelInteractionPanGestureUpdateAtPoint:(CGPoint)hitPoint;
- (NSDictionary *)_stageModeInfoForTesting;
#endif

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
- (BOOL)_allowAnimationControls;
#endif

- (void)dismissFormAccessoryView;

- (void)_selectDataListOption:(NSInteger)optionIndex;
- (void)_setDataListSuggestionsControl:(WKDataListSuggestionsControl *)control;

@property (nonatomic, readonly) BOOL isShowingDataListSuggestions;

@property (nonatomic, readonly) NSString *textContentTypeForTesting;
@property (nonatomic, readonly) NSString *selectFormPopoverTitle;
@property (nonatomic, readonly) NSString *formInputLabel;
#if !PLATFORM(WATCHOS)
@property (nonatomic, readonly) WKDateTimeInputControl *dateTimeInputControl;
#endif
@property (nonatomic, readonly) WKFormSelectControl *selectControl;
#if ENABLE(DRAG_SUPPORT)
@property (nonatomic, readonly, getter=isAnimatingDragCancel) BOOL animatingDragCancel;
#endif

@property (nonatomic, readonly) UITapGestureRecognizer *singleTapGestureRecognizer;
@property (nonatomic, readonly) UIWKTextInteractionAssistant *textInteractionAssistant;

@end

#if HAVE(LINK_PREVIEW)
#if USE(UICONTEXTMENU)
@interface WKContentView (WKInteractionPreview) <UIContextMenuInteractionDelegate, UIPreviewItemDelegate>
@property (nonatomic, readonly) UIContextMenuInteraction *contextMenuInteraction;
#else
@interface WKContentView (WKInteractionPreview) <UIPreviewItemDelegate>
#endif
- (void)_registerPreview;
- (void)_unregisterPreview;
@end
#endif

#if ENABLE(POINTER_LOCK)
@interface WKContentView (PointerLock)
- (void)_beginPointerLockMouseTracking;
- (void)_endPointerLockMouseTracking;
@end
#endif

#endif // PLATFORM(IOS_FAMILY)
