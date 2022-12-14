/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#import "SyntheticEditingCommandType.h"
#import "TextCheckingController.h"
#import "TransactionID.h"
#import "UIKitSPI.h"
#import <WebKit/WKActionSheetAssistant.h>
#import <WebKit/WKAirPlayRoutePicker.h>
#import <WebKit/WKContactPicker.h>
#import <WebKit/WKDeferringGestureRecognizer.h>
#import <WebKit/WKFileUploadPanel.h>
#import <WebKit/WKFormPeripheral.h>
#import <WebKit/WKKeyboardScrollingAnimator.h>
#import <WebKit/WKShareSheet.h>
#import <WebKit/WKSyntheticTapGestureRecognizer.h>
#import <WebKit/WKTouchActionGestureRecognizer.h>
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
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/Forward.h>
#import <wtf/Function.h>
#import <wtf/HashSet.h>
#import <wtf/ObjectIdentifier.h>
#import <wtf/OptionSet.h>
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
struct PromisedAttachmentInfo;
struct ShareDataWithParsedURL;
struct TextRecognitionResult;
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
enum class MouseEventPolicy : uint8_t;
enum class RouteSharingPolicy : uint8_t;
enum class TextIndicatorDismissalAnimation : uint8_t;

#if ENABLE(DRAG_SUPPORT)
struct DragItem;
#endif
}

namespace WebKit {
class InputViewUpdateDeferrer;
class NativeWebTouchEvent;
class SmartMagnificationController;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
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
@class WKHoverGestureRecognizer;
@class WKImageAnalysisGestureRecognizer;
@class WKMouseGestureRecognizer;
@class WKInspectorNodeSearchGestureRecognizer;
@class WKTargetedPreviewContainer;
@class WKTextRange;
@class _WKTextInputContext;

#if !PLATFORM(WATCHOS)
@class WKDateTimeInputControl;
#endif

@class UIPointerInteraction;
@class UITargetedPreview;
@class _UILookupGestureRecognizer;
@class _UIHighlightView;

#if HAVE(PEPPER_UI_CORE)
@class PUICQuickboardViewController;
#if HAVE(QUICKBOARD_CONTROLLER)
@class PUICQuickboardController;
#endif
#endif

typedef void (^UIWKAutocorrectionCompletionHandler)(UIWKAutocorrectionRects *rectsForInput);
typedef void (^UIWKAutocorrectionContextHandler)(UIWKAutocorrectionContext *autocorrectionContext);
typedef void (^UIWKDictationContextHandler)(NSString *selectedText, NSString *beforeText, NSString *afterText);
typedef void (^UIWKSelectionCompletionHandler)(void);
typedef void (^UIWKSelectionWithDirectionCompletionHandler)(BOOL selectionEndIsMoving);

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
    M(_findSelected)
#else
#define FOR_EACH_FIND_WKCONTENTVIEW_ACTION(M)
#endif

#define FOR_EACH_WKCONTENTVIEW_ACTION(M) \
    FOR_EACH_INSERT_TEXT_FROM_CAMERA_WKCONTENTVIEW_ACTION(M) \
    FOR_EACH_FIND_WKCONTENTVIEW_ACTION(M) \
    M(_addShortcut) \
    M(_define) \
    M(_lookup) \
    M(_translate) \
    M(_promptForReplace) \
    M(_share) \
    M(_showTextStyleOptions) \
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
    M(makeTextWritingDirectionRightToLeft)

#define FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(M) \
    M(_alignCenter) \
    M(_alignJustified) \
    M(_alignLeft) \
    M(_alignRight) \
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
    EditableRootIsTransparentOrFullyClipped = 1 << 0,
    FocusedElementIsTooSmall = 1 << 1,
    InteractionIsHappening = 1 << 2,
    ShowingFullscreenVideo = 1 << 3,
};

struct WKSelectionDrawingInfo {
    enum class SelectionType { None, Plugin, Range };
    WKSelectionDrawingInfo();
    explicit WKSelectionDrawingInfo(const EditorState&);
    SelectionType type;
    WebCore::IntRect caretRect;
    WebCore::Color caretColor;
    Vector<WebCore::SelectionGeometry> selectionGeometries;
    WebCore::IntRect selectionClipRect;
};

WTF::TextStream& operator<<(WTF::TextStream&, const WKSelectionDrawingInfo&);

struct WKAutoCorrectionData {
    RetainPtr<UIFont> font;
    CGRect textFirstRect;
    CGRect textLastRect;
};

struct RemoveBackgroundData {
    WebCore::ElementContext element;
    RetainPtr<CGImageRef> image;
    String preferredMIMEType;
};

enum class ProceedWithTextSelectionInImage : bool {
    No,
    Yes
};

enum ImageAnalysisRequestIdentifierType { };
using ImageAnalysisRequestIdentifier = ObjectIdentifier<ImageAnalysisRequestIdentifierType>;

struct ImageAnalysisContextMenuActionData {
    bool hasSelectableText { false };
    bool hasVisualSearchResults { false };
    RetainPtr<CGImageRef> copySubjectResult;
    RetainPtr<UIMenu> machineReadableCodeMenu;
};

}

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
    std::unique_ptr<WebKit::GestureRecognizerConsistencyEnforcer> _gestureRecognizerConsistencyEnforcer;
    RetainPtr<UIWebTouchEventsGestureRecognizer> _touchEventGestureRecognizer;

    BOOL _touchEventsCanPreventNativeGestures;
    BOOL _preventsPanningInXAxis;
    BOOL _preventsPanningInYAxis;

    RetainPtr<WKSyntheticTapGestureRecognizer> _singleTapGestureRecognizer;
    RetainPtr<WKHighlightLongPressGestureRecognizer> _highlightLongPressGestureRecognizer;
    RetainPtr<UILongPressGestureRecognizer> _longPressGestureRecognizer;
    RetainPtr<WKSyntheticTapGestureRecognizer> _doubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _nonBlockingDoubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _doubleTapGestureRecognizerForDoubleClick;
    RetainPtr<UITapGestureRecognizer> _twoFingerDoubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _twoFingerSingleTapGestureRecognizer;
    RetainPtr<WKInspectorNodeSearchGestureRecognizer> _inspectorNodeSearchGestureRecognizer;

    RetainPtr<WKTouchActionGestureRecognizer> _touchActionGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionLeftSwipeGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionRightSwipeGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionUpSwipeGestureRecognizer;
    RetainPtr<UISwipeGestureRecognizer> _touchActionDownSwipeGestureRecognizer;

#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
    RetainPtr<_UILookupGestureRecognizer> _lookupGestureRecognizer;
#endif

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    RetainPtr<WKMouseGestureRecognizer> _mouseGestureRecognizer;
    WebCore::MouseEventPolicy _mouseEventPolicy;
#if ENABLE(PENCIL_HOVER)
    RetainPtr<WKMouseGestureRecognizer> _pencilHoverGestureRecognizer;
#endif
#endif

#if HAVE(PENCILKIT_TEXT_INPUT)
    RetainPtr<UIIndirectScribbleInteraction> _scribbleInteraction;
#endif

#if HAVE(UI_POINTER_INTERACTION)
    RetainPtr<UIPointerInteraction> _pointerInteraction;
    BOOL _hasOutstandingPointerInteractionRequest;
    std::optional<std::pair<WebKit::InteractionInformationRequest, BlockPtr<void(UIPointerRegion *)>>> _deferredPointerInteractionRequest;
#endif

    RetainPtr<UIWKTextInteractionAssistant> _textInteractionAssistant;
    OptionSet<WebKit::SuppressSelectionAssistantReason> _suppressSelectionAssistantReasons;

    RetainPtr<UITextInputTraits> _traits;
    RetainPtr<UIWebFormAccessory> _formAccessoryView;
    RetainPtr<_UIHighlightView> _highlightView;
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
    WebKit::FrameInfoData _frameInfoForFileUploadPanel;
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    RetainPtr<WKShareSheet> _shareSheet;
#endif
#if HAVE(CONTACTSUI)
    RetainPtr<WKContactPicker> _contactPicker;
#endif
    RetainPtr<UIGestureRecognizer> _previewGestureRecognizer;
    RetainPtr<UIGestureRecognizer> _previewSecondaryGestureRecognizer;
    Vector<bool> _focusStateStack;
#if HAVE(LINK_PREVIEW)
#if USE(UICONTEXTMENU)
    RetainPtr<UIContextMenuInteraction> _contextMenuInteraction;
    RetainPtr<WKContextMenuElementInfo> _contextMenuElementInfo;
    BOOL _showLinkPreviews;
    RetainPtr<UIViewController> _contextMenuLegacyPreviewController;
    RetainPtr<UIMenu> _contextMenuLegacyMenu;
    BOOL _contextMenuHasRequestedLegacyData;
    BOOL _contextMenuActionProviderDelegateNeedsOverride;
    BOOL _contextMenuIsUsingAlternateURLForImage;
    BOOL _isDisplayingContextMenuWithAnimation;
#endif
    RetainPtr<UIPreviewItemController> _previewItemController;
#endif

    RefPtr<WebCore::TextIndicator> _textIndicator;
    RetainPtr<WebTextIndicatorLayer> _textIndicatorLayer;

#if USE(UICONTEXTMENU)
    RetainPtr<UITargetedPreview> _contextMenuInteractionTargetedPreview;
#endif

    std::unique_ptr<WebKit::SmartMagnificationController> _smartMagnificationController;

    WeakObjCPtr<id <UITextInputDelegate>> _inputDelegate;

    WebKit::TapIdentifier _latestTapID;
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
    WebKit::FocusedElementInformation _focusedElementInformation;
    RetainPtr<NSObject<WKFormPeripheral>> _inputPeripheral;
    BlockPtr<void(::WebEvent *, BOOL)> _keyWebEventHandler;

    CGPoint _lastInteractionLocation;
    WebKit::TransactionID _layerTreeTransactionIdAtLastInteractionStart;

    WebKit::WKSelectionDrawingInfo _lastSelectionDrawingInfo;
    RetainPtr<WKTextRange> _cachedSelectedTextRange;

    std::optional<WebKit::InteractionInformationRequest> _lastOutstandingPositionInformationRequest;

    uint64_t _positionInformationCallbackDepth;
    Vector<std::optional<InteractionInformationRequestAndCallback>> _pendingPositionInformationHandlers;
    
    std::unique_ptr<WebKit::InputViewUpdateDeferrer> _inputViewUpdateDeferrer;

    RetainPtr<WKKeyboardScrollViewAnimator> _keyboardScrollingAnimator;

    Vector<BlockPtr<void()>> _actionsToPerformAfterEditorStateUpdate;

#if ENABLE(DATALIST_ELEMENT)
    RetainPtr<UIView <WKFormControl>> _dataListTextSuggestionsInputView;
    RetainPtr<NSArray<UITextSuggestion *>> _dataListTextSuggestions;
    WeakObjCPtr<WKDataListSuggestionsControl> _dataListSuggestionsControl;
#endif

    BOOL _isEditable;
    BOOL _showingTextStyleOptions;
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
    BOOL _waitingForKeyboardToStartAnimatingInAfterElementFocus;
    BOOL _shouldZoomToFocusRectAfterShowingKeyboard;
    BOOL _isHidingKeyboard;
    BOOL _isPreparingEditMenu;

    BOOL _focusRequiresStrongPasswordAssistance;
    BOOL _waitingForEditDragSnapshot;
    NSInteger _dropAnimationCount;

    BOOL _hasSetUpInteractions;
    NSUInteger _ignoreSelectionCommandFadeCount;
    NSUInteger _activeTextInteractionCount;
    NSInteger _suppressNonEditableSingleTapTextInteractionCount;
    CompletionHandler<void(WebCore::DOMPasteAccessResponse)> _domPasteRequestHandler;
    std::optional<WebCore::DOMPasteAccessCategory> _domPasteRequestCategory;
    BlockPtr<void(UIWKAutocorrectionContext *)> _pendingAutocorrectionContextHandler;
    CompletionHandler<void()> _pendingRunModalJavaScriptDialogCallback;

    RetainPtr<NSDictionary> _additionalContextForStrongPasswordAssistance;

    std::optional<UChar32> _lastInsertedCharacterToOverrideCharacterBeforeSelection;
    unsigned _selectionChangeNestingLevel;

#if ENABLE(DRAG_SUPPORT)
    WebKit::DragDropInteractionState _dragDropInteractionState;
    RetainPtr<UIDragInteraction> _dragInteraction;
    RetainPtr<UIDropInteraction> _dropInteraction;
    BOOL _isAnimatingDragCancel;
    BOOL _shouldRestoreCalloutBarAfterDrop;
    RetainPtr<UIView> _visibleContentViewSnapshot;
    RetainPtr<UIView> _unselectedContentSnapshot;
    RetainPtr<_UITextDragCaretView> _editDropCaretView;
    BlockPtr<void()> _actionToPerformAfterReceivingEditDragSnapshot;
#endif

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
    RetainPtr<UILongPressGestureRecognizer> _imageAnalysisTimeoutGestureRecognizer;
    std::optional<WebKit::ImageAnalysisRequestIdentifier> _pendingImageAnalysisRequestIdentifier;
    std::optional<WebCore::ElementContext> _elementPendingImageAnalysis;
    Vector<BlockPtr<void(WebKit::ProceedWithTextSelectionInImage)>> _actionsToPerformAfterPendingImageAnalysis;
#if USE(UICONTEXTMENU)
    BOOL _contextMenuWasTriggeredByImageAnalysisTimeout;
#endif // USE(UICONTEXTMENU)
    BOOL _isProceedingWithTextSelectionInImage;
    RetainPtr<CocoaImageAnalyzer> _imageAnalyzer;
#if USE(QUICK_LOOK)
    RetainPtr<QLPreviewController> _visualSearchPreviewController;
    RetainPtr<UIImage> _visualSearchPreviewImage;
    RetainPtr<NSURL> _visualSearchPreviewImageURL;
    RetainPtr<NSString> _visualSearchPreviewTitle;
    CGRect _visualSearchPreviewImageBounds;
#endif // USE(QUICK_LOOK)
    BOOL _waitingForDynamicImageAnalysisContextMenuActions;
    std::optional<WebKit::ImageAnalysisContextMenuActionData> _imageAnalysisContextMenuActionData;
#endif // ENABLE(IMAGE_ANALYSIS)
    uint32_t _fullscreenVideoImageAnalysisRequestIdentifier;
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    RetainPtr<VKCImageAnalysisInteraction> _imageAnalysisInteraction;
    RetainPtr<NSMutableSet<UIButton *>> _imageAnalysisActionButtons;
    WebCore::FloatRect _imageAnalysisInteractionBounds;
    std::optional<WebKit::RemoveBackgroundData> _removeBackgroundData;
#endif
}

@end

@interface WKContentView (WKInteraction) <UIGestureRecognizerDelegate, UITextAutoscrolling, UITextInputMultiDocument, UITextInputPrivate, UIWebFormAccessoryDelegate, UIWebTouchEventsGestureRecognizerDelegate, UIWKInteractionViewProtocol, _UITextInputTranslationSupport, WKActionSheetAssistantDelegate, WKFileUploadPanelDelegate, WKKeyboardScrollViewAnimatorDelegate, WKDeferringGestureRecognizerDelegate
#if HAVE(CONTACTSUI)
    , WKContactPickerDelegate
#endif
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    , WKShareSheetDelegate
#endif
#if ENABLE(DRAG_SUPPORT)
    , UIDragInteractionDelegate, UIDropInteractionDelegate
#endif
    , WKTouchActionGestureRecognizerDelegate
#if HAVE(UIFINDINTERACTION)
    , UITextSearching
#endif
>

@property (nonatomic, readonly) CGPoint lastInteractionLocation;
@property (nonatomic, readonly) BOOL isEditable;
@property (nonatomic, readonly) BOOL shouldHideSelectionWhenScrolling;
@property (nonatomic, readonly) BOOL shouldIgnoreKeyboardWillHideNotification;
@property (nonatomic, readonly) const WebKit::InteractionInformationAtPosition& positionInformation;
@property (nonatomic, readonly) const WebKit::WKAutoCorrectionData& autocorrectionData;
@property (nonatomic, readonly) const WebKit::FocusedElementInformation& focusedElementInformation;
@property (nonatomic, readonly) UIWebFormAccessory *formAccessoryView;
@property (nonatomic, readonly) UITextInputAssistantItem *inputAssistantItemForWebView;
@property (nonatomic, readonly) UIView *inputViewForWebView;
@property (nonatomic, readonly) UIView *inputAccessoryViewForWebView;
@property (nonatomic, readonly) UITextInputTraits *textInputTraitsForWebView;
@property (nonatomic, readonly) BOOL preventsPanningInXAxis;
@property (nonatomic, readonly) BOOL preventsPanningInYAxis;
@property (nonatomic, readonly) UIWebTouchEventsGestureRecognizer *touchEventGestureRecognizer;
@property (nonatomic, readonly) NSArray<WKDeferringGestureRecognizer *> *deferringGestures;
@property (nonatomic, readonly) WebKit::GestureRecognizerConsistencyEnforcer& gestureRecognizerConsistencyEnforcer;
@property (nonatomic, readonly) CGRect tapHighlightViewRect;
@property (nonatomic, readonly) UIGestureRecognizer *imageAnalysisGestureRecognizer;

#if ENABLE(DATALIST_ELEMENT)
@property (nonatomic, strong) UIView <WKFormControl> *dataListTextSuggestionsInputView;
@property (nonatomic, strong) NSArray<UITextSuggestion *> *dataListTextSuggestions;
#endif

- (void)setUpInteraction;
- (void)cleanUpInteraction;
- (void)cleanUpInteractionPreviewContainers;

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

#define DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW(_action) \
    - (void)_action ## ForWebView:(id)sender;
FOR_EACH_WKCONTENTVIEW_ACTION(DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW)
FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW)
#undef DECLARE_WKCONTENTVIEW_ACTION_FOR_WEB_VIEW

- (void)_setFontForWebView:(UIFont *)fontFamily sender:(id)sender;
- (void)_setFontSizeForWebView:(CGFloat)fontSize sender:(id)sender;
- (void)_setTextColorForWebView:(UIColor *)color sender:(id)sender;

#if ENABLE(TOUCH_EVENTS)
- (void)_webTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsDefault;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
- (void)_doneDeferringTouchStart:(BOOL)preventNativeGestures;
- (void)_doneDeferringTouchMove:(BOOL)preventNativeGestures;
- (void)_doneDeferringTouchEnd:(BOOL)preventNativeGestures;
#endif
- (void)_commitPotentialTapFailed;
- (void)_didNotHandleTapAsClick:(const WebCore::IntPoint&)point;
- (void)_didCompleteSyntheticClick;

- (void)_didGetTapHighlightForRequest:(WebKit::TapIdentifier)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling;

- (BOOL)_mayDisableDoubleTapGesturesDuringSingleTap;
- (void)_disableDoubleTapGesturesDuringTapIfNecessary:(WebKit::TapIdentifier)requestID;
- (void)_handleSmartMagnificationInformationForPotentialTap:(WebKit::TapIdentifier)requestID renderRect:(const WebCore::FloatRect&)renderRect fitEntireRect:(BOOL)fitEntireRect viewportMinimumScale:(double)viewportMinimumScale viewportMaximumScale:(double)viewportMaximumScale nodeIsRootLevel:(BOOL)nodeIsRootLevel;
- (void)_elementDidFocus:(const WebKit::FocusedElementInformation&)information userIsInteracting:(BOOL)userIsInteracting blurPreviousNode:(BOOL)blurPreviousNode activityStateChanges:(OptionSet<WebCore::ActivityState::Flag>)activityStateChanges userObject:(NSObject <NSSecureCoding> *)userObject;
- (void)_updateInputContextAfterBlurringAndRefocusingElement;
- (void)_elementDidBlur;
- (void)_didUpdateInputMode:(WebCore::InputMode)mode;
- (void)_didUpdateEditorState;
- (void)_hardwareKeyboardAvailabilityChanged;
- (void)_selectionChanged;
- (void)_updateChangedSelection;
- (BOOL)_interpretKeyEvent:(::WebEvent *)theEvent isCharEvent:(BOOL)isCharEvent;
- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
- (BOOL)_currentPositionInformationIsValidForRequest:(const WebKit::InteractionInformationRequest&)request;
- (void)_attemptSyntheticClickAtLocation:(CGPoint)location modifierFlags:(UIKeyModifierFlags)modifierFlags;
- (void)_willStartScrollingOrZooming;
- (void)_didScroll;
- (void)_didEndScrollingOrZooming;
- (void)_scrollingNodeScrollingWillBegin:(WebCore::ScrollingNodeID)nodeID;
- (void)_scrollingNodeScrollingDidEnd:(WebCore::ScrollingNodeID)nodeID;
- (void)_showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect routeSharingPolicy:(WebCore::RouteSharingPolicy)policy routingContextUID:(NSString *)contextUID;
- (void)_showRunOpenPanel:(API::OpenPanelParameters*)parameters frameInfo:(const WebKit::FrameInfoData&)frameInfo resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener;
- (void)_showShareSheet:(const WebCore::ShareDataWithParsedURL&)shareData inRect:(std::optional<WebCore::FloatRect>)rect completionHandler:(WTF::CompletionHandler<void(bool)>&&)completionHandler;
- (void)_showContactPicker:(const WebCore::ContactsRequestData&)requestData completionHandler:(WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&)completionHandler;
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
- (NSArray<NSValue *> *)_uiTextSelectionRects;
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

// UIWebFormAccessoryDelegate protocol
- (void)accessoryDone;
- (void)accessoryOpen;

- (void)updateFocusedElementValueAsColor:(UIColor *)value;
- (void)updateFocusedElementValue:(NSString *)value;
- (void)updateFocusedElementSelectedIndex:(uint32_t)index allowsMultipleSelection:(bool)allowsMultipleSelection;
- (void)updateFocusedElementFocusedWithDataListDropdown:(BOOL)value;

- (void)_requestDOMPasteAccessForCategory:(WebCore::DOMPasteAccessCategory)pasteAccessCategory elementRect:(const WebCore::IntRect&)elementRect originIdentifier:(const String&)originIdentifier completionHandler:(CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&)completionHandler;

- (void)doAfterPositionInformationUpdate:(void (^)(WebKit::InteractionInformationAtPosition))action forRequest:(WebKit::InteractionInformationRequest)request;
- (BOOL)ensurePositionInformationIsUpToDate:(WebKit::InteractionInformationRequest)request;

- (void)doAfterEditorStateUpdateAfterFocusingElement:(dispatch_block_t)block;
- (void)runModalJavaScriptDialog:(CompletionHandler<void()>&&)callback;

#if ENABLE(DRAG_SUPPORT)
- (void)_didChangeDragInteractionPolicy;
- (void)_didPerformDragOperation:(BOOL)handled;
- (void)_didHandleDragStartRequest:(BOOL)started;
- (void)_didHandleAdditionalDragItemsRequest:(BOOL)added;
- (void)_startDrag:(RetainPtr<CGImageRef>)image item:(const WebCore::DragItem&)item;
- (void)_willReceiveEditDragSnapshot;
- (void)_didReceiveEditDragSnapshot:(std::optional<WebCore::TextIndicatorData>)data;
- (void)_didChangeDragCaretRect:(CGRect)previousRect currentRect:(CGRect)rect;
#endif

- (void)reloadContextViewForPresentedListViewController;

#if ENABLE(DATALIST_ELEMENT)
- (void)updateTextSuggestionsForInputDelegate;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
- (void)_didEnterFullscreen;
- (void)_didExitFullscreen;
#endif

- (void)_requestTextInputContextsInRect:(CGRect)rect completionHandler:(void (^)(NSArray<_WKTextInputContext *> *))completionHandler;
- (void)_focusTextInputContext:(_WKTextInputContext *)context placeCaretAt:(CGPoint)point completionHandler:(void (^)(UIResponder<UITextInput> *))completionHandler;
- (void)_willBeginTextInteractionInTextInputContext:(_WKTextInputContext *)context;
- (void)_didFinishTextInteractionInTextInputContext:(_WKTextInputContext *)context;

- (void)_handleAutocorrectionContext:(const WebKit::WebAutocorrectionContext&)context;

- (void)_didStartProvisionalLoadForMainFrame;
- (void)_didCommitLoadForMainFrame;

- (void)setUpTextIndicator:(Ref<WebCore::TextIndicator>)textIndicator;
- (void)setTextIndicatorAnimationProgress:(float)NSAnimationProgress;
- (void)clearTextIndicator:(WebCore::TextIndicatorDismissalAnimation)animation;

@property (nonatomic, readonly) BOOL _shouldUseContextMenus;
@property (nonatomic, readonly) BOOL _shouldUseContextMenusForFormControls;
@property (nonatomic, readonly) BOOL _shouldAvoidResizingWhenInputViewBoundsChange;
@property (nonatomic, readonly) BOOL _shouldAvoidScrollingWhenFocusedContentIsVisible;
@property (nonatomic, readonly) BOOL _shouldUseLegacySelectPopoverDismissalBehavior;
@property (nonatomic, readonly) BOOL _shouldAvoidSecurityHeuristicScoreUpdates;

- (void)_didChangeLinkPreviewAvailability;
- (void)setContinuousSpellCheckingEnabled:(BOOL)enabled;

- (void)updateSoftwareKeyboardSuppressionStateFromWebView;

#if USE(UICONTEXTMENU)
- (UIView *)textEffectsWindow;

- (void)presentContextMenu:(UIContextMenuInteraction *)contextMenuInteraction atLocation:(CGPoint)location;

- (UITargetedPreview *)_createTargetedContextMenuHintPreviewForFocusedElement;
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
- (void)_showMediaControlsContextMenu:(WebCore::FloatRect&&)targetFrame items:(Vector<WebCore::MediaControlsContextMenuItem>&&)items completionHandler:(CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&)completionHandler;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(IOS_FORM_CONTROL_REFRESH)
- (BOOL)_formControlRefreshEnabled;
#endif

- (WebCore::DataOwnerType)_dataOwnerForPasteboard:(WebKit::PasteboardAccessIntent)intent;

#if ENABLE(IMAGE_ANALYSIS)
- (void)_endImageAnalysisGestureDeferral:(WebKit::ShouldPreventGestures)shouldPreventGestures;
- (void)requestTextRecognition:(NSURL *)imageURL imageData:(const WebKit::ShareableBitmapHandle&)imageData sourceLanguageIdentifier:(NSString *)sourceLanguageIdentifier targetLanguageIdentifier:(NSString *)targetLanguageIdentifier completionHandler:(CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&)completion;
#endif

#if HAVE(UIFINDINTERACTION)
@property (nonatomic, readonly) BOOL supportsTextReplacementForWebView;
- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition inDocument:(UITextSearchDocumentIdentifier)document;
- (void)didBeginTextSearchOperation;
- (void)didEndTextSearchOperation;

- (void)requestRectForFoundTextRange:(UITextRange *)range completionHandler:(void (^)(CGRect))completionHandler;
#endif

- (void)beginTextRecognitionForFullscreenVideo:(const WebKit::ShareableBitmapHandle&)imageHandle playerViewController:(AVPlayerViewController *)playerViewController;
- (void)cancelTextRecognitionForFullscreenVideo:(AVPlayerViewController *)controller;
@property (nonatomic, readonly) BOOL isTextRecognitionInFullscreenVideoEnabled;

- (void)beginTextRecognitionForVideoInElementFullscreen:(const WebKit::ShareableBitmapHandle&)bitmapHandle bounds:(WebCore::FloatRect)bounds;
- (void)cancelTextRecognitionForVideoInElementFullscreen;

@end

@interface WKContentView (WKTesting)

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

#if ENABLE(DATALIST_ELEMENT)
- (void)_selectDataListOption:(NSInteger)optionIndex;
- (void)_setDataListSuggestionsControl:(WKDataListSuggestionsControl *)control;

@property (nonatomic, readonly) BOOL isShowingDataListSuggestions;
#endif

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

#endif // PLATFORM(IOS_FAMILY)
