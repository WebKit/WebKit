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

#import "config.h"
#import "WKContentViewInteraction.h"

#if PLATFORM(IOS)

#import "APIUIClient.h"
#import "DataDetectorsUISPI.h"
#import "EditingRange.h"
#import "ManagedConfigurationSPI.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebTouchEvent.h"
#import "SafariServicesSPI.h"
#import "SmartMagnificationController.h"
#import "TextInputSPI.h"
#import "UIKitSPI.h"
#import "WKActionSheetAssistant.h"
#import "WKFormInputControl.h"
#import "WKFormSelectControl.h"
#import "WKImagePreviewViewController.h"
#import "WKInspectorNodeSearchGestureRecognizer.h"
#import "WKNSURLExtras.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebEvent.h"
#import "WebIOSEventFactory.h"
#import "WebPageMessages.h"
#import "WebProcessProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKFocusedElementInfo.h"
#import "_WKFormInputSession.h"
#import "_WKInputDelegate.h"
#import "_WKPreviewElementInfoInternal.h"
#import <CoreText/CTFont.h>
#import <CoreText/CTFontDescriptor.h>
#import <MobileCoreServices/UTCoreTypes.h>
#import <WebCore/Color.h>
#import <WebCore/CoreGraphicsSPI.h>
#import <WebCore/DataDetectorsCoreSPI.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/Path.h>
#import <WebCore/PathUtilities.h>
#import <WebCore/RuntimeApplicationChecksIOS.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/WebCoreNSURLExtras.h>
#import <WebCore/WebEvent.h>
#import <WebKit/WebSelectionRect.h> // FIXME: WK2 should not include WebKit headers!
#import <wtf/RetainPtr.h>

@interface UIEvent(UIEventInternal)
@property (nonatomic, assign) UIKeyboardInputFlags _inputFlags;
@end

@interface WKWebEvent : WebEvent
@property (nonatomic, retain) UIEvent *uiEvent;
@end

@implementation WKWebEvent

- (void)dealloc
{
    [_uiEvent release];
    [super dealloc];
}

@end

using namespace WebCore;
using namespace WebKit;

namespace WebKit {

WKSelectionDrawingInfo::WKSelectionDrawingInfo()
    : type(SelectionType::None)
{
}

WKSelectionDrawingInfo::WKSelectionDrawingInfo(const EditorState& editorState)
{
    if (editorState.selectionIsNone) {
        type = SelectionType::None;
        return;
    }

    if (editorState.isInPlugin) {
        type = SelectionType::Plugin;
        return;
    }

    type = SelectionType::Range;
    auto& postLayoutData = editorState.postLayoutData();
    caretRect = postLayoutData.caretRectAtEnd;
    selectionRects = postLayoutData.selectionRects;
}

inline bool operator==(const WKSelectionDrawingInfo& a, const WKSelectionDrawingInfo& b)
{
    if (a.type != b.type)
        return false;

    if (a.type == WKSelectionDrawingInfo::SelectionType::Range) {
        if (a.caretRect != b.caretRect)
            return false;

        if (a.selectionRects.size() != b.selectionRects.size())
            return false;

        for (unsigned i = 0; i < a.selectionRects.size(); ++i) {
            if (a.selectionRects[i].rect() != b.selectionRects[i].rect())
                return false;
        }
    }

    return true;
}

inline bool operator!=(const WKSelectionDrawingInfo& a, const WKSelectionDrawingInfo& b)
{
    return !(a == b);
}

} // namespace WebKit

static const float highlightDelay = 0.12;
static const float tapAndHoldDelay  = 0.75;
const CGFloat minimumTapHighlightRadius = 2.0;

@interface WKTextRange : UITextRange {
    CGRect _startRect;
    CGRect _endRect;
    BOOL _isNone;
    BOOL _isRange;
    BOOL _isEditable;
    NSArray *_selectionRects;
    NSUInteger _selectedTextLength;
}
@property (nonatomic) CGRect startRect;
@property (nonatomic) CGRect endRect;
@property (nonatomic) BOOL isNone;
@property (nonatomic) BOOL isRange;
@property (nonatomic) BOOL isEditable;
@property (nonatomic) NSUInteger selectedTextLength;
@property (copy, nonatomic) NSArray *selectionRects;

+ (WKTextRange *)textRangeWithState:(BOOL)isNone isRange:(BOOL)isRange isEditable:(BOOL)isEditable startRect:(CGRect)startRect endRect:(CGRect)endRect selectionRects:(NSArray *)selectionRects selectedTextLength:(NSUInteger)selectedTextLength;

@end

@interface WKTextPosition : UITextPosition {
    CGRect _positionRect;
}

@property (nonatomic) CGRect positionRect;

+ (WKTextPosition *)textPositionWithRect:(CGRect)positionRect;

@end

@interface WKTextSelectionRect : UITextSelectionRect

@property (nonatomic, retain) WebSelectionRect *webRect;

+ (NSArray *)textSelectionRectsWithWebRects:(NSArray *)webRects;

@end

@interface WKAutocorrectionRects : UIWKAutocorrectionRects
+ (WKAutocorrectionRects *)autocorrectionRectsWithRects:(CGRect)firstRect lastRect:(CGRect)lastRect;
@end

@interface WKAutocorrectionContext : UIWKAutocorrectionContext
+ (WKAutocorrectionContext *)autocorrectionContextWithData:(NSString *)beforeText markedText:(NSString *)markedText selectedText:(NSString *)selectedText afterText:(NSString *)afterText selectedRangeInMarkedText:(NSRange)range;
@end

@interface UITextInteractionAssistant (UITextInteractionAssistant_Internal)
// FIXME: this needs to be moved from the internal header to the private.
- (id)initWithView:(UIResponder <UITextInput> *)view;
- (void)selectWord;
- (void)scheduleReanalysis;
@end

@interface UITextInteractionAssistant (StagingToRemove)
- (void)scheduleReplacementsForText:(NSString *)text;
- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)scheduleChineseTransliterationForText:(NSString *)text;
- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)lookup:(NSString *)textWithContext fromRect:(CGRect)presentationRect;
@end

@interface UIWKSelectionAssistant (StagingToRemove)
- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)lookup:(NSString *)textWithContext fromRect:(CGRect)presentationRect;
@end

@interface UIKeyboardImpl (StagingToRemove)
- (void)didHandleWebKeyEvent;
- (void)didHandleWebKeyEvent:(WebIOSEvent *)event;
- (void)deleteFromInputWithFlags:(NSUInteger)flags;
@end

@interface UIView (UIViewInternalHack)
+ (BOOL)_addCompletion:(void(^)(BOOL))completion;
@end

@protocol UISelectionInteractionAssistant;
#if HAVE(LINK_PREVIEW)
@interface UIPreviewItemController (StagingToRemove)
@property (strong, nonatomic, readonly) UIGestureRecognizer *presentationSecondaryGestureRecognizer;
@end
#endif

@interface DDDetectionController (StagingToRemove)
- (DDResultRef)resultForURL:(NSURL *)url identifier:(NSString *)identifier selectedText:(NSString *)selectedText results:(NSArray *)results context:(NSDictionary *)context extendedContext:(NSDictionary **)extendedContext;
@end

@interface WKFocusedElementInfo : NSObject <_WKFocusedElementInfo>
- (instancetype)initWithAssistedNodeInformation:(const AssistedNodeInformation&)information isUserInitiated:(BOOL)isUserInitiated;
@end

@interface WKFormInputSession : NSObject <_WKFormInputSession>

- (instancetype)initWithContentView:(WKContentView *)view focusedElementInfo:(WKFocusedElementInfo *)elementInfo userObject:(NSObject <NSSecureCoding> *)userObject;
- (void)invalidate;

@end

@implementation WKFormInputSession {
    WKContentView *_contentView;
    RetainPtr<NSObject <NSSecureCoding>> _userObject;
    RetainPtr<WKFocusedElementInfo> _focusedElementInfo;
    RetainPtr<UIView> _customInputView;
}

- (instancetype)initWithContentView:(WKContentView *)view focusedElementInfo:(WKFocusedElementInfo *)elementInfo userObject:(NSObject <NSSecureCoding> *)userObject
{
    if (!(self = [super init]))
        return nil;

    _contentView = view;
    _focusedElementInfo = elementInfo;
    _userObject = userObject;

    return self;
}

- (id <_WKFocusedElementInfo>)focusedElementInfo
{
    return _focusedElementInfo.get();
}

- (NSObject <NSSecureCoding> *)userObject
{
    return _userObject.get();
}

- (BOOL)isValid
{
    return _contentView != nil;
}

- (NSString *)accessoryViewCustomButtonTitle
{
    return [[[_contentView formAccessoryView] _autofill] title];
}

- (void)setAccessoryViewCustomButtonTitle:(NSString *)title
{
    if (title.length)
        [[_contentView formAccessoryView] showAutoFillButtonWithTitle:title];
    else
        [[_contentView formAccessoryView] hideAutoFillButton];
    if (UICurrentUserInterfaceIdiomIsPad())
        [_contentView reloadInputViews];
}

- (UIView *)customInputView
{
    return _customInputView.get();
}

- (void)setCustomInputView:(UIView *)customInputView
{
    if (customInputView == _customInputView)
        return;

    _customInputView = customInputView;
    [_contentView reloadInputViews];
}

- (void)invalidate
{
    _contentView = nil;
}

@end

@implementation WKFocusedElementInfo {
    WKInputType _type;
    RetainPtr<NSString> _value;
    BOOL _isUserInitiated;
}

- (instancetype)initWithAssistedNodeInformation:(const AssistedNodeInformation&)information isUserInitiated:(BOOL)isUserInitiated
{
    if (!(self = [super init]))
        return nil;

    switch (information.elementType) {
    case WebKit::InputType::ContentEditable:
        _type = WKInputTypeContentEditable;
        break;
    case WebKit::InputType::Text:
        _type = WKInputTypeText;
        break;
    case WebKit::InputType::Password:
        _type = WKInputTypePassword;
        break;
    case WebKit::InputType::TextArea:
        _type = WKInputTypeTextArea;
        break;
    case WebKit::InputType::Search:
        _type = WKInputTypeSearch;
        break;
    case WebKit::InputType::Email:
        _type = WKInputTypeEmail;
        break;
    case WebKit::InputType::URL:
        _type = WKInputTypeURL;
        break;
    case WebKit::InputType::Phone:
        _type = WKInputTypePhone;
        break;
    case WebKit::InputType::Number:
        _type = WKInputTypeNumber;
        break;
    case WebKit::InputType::NumberPad:
        _type = WKInputTypeNumberPad;
        break;
    case WebKit::InputType::Date:
        _type = WKInputTypeDate;
        break;
    case WebKit::InputType::DateTime:
        _type = WKInputTypeDateTime;
        break;
    case WebKit::InputType::DateTimeLocal:
        _type = WKInputTypeDateTimeLocal;
        break;
    case WebKit::InputType::Month:
        _type = WKInputTypeMonth;
        break;
    case WebKit::InputType::Week:
        _type = WKInputTypeWeek;
        break;
    case WebKit::InputType::Time:
        _type = WKInputTypeTime;
        break;
    case WebKit::InputType::Select:
        _type = WKInputTypeSelect;
        break;
    case WebKit::InputType::None:
        _type = WKInputTypeNone;
        break;
    }
    _value = information.value;
    _isUserInitiated = isUserInitiated;
    return self;
}

- (WKInputType)type
{
    return _type;
}

- (NSString *)value
{
    return _value.get();
}

- (BOOL)isUserInitiated
{
    return _isUserInitiated;
}
@end

@interface WKContentView (WKInteractionPrivate)
- (void)accessibilitySpeakSelectionSetContent:(NSString *)string;
@end

@implementation WKContentView (WKInteraction)

static UIWebSelectionMode toUIWebSelectionMode(WKSelectionGranularity granularity)
{
    switch (granularity) {
    case WKSelectionGranularityDynamic:
        return UIWebSelectionModeWeb;
    case WKSelectionGranularityCharacter:
        return UIWebSelectionModeTextOnly;
    }

    ASSERT_NOT_REACHED();
    return UIWebSelectionModeWeb;
}

- (void)_createAndConfigureDoubleTapGestureRecognizer
{
    _doubleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_doubleTapRecognized:)]);
    [_doubleTapGestureRecognizer setNumberOfTapsRequired:2];
    [_doubleTapGestureRecognizer setAllowedTouchTypes:@[@(UITouchTypeDirect)]];
    [_doubleTapGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [_singleTapGestureRecognizer requireOtherGestureToFail:_doubleTapGestureRecognizer.get()];
}

- (void)setupInteraction
{
    if (!_interactionViewsContainerView) {
        _interactionViewsContainerView = adoptNS([[UIView alloc] init]);
        [_interactionViewsContainerView setOpaque:NO];
        [_interactionViewsContainerView layer].anchorPoint = CGPointZero;
        [self.superview addSubview:_interactionViewsContainerView.get()];
    }

    [self.layer addObserver:self forKeyPath:@"transform" options:NSKeyValueObservingOptionInitial context:nil];

#if ENABLE(TOUCH_EVENTS)
    _touchEventGestureRecognizer = adoptNS([[UIWebTouchEventsGestureRecognizer alloc] initWithTarget:self action:@selector(_webTouchEventsRecognized:) touchDelegate:self]);
    [_touchEventGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_touchEventGestureRecognizer.get()];
#endif

    _singleTapGestureRecognizer = adoptNS([[WKSyntheticClickTapGestureRecognizer alloc] initWithTarget:self action:@selector(_singleTapCommited:)]);
    [_singleTapGestureRecognizer setDelegate:self];
    [_singleTapGestureRecognizer setGestureRecognizedTarget:self action:@selector(_singleTapRecognized:)];
    [_singleTapGestureRecognizer setResetTarget:self action:@selector(_singleTapDidReset:)];
    [self addGestureRecognizer:_singleTapGestureRecognizer.get()];

    [self _createAndConfigureDoubleTapGestureRecognizer];

    _twoFingerDoubleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_twoFingerDoubleTapRecognized:)]);
    [_twoFingerDoubleTapGestureRecognizer setNumberOfTapsRequired:2];
    [_twoFingerDoubleTapGestureRecognizer setNumberOfTouchesRequired:2];
    [_twoFingerDoubleTapGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];

    _highlightLongPressGestureRecognizer = adoptNS([[_UIWebHighlightLongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_highlightLongPressRecognized:)]);
    [_highlightLongPressGestureRecognizer setDelay:highlightDelay];
    [_highlightLongPressGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];

    _longPressGestureRecognizer = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_longPressRecognized:)]);
    [_longPressGestureRecognizer setDelay:tapAndHoldDelay];
    [_longPressGestureRecognizer setDelegate:self];
    [_longPressGestureRecognizer _setRequiresQuietImpulse:YES];
    [self addGestureRecognizer:_longPressGestureRecognizer.get()];

#if HAVE(LINK_PREVIEW)
    [self _registerPreview];
#endif

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_resetShowingTextStyle:) name:UIMenuControllerDidHideMenuNotification object:nil];
    _showingTextStyleOptions = NO;

    // FIXME: This should be called when we get notified that loading has completed.
    [self useSelectionAssistantWithMode:toUIWebSelectionMode([_webView _selectionGranularity])];
    
    _actionSheetAssistant = adoptNS([[WKActionSheetAssistant alloc] initWithView:self]);
    [_actionSheetAssistant setDelegate:self];
    _smartMagnificationController = std::make_unique<SmartMagnificationController>(self);
    _isExpectingFastSingleTapCommit = NO;
    _showDebugTapHighlightsForFastClicking = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitShowFastClickDebugTapHighlights"];
}

- (void)cleanupInteraction
{
    _webSelectionAssistant = nil;
    _textSelectionAssistant = nil;
    _actionSheetAssistant = nil;
    _smartMagnificationController = nil;
    _didAccessoryTabInitiateFocus = NO;
    _isExpectingFastSingleTapCommit = NO;
    [_formInputSession invalidate];
    _formInputSession = nil;
    [_highlightView removeFromSuperview];

    if (_interactionViewsContainerView) {
        [self.layer removeObserver:self forKeyPath:@"transform"];
        [_interactionViewsContainerView removeFromSuperview];
        _interactionViewsContainerView = nil;
    }

    [_touchEventGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_touchEventGestureRecognizer.get()];

    [_singleTapGestureRecognizer setDelegate:nil];
    [_singleTapGestureRecognizer setGestureRecognizedTarget:nil action:nil];
    [_singleTapGestureRecognizer setResetTarget:nil action:nil];
    [self removeGestureRecognizer:_singleTapGestureRecognizer.get()];

    [_highlightLongPressGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_highlightLongPressGestureRecognizer.get()];

    [_longPressGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_longPressGestureRecognizer.get()];

    [_doubleTapGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];

    [_twoFingerDoubleTapGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];

    _inspectorNodeSearchEnabled = NO;
    if (_inspectorNodeSearchGestureRecognizer) {
        [_inspectorNodeSearchGestureRecognizer setDelegate:nil];
        [self removeGestureRecognizer:_inspectorNodeSearchGestureRecognizer.get()];
        _inspectorNodeSearchGestureRecognizer = nil;
    }

#if HAVE(LINK_PREVIEW)
    [self _unregisterPreview];
#endif

    if (_fileUploadPanel) {
        [_fileUploadPanel setDelegate:nil];
        [_fileUploadPanel dismiss];
        _fileUploadPanel = nil;
    }
}

- (void)_removeDefaultGestureRecognizers
{
    [self removeGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self removeGestureRecognizer:_singleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
    [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];
}

- (void)_addDefaultGestureRecognizers
{
    [self addGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self addGestureRecognizer:_singleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
    [self addGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];
}

- (UIView*)unscaledView
{
    return _interactionViewsContainerView.get();
}

- (CGFloat)inverseScale
{
    return 1 / [[self layer] transform].m11;
}

- (UIScrollView *)_scroller
{
    return [_webView scrollView];
}

- (CGRect)unobscuredContentRect
{
    return _page->unobscuredContentRect();
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    ASSERT([keyPath isEqualToString:@"transform"]);
    ASSERT(object == self.layer);

    if ([UIView _isInAnimationBlock] && _page->editorState().selectionIsNone) {
        // If the utility views are not already visible, we don't want them to become visible during the animation since
        // they could not start from a reasonable state.
        // This is not perfect since views could also get updated during the animation, in practice this is rare and the end state
        // remains correct.
        [self _cancelInteraction];
        [_interactionViewsContainerView setHidden:YES];
        [UIView _addCompletion:^(BOOL){ [_interactionViewsContainerView setHidden:NO]; }];
    }

    _selectionNeedsUpdate = YES;
    [self _updateChangedSelection:YES];
    [self _updateTapHighlight];
}

- (void)_enableInspectorNodeSearch
{
    _inspectorNodeSearchEnabled = YES;

    [self _cancelInteraction];

    [self _removeDefaultGestureRecognizers];
    _inspectorNodeSearchGestureRecognizer = adoptNS([[WKInspectorNodeSearchGestureRecognizer alloc] initWithTarget:self action:@selector(_inspectorNodeSearchRecognized:)]);
    [self addGestureRecognizer:_inspectorNodeSearchGestureRecognizer.get()];
}

- (void)_disableInspectorNodeSearch
{
    _inspectorNodeSearchEnabled = NO;

    [self _addDefaultGestureRecognizers];
    [self removeGestureRecognizer:_inspectorNodeSearchGestureRecognizer.get()];
    _inspectorNodeSearchGestureRecognizer = nil;
}

- (UIView *)hitTest:(CGPoint)point withEvent:(::UIEvent *)event
{
    for (UIView *subView in [_interactionViewsContainerView.get() subviews]) {
        UIView *hitView = [subView hitTest:[subView convertPoint:point fromView:self] withEvent:event];
        if (hitView)
            return hitView;
    }
    return [super hitTest:point withEvent:event];
}

- (const InteractionInformationAtPosition&)positionInformation
{
    return _positionInformation;
}

- (void)setInputDelegate:(id <UITextInputDelegate>)inputDelegate
{
    _inputDelegate = inputDelegate;
}

- (id <UITextInputDelegate>)inputDelegate
{
    return _inputDelegate;
}

- (CGPoint)lastInteractionLocation
{
    return _lastInteractionLocation;
}

- (BOOL)isEditable
{
    return _isEditable;
}

- (BOOL)canBecomeFirstResponder
{
    if (_resigningFirstResponder)
        return NO;
    // We might want to return something else
    // if we decide to enable/disable interaction programmatically.
    return YES;
}

- (BOOL)becomeFirstResponder
{
    if (_resigningFirstResponder)
        return NO;
    BOOL didBecomeFirstResponder = [super becomeFirstResponder];
    if (didBecomeFirstResponder)
        [_textSelectionAssistant activateSelection];

    return didBecomeFirstResponder;
}

- (BOOL)resignFirstResponder
{
    // FIXME: Maybe we should call resignFirstResponder on the superclass
    // and do nothing if the return value is NO.

    _resigningFirstResponder = YES;

    if (!_webView->_activeFocusedStateRetainCount) {
        // We need to complete the editing operation before we blur the element.
        [_inputPeripheral endEditing];
        _page->blurAssistedNode();
    }

    [self _cancelInteraction];
    [_webSelectionAssistant resignedFirstResponder];
    [_textSelectionAssistant deactivateSelection];

    bool superDidResign = [super resignFirstResponder];

    _resigningFirstResponder = NO;

    return superDidResign;
}

#if ENABLE(TOUCH_EVENTS)
- (void)_webTouchEventsRecognized:(UIWebTouchEventsGestureRecognizer *)gestureRecognizer
{
    const _UIWebTouchEvent* lastTouchEvent = gestureRecognizer.lastTouchEvent;
    NativeWebTouchEvent nativeWebTouchEvent(lastTouchEvent);

    _lastInteractionLocation = lastTouchEvent->locationInDocumentCoordinates;
    nativeWebTouchEvent.setCanPreventNativeGestures(!_canSendTouchEventsAsynchronously || [gestureRecognizer isDefaultPrevented]);

    if (_canSendTouchEventsAsynchronously)
        _page->handleTouchEventAsynchronously(nativeWebTouchEvent);
    else
        _page->handleTouchEventSynchronously(nativeWebTouchEvent);

    if (nativeWebTouchEvent.allTouchPointsAreReleased())
        _canSendTouchEventsAsynchronously = NO;
}
#endif

- (void)_inspectorNodeSearchRecognized:(UIGestureRecognizer *)gestureRecognizer
{
    ASSERT(_inspectorNodeSearchEnabled);

    CGPoint point = [gestureRecognizer locationInView:self];

    switch (gestureRecognizer.state) {
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
        _page->inspectorNodeSearchMovedToPosition(point);
        break;
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    default: // To ensure we turn off node search.
        _page->inspectorNodeSearchEndedAtPosition(point);
        break;
    }
}

static FloatQuad inflateQuad(const FloatQuad& quad, float inflateSize)
{
    // We sort the output points like this (as expected by the highlight view):
    //  p2------p3
    //  |       |
    //  p1------p4

    // 1) Sort the points horizontally.
    FloatPoint points[4] = { quad.p1(), quad.p4(), quad.p2(), quad.p3() };
    if (points[0].x() > points[1].x())
        std::swap(points[0], points[1]);
    if (points[2].x() > points[3].x())
        std::swap(points[2], points[3]);

    if (points[0].x() > points[2].x())
        std::swap(points[0], points[2]);
    if (points[1].x() > points[3].x())
        std::swap(points[1], points[3]);

    if (points[1].x() > points[2].x())
        std::swap(points[1], points[2]);

    // 2) Swap them vertically to have the output points [p2, p1, p3, p4].
    if (points[1].y() < points[0].y())
        std::swap(points[0], points[1]);
    if (points[3].y() < points[2].y())
        std::swap(points[2], points[3]);

    // 3) Adjust the positions.
    points[0].move(-inflateSize, -inflateSize);
    points[1].move(-inflateSize, inflateSize);
    points[2].move(inflateSize, -inflateSize);
    points[3].move(inflateSize, inflateSize);

    return FloatQuad(points[1], points[0], points[2], points[3]);
}

#if ENABLE(TOUCH_EVENTS)
- (void)_webTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsNativeGesture
{
    if (preventsNativeGesture) {
        _highlightLongPressCanClick = NO;

        _canSendTouchEventsAsynchronously = YES;
        [_touchEventGestureRecognizer setDefaultPrevented:YES];
    }
}
#endif

static inline bool highlightedQuadsAreSmallerThanRect(const Vector<FloatQuad>& quads, const FloatRect& rect)
{
    for (size_t i = 0; i < quads.size(); ++i) {
        FloatRect boundingBox = quads[i].boundingBox();
        if (boundingBox.width() > rect.width() || boundingBox.height() > rect.height())
            return false;
    }
    return true;
}

static NSValue *nsSizeForTapHighlightBorderRadius(WebCore::IntSize borderRadius, CGFloat borderRadiusScale)
{
    return [NSValue valueWithCGSize:CGSizeMake((borderRadius.width() * borderRadiusScale) + minimumTapHighlightRadius, (borderRadius.height() * borderRadiusScale) + minimumTapHighlightRadius)];
}

- (void)_updateTapHighlight
{
    if (![_highlightView superview])
        return;

    {
        RetainPtr<UIColor> highlightUIKitColor = adoptNS([[UIColor alloc] initWithCGColor:cachedCGColor(_tapHighlightInformation.color)]);
        [_highlightView setColor:highlightUIKitColor.get()];
    }

    CGFloat selfScale = self.layer.transform.m11;
    bool allHighlightRectsAreRectilinear = true;
    float deviceScaleFactor = _page->deviceScaleFactor();
    const Vector<WebCore::FloatQuad>& highlightedQuads = _tapHighlightInformation.quads;
    const size_t quadCount = highlightedQuads.size();
    RetainPtr<NSMutableArray> rects = adoptNS([[NSMutableArray alloc] initWithCapacity:static_cast<const NSUInteger>(quadCount)]);
    for (size_t i = 0; i < quadCount; ++i) {
        const FloatQuad& quad = highlightedQuads[i];
        if (quad.isRectilinear()) {
            FloatRect boundingBox = quad.boundingBox();
            boundingBox.scale(selfScale);
            boundingBox.inflate(minimumTapHighlightRadius);
            CGRect pixelAlignedRect = static_cast<CGRect>(encloseRectToDevicePixels(boundingBox, deviceScaleFactor));
            [rects addObject:[NSValue valueWithCGRect:pixelAlignedRect]];
        } else {
            allHighlightRectsAreRectilinear = false;
            rects.clear();
            break;
        }
    }

    if (allHighlightRectsAreRectilinear)
        [_highlightView setFrames:rects.get() boundaryRect:_page->exposedContentRect()];
    else {
        RetainPtr<NSMutableArray> quads = adoptNS([[NSMutableArray alloc] initWithCapacity:static_cast<const NSUInteger>(quadCount)]);
        for (size_t i = 0; i < quadCount; ++i) {
            FloatQuad quad = highlightedQuads[i];
            quad.scale(selfScale, selfScale);
            FloatQuad extendedQuad = inflateQuad(quad, minimumTapHighlightRadius);
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p1()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p2()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p3()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p4()]];
        }
        [_highlightView setQuads:quads.get() boundaryRect:_page->exposedContentRect()];
    }

    RetainPtr<NSMutableArray> borderRadii = adoptNS([[NSMutableArray alloc] initWithCapacity:4]);
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.topLeftRadius, selfScale)];
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.topRightRadius, selfScale)];
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.bottomLeftRadius, selfScale)];
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.bottomRightRadius, selfScale)];
    [_highlightView setCornerRadii:borderRadii.get()];
}

- (void)_showTapHighlight
{
    if (!highlightedQuadsAreSmallerThanRect(_tapHighlightInformation.quads, _page->unobscuredContentRect()) && !_showDebugTapHighlightsForFastClicking)
        return;

    if (!_highlightView) {
        _highlightView = adoptNS([[_UIHighlightView alloc] initWithFrame:CGRectZero]);
        [_highlightView setUserInteractionEnabled:NO];
        [_highlightView setOpaque:NO];
        [_highlightView setCornerRadius:minimumTapHighlightRadius];
    }
    [_highlightView layer].opacity = 1;
    [_interactionViewsContainerView addSubview:_highlightView.get()];
    [self _updateTapHighlight];
}

- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius
{
    if (!_isTapHighlightIDValid || _latestTapID != requestID)
        return;

    _isTapHighlightIDValid = NO;

    _tapHighlightInformation.quads = highlightedQuads;
    _tapHighlightInformation.topLeftRadius = topLeftRadius;
    _tapHighlightInformation.topRightRadius = topRightRadius;
    _tapHighlightInformation.bottomLeftRadius = bottomLeftRadius;
    _tapHighlightInformation.bottomRightRadius = bottomRightRadius;
    if (_showDebugTapHighlightsForFastClicking)
        _tapHighlightInformation.color = [self _tapHighlightColorForFastClick:![_doubleTapGestureRecognizer isEnabled]];
    else
        _tapHighlightInformation.color = color;

    if (_potentialTapInProgress) {
        _hasTapHighlightForPotentialTap = YES;
        return;
    }

    [self _showTapHighlight];
    if (_isExpectingFastSingleTapCommit) {
        [self _finishInteraction];
        if (!_potentialTapInProgress)
            _isExpectingFastSingleTapCommit = NO;
    }
}

- (BOOL)_mayDisableDoubleTapGesturesDuringSingleTap
{
    return _potentialTapInProgress;
}

- (void)_disableDoubleTapGesturesDuringTapIfNecessary:(uint64_t)requestID
{
    if (!_potentialTapInProgress || _latestTapID != requestID)
        return;

    [self _setDoubleTapGesturesEnabled:NO];
}

- (void)_cancelLongPressGestureRecognizer
{
    [_highlightLongPressGestureRecognizer cancel];
}

- (void)_didScroll
{
    [self _cancelLongPressGestureRecognizer];
    [self _cancelInteraction];
}

- (void)_overflowScrollingWillBegin
{
    [_webSelectionAssistant willStartScrollingOverflow];
    [_textSelectionAssistant willStartScrollingOverflow];    
}

- (void)_overflowScrollingDidEnd
{
    // If scrolling ends before we've received a selection update,
    // we postpone showing the selection until the update is received.
    if (!_selectionNeedsUpdate) {
        _shouldRestoreSelection = YES;
        return;
    }
    [self _updateChangedSelection];
    [_webSelectionAssistant didEndScrollingOverflow];
    [_textSelectionAssistant didEndScrollingOverflow];
}

- (BOOL)_requiresKeyboardWhenFirstResponder
{
    // FIXME: We should add the logic to handle keyboard visibility during focus redirects.
    switch (_assistedNodeInformation.elementType) {
    case InputType::None:
        return NO;
    case InputType::Select:
        return !UICurrentUserInterfaceIdiomIsPad();
    case InputType::Date:
    case InputType::Month:
    case InputType::DateTimeLocal:
    case InputType::Time:
        return !UICurrentUserInterfaceIdiomIsPad();
    default:
        return !_assistedNodeInformation.isReadOnly;
    }
    return NO;
}

- (BOOL)_requiresKeyboardResetOnReload
{
    return YES;
}

- (void)_displayFormNodeInputView
{
    [self _zoomToFocusRect:_assistedNodeInformation.elementRect
             selectionRect: _didAccessoryTabInitiateFocus ? IntRect() : _assistedNodeInformation.selectionRect
                  fontSize:_assistedNodeInformation.nodeFontSize
              minimumScale:_assistedNodeInformation.minimumScaleFactor
              maximumScale:_assistedNodeInformation.maximumScaleFactor
              allowScaling:(_assistedNodeInformation.allowsUserScaling && !UICurrentUserInterfaceIdiomIsPad())
               forceScroll:[self requiresAccessoryView]];
    _didAccessoryTabInitiateFocus = NO;
    [self _ensureFormAccessoryView];
    [self _updateAccessory];
}

- (UIView *)inputView
{
    if (_assistedNodeInformation.elementType == InputType::None)
        return nil;

    if (!_inputPeripheral)
        _inputPeripheral = adoptNS(_assistedNodeInformation.elementType == InputType::Select ? [WKFormSelectControl createPeripheralWithView:self] : [WKFormInputControl createPeripheralWithView:self]);
    else
        [self _displayFormNodeInputView];

    return [_formInputSession customInputView] ?: [_inputPeripheral assistantView];
}

- (CGRect)_selectionClipRect
{
    if (_assistedNodeInformation.elementType == InputType::None)
        return CGRectZero;
    return _page->editorState().postLayoutData().selectionClipRect;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer canPreventGestureRecognizer:(UIGestureRecognizer *)preventedGestureRecognizer
{
    // A long-press gesture can not be recognized while panning, but a pan can be recognized
    // during a long-press gesture.
    BOOL shouldNotPreventScrollViewGestures = preventingGestureRecognizer == _highlightLongPressGestureRecognizer || preventingGestureRecognizer == _longPressGestureRecognizer;
    return !(shouldNotPreventScrollViewGestures
        && ([preventedGestureRecognizer isKindOfClass:NSClassFromString(@"UIScrollViewPanGestureRecognizer")] || [preventedGestureRecognizer isKindOfClass:NSClassFromString(@"UIScrollViewPinchGestureRecognizer")]));
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)preventedGestureRecognizer canBePreventedByGestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer {
    // Don't allow the highlight to be prevented by a selection gesture. Press-and-hold on a link should highlight the link, not select it.
    if ((preventingGestureRecognizer == _textSelectionAssistant.get().loupeGesture || [_webSelectionAssistant isSelectionGestureRecognizer:preventingGestureRecognizer])
        && (preventedGestureRecognizer == _highlightLongPressGestureRecognizer || preventedGestureRecognizer == _longPressGestureRecognizer)) {
        return NO;
    }

    return YES;
}

static inline bool isSamePair(UIGestureRecognizer *a, UIGestureRecognizer *b, UIGestureRecognizer *x, UIGestureRecognizer *y)
{
    return (a == x && b == y) || (b == x && a == y);
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer*)otherGestureRecognizer
{
    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _longPressGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _webSelectionAssistant.get().selectionLongPressRecognizer))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _singleTapGestureRecognizer.get(), _textSelectionAssistant.get().singleTapGesture))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _previewSecondaryGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _previewGestureRecognizer.get()))
        return YES;

    return NO;
}

- (void)_showImageSheet
{
    [_actionSheetAssistant showImageSheet];
}

- (void)_showLinkSheet
{
    [_actionSheetAssistant showLinkSheet];
}

- (void)_showDataDetectorsSheet
{
    [_actionSheetAssistant showDataDetectorsSheet];
}

- (SEL)_actionForLongPress
{
    if (!_positionInformation.touchCalloutEnabled)
        return nil;

    if (_positionInformation.isImage)
        return @selector(_showImageSheet);

    if (_positionInformation.isLink) {
        NSURL *targetURL = [NSURL URLWithString:_positionInformation.url];
        if ([[getDDDetectionControllerClass() tapAndHoldSchemes] containsObject:[targetURL scheme]])
            return @selector(_showDataDetectorsSheet);
        return @selector(_showLinkSheet);
    }

    return nil;
}

- (void)ensurePositionInformationIsUpToDate:(CGPoint)point
{
    if (!_hasValidPositionInformation || roundedIntPoint(point) != _positionInformation.point) {
        _page->getPositionInformation(roundedIntPoint(point), _positionInformation);
        _hasValidPositionInformation = YES;
    }
}

#if ENABLE(DATA_DETECTION)
- (NSArray *)_dataDetectionResults
{
    return _page->dataDetectionResults();
}
#endif

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    CGPoint point = [gestureRecognizer locationInView:self];

    if (gestureRecognizer == _highlightLongPressGestureRecognizer
        || gestureRecognizer == _doubleTapGestureRecognizer
        || gestureRecognizer == _twoFingerDoubleTapGestureRecognizer
        || gestureRecognizer == _singleTapGestureRecognizer) {

        if (_textSelectionAssistant) {
            // Request information about the position with sync message.
            // If the assisted node is the same, prevent the gesture.
            _page->getPositionInformation(roundedIntPoint(point), _positionInformation);
            _hasValidPositionInformation = YES;
            if (_positionInformation.nodeAtPositionIsAssistedNode)
                return NO;
        }
    }

    if (gestureRecognizer == _highlightLongPressGestureRecognizer) {
        if (_textSelectionAssistant) {
            // This is a different node than the assisted one.
            // Prevent the gesture if there is no node.
            // Allow the gesture if it is a node that wants highlight or if there is an action for it.
            if (!_positionInformation.isClickableElement)
                return NO;
            return [self _actionForLongPress] != nil;
        } else {
            // We still have no idea about what is at the location.
            // Send and async message to find out.
            _hasValidPositionInformation = NO;
            _page->requestPositionInformation(roundedIntPoint(point));
            return YES;
        }
    }

    if (gestureRecognizer == _longPressGestureRecognizer) {
        // Use the information retrieved with one of the previous calls
        // to gestureRecognizerShouldBegin.
        // Force a sync call if not ready yet.
        [self ensurePositionInformationIsUpToDate:point];

        if (_textSelectionAssistant) {
            // Prevent the gesture if it is the same node.
            if (_positionInformation.nodeAtPositionIsAssistedNode)
                return NO;
        } else {
            // Prevent the gesture if there is no action for the node.
            return [self _actionForLongPress] != nil;
        }
    }

    return YES;
}

- (void)_cancelInteraction
{
    _isTapHighlightIDValid = NO;
    [_highlightView removeFromSuperview];
}

- (void)_finishInteraction
{
    _isTapHighlightIDValid = NO;
    CGFloat tapHighlightFadeDuration = _showDebugTapHighlightsForFastClicking ? 0.25 : 0.1;
    [UIView animateWithDuration:tapHighlightFadeDuration
                     animations:^{
                         [_highlightView layer].opacity = 0;
                     }
                     completion:^(BOOL finished){
                         if (finished)
                             [_highlightView removeFromSuperview];
                     }];
}

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point
{
    if (_inspectorNodeSearchEnabled)
        return NO;

    [self ensurePositionInformationIsUpToDate:point];
    return _positionInformation.isSelectable;
}

- (BOOL)pointIsNearMarkedText:(CGPoint)point
{
    [self ensurePositionInformationIsUpToDate:point];
    return _positionInformation.isNearMarkedText;
}

- (BOOL)pointIsInAssistedNode:(CGPoint)point
{
    [self ensurePositionInformationIsUpToDate:point];
    return _positionInformation.nodeAtPositionIsAssistedNode;
}

- (NSArray *)webSelectionRects
{
    if (_page->editorState().selectionIsNone)
        return nil;
    const auto& selectionRects = _page->editorState().postLayoutData().selectionRects;
    unsigned size = selectionRects.size();
    if (!size)
        return nil;

    NSMutableArray *webRects = [NSMutableArray arrayWithCapacity:size];
    for (unsigned i = 0; i < size; i++) {
        const WebCore::SelectionRect& coreRect = selectionRects[i];
        WebSelectionRect *webRect = [WebSelectionRect selectionRect];
        webRect.rect = coreRect.rect();
        webRect.writingDirection = coreRect.direction() == LTR ? WKWritingDirectionLeftToRight : WKWritingDirectionRightToLeft;
        webRect.isLineBreak = coreRect.isLineBreak();
        webRect.isFirstOnLine = coreRect.isFirstOnLine();
        webRect.isLastOnLine = coreRect.isLastOnLine();
        webRect.containsStart = coreRect.containsStart();
        webRect.containsEnd = coreRect.containsEnd();
        webRect.isInFixedPosition = coreRect.isInFixedPosition();
        webRect.isHorizontal = coreRect.isHorizontal();
        [webRects addObject:webRect];
    }

    return webRects;
}

- (void)_highlightLongPressRecognized:(UILongPressGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _highlightLongPressGestureRecognizer);

    _lastInteractionLocation = gestureRecognizer.startPoint;

    switch ([gestureRecognizer state]) {
    case UIGestureRecognizerStateBegan:
        _highlightLongPressCanClick = YES;
        cancelPotentialTapIfNecessary(self);
        _page->tapHighlightAtPosition([gestureRecognizer startPoint], ++_latestTapID);
        _isTapHighlightIDValid = YES;
        break;
    case UIGestureRecognizerStateEnded:
        if (_highlightLongPressCanClick && _positionInformation.isClickableElement) {
            [self _attemptClickAtLocation:[gestureRecognizer startPoint]];
            [self _finishInteraction];
        } else
            [self _cancelInteraction];
        _highlightLongPressCanClick = NO;
        break;
    case UIGestureRecognizerStateCancelled:
        [self _cancelInteraction];
        _highlightLongPressCanClick = NO;
        break;
    default:
        break;
    }
}

- (void)_longPressRecognized:(UILongPressGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _longPressGestureRecognizer);

    _lastInteractionLocation = gestureRecognizer.startPoint;

    if ([gestureRecognizer state] == UIGestureRecognizerStateBegan) {
        SEL action = [self _actionForLongPress];
        if (action) {
            [self performSelector:action];
            [self _cancelLongPressGestureRecognizer];
        }
    }
}

- (void)_endPotentialTapAndEnableDoubleTapGesturesIfNecessary
{
    if (_webView._allowsDoubleTapGestures)
        [self _setDoubleTapGesturesEnabled:YES];

    _potentialTapInProgress = NO;
}

- (void)_singleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);
    ASSERT(!_potentialTapInProgress);

    _page->potentialTapAtPosition(gestureRecognizer.location, ++_latestTapID);
    _potentialTapInProgress = YES;
    _isTapHighlightIDValid = YES;
    _isExpectingFastSingleTapCommit = !_doubleTapGestureRecognizer.get().enabled;
}

static void cancelPotentialTapIfNecessary(WKContentView* contentView)
{
    if (contentView->_potentialTapInProgress) {
        [contentView _endPotentialTapAndEnableDoubleTapGesturesIfNecessary];
        [contentView _cancelInteraction];
        contentView->_page->cancelPotentialTap();
    }
}

- (void)_singleTapDidReset:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);
    cancelPotentialTapIfNecessary(self);
}

- (void)_commitPotentialTapFailed
{
    [self _cancelInteraction];
}

- (void)_singleTapCommited:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);

    if (![self isFirstResponder])
        [self becomeFirstResponder];

    if (_webSelectionAssistant && ![_webSelectionAssistant shouldHandleSingleTapAtPoint:gestureRecognizer.location]) {
        [self _singleTapDidReset:gestureRecognizer];
        return;
    }

    ASSERT(_potentialTapInProgress);

    // We don't want to clear the selection if it is in editable content.
    // The selection could have been set by autofocusing on page load and not
    // reflected in the UI process since the user was not interacting with the page.
    UITouch *touch = [gestureRecognizer.touches lastObject];
    if (!_page->editorState().isContentEditable && touch.type == UITouchTypeDirect)
        [_webSelectionAssistant clearSelection];

    _lastInteractionLocation = gestureRecognizer.location;

    [self _endPotentialTapAndEnableDoubleTapGesturesIfNecessary];

    if (_hasTapHighlightForPotentialTap) {
        [self _showTapHighlight];
        _hasTapHighlightForPotentialTap = NO;
    }

    _page->commitPotentialTap();

    if (!_isExpectingFastSingleTapCommit)
        [self _finishInteraction];
}

- (void)_doubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    _lastInteractionLocation = gestureRecognizer.location;

    _smartMagnificationController->handleSmartMagnificationGesture(gestureRecognizer.location);
}

- (void)_twoFingerDoubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    _lastInteractionLocation = gestureRecognizer.location;

    _smartMagnificationController->handleResetMagnificationGesture(gestureRecognizer.location);
}

- (void)_attemptClickAtLocation:(CGPoint)location
{
    if (![self isFirstResponder])
        [self becomeFirstResponder];

    _page->handleTap(location);
}

- (void)useSelectionAssistantWithMode:(UIWebSelectionMode)selectionMode
{
    if (selectionMode == UIWebSelectionModeWeb) {
        if (_textSelectionAssistant) {
            [_textSelectionAssistant deactivateSelection];
            _textSelectionAssistant = nil;
        }
        if (!_webSelectionAssistant)
            _webSelectionAssistant = adoptNS([[UIWKSelectionAssistant alloc] initWithView:self]);
    } else if (selectionMode == UIWebSelectionModeTextOnly) {
        if (_webSelectionAssistant)
            _webSelectionAssistant = nil;

        if (!_textSelectionAssistant)
            _textSelectionAssistant = adoptNS([[UIWKTextInteractionAssistant alloc] initWithView:self]);
        else {
            // Reset the gesture recognizers in case editibility has changed.
            [_textSelectionAssistant setGestureRecognizers];
        }

        if (self.isFirstResponder)
            [_textSelectionAssistant activateSelection];
    }
}

- (void)clearSelection
{
    _page->clearSelection();
}

- (void)_didNotHandleTapAsClick:(const WebCore::IntPoint&)point
{
    // FIXME: we should also take into account whether or not the UI delegate
    // has handled this notification.
    if (_hasValidPositionInformation && point == _positionInformation.point && _positionInformation.isDataDetectorLink)
        [self _showDataDetectorsSheet];
}

- (void)_positionInformationDidChange:(const InteractionInformationAtPosition&)info
{
    _positionInformation = info;
    _hasValidPositionInformation = YES;
    if (_actionSheetAssistant)
        [_actionSheetAssistant updateSheetPosition];
}

- (void)_willStartScrollingOrZooming
{
    [_webSelectionAssistant willStartScrollingOrZoomingPage];
    [_textSelectionAssistant willStartScrollingOverflow];
}

- (void)scrollViewWillStartPanOrPinchGesture
{
    _canSendTouchEventsAsynchronously = YES;
}

- (void)_didEndScrollingOrZooming
{
    [_webSelectionAssistant didEndScrollingOrZoomingPage];
    [_textSelectionAssistant didEndScrollingOverflow];
}

- (BOOL)requiresAccessoryView
{
    switch (_assistedNodeInformation.elementType) {
    case InputType::None:
        return NO;
    case InputType::Text:
    case InputType::Password:
    case InputType::Search:
    case InputType::Email:
    case InputType::URL:
    case InputType::Phone:
    case InputType::Number:
    case InputType::NumberPad:
        return !UICurrentUserInterfaceIdiomIsPad();
    case InputType::ContentEditable:
    case InputType::TextArea:
        return !UICurrentUserInterfaceIdiomIsPad();
    case InputType::Select:
    case InputType::Date:
    case InputType::DateTime:
    case InputType::DateTimeLocal:
    case InputType::Month:
    case InputType::Week:
    case InputType::Time:
        return !UICurrentUserInterfaceIdiomIsPad();
    }
}

- (void)_ensureFormAccessoryView
{
    if (_formAccessoryView)
        return;

    _formAccessoryView = adoptNS([[UIWebFormAccessory alloc] initWithInputAssistantItem:self.inputAssistantItem]);
    [_formAccessoryView setDelegate:self];
}

- (UIView *)inputAccessoryView
{
    if (![self requiresAccessoryView])
        return nil;

    return self.formAccessoryView;
}

- (NSArray *)supportedPasteboardTypesForCurrentSelection
{
    if (_page->editorState().selectionIsNone)
        return nil;
    
    static NSMutableArray *richTypes = nil;
    static NSMutableArray *plainTextTypes = nil;
    if (!plainTextTypes) {
        plainTextTypes = [[NSMutableArray alloc] init];
        [plainTextTypes addObject:(id)kUTTypeURL];
        [plainTextTypes addObjectsFromArray:UIPasteboardTypeListString];

        richTypes = [[NSMutableArray alloc] init];
        [richTypes addObject:WebArchivePboardType];
        [richTypes addObjectsFromArray:UIPasteboardTypeListImage];
        [richTypes addObjectsFromArray:plainTextTypes];
    }

    return (_page->editorState().isContentRichlyEditable) ? richTypes : plainTextTypes;
}

- (void)_lookup:(CGPoint)point
{
    RetainPtr<WKContentView> view = self;
    _page->getLookupContextAtPoint(WebCore::IntPoint(point), [view](const String& string, CallbackBase::Error error) {
        if (error != CallbackBase::Error::None)
            return;
        if (!string)
            return;
        
        CGRect presentationRect = view->_page->editorState().selectionIsRange ? view->_page->editorState().postLayoutData().selectionRects[0].rect() : view->_page->editorState().postLayoutData().caretRectAtStart;
        
        if (view->_textSelectionAssistant && [view->_textSelectionAssistant respondsToSelector:@selector(lookup:fromRect:)])
            [view->_textSelectionAssistant lookup:string fromRect:presentationRect];
        else if (view->_webSelectionAssistant && [view->_webSelectionAssistant respondsToSelector:@selector(lookup:fromRect:)])
            [view->_webSelectionAssistant lookup:string fromRect:presentationRect];
    });
}

- (void)_share:(id)sender
{
    RetainPtr<WKContentView> view = self;
    _page->getSelectionOrContentsAsString([view](const String& string, CallbackBase::Error error) {
        if (error != CallbackBase::Error::None)
            return;
        if (!string)
            return;

        CGRect presentationRect = view->_page->editorState().postLayoutData().selectionRects[0].rect();

        if (view->_textSelectionAssistant && [view->_textSelectionAssistant respondsToSelector:@selector(showShareSheetFor:fromRect:)])
            [view->_textSelectionAssistant showShareSheetFor:string fromRect:presentationRect];
        else if (view->_webSelectionAssistant && [view->_webSelectionAssistant respondsToSelector:@selector(showShareSheetFor:fromRect:)])
            [view->_webSelectionAssistant showShareSheetFor:string fromRect:presentationRect];
    });
}

- (void)_addShortcut:(id)sender
{
    if (_textSelectionAssistant && [_textSelectionAssistant respondsToSelector:@selector(showTextServiceFor:fromRect:)])
        [_textSelectionAssistant showTextServiceFor:[self selectedText] fromRect:_page->editorState().postLayoutData().selectionRects[0].rect()];
    else if (_webSelectionAssistant && [_webSelectionAssistant respondsToSelector:@selector(showTextServiceFor:fromRect:)])
        [_webSelectionAssistant showTextServiceFor:[self selectedText] fromRect:_page->editorState().postLayoutData().selectionRects[0].rect()];
}

- (NSString *)selectedText
{
    return (NSString *)_page->editorState().postLayoutData().wordAtSelection;
}

- (BOOL)isReplaceAllowed
{
    return _page->editorState().postLayoutData().isReplaceAllowed;
}

- (void)replaceText:(NSString *)text withText:(NSString *)word
{
    _page->replaceSelectedText(text, word);
}

- (void)selectWordBackward
{
    _page->selectWordBackward();
}

- (void)_promptForReplace:(id)sender
{
    const auto& wordAtSelection = _page->editorState().postLayoutData().wordAtSelection;
    if (wordAtSelection.isEmpty())
        return;

    if ([_textSelectionAssistant respondsToSelector:@selector(scheduleReplacementsForText:)])
        [_textSelectionAssistant scheduleReplacementsForText:wordAtSelection];
}

- (void)_transliterateChinese:(id)sender
{
    if ([_textSelectionAssistant respondsToSelector:@selector(scheduleChineseTransliterationForText:)])
        [_textSelectionAssistant scheduleChineseTransliterationForText:_page->editorState().postLayoutData().wordAtSelection];
}

- (void)_reanalyze:(id)sender
{
    [_textSelectionAssistant scheduleReanalysis];
}

- (void)replace:(id)sender
{
    [[UIKeyboardImpl sharedInstance] replaceText:sender];
}

- (NSDictionary *)textStylingAtPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
    if (!position || !_page->editorState().isContentRichlyEditable)
        return nil;

    NSMutableDictionary* result = [NSMutableDictionary dictionary];

    auto typingAttributes = _page->editorState().postLayoutData().typingAttributes;
    CTFontSymbolicTraits symbolicTraits = 0;
    if (typingAttributes & AttributeBold)
        symbolicTraits |= kCTFontBoldTrait;
    if (typingAttributes & AttributeItalics)
        symbolicTraits |= kCTFontTraitItalic;

    // We chose a random font family and size.
    // What matters are the traits but the caller expects a font object
    // in the dictionary for NSFontAttributeName.
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontDescriptorCreateWithNameAndSize(CFSTR("Helvetica"), 10));
    if (symbolicTraits)
        fontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithSymbolicTraits(fontDescriptor.get(), symbolicTraits, symbolicTraits));
    
    RetainPtr<CTFontRef> font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 10, nullptr));
    if (font)
        [result setObject:(id)font.get() forKey:NSFontAttributeName];
    
    if (typingAttributes & AttributeUnderline)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];

    return result;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender
{
    BOOL hasWebSelection = _webSelectionAssistant && !CGRectIsEmpty(_webSelectionAssistant.get().selectionFrame);

    if (action == @selector(_arrowKey:))
        return [self isFirstResponder];
        
    if (action == @selector(_showTextStyleOptions:))
        return _page->editorState().isContentRichlyEditable && _page->editorState().selectionIsRange && !_showingTextStyleOptions;
    if (_showingTextStyleOptions)
        return (action == @selector(toggleBoldface:) || action == @selector(toggleItalics:) || action == @selector(toggleUnderline:));
    if (action == @selector(toggleBoldface:) || action == @selector(toggleItalics:) || action == @selector(toggleUnderline:))
        return _page->editorState().isContentRichlyEditable;
    if (action == @selector(cut:))
        return !_page->editorState().isInPasswordField && _page->editorState().isContentEditable && _page->editorState().selectionIsRange;
    
    if (action == @selector(paste:)) {
        if (_page->editorState().selectionIsNone || !_page->editorState().isContentEditable)
            return NO;
        UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
        NSArray *types = [self supportedPasteboardTypesForCurrentSelection];
        NSIndexSet *indices = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, [pasteboard numberOfItems])];
        return [pasteboard containsPasteboardTypes:types inItemSet:indices];
    }

    if (action == @selector(copy:)) {
        if (_page->editorState().isInPasswordField)
            return NO;
        return hasWebSelection || _page->editorState().selectionIsRange;
    }

    if (action == @selector(_define:)) {
        if (_page->editorState().isInPasswordField || !(hasWebSelection || _page->editorState().selectionIsRange))
            return NO;

        NSUInteger textLength = _page->editorState().postLayoutData().selectedTextLength;
        // FIXME: We should be calling UIReferenceLibraryViewController to check if the length is
        // acceptable, but the interface takes a string.
        // <rdar://problem/15254406>
        if (!textLength || textLength > 200)
            return NO;

        if ([[getMCProfileConnectionClass() sharedConnection] effectiveBoolValueForSetting:MCFeatureDefinitionLookupAllowed] == MCRestrictedBoolExplicitNo)
            return NO;
            
        return YES;
    }

    if (action == @selector(_lookup:)) {
        if (_page->editorState().isInPasswordField)
            return NO;
        
        if ([[getMCProfileConnectionClass() sharedConnection] effectiveBoolValueForSetting:MCFeatureDefinitionLookupAllowed] == MCRestrictedBoolExplicitNo)
            return NO;
        
        return YES;
    }

    if (action == @selector(_share:)) {
        if (_page->editorState().isInPasswordField || !(hasWebSelection || _page->editorState().selectionIsRange))
            return NO;

        NSUInteger textLength = _page->editorState().postLayoutData().selectedTextLength;
        // See FIXME above for _define.
        if (!textLength || textLength > 200)
            return NO;
        
        return YES;
    }

    if (action == @selector(_addShortcut:)) {
        if (_page->editorState().isInPasswordField || !(hasWebSelection || _page->editorState().selectionIsRange))
            return NO;

        NSString *selectedText = [self selectedText];
        if (![selectedText length])
            return NO;

        if (!UIKeyboardEnabledInputModesAllowOneToManyShortcuts())
            return NO;
        if (![selectedText _containsCJScripts])
            return NO;
        return YES;
    }

    if (action == @selector(_promptForReplace:)) {
        if (!_page->editorState().selectionIsRange || !_page->editorState().postLayoutData().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        if ([[self selectedText] _containsCJScriptsOnly])
            return NO;
        return YES;
    }

    if (action == @selector(_transliterateChinese:)) {
        if (!_page->editorState().selectionIsRange || !_page->editorState().postLayoutData().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        return UIKeyboardEnabledInputModesAllowChineseTransliterationForText([self selectedText]);
    }

    if (action == @selector(_reanalyze:)) {
        if (!_page->editorState().selectionIsRange || !_page->editorState().postLayoutData().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        return UIKeyboardCurrentInputModeAllowsChineseOrJapaneseReanalysisForText([self selectedText]);
    }

    if (action == @selector(select:)) {
        // Disable select in password fields so that you can't see word boundaries.
        return !_page->editorState().isInPasswordField && [self hasContent] && !_page->editorState().selectionIsNone && !_page->editorState().selectionIsRange;
    }

    if (action == @selector(selectAll:)) {
        if (_page->editorState().selectionIsNone || ![self hasContent])
            return NO;
        if (!_page->editorState().selectionIsRange)
            return YES;
        // Enable selectAll for non-editable text, where the user can't access
        // this command via long-press to get a caret.
        if (_page->editorState().isContentEditable)
            return NO;
        // Don't attempt selectAll with general web content.
        if (hasWebSelection)
            return NO;
        // FIXME: Only enable if the selection doesn't already span the entire document.
        return YES;
    }

    if (action == @selector(replace:))
        return _page->editorState().isContentEditable && !_page->editorState().isInPasswordField;

    return [super canPerformAction:action withSender:sender];
}

- (void)_resetShowingTextStyle:(NSNotification *)notification
{
    _showingTextStyleOptions = NO;
    [_textSelectionAssistant hideTextStyleOptions];
}

- (void)copy:(id)sender
{
    _page->executeEditCommand(ASCIILiteral("copy"));
}

- (void)cut:(id)sender
{
    _page->executeEditCommand(ASCIILiteral("cut"));
}

- (void)paste:(id)sender
{
    _page->executeEditCommand(ASCIILiteral("paste"));
}

- (void)select:(id)sender
{
    [_textSelectionAssistant selectWord];
    // We cannot use selectWord command, because we want to be able to select the word even when it is the last in the paragraph.
    _page->extendSelection(WordGranularity);
}

- (void)selectAll:(id)sender
{
    [_textSelectionAssistant selectAll:sender];
    _page->executeEditCommand(ASCIILiteral("selectAll"));
}

- (void)toggleBoldface:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    [self executeEditCommandWithCallback:@"toggleBold"];
}

- (void)toggleItalics:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    [self executeEditCommandWithCallback:@"toggleItalic"];
}

- (void)toggleUnderline:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    [self executeEditCommandWithCallback:@"toggleUnderline"];
}

- (void)_showTextStyleOptions:(id)sender
{
    _showingTextStyleOptions = YES;
    [_textSelectionAssistant showTextStyleOptions];
}

- (void)_showDictionary:(NSString *)text
{
    CGRect presentationRect = _page->editorState().postLayoutData().selectionRects[0].rect();
    if (_textSelectionAssistant)
        [_textSelectionAssistant showDictionaryFor:text fromRect:presentationRect];
    else
        [_webSelectionAssistant showDictionaryFor:text fromRect:presentationRect];
}

- (void)_define:(id)sender
{
    if ([[getMCProfileConnectionClass() sharedConnection] effectiveBoolValueForSetting:MCFeatureDefinitionLookupAllowed] == MCRestrictedBoolExplicitNo)
        return;

    RetainPtr<WKContentView> view = self;
    _page->getSelectionOrContentsAsString([view](const String& string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;
        if (!string)
            return;

        [view _showDictionary:string];
    });
}

- (void)accessibilityRetrieveSpeakSelectionContent
{
    RetainPtr<WKContentView> view = self;
    _page->getSelectionOrContentsAsString([view](const String& string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;
        if ([view respondsToSelector:@selector(accessibilitySpeakSelectionSetContent:)])
            [view accessibilitySpeakSelectionSetContent:string];
    });
}

// UIWKInteractionViewProtocol

static inline GestureType toGestureType(UIWKGestureType gestureType)
{
    switch (gestureType) {
    case UIWKGestureLoupe:
        return GestureType::Loupe;
    case UIWKGestureOneFingerTap:
        return GestureType::OneFingerTap;
    case UIWKGestureTapAndAHalf:
        return GestureType::TapAndAHalf;
    case UIWKGestureDoubleTap:
        return GestureType::DoubleTap;
    case UIWKGestureTapAndHalf:
        return GestureType::TapAndHalf;
    case UIWKGestureDoubleTapInUneditable:
        return GestureType::DoubleTapInUneditable;
    case UIWKGestureOneFingerTapInUneditable:
        return GestureType::OneFingerTapInUneditable;
    case UIWKGestureOneFingerTapSelectsAll:
        return GestureType::OneFingerTapSelectsAll;
    case UIWKGestureOneFingerDoubleTap:
        return GestureType::OneFingerDoubleTap;
    case UIWKGestureOneFingerTripleTap:
        return GestureType::OneFingerTripleTap;
    case UIWKGestureTwoFingerSingleTap:
        return GestureType::TwoFingerSingleTap;
    case UIWKGestureTwoFingerRangedSelectGesture:
        return GestureType::TwoFingerRangedSelectGesture;
    case UIWKGestureTapOnLinkWithGesture:
        return GestureType::TapOnLinkWithGesture;
    case UIWKGestureMakeWebSelection:
        return GestureType::MakeWebSelection;
    case UIWKGesturePhraseBoundary:
        return GestureType::PhraseBoundary;
    }
    ASSERT_NOT_REACHED();
    return GestureType::Loupe;
}

static inline UIWKGestureType toUIWKGestureType(GestureType gestureType)
{
    switch (gestureType) {
    case GestureType::Loupe:
        return UIWKGestureLoupe;
    case GestureType::OneFingerTap:
        return UIWKGestureOneFingerTap;
    case GestureType::TapAndAHalf:
        return UIWKGestureTapAndAHalf;
    case GestureType::DoubleTap:
        return UIWKGestureDoubleTap;
    case GestureType::TapAndHalf:
        return UIWKGestureTapAndHalf;
    case GestureType::DoubleTapInUneditable:
        return UIWKGestureDoubleTapInUneditable;
    case GestureType::OneFingerTapInUneditable:
        return UIWKGestureOneFingerTapInUneditable;
    case GestureType::OneFingerTapSelectsAll:
        return UIWKGestureOneFingerTapSelectsAll;
    case GestureType::OneFingerDoubleTap:
        return UIWKGestureOneFingerDoubleTap;
    case GestureType::OneFingerTripleTap:
        return UIWKGestureOneFingerTripleTap;
    case GestureType::TwoFingerSingleTap:
        return UIWKGestureTwoFingerSingleTap;
    case GestureType::TwoFingerRangedSelectGesture:
        return UIWKGestureTwoFingerRangedSelectGesture;
    case GestureType::TapOnLinkWithGesture:
        return UIWKGestureTapOnLinkWithGesture;
    case GestureType::MakeWebSelection:
        return UIWKGestureMakeWebSelection;
    case GestureType::PhraseBoundary:
        return UIWKGesturePhraseBoundary;
    }
}

static inline SelectionTouch toSelectionTouch(UIWKSelectionTouch touch)
{
    switch (touch) {
    case UIWKSelectionTouchStarted:
        return SelectionTouch::Started;
    case UIWKSelectionTouchMoved:
        return SelectionTouch::Moved;
    case UIWKSelectionTouchEnded:
        return SelectionTouch::Ended;
    case UIWKSelectionTouchEndedMovingForward:
        return SelectionTouch::EndedMovingForward;
    case UIWKSelectionTouchEndedMovingBackward:
        return SelectionTouch::EndedMovingBackward;
    case UIWKSelectionTouchEndedNotMoving:
        return SelectionTouch::EndedNotMoving;
    }
    ASSERT_NOT_REACHED();
    return SelectionTouch::Ended;
}

static inline UIWKSelectionTouch toUIWKSelectionTouch(SelectionTouch touch)
{
    switch (touch) {
    case SelectionTouch::Started:
        return UIWKSelectionTouchStarted;
    case SelectionTouch::Moved:
        return UIWKSelectionTouchMoved;
    case SelectionTouch::Ended:
        return UIWKSelectionTouchEnded;
    case SelectionTouch::EndedMovingForward:
        return UIWKSelectionTouchEndedMovingForward;
    case SelectionTouch::EndedMovingBackward:
        return UIWKSelectionTouchEndedMovingBackward;
    case SelectionTouch::EndedNotMoving:
        return UIWKSelectionTouchEndedNotMoving;
    }
}

static inline GestureRecognizerState toGestureRecognizerState(UIGestureRecognizerState state)
{
    switch (state) {
    case UIGestureRecognizerStatePossible:
        return GestureRecognizerState::Possible;
    case UIGestureRecognizerStateBegan:
        return GestureRecognizerState::Began;
    case UIGestureRecognizerStateChanged:
        return GestureRecognizerState::Changed;
    case UIGestureRecognizerStateCancelled:
        return GestureRecognizerState::Cancelled;
    case UIGestureRecognizerStateEnded:
        return GestureRecognizerState::Ended;
    case UIGestureRecognizerStateFailed:
        return GestureRecognizerState::Failed;
    }
}

static inline UIGestureRecognizerState toUIGestureRecognizerState(GestureRecognizerState state)
{
    switch (state) {
    case GestureRecognizerState::Possible:
        return UIGestureRecognizerStatePossible;
    case GestureRecognizerState::Began:
        return UIGestureRecognizerStateBegan;
    case GestureRecognizerState::Changed:
        return UIGestureRecognizerStateChanged;
    case GestureRecognizerState::Cancelled:
        return UIGestureRecognizerStateCancelled;
    case GestureRecognizerState::Ended:
        return UIGestureRecognizerStateEnded;
    case GestureRecognizerState::Failed:
        return UIGestureRecognizerStateFailed;
    }
}

static inline UIWKSelectionFlags toUIWKSelectionFlags(SelectionFlags flags)
{
    NSInteger uiFlags = UIWKNone;
    if (flags & WordIsNearTap)
        uiFlags |= UIWKWordIsNearTap;
    if (flags & IsBlockSelection)
        uiFlags |= UIWKIsBlockSelection;
    if (flags & PhraseBoundaryChanged)
        uiFlags |= UIWKPhraseBoundaryChanged;

    return static_cast<UIWKSelectionFlags>(uiFlags);
}

static inline SelectionHandlePosition toSelectionHandlePosition(UIWKHandlePosition position)
{
    switch (position) {
    case UIWKHandleTop:
        return SelectionHandlePosition::Top;
    case UIWKHandleRight:
        return SelectionHandlePosition::Right;
    case UIWKHandleBottom:
        return SelectionHandlePosition::Bottom;
    case UIWKHandleLeft:
        return SelectionHandlePosition::Left;
    }
}

static inline WebCore::TextGranularity toWKTextGranularity(UITextGranularity granularity)
{
    switch (granularity) {
    case UITextGranularityCharacter:
        return CharacterGranularity;
    case UITextGranularityWord:
        return WordGranularity;
    case UITextGranularitySentence:
        return SentenceGranularity;
    case UITextGranularityParagraph:
        return ParagraphGranularity;
    case UITextGranularityLine:
        return LineGranularity;
    case UITextGranularityDocument:
        return DocumentGranularity;
    }
}

static inline WebCore::SelectionDirection toWKSelectionDirection(UITextDirection direction)
{
    switch (direction) {
    case UITextLayoutDirectionDown:
    case UITextLayoutDirectionRight:
        return DirectionRight;
    case UITextLayoutDirectionUp:
    case UITextLayoutDirectionLeft:
        return DirectionLeft;
    default:
        // UITextDirection is not an enum, but we only want to accept values from UITextLayoutDirection.
        ASSERT_NOT_REACHED();
        return DirectionRight;
    }
}

static void selectionChangedWithGesture(WKContentView *view, const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, WebKit::CallbackBase::Error error)
{
    if (error != WebKit::CallbackBase::Error::None) {
        ASSERT_NOT_REACHED();
        return;
    }
    if ([view webSelectionAssistant])
        [(UIWKSelectionAssistant *)[view webSelectionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType((GestureType)gestureType) withState:toUIGestureRecognizerState(static_cast<GestureRecognizerState>(gestureState)) withFlags:(toUIWKSelectionFlags((SelectionFlags)flags))];
    else
        [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType((GestureType)gestureType) withState:toUIGestureRecognizerState(static_cast<GestureRecognizerState>(gestureState)) withFlags:(toUIWKSelectionFlags((SelectionFlags)flags))];
}

static void selectionChangedWithTouch(WKContentView *view, const WebCore::IntPoint& point, uint32_t touch, WebKit::CallbackBase::Error error)
{
    if (error != WebKit::CallbackBase::Error::None) {
        ASSERT_NOT_REACHED();
        return;
    }
    if ([view webSelectionAssistant])
        [(UIWKSelectionAssistant *)[view webSelectionAssistant] selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:toUIWKSelectionTouch((SelectionTouch)touch)];
    else
        [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:toUIWKSelectionTouch((SelectionTouch)touch)];
}

- (void)_didUpdateBlockSelectionWithTouch:(SelectionTouch)touch withFlags:(SelectionFlags)flags growThreshold:(CGFloat)growThreshold shrinkThreshold:(CGFloat)shrinkThreshold
{
    [_webSelectionAssistant blockSelectionChangedWithTouch:toUIWKSelectionTouch(touch) withFlags:toUIWKSelectionFlags(flags) growThreshold:growThreshold shrinkThreshold:shrinkThreshold];
    if (touch != SelectionTouch::Started && touch != SelectionTouch::Moved)
        _usingGestureForSelection = NO;
}

- (BOOL)_isInteractingWithAssistedNode
{
    return _textSelectionAssistant != nil;
}

- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state
{
    _usingGestureForSelection = YES;
    _page->selectWithGesture(WebCore::IntPoint(point), CharacterGranularity, static_cast<uint32_t>(toGestureType(gestureType)), static_cast<uint32_t>(toGestureRecognizerState(state)), [self _isInteractingWithAssistedNode], [self, state](const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, WebKit::CallbackBase::Error error) {
        selectionChangedWithGesture(self, point, gestureType, gestureState, flags, error);
        if (state == UIGestureRecognizerStateEnded || state == UIGestureRecognizerStateCancelled)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart
{
    _usingGestureForSelection = YES;
    _page->updateSelectionWithTouches(WebCore::IntPoint(point), static_cast<uint32_t>(toSelectionTouch(touch)), baseIsStart, [self, touch](const WebCore::IntPoint& point, uint32_t touch, WebKit::CallbackBase::Error error) {
        selectionChangedWithTouch(self, point, touch, error);
        if (touch != UIWKSelectionTouchStarted && touch != UIWKSelectionTouchMoved)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState
{
    _usingGestureForSelection = YES;
    _page->selectWithTwoTouches(WebCore::IntPoint(from), WebCore::IntPoint(to), static_cast<uint32_t>(toGestureType(gestureType)), static_cast<uint32_t>(toGestureRecognizerState(gestureState)), [self, gestureState](const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, WebKit::CallbackBase::Error error) {
        selectionChangedWithGesture(self, point, gestureType, gestureState, flags, error);
        if (gestureState == UIGestureRecognizerStateEnded || gestureState == UIGestureRecognizerStateCancelled)
            _usingGestureForSelection = NO;
    });
}

- (void)changeBlockSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch forHandle:(UIWKHandlePosition)handle
{
    _usingGestureForSelection = YES;
    _page->updateBlockSelectionWithTouch(WebCore::IntPoint(point), static_cast<uint32_t>(toSelectionTouch(touch)), static_cast<uint32_t>(toSelectionHandlePosition(handle)));
}

- (void)moveByOffset:(NSInteger)offset
{
    if (!offset)
        return;
    
    [self beginSelectionChange];
    RetainPtr<WKContentView> view = self;
    _page->moveSelectionByOffset(offset, [view](WebKit::CallbackBase::Error) {
        [view endSelectionChange];
    });
}

- (const WKAutoCorrectionData&)autocorrectionData
{
    return _autocorrectionData;
}

// The completion handler can pass nil if input does not match the actual text preceding the insertion point.
- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler
{
    if (!input || ![input length]) {
        completionHandler(nil);
        return;
    }

    RetainPtr<WKContentView> view = self;
    _autocorrectionData.autocorrectionHandler = [completionHandler copy];
    _page->requestAutocorrectionData(input, [view](const Vector<FloatRect>& rects, const String& fontName, double fontSize, uint64_t traits, WebKit::CallbackBase::Error) {
        CGRect firstRect = CGRectZero;
        CGRect lastRect = CGRectZero;
        if (rects.size()) {
            firstRect = rects[0];
            lastRect = rects[rects.size() - 1];
        }
        
        view->_autocorrectionData.fontName = fontName;
        view->_autocorrectionData.fontSize = fontSize;
        view->_autocorrectionData.fontTraits = traits;
        view->_autocorrectionData.textFirstRect = firstRect;
        view->_autocorrectionData.textLastRect = lastRect;

        view->_autocorrectionData.autocorrectionHandler(rects.size() ? [WKAutocorrectionRects autocorrectionRectsWithRects:firstRect lastRect:lastRect] : nil);
        [view->_autocorrectionData.autocorrectionHandler release];
        view->_autocorrectionData.autocorrectionHandler = nil;
    });
}

- (void)selectPositionAtPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    _usingGestureForSelection = YES;
    UIWKSelectionCompletionHandler selectionHandler = [completionHandler copy];
    RetainPtr<WKContentView> view = self;
    
    _page->selectPositionAtPoint(WebCore::IntPoint(point), [self _isInteractingWithAssistedNode], [view, selectionHandler](WebKit::CallbackBase::Error error) {
        selectionHandler();
        view->_usingGestureForSelection = NO;
        [selectionHandler release];
    });
}

- (void)selectPositionAtBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction fromPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    _usingGestureForSelection = YES;
    UIWKSelectionCompletionHandler selectionHandler = [completionHandler copy];
    RetainPtr<WKContentView> view = self;
    
    _page->selectPositionAtBoundaryWithDirection(WebCore::IntPoint(point), toWKTextGranularity(granularity), toWKSelectionDirection(direction), [self _isInteractingWithAssistedNode], [view, selectionHandler](WebKit::CallbackBase::Error error) {
        selectionHandler();
        view->_usingGestureForSelection = NO;
        [selectionHandler release];
    });
}

- (void)moveSelectionAtBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction completionHandler:(void (^)(void))completionHandler
{
    _usingGestureForSelection = YES;
    UIWKSelectionCompletionHandler selectionHandler = [completionHandler copy];
    RetainPtr<WKContentView> view = self;
    
    _page->moveSelectionAtBoundaryWithDirection(toWKTextGranularity(granularity), toWKSelectionDirection(direction), [view, selectionHandler](WebKit::CallbackBase::Error error) {
        selectionHandler();
        view->_usingGestureForSelection = NO;
        [selectionHandler release];
    });
}

- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    _usingGestureForSelection = YES;
    UIWKSelectionCompletionHandler selectionHandler = [completionHandler copy];
    RetainPtr<WKContentView> view = self;

    _page->selectTextWithGranularityAtPoint(WebCore::IntPoint(point), toWKTextGranularity(granularity), [self _isInteractingWithAssistedNode], [view, selectionHandler](WebKit::CallbackBase::Error error) {
        selectionHandler();
        view->_usingGestureForSelection = NO;
        [selectionHandler release];
    });
}

- (void)beginSelectionInDirection:(UITextDirection)direction completionHandler:(void (^)(BOOL endIsMoving))completionHandler
{
    UIWKSelectionWithDirectionCompletionHandler selectionHandler = [completionHandler copy];

    _page->beginSelectionInDirection(toWKSelectionDirection(direction), [selectionHandler](bool endIsMoving, WebKit::CallbackBase::Error error) {
        selectionHandler(endIsMoving);
        [selectionHandler release];
    });
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point completionHandler:(void (^)(BOOL endIsMoving))completionHandler
{
    UIWKSelectionWithDirectionCompletionHandler selectionHandler = [completionHandler copy];
    
    _page->updateSelectionWithExtentPoint(WebCore::IntPoint(point), [self _isInteractingWithAssistedNode], [selectionHandler](bool endIsMoving, WebKit::CallbackBase::Error error) {
        selectionHandler(endIsMoving);
        [selectionHandler release];
    });
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity completionHandler:(void (^)(BOOL selectionEndIsMoving))completionHandler
{
    UIWKSelectionWithDirectionCompletionHandler selectionHandler = [completionHandler copy];
    
    _page->updateSelectionWithExtentPointAndBoundary(WebCore::IntPoint(point), toWKTextGranularity(granularity), [self _isInteractingWithAssistedNode], [selectionHandler](bool endIsMoving, WebKit::CallbackBase::Error error) {
        selectionHandler(endIsMoving);
        [selectionHandler release];
    });
}

- (UTF32Char)_characterBeforeCaretSelection
{
    return _page->editorState().postLayoutData().characterBeforeSelection;
}

- (UTF32Char)_characterInRelationToCaretSelection:(int)amount
{
    switch (amount) {
    case 0:
        return _page->editorState().postLayoutData().characterAfterSelection;
    case -1:
        return _page->editorState().postLayoutData().characterBeforeSelection;
    case -2:
        return _page->editorState().postLayoutData().twoCharacterBeforeSelection;
    default:
        return 0;
    }
}

- (BOOL)_selectionAtDocumentStart
{
    return !_page->editorState().postLayoutData().characterBeforeSelection;
}

- (CGRect)textFirstRect
{
    return (_page->editorState().hasComposition) ? _page->editorState().firstMarkedRect : _autocorrectionData.textFirstRect;
}

- (CGRect)textLastRect
{
    return (_page->editorState().hasComposition) ? _page->editorState().lastMarkedRect : _autocorrectionData.textLastRect;
}

- (void)replaceDictatedText:(NSString*)oldText withText:(NSString *)newText
{
    _page->replaceDictatedText(oldText, newText);
}

- (void)requestDictationContext:(void (^)(NSString *selectedText, NSString *beforeText, NSString *afterText))completionHandler
{
    UIWKDictationContextHandler dictationHandler = [completionHandler copy];

    _page->requestDictationContext([dictationHandler](const String& selectedText, const String& beforeText, const String& afterText, WebKit::CallbackBase::Error) {
        dictationHandler(selectedText, beforeText, afterText);
        [dictationHandler release];
    });
}

// The completion handler should pass the rect of the correction text after replacing the input text, or nil if the replacement could not be performed.
- (void)applyAutocorrection:(NSString *)correction toString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForCorrection))completionHandler
{
    // FIXME: Remove the synchronous call when <rdar://problem/16207002> is fixed.
    const bool useSyncRequest = true;

    if (useSyncRequest) {
        completionHandler(_page->applyAutocorrection(correction, input) ? [WKAutocorrectionRects autocorrectionRectsWithRects:_autocorrectionData.textFirstRect lastRect:_autocorrectionData.textLastRect] : nil);
        return;
    }
    _autocorrectionData.autocorrectionHandler = [completionHandler copy];
    RetainPtr<WKContentView> view = self;
    _page->applyAutocorrection(correction, input, [view](const String& string, WebKit::CallbackBase::Error error) {
        view->_autocorrectionData.autocorrectionHandler(!string.isNull() ? [WKAutocorrectionRects autocorrectionRectsWithRects:view->_autocorrectionData.textFirstRect lastRect:view->_autocorrectionData.textLastRect] : nil);
        [view->_autocorrectionData.autocorrectionHandler release];
        view->_autocorrectionData.autocorrectionHandler = nil;
    });
}

- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler
{
    // FIXME: Remove the synchronous call when <rdar://problem/16207002> is fixed.
    const bool useSyncRequest = true;

    if (useSyncRequest) {
        String beforeText;
        String markedText;
        String selectedText;
        String afterText;
        uint64_t location;
        uint64_t length;
        _page->getAutocorrectionContext(beforeText, markedText, selectedText, afterText, location, length);
        completionHandler([WKAutocorrectionContext autocorrectionContextWithData:beforeText markedText:markedText selectedText:selectedText afterText:afterText selectedRangeInMarkedText:NSMakeRange(location, length)]);
    } else {
        _autocorrectionData.autocorrectionContextHandler = [completionHandler copy];
        RetainPtr<WKContentView> view = self;
        _page->requestAutocorrectionContext([view](const String& beforeText, const String& markedText, const String& selectedText, const String& afterText, uint64_t location, uint64_t length, WebKit::CallbackBase::Error) {
            view->_autocorrectionData.autocorrectionContextHandler([WKAutocorrectionContext autocorrectionContextWithData:beforeText markedText:markedText selectedText:selectedText afterText:afterText selectedRangeInMarkedText:NSMakeRange(location, length)]);
        });
    }
}

// UIWebFormAccessoryDelegate
- (void)accessoryDone
{
    [self resignFirstResponder];
}

- (NSArray *)keyCommands
{
    static NSArray* nonEditableKeyCommands = [@[
       [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:0 action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:0 action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:0 action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:0 action:@selector(_arrowKey:)],
       
       [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierCommand action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierCommand action:@selector(_arrowKey:)],
       
       [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierShift action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierShift action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierShift action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierShift action:@selector(_arrowKey:)],
       
       [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow modifierFlags:UIKeyModifierAlternate action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow modifierFlags:UIKeyModifierAlternate action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow modifierFlags:UIKeyModifierAlternate action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow modifierFlags:UIKeyModifierAlternate action:@selector(_arrowKey:)],
       
       [UIKeyCommand keyCommandWithInput:@" " modifierFlags:0 action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:@" " modifierFlags:UIKeyModifierShift action:@selector(_arrowKey:)],
       
       [UIKeyCommand keyCommandWithInput:UIKeyInputPageDown modifierFlags:0 action:@selector(_arrowKey:)],
       [UIKeyCommand keyCommandWithInput:UIKeyInputPageDown modifierFlags:0 action:@selector(_arrowKey:)],
    ] retain];

    static NSArray* editableKeyCommands = [@[
       [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:0 action:@selector(_nextAccessoryTab:)],
       [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:UIKeyModifierShift action:@selector(_prevAccessoryTab:)]
    ] retain];
    
    return (_page->editorState().isContentEditable) ? editableKeyCommands : nonEditableKeyCommands;
}

- (void)_arrowKey:(id)sender
{
    UIKeyCommand* command = sender;
    [self handleKeyEvent:command._triggeringEvent];
}

- (void)_nextAccessoryTab:(id)sender
{
    [self accessoryTab:YES];
}

- (void)_prevAccessoryTab:(id)sender
{
    [self accessoryTab:NO];
}

- (void)accessoryTab:(BOOL)isNext
{
    [_inputPeripheral endEditing];
    _inputPeripheral = nil;

    _didAccessoryTabInitiateFocus = YES; // Will be cleared in either -_displayFormNodeInputView or -cleanupInteraction.
    [self beginSelectionChange];
    RetainPtr<WKContentView> view = self;
    _page->focusNextAssistedNode(isNext, [view](WebKit::CallbackBase::Error) {
        [view endSelectionChange];
        [view reloadInputViews];
    });

}

- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler
{
    auto completionHandlerCopy = Block_copy(completionHandler);
    RetainPtr<WKContentView> view = self;
    _page->setInitialFocus(selectingForward, false, WebKit::WebKeyboardEvent(), [view, completionHandlerCopy](WebKit::CallbackBase::Error) {
        BOOL didBecomeFirstResponder = view->_assistedNodeInformation.elementType != InputType::None && [view becomeFirstResponder];
        completionHandlerCopy(didBecomeFirstResponder);
        Block_release(completionHandlerCopy);
    });
}

- (WebCore::Color)_tapHighlightColorForFastClick:(BOOL)forFastClick
{
    ASSERT(_showDebugTapHighlightsForFastClicking);
    return forFastClick ? WebCore::Color(0, 225, 0, 127) : WebCore::Color(225, 0, 0, 127);
}

- (void)_setDoubleTapGesturesEnabled:(BOOL)enabled
{
    if (enabled && ![_doubleTapGestureRecognizer isEnabled]) {
        // The first tap recognized after re-enabling double tap gestures will not wait for the
        // second tap before committing. To fix this, we use a new double tap gesture recognizer.
        [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];
        [_doubleTapGestureRecognizer setDelegate:nil];
        [self _createAndConfigureDoubleTapGestureRecognizer];
    }

    if (_showDebugTapHighlightsForFastClicking && !enabled)
        _tapHighlightInformation.color = [self _tapHighlightColorForFastClick:YES];

    [_doubleTapGestureRecognizer setEnabled:enabled];
}

- (void)accessoryAutoFill
{
    id <_WKInputDelegate> inputDelegate = [_webView _inputDelegate];
    if ([inputDelegate respondsToSelector:@selector(_webView:accessoryViewCustomButtonTappedInFormInputSession:)])
        [inputDelegate _webView:_webView accessoryViewCustomButtonTappedInFormInputSession:_formInputSession.get()];
}

- (void)accessoryClear
{
    _page->setAssistedNodeValue(String());
}

- (void)_updateAccessory
{
    [_formAccessoryView setNextEnabled:_assistedNodeInformation.hasNextNode];
    [_formAccessoryView setPreviousEnabled:_assistedNodeInformation.hasPreviousNode];

    if (UICurrentUserInterfaceIdiomIsPad())
        [_formAccessoryView setClearVisible:NO];
    else {
        switch (_assistedNodeInformation.elementType) {
        case InputType::Date:
        case InputType::Month:
        case InputType::DateTimeLocal:
        case InputType::Time:
            [_formAccessoryView setClearVisible:YES];
            break;
        default:
            [_formAccessoryView setClearVisible:NO];
            break;
        }
    }

    // FIXME: hide or show the AutoFill button as needed.
}

// Keyboard interaction
// UITextInput protocol implementation

- (void)beginSelectionChange
{
    [self.inputDelegate selectionWillChange:self];
}

- (void)endSelectionChange
{
    [self.inputDelegate selectionDidChange:self];
}

- (NSString *)textInRange:(UITextRange *)range
{
    return nil;
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text
{
}

- (UITextRange *)selectedTextRange
{
    if (_page->editorState().selectionIsNone)
        return nil;
    auto& postLayoutEditorStateData = _page->editorState().postLayoutData();
    FloatRect startRect = postLayoutEditorStateData.caretRectAtStart;
    FloatRect endRect = postLayoutEditorStateData.caretRectAtEnd;
    double inverseScale = [self inverseScale];
    // We want to keep the original caret width, while the height scales with
    // the content taking orientation into account.
    // We achieve this by scaling the width with the inverse
    // scale factor. This way, when it is converted from the content view
    // the width remains unchanged.
    if (startRect.width() < startRect.height())
        startRect.setWidth(startRect.width() * inverseScale);
    else
        startRect.setHeight(startRect.height() * inverseScale);
    if (endRect.width() < endRect.height()) {
        double delta = endRect.width();
        endRect.setWidth(endRect.width() * inverseScale);
        delta = endRect.width() - delta;
        endRect.move(delta, 0);
    } else {
        double delta = endRect.height();
        endRect.setHeight(endRect.height() * inverseScale);
        delta = endRect.height() - delta;
        endRect.move(0, delta);
    }
    return [WKTextRange textRangeWithState:_page->editorState().selectionIsNone
                                   isRange:_page->editorState().selectionIsRange
                                isEditable:_page->editorState().isContentEditable
                                 startRect:startRect
                                   endRect:endRect
                            selectionRects:[self webSelectionRects]
                        selectedTextLength:postLayoutEditorStateData.selectedTextLength];
}

- (CGRect)caretRectForPosition:(UITextPosition *)position
{
    return ((WKTextPosition *)position).positionRect;
}

- (NSArray *)selectionRectsForRange:(UITextRange *)range
{
    return [WKTextSelectionRect textSelectionRectsWithWebRects:((WKTextRange *)range).selectionRects];
}

- (void)setSelectedTextRange:(UITextRange *)range
{
}

- (BOOL)hasMarkedText
{
    return [_markedText length];
}

- (NSString *)markedText
{
    return _markedText.get();
}

- (UITextRange *)markedTextRange
{
    return nil;
}

- (NSDictionary *)markedTextStyle
{
    return nil;
}

- (void)setMarkedTextStyle:(NSDictionary *)styleDictionary
{
}

- (void)setMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange
{
    _markedText = markedText;
    _page->setCompositionAsync(markedText, Vector<WebCore::CompositionUnderline>(), selectedRange, EditingRange());
}

- (void)unmarkText
{
    _markedText = nil;
    _page->confirmCompositionAsync();
}

- (UITextPosition *)beginningOfDocument
{
    return nil;
}

- (UITextPosition *)endOfDocument
{
    return nil;
}

- (UITextRange *)textRangeFromPosition:(UITextPosition *)fromPosition toPosition:(UITextPosition *)toPosition
{
    return nil;
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position offset:(NSInteger)offset
{
    return nil;
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction offset:(NSInteger)offset
{
    return nil;
}

- (NSComparisonResult)comparePosition:(UITextPosition *)position toPosition:(UITextPosition *)other
{
    return NSOrderedSame;
}

- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition
{
    return 0;
}

- (id <UITextInputTokenizer>)tokenizer
{
    return nil;
}

- (UITextPosition *)positionWithinRange:(UITextRange *)range farthestInDirection:(UITextLayoutDirection)direction
{
    return nil;
}

- (UITextRange *)characterRangeByExtendingPosition:(UITextPosition *)position inDirection:(UITextLayoutDirection)direction
{
    return nil;
}

- (UITextWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
    return UITextWritingDirectionLeftToRight;
}

- (void)setBaseWritingDirection:(UITextWritingDirection)writingDirection forRange:(UITextRange *)range
{
}

- (CGRect)firstRectForRange:(UITextRange *)range
{
    return CGRectZero;
}

/* Hit testing. */
- (UITextPosition *)closestPositionToPoint:(CGPoint)point
{
    return nil;
}

- (UITextPosition *)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange *)range
{
    return nil;
}

- (UITextRange *)characterRangeAtPoint:(CGPoint)point
{
    return nil;
}

- (void)deleteBackward
{
    _page->executeEditCommand(ASCIILiteral("deleteBackward"));
}

// Inserts the given string, replacing any selected or marked text.
- (void)insertText:(NSString *)aStringValue
{
    _page->insertTextAsync(aStringValue, EditingRange());
}

- (BOOL)hasText
{
    return YES;
}

// end of UITextInput protocol implementation

static UITextAutocapitalizationType toUITextAutocapitalize(WebAutocapitalizeType webkitType)
{
    switch (webkitType) {
    case WebAutocapitalizeTypeDefault:
        return UITextAutocapitalizationTypeSentences;
    case WebAutocapitalizeTypeNone:
        return UITextAutocapitalizationTypeNone;
    case WebAutocapitalizeTypeWords:
        return UITextAutocapitalizationTypeWords;
    case WebAutocapitalizeTypeSentences:
        return UITextAutocapitalizationTypeSentences;
    case WebAutocapitalizeTypeAllCharacters:
        return UITextAutocapitalizationTypeAllCharacters;
    }

    return UITextAutocapitalizationTypeSentences;
}

// UITextInputPrivate protocol
// Direct access to the (private) UITextInputTraits object.
- (UITextInputTraits *)textInputTraits
{
    if (!_traits)
        _traits = adoptNS([[UITextInputTraits alloc] init]);

    [_traits setSecureTextEntry:_assistedNodeInformation.elementType == InputType::Password];
    [_traits setShortcutConversionType:_assistedNodeInformation.elementType == InputType::Password ? UITextShortcutConversionTypeNo : UITextShortcutConversionTypeDefault];

    if (!_assistedNodeInformation.formAction.isEmpty())
        [_traits setReturnKeyType:(_assistedNodeInformation.elementType == InputType::Search) ? UIReturnKeySearch : UIReturnKeyGo];

    if (_assistedNodeInformation.elementType == InputType::Password || _assistedNodeInformation.elementType == InputType::Email || _assistedNodeInformation.elementType == InputType::URL || _assistedNodeInformation.formAction.contains("login")) {
        [_traits setAutocapitalizationType:UITextAutocapitalizationTypeNone];
        [_traits setAutocorrectionType:UITextAutocorrectionTypeNo];
    } else {
        [_traits setAutocapitalizationType:toUITextAutocapitalize(_assistedNodeInformation.autocapitalizeType)];
        [_traits setAutocorrectionType:_assistedNodeInformation.isAutocorrect ? UITextAutocorrectionTypeYes : UITextAutocorrectionTypeNo];
    }

    switch (_assistedNodeInformation.elementType) {
    case InputType::Phone:
         [_traits setKeyboardType:UIKeyboardTypePhonePad];
         break;
    case InputType::URL:
         [_traits setKeyboardType:UIKeyboardTypeURL];
         break;
    case InputType::Email:
         [_traits setKeyboardType:UIKeyboardTypeEmailAddress];
          break;
    case InputType::Number:
         [_traits setKeyboardType:UIKeyboardTypeNumbersAndPunctuation];
         break;
    case InputType::NumberPad:
         [_traits setKeyboardType:UIKeyboardTypeNumberPad];
         break;
    default:
         [_traits setKeyboardType:UIKeyboardTypeDefault];
    }

    return _traits.get();
}

- (UITextInteractionAssistant *)interactionAssistant
{
    return _textSelectionAssistant.get();
}

- (UIWebSelectionAssistant *)webSelectionAssistant
{
    return _webSelectionAssistant.get();
}

- (id<UISelectionInteractionAssistant>)selectionInteractionAssistant
{
    if ([_webSelectionAssistant conformsToProtocol:@protocol(UISelectionInteractionAssistant)])
        return (id<UISelectionInteractionAssistant>)_webSelectionAssistant.get();
    return nil;
}

// NSRange support.  Would like to deprecate to the extent possible, although some support
// (i.e. selectionRange) has shipped as API.
- (NSRange)selectionRange
{
    return NSMakeRange(NSNotFound, 0);
}

- (CGRect)rectForNSRange:(NSRange)range
{
    return CGRectZero;
}

- (NSRange)_markedTextNSRange
{
    return NSMakeRange(NSNotFound, 0);
}

// DOM range support.
- (DOMRange *)selectedDOMRange
{
    return nil;
}

- (void)setSelectedDOMRange:(DOMRange *)range affinityDownstream:(BOOL)affinityDownstream
{
}

// Modify text without starting a new undo grouping.
- (void)replaceRangeWithTextWithoutClosingTyping:(UITextRange *)range replacementText:(NSString *)text
{
}

// Caret rect support.  Shouldn't be necessary, but firstRectForRange doesn't offer precisely
// the same functionality.
- (CGRect)rectContainingCaretSelection
{
    return CGRectZero;
}

// Web events.
- (BOOL)requiresKeyEvents
{
    return YES;
}

- (void)_handleKeyUIEvent:(::UIEvent *)event
{
    // We only want to handle key event from the hardware keyboard when we are
    // first responder and we are not interacting with editable content.
    if ([self isFirstResponder] && event._hidEvent && !_page->editorState().isContentEditable) {
        [self handleKeyEvent:event];
        return;
    }

    [super _handleKeyUIEvent:event];
}

- (void)handleKeyEvent:(::UIEvent *)event
{
    // WebCore has already seen the event, no need for custom processing.
    if (event == _uiEventBeingResent)
        return;

    WKWebEvent *webEvent = [[[WKWebEvent alloc] initWithKeyEventType:(event._isKeyDown) ? WebEventKeyDown : WebEventKeyUp
                                                           timeStamp:event.timestamp
                                                          characters:event._modifiedInput
                                         charactersIgnoringModifiers:event._unmodifiedInput
                                                           modifiers:event._modifierFlags
                                                         isRepeating:(event._inputFlags & kUIKeyboardInputRepeat)
                                                           withFlags:event._inputFlags
                                                             keyCode:0
                                                            isTabKey:[event._modifiedInput isEqualToString:@"\t"]
                                                        characterSet:WebEventCharacterSetUnicode] autorelease];
    webEvent.uiEvent = event;
    
    [self handleKeyWebEvent:webEvent];    
}

- (void)handleKeyWebEvent:(WebIOSEvent *)theEvent
{
    _page->handleKeyboardEvent(NativeWebKeyboardEvent(theEvent));
}

- (void)_didHandleKeyEvent:(WebIOSEvent *)event eventWasHandled:(BOOL)eventWasHandled
{
    if (event.type == WebEventKeyDown) {
        // FIXME: This is only for staging purposes.
        if ([[UIKeyboardImpl sharedInstance] respondsToSelector:@selector(didHandleWebKeyEvent:)])
            [[UIKeyboardImpl sharedInstance] didHandleWebKeyEvent:event];
        else
            [[UIKeyboardImpl sharedInstance] didHandleWebKeyEvent];
    }

    // If we aren't interacting with editable content, we still need to call [super _handleKeyUIEvent:]
    // so that keyboard repeat will work correctly. If we are interacting with editable content,
    // we already did so in _handleKeyUIEvent.
    if (eventWasHandled && _page->editorState().isContentEditable)
        return;

    if (![event isKindOfClass:[WKWebEvent class]])
        return;

    // Resending the event may destroy this WKContentView.
    RetainPtr<WKContentView> protector(self);

    // We keep here the event when resending it to the application to distinguish
    // the case of a new event from one that has been already sent to WebCore.
    ASSERT(!_uiEventBeingResent);
    _uiEventBeingResent = [(WKWebEvent *)event uiEvent];
    [super _handleKeyUIEvent:_uiEventBeingResent.get()];
    _uiEventBeingResent = nil;
}

- (Optional<FloatPoint>)_scrollOffsetForEvent:(WebIOSEvent *)event
{
    static const unsigned kWebSpaceKey = 0x20;

    if (_page->editorState().isContentEditable)
        return Nullopt;

    NSString *charactersIgnoringModifiers = event.charactersIgnoringModifiers;
    if (!charactersIgnoringModifiers.length)
        return Nullopt;

    enum ScrollingIncrement { Document, Page, Line };
    enum ScrollingDirection { Up, Down, Left, Right };

    auto computeOffset = ^(ScrollingIncrement increment, ScrollingDirection direction) {
        bool isHorizontal = (direction == Left || direction == Right);

        CGFloat scrollDistance = ^ CGFloat {
            switch (increment) {
            case Document:
                ASSERT(!isHorizontal);
                return self.bounds.size.height;
            case Page:
                ASSERT(!isHorizontal);
                return Scrollbar::pageStep(_page->unobscuredContentRect().height(), self.bounds.size.height);
            case Line:
                return Scrollbar::pixelsPerLineStep();
            }
            ASSERT_NOT_REACHED();
            return 0;
        }();

        if (direction == Up || direction == Left)
            scrollDistance = -scrollDistance;
        
        return (isHorizontal ? FloatPoint(scrollDistance, 0) : FloatPoint(0, scrollDistance));
    };

    if ([charactersIgnoringModifiers isEqualToString:UIKeyInputLeftArrow])
        return computeOffset(Line, Left);
    if ([charactersIgnoringModifiers isEqualToString:UIKeyInputRightArrow])
        return computeOffset(Line, Right);

    ScrollingIncrement incrementForVerticalArrowKey = Line;
    if (event.modifierFlags & WebEventFlagMaskAlternate)
        incrementForVerticalArrowKey = Page;
    else if (event.modifierFlags & WebEventFlagMaskCommand)
        incrementForVerticalArrowKey = Document;
    if ([charactersIgnoringModifiers isEqualToString:UIKeyInputUpArrow])
        return computeOffset(incrementForVerticalArrowKey, Up);
    if ([charactersIgnoringModifiers isEqualToString:UIKeyInputDownArrow])
        return computeOffset(incrementForVerticalArrowKey, Down);

    if ([charactersIgnoringModifiers isEqualToString:UIKeyInputPageDown])
        return computeOffset(Page, Down);
    if ([charactersIgnoringModifiers isEqualToString:UIKeyInputPageUp])
        return computeOffset(Page, Up);

    if ([charactersIgnoringModifiers characterAtIndex:0] == kWebSpaceKey)
        return computeOffset(Page, (event.modifierFlags & WebEventFlagMaskShift) ? Up : Down);

    return Nullopt;
}

- (BOOL)_interpretKeyEvent:(WebIOSEvent *)event isCharEvent:(BOOL)isCharEvent
{
    static const unsigned kWebEnterKey = 0x0003;
    static const unsigned kWebBackspaceKey = 0x0008;
    static const unsigned kWebReturnKey = 0x000D;
    static const unsigned kWebDeleteKey = 0x007F;
    static const unsigned kWebDeleteForwardKey = 0xF728;
    static const unsigned kWebSpaceKey = 0x20;

    BOOL contentEditable = _page->editorState().isContentEditable;

    if (!contentEditable && event.isTabKey)
        return NO;

    if (Optional<FloatPoint> scrollOffset = [self _scrollOffsetForEvent:event]) {
        [_webView _scrollByContentOffset:*scrollOffset];
        return YES;
    }

    UIKeyboardImpl *keyboard = [UIKeyboardImpl sharedInstance];
    NSString *characters = event.characters;
    
    if (!characters.length)
        return NO;

    switch ([characters characterAtIndex:0]) {
    case kWebBackspaceKey:
    case kWebDeleteKey:
        if (contentEditable) {
            // FIXME: remove deleteFromInput once UIKit adopts deleteFromInputWithFlags
            if ([keyboard respondsToSelector:@selector(deleteFromInputWithFlags:)])
                [keyboard deleteFromInputWithFlags:event.keyboardFlags];
            else
                [keyboard deleteFromInput];
            return YES;
        }
        break;

    case kWebSpaceKey:
        if (contentEditable && isCharEvent) {
            [keyboard addInputString:event.characters withFlags:event.keyboardFlags];
            return YES;
        }
        break;

    case kWebEnterKey:
    case kWebReturnKey:
        if (contentEditable && isCharEvent) {
            // Map \r from HW keyboard to \n to match the behavior of the soft keyboard.
            [keyboard addInputString:@"\n" withFlags:0];
            return YES;
        }
        break;

    case kWebDeleteForwardKey:
        _page->executeEditCommand(ASCIILiteral("deleteForward"));
        return YES;

    default:
        if (contentEditable && isCharEvent) {
            [keyboard addInputString:event.characters withFlags:event.keyboardFlags];
            return YES;
        }
        break;
    }

    return NO;
}

- (void)executeEditCommandWithCallback:(NSString *)commandName
{
    [self beginSelectionChange];
    RetainPtr<WKContentView> view = self;
    _page->executeEditCommand(commandName, [view](WebKit::CallbackBase::Error) {
        [view endSelectionChange];
    });
}

- (UITextInputArrowKeyHistory *)_moveUp:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveUpAndModifySelection" : @"moveUp"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveDown:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveDownAndModifySelection" : @"moveDown"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveLeft:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending? @"moveLeftAndModifySelection" : @"moveLeft"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveRight:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveRightAndModifySelection" : @"moveRight"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfWord:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveWordBackwardAndModifySelection" : @"moveWordBackward"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfParagraph:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveToBeginningOfParagraphAndModifySelection" : @"moveToBeginningOfParagraph"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfLine:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveToBeginningOfLineAndModifySelection" : @"moveToBeginningOfLine"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfDocument:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveToBeginningOfDocumentAndModifySelection" : @"moveToBeginningOfDocument"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfWord:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveWordForwardAndModifySelection" : @"moveWordForward"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfParagraph:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveToEndOfParagraphAndModifySelection" : @"moveToEndOfParagraph"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfLine:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveToEndOfLineAndModifySelection" : @"moveToEndOfLine"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfDocument:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:extending ? @"moveToEndOfDocumentAndModifySelection" : @"moveToEndOfDocument"];
    return nil;
}

// Sets a buffer to make room for autocorrection views
- (void)setBottomBufferHeight:(CGFloat)bottomBuffer
{
}

- (UIView *)automaticallySelectedOverlay
{
    return self;
}

- (UITextGranularity)selectionGranularity
{
    return UITextGranularityCharacter;
}

// Should return an array of NSDictionary objects that key/value paries for the final text, correction identifier and
// alternative selection counts using the keys defined at the top of this header.
- (NSArray *)metadataDictionariesForDictationResults
{
    return nil;
}

// Returns the dictation result boundaries from position so that text that was not dictated can be excluded from logging.
// If these are not implemented, no text will be logged.
- (UITextPosition *)previousUnperturbedDictationResultBoundaryFromPosition:(UITextPosition *)position
{
    return nil;
}

- (UITextPosition *)nextUnperturbedDictationResultBoundaryFromPosition:(UITextPosition *)position
{
    return nil;
}

// The can all be (and have been) trivially implemented in terms of UITextInput.  Deprecate and remove.
- (void)moveBackward:(unsigned)count
{
}

- (void)moveForward:(unsigned)count
{
}

- (unichar)characterBeforeCaretSelection
{
    return 0;
}

- (NSString *)wordContainingCaretSelection
{
    return nil;
}

- (DOMRange *)wordRangeContainingCaretSelection
{
    return nil;
}

- (void)setMarkedText:(NSString *)text
{
}

- (BOOL)hasContent
{
    return _page->editorState().postLayoutData().hasContent;
}

- (void)selectAll
{
}

- (UIColor *)textColorForCaretSelection
{
    return [UIColor blackColor];
}

- (UIFont *)fontForCaretSelection
{
    CGFloat zoomScale = 1.0;    // FIXME: retrieve the actual document scale factor.
    CGFloat scaledSize = _autocorrectionData.fontSize;
    if (CGFAbs(zoomScale - 1.0) > FLT_EPSILON)
        scaledSize *= zoomScale;
    return [UIFont fontWithFamilyName:_autocorrectionData.fontName traits:(UIFontTrait)_autocorrectionData.fontTraits size:scaledSize];
}

- (BOOL)hasSelection
{
    return NO;
}

- (BOOL)isPosition:(UITextPosition *)position atBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction
{
    return NO;
}

- (UITextPosition *)positionFromPosition:(UITextPosition *)position toBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction
{
    return nil;
}

- (BOOL)isPosition:(UITextPosition *)position withinTextUnit:(UITextGranularity)granularity inDirection:(UITextDirection)direction
{
    return NO;
}

- (UITextRange *)rangeEnclosingPosition:(UITextPosition *)position withGranularity:(UITextGranularity)granularity inDirection:(UITextDirection)direction
{
    return nil;
}

- (void)takeTraitsFrom:(UITextInputTraits *)traits
{
    [[self textInputTraits] takeTraitsFrom:traits];
}

// FIXME: I want to change the name of these functions, but I'm leaving it for now
// to make it easier to look up the corresponding functions in UIKit.

- (void)_startAssistingKeyboard
{
    [self useSelectionAssistantWithMode:UIWebSelectionModeTextOnly];
    [self reloadInputViews];
}

- (void)_stopAssistingKeyboard
{
    [self useSelectionAssistantWithMode:toUIWebSelectionMode([_webView _selectionGranularity])];
}

- (const AssistedNodeInformation&)assistedNodeInformation
{
    return _assistedNodeInformation;
}

- (Vector<OptionItem>&)assistedNodeSelectOptions
{
    return _assistedNodeInformation.selectOptions;
}

- (UIWebFormAccessory *)formAccessoryView
{
    [self _ensureFormAccessoryView];
    return _formAccessoryView.get();
}

static bool isAssistableInputType(InputType type)
{
    switch (type) {
    case InputType::ContentEditable:
    case InputType::Text:
    case InputType::Password:
    case InputType::TextArea:
    case InputType::Search:
    case InputType::Email:
    case InputType::URL:
    case InputType::Phone:
    case InputType::Number:
    case InputType::NumberPad:
    case InputType::Date:
    case InputType::DateTime:
    case InputType::DateTimeLocal:
    case InputType::Month:
    case InputType::Week:
    case InputType::Time:
    case InputType::Select:
        return true;

    case InputType::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

- (void)_startAssistingNode:(const AssistedNodeInformation&)information userIsInteracting:(BOOL)userIsInteracting blurPreviousNode:(BOOL)blurPreviousNode userObject:(NSObject <NSSecureCoding> *)userObject
{
    id <_WKInputDelegate> inputDelegate = [_webView _inputDelegate];
    RetainPtr<WKFocusedElementInfo> focusedElementInfo = adoptNS([[WKFocusedElementInfo alloc] initWithAssistedNodeInformation:information isUserInitiated:userIsInteracting]);
    BOOL shouldShowKeyboard;

    if ([inputDelegate respondsToSelector:@selector(_webView:focusShouldStartInputSession:)])
        shouldShowKeyboard = [inputDelegate _webView:_webView focusShouldStartInputSession:focusedElementInfo.get()];
    else {
        // The default behavior is to allow node assistance if the user is interacting or the keyboard is already active.
        shouldShowKeyboard = userIsInteracting || _textSelectionAssistant;
    }
    if (!shouldShowKeyboard)
        return;

    if (blurPreviousNode)
        [self _stopAssistingNode];

    if (!isAssistableInputType(information.elementType))
        return;

    // FIXME: We should remove this check when we manage to send StartAssistingNode from the WebProcess
    // only when it is truly time to show the keyboard.
    if (_assistedNodeInformation.elementType == information.elementType && _assistedNodeInformation.elementRect == information.elementRect)
        return;

    _isEditable = YES;
    _assistedNodeInformation = information;
    _inputPeripheral = nil;
    _traits = nil;
    if (![self isFirstResponder])
        [self becomeFirstResponder];

    [self reloadInputViews];
    
    switch (information.elementType) {
    case InputType::Select:
    case InputType::DateTimeLocal:
    case InputType::Time:
    case InputType::Month:
    case InputType::Date:
        break;
    default:
        [self _startAssistingKeyboard];
        break;
    }
    
    if (information.insideFixedPosition)
        [_webView _updateVisibleContentRects];
    
    [self _displayFormNodeInputView];

    // _inputPeripheral has been initialized in inputView called by reloadInputViews.
    [_inputPeripheral beginEditing];

    if ([inputDelegate respondsToSelector:@selector(_webView:didStartInputSession:)]) {
        _formInputSession = adoptNS([[WKFormInputSession alloc] initWithContentView:self focusedElementInfo:focusedElementInfo.get() userObject:userObject]);
        [inputDelegate _webView:_webView didStartInputSession:_formInputSession.get()];
    }
}

- (void)_stopAssistingNode
{
    [_formInputSession invalidate];
    _formInputSession = nil;
    _isEditable = NO;
    _assistedNodeInformation.elementType = InputType::None;
    [_inputPeripheral endEditing];
    _inputPeripheral = nil;

    [self _stopAssistingKeyboard];
    [_formAccessoryView hideAutoFillButton];
    [self reloadInputViews];
    [self _updateAccessory];
    // The name is misleading, but this actually clears the selection views and removes any selection.
    [_webSelectionAssistant resignedFirstResponder];
}

- (void)_selectionChanged
{
    _selectionNeedsUpdate = YES;
    // If we are changing the selection with a gesture there is no need
    // to wait to paint the selection.
    if (_usingGestureForSelection)
        [self _updateChangedSelection];
}

- (void)selectWordForReplacement
{
    _page->extendSelection(WordGranularity);
}

- (void)_updateChangedSelection
{
    [self _updateChangedSelection:NO];
}

- (void)_updateChangedSelection:(BOOL)force
{
    if (!_selectionNeedsUpdate)
        return;

    WKSelectionDrawingInfo selectionDrawingInfo(_page->editorState());
    if (!force && selectionDrawingInfo == _lastSelectionDrawingInfo)
        return;

    _lastSelectionDrawingInfo = selectionDrawingInfo;

    // FIXME: We need to figure out what to do if the selection is changed by Javascript.
    if (_textSelectionAssistant) {
        _markedText = (_page->editorState().hasComposition) ? _page->editorState().markedText : String();
        if (!_showingTextStyleOptions)
            [_textSelectionAssistant selectionChanged];
    } else if (!_page->editorState().isContentEditable)
        [_webSelectionAssistant selectionChanged];
    _selectionNeedsUpdate = NO;
    if (_shouldRestoreSelection) {
        [_webSelectionAssistant didEndScrollingOverflow];
        [_textSelectionAssistant didEndScrollingOverflow];
        _shouldRestoreSelection = NO;
    }
}

- (void)_showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const IntRect&)elementRect
{
    if (!_airPlayRoutePicker)
        _airPlayRoutePicker = adoptNS([[WKAirPlayRoutePicker alloc] initWithView:self]);
    [_airPlayRoutePicker show:hasVideo fromRect:elementRect];
}

- (void)_showRunOpenPanel:(WebOpenPanelParameters*)parameters resultListener:(WebOpenPanelResultListenerProxy*)listener
{
    ASSERT(!_fileUploadPanel);
    if (_fileUploadPanel)
        return;

    _fileUploadPanel = adoptNS([[WKFileUploadPanel alloc] initWithView:self]);
    [_fileUploadPanel setDelegate:self];
    [_fileUploadPanel presentWithParameters:parameters resultListener:listener];
}

- (void)fileUploadPanelDidDismiss:(WKFileUploadPanel *)fileUploadPanel
{
    ASSERT(_fileUploadPanel.get() == fileUploadPanel);

    [_fileUploadPanel setDelegate:nil];
    _fileUploadPanel = nil;
}

#pragma mark - Implementation of UIWebTouchEventsGestureRecognizerDelegate.

- (BOOL)shouldIgnoreWebTouch
{
    return NO;
}

- (BOOL)isAnyTouchOverActiveArea:(NSSet *)touches
{
    return YES;
}

#pragma mark - Implementation of WKActionSheetAssistantDelegate.

- (const WebKit::InteractionInformationAtPosition&)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    return _positionInformation;
}

- (void)updatePositionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    _hasValidPositionInformation = NO;
    _page->requestPositionInformation(_positionInformation.point);
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant performAction:(WebKit::SheetAction)action
{
    _page->performActionOnElement((uint32_t)action);
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant openElementAtLocation:(CGPoint)location
{
    [self _attemptClickAtLocation:location];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithURL:(NSURL *)url rect:(CGRect)boundingRect
{
    if (_textSelectionAssistant)
        [_textSelectionAssistant showShareSheetFor:userVisibleString(url) fromRect:boundingRect];
    else if (_webSelectionAssistant)
        [_webSelectionAssistant showShareSheetFor:userVisibleString(url) fromRect:boundingRect];
}

#if HAVE(APP_LINKS)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element
{
    return _page->uiClient().shouldIncludeAppLinkActionsForElement(element);
}
#endif

- (RetainPtr<NSArray>)actionSheetAssistant:(WKActionSheetAssistant *)assistant decideActionsForElement:(_WKActivatedElementInfo *)element defaultActions:(RetainPtr<NSArray>)defaultActions
{
    return _page->uiClient().actionsForElement(element, WTFMove(defaultActions));
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant willStartInteractionWithElement:(_WKActivatedElementInfo *)element
{
    _page->startInteractionWithElementAtPosition(_positionInformation.point);
}

- (void)actionSheetAssistantDidStopInteraction:(WKActionSheetAssistant *)assistant
{
    _page->stopInteraction();
}

- (NSDictionary *)dataDetectionContextForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    NSDictionary *context = nil;
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([uiDelegate respondsToSelector:@selector(_dataDetectionContextForWebView:)])
        context = [uiDelegate _dataDetectionContextForWebView:_webView];
    return context;
}

- (NSString *)selectedTextForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    return [self selectedText];
}

@end

#if HAVE(LINK_PREVIEW)

@implementation WKContentView (WKInteractionPreview)

- (void)_registerPreview
{
    if (!_webView.allowsLinkPreview)
        return;

    _previewItemController = adoptNS([[UIPreviewItemController alloc] initWithView:self]);
    [_previewItemController setDelegate:self];
    _previewGestureRecognizer = _previewItemController.get().presentationGestureRecognizer;
    if ([_previewItemController respondsToSelector:@selector(presentationSecondaryGestureRecognizer)])
        _previewSecondaryGestureRecognizer = _previewItemController.get().presentationSecondaryGestureRecognizer;
}

- (void)_unregisterPreview
{
    [_previewItemController setDelegate:nil];
    _previewGestureRecognizer = nil;
    _previewSecondaryGestureRecognizer = nil;
    _previewItemController = nil;
}

- (BOOL)_interactionShouldBeginFromPreviewItemController:(UIPreviewItemController *)controller forPosition:(CGPoint)position
{
    if (!_highlightLongPressCanClick)
        return NO;

    [self ensurePositionInformationIsUpToDate:position];
    if (!_positionInformation.isLink && !_positionInformation.isImage)
        return NO;
    
    String absoluteLinkURL = _positionInformation.url;
    if (_positionInformation.isLink) {
        NSURL *targetURL = [NSURL _web_URLWithWTFString:_positionInformation.url];
        id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
        if ([uiDelegate respondsToSelector:@selector(_webView:shouldPreviewElement:)]) {
            auto previewElementInfo = adoptNS([[_WKPreviewElementInfo alloc] _initWithLinkURL:targetURL]);
            return [uiDelegate _webView:_webView shouldPreviewElement:previewElementInfo.get()];
        }
        if (absoluteLinkURL.isEmpty())
            return NO;
        if (WebCore::protocolIsInHTTPFamily(absoluteLinkURL))
            return YES;
        if ([[getDDDetectionControllerClass() tapAndHoldSchemes] containsObject:[targetURL scheme]])
            return YES;
        return NO;
    }
    return YES;
}

- (NSDictionary *)_dataForPreviewItemController:(UIPreviewItemController *)controller atPosition:(CGPoint)position type:(UIPreviewItemType *)type
{
    *type = UIPreviewItemTypeNone;

    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    BOOL supportsImagePreview = [uiDelegate respondsToSelector:@selector(_webView:commitPreviewedImageWithURL:)];
    BOOL canShowImagePreview = _positionInformation.isImage && supportsImagePreview;
    BOOL canShowLinkPreview = _positionInformation.isLink || canShowImagePreview;
    BOOL useImageURLForLink = NO;

    if (canShowImagePreview && _positionInformation.isAnimatedImage) {
        canShowImagePreview = NO;
        canShowLinkPreview = YES;
        useImageURLForLink = YES;
    }

    if (!canShowLinkPreview && !canShowImagePreview)
        return nil;

    String absoluteLinkURL = _positionInformation.url;
    if (!useImageURLForLink && (absoluteLinkURL.isEmpty() || (!WebCore::protocolIsInHTTPFamily(absoluteLinkURL) && !_positionInformation.isDataDetectorLink))) {
        if (canShowLinkPreview && !canShowImagePreview)
            return nil;
        canShowLinkPreview = NO;
    }

    NSMutableDictionary *dataForPreview = [[[NSMutableDictionary alloc] init] autorelease];
    if (canShowLinkPreview) {
        *type = UIPreviewItemTypeLink;
        if (useImageURLForLink)
            dataForPreview[UIPreviewDataLink] = [NSURL _web_URLWithWTFString:_positionInformation.imageURL];
        else
            dataForPreview[UIPreviewDataLink] = [NSURL _web_URLWithWTFString:_positionInformation.url];
        if (_positionInformation.isDataDetectorLink) {
            NSDictionary *context = nil;
            id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
            if ([uiDelegate respondsToSelector:@selector(_dataDetectionContextForWebView:)])
                context = [uiDelegate _dataDetectionContextForWebView:_webView];

            DDDetectionController *controller = [getDDDetectionControllerClass() sharedController];
            if ([controller respondsToSelector:@selector(resultForURL:identifier:selectedText:results:context:extendedContext:)]) {
                NSDictionary *newContext = nil;
                DDResultRef ddResult = [controller resultForURL:dataForPreview[UIPreviewDataLink] identifier:_positionInformation.dataDetectorIdentifier selectedText:[self selectedText] results:_positionInformation.dataDetectorResults.get() context:context extendedContext:&newContext];
                if (ddResult)
                    dataForPreview[UIPreviewDataDDResult] = (__bridge id)ddResult;
                if (newContext)
                    dataForPreview[UIPreviewDataDDContext] = newContext;
            }
        }
    } else if (canShowImagePreview) {
        *type = UIPreviewItemTypeImage;
        dataForPreview[UIPreviewDataLink] = [NSURL _web_URLWithWTFString:_positionInformation.imageURL];
    }
    
    return dataForPreview;
}

- (CGRect)_presentationRectForPreviewItemController:(UIPreviewItemController *)controller
{
    return _positionInformation.bounds;
}

- (UIViewController *)_presentedViewControllerForPreviewItemController:(UIPreviewItemController *)controller
{
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);

    NSURL *targetURL = controller.previewData[UIPreviewDataLink];
    URL coreTargetURL = targetURL;
    bool isValidURLForImagePreview = !coreTargetURL.isEmpty() && (WebCore::protocolIsInHTTPFamily(coreTargetURL) || WebCore::protocolIs(coreTargetURL, "data"));

    if ([_previewItemController type] == UIPreviewItemTypeLink) {
        _highlightLongPressCanClick = NO;
        _page->startInteractionWithElementAtPosition(_positionInformation.point);

        // Treat animated images like a link preview
        if (isValidURLForImagePreview && _positionInformation.isAnimatedImage) {
            RetainPtr<_WKActivatedElementInfo> animatedImageElementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:targetURL location:_positionInformation.point title:_positionInformation.title rect:_positionInformation.bounds image:_positionInformation.image.get()]);

            if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForAnimatedImageAtURL:defaultActions:elementInfo:imageSize:)]) {
                RetainPtr<NSArray> actions = [_actionSheetAssistant defaultActionsForImageSheet:animatedImageElementInfo.get()];
                return [uiDelegate _webView:_webView previewViewControllerForAnimatedImageAtURL:targetURL defaultActions:actions.get() elementInfo:animatedImageElementInfo.get() imageSize:_positionInformation.image->size()];
            }
        }

        RetainPtr<_WKActivatedElementInfo> elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeLink URL:targetURL location:_positionInformation.point title:_positionInformation.title rect:_positionInformation.bounds image:_positionInformation.image.get()]);

        RetainPtr<NSArray> actions = [_actionSheetAssistant defaultActionsForLinkSheet:elementInfo.get()];
        if ([uiDelegate respondsToSelector:@selector(_webView:previewingViewControllerForElement:defaultActions:)]) {
            auto previewElementInfo = adoptNS([[_WKPreviewElementInfo alloc] _initWithLinkURL:targetURL]);
            if (UIViewController *controller = [uiDelegate _webView:_webView previewingViewControllerForElement:previewElementInfo.get() defaultActions:actions.get()])
                return controller;
        }

        if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForURL:defaultActions:elementInfo:)])
            return [uiDelegate _webView:_webView previewViewControllerForURL:targetURL defaultActions:actions.get() elementInfo:elementInfo.get()];

        if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForURL:)])
            return [uiDelegate _webView:_webView previewViewControllerForURL:targetURL];
        return nil;
    }

    if ([_previewItemController type] == UIPreviewItemTypeImage) {
        if (!isValidURLForImagePreview)
            return nil;

        RetainPtr<_WKActivatedElementInfo> elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:targetURL location:_positionInformation.point title:_positionInformation.title rect:_positionInformation.bounds image:_positionInformation.image.get()]);
        _page->startInteractionWithElementAtPosition(_positionInformation.point);

        if ([uiDelegate respondsToSelector:@selector(_webView:willPreviewImageWithURL:)])
            [uiDelegate _webView:_webView willPreviewImageWithURL:targetURL];
        return [[[WKImagePreviewViewController alloc] initWithCGImage:_positionInformation.image->makeCGImageCopy() defaultActions:[_actionSheetAssistant defaultActionsForImageSheet:elementInfo.get()] elementInfo:elementInfo] autorelease];
    }

    return nil;
}

- (void)_previewItemController:(UIPreviewItemController *)controller commitPreview:(UIViewController *)viewController
{
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([_previewItemController type] == UIPreviewItemTypeImage) {
        if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewedImageWithURL:)]) {
            String absoluteImageURL = _positionInformation.imageURL;
            if (absoluteImageURL.isEmpty() || !(WebCore::protocolIsInHTTPFamily(absoluteImageURL) || WebCore::protocolIs(absoluteImageURL, "data")))
                return;
            [uiDelegate _webView:_webView commitPreviewedImageWithURL:[NSURL _web_URLWithWTFString:absoluteImageURL]];
            return;
        }
        return;
    }

    if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewingViewController:)]) {
        [uiDelegate _webView:_webView commitPreviewingViewController:viewController];
        return;
    }

    if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewedViewController:)]) {
        [uiDelegate _webView:_webView commitPreviewedViewController:viewController];
        return;
    }

}

- (void)_interactionStartedFromPreviewItemController:(UIPreviewItemController *)controller
{
    [self _removeDefaultGestureRecognizers];

    [self _cancelInteraction];
}

- (void)_interactionStoppedFromPreviewItemController:(UIPreviewItemController *)controller
{
    [self _addDefaultGestureRecognizers];

    if (![_actionSheetAssistant isShowingSheet])
        _page->stopInteraction();
}

- (void)_previewItemController:(UIPreviewItemController *)controller didDismissPreview:(UIViewController *)viewController committing:(BOOL)committing
{
    id<WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([uiDelegate respondsToSelector:@selector(_webView:didDismissPreviewViewController:committing:)])
        [uiDelegate _webView:_webView didDismissPreviewViewController:viewController committing:committing];
    else if ([uiDelegate respondsToSelector:@selector(_webView:didDismissPreviewViewController:)])
        [uiDelegate _webView:_webView didDismissPreviewViewController:viewController];
}

- (UIImage *)_presentationSnapshotForPreviewItemController:(UIPreviewItemController *)controller
{
    if (!_positionInformation.linkIndicator.contentImage)
        return nullptr;
    return [[[UIImage alloc] initWithCGImage:_positionInformation.linkIndicator.contentImage->getCGImageRef()] autorelease];
}

- (NSArray *)_presentationRectsForPreviewItemController:(UIPreviewItemController *)controller
{
    RetainPtr<NSMutableArray> rectArray = adoptNS([[NSMutableArray alloc] init]);

    if (_positionInformation.linkIndicator.contentImage) {
        FloatPoint origin = _positionInformation.linkIndicator.textBoundingRectInRootViewCoordinates.location();
        for (FloatRect& rect : _positionInformation.linkIndicator.textRectsInBoundingRectCoordinates) {
            CGRect cgRect = rect;
            cgRect.origin.x += origin.x();
            cgRect.origin.y += origin.y();
            [rectArray addObject:[NSValue valueWithCGRect:cgRect]];
        }
    } else {
        const float marginInPx = 4 * _page->deviceScaleFactor();
        CGRect cgRect = CGRectInset(_positionInformation.bounds, -marginInPx, -marginInPx);
        [rectArray addObject:[NSValue valueWithCGRect:cgRect]];
    }

    return rectArray.autorelease();
}

- (void)_previewItemControllerDidCancelPreview:(UIPreviewItemController *)controller
{
    _highlightLongPressCanClick = NO;
}

@end

#endif

// UITextRange, UITextPosition and UITextSelectionRect implementations for WK2

@implementation WKTextRange (UITextInputAdditions)

- (BOOL)_isCaret
{
    return self.empty;
}

- (BOOL)_isRanged
{
    return !self.empty;
}

@end

@implementation WKTextRange

+(WKTextRange *)textRangeWithState:(BOOL)isNone isRange:(BOOL)isRange isEditable:(BOOL)isEditable startRect:(CGRect)startRect endRect:(CGRect)endRect selectionRects:(NSArray *)selectionRects selectedTextLength:(NSUInteger)selectedTextLength
{
    WKTextRange *range = [[WKTextRange alloc] init];
    range.isNone = isNone;
    range.isRange = isRange;
    range.isEditable = isEditable;
    range.startRect = startRect;
    range.endRect = endRect;
    range.selectedTextLength = selectedTextLength;
    range.selectionRects = selectionRects;
    return [range autorelease];
}

- (void)dealloc
{
    [self.selectionRects release];
    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@(%p) - start:%@, end:%@", [self class], self, NSStringFromCGRect(self.startRect), NSStringFromCGRect(self.endRect)];
}

- (WKTextPosition *)start
{
    WKTextPosition *pos = [WKTextPosition textPositionWithRect:self.startRect];
    return pos;
}

- (UITextPosition *)end
{
    WKTextPosition *pos = [WKTextPosition textPositionWithRect:self.endRect];
    return pos;
}

- (BOOL)isEmpty
{
    return !self.isRange;
}

// FIXME: Overriding isEqual: without overriding hash will cause trouble if this ever goes into an NSSet or is the key in an NSDictionary,
// since two equal items could have different hashes.
- (BOOL)isEqual:(id)other
{
    if (![other isKindOfClass:[WKTextRange class]])
        return NO;

    WKTextRange *otherRange = (WKTextRange *)other;

    if (self == other)
        return YES;

    // FIXME: Probably incorrect for equality to ignore so much of the object state.
    // It ignores isNone, isEditable, selectedTextLength, and selectionRects.

    if (self.isRange) {
        if (!otherRange.isRange)
            return NO;
        return CGRectEqualToRect(self.startRect, otherRange.startRect) && CGRectEqualToRect(self.endRect, otherRange.endRect);
    } else {
        if (otherRange.isRange)
            return NO;
        // FIXME: Do we need to check isNone here?
        return CGRectEqualToRect(self.startRect, otherRange.startRect);
    }
}

@end

@implementation WKTextPosition

@synthesize positionRect = _positionRect;

+ (WKTextPosition *)textPositionWithRect:(CGRect)positionRect
{
    WKTextPosition *pos =[[WKTextPosition alloc] init];
    pos.positionRect = positionRect;
    return [pos autorelease];
}

// FIXME: Overriding isEqual: without overriding hash will cause trouble if this ever goes into a NSSet or is the key in an NSDictionary,
// since two equal items could have different hashes.
- (BOOL)isEqual:(id)other
{
    if (![other isKindOfClass:[WKTextPosition class]])
        return NO;

    return CGRectEqualToRect(self.positionRect, ((WKTextPosition *)other).positionRect);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<WKTextPosition: %p, {%@}>", self, NSStringFromCGRect(self.positionRect)];
}

@end

@implementation WKTextSelectionRect

- (id)initWithWebRect:(WebSelectionRect *)wRect
{
    self = [super init];
    if (self)
        self.webRect = wRect;

    return self;
}

- (void)dealloc
{
    self.webRect = nil;
    [super dealloc];
}

// FIXME: we are using this implementation for now
// that uses WebSelectionRect, but we want to provide our own
// based on WebCore::SelectionRect.

+ (NSArray *)textSelectionRectsWithWebRects:(NSArray *)webRects
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:webRects.count];
    for (WebSelectionRect *webRect in webRects) {
        RetainPtr<WKTextSelectionRect> rect = adoptNS([[WKTextSelectionRect alloc] initWithWebRect:webRect]);
        [array addObject:rect.get()];
    }
    return array;
}

- (CGRect)rect
{
    return _webRect.rect;
}

- (UITextWritingDirection)writingDirection
{
    return (UITextWritingDirection)_webRect.writingDirection;
}

- (UITextRange *)range
{
    return nil;
}

- (BOOL)containsStart
{
    return _webRect.containsStart;
}

- (BOOL)containsEnd
{
    return _webRect.containsEnd;
}

- (BOOL)isVertical
{
    return !_webRect.isHorizontal;
}

@end

@implementation WKAutocorrectionRects

+ (WKAutocorrectionRects *)autocorrectionRectsWithRects:(CGRect)firstRect lastRect:(CGRect)lastRect
{
    WKAutocorrectionRects *rects =[[WKAutocorrectionRects alloc] init];
    rects.firstRect = firstRect;
    rects.lastRect = lastRect;
    return [rects autorelease];
}

@end

@implementation WKAutocorrectionContext

+ (WKAutocorrectionContext *)autocorrectionContextWithData:(NSString *)beforeText markedText:(NSString *)markedText selectedText:(NSString *)selectedText afterText:(NSString *)afterText selectedRangeInMarkedText:(NSRange)range
{
    WKAutocorrectionContext *context = [[WKAutocorrectionContext alloc] init];

    if ([beforeText length])
        context.contextBeforeSelection = [beforeText copy];
    if ([selectedText length])
        context.selectedText = [selectedText copy];
    if ([markedText length])
        context.markedText = [markedText copy];
    if ([afterText length])
        context.contextAfterSelection = [afterText copy];
    context.rangeInMarkedText = range;
    return [context autorelease];
}

- (void)dealloc
{
    [self.contextBeforeSelection release];
    [self.markedText release];
    [self.selectedText release];
    [self.contextAfterSelection release];

    [super dealloc];
}

@end

#endif // PLATFORM(IOS)
