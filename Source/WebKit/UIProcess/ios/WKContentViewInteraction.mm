/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "APIUIClient.h"
#import "CompletionHandlerCallChecker.h"
#import "DocumentEditingContext.h"
#import "EditableImageController.h"
#import "InputViewUpdateDeferrer.h"
#import "InsertTextOptions.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebTouchEvent.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeViews.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "SmartMagnificationController.h"
#import "TextChecker.h"
#import "TextInputSPI.h"
#import "UIKitSPI.h"
#import "UserInterfaceIdiom.h"
#import "VersionChecks.h"
#import "WKActionSheetAssistant.h"
#import "WKContextMenuElementInfoInternal.h"
#import "WKContextMenuElementInfoPrivate.h"
#import "WKDatePickerViewController.h"
#import "WKDateTimeInputControl.h"
#import "WKDrawingCoordinator.h"
#import "WKError.h"
#import "WKFocusedFormControlView.h"
#import "WKFormSelectControl.h"
#import "WKFrameInfoInternal.h"
#import "WKHighlightLongPressGestureRecognizer.h"
#import "WKImagePreviewViewController.h"
#import "WKInspectorNodeSearchGestureRecognizer.h"
#import "WKMouseGestureRecognizer.h"
#import "WKNSURLExtras.h"
#import "WKPreviewActionItemIdentifiers.h"
#import "WKPreviewActionItemInternal.h"
#import "WKPreviewElementInfoInternal.h"
#import "WKQuickboardListViewController.h"
#import "WKSelectMenuListViewController.h"
#import "WKSyntheticFlagsChangedWebEvent.h"
#import "WKTextInputListViewController.h"
#import "WKTextPlaceholder.h"
#import "WKTextSelectionRect.h"
#import "WKTimePickerViewController.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewIOS.h"
#import "WKWebViewPrivate.h"
#import "WKWebViewPrivateForTestingIOS.h"
#import "WebAutocorrectionContext.h"
#import "WebAutocorrectionData.h"
#import "WebDataListSuggestionsDropdownIOS.h"
#import "WebEvent.h"
#import "WebIOSEventFactory.h"
#import "WebPageMessages.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKElementAction.h"
#import "_WKElementActionInternal.h"
#import "_WKFocusedElementInfo.h"
#import "_WKInputDelegate.h"
#import "_WKTextInputContextInternal.h"
#import <CoreText/CTFont.h>
#import <CoreText/CTFontDescriptor.h>
#import <MobileCoreServices/UTCoreTypes.h>
#import <WebCore/Color.h>
#import <WebCore/ColorIOS.h>
#import <WebCore/CompositionHighlight.h>
#import <WebCore/DOMPasteAccess.h>
#import <WebCore/DataDetection.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FontAttributeChanges.h>
#import <WebCore/InputMode.h>
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/Path.h>
#import <WebCore/PathUtilities.h>
#import <WebCore/PromisedAttachmentInfo.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/ShareData.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TouchAction.h>
#import <WebCore/UTIUtilities.h>
#import <WebCore/VisibleSelection.h>
#import <WebCore/WebEvent.h>
#import <WebCore/WritingDirection.h>
#import <WebKit/WebSelectionRect.h> // FIXME: WebKit should not include WebKitLegacy headers!
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/DataDetectorsCoreSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <pal/spi/ios/DataDetectorsUISPI.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/Optional.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/TextStream.h>

#if ENABLE(DRAG_SUPPORT)
#import <WebCore/DragData.h>
#import <WebCore/DragItem.h>
#import <WebCore/PlatformPasteboard.h>
#import <WebCore/WebItemProviderPasteboard.h>
#endif

#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
#import <UIKit/_UILookupGestureRecognizer.h>
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
#import "APIAttachment.h"
#endif

#if ENABLE(INPUT_TYPE_COLOR)
#import "WKFormColorControl.h"
#endif

#if !USE(UIKIT_KEYBOARD_ADDITIONS)
#import "WKWebEvent.h"
#endif

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WKPlatformFileUploadPanel.mm>)
#import <WebKitAdditions/WKPlatformFileUploadPanel.mm>
#endif

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WKContentViewInteractionAdditions.mm>
#endif

#import <pal/ios/ManagedConfigurationSoftLink.h>

#if HAVE(LINK_PREVIEW) && USE(UICONTEXTMENU)
static NSString * const webkitShowLinkPreviewsPreferenceKey = @"WebKitShowLinkPreviews";
#endif

#if HAVE(UI_CURSOR_INTERACTION)
static NSString * const cursorRegionIdentifier = @"WKCursorRegion";
static NSString * const editableCursorRegionIdentifier = @"WKEditableCursorRegion";

@interface WKContentView (WKUICursorInteractionDelegate) <_UICursorInteractionDelegate>
@end
#endif

#if PLATFORM(WATCHOS)
@interface WKContentView (WatchSupport) <WKFocusedFormControlViewDelegate, WKSelectMenuListViewControllerDelegate, WKTextInputListViewControllerDelegate>
@end
#endif

namespace WebKit {
using namespace WebCore;
using namespace WebKit;

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
    selectionClipRect = postLayoutData.focusedElementRect;
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

    if (a.type != WKSelectionDrawingInfo::SelectionType::None && a.selectionClipRect != b.selectionClipRect)
        return false;

    return true;
}

inline bool operator!=(const WKSelectionDrawingInfo& a, const WKSelectionDrawingInfo& b)
{
    return !(a == b);
}

static TextStream& operator<<(TextStream& stream, WKSelectionDrawingInfo::SelectionType type)
{
    switch (type) {
    case WKSelectionDrawingInfo::SelectionType::None: stream << "none"; break;
    case WKSelectionDrawingInfo::SelectionType::Plugin: stream << "plugin"; break;
    case WKSelectionDrawingInfo::SelectionType::Range: stream << "range"; break;
    }
    
    return stream;
}

TextStream& operator<<(TextStream& stream, const WKSelectionDrawingInfo& info)
{
    TextStream::GroupScope group(stream);
    stream.dumpProperty("type", info.type);
    stream.dumpProperty("caret rect", info.caretRect);
    stream.dumpProperty("selection rects", info.selectionRects);
    stream.dumpProperty("selection clip rect", info.selectionClipRect);
    return stream;
}

} // namespace WebKit

constexpr float highlightDelay = 0.12;
constexpr float tapAndHoldDelay = 0.75;
constexpr CGFloat minimumTapHighlightRadius = 2.0;
constexpr double fasterTapSignificantZoomThreshold = 0.8;

@interface WKTextRange : UITextRange {
    CGRect _startRect;
    CGRect _endRect;
    BOOL _isNone;
    BOOL _isRange;
    BOOL _isEditable;
    NSArray<WKTextSelectionRect *>*_selectionRects;
    NSUInteger _selectedTextLength;
}
@property (nonatomic) CGRect startRect;
@property (nonatomic) CGRect endRect;
@property (nonatomic) BOOL isNone;
@property (nonatomic) BOOL isRange;
@property (nonatomic) BOOL isEditable;
@property (nonatomic) NSUInteger selectedTextLength;
@property (copy, nonatomic) NSArray<WKTextSelectionRect *> *selectionRects;

+ (WKTextRange *)textRangeWithState:(BOOL)isNone isRange:(BOOL)isRange isEditable:(BOOL)isEditable startRect:(CGRect)startRect endRect:(CGRect)endRect selectionRects:(NSArray *)selectionRects selectedTextLength:(NSUInteger)selectedTextLength;

@end

@interface WKTextPosition : UITextPosition {
    CGRect _positionRect;
}

@property (nonatomic) CGRect positionRect;

+ (WKTextPosition *)textPositionWithRect:(CGRect)positionRect;

@end

@interface WKAutocorrectionRects : UIWKAutocorrectionRects
+ (WKAutocorrectionRects *)autocorrectionRectsWithFirstCGRect:(CGRect)firstRect lastCGRect:(CGRect)lastRect;
@end

@interface WKAutocorrectionContext : UIWKAutocorrectionContext
+ (WKAutocorrectionContext *)emptyAutocorrectionContext;
+ (WKAutocorrectionContext *)autocorrectionContextWithWebContext:(const WebKit::WebAutocorrectionContext&)context;
@end

@interface UITextInteractionAssistant (UITextInteractionAssistant_Internal)
// FIXME: this needs to be moved from the internal header to the private.
- (id)initWithView:(UIResponder <UITextInput> *)view;
- (void)selectWord;
@end

@interface UITextInteractionAssistant (WebKit)
@property (nonatomic, readonly) BOOL _wk_hasFloatingCursor;
@end

@interface UIView (UIViewInternalHack)
+ (BOOL)_addCompletion:(void(^)(BOOL))completion;
@end

@protocol UISelectionInteractionAssistant;

@interface WKFocusedElementInfo : NSObject <_WKFocusedElementInfo>
- (instancetype)initWithFocusedElementInformation:(const WebKit::FocusedElementInformation&)information isUserInitiated:(BOOL)isUserInitiated userObject:(NSObject <NSSecureCoding> *)userObject;
@end

@implementation UITextInteractionAssistant (WebKit)

- (BOOL)_wk_hasFloatingCursor
{
    return self.inGesture && !self.interactions.inGesture;
}

@end

@implementation WKFormInputSession {
    WeakObjCPtr<WKContentView> _contentView;
    RetainPtr<WKFocusedElementInfo> _focusedElementInfo;
    RetainPtr<UIView> _customInputView;
    RetainPtr<UIView> _customInputAccessoryView;
    RetainPtr<NSArray<UITextSuggestion *>> _suggestions;
    BOOL _accessoryViewShouldNotShow;
    BOOL _forceSecureTextEntry;
    BOOL _requiresStrongPasswordAssistance;
}

- (instancetype)initWithContentView:(WKContentView *)view focusedElementInfo:(WKFocusedElementInfo *)elementInfo requiresStrongPasswordAssistance:(BOOL)requiresStrongPasswordAssistance
{
    if (!(self = [super init]))
        return nil;

    _contentView = view;
    _focusedElementInfo = elementInfo;
    _requiresStrongPasswordAssistance = requiresStrongPasswordAssistance;

    return self;
}

- (id <_WKFocusedElementInfo>)focusedElementInfo
{
    return _focusedElementInfo.get();
}

- (NSObject <NSSecureCoding> *)userObject
{
    return [_focusedElementInfo userObject];
}

- (BOOL)isValid
{
    return !!_contentView;
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
    if (WebKit::currentUserInterfaceIdiomIsPad())
        [_contentView reloadInputViews];
}

- (BOOL)accessoryViewShouldNotShow
{
    return _accessoryViewShouldNotShow;
}

- (void)setAccessoryViewShouldNotShow:(BOOL)accessoryViewShouldNotShow
{
    if (_accessoryViewShouldNotShow == accessoryViewShouldNotShow)
        return;

    _accessoryViewShouldNotShow = accessoryViewShouldNotShow;
    [_contentView reloadInputViews];
}

- (BOOL)forceSecureTextEntry
{
    return _forceSecureTextEntry;
}

- (void)setForceSecureTextEntry:(BOOL)forceSecureTextEntry
{
    if (_forceSecureTextEntry == forceSecureTextEntry)
        return;

    _forceSecureTextEntry = forceSecureTextEntry;
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

- (UIView *)customInputAccessoryView
{
    return _customInputAccessoryView.get();
}

- (void)setCustomInputAccessoryView:(UIView *)customInputAccessoryView
{
    if (_customInputAccessoryView == customInputAccessoryView)
        return;

    _customInputAccessoryView = customInputAccessoryView;
    [_contentView reloadInputViews];
}

- (void)endEditing
{
    if ([_customInputView conformsToProtocol:@protocol(WKFormControl)])
        [(id<WKFormControl>)_customInputView.get() controlEndEditing];
}

- (NSArray<UITextSuggestion *> *)suggestions
{
    return _suggestions.get();
}

- (void)setSuggestions:(NSArray<UITextSuggestion *> *)suggestions
{
    if (suggestions == _suggestions || [suggestions isEqualToArray:_suggestions.get()])
        return;

    _suggestions = adoptNS([suggestions copy]);
    [_contentView updateTextSuggestionsForInputDelegate];
}

- (BOOL)requiresStrongPasswordAssistance
{
    return _requiresStrongPasswordAssistance;
}

- (void)invalidate
{
    id <UITextInputSuggestionDelegate> suggestionDelegate = (id <UITextInputSuggestionDelegate>)[_contentView inputDelegate];
    [suggestionDelegate setSuggestions:nil];
    _contentView = nil;
}

- (void)reloadFocusedElementContextView
{
    [_contentView reloadContextViewForPresentedListViewController];
}

@end

@implementation WKFocusedElementInfo {
    WKInputType _type;
    RetainPtr<NSString> _value;
    BOOL _isUserInitiated;
    RetainPtr<NSObject <NSSecureCoding>> _userObject;
    RetainPtr<NSString> _placeholder;
    RetainPtr<NSString> _label;
}

- (instancetype)initWithFocusedElementInformation:(const WebKit::FocusedElementInformation&)information isUserInitiated:(BOOL)isUserInitiated userObject:(NSObject <NSSecureCoding> *)userObject
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
    case WebKit::InputType::Drawing:
        _type = WKInputTypeDrawing;
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
        _type = WKInputTypeColor;
        break;
#endif
    case WebKit::InputType::None:
        _type = WKInputTypeNone;
        break;
    }
    _value = information.value;
    _isUserInitiated = isUserInitiated;
    _userObject = userObject;
    _placeholder = information.placeholder;
    _label = information.label;
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

- (NSObject <NSSecureCoding> *)userObject
{
    return _userObject.get();
}

- (NSString *)label
{
    return _label.get();
}

- (NSString *)placeholder
{
    return _placeholder.get();
}

@end

#if ENABLE(DRAG_SUPPORT)

@interface WKDragSessionContext : NSObject
- (void)addTemporaryDirectory:(NSString *)temporaryDirectory;
- (void)cleanUpTemporaryDirectories;
@end

@implementation WKDragSessionContext {
    RetainPtr<NSMutableArray> _temporaryDirectories;
}

- (void)addTemporaryDirectory:(NSString *)temporaryDirectory
{
    if (!_temporaryDirectories)
        _temporaryDirectories = adoptNS([NSMutableArray new]);
    [_temporaryDirectories addObject:temporaryDirectory];
}

- (void)cleanUpTemporaryDirectories
{
    for (NSString *directory in _temporaryDirectories.get()) {
        NSError *error = nil;
        [[NSFileManager defaultManager] removeItemAtPath:directory error:&error];
        RELEASE_LOG(DragAndDrop, "Removed temporary download directory: %@ with error: %@", directory, error);
    }
    _temporaryDirectories = nil;
}

@end

static WKDragSessionContext *existingLocalDragSessionContext(id <UIDragSession> session)
{
    return [session.localContext isKindOfClass:[WKDragSessionContext class]] ? (WKDragSessionContext *)session.localContext : nil;
}

static WKDragSessionContext *ensureLocalDragSessionContext(id <UIDragSession> session)
{
    if (WKDragSessionContext *existingContext = existingLocalDragSessionContext(session))
        return existingContext;

    if (session.localContext) {
        RELEASE_LOG(DragAndDrop, "Overriding existing local context: %@ on session: %@", session.localContext, session);
        ASSERT_NOT_REACHED();
    }

    session.localContext = [[[WKDragSessionContext alloc] init] autorelease];
    return (WKDragSessionContext *)session.localContext;
}

#endif // ENABLE(DRAG_SUPPORT)

@implementation UIGestureRecognizer (WKContentViewHelpers)

- (void)_wk_cancel
{
    if (!self.enabled)
        return;

    [self setEnabled:NO];
    [self setEnabled:YES];
}

@end

@interface WKContentView (WKInteractionPrivate)
- (void)accessibilitySpeakSelectionSetContent:(NSString *)string;
- (NSArray *)webSelectionRectsForSelectionRects:(const Vector<WebCore::SelectionRect>&)selectionRects;
- (void)_accessibilityDidGetSelectionRects:(NSArray *)selectionRects withGranularity:(UITextGranularity)granularity atOffset:(NSInteger)offset;
@end

@implementation WKContentView (WKInteraction)

- (BOOL)preventsPanningInXAxis
{
    return _preventsPanningInXAxis;
}

- (BOOL)preventsPanningInYAxis
{
    return _preventsPanningInYAxis;
}

- (WKFormInputSession *)_formInputSession
{
    return _formInputSession.get();
}

- (void)_createAndConfigureDoubleTapGestureRecognizer
{
    if (_doubleTapGestureRecognizer) {
        [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];
        [_doubleTapGestureRecognizer setDelegate:nil];
        [_doubleTapGestureRecognizer setGestureFailedTarget:nil action:nil];
    }

    _doubleTapGestureRecognizer = adoptNS([[WKSyntheticTapGestureRecognizer alloc] initWithTarget:self action:@selector(_doubleTapRecognized:)]);
    [_doubleTapGestureRecognizer setGestureFailedTarget:self action:@selector(_doubleTapDidFail:)];
    [_doubleTapGestureRecognizer setNumberOfTapsRequired:2];
    [_doubleTapGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [_singleTapGestureRecognizer requireGestureRecognizerToFail:_doubleTapGestureRecognizer.get()];
}

- (void)_createAndConfigureLongPressGestureRecognizer
{
    _longPressGestureRecognizer = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_longPressRecognized:)]);
    [_longPressGestureRecognizer setDelay:tapAndHoldDelay];
    [_longPressGestureRecognizer setDelegate:self];
    [_longPressGestureRecognizer _setRequiresQuietImpulse:YES];
    [self addGestureRecognizer:_longPressGestureRecognizer.get()];
}

- (void)setUpInteraction
{
    // If the page is not valid yet then delay interaction setup until the process is launched/relaunched.
    if (!_page->hasRunningProcess())
        return;

    if (_hasSetUpInteractions)
        return;

    if (!_interactionViewsContainerView) {
        _interactionViewsContainerView = adoptNS([[UIView alloc] init]);
        [_interactionViewsContainerView layer].name = @"InteractionViewsContainer";
        [_interactionViewsContainerView setOpaque:NO];
        [_interactionViewsContainerView layer].anchorPoint = CGPointZero;
        [self.superview addSubview:_interactionViewsContainerView.get()];
    }

    _keyboardScrollingAnimator = adoptNS([[WKKeyboardScrollViewAnimator alloc] initWithScrollView:self.webView.scrollView]);
    [_keyboardScrollingAnimator setDelegate:self];

    [self.layer addObserver:self forKeyPath:@"transform" options:NSKeyValueObservingOptionInitial context:nil];

    _touchActionLeftSwipeGestureRecognizer = adoptNS([[UISwipeGestureRecognizer alloc] initWithTarget:nil action:nil]);
    [_touchActionLeftSwipeGestureRecognizer setDirection:UISwipeGestureRecognizerDirectionLeft];
    [_touchActionLeftSwipeGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_touchActionLeftSwipeGestureRecognizer.get()];

    _touchActionRightSwipeGestureRecognizer = adoptNS([[UISwipeGestureRecognizer alloc] initWithTarget:nil action:nil]);
    [_touchActionRightSwipeGestureRecognizer setDirection:UISwipeGestureRecognizerDirectionRight];
    [_touchActionRightSwipeGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_touchActionRightSwipeGestureRecognizer.get()];

    _touchActionUpSwipeGestureRecognizer = adoptNS([[UISwipeGestureRecognizer alloc] initWithTarget:nil action:nil]);
    [_touchActionUpSwipeGestureRecognizer setDirection:UISwipeGestureRecognizerDirectionUp];
    [_touchActionUpSwipeGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_touchActionUpSwipeGestureRecognizer.get()];

    _touchActionDownSwipeGestureRecognizer = adoptNS([[UISwipeGestureRecognizer alloc] initWithTarget:nil action:nil]);
    [_touchActionDownSwipeGestureRecognizer setDirection:UISwipeGestureRecognizerDirectionDown];
    [_touchActionDownSwipeGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_touchActionDownSwipeGestureRecognizer.get()];

#if ENABLE(IOS_TOUCH_EVENTS)
    _deferringGestureRecognizerForImmediatelyResettableGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_deferringGestureRecognizerForImmediatelyResettableGestures setName:@"Touch event deferrer (immediate reset)"];

    _deferringGestureRecognizerForDelayedResettableGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_deferringGestureRecognizerForDelayedResettableGestures setName:@"Touch event deferrer (delayed reset)"];

    _deferringGestureRecognizerForSyntheticTapGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_deferringGestureRecognizerForSyntheticTapGestures setName:@"Touch event deferrer (synthetic tap)"];

    for (WKDeferringGestureRecognizer *gesture in self._deferringGestureRecognizers) {
        gesture.delegate = self;
        [self addGestureRecognizer:gesture];
    }
#endif

    _touchEventGestureRecognizer = adoptNS([[UIWebTouchEventsGestureRecognizer alloc] initWithTarget:self action:@selector(_webTouchEventsRecognized:) touchDelegate:self]);
    [_touchEventGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_touchEventGestureRecognizer.get()];

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [self setUpMouseGestureRecognizer];
#endif

#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
    _lookupGestureRecognizer = adoptNS([[_UILookupGestureRecognizer alloc] initWithTarget:self action:@selector(_lookupGestureRecognized:)]);
    [_lookupGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_lookupGestureRecognizer.get()];
#endif

    _singleTapGestureRecognizer = adoptNS([[WKSyntheticTapGestureRecognizer alloc] initWithTarget:self action:@selector(_singleTapRecognized:)]);
    [_singleTapGestureRecognizer setDelegate:self];
    [_singleTapGestureRecognizer setGestureIdentifiedTarget:self action:@selector(_singleTapIdentified:)];
    [_singleTapGestureRecognizer setResetTarget:self action:@selector(_singleTapDidReset:)];
    [_singleTapGestureRecognizer setSupportingWebTouchEventsGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self addGestureRecognizer:_singleTapGestureRecognizer.get()];

    _nonBlockingDoubleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_nonBlockingDoubleTapRecognized:)]);
    [_nonBlockingDoubleTapGestureRecognizer setNumberOfTapsRequired:2];
    [_nonBlockingDoubleTapGestureRecognizer setDelegate:self];
    [_nonBlockingDoubleTapGestureRecognizer setEnabled:NO];
    [self addGestureRecognizer:_nonBlockingDoubleTapGestureRecognizer.get()];

    _doubleTapGestureRecognizerForDoubleClick = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_doubleTapRecognizedForDoubleClick:)]);
    [_doubleTapGestureRecognizerForDoubleClick setNumberOfTapsRequired:2];
    [_doubleTapGestureRecognizerForDoubleClick setDelegate:self];
    [self addGestureRecognizer:_doubleTapGestureRecognizerForDoubleClick.get()];

    [self _createAndConfigureDoubleTapGestureRecognizer];

    _twoFingerDoubleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_twoFingerDoubleTapRecognized:)]);
    [_twoFingerDoubleTapGestureRecognizer setNumberOfTapsRequired:2];
    [_twoFingerDoubleTapGestureRecognizer setNumberOfTouchesRequired:2];
    [_twoFingerDoubleTapGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];

    _highlightLongPressGestureRecognizer = adoptNS([[WKHighlightLongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_highlightLongPressRecognized:)]);
    [_highlightLongPressGestureRecognizer setDelay:highlightDelay];
    [_highlightLongPressGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];

    [self _createAndConfigureLongPressGestureRecognizer];

#if HAVE(LINK_PREVIEW)
    [self _updateLongPressAndHighlightLongPressGestures];
#endif

#if ENABLE(DATA_INTERACTION)
    [self setUpDragAndDropInteractions];
#endif

#if HAVE(UI_CURSOR_INTERACTION)
    [self setUpCursorInteraction];
#endif

#if USE(APPLE_INTERNAL_SDK)
    [self setUpAdditionalInteractions];
#endif

    _twoFingerSingleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_twoFingerSingleTapGestureRecognized:)]);
    [_twoFingerSingleTapGestureRecognizer setAllowableMovement:60];
    [_twoFingerSingleTapGestureRecognizer _setAllowableSeparation:150];
    [_twoFingerSingleTapGestureRecognizer setNumberOfTapsRequired:1];
    [_twoFingerSingleTapGestureRecognizer setNumberOfTouchesRequired:2];
    [_twoFingerSingleTapGestureRecognizer setDelaysTouchesEnded:NO];
    [_twoFingerSingleTapGestureRecognizer setDelegate:self];
    [_twoFingerSingleTapGestureRecognizer setEnabled:!self.webView._editable];
    [self addGestureRecognizer:_twoFingerSingleTapGestureRecognizer.get()];

    _stylusSingleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_stylusSingleTapRecognized:)]);
    [_stylusSingleTapGestureRecognizer setNumberOfTapsRequired:1];
    [_stylusSingleTapGestureRecognizer setDelegate:self];
    [_stylusSingleTapGestureRecognizer setAllowedTouchTypes:@[ @(UITouchTypePencil) ]];
    [self addGestureRecognizer:_stylusSingleTapGestureRecognizer.get()];

    _touchActionGestureRecognizer = adoptNS([[WKTouchActionGestureRecognizer alloc] initWithTouchActionDelegate:self]);
    [self addGestureRecognizer:_touchActionGestureRecognizer.get()];

#if HAVE(LINK_PREVIEW)
    [self _registerPreview];
#endif

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(_willHideMenu:) name:UIMenuControllerWillHideMenuNotification object:nil];
    [center addObserver:self selector:@selector(_didHideMenu:) name:UIMenuControllerDidHideMenuNotification object:nil];
    [center addObserver:self selector:@selector(_keyboardDidRequestDismissal:) name:UIKeyboardPrivateDidRequestDismissalNotification object:nil];

    _showingTextStyleOptions = NO;

    // FIXME: This should be called when we get notified that loading has completed.
    [self setUpTextSelectionAssistant];
    
    _actionSheetAssistant = adoptNS([[WKActionSheetAssistant alloc] initWithView:self]);
    [_actionSheetAssistant setDelegate:self];
    _smartMagnificationController = makeUnique<WebKit::SmartMagnificationController>(self);
    _touchEventsCanPreventNativeGestures = YES;
    _isExpectingFastSingleTapCommit = NO;
    _potentialTapInProgress = NO;
    _isDoubleTapPending = NO;
    _showDebugTapHighlightsForFastClicking = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitShowFastClickDebugTapHighlights"];
    _needsDeferredEndScrollingSelectionUpdate = NO;
    _isChangingFocus = NO;
    _isBlurringFocusedElement = NO;

#if ENABLE(DATALIST_ELEMENT)
    _dataListTextSuggestionsInputView = nil;
    _dataListTextSuggestions = nil;
#endif

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    _textCheckingController = makeUnique<WebKit::TextCheckingController>(*_page);
#endif

    _page->process().updateTextCheckerState();
    _page->setScreenIsBeingCaptured([[[self window] screen] isCaptured]);

    _hasSetUpInteractions = YES;
}

- (void)cleanUpInteraction
{
    if (!_hasSetUpInteractions)
        return;

    _textInteractionAssistant = nil;
    
    [_actionSheetAssistant cleanupSheet];
    _actionSheetAssistant = nil;
    
    _smartMagnificationController = nil;
    _didAccessoryTabInitiateFocus = NO;
    _isChangingFocusUsingAccessoryTab = NO;
    _isExpectingFastSingleTapCommit = NO;
    _needsDeferredEndScrollingSelectionUpdate = NO;
    [_formInputSession invalidate];
    _formInputSession = nil;
    [_highlightView removeFromSuperview];
    _outstandingPositionInformationRequest = WTF::nullopt;
    _isWaitingOnPositionInformation = NO;

    _focusRequiresStrongPasswordAssistance = NO;
    _additionalContextForStrongPasswordAssistance = nil;
    _waitingForEditDragSnapshot = NO;

#if USE(UIKIT_KEYBOARD_ADDITIONS)
    _candidateViewNeedsUpdate = NO;
    _seenHardwareKeyDownInNonEditableElement = NO;
#endif

    _textInteractionDidChangeFocusedElement = NO;
    _textInteractionIsHappening = NO;

    if (_interactionViewsContainerView) {
        [self.layer removeObserver:self forKeyPath:@"transform"];
        [_interactionViewsContainerView removeFromSuperview];
        _interactionViewsContainerView = nil;
    }

    [_touchEventGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_touchEventGestureRecognizer.get()];

#if ENABLE(IOS_TOUCH_EVENTS)
    for (WKDeferringGestureRecognizer *gesture in self._deferringGestureRecognizers) {
        gesture.delegate = nil;
        [self removeGestureRecognizer:gesture];
    }
#endif

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [_mouseGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_mouseGestureRecognizer.get()];
#endif

#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
    [_lookupGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_lookupGestureRecognizer.get()];
#endif

    [_singleTapGestureRecognizer setDelegate:nil];
    [_singleTapGestureRecognizer setGestureIdentifiedTarget:nil action:nil];
    [_singleTapGestureRecognizer setResetTarget:nil action:nil];
    [_singleTapGestureRecognizer setSupportingWebTouchEventsGestureRecognizer:nil];
    [self removeGestureRecognizer:_singleTapGestureRecognizer.get()];

    [_highlightLongPressGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_highlightLongPressGestureRecognizer.get()];

    [_longPressGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_longPressGestureRecognizer.get()];

    [_doubleTapGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];

    [_nonBlockingDoubleTapGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_nonBlockingDoubleTapGestureRecognizer.get()];

    [_doubleTapGestureRecognizerForDoubleClick setDelegate:nil];
    [self removeGestureRecognizer:_doubleTapGestureRecognizerForDoubleClick.get()];

    [_twoFingerDoubleTapGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];

    [_twoFingerSingleTapGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_twoFingerSingleTapGestureRecognizer.get()];

    [_stylusSingleTapGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_stylusSingleTapGestureRecognizer.get()];

    [self removeGestureRecognizer:_touchActionGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionLeftSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionRightSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionUpSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionDownSwipeGestureRecognizer.get()];

    _layerTreeTransactionIdAtLastInteractionStart = { };

#if ENABLE(DATA_INTERACTION)
    [existingLocalDragSessionContext(_dragDropInteractionState.dragSession()) cleanUpTemporaryDirectories];
    [self teardownDragAndDropInteractions];
#endif

#if HAVE(UI_CURSOR_INTERACTION)
    [self removeInteraction:_cursorInteraction.get()];
    _cursorInteraction = nil;
#endif

#if USE(APPLE_INTERNAL_SDK)
    [self cleanUpAdditionalInteractions];
#endif

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

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if (_shareSheet) {
        [_shareSheet setDelegate:nil];
        [_shareSheet dismiss];
        _shareSheet = nil;
    }
#endif

    [self _resetInputViewDeferral];
    _focusedElementInformation = { };
    
    [_keyboardScrollingAnimator invalidate];
    _keyboardScrollingAnimator = nil;

#if HAVE(PENCILKIT)
    _drawingCoordinator = nil;
#endif

#if ENABLE(DATALIST_ELEMENT)
    _dataListTextSuggestionsInputView = nil;
    _dataListTextSuggestions = nil;
#endif

    _hasSetUpInteractions = NO;
    _suppressSelectionAssistantReasons = { };

    [self _resetPanningPreventionFlags];
    [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];
}

- (void)_removeDefaultGestureRecognizers
{
#if ENABLE(IOS_TOUCH_EVENTS)
    for (WKDeferringGestureRecognizer *gesture in self._deferringGestureRecognizers)
        [self removeGestureRecognizer:gesture];
#endif
    [self removeGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self removeGestureRecognizer:_singleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
    [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_nonBlockingDoubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_doubleTapGestureRecognizerForDoubleClick.get()];
    [self removeGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_twoFingerSingleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_stylusSingleTapGestureRecognizer.get()];
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [self removeGestureRecognizer:_mouseGestureRecognizer.get()];
#endif
#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
    [self removeGestureRecognizer:_lookupGestureRecognizer.get()];
#endif
    [self removeGestureRecognizer:_touchActionGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionLeftSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionRightSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionUpSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionDownSwipeGestureRecognizer.get()];
}

- (void)_addDefaultGestureRecognizers
{
#if ENABLE(IOS_TOUCH_EVENTS)
    for (WKDeferringGestureRecognizer *gesture in self._deferringGestureRecognizers)
        [self addGestureRecognizer:gesture];
#endif
    [self addGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self addGestureRecognizer:_singleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
    [self addGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_nonBlockingDoubleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_doubleTapGestureRecognizerForDoubleClick.get()];
    [self addGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_twoFingerSingleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_stylusSingleTapGestureRecognizer.get()];
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [self addGestureRecognizer:_mouseGestureRecognizer.get()];
#endif
#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
    [self addGestureRecognizer:_lookupGestureRecognizer.get()];
#endif
    [self addGestureRecognizer:_touchActionGestureRecognizer.get()];
    [self addGestureRecognizer:_touchActionLeftSwipeGestureRecognizer.get()];
    [self addGestureRecognizer:_touchActionRightSwipeGestureRecognizer.get()];
    [self addGestureRecognizer:_touchActionUpSwipeGestureRecognizer.get()];
    [self addGestureRecognizer:_touchActionDownSwipeGestureRecognizer.get()];
}

- (void)_didChangeLinkPreviewAvailability
{
    [self _updateLongPressAndHighlightLongPressGestures];
}

- (void)_updateLongPressAndHighlightLongPressGestures
{
    // We only disable the highlight long press gesture in the case where UIContextMenu is available and we
    // also allow link previews, since the context menu interaction's gestures need to take precedence over
    // highlight long press gestures.
    [_highlightLongPressGestureRecognizer setEnabled:!self._shouldUseContextMenus || !self.webView.allowsLinkPreview];

    // We only enable the long press gesture in the case where the app is linked on iOS 12 or earlier (and
    // therefore prefers the legacy action sheet over context menus), and link previews are also enabled.
    [_longPressGestureRecognizer setEnabled:!self._shouldUseContextMenus && self.webView.allowsLinkPreview];
}

- (UIView *)unscaledView
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


#pragma mark - UITextAutoscrolling
- (void)startAutoscroll:(CGPoint)pointInDocument
{
    _page->startAutoscrollAtPosition(pointInDocument);
}

- (void)cancelAutoscroll
{
    _page->cancelAutoscroll();
}

- (void)scrollSelectionToVisible:(BOOL)animated
{
    // Used to scroll selection on keyboard up; we already scroll to visible.
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

    [self _updateTapHighlight];

    _selectionNeedsUpdate = YES;
    [self _updateChangedSelection:YES];
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
        if (hitView) {
            LOG_WITH_STREAM(UIHitTesting, stream << self << "hitTest at " << WebCore::FloatPoint(point) << " found interaction view " << hitView);
            return hitView;
        }
    }

    LOG_WITH_STREAM(UIHitTesting, stream << "hit-testing WKContentView subviews " << [[self recursiveDescription] UTF8String]);
    UIView* hitView = [super hitTest:point withEvent:event];
    LOG_WITH_STREAM(UIHitTesting, stream << " found view " << [hitView class] << " " << (void*)hitView);
    return hitView;
}

- (const WebKit::InteractionInformationAtPosition&)positionInformation
{
    return _positionInformation;
}

- (void)setInputDelegate:(id <UITextInputDelegate>)inputDelegate
{
    _inputDelegate = inputDelegate;
}

- (id <UITextInputDelegate>)inputDelegate
{
    return _inputDelegate.getAutoreleased();
}

- (CGPoint)lastInteractionLocation
{
    return _lastInteractionLocation;
}

- (BOOL)shouldHideSelectionWhenScrolling
{
    if (_isEditable)
        return _focusedElementInformation.insideFixedPosition;

    auto& editorState = _page->editorState();
    return !editorState.isMissingPostLayoutData && editorState.postLayoutData().insideFixedPosition;
}

- (BOOL)isEditable
{
    return _isEditable;
}

- (BOOL)setIsEditable:(BOOL)isEditable
{
    if (isEditable == _isEditable)
        return NO;

    _isEditable = isEditable;
    return YES;
}

- (void)_endEditing
{
    [_inputPeripheral endEditing];
    [_formInputSession endEditing];
#if ENABLE(DATALIST_ELEMENT)
    [_dataListTextSuggestionsInputView controlEndEditing];
#endif
}

- (void)_cancelPreviousResetInputViewDeferralRequest
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_resetInputViewDeferral) object:nil];
}

- (void)_scheduleResetInputViewDeferralAfterBecomingFirstResponder
{
    [self _cancelPreviousResetInputViewDeferralRequest];

    const NSTimeInterval inputViewDeferralWatchdogTimerDuration = 0.5;
    [self performSelector:@selector(_resetInputViewDeferral) withObject:self afterDelay:inputViewDeferralWatchdogTimerDuration];
}

- (void)_resetInputViewDeferral
{
    [self _cancelPreviousResetInputViewDeferralRequest];
    _inputViewUpdateDeferrer = nullptr;
}

- (BOOL)canBecomeFirstResponder
{
    return _becomingFirstResponder;
}

- (BOOL)canBecomeFirstResponderForWebView
{
    if (_resigningFirstResponder)
        return NO;
    // We might want to return something else
    // if we decide to enable/disable interaction programmatically.
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return [_webView becomeFirstResponder];
}

- (BOOL)becomeFirstResponderForWebView
{
    if (_resigningFirstResponder)
        return NO;

    if (!_inputViewUpdateDeferrer)
        _inputViewUpdateDeferrer = makeUnique<WebKit::InputViewUpdateDeferrer>(self);

    BOOL didBecomeFirstResponder;
    {
        SetForScope<BOOL> becomingFirstResponder { _becomingFirstResponder, YES };
        didBecomeFirstResponder = [super becomeFirstResponder];
    }

    if (didBecomeFirstResponder) {
        _page->installActivityStateChangeCompletionHandler([weakSelf = WeakObjCPtr<WKContentView>(self)] {
            if (!weakSelf)
                return;

            auto strongSelf = weakSelf.get();
            [strongSelf _resetInputViewDeferral];
        });

        _page->activityStateDidChange(WebCore::ActivityState::IsFocused, WebKit::WebPageProxy::ActivityStateChangeDispatchMode::Immediate);

        if ([self canShowNonEmptySelectionView] || (!_suppressSelectionAssistantReasons && _textInteractionIsHappening))
            [_textInteractionAssistant activateSelection];

        [self _scheduleResetInputViewDeferralAfterBecomingFirstResponder];
    } else
        [self _resetInputViewDeferral];

    return didBecomeFirstResponder;
}

- (BOOL)resignFirstResponder
{
    return [_webView resignFirstResponder];
}

typedef NS_ENUM(NSInteger, EndEditingReason) {
    EndEditingReasonAccessoryDone,
    EndEditingReasonResigningFirstResponder,
};

- (void)endEditingAndUpdateFocusAppearanceWithReason:(EndEditingReason)reason
{
    if (!self.webView._retainingActiveFocusedState) {
        // We need to complete the editing operation before we blur the element.
        [self _endEditing];
        if ((reason == EndEditingReasonAccessoryDone && !WebKit::currentUserInterfaceIdiomIsPad()) || _keyboardDidRequestDismissal || self._shouldUseLegacySelectPopoverDismissalBehavior) {
            _page->blurFocusedElement();
            // Don't wait for WebPageProxy::blurFocusedElement() to round-trip back to us to hide the keyboard
            // because we know that the user explicitly requested us to do so.
            [self _elementDidBlur];
        }
    }

    [self _cancelInteraction];
    [_textInteractionAssistant deactivateSelection];

    [self _resetInputViewDeferral];
}

- (BOOL)resignFirstResponderForWebView
{
    // FIXME: Maybe we should call resignFirstResponder on the superclass
    // and do nothing if the return value is NO.

    SetForScope<BOOL> resigningFirstResponderScope { _resigningFirstResponder, YES };

    [self endEditingAndUpdateFocusAppearanceWithReason:EndEditingReasonResigningFirstResponder];

    // If the user explicitly dismissed the keyboard then we will lose first responder
    // status only to gain it back again. Just don't resign in that case.
    if (_keyboardDidRequestDismissal) {
        _keyboardDidRequestDismissal = NO;
        return NO;
    }

    bool superDidResign = [super resignFirstResponder];

    if (superDidResign) {
        [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];
        _page->activityStateDidChange(WebCore::ActivityState::IsFocused, WebKit::WebPageProxy::ActivityStateChangeDispatchMode::Immediate);

        if (_keyWebEventHandler) {
            dispatch_async(dispatch_get_main_queue(), [weakHandler = WeakObjCPtr<id>(_keyWebEventHandler.get()), weakSelf = WeakObjCPtr<WKContentView>(self)] {
                auto strongSelf = weakSelf.get();
                if (!strongSelf || [strongSelf isFirstResponder])
                    return;
                auto strongHandler = weakHandler.get();
                if (!strongHandler)
                    return;
                if (strongSelf->_keyWebEventHandler.get() != strongHandler.get())
                    return;
                auto keyEventHandler = std::exchange(strongSelf->_keyWebEventHandler, nil);
                auto page = strongSelf->_page;
                if (!page)
                    return;
                ASSERT(page->hasQueuedKeyEvent());
                keyEventHandler(page->firstQueuedKeyEvent().nativeEvent(), YES);
            });
        }
    }

    return superDidResign;
}

- (void)cancelPointersForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    NSMapTable<NSNumber *, UITouch *> *activeTouches = [_touchEventGestureRecognizer activeTouchesByIdentifier];
    for (NSNumber *touchIdentifier in activeTouches) {
        UITouch *touch = [activeTouches objectForKey:touchIdentifier];
        if ([touch.gestureRecognizers containsObject:gestureRecognizer])
            _page->cancelPointer([touchIdentifier unsignedIntValue], WebCore::roundedIntPoint([touch locationInView:self]));
    }
}

- (WTF::Optional<unsigned>)activeTouchIdentifierForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    NSMapTable<NSNumber *, UITouch *> *activeTouches = [_touchEventGestureRecognizer activeTouchesByIdentifier];
    for (NSNumber *touchIdentifier in activeTouches) {
        UITouch *touch = [activeTouches objectForKey:touchIdentifier];
        if ([touch.gestureRecognizers containsObject:gestureRecognizer])
            return [touchIdentifier unsignedIntValue];
    }
    return WTF::nullopt;
}

inline static UIKeyModifierFlags gestureRecognizerModifierFlags(UIGestureRecognizer *recognizer)
{
#if HAVE(UI_GESTURE_RECOGNIZER_MODIFIER_FLAGS)
    return recognizer.modifierFlags;
#else
    return [recognizer respondsToSelector:@selector(_modifierFlags)] ? [recognizer _modifierFlags] : 0;
#endif
}

- (void)_webTouchEventsRecognized:(UIWebTouchEventsGestureRecognizer *)gestureRecognizer
{
    if (!_page->hasRunningProcess())
        return;

    const _UIWebTouchEvent* lastTouchEvent = gestureRecognizer.lastTouchEvent;

    _lastInteractionLocation = lastTouchEvent->locationInDocumentCoordinates;
    if (lastTouchEvent->type == UIWebTouchEventTouchBegin) {
        [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];
        _layerTreeTransactionIdAtLastInteractionStart = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).lastCommittedLayerTreeTransactionID();

#if ENABLE(TOUCH_EVENTS)
        _page->resetPotentialTapSecurityOrigin();
#endif

        WebKit::InteractionInformationRequest positionInformationRequest { WebCore::IntPoint(_lastInteractionLocation) };
        [self doAfterPositionInformationUpdate:[assistant = WeakObjCPtr<WKActionSheetAssistant>(_actionSheetAssistant.get())] (WebKit::InteractionInformationAtPosition information) {
            [assistant interactionDidStartWithPositionInformation:information];
        } forRequest:positionInformationRequest];
    }

#if ENABLE(TOUCH_EVENTS)
    WebKit::NativeWebTouchEvent nativeWebTouchEvent { lastTouchEvent, gestureRecognizerModifierFlags(gestureRecognizer) };
    nativeWebTouchEvent.setCanPreventNativeGestures(_touchEventsCanPreventNativeGestures || [gestureRecognizer isDefaultPrevented]);

    [self _handleTouchActionsForTouchEvent:nativeWebTouchEvent];

    if (_touchEventsCanPreventNativeGestures)
        _page->handlePreventableTouchEvent(nativeWebTouchEvent);
    else
        _page->handleUnpreventableTouchEvent(nativeWebTouchEvent);

    if (nativeWebTouchEvent.allTouchPointsAreReleased()) {
        _touchEventsCanPreventNativeGestures = YES;

        if (!_page->isScrollingOrZooming())
            [self _resetPanningPreventionFlags];
    }
#endif
}

#if ENABLE(TOUCH_EVENTS)
- (void)_handleTouchActionsForTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent
{
    auto* scrollingCoordinator = _page->scrollingCoordinatorProxy();
    if (!scrollingCoordinator)
        return;

    for (const auto& touchPoint : touchEvent.touchPoints()) {
        auto phase = touchPoint.phase();
        if (phase == WebKit::WebPlatformTouchPoint::TouchPressed) {
            auto touchActions = WebKit::touchActionsForPoint(self, touchPoint.location());
            LOG_WITH_STREAM(UIHitTesting, stream << "touchActionsForPoint " << touchPoint.location() << " found " << touchActions);
            if (!touchActions || touchActions.contains(WebCore::TouchAction::Auto))
                continue;
            [_touchActionGestureRecognizer setTouchActions:touchActions forTouchIdentifier:touchPoint.identifier()];
            scrollingCoordinator->setTouchActionsForTouchIdentifier(touchActions, touchPoint.identifier());
            _preventsPanningInXAxis = !touchActions.containsAny({ WebCore::TouchAction::PanX, WebCore::TouchAction::Manipulation });
            _preventsPanningInYAxis = !touchActions.containsAny({ WebCore::TouchAction::PanY, WebCore::TouchAction::Manipulation });
        } else if (phase == WebKit::WebPlatformTouchPoint::TouchReleased || phase == WebKit::WebPlatformTouchPoint::TouchCancelled) {
            [_touchActionGestureRecognizer clearTouchActionsForTouchIdentifier:touchPoint.identifier()];
            scrollingCoordinator->clearTouchActionsForTouchIdentifier(touchPoint.identifier());
        }
    }
}
#endif // ENABLE(TOUCH_EVENTS)

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
    if (gestureRecognizer == _lookupGestureRecognizer)
        return YES;
#endif

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    if (gestureRecognizer != _mouseGestureRecognizer && [_mouseGestureRecognizer mouseTouch] == touch)
        return NO;

    if (gestureRecognizer == _doubleTapGestureRecognizer || gestureRecognizer == _nonBlockingDoubleTapGestureRecognizer)
        return touch.type != UITouchTypeIndirectPointer;
#endif

    if (gestureRecognizer == _touchActionLeftSwipeGestureRecognizer || gestureRecognizer == _touchActionRightSwipeGestureRecognizer || gestureRecognizer == _touchActionUpSwipeGestureRecognizer || gestureRecognizer == _touchActionDownSwipeGestureRecognizer) {
        // We update the enabled state of the various swipe gesture recognizers such that if we have a unidirectional touch-action
        // specified (only pan-x or only pan-y) we enable the two recognizers in the opposite axis to prevent scrolling from starting
        // if the initial gesture is such a swipe. Since the recognizers are specified to use a single finger for recognition, we don't
        // need to worry about the case where there may be more than a single touch for a given UIScrollView.
        auto touchLocation = [touch locationInView:self];
        auto touchActions = WebKit::touchActionsForPoint(self, WebCore::roundedIntPoint(touchLocation));
        LOG_WITH_STREAM(UIHitTesting, stream << "gestureRecognizer:shouldReceiveTouch: at " << WebCore::FloatPoint(touchLocation) << " - touch actions " << touchActions);
        if (gestureRecognizer == _touchActionLeftSwipeGestureRecognizer || gestureRecognizer == _touchActionRightSwipeGestureRecognizer)
            return touchActions == WebCore::TouchAction::PanY;
        return touchActions == WebCore::TouchAction::PanX;
    }

    return YES;
}

#pragma mark - WKTouchActionGestureRecognizerDelegate implementation

- (BOOL)gestureRecognizerMayPanWebView:(UIGestureRecognizer *)gestureRecognizer
{
    // The gesture recognizer is the main UIScrollView's UIPanGestureRecognizer.
    if (gestureRecognizer == [_webView scrollView].panGestureRecognizer)
        return YES;

    // The gesture recognizer is a child UIScrollView's UIPanGestureRecognizer created by WebKit.
    if (gestureRecognizer.view && [gestureRecognizer.view isKindOfClass:[WKChildScrollView class]])
        return YES;

    return NO;
}

- (BOOL)gestureRecognizerMayPinchToZoomWebView:(UIGestureRecognizer *)gestureRecognizer
{
    // The gesture recognizer is the main UIScrollView's UIPinchGestureRecognizer.
    if (gestureRecognizer == [_webView scrollView].pinchGestureRecognizer)
        return YES;

    // The gesture recognizer is another UIPichGestureRecognizer known to lead to pinch-to-zoom.
    if ([gestureRecognizer isKindOfClass:[UIPinchGestureRecognizer class]]) {
        if (auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>(self.webView.UIDelegate)) {
            if ([uiDelegate respondsToSelector:@selector(_webView:gestureRecognizerCouldPinch:)])
                return [uiDelegate _webView:self.webView gestureRecognizerCouldPinch:gestureRecognizer];
        }
    }
    return NO;
}

- (BOOL)gestureRecognizerMayDoubleTapToZoomWebView:(UIGestureRecognizer *)gestureRecognizer
{
    return gestureRecognizer == _doubleTapGestureRecognizer || gestureRecognizer == _twoFingerDoubleTapGestureRecognizer;
}

- (NSMapTable<NSNumber *, UITouch *> *)touchActionActiveTouches
{
    return [_touchEventGestureRecognizer activeTouchesByIdentifier];
}

- (void)_resetPanningPreventionFlags
{
    _preventsPanningInXAxis = NO;
    _preventsPanningInYAxis = NO;
}

- (void)_inspectorNodeSearchRecognized:(UIGestureRecognizer *)gestureRecognizer
{
    ASSERT(_inspectorNodeSearchEnabled);
    [self _resetIsDoubleTapPending];

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

static WebCore::FloatQuad inflateQuad(const WebCore::FloatQuad& quad, float inflateSize)
{
    // We sort the output points like this (as expected by the highlight view):
    //  p2------p3
    //  |       |
    //  p1------p4

    // 1) Sort the points horizontally.
    WebCore::FloatPoint points[4] = { quad.p1(), quad.p4(), quad.p2(), quad.p3() };
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

    return WebCore::FloatQuad(points[1], points[0], points[2], points[3]);
}

#if ENABLE(TOUCH_EVENTS)
- (void)_webTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsNativeGesture
{
    if (!preventsNativeGesture || ![_touchEventGestureRecognizer isDispatchingTouchEvents])
        return;

    _longPressCanClick = NO;
    _touchEventsCanPreventNativeGestures = NO;
    [_touchEventGestureRecognizer setDefaultPrevented:YES];
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS)

- (NSArray<WKDeferringGestureRecognizer *> *)_deferringGestureRecognizers
{
    WKDeferringGestureRecognizer *recognizers[3];
    NSUInteger count = 0;
    auto add = [&] (const RetainPtr<WKDeferringGestureRecognizer>& recognizer) {
        if (recognizer)
            recognizers[count++] = recognizer.get();
    };
    add(_deferringGestureRecognizerForImmediatelyResettableGestures);
    add(_deferringGestureRecognizerForDelayedResettableGestures);
    add(_deferringGestureRecognizerForSyntheticTapGestures);
    return [NSArray arrayWithObjects:recognizers count:count];
}

- (void)_doneDeferringNativeGestures:(BOOL)preventNativeGestures
{
    for (WKDeferringGestureRecognizer *gesture in self._deferringGestureRecognizers)
        [gesture setDefaultPrevented:preventNativeGestures];
}

#endif // ENABLE(IOS_TOUCH_EVENTS)

static NSValue *nsSizeForTapHighlightBorderRadius(WebCore::IntSize borderRadius, CGFloat borderRadiusScale)
{
    return [NSValue valueWithCGSize:CGSizeMake((borderRadius.width() * borderRadiusScale) + minimumTapHighlightRadius, (borderRadius.height() * borderRadiusScale) + minimumTapHighlightRadius)];
}

- (void)_updateTapHighlight
{
    if (![_highlightView superview])
        return;

    [_highlightView setColor:adoptNS([[UIColor alloc] initWithCGColor:cachedCGColor(_tapHighlightInformation.color)]).get()];

    auto& highlightedQuads = _tapHighlightInformation.quads;
    bool allRectilinear = true;
    for (auto& quad : highlightedQuads) {
        if (!quad.isRectilinear()) {
            allRectilinear = false;
            break;
        }
    }

    auto selfScale = self.layer.transform.m11;
    if (allRectilinear) {
        float deviceScaleFactor = _page->deviceScaleFactor();
        auto rects = createNSArray(highlightedQuads, [&] (auto& quad) {
            auto boundingBox = quad.boundingBox();
            boundingBox.scale(selfScale);
            boundingBox.inflate(minimumTapHighlightRadius);
            return [NSValue valueWithCGRect:encloseRectToDevicePixels(boundingBox, deviceScaleFactor)];
        });
        [_highlightView setFrames:rects.get() boundaryRect:_page->exposedContentRect()];
    } else {
        auto quads = adoptNS([[NSMutableArray alloc] initWithCapacity:highlightedQuads.size() * 4]);
        for (auto quad : highlightedQuads) {
            quad.scale(selfScale);
            auto extendedQuad = inflateQuad(quad, minimumTapHighlightRadius);
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p1()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p2()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p3()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p4()]];
        }
        [_highlightView setQuads:quads.get() boundaryRect:_page->exposedContentRect()];
    }

    [_highlightView setCornerRadii:@[
        nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.topLeftRadius, selfScale),
        nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.topRightRadius, selfScale),
        nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.bottomLeftRadius, selfScale),
        nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.bottomRightRadius, selfScale),
    ]];
}

- (void)_showTapHighlight
{
    auto shouldPaintTapHighlight = [&](const WebCore::FloatRect& rect) {
#if PLATFORM(MACCATALYST)
        UNUSED_PARAM(rect);
        return NO;
#else
        if (_tapHighlightInformation.nodeHasBuiltInClickHandling)
            return true;

        static const float highlightPaintThreshold = 0.3; // 30%
        float highlightArea = 0;
        for (auto highlightQuad : _tapHighlightInformation.quads) {
            auto boundingBox = highlightQuad.boundingBox();
            highlightArea += boundingBox.area(); 
            if (boundingBox.width() > (rect.width() * highlightPaintThreshold) || boundingBox.height() > (rect.height() * highlightPaintThreshold))
                return false;
        }
        return highlightArea < rect.area() * highlightPaintThreshold;
#endif
    };

    if (!shouldPaintTapHighlight(_page->unobscuredContentRect()) && !_showDebugTapHighlightsForFastClicking)
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

- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling
{
    if (!_isTapHighlightIDValid || _latestTapID != requestID)
        return;

    if (self._hasFocusedElement && _positionInformation.elementContext && _positionInformation.elementContext->isSameElement(_focusedElementInformation.elementContext))
        return;

    _isTapHighlightIDValid = NO;

    _tapHighlightInformation.quads = highlightedQuads;
    _tapHighlightInformation.topLeftRadius = topLeftRadius;
    _tapHighlightInformation.topRightRadius = topRightRadius;
    _tapHighlightInformation.bottomLeftRadius = bottomLeftRadius;
    _tapHighlightInformation.bottomRightRadius = bottomRightRadius;
    _tapHighlightInformation.nodeHasBuiltInClickHandling = nodeHasBuiltInClickHandling;
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
    if (_latestTapID != requestID)
        return;

    [self _setDoubleTapGesturesEnabled:NO];
}

- (void)_handleSmartMagnificationInformationForPotentialTap:(uint64_t)requestID renderRect:(const WebCore::FloatRect&)renderRect fitEntireRect:(BOOL)fitEntireRect viewportMinimumScale:(double)viewportMinimumScale viewportMaximumScale:(double)viewportMaximumScale nodeIsRootLevel:(BOOL)nodeIsRootLevel
{
    const auto& preferences = _page->preferences();

    ASSERT(preferences.fasterClicksEnabled());
    if (!_potentialTapInProgress)
        return;

    // We check both the system preference and the page preference, because we only want this
    // to apply in "desktop" mode.
    if (preferences.preferFasterClickOverDoubleTap() && _page->preferFasterClickOverDoubleTap()) {
        RELEASE_LOG(ViewGestures, "Potential tap found an element and fast taps are preferred. Trigger click. (%p)", self);
        if (preferences.zoomOnDoubleTapWhenRoot() && nodeIsRootLevel) {
            RELEASE_LOG(ViewGestures, "The click handler was on a root-level element, so don't disable double-tap. (%p)", self);
            return;
        }

        if (preferences.alwaysZoomOnDoubleTap()) {
            RELEASE_LOG(ViewGestures, "DTTZ is forced on, so don't disable double-tap. (%p)", self);
            return;
        }

        RELEASE_LOG(ViewGestures, "Give preference to click by disabling double-tap. (%p)", self);
        [self _setDoubleTapGesturesEnabled:NO];
        return;
    }

    auto targetScale = _smartMagnificationController->zoomFactorForTargetRect(renderRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale);

    auto initialScale = [self _initialScaleFactor];
    if (std::min(targetScale, initialScale) / std::max(targetScale, initialScale) > fasterTapSignificantZoomThreshold) {
        RELEASE_LOG(ViewGestures, "Potential tap would not cause a significant zoom. Trigger click. (%p)", self);
        [self _setDoubleTapGesturesEnabled:NO];
        return;
    }
    RELEASE_LOG(ViewGestures, "Potential tap may cause significant zoom. Wait. (%p)", self);
}

- (void)_cancelLongPressGestureRecognizer
{
    [_highlightLongPressGestureRecognizer cancel];
}

- (void)_cancelTouchEventGestureRecognizer
{
#if HAVE(CANCEL_WEB_TOUCH_EVENTS_GESTURE)
    [_touchEventGestureRecognizer cancel];
#endif
}

- (void)_didScroll
{
    [self _cancelLongPressGestureRecognizer];
    [self _cancelInteraction];
}

- (void)_scrollingNodeScrollingWillBegin
{
    [_textInteractionAssistant willStartScrollingOverflow];
}

- (void)_scrollingNodeScrollingDidEnd
{
    // If scrolling ends before we've received a selection update,
    // we postpone showing the selection until the update is received.
    if (!_selectionNeedsUpdate) {
        _shouldRestoreSelection = YES;
        return;
    }
    [self _updateChangedSelection];
    [_textInteractionAssistant didEndScrollingOverflow];
}

- (BOOL)shouldShowAutomaticKeyboardUI
{
    if (_focusedElementInformation.inputMode == WebCore::InputMode::None)
        return NO;

    return [self _shouldShowAutomaticKeyboardUIIgnoringInputMode];
}

- (BOOL)_shouldShowAutomaticKeyboardUIIgnoringInputMode
{
    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::None:
    case WebKit::InputType::Drawing:
    case WebKit::InputType::Date:
    case WebKit::InputType::Month:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Time:
        return NO;
    case WebKit::InputType::Select:
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
#endif
    default:
        return !_focusedElementInformation.isReadOnly;
    }
    return NO;
}

#if USE(UIKIT_KEYBOARD_ADDITIONS)
- (BOOL)_disableAutomaticKeyboardUI
{
    // Always enable automatic keyboard UI if we are not the first responder to avoid
    // interfering with other focused views (e.g. Find-in-page).
    return [self isFirstResponder] && ![self shouldShowAutomaticKeyboardUI];
}
#endif

- (BOOL)_requiresKeyboardWhenFirstResponder
{
    // FIXME: We should add the logic to handle keyboard visibility during focus redirects.
    return [self _shouldShowAutomaticKeyboardUIIgnoringInputMode]
#if USE(UIKIT_KEYBOARD_ADDITIONS)
        || _seenHardwareKeyDownInNonEditableElement
#endif
        ;
}

- (BOOL)_requiresKeyboardResetOnReload
{
    return YES;
}

- (void)_zoomToRevealFocusedElement
{
    if (_suppressSelectionAssistantReasons || _textInteractionIsHappening)
        return;

    // In case user scaling is force enabled, do not use that scaling when zooming in with an input field.
    // Zooming above the page's default scale factor should only happen when the user performs it.
    [self _zoomToFocusRect:_focusedElementInformation.interactionRect
        selectionRect:_didAccessoryTabInitiateFocus ? WebCore::FloatRect() : rectToRevealWhenZoomingToFocusedElement(_focusedElementInformation, _page->editorState())
        insideFixed:_focusedElementInformation.insideFixedPosition
        fontSize:_focusedElementInformation.nodeFontSize
        minimumScale:_focusedElementInformation.minimumScaleFactor
        maximumScale:_focusedElementInformation.maximumScaleFactorIgnoringAlwaysScalable
        allowScaling:_focusedElementInformation.allowsUserScalingIgnoringAlwaysScalable && !WebKit::currentUserInterfaceIdiomIsPad()
        forceScroll:[self requiresAccessoryView]];
}

- (UIView *)inputView
{
    return [_webView inputView];
}

- (UIView *)inputViewForWebView
{
    if (!self._hasFocusedElement)
        return nil;

    if (_inputPeripheral) {
        // FIXME: UIKit may invoke -[WKContentView inputView] at any time when WKContentView is the first responder;
        // as such, it doesn't make sense to change the enclosing scroll view's zoom scale and content offset to reveal
        // the focused element here. It seems this behavior was added to match logic in legacy WebKit (refer to
        // UIWebBrowserView). Instead, we should find the places where we currently assume that UIKit (or other clients)
        // invoke -inputView to zoom to the focused element, and either surface SPI for clients to zoom to the focused
        // element, or explicitly trigger the zoom from WebKit.
        // For instance, one use case that currently relies on this detail is adjusting the zoom scale and viewport upon
        // rotation, when a select element is focused. See <https://webkit.org/b/192878> for more information.
        [self _zoomToRevealFocusedElement];

        [self _updateAccessory];
    }

    if (UIView *customInputView = [_formInputSession customInputView])
        return customInputView;

#if ENABLE(DATALIST_ELEMENT)
    if (_dataListTextSuggestionsInputView)
        return _dataListTextSuggestionsInputView.get();
#endif

    return [_inputPeripheral assistantView];
}

- (CGRect)_selectionClipRect
{
    if (!self._hasFocusedElement)
        return CGRectNull;

    if (_page->waitingForPostLayoutEditorStateUpdateAfterFocusingElement())
        return _focusedElementInformation.interactionRect;

    return _page->editorState().postLayoutData().focusedElementRect;
}

static BOOL isBuiltInScrollViewGestureRecognizer(UIGestureRecognizer *recognizer)
{
    return ([recognizer isKindOfClass:NSClassFromString(@"UIScrollViewPanGestureRecognizer")]
        || [recognizer isKindOfClass:NSClassFromString(@"UIScrollViewPinchGestureRecognizer")]
        || [recognizer isKindOfClass:NSClassFromString(@"UIScrollViewKnobLongPressGestureRecognizer")]
        );
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer canPreventGestureRecognizer:(UIGestureRecognizer *)preventedGestureRecognizer
{
    // A long-press gesture can not be recognized while panning, but a pan can be recognized
    // during a long-press gesture.
    BOOL shouldNotPreventScrollViewGestures = preventingGestureRecognizer == _highlightLongPressGestureRecognizer || preventingGestureRecognizer == _longPressGestureRecognizer;
    
    return !(shouldNotPreventScrollViewGestures && isBuiltInScrollViewGestureRecognizer(preventedGestureRecognizer));
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)preventedGestureRecognizer canBePreventedByGestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer {
    // Don't allow the highlight to be prevented by a selection gesture. Press-and-hold on a link should highlight the link, not select it.
    bool isForcePressGesture = NO;
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    isForcePressGesture = (preventingGestureRecognizer == _textInteractionAssistant.get().forcePressGesture);
#endif
#if PLATFORM(MACCATALYST)
    if ((preventingGestureRecognizer == _textInteractionAssistant.get().loupeGesture) && (preventedGestureRecognizer == _highlightLongPressGestureRecognizer || preventedGestureRecognizer == _longPressGestureRecognizer || preventedGestureRecognizer == _textInteractionAssistant.get().forcePressGesture))
        return YES;
#endif
    
    if ((preventingGestureRecognizer == _textInteractionAssistant.get().loupeGesture || isForcePressGesture) && (preventedGestureRecognizer == _highlightLongPressGestureRecognizer || preventedGestureRecognizer == _longPressGestureRecognizer))
        return NO;

    return YES;
}

static inline bool isSamePair(UIGestureRecognizer *a, UIGestureRecognizer *b, UIGestureRecognizer *x, UIGestureRecognizer *y)
{
    return (a == x && b == y) || (b == x && a == y);
}

static Class tapAndAHalfRecognizerClass()
{
    static dispatch_once_t onceToken;
    static Class theClass;
    dispatch_once(&onceToken, ^{
        theClass = NSClassFromString(@"UITapAndAHalfRecognizer");
    });
    return theClass;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer*)otherGestureRecognizer
{
#if ENABLE(IOS_TOUCH_EVENTS)
    for (WKDeferringGestureRecognizer *gesture in self._deferringGestureRecognizers) {
        if (isSamePair(gestureRecognizer, otherGestureRecognizer, _touchEventGestureRecognizer.get(), gesture))
            return YES;
    }
#endif

    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class] && [otherGestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _longPressGestureRecognizer.get()))
        return YES;

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    if ([gestureRecognizer isKindOfClass:[WKMouseGestureRecognizer class]] || [otherGestureRecognizer isKindOfClass:[WKMouseGestureRecognizer class]])
        return YES;
#endif

#if PLATFORM(MACCATALYST)
    if (isSamePair(gestureRecognizer, otherGestureRecognizer, [_textInteractionAssistant loupeGesture], [_textInteractionAssistant forcePressGesture]))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _singleTapGestureRecognizer.get(), [_textInteractionAssistant loupeGesture]))
        return YES;

    if (([gestureRecognizer isKindOfClass:[_UILookupGestureRecognizer class]] && [otherGestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]]) || ([otherGestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]] && [gestureRecognizer isKindOfClass:[_UILookupGestureRecognizer class]]))
        return YES;
#endif // PLATFORM(MACCATALYST)

    if (gestureRecognizer == _highlightLongPressGestureRecognizer.get() || otherGestureRecognizer == _highlightLongPressGestureRecognizer.get()) {
        auto forcePressGesture = [_textInteractionAssistant forcePressGesture];
        if (gestureRecognizer == forcePressGesture || otherGestureRecognizer == forcePressGesture)
            return YES;

        auto loupeGesture = [_textInteractionAssistant loupeGesture];
        if (gestureRecognizer == loupeGesture || otherGestureRecognizer == loupeGesture)
            return YES;

        if ([gestureRecognizer isKindOfClass:tapAndAHalfRecognizerClass()] || [otherGestureRecognizer isKindOfClass:tapAndAHalfRecognizerClass()])
            return YES;
    }

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _singleTapGestureRecognizer.get(), [_textInteractionAssistant singleTapGesture]))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _singleTapGestureRecognizer.get(), _nonBlockingDoubleTapGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _nonBlockingDoubleTapGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _previewSecondaryGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _highlightLongPressGestureRecognizer.get(), _previewGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _nonBlockingDoubleTapGestureRecognizer.get(), _doubleTapGestureRecognizerForDoubleClick.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _doubleTapGestureRecognizer.get(), _doubleTapGestureRecognizerForDoubleClick.get()))
        return YES;

    return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if (gestureRecognizer == _touchEventGestureRecognizer && [_webView _isNavigationSwipeGestureRecognizer:otherGestureRecognizer])
        return YES;

    if ([otherGestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)otherGestureRecognizer shouldDeferGestureRecognizer:gestureRecognizer];

    return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)gestureRecognizer shouldDeferGestureRecognizer:otherGestureRecognizer];

    return NO;
}

- (void)_showImageSheet
{
    [_actionSheetAssistant showImageSheet];
}

- (void)_showAttachmentSheet
{
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if (![uiDelegate respondsToSelector:@selector(_webView:showCustomSheetForElement:)])
        return;

    auto element = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeAttachment URL:(NSURL *)_positionInformation.url imageURL:(NSURL *)_positionInformation.imageURL location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:nil]);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [uiDelegate _webView:self.webView showCustomSheetForElement:element.get()];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)_showLinkSheet
{
    [_actionSheetAssistant showLinkSheet];
}

- (void)_showDataDetectorsUI
{
    [self _showDataDetectorsUIForPositionInformation:_positionInformation];
}

- (void)_showDataDetectorsUIForPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation
{
    [_actionSheetAssistant showDataDetectorsUIForPositionInformation:positionInformation];
}

- (SEL)_actionForLongPressFromPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation
{
    if (!self.webView.configuration._longPressActionsEnabled)
        return nil;

    if (!positionInformation.touchCalloutEnabled)
        return nil;

    if (positionInformation.isImage)
        return @selector(_showImageSheet);

    if (positionInformation.isLink) {
#if ENABLE(DATA_DETECTION)
        if (WebCore::DataDetection::canBePresentedByDataDetectors(positionInformation.url))
            return @selector(_showDataDetectorsUI);
#endif
        return @selector(_showLinkSheet);
    }
    if (positionInformation.isAttachment)
        return @selector(_showAttachmentSheet);

    return nil;
}

- (SEL)_actionForLongPress
{
    return [self _actionForLongPressFromPositionInformation:_positionInformation];
}

- (void)doAfterPositionInformationUpdate:(void (^)(WebKit::InteractionInformationAtPosition))action forRequest:(WebKit::InteractionInformationRequest)request
{
    if ([self _currentPositionInformationIsValidForRequest:request]) {
        // If the most recent position information is already valid, invoke the given action block immediately.
        action(_positionInformation);
        return;
    }

    _pendingPositionInformationHandlers.append(InteractionInformationRequestAndCallback(request, action));

    if (![self _hasValidOutstandingPositionInformationRequest:request])
        [self requestAsynchronousPositionInformationUpdate:request];
}

- (BOOL)ensurePositionInformationIsUpToDate:(WebKit::InteractionInformationRequest)request
{
    if ([self _currentPositionInformationIsValidForRequest:request])
        return YES;

    if (!_page->hasRunningProcess())
        return NO;

    auto* connection = _page->process().connection();
    if (!connection)
        return NO;

    if (_isWaitingOnPositionInformation)
        return NO;
    
    _isWaitingOnPositionInformation = YES;
    if (![self _hasValidOutstandingPositionInformationRequest:request])
        [self requestAsynchronousPositionInformationUpdate:request];
    bool receivedResponse = connection->waitForAndDispatchImmediately<Messages::WebPageProxy::DidReceivePositionInformation>(_page->webPageID(), 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
    _hasValidPositionInformation = receivedResponse && _positionInformation.canBeValid;
    return _hasValidPositionInformation;
}

- (void)requestAsynchronousPositionInformationUpdate:(WebKit::InteractionInformationRequest)request
{
    if ([self _currentPositionInformationIsValidForRequest:request])
        return;

    _outstandingPositionInformationRequest = request;

    _page->requestPositionInformation(request);
}

- (BOOL)_currentPositionInformationIsValidForRequest:(const WebKit::InteractionInformationRequest&)request
{
    return _hasValidPositionInformation && _positionInformation.request.isValidForRequest(request);
}

- (BOOL)_hasValidOutstandingPositionInformationRequest:(const WebKit::InteractionInformationRequest&)request
{
    return _outstandingPositionInformationRequest && _outstandingPositionInformationRequest->isValidForRequest(request);
}

- (BOOL)_currentPositionInformationIsApproximatelyValidForRequest:(const WebKit::InteractionInformationRequest&)request radiusForApproximation:(int)radius
{
    return _hasValidPositionInformation && _positionInformation.request.isApproximatelyValidForRequest(request, radius);
}

- (void)_invokeAndRemovePendingHandlersValidForCurrentPositionInformation
{
    ASSERT(_hasValidPositionInformation);

    // FIXME: We need to clean up these handlers in the event that we are not able to collect data, or if the WebProcess crashes.
    ++_positionInformationCallbackDepth;
    auto updatedPositionInformation = _positionInformation;

    for (size_t index = 0; index < _pendingPositionInformationHandlers.size(); ++index) {
        auto requestAndHandler = _pendingPositionInformationHandlers[index];
        if (!requestAndHandler)
            continue;

        if (![self _currentPositionInformationIsValidForRequest:requestAndHandler->first])
            continue;

        _pendingPositionInformationHandlers[index] = WTF::nullopt;

        if (requestAndHandler->second)
            requestAndHandler->second(updatedPositionInformation);
    }

    if (--_positionInformationCallbackDepth)
        return;

    for (int index = _pendingPositionInformationHandlers.size() - 1; index >= 0; --index) {
        if (!_pendingPositionInformationHandlers[index])
            _pendingPositionInformationHandlers.remove(index);
    }
}

#if ENABLE(DATA_DETECTION)
- (NSArray *)_dataDetectionResults
{
    return _page->dataDetectionResults();
}
#endif

- (NSArray<NSValue *> *)_uiTextSelectionRects
{
    NSMutableArray *textSelectionRects = [NSMutableArray array];

    if (_textInteractionAssistant) {
        for (WKTextSelectionRect *selectionRect in [_textInteractionAssistant valueForKeyPath:@"selectionView.selection.selectionRects"])
            [textSelectionRects addObject:[NSValue valueWithCGRect:selectionRect.rect]];
    }

    return textSelectionRects;
}

- (BOOL)_pointIsInsideSelectionRect:(CGPoint)point outBoundingRect:(WebCore::FloatRect *)outBoundingRect
{
    BOOL pointIsInSelectionRect = NO;
    for (auto& rectInfo : _lastSelectionDrawingInfo.selectionRects) {
        auto rect = rectInfo.rect();
        if (rect.isEmpty())
            continue;

        pointIsInSelectionRect |= rect.contains(WebCore::roundedIntPoint(point));
        if (outBoundingRect)
            outBoundingRect->unite(rect);
    }
    return pointIsInSelectionRect;
}

- (BOOL)_shouldToggleSelectionCommandsAfterTapAt:(CGPoint)point
{
    if (_lastSelectionDrawingInfo.selectionRects.isEmpty())
        return NO;

    WebCore::FloatRect selectionBoundingRect;
    BOOL pointIsInSelectionRect = [self _pointIsInsideSelectionRect:point outBoundingRect:&selectionBoundingRect];
    WebCore::FloatRect unobscuredContentRect = self.unobscuredContentRect;
    selectionBoundingRect.intersect(unobscuredContentRect);

    float unobscuredArea = unobscuredContentRect.area();
    float ratioForConsideringSelectionRectToCoverVastMajorityOfContent = 0.75;
    if (!unobscuredArea || selectionBoundingRect.area() / unobscuredArea > ratioForConsideringSelectionRectToCoverVastMajorityOfContent)
        return NO;

    return pointIsInSelectionRect;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    CGPoint point = [gestureRecognizer locationInView:self];

    if (gestureRecognizer == _stylusSingleTapGestureRecognizer)
        return self.webView._stylusTapGestureShouldCreateEditableImage;

    auto isInterruptingDecelerationForScrollViewOrAncestor = [&] (UIScrollView *scrollView) {
        UIScrollView *mainScroller = self.webView.scrollView;
        UIView *view = scrollView ?: mainScroller;
        while (view) {
            if ([view isKindOfClass:UIScrollView.class] && [(UIScrollView *)view _isInterruptingDeceleration])
                return YES;

            if (mainScroller == view)
                break;

            view = view.superview;
        }
        return NO;
    };

    if (gestureRecognizer == _singleTapGestureRecognizer) {
        if ([self _shouldToggleSelectionCommandsAfterTapAt:point])
            return NO;
        return !isInterruptingDecelerationForScrollViewOrAncestor([_singleTapGestureRecognizer lastTouchedScrollView]);
    }

    if (gestureRecognizer == _doubleTapGestureRecognizerForDoubleClick) {
        // Do not start the double-tap-for-double-click gesture recognizer unless we've got a dblclick event handler on the node at the tap location.
        WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
        if ([self _currentPositionInformationIsApproximatelyValidForRequest:request radiusForApproximation:[_doubleTapGestureRecognizerForDoubleClick allowableMovement]])
            return _positionInformation.nodeAtPositionHasDoubleClickHandler;
        if (![self ensurePositionInformationIsUpToDate:request])
            return NO;
        return _positionInformation.nodeAtPositionHasDoubleClickHandler;
    }

    if (gestureRecognizer == _highlightLongPressGestureRecognizer
        || gestureRecognizer == _doubleTapGestureRecognizer
        || gestureRecognizer == _nonBlockingDoubleTapGestureRecognizer
        || gestureRecognizer == _twoFingerDoubleTapGestureRecognizer) {

        if (self._hasFocusedElement) {
            // Request information about the position with sync message.
            // If the focused element is the same, prevent the gesture.
            if (![self ensurePositionInformationIsUpToDate:WebKit::InteractionInformationRequest(WebCore::roundedIntPoint(point))])
                return NO;
            if (_positionInformation.elementContext && _positionInformation.elementContext->isSameElement(_focusedElementInformation.elementContext))
                return NO;
        }
    }

    if (gestureRecognizer == _highlightLongPressGestureRecognizer) {
        if (isInterruptingDecelerationForScrollViewOrAncestor([_highlightLongPressGestureRecognizer lastTouchedScrollView]))
            return NO;

        if (self._hasFocusedElement) {
            // This is a different element than the focused one.
            // Prevent the gesture if there is no node.
            // Allow the gesture if it is a node that wants highlight or if there is an action for it.
            if (!_positionInformation.isElement)
                return NO;
            return [self _actionForLongPress] != nil;
        }
        // We still have no idea about what is at the location.
        // Send an async message to find out.
        _hasValidPositionInformation = NO;
        WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));

        // If 3D Touch is enabled, asynchronously collect snapshots in the hopes that
        // they'll arrive before we have to synchronously request them in
        // _interactionShouldBeginFromPreviewItemController.
        if (self.traitCollection.forceTouchCapability == UIForceTouchCapabilityAvailable) {
            request.includeSnapshot = true;
            request.includeLinkIndicator = true;
            request.linkIndicatorShouldHaveLegacyMargins = !self._shouldUseContextMenus;
        }

        [self requestAsynchronousPositionInformationUpdate:request];
        return YES;

    }

    if (gestureRecognizer == _longPressGestureRecognizer) {
        // Use the information retrieved with one of the previous calls
        // to gestureRecognizerShouldBegin.
        // Force a sync call if not ready yet.
        WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
        if (![self ensurePositionInformationIsUpToDate:request])
            return NO;

        if (self._hasFocusedElement) {
            // Prevent the gesture if it is the same node.
            if (_positionInformation.elementContext && _positionInformation.elementContext->isSameElement(_focusedElementInformation.elementContext))
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

- (BOOL)canShowNonEmptySelectionView
{
    if (_suppressSelectionAssistantReasons)
        return NO;

    auto& state = _page->editorState();
    return !state.isMissingPostLayoutData && !state.selectionIsNone;
}

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point
{
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return NO;

    if (_suppressSelectionAssistantReasons)
        return NO;

    if (_inspectorNodeSearchEnabled)
        return NO;

    WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
    if (![self ensurePositionInformationIsUpToDate:request])
        return NO;

    return _positionInformation.isSelectable;
}

- (BOOL)pointIsNearMarkedText:(CGPoint)point
{
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return NO;

    if (_suppressSelectionAssistantReasons)
        return NO;

    WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
    if (![self ensurePositionInformationIsUpToDate:request])
        return NO;
    return _positionInformation.isNearMarkedText;
}

- (BOOL)textInteractionGesture:(UIWKGestureType)gesture shouldBeginAtPoint:(CGPoint)point
{
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return NO;

    if (_domPasteRequestHandler)
        return NO;

    if (_suppressSelectionAssistantReasons)
        return NO;
    
    if (!self.isFocusingElement) {
        if (gesture == UIWKGestureDoubleTap) {
            // Don't allow double tap text gestures in noneditable content.
            return NO;
        }

        if (gesture == UIWKGestureOneFingerTap) {
            ASSERT(_suppressNonEditableSingleTapTextInteractionCount >= 0);
            if (_suppressNonEditableSingleTapTextInteractionCount > 0)
                return NO;

            switch ([_textInteractionAssistant loupeGesture].state) {
            case UIGestureRecognizerStateBegan:
            case UIGestureRecognizerStateChanged:
            case UIGestureRecognizerStateEnded: {
                // Avoid handling one-finger taps while the web process is processing certain selection changes.
                // This works around a scenario where UIKeyboardImpl blocks the main thread while handling a one-
                // finger tap, which subsequently prevents the UI process from handling any incoming IPC messages.
                return NO;
            }
            default:
                if (!_page->editorState().selectionIsRange)
                    return NO;
                return [self _pointIsInsideSelectionRect:point outBoundingRect:nil];
            }
        }
    }

    WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
    if (![self ensurePositionInformationIsUpToDate:request])
        return NO;

#if ENABLE(DATALIST_ELEMENT)
    if (_positionInformation.preventTextInteraction)
        return NO;
#endif

    // If we're currently focusing an editable element, only allow the selection to move within that focused element.
    if (self.isFocusingElement)
        return _positionInformation.elementContext && _positionInformation.elementContext->isSameElement(_focusedElementInformation.elementContext);

    if (_positionInformation.prefersDraggingOverTextSelection)
        return NO;

    // If we're selecting something, don't activate highlight.
    if (gesture == UIWKGestureLoupe && [self hasSelectablePositionAtPoint:point])
        [self _cancelLongPressGestureRecognizer];
    
    // Otherwise, if we're using a text interaction assistant outside of editing purposes (e.g. the selection mode
    // is character granularity) then allow text selection.
    return YES;
}

- (NSArray *)webSelectionRectsForSelectionRects:(const Vector<WebCore::SelectionRect>&)selectionRects
{
    if (selectionRects.isEmpty())
        return nil;

    return createNSArray(selectionRects, [] (auto& coreRect) {
        auto webRect = [WebSelectionRect selectionRect];
        webRect.rect = coreRect.rect();
        webRect.writingDirection = coreRect.direction() == WebCore::TextDirection::LTR ? WKWritingDirectionLeftToRight : WKWritingDirectionRightToLeft;
        webRect.isLineBreak = coreRect.isLineBreak();
        webRect.isFirstOnLine = coreRect.isFirstOnLine();
        webRect.isLastOnLine = coreRect.isLastOnLine();
        webRect.containsStart = coreRect.containsStart();
        webRect.containsEnd = coreRect.containsEnd();
        webRect.isInFixedPosition = coreRect.isInFixedPosition();
        webRect.isHorizontal = coreRect.isHorizontal();
        return webRect;
    }).autorelease();
}

- (NSArray *)webSelectionRects
{
    if (_page->editorState().isMissingPostLayoutData || _page->editorState().selectionIsNone)
        return nil;
    const auto& selectionRects = _page->editorState().postLayoutData().selectionRects;
    return [self webSelectionRectsForSelectionRects:selectionRects];
}

- (void)_highlightLongPressRecognized:(UILongPressGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _highlightLongPressGestureRecognizer);
    [self _resetIsDoubleTapPending];

    _lastInteractionLocation = gestureRecognizer.startPoint;

    switch ([gestureRecognizer state]) {
    case UIGestureRecognizerStateBegan:
        _longPressCanClick = YES;
        cancelPotentialTapIfNecessary(self);
        _page->tapHighlightAtPosition([gestureRecognizer startPoint], ++_latestTapID);
        _isTapHighlightIDValid = YES;
        break;
    case UIGestureRecognizerStateEnded:
        if (_longPressCanClick && _positionInformation.isElement) {
            [self _attemptClickAtLocation:gestureRecognizer.startPoint modifierFlags:gestureRecognizerModifierFlags(gestureRecognizer)];
            [self _finishInteraction];
        } else
            [self _cancelInteraction];
        _longPressCanClick = NO;
        break;
    case UIGestureRecognizerStateCancelled:
        [self _cancelInteraction];
        _longPressCanClick = NO;
        break;
    default:
        break;
    }
}

- (void)_doubleTapRecognizedForDoubleClick:(UITapGestureRecognizer *)gestureRecognizer
{
    _page->handleDoubleTapForDoubleClickAtPoint(WebCore::IntPoint(gestureRecognizer.location), WebKit::webEventModifierFlags(gestureRecognizerModifierFlags(gestureRecognizer)), _layerTreeTransactionIdAtLastInteractionStart);
}

- (void)_twoFingerSingleTapGestureRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    _isTapHighlightIDValid = YES;
    _isExpectingFastSingleTapCommit = YES;
    _page->handleTwoFingerTapAtPoint(WebCore::roundedIntPoint(gestureRecognizer.centroid), WebKit::webEventModifierFlags(gestureRecognizerModifierFlags(gestureRecognizer) | UIKeyModifierCommand), ++_latestTapID);
}

- (void)_stylusSingleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(self.webView._stylusTapGestureShouldCreateEditableImage);
    ASSERT(gestureRecognizer == _stylusSingleTapGestureRecognizer);
    _page->handleStylusSingleTapAtPoint(WebCore::roundedIntPoint(gestureRecognizer.location), ++_latestTapID);
}

- (void)_longPressRecognized:(UILongPressGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _longPressGestureRecognizer);
    [self _resetIsDoubleTapPending];
    [self _cancelTouchEventGestureRecognizer];
    _page->didRecognizeLongPress();

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
    if (self.webView._allowsDoubleTapGestures) {
        RELEASE_LOG(ViewGestures, "ending potential tap - double taps are back. (%p)", self);

        [self _setDoubleTapGesturesEnabled:YES];
    }

    RELEASE_LOG(ViewGestures, "Ending potential tap. (%p)", self);

    _potentialTapInProgress = NO;
}

- (void)_singleTapIdentified:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);
    ASSERT(!_potentialTapInProgress);
    [self _resetIsDoubleTapPending];

    bool shouldRequestMagnificationInformation = _page->preferences().fasterClicksEnabled();
    if (shouldRequestMagnificationInformation)
        RELEASE_LOG(ViewGestures, "Single tap identified. Request details on potential zoom. (%p)", self);

    _page->potentialTapAtPosition(gestureRecognizer.location, shouldRequestMagnificationInformation, ++_latestTapID);
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
    if (auto* singleTapTouchIdentifier = [_singleTapGestureRecognizer lastActiveTouchIdentifier]) {
        WebCore::PointerID pointerId = [singleTapTouchIdentifier unsignedIntValue];
        if (m_commitPotentialTapPointerId != pointerId)
            _page->touchWithIdentifierWasRemoved(pointerId);
    }
    auto actionsToPerform = std::exchange(_actionsToPerformAfterResettingSingleTapGestureRecognizer, { });
    for (const auto& action : actionsToPerform)
        action();
}

- (void)_doubleTapDidFail:(UITapGestureRecognizer *)gestureRecognizer
{
    RELEASE_LOG(ViewGestures, "Double tap was not recognized. (%p)", self);
    ASSERT(gestureRecognizer == _doubleTapGestureRecognizer);
}

- (void)_commitPotentialTapFailed
{
    _page->touchWithIdentifierWasRemoved(m_commitPotentialTapPointerId);
    m_commitPotentialTapPointerId = 0;

    [self _cancelInteraction];
    
    [self _resetInputViewDeferral];
}

- (void)_didNotHandleTapAsClick:(const WebCore::IntPoint&)point
{
    [self _resetInputViewDeferral];

    if (!_isDoubleTapPending)
        return;

    _smartMagnificationController->handleSmartMagnificationGesture(_lastInteractionLocation);
    _isDoubleTapPending = NO;
}

- (void)_didCompleteSyntheticClick
{
    _page->touchWithIdentifierWasRemoved(m_commitPotentialTapPointerId);
    m_commitPotentialTapPointerId = 0;

    RELEASE_LOG(ViewGestures, "Synthetic click completed. (%p)", self);
    [self _resetInputViewDeferral];
}

- (void)_singleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);

    if (![self isFirstResponder])
        [self becomeFirstResponder];

    ASSERT(_potentialTapInProgress);

    _lastInteractionLocation = gestureRecognizer.location;

    [self _endPotentialTapAndEnableDoubleTapGesturesIfNecessary];

    if (_hasTapHighlightForPotentialTap) {
        [self _showTapHighlight];
        _hasTapHighlightForPotentialTap = NO;
    }

    [_inputPeripheral endEditing];

    RELEASE_LOG(ViewGestures, "Single tap recognized - commit potential tap (%p)", self);

    WebCore::PointerID pointerId = WebCore::mousePointerID;
    if (auto* singleTapTouchIdentifier = [_singleTapGestureRecognizer lastActiveTouchIdentifier]) {
        pointerId = [singleTapTouchIdentifier unsignedIntValue];
        m_commitPotentialTapPointerId = pointerId;
    }
    _page->commitPotentialTap(WebKit::webEventModifierFlags(gestureRecognizerModifierFlags(gestureRecognizer)), _layerTreeTransactionIdAtLastInteractionStart, pointerId);

    if (!_isExpectingFastSingleTapCommit)
        [self _finishInteraction];
}

- (void)_doubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    RELEASE_LOG(ViewGestures, "Identified a double tap (%p)", self);

    [self _resetIsDoubleTapPending];
    _lastInteractionLocation = gestureRecognizer.location;

    _smartMagnificationController->handleSmartMagnificationGesture(gestureRecognizer.location);
}

- (void)_resetIsDoubleTapPending
{
    _isDoubleTapPending = NO;
}

- (void)_nonBlockingDoubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    _lastInteractionLocation = gestureRecognizer.location;
    _isDoubleTapPending = YES;
}

- (void)_twoFingerDoubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    [self _resetIsDoubleTapPending];
    _lastInteractionLocation = gestureRecognizer.location;

    _smartMagnificationController->handleResetMagnificationGesture(gestureRecognizer.location);
}

- (void)_attemptClickAtLocation:(CGPoint)location modifierFlags:(UIKeyModifierFlags)modifierFlags
{
    if (![self isFirstResponder])
        [self becomeFirstResponder];

    [_inputPeripheral endEditing];
    _page->handleTap(location, WebKit::webEventModifierFlags(modifierFlags), _layerTreeTransactionIdAtLastInteractionStart);
}

- (void)setUpTextSelectionAssistant
{
    if (!_textInteractionAssistant)
        _textInteractionAssistant = adoptNS([[UIWKTextInteractionAssistant alloc] initWithView:self]);
    else {
        // Reset the gesture recognizers in case editability has changed.
        [_textInteractionAssistant setGestureRecognizers];
    }
}

- (void)pasteWithCompletionHandler:(void (^)(void))completionHandler
{
    _page->executeEditCommand("Paste"_s, { }, [completion = makeBlockPtr(completionHandler)] {
        if (completion)
            completion();
    });
}

- (void)clearSelection
{
    [self _elementDidBlur];
    _page->clearSelection();
}

- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info
{
    _outstandingPositionInformationRequest = WTF::nullopt;
    _isWaitingOnPositionInformation = NO;

    WebKit::InteractionInformationAtPosition newInfo = info;
    newInfo.mergeCompatibleOptionalInformation(_positionInformation);

    _positionInformation = newInfo;
    _hasValidPositionInformation = _positionInformation.canBeValid;
    if (_actionSheetAssistant)
        [_actionSheetAssistant updateSheetPosition];
    [self _invokeAndRemovePendingHandlersValidForCurrentPositionInformation];
}

- (void)_willStartScrollingOrZooming
{
    if ([_textInteractionAssistant respondsToSelector:@selector(willStartScrollingOrZooming)])
        [_textInteractionAssistant willStartScrollingOrZooming];
    else
        [_textInteractionAssistant willStartScrollingOverflow];
    _page->setIsScrollingOrZooming(true);

#if PLATFORM(WATCHOS)
    [_focusedFormControlView disengageFocusedFormControlNavigation];
#endif
}

- (void)scrollViewWillStartPanOrPinchGesture
{
    _page->hideValidationMessage();

    [_keyboardScrollingAnimator willStartInteractiveScroll];

    _touchEventsCanPreventNativeGestures = NO;
}

- (void)_didEndScrollingOrZooming
{
    if (!_needsDeferredEndScrollingSelectionUpdate) {
        if ([_textInteractionAssistant respondsToSelector:@selector(didEndScrollingOrZooming)])
            [_textInteractionAssistant didEndScrollingOrZooming];
        else
            [_textInteractionAssistant didEndScrollingOverflow];
    }
    _page->setIsScrollingOrZooming(false);

    [self _resetPanningPreventionFlags];

#if PLATFORM(WATCHOS)
    [_focusedFormControlView engageFocusedFormControlNavigation];
#endif
}

- (BOOL)requiresAccessoryView
{
    if ([_formInputSession accessoryViewShouldNotShow])
        return NO;

    if ([_formInputSession customInputAccessoryView])
        return YES;

    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::None:
    case WebKit::InputType::Drawing:
    case WebKit::InputType::Date:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Month:
    case WebKit::InputType::Time:
        return NO;
    case WebKit::InputType::Text:
    case WebKit::InputType::Password:
    case WebKit::InputType::Search:
    case WebKit::InputType::Email:
    case WebKit::InputType::URL:
    case WebKit::InputType::Phone:
    case WebKit::InputType::Number:
    case WebKit::InputType::NumberPad:
    case WebKit::InputType::ContentEditable:
    case WebKit::InputType::TextArea:
    case WebKit::InputType::Select:
    case WebKit::InputType::DateTime:
    case WebKit::InputType::Week:
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
#endif
        return !WebKit::currentUserInterfaceIdiomIsPad();
    }
}

- (UITextInputAssistantItem *)inputAssistantItem
{
    return [_webView inputAssistantItem];
}

- (UITextInputAssistantItem *)inputAssistantItemForWebView
{
    return [super inputAssistantItem];
}

- (UIView *)inputAccessoryView
{
    return [_webView inputAccessoryView];
}

- (UIView *)inputAccessoryViewForWebView
{
    if (![self requiresAccessoryView])
        return nil;

    return [_formInputSession customInputAccessoryView] ?: self.formAccessoryView;
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
        [richTypes addObject:WebCore::WebArchivePboardType];
        [richTypes addObjectsFromArray:UIPasteboardTypeListImage];
        [richTypes addObjectsFromArray:plainTextTypes];
    }

    return (_page->editorState().isContentRichlyEditable) ? richTypes : plainTextTypes;
}

#define FORWARD_ACTION_TO_WKWEBVIEW(_action) \
    - (void)_action:(id)sender \
    { \
        SEL action = @selector(_action:);\
        [self _willPerformAction:action sender:sender];\
        [_webView _action:sender]; \
        [self _didPerformAction:action sender:sender];\
    }

FOR_EACH_WKCONTENTVIEW_ACTION(FORWARD_ACTION_TO_WKWEBVIEW)
FOR_EACH_PRIVATE_WKCONTENTVIEW_ACTION(FORWARD_ACTION_TO_WKWEBVIEW)

#undef FORWARD_ACTION_TO_WKWEBVIEW

- (void)_lookupForWebView:(id)sender
{
    RetainPtr<WKContentView> view = self;
    _page->getSelectionContext([view](const String& selectedText, const String& textBefore, const String& textAfter, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;
        if (!selectedText)
            return;

        auto& editorState = view->_page->editorState();
        auto& postLayoutData = editorState.postLayoutData();
        CGRect presentationRect;
        if (editorState.selectionIsRange && !postLayoutData.selectionRects.isEmpty())
            presentationRect = postLayoutData.selectionRects[0].rect();
        else
            presentationRect = postLayoutData.caretRectAtStart;
        
        String selectionContext = textBefore + selectedText + textAfter;
        NSRange selectedRangeInContext = NSMakeRange(textBefore.length(), selectedText.length());

        if (auto textSelectionAssistant = view->_textInteractionAssistant)
            [textSelectionAssistant lookup:selectionContext withRange:selectedRangeInContext fromRect:presentationRect];
    });
}

- (void)_shareForWebView:(id)sender
{
    RetainPtr<WKContentView> view = self;
    _page->getSelectionOrContentsAsString([view](const String& string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;

        if (!view->_textInteractionAssistant || !string || view->_page->editorState().isMissingPostLayoutData)
            return;

        auto& selectionRects = view->_page->editorState().postLayoutData().selectionRects;
        if (selectionRects.isEmpty())
            return;

        [view->_textInteractionAssistant showShareSheetFor:string fromRect:selectionRects.first().rect()];
    });
}

- (void)_addShortcutForWebView:(id)sender
{
    if (_textInteractionAssistant)
        [_textInteractionAssistant showTextServiceFor:[self selectedText] fromRect:_page->editorState().postLayoutData().selectionRects[0].rect()];
}

- (NSString *)selectedText
{
    return (NSString *)_page->editorState().postLayoutData().wordAtSelection;
}

- (void)makeTextWritingDirectionNaturalForWebView:(id)sender
{
    // Match platform behavior on iOS as well as legacy WebKit behavior by modifying the
    // base (paragraph) writing direction rather than the inline direction.
    _page->setBaseWritingDirection(WebCore::WritingDirection::Natural);
}

- (void)makeTextWritingDirectionLeftToRightForWebView:(id)sender
{
    _page->setBaseWritingDirection(WebCore::WritingDirection::LeftToRight);
}

- (void)makeTextWritingDirectionRightToLeftForWebView:(id)sender
{
    _page->setBaseWritingDirection(WebCore::WritingDirection::RightToLeft);
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

- (void)_promptForReplaceForWebView:(id)sender
{
    const auto& wordAtSelection = _page->editorState().postLayoutData().wordAtSelection;
    if (wordAtSelection.isEmpty())
        return;

    [_textInteractionAssistant scheduleReplacementsForText:wordAtSelection];
}

- (void)_transliterateChineseForWebView:(id)sender
{
    [_textInteractionAssistant scheduleChineseTransliterationForText:_page->editorState().postLayoutData().wordAtSelection];
}

- (void)replaceForWebView:(id)sender
{
    [[UIKeyboardImpl sharedInstance] replaceText:sender];
}

#define WEBCORE_COMMAND_FOR_WEBVIEW(command) \
    - (void)_ ## command ## ForWebView:(id)sender { _page->executeEditCommand(#command ## _s); } \
    - (void)command ## ForWebView:(id)sender { [self _ ## command ## ForWebView:sender]; }
WEBCORE_COMMAND_FOR_WEBVIEW(insertOrderedList);
WEBCORE_COMMAND_FOR_WEBVIEW(insertUnorderedList);
WEBCORE_COMMAND_FOR_WEBVIEW(insertNestedOrderedList);
WEBCORE_COMMAND_FOR_WEBVIEW(insertNestedUnorderedList);
WEBCORE_COMMAND_FOR_WEBVIEW(indent);
WEBCORE_COMMAND_FOR_WEBVIEW(outdent);
WEBCORE_COMMAND_FOR_WEBVIEW(alignLeft);
WEBCORE_COMMAND_FOR_WEBVIEW(alignRight);
WEBCORE_COMMAND_FOR_WEBVIEW(alignCenter);
WEBCORE_COMMAND_FOR_WEBVIEW(alignJustified);
WEBCORE_COMMAND_FOR_WEBVIEW(pasteAndMatchStyle);
#undef WEBCORE_COMMAND_FOR_WEBVIEW

- (void)_increaseListLevelForWebView:(id)sender
{
    _page->increaseListLevel();
}

- (void)_decreaseListLevelForWebView:(id)sender
{
    _page->decreaseListLevel();
}

- (void)_changeListTypeForWebView:(id)sender
{
    _page->changeListType();
}

- (void)_toggleStrikeThroughForWebView:(id)sender
{
    _page->executeEditCommand("StrikeThrough"_s);
}

- (void)increaseSizeForWebView:(id)sender
{
    _page->executeEditCommand("FontSizeDelta"_s, "1"_s);
}

- (void)decreaseSizeForWebView:(id)sender
{
    _page->executeEditCommand("FontSizeDelta"_s, "-1"_s);
}

- (void)_setFontForWebView:(UIFont *)font sender:(id)sender
{
    WebCore::FontChanges changes;
    changes.setFontFamily(font.familyName);
    changes.setFontName(font.fontName);
    changes.setFontSize(font.pointSize);
    changes.setBold(font.traits & UIFontTraitBold);
    changes.setItalic(font.traits & UIFontTraitItalic);
    _page->changeFont(WTFMove(changes));
}

- (void)_setFontSizeForWebView:(CGFloat)fontSize sender:(id)sender
{
    WebCore::FontChanges changes;
    changes.setFontSize(fontSize);
    _page->changeFont(WTFMove(changes));
}

- (void)_setTextColorForWebView:(UIColor *)color sender:(id)sender
{
    _page->executeEditCommand("ForeColor"_s, WebCore::Color(color.CGColor).serialized());
}

- (void)toggleStrikeThroughForWebView:(id)sender
{
    [self _toggleStrikeThroughForWebView:sender];
}

- (NSDictionary *)textStylingAtPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
    if (!position || !_page->editorState().isContentRichlyEditable)
        return nil;

    NSMutableDictionary* result = [NSMutableDictionary dictionary];

    auto typingAttributes = _page->editorState().postLayoutData().typingAttributes;
    CTFontSymbolicTraits symbolicTraits = 0;
    if (typingAttributes & WebKit::AttributeBold)
        symbolicTraits |= kCTFontBoldTrait;
    if (typingAttributes & WebKit::AttributeItalics)
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
    
    if (typingAttributes & WebKit::AttributeUnderline)
        [result setObject:@(NSUnderlineStyleSingle) forKey:NSUnderlineStyleAttributeName];

    return result;
}

- (UIColor *)insertionPointColor
{
    // On macCatalyst we need to explicitly return the color we have calculated, rather than rely on textTraits, as on macCatalyst, UIKit ignores text traits.
#if PLATFORM(MACCATALYST)
    return [self _cascadeInteractionTintColor];
#else
    return [self.textInputTraits insertionPointColor];
#endif
}

- (UIColor *)selectionBarColor
{
    return [self.textInputTraits selectionBarColor];
}

- (UIColor *)selectionHighlightColor
{
    return [self.textInputTraits selectionHighlightColor];
}

- (UIColor *)_cascadeInteractionTintColor
{
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return [UIColor clearColor];

    if (!_page->editorState().isMissingPostLayoutData) {
        WebCore::Color caretColor = _page->editorState().postLayoutData().caretColor;
        if (caretColor.isValid())
            return [UIColor colorWithCGColor:cachedCGColor(caretColor)];
    }
    return [self _inheritedInteractionTintColor];
}

- (void)_updateInteractionTintColor
{
    [_traits _setColorsToMatchTintColor:[self _cascadeInteractionTintColor]];
}

- (void)tintColorDidChange
{
    [super tintColorDidChange];

    BOOL shouldUpdateTextSelection = self.isFirstResponder && [self canShowNonEmptySelectionView];
    if (shouldUpdateTextSelection)
        [_textInteractionAssistant deactivateSelection];
    [self _updateInteractionTintColor];
    if (shouldUpdateTextSelection)
        [_textInteractionAssistant activateSelection];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender
{
    if (_domPasteRequestHandler)
        return action == @selector(paste:);

    // These are UIKit IPI selectors. We don't want to forward them to the web view.
    auto editorState = _page->editorState();
    if (action == @selector(_moveDown:withHistory:) || action == @selector(_moveLeft:withHistory:) || action == @selector(_moveRight:withHistory:)
        || action == @selector(_moveToEndOfDocument:withHistory:) || action == @selector(_moveToEndOfLine:withHistory:) || action == @selector(_moveToEndOfParagraph:withHistory:)
        || action == @selector(_moveToEndOfWord:withHistory:) || action == @selector(_moveToStartOfDocument:withHistory:) || action == @selector(_moveToStartOfLine:withHistory:)
        || action == @selector(_moveToStartOfParagraph:withHistory:) || action == @selector(_moveToStartOfWord:withHistory:) || action == @selector(_moveUp:withHistory:))
        return !editorState.selectionIsNone;

    if (action == @selector(_deleteByWord) || action == @selector(_deleteForwardAndNotify:) || action == @selector(_deleteToEndOfParagraph) || action == @selector(_deleteToStartOfLine)
        || action == @selector(_transpose))
        return editorState.isContentEditable;

    return [_webView canPerformAction:action withSender:sender];
}

- (BOOL)canPerformActionForWebView:(SEL)action withSender:(id)sender
{
    if (_domPasteRequestHandler)
        return action == @selector(paste:);

    if (action == @selector(_nextAccessoryTab:))
        return self._hasFocusedElement && _focusedElementInformation.hasNextNode;
    if (action == @selector(_previousAccessoryTab:))
        return self._hasFocusedElement && _focusedElementInformation.hasPreviousNode;

    auto editorState = _page->editorState();
    if (action == @selector(_showTextStyleOptions:))
        return editorState.isContentRichlyEditable && editorState.selectionIsRange && !_showingTextStyleOptions;
    if (_showingTextStyleOptions)
        return (action == @selector(toggleBoldface:) || action == @selector(toggleItalics:) || action == @selector(toggleUnderline:));
    // FIXME: Some of the following checks should be removed once internal clients move to the underscore-prefixed versions.
    if (action == @selector(toggleBoldface:) || action == @selector(toggleItalics:) || action == @selector(toggleUnderline:) || action == @selector(_toggleStrikeThrough:)
        || action == @selector(_alignLeft:) || action == @selector(_alignRight:) || action == @selector(_alignCenter:) || action == @selector(_alignJustified:)
        || action == @selector(_setTextColor:sender:) || action == @selector(_setFont:sender:) || action == @selector(_setFontSize:sender:)
        || action == @selector(_insertOrderedList:) || action == @selector(_insertUnorderedList:) || action == @selector(_insertNestedOrderedList:) || action == @selector(_insertNestedUnorderedList:)
        || action == @selector(_increaseListLevel:) || action == @selector(_decreaseListLevel:) || action == @selector(_changeListType:) || action == @selector(_indent:) || action == @selector(_outdent:)
        || action == @selector(increaseSize:) || action == @selector(decreaseSize:) || action == @selector(makeTextWritingDirectionNatural:)) {
        // FIXME: This should be more nuanced in the future, rather than returning YES for all richly editable areas. For instance, outdent: should be disabled when the selection is already
        // at the outermost indentation level.
        return editorState.isContentRichlyEditable;
    }
    if (action == @selector(cut:))
        return !editorState.isInPasswordField && editorState.isContentEditable && editorState.selectionIsRange;
    
    if (action == @selector(paste:) || action == @selector(_pasteAsQuotation:) || action == @selector(_pasteAndMatchStyle:) || action == @selector(pasteAndMatchStyle:)) {
        if (editorState.selectionIsNone || !editorState.isContentEditable)
            return NO;
        UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
        NSArray *types = [self supportedPasteboardTypesForCurrentSelection];
        NSIndexSet *indices = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, [pasteboard numberOfItems])];
        if ([pasteboard containsPasteboardTypes:types inItemSet:indices])
            return YES;

#if PLATFORM(IOS)
        if (editorState.isContentRichlyEditable && self.webView.configuration._attachmentElementEnabled) {
            for (NSItemProvider *itemProvider in pasteboard.itemProviders) {
                auto preferredPresentationStyle = itemProvider.preferredPresentationStyle;
                if (preferredPresentationStyle == UIPreferredPresentationStyleInline)
                    continue;

                if (preferredPresentationStyle == UIPreferredPresentationStyleUnspecified && !itemProvider.suggestedName.length)
                    continue;

                if (itemProvider.web_fileUploadContentTypes.count)
                    return YES;
            }
        }
#endif // PLATFORM(IOS)

        auto focusedDocumentOrigin = editorState.originIdentifierForPasteboard;
        if (focusedDocumentOrigin.isEmpty())
            return NO;

        NSArray *allCustomPasteboardData = [pasteboard dataForPasteboardType:@(WebCore::PasteboardCustomData::cocoaType()) inItemSet:indices];
        for (NSData *data in allCustomPasteboardData) {
            auto buffer = WebCore::SharedBuffer::create(data);
            if (WebCore::PasteboardCustomData::fromSharedBuffer(buffer.get()).origin() == focusedDocumentOrigin)
                return YES;
        }
        return NO;
    }

    if (action == @selector(copy:)) {
        if (editorState.isInPasswordField)
            return NO;
        return editorState.selectionIsRange;
    }

    if (action == @selector(_define:)) {
        if (editorState.isInPasswordField || !editorState.selectionIsRange)
            return NO;

        NSUInteger textLength = editorState.postLayoutData().selectedTextLength;
        // FIXME: We should be calling UIReferenceLibraryViewController to check if the length is
        // acceptable, but the interface takes a string.
        // <rdar://problem/15254406>
        if (!textLength || textLength > 200)
            return NO;

#if !PLATFORM(MACCATALYST)
        if ([[PAL::getMCProfileConnectionClass() sharedConnection] effectiveBoolValueForSetting:PAL::get_ManagedConfiguration_MCFeatureDefinitionLookupAllowed()] == MCRestrictedBoolExplicitNo)
            return NO;
#endif
            
        return YES;
    }

    if (action == @selector(_lookup:)) {
        if (editorState.isInPasswordField)
            return NO;

#if !PLATFORM(MACCATALYST)
        if ([[PAL::getMCProfileConnectionClass() sharedConnection] effectiveBoolValueForSetting:PAL::get_ManagedConfiguration_MCFeatureDefinitionLookupAllowed()] == MCRestrictedBoolExplicitNo)
            return NO;
#endif

        return editorState.selectionIsRange;
    }

    if (action == @selector(_share:)) {
        if (editorState.isInPasswordField || !editorState.selectionIsRange)
            return NO;

        return editorState.postLayoutData().selectedTextLength > 0;
    }

    if (action == @selector(_addShortcut:)) {
        if (editorState.isInPasswordField || !editorState.selectionIsRange)
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
        if (!editorState.selectionIsRange || !editorState.postLayoutData().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        if ([[self selectedText] _containsCJScriptsOnly])
            return NO;
        return YES;
    }

    if (action == @selector(_transliterateChinese:)) {
        if (!editorState.selectionIsRange || !editorState.postLayoutData().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        return UIKeyboardEnabledInputModesAllowChineseTransliterationForText([self selectedText]);
    }

    if (action == @selector(select:)) {
        // Disable select in password fields so that you can't see word boundaries.
        return !editorState.isInPasswordField && [self hasContent] && !editorState.selectionIsNone && !editorState.selectionIsRange;
    }

    if (action == @selector(selectAll:)) {
        // By platform convention we don't show Select All in the callout menu for a range selection.
        if ([sender isKindOfClass:UIMenuController.class])
            return !editorState.selectionIsNone && !editorState.selectionIsRange;
#if USE(UIKIT_KEYBOARD_ADDITIONS)
        return YES;
#else
        return !editorState.selectionIsNone && self.hasContent;
#endif
    }

    if (action == @selector(replace:))
        return editorState.isContentEditable && !editorState.isInPasswordField;

    if (action == @selector(makeTextWritingDirectionLeftToRight:) || action == @selector(makeTextWritingDirectionRightToLeft:)) {
        if (!editorState.isContentEditable)
            return NO;

        auto baseWritingDirection = editorState.postLayoutData().baseWritingDirection;
        if (baseWritingDirection == WebCore::WritingDirection::LeftToRight && !UIKeyboardIsRightToLeftInputModeActive()) {
            // A keyboard is considered "active" if it is available for the user to switch to. As such, this check prevents
            // text direction actions from showing up in the case where a user has only added left-to-right keyboards, and
            // is also not editing right-to-left content.
            return NO;
        }

        if (action == @selector(makeTextWritingDirectionLeftToRight:))
            return baseWritingDirection != WebCore::WritingDirection::LeftToRight;

        return baseWritingDirection != WebCore::WritingDirection::RightToLeft;
    }

    return [super canPerformAction:action withSender:sender];
}

- (id)targetForAction:(SEL)action withSender:(id)sender
{
    return [_webView targetForAction:action withSender:sender];
}

- (id)targetForActionForWebView:(SEL)action withSender:(id)sender
{
    return [super targetForAction:action withSender:sender];
}

- (void)_willHideMenu:(NSNotification *)notification
{
    [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];
}

- (void)_didHideMenu:(NSNotification *)notification
{
    _showingTextStyleOptions = NO;
    [_textInteractionAssistant hideTextStyleOptions];
}

- (void)_keyboardDidRequestDismissal:(NSNotification *)notification
{
    if (![self isFirstResponder])
        return;
    _keyboardDidRequestDismissal = YES;
}

- (void)copyForWebView:(id)sender
{
    _page->executeEditCommand("copy"_s);
}

- (void)cutForWebView:(id)sender
{
    [self executeEditCommandWithCallback:@"cut"];
}

- (void)pasteForWebView:(id)sender
{
    if (sender == UIMenuController.sharedMenuController && [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::GrantedForGesture])
        return;

    _page->executeEditCommand("paste"_s);
}

- (void)_pasteAsQuotationForWebView:(id)sender
{
    _page->executeEditCommand("PasteAsQuotation"_s);
}

- (void)selectForWebView:(id)sender
{
    [_textInteractionAssistant selectWord];
    // We cannot use selectWord command, because we want to be able to select the word even when it is the last in the paragraph.
    _page->extendSelection(WebCore::TextGranularity::WordGranularity);
}

- (void)selectAllForWebView:(id)sender
{
    [_textInteractionAssistant selectAll:sender];
    _page->selectAll();
}

- (BOOL)shouldSynthesizeKeyEvents
{
    if (_focusedElementInformation.shouldSynthesizeKeyEventsForEditing && self.hasHiddenContentEditable)
        return true;
    return false;
}

- (void)toggleBoldfaceForWebView:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    [self executeEditCommandWithCallback:@"toggleBold"];

    if (self.shouldSynthesizeKeyEvents)
        _page->generateSyntheticEditingCommand(WebKit::SyntheticEditingCommandType::ToggleBoldface);
}

- (void)toggleItalicsForWebView:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    [self executeEditCommandWithCallback:@"toggleItalic"];

    if (self.shouldSynthesizeKeyEvents)
        _page->generateSyntheticEditingCommand(WebKit::SyntheticEditingCommandType::ToggleItalic);
}

- (void)toggleUnderlineForWebView:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    [self executeEditCommandWithCallback:@"toggleUnderline"];

    if (self.shouldSynthesizeKeyEvents)
        _page->generateSyntheticEditingCommand(WebKit::SyntheticEditingCommandType::ToggleUnderline);
}

- (void)_showTextStyleOptionsForWebView:(id)sender
{
    _showingTextStyleOptions = YES;
    [_textInteractionAssistant showTextStyleOptions];
}

- (void)_showDictionary:(NSString *)text
{
    CGRect presentationRect = _page->editorState().postLayoutData().selectionRects[0].rect();
    if (_textInteractionAssistant)
        [_textInteractionAssistant showDictionaryFor:text fromRect:presentationRect];
}

- (void)_defineForWebView:(id)sender
{
#if !PLATFORM(MACCATALYST)
    MCProfileConnection *connection = [PAL::getMCProfileConnectionClass() sharedConnection];
    if ([connection effectiveBoolValueForSetting:PAL::get_ManagedConfiguration_MCFeatureDefinitionLookupAllowed()] == MCRestrictedBoolExplicitNo)
        return;
#endif

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
    RetainPtr<WKWebView> webView = _webView.get();
    _page->getSelectionOrContentsAsString([view, webView](const String& string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;
        [webView _accessibilityDidGetSpeakSelectionContent:string];
        if ([view respondsToSelector:@selector(accessibilitySpeakSelectionSetContent:)])
            [view accessibilitySpeakSelectionSetContent:string];
    });
}

- (void)_accessibilityRetrieveRectsEnclosingSelectionOffset:(NSInteger)offset withGranularity:(UITextGranularity)granularity
{
    RetainPtr<WKContentView> view = self;
    _page->requestRectsForGranularityWithSelectionOffset(toWKTextGranularity(granularity), offset , [view, offset, granularity](const Vector<WebCore::SelectionRect>& selectionRects, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;
        if ([view respondsToSelector:@selector(_accessibilityDidGetSelectionRects:withGranularity:atOffset:)])
            [view _accessibilityDidGetSelectionRects:[view webSelectionRectsForSelectionRects:selectionRects] withGranularity:granularity atOffset:offset];
    });
}

- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text
{
    [self _accessibilityRetrieveRectsAtSelectionOffset:offset withText:text completionHandler:nil];
}

- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text completionHandler:(void (^)(const Vector<WebCore::SelectionRect>& rects))completionHandler
{
    RetainPtr<WKContentView> view = self;
    _page->requestRectsAtSelectionOffsetWithText(offset, text, [view, offset, capturedCompletionHandler = makeBlockPtr(completionHandler)](const Vector<WebCore::SelectionRect>& selectionRects, WebKit::CallbackBase::Error error) {
        if (capturedCompletionHandler)
            capturedCompletionHandler(selectionRects);

        if (error != WebKit::CallbackBase::Error::None)
            return;
        if ([view respondsToSelector:@selector(_accessibilityDidGetSelectionRects:withGranularity:atOffset:)])
            [view _accessibilityDidGetSelectionRects:[view webSelectionRectsForSelectionRects:selectionRects] withGranularity:UITextGranularityWord atOffset:offset];
    });
}

- (void)_accessibilityStoreSelection
{
    _page->storeSelectionForAccessibility(true);
}

- (void)_accessibilityClearSelection
{
    _page->storeSelectionForAccessibility(false);
}

- (BOOL)_handleDOMPasteRequestWithResult:(WebCore::DOMPasteAccessResponse)response
{
    if (response == WebCore::DOMPasteAccessResponse::GrantedForCommand || response == WebCore::DOMPasteAccessResponse::GrantedForGesture)
        _page->grantAccessToCurrentPasteboardData(UIPasteboardNameGeneral);

    if (auto pasteHandler = WTFMove(_domPasteRequestHandler)) {
        [UIMenuController.sharedMenuController hideMenuFromView:self];
        pasteHandler(response);
        return YES;
    }
    return NO;
}

- (void)_willPerformAction:(SEL)action sender:(id)sender
{
    if (action != @selector(paste:))
        [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];
}

- (void)_didPerformAction:(SEL)action sender:(id)sender
{
    if (action == @selector(paste:))
        [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];
}

// UIWKInteractionViewProtocol

static inline WebKit::GestureType toGestureType(UIWKGestureType gestureType)
{
    switch (gestureType) {
    case UIWKGestureLoupe:
        return WebKit::GestureType::Loupe;
    case UIWKGestureOneFingerTap:
        return WebKit::GestureType::OneFingerTap;
    case UIWKGestureTapAndAHalf:
        return WebKit::GestureType::TapAndAHalf;
    case UIWKGestureDoubleTap:
        return WebKit::GestureType::DoubleTap;
    case UIWKGestureTapAndHalf:
        return WebKit::GestureType::TapAndHalf;
    case UIWKGestureDoubleTapInUneditable:
        return WebKit::GestureType::DoubleTapInUneditable;
    case UIWKGestureOneFingerTapInUneditable:
        return WebKit::GestureType::OneFingerTapInUneditable;
    case UIWKGestureOneFingerTapSelectsAll:
        return WebKit::GestureType::OneFingerTapSelectsAll;
    case UIWKGestureOneFingerDoubleTap:
        return WebKit::GestureType::OneFingerDoubleTap;
    case UIWKGestureOneFingerTripleTap:
        return WebKit::GestureType::OneFingerTripleTap;
    case UIWKGestureTwoFingerSingleTap:
        return WebKit::GestureType::TwoFingerSingleTap;
    case UIWKGestureTwoFingerRangedSelectGesture:
        return WebKit::GestureType::TwoFingerRangedSelectGesture;
    case UIWKGestureTapOnLinkWithGesture:
        return WebKit::GestureType::TapOnLinkWithGesture;
    case UIWKGesturePhraseBoundary:
        return WebKit::GestureType::PhraseBoundary;
    case UIWKGestureMakeWebSelection:
        ASSERT_NOT_REACHED();
        return WebKit::GestureType::Loupe;
    }
    ASSERT_NOT_REACHED();
    return WebKit::GestureType::Loupe;
}

static inline UIWKGestureType toUIWKGestureType(WebKit::GestureType gestureType)
{
    switch (gestureType) {
    case WebKit::GestureType::Loupe:
        return UIWKGestureLoupe;
    case WebKit::GestureType::OneFingerTap:
        return UIWKGestureOneFingerTap;
    case WebKit::GestureType::TapAndAHalf:
        return UIWKGestureTapAndAHalf;
    case WebKit::GestureType::DoubleTap:
        return UIWKGestureDoubleTap;
    case WebKit::GestureType::TapAndHalf:
        return UIWKGestureTapAndHalf;
    case WebKit::GestureType::DoubleTapInUneditable:
        return UIWKGestureDoubleTapInUneditable;
    case WebKit::GestureType::OneFingerTapInUneditable:
        return UIWKGestureOneFingerTapInUneditable;
    case WebKit::GestureType::OneFingerTapSelectsAll:
        return UIWKGestureOneFingerTapSelectsAll;
    case WebKit::GestureType::OneFingerDoubleTap:
        return UIWKGestureOneFingerDoubleTap;
    case WebKit::GestureType::OneFingerTripleTap:
        return UIWKGestureOneFingerTripleTap;
    case WebKit::GestureType::TwoFingerSingleTap:
        return UIWKGestureTwoFingerSingleTap;
    case WebKit::GestureType::TwoFingerRangedSelectGesture:
        return UIWKGestureTwoFingerRangedSelectGesture;
    case WebKit::GestureType::TapOnLinkWithGesture:
        return UIWKGestureTapOnLinkWithGesture;
    case WebKit::GestureType::PhraseBoundary:
        return UIWKGesturePhraseBoundary;
    }
}

static inline WebKit::SelectionTouch toSelectionTouch(UIWKSelectionTouch touch)
{
    switch (touch) {
    case UIWKSelectionTouchStarted:
        return WebKit::SelectionTouch::Started;
    case UIWKSelectionTouchMoved:
        return WebKit::SelectionTouch::Moved;
    case UIWKSelectionTouchEnded:
        return WebKit::SelectionTouch::Ended;
    case UIWKSelectionTouchEndedMovingForward:
        return WebKit::SelectionTouch::EndedMovingForward;
    case UIWKSelectionTouchEndedMovingBackward:
        return WebKit::SelectionTouch::EndedMovingBackward;
    case UIWKSelectionTouchEndedNotMoving:
        return WebKit::SelectionTouch::EndedNotMoving;
    }
    ASSERT_NOT_REACHED();
    return WebKit::SelectionTouch::Ended;
}

static inline UIWKSelectionTouch toUIWKSelectionTouch(WebKit::SelectionTouch touch)
{
    switch (touch) {
    case WebKit::SelectionTouch::Started:
        return UIWKSelectionTouchStarted;
    case WebKit::SelectionTouch::Moved:
        return UIWKSelectionTouchMoved;
    case WebKit::SelectionTouch::Ended:
        return UIWKSelectionTouchEnded;
    case WebKit::SelectionTouch::EndedMovingForward:
        return UIWKSelectionTouchEndedMovingForward;
    case WebKit::SelectionTouch::EndedMovingBackward:
        return UIWKSelectionTouchEndedMovingBackward;
    case WebKit::SelectionTouch::EndedNotMoving:
        return UIWKSelectionTouchEndedNotMoving;
    }
}

static inline WebKit::GestureRecognizerState toGestureRecognizerState(UIGestureRecognizerState state)
{
    switch (state) {
    case UIGestureRecognizerStatePossible:
        return WebKit::GestureRecognizerState::Possible;
    case UIGestureRecognizerStateBegan:
        return WebKit::GestureRecognizerState::Began;
    case UIGestureRecognizerStateChanged:
        return WebKit::GestureRecognizerState::Changed;
    case UIGestureRecognizerStateCancelled:
        return WebKit::GestureRecognizerState::Cancelled;
    case UIGestureRecognizerStateEnded:
        return WebKit::GestureRecognizerState::Ended;
    case UIGestureRecognizerStateFailed:
        return WebKit::GestureRecognizerState::Failed;
    }
}

static inline UIGestureRecognizerState toUIGestureRecognizerState(WebKit::GestureRecognizerState state)
{
    switch (state) {
    case WebKit::GestureRecognizerState::Possible:
        return UIGestureRecognizerStatePossible;
    case WebKit::GestureRecognizerState::Began:
        return UIGestureRecognizerStateBegan;
    case WebKit::GestureRecognizerState::Changed:
        return UIGestureRecognizerStateChanged;
    case WebKit::GestureRecognizerState::Cancelled:
        return UIGestureRecognizerStateCancelled;
    case WebKit::GestureRecognizerState::Ended:
        return UIGestureRecognizerStateEnded;
    case WebKit::GestureRecognizerState::Failed:
        return UIGestureRecognizerStateFailed;
    }
}

static inline UIWKSelectionFlags toUIWKSelectionFlags(OptionSet<WebKit::SelectionFlags> flags)
{
    NSInteger uiFlags = UIWKNone;
    if (flags.contains(WebKit::WordIsNearTap))
        uiFlags |= UIWKWordIsNearTap;
    if (flags.contains(WebKit::PhraseBoundaryChanged))
        uiFlags |= UIWKPhraseBoundaryChanged;

    return static_cast<UIWKSelectionFlags>(uiFlags);
}

static inline OptionSet<WebKit::SelectionFlags> toSelectionFlags(UIWKSelectionFlags uiFlags)
{
    OptionSet<WebKit::SelectionFlags> flags;
    if (uiFlags & UIWKWordIsNearTap)
        flags.add(WebKit::WordIsNearTap);
    if (uiFlags & UIWKPhraseBoundaryChanged)
        flags.add(WebKit::PhraseBoundaryChanged);
    return flags;
}

static inline WebCore::TextGranularity toWKTextGranularity(UITextGranularity granularity)
{
    switch (granularity) {
    case UITextGranularityCharacter:
        return WebCore::TextGranularity::CharacterGranularity;
    case UITextGranularityWord:
        return WebCore::TextGranularity::WordGranularity;
    case UITextGranularitySentence:
        return WebCore::TextGranularity::SentenceGranularity;
    case UITextGranularityParagraph:
        return WebCore::TextGranularity::ParagraphGranularity;
    case UITextGranularityLine:
        return WebCore::TextGranularity::LineGranularity;
    case UITextGranularityDocument:
        return WebCore::TextGranularity::DocumentGranularity;
    }
}

static inline WebCore::SelectionDirection toWKSelectionDirection(UITextDirection direction)
{
    switch (direction) {
    case UITextLayoutDirectionDown:
    case UITextLayoutDirectionRight:
        return WebCore::SelectionDirection::Right;
    case UITextLayoutDirectionUp:
    case UITextLayoutDirectionLeft:
        return WebCore::SelectionDirection::Left;
    default:
        // UITextDirection is not an enum, but we only want to accept values from UITextLayoutDirection.
        ASSERT_NOT_REACHED();
        return WebCore::SelectionDirection::Right;
    }
}

static void selectionChangedWithGesture(WKContentView *view, const WebCore::IntPoint& point, WebKit::GestureType gestureType, WebKit::GestureRecognizerState gestureState, OptionSet<WebKit::SelectionFlags> flags, WebKit::CallbackBase::Error error)
{
    if (error != WebKit::CallbackBase::Error::None)
        return;
    [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType(gestureType) withState:toUIGestureRecognizerState(gestureState) withFlags:toUIWKSelectionFlags(flags)];
}

static void selectionChangedWithTouch(WKContentView *view, const WebCore::IntPoint& point, WebKit::SelectionTouch touch, OptionSet<WebKit::SelectionFlags> flags, WebKit::CallbackBase::Error error)
{
    if (error != WebKit::CallbackBase::Error::None)
        return;
    [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:toUIWKSelectionTouch((WebKit::SelectionTouch)touch) withFlags:toUIWKSelectionFlags(flags)];
}

- (BOOL)_hasFocusedElement
{
    return _focusedElementInformation.elementType != WebKit::InputType::None;
}

- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state
{
    [self changeSelectionWithGestureAt:point withGesture:gestureType withState:state withFlags:UIWKNone];
}

- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state withFlags:(UIWKSelectionFlags)flags
{
    _usingGestureForSelection = YES;
    _page->selectWithGesture(WebCore::IntPoint(point), toGestureType(gestureType), toGestureRecognizerState(state), self._hasFocusedElement, [self, strongSelf = retainPtr(self), state, flags](const WebCore::IntPoint& point, WebKit::GestureType gestureType, WebKit::GestureRecognizerState gestureState, OptionSet<WebKit::SelectionFlags> innerFlags, WebKit::CallbackBase::Error error) {
        selectionChangedWithGesture(self, point, gestureType, gestureState, toSelectionFlags(flags) | innerFlags, error);
        if (state == UIGestureRecognizerStateEnded || state == UIGestureRecognizerStateCancelled)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart withFlags:(UIWKSelectionFlags)flags
{
    _usingGestureForSelection = YES;
    _page->updateSelectionWithTouches(WebCore::IntPoint(point), toSelectionTouch(touch), baseIsStart, [self, strongSelf = retainPtr(self), flags](const WebCore::IntPoint& point, WebKit::SelectionTouch touch, OptionSet<WebKit::SelectionFlags> innerFlags, WebKit::CallbackBase::Error error) {
        selectionChangedWithTouch(self, point, touch, toSelectionFlags(flags) | innerFlags, error);
        if (toUIWKSelectionTouch(touch) != UIWKSelectionTouchStarted && toUIWKSelectionTouch(touch) != UIWKSelectionTouchMoved)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState
{
    _usingGestureForSelection = YES;
    _page->selectWithTwoTouches(WebCore::IntPoint(from), WebCore::IntPoint(to), toGestureType(gestureType), toGestureRecognizerState(gestureState), [self, strongSelf = retainPtr(self)](const WebCore::IntPoint& point, WebKit::GestureType gestureType, WebKit::GestureRecognizerState gestureState, OptionSet<WebKit::SelectionFlags> flags, WebKit::CallbackBase::Error error) {
        selectionChangedWithGesture(self, point, gestureType, gestureState, flags, error);
        if (toUIGestureRecognizerState(gestureState) == UIGestureRecognizerStateEnded || toUIGestureRecognizerState(gestureState) == UIGestureRecognizerStateCancelled)
            _usingGestureForSelection = NO;
    });
}

- (void)moveByOffset:(NSInteger)offset
{
    if (!offset)
        return;
    
    [self beginSelectionChange];
    RetainPtr<WKContentView> view = self;
    _page->moveSelectionByOffset(offset, [view] {
        [view endSelectionChange];
    });
}

- (const WebKit::WKAutoCorrectionData&)autocorrectionData
{
    return _autocorrectionData;
}

// The completion handler can pass nil if input does not match the actual text preceding the insertion point.
- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler
{
    if (!completionHandler) {
        [NSException raise:NSInvalidArgumentException format:@"Expected a nonnull completion handler in %s.", __PRETTY_FUNCTION__];
        return;
    }

    if (!input || ![input length]) {
        completionHandler(nil);
        return;
    }

    _page->requestAutocorrectionData(input, [view = retainPtr(self), completion = makeBlockPtr(completionHandler)](auto data) {
        CGRect firstRect;
        CGRect lastRect;
        auto& rects = data.textRects;
        if (rects.isEmpty()) {
            firstRect = CGRectZero;
            lastRect = CGRectZero;
        } else {
            firstRect = rects.first();
            lastRect = rects.last();
        }

        view->_autocorrectionData.font = data.font;
        view->_autocorrectionData.textFirstRect = firstRect;
        view->_autocorrectionData.textLastRect = lastRect;

        completion(!rects.isEmpty() ? [WKAutocorrectionRects autocorrectionRectsWithFirstCGRect:firstRect lastCGRect:lastRect] : nil);
    });
}

- (void)requestRectsToEvadeForSelectionCommandsWithCompletionHandler:(void(^)(NSArray<NSValue *> *rects))completionHandler
{
    if (!completionHandler) {
        [NSException raise:NSInvalidArgumentException format:@"Expected a nonnull completion handler in %s.", __PRETTY_FUNCTION__];
        return;
    }

    if ([self _shouldSuppressSelectionCommands] || self.webView._editable) {
        completionHandler(@[ ]);
        return;
    }

    if (_focusedElementInformation.elementType != WebKit::InputType::ContentEditable && _focusedElementInformation.elementType != WebKit::InputType::TextArea) {
        completionHandler(@[ ]);
        return;
    }

    // Give the page some time to present custom editing UI before attempting to detect and evade it.
    auto delayBeforeShowingCalloutBar = (0.25_s).nanoseconds();
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delayBeforeShowingCalloutBar), dispatch_get_main_queue(), [completion = makeBlockPtr(completionHandler), weakSelf = WeakObjCPtr<WKContentView>(self)] () mutable {
        if (!weakSelf) {
            completion(@[ ]);
            return;
        }

        auto strongSelf = weakSelf.get();
        if (!strongSelf->_page) {
            completion(@[ ]);
            return;
        }

        strongSelf->_page->requestEvasionRectsAboveSelection([completion = WTFMove(completion)] (auto& rects) {
            completion(createNSArray(rects).get());
        });
    });
}

- (void)selectPositionAtPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    [self _selectPositionAtPoint:point stayingWithinFocusedElement:self._hasFocusedElement completionHandler:completionHandler];
}

- (void)_selectPositionAtPoint:(CGPoint)point stayingWithinFocusedElement:(BOOL)stayingWithinFocusedElement completionHandler:(void (^)(void))completionHandler
{
    _usingGestureForSelection = YES;
    UIWKSelectionCompletionHandler selectionHandler = [completionHandler copy];
    RetainPtr<WKContentView> view = self;

    _page->selectPositionAtPoint(WebCore::IntPoint(point), stayingWithinFocusedElement, [view, selectionHandler]() {
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
    
    _page->selectPositionAtBoundaryWithDirection(WebCore::IntPoint(point), toWKTextGranularity(granularity), toWKSelectionDirection(direction), self._hasFocusedElement, [view, selectionHandler]() {
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
    
    _page->moveSelectionAtBoundaryWithDirection(toWKTextGranularity(granularity), toWKSelectionDirection(direction), [view, selectionHandler] {
        selectionHandler();
        view->_usingGestureForSelection = NO;
        [selectionHandler release];
    });
}

- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    _usingGestureForSelection = YES;
    ++_suppressNonEditableSingleTapTextInteractionCount;
    UIWKSelectionCompletionHandler selectionHandler = [completionHandler copy];
    RetainPtr<WKContentView> view = self;

    _page->selectTextWithGranularityAtPoint(WebCore::IntPoint(point), toWKTextGranularity(granularity), self._hasFocusedElement, [view, selectionHandler] {
        selectionHandler();
        view->_usingGestureForSelection = NO;
        --view->_suppressNonEditableSingleTapTextInteractionCount;
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
    
    auto respectSelectionAnchor = self.interactionAssistant._wk_hasFloatingCursor ? WebKit::RespectSelectionAnchor::Yes : WebKit::RespectSelectionAnchor::No;
    _page->updateSelectionWithExtentPoint(WebCore::IntPoint(point), self._hasFocusedElement, respectSelectionAnchor, [selectionHandler](bool endIsMoving, WebKit::CallbackBase::Error error) {
        selectionHandler(endIsMoving);
        [selectionHandler release];
    });
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity completionHandler:(void (^)(BOOL selectionEndIsMoving))completionHandler
{
    UIWKSelectionWithDirectionCompletionHandler selectionHandler = [completionHandler copy];
    
    ++_suppressNonEditableSingleTapTextInteractionCount;
    _page->updateSelectionWithExtentPointAndBoundary(WebCore::IntPoint(point), toWKTextGranularity(granularity), self._hasFocusedElement, [selectionHandler, protectedSelf = retainPtr(self)] (bool endIsMoving, WebKit::CallbackBase::Error error) {
        selectionHandler(endIsMoving);
        [selectionHandler release];
        --protectedSelf->_suppressNonEditableSingleTapTextInteractionCount;
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
    auto& editorState = _page->editorState();
    if (editorState.hasComposition) {
        auto& markedTextRects = editorState.postLayoutData().markedTextRects;
        return markedTextRects.isEmpty() ? CGRectZero : markedTextRects.first().rect();
    }
    return _autocorrectionData.textFirstRect;
}

- (CGRect)textLastRect
{
    auto& editorState = _page->editorState();
    if (editorState.hasComposition) {
        auto& markedTextRects = editorState.postLayoutData().markedTextRects;
        return markedTextRects.isEmpty() ? CGRectZero : markedTextRects.last().rect();
    }
    return _autocorrectionData.textLastRect;
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
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    if ([self _disableAutomaticKeyboardUI]) {
        if (completionHandler)
            completionHandler(nil);
        return;
    }
#endif

    // FIXME: Remove the synchronous call when <rdar://problem/16207002> is fixed.
    const bool useSyncRequest = true;

    if (useSyncRequest) {
        if (completionHandler)
            completionHandler(_page->applyAutocorrection(correction, input) ? [WKAutocorrectionRects autocorrectionRectsWithFirstCGRect:_autocorrectionData.textFirstRect lastCGRect:_autocorrectionData.textLastRect] : nil);
        return;
    }

    _page->applyAutocorrection(correction, input, [view = retainPtr(self), completion = makeBlockPtr(completionHandler)](auto& string, auto error) {
        if (completion)
            completion(!string.isNull() ? [WKAutocorrectionRects autocorrectionRectsWithFirstCGRect:view->_autocorrectionData.textFirstRect lastCGRect:view->_autocorrectionData.textLastRect] : nil);
    });
}

- (void)_invokePendingAutocorrectionContextHandler:(WKAutocorrectionContext *)context
{
    if (auto handler = WTFMove(_pendingAutocorrectionContextHandler))
        handler(context);
}

- (void)_cancelPendingAutocorrectionContextHandler
{
    [self _invokePendingAutocorrectionContextHandler:WKAutocorrectionContext.emptyAutocorrectionContext];
}

- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler
{
    if (!completionHandler) {
        [NSException raise:NSInvalidArgumentException format:@"Expected a nonnull completion handler in %s.", __PRETTY_FUNCTION__];
        return;
    }

#if USE(UIKIT_KEYBOARD_ADDITIONS)
    if ([self _disableAutomaticKeyboardUI]) {
        completionHandler(WKAutocorrectionContext.emptyAutocorrectionContext);
        return;
    }
#endif

    if (!_page->hasRunningProcess()) {
        completionHandler(WKAutocorrectionContext.emptyAutocorrectionContext);
        return;
    }

    // FIXME: Remove the synchronous call when <rdar://problem/16207002> is fixed.
    const bool useSyncRequest = true;

    if (useSyncRequest && _pendingAutocorrectionContextHandler) {
        completionHandler(WKAutocorrectionContext.emptyAutocorrectionContext);
        return;
    }

    _pendingAutocorrectionContextHandler = completionHandler;
    _page->requestAutocorrectionContext();

    if (useSyncRequest) {
        _page->process().connection()->waitForAndDispatchImmediately<Messages::WebPageProxy::HandleAutocorrectionContext>(_page->webPageID(), 1_s, IPC::WaitForOption::DispatchIncomingSyncMessagesWhileWaiting);
        [self _cancelPendingAutocorrectionContextHandler];
    }
}

- (void)_handleAutocorrectionContext:(const WebKit::WebAutocorrectionContext&)context
{
    [self _invokePendingAutocorrectionContextHandler:[WKAutocorrectionContext autocorrectionContextWithWebContext:context]];
}

- (void)_didStartProvisionalLoadForMainFrame
{
    // Reset the double tap gesture recognizer to prevent any double click that is in the process of being recognized.
    [_doubleTapGestureRecognizerForDoubleClick _wk_cancel];
    // We also need to disable the double-tap gesture recognizers that are enabled for double-tap-to-zoom and which
    // are enabled when a single tap is first recognized. This avoids tests running in sequence and simulating taps
    // in the same location to trigger double-tap recognition.
    [self _setDoubleTapGesturesEnabled:NO];
    [_twoFingerDoubleTapGestureRecognizer _wk_cancel];
}

- (void)_didCommitLoadForMainFrame
{
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    _seenHardwareKeyDownInNonEditableElement = NO;
#endif
    [self _elementDidBlur];
    [self _cancelLongPressGestureRecognizer];
    [self _hideTargetedPreviewContainerViews];
    [_webView _didCommitLoadForMainFrame];

    _textInteractionDidChangeFocusedElement = NO;
    _textInteractionIsHappening = NO;
    _hasValidPositionInformation = NO;
    _positionInformation = { };
}

#if !USE(UIKIT_KEYBOARD_ADDITIONS)
- (NSArray *)keyCommands
{
    if (!_page->editorState().isContentEditable)
        return nil;

    static NSArray* editableKeyCommands = [@[
        [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:0 action:@selector(_nextAccessoryTab:)],
        [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:UIKeyModifierShift action:@selector(_previousAccessoryTab:)]
    ] retain];
    return editableKeyCommands;
}
#endif

- (void)_nextAccessoryTabForWebView:(id)sender
{
    [self accessoryTab:YES];
}

- (void)_previousAccessoryTabForWebView:(id)sender
{
    [self accessoryTab:NO];
}

- (void)_becomeFirstResponderWithSelectionMovingForward:(BOOL)selectingForward completionHandler:(void (^)(BOOL didBecomeFirstResponder))completionHandler
{
    constexpr bool isKeyboardEventValid = false;
    _page->setInitialFocus(selectingForward, isKeyboardEventValid, { }, [protectedSelf = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler([protectedSelf becomeFirstResponder]);
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
        [self _createAndConfigureDoubleTapGestureRecognizer];
    }

    if (_showDebugTapHighlightsForFastClicking && !enabled)
        _tapHighlightInformation.color = [self _tapHighlightColorForFastClick:YES];

    [_doubleTapGestureRecognizer setEnabled:enabled];
    [_nonBlockingDoubleTapGestureRecognizer setEnabled:!enabled];
    [self _resetIsDoubleTapPending];
}

// MARK: UIWebFormAccessoryDelegate protocol and accessory methods

- (void)accessoryClear
{
    _page->setFocusedElementValue({ });
}

- (void)accessoryDone
{
    [self endEditingAndUpdateFocusAppearanceWithReason:EndEditingReasonAccessoryDone];
    _page->setIsShowingInputViewForFocusedElement(false);
}

- (void)updateFocusedElementValueAsNumber:(double)value
{
    _page->setFocusedElementValueAsNumber(value);
    _focusedElementInformation.valueAsNumber = value;
}

- (void)updateFocusedElementValue:(NSString *)value
{
    _page->setFocusedElementValue(value);
    _focusedElementInformation.value = value;
}

- (void)accessoryTab:(BOOL)isNext
{
    // The input peripheral may need to update the focused DOM node before we switch focus. The UI process does
    // not maintain a handle to the actual focused DOM node – only the web process has such a handle. So, we need
    // to end the editing session now before we tell the web process to switch focus. Once the web process tells
    // us the newly focused element we are no longer are in a position to effect the previously focused element.
    // See <https://bugs.webkit.org/show_bug.cgi?id=134409>.
    [self _endEditing];
    _inputPeripheral = nil; // Nullify so that we don't tell the input peripheral to end editing again in -_elementDidBlur.

    _isChangingFocusUsingAccessoryTab = YES;
    [self beginSelectionChange];
    _page->focusNextFocusedElement(isNext, [protectedSelf = retainPtr(self)] {
        [protectedSelf endSelectionChange];
        [protectedSelf reloadInputViews];
        protectedSelf->_isChangingFocusUsingAccessoryTab = NO;
    });
}

- (void)accessoryAutoFill
{
    id <_WKInputDelegate> inputDelegate = [_webView _inputDelegate];
    if ([inputDelegate respondsToSelector:@selector(_webView:accessoryViewCustomButtonTappedInFormInputSession:)])
        [inputDelegate _webView:self.webView accessoryViewCustomButtonTappedInFormInputSession:_formInputSession.get()];
}

- (UIWebFormAccessory *)formAccessoryView
{
    if (_formAccessoryView)
        return _formAccessoryView.get();
    _formAccessoryView = adoptNS([[UIWebFormAccessory alloc] initWithInputAssistantItem:self.inputAssistantItem]);
    [_formAccessoryView setDelegate:self];
    return _formAccessoryView.get();
}

- (void)accessoryOpen
{
    if (!_inputPeripheral)
        return;
    [self _zoomToRevealFocusedElement];
    [self _updateAccessory];
    [_inputPeripheral beginEditing];
}

- (void)_updateAccessory
{
    auto* accessoryView = self.formAccessoryView; // Creates one, if needed.

    if ([accessoryView respondsToSelector:@selector(setNextPreviousItemsVisible:)])
        [accessoryView setNextPreviousItemsVisible:!self.webView._editable];

    [accessoryView setNextEnabled:_focusedElementInformation.hasNextNode];
    [accessoryView setPreviousEnabled:_focusedElementInformation.hasPreviousNode];

    if (WebKit::currentUserInterfaceIdiomIsPad()) {
        [accessoryView setClearVisible:NO];
        return;
    }

    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::Date:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Month:
    case WebKit::InputType::Time:
        [accessoryView setClearVisible:YES];
        return;
    default:
        [accessoryView setClearVisible:NO];
        return;
    }
}

// MARK: Keyboard interaction
// UITextInput protocol implementation

- (BOOL)_allowAnimatedUpdateSelectionRectViews
{
    return NO;
}

- (void)beginSelectionChange
{
    [self.inputDelegate selectionWillChange:self];
}

- (void)endSelectionChange
{
    [self.inputDelegate selectionDidChange:self];
}

- (void)willFinishIgnoringCalloutBarFadeAfterPerformingAction
{
    _ignoreSelectionCommandFadeCount++;
    _page->scheduleFullEditorStateUpdate();
    _page->callAfterNextPresentationUpdate([weakSelf = WeakObjCPtr<WKContentView>(self)] (auto) {
        if (auto strongSelf = weakSelf.get())
            strongSelf->_ignoreSelectionCommandFadeCount--;
    });
}

- (void)_didChangeWebViewEditability
{
    if ([_formAccessoryView respondsToSelector:@selector(setNextPreviousItemsVisible:)])
        [_formAccessoryView setNextPreviousItemsVisible:!self.webView._editable];
    
    [_twoFingerSingleTapGestureRecognizer setEnabled:!self.webView._editable];
}

- (void)insertTextSuggestion:(UITextSuggestion *)textSuggestion
{
    // FIXME: Replace NSClassFromString with actual class as soon as UIKit submitted the new class into the iOS SDK.
    if ([textSuggestion isKindOfClass:[UITextAutofillSuggestion class]]) {
        _page->autofillLoginCredentials([(UITextAutofillSuggestion *)textSuggestion username], [(UITextAutofillSuggestion *)textSuggestion password]);
        return;
    }
#if ENABLE(DATALIST_ELEMENT)
    if ([textSuggestion isKindOfClass:[WKDataListTextSuggestion class]]) {
        _page->setFocusedElementValue([textSuggestion inputText]);
        return;
    }
#endif
    id <_WKInputDelegate> inputDelegate = [_webView _inputDelegate];
    if ([inputDelegate respondsToSelector:@selector(_webView:insertTextSuggestion:inInputSession:)])
        [inputDelegate _webView:self.webView insertTextSuggestion:textSuggestion inInputSession:_formInputSession.get()];
}

- (NSString *)textInRange:(UITextRange *)range
{
    return nil;
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text
{
}

static NSArray<WKTextSelectionRect *> *wkTextSelectionRects(const Vector<WebCore::SelectionRect>& rects)
{
    return createNSArray(rects, [] (auto& rect) {
        return adoptNS([[WKTextSelectionRect alloc] initWithSelectionRect:rect]);
    }).autorelease();
}

- (WebCore::FloatRect)_scaledCaretRectForSelectionStart:(WebCore::FloatRect)caretRect
{
    // The logical height of the caret is scaled inversely by the view's zoom scale
    // to achieve the visual effect that the caret is narrow when zoomed in and wide
    // when zoomed out.
    double inverseScale = [self inverseScale];
    if (bool isHorizontalCaret = caretRect.width() < caretRect.height())
        caretRect.setWidth(caretRect.width() * inverseScale);
    else
        caretRect.setHeight(caretRect.height() * inverseScale);
    return caretRect;
}

- (WebCore::FloatRect)_scaledCaretRectForSelectionEnd:(WebCore::FloatRect)caretRect
{
    // The logical height of the caret is scaled inversely by the view's zoom scale
    // to achieve the visual effect that the caret is narrow when zoomed in and wide
    // when zoomed out.
    double inverseScale = [self inverseScale];
    if (bool isHorizontalCaret = caretRect.width() < caretRect.height()) {
        float originalWidth = caretRect.width();
        caretRect.setWidth(originalWidth * inverseScale);
        caretRect.move(caretRect.width() - originalWidth, 0);
    } else {
        float originalHeight = caretRect.height();
        caretRect.setHeight(caretRect.height() * inverseScale);
        caretRect.move(0, caretRect.height() - originalHeight);
    }
    return caretRect;
}

- (UITextRange *)selectedTextRange
{
    auto& editorState = _page->editorState();
    auto hasSelection = !editorState.selectionIsNone;
    if (!hasSelection || editorState.isMissingPostLayoutData)
        return nil;

    auto isRange = editorState.selectionIsRange;
    auto isContentEditable = editorState.isContentEditable;
    // UIKit does not expect caret selections in non-editable content.
    if (!isContentEditable && !isRange)
        return nil;

    auto caretStartRect = [self _scaledCaretRectForSelectionStart:_page->editorState().postLayoutData().caretRectAtStart];
    auto caretEndRect = [self _scaledCaretRectForSelectionEnd:_page->editorState().postLayoutData().caretRectAtEnd];
    auto selectionRects = wkTextSelectionRects(_page->editorState().postLayoutData().selectionRects);
    auto selectedTextLength = editorState.postLayoutData().selectedTextLength;
    return [WKTextRange textRangeWithState:!hasSelection isRange:isRange isEditable:isContentEditable startRect:caretStartRect endRect:caretEndRect selectionRects:selectionRects selectedTextLength:selectedTextLength];
}

- (CGRect)caretRectForPosition:(UITextPosition *)position
{
    return ((WKTextPosition *)position).positionRect;
}

- (NSArray *)selectionRectsForRange:(UITextRange *)range
{
    return [(WKTextRange *)range selectionRects];
}

- (void)setSelectedTextRange:(UITextRange *)range
{
    if (range)
        return;
#if !PLATFORM(MACCATALYST)
    if (!self._hasFocusedElement)
        return;
#endif
    [self clearSelection];
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
    auto& editorState = _page->editorState();
    bool hasComposition = editorState.hasComposition;
    if (!hasComposition || editorState.isMissingPostLayoutData)
        return nil;
    auto& postLayoutData = editorState.postLayoutData();
    auto unscaledCaretRectAtStart = postLayoutData.markedTextCaretRectAtStart;
    auto unscaledCaretRectAtEnd = postLayoutData.markedTextCaretRectAtEnd;
    auto isRange = unscaledCaretRectAtStart != unscaledCaretRectAtEnd;
    auto isContentEditable = editorState.isContentEditable;
    auto caretStartRect = [self _scaledCaretRectForSelectionStart:unscaledCaretRectAtStart];
    auto caretEndRect = [self _scaledCaretRectForSelectionEnd:unscaledCaretRectAtEnd];
    auto selectionRects = wkTextSelectionRects(postLayoutData.markedTextRects);
    auto selectedTextLength = postLayoutData.markedText.length();
    return [WKTextRange textRangeWithState:!hasComposition isRange:isRange isEditable:isContentEditable startRect:caretStartRect endRect:caretEndRect selectionRects:selectionRects selectedTextLength:selectedTextLength];
}

- (NSDictionary *)markedTextStyle
{
    return nil;
}

- (void)setMarkedTextStyle:(NSDictionary *)styleDictionary
{
}

static Vector<WebCore::CompositionHighlight> compositionHighlights(NSAttributedString *string)
{
    if (!string.length)
        return { };

    Vector<WebCore::CompositionHighlight> highlights;
    [string enumerateAttributesInRange:NSMakeRange(0, string.length) options:0 usingBlock:[&highlights](NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        if (!attributes[NSMarkedClauseSegmentAttributeName])
            return;

        WebCore::Color highlightColor { WebCore::CompositionHighlight::defaultCompositionFillColor };
        if (UIColor *uiColor = attributes[NSBackgroundColorAttributeName])
            highlightColor = WebCore::colorFromUIColor(uiColor);
        highlights.append({ static_cast<unsigned>(range.location), static_cast<unsigned>(NSMaxRange(range)), highlightColor });
    }];

    std::sort(highlights.begin(), highlights.end(), [](auto& a, auto& b) {
        if (a.startOffset < b.startOffset)
            return true;
        if (a.startOffset > b.startOffset)
            return false;
        return a.endOffset < b.endOffset;
    });

    Vector<WebCore::CompositionHighlight> mergedHighlights;
    mergedHighlights.reserveInitialCapacity(highlights.size());
    for (auto& highlight : highlights) {
        if (mergedHighlights.isEmpty() || mergedHighlights.last().color != highlight.color)
            mergedHighlights.append(highlight);
        else
            mergedHighlights.last().endOffset = highlight.endOffset;
    }

    return mergedHighlights;
}

- (void)setAttributedMarkedText:(NSAttributedString *)markedText selectedRange:(NSRange)selectedRange
{
    [self _setMarkedText:markedText.string highlights:compositionHighlights(markedText) selectedRange:selectedRange];
}

- (void)setMarkedText:(NSString *)markedText selectedRange:(NSRange)selectedRange
{
    [self _setMarkedText:markedText highlights:Vector<WebCore::CompositionHighlight> { } selectedRange:selectedRange];
}

- (void)_setMarkedText:(NSString *)markedText highlights:(const Vector<WebCore::CompositionHighlight>&)highlights selectedRange:(NSRange)selectedRange
{
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    _candidateViewNeedsUpdate = !self.hasMarkedText;
#endif
    _markedText = markedText;
    _page->setCompositionAsync(markedText, { }, highlights, selectedRange, { });
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

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition *)position inDirection:(UITextStorageDirection)direction
{
    return NSWritingDirectionLeftToRight;
}

static WebKit::WritingDirection coreWritingDirection(NSWritingDirection direction)
{
    switch (direction) {
    case NSWritingDirectionNatural:
        return WebCore::WritingDirection::Natural;
    case NSWritingDirectionLeftToRight:
        return WebCore::WritingDirection::LeftToRight;
    case NSWritingDirectionRightToLeft:
        return WebCore::WritingDirection::RightToLeft;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::WritingDirection::Natural;
    }
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction forRange:(UITextRange *)range
{
    if (!_page->isEditable())
        return;

    if (range && ![range isEqual:self.selectedTextRange]) {
        // We currently only support changing the base writing direction at the selection.
        return;
    }
    _page->setBaseWritingDirection(coreWritingDirection(direction));
}

- (CGRect)firstRectForRange:(UITextRange *)range
{
    return CGRectZero;
}

/* Hit testing. */
- (UITextPosition *)closestPositionToPoint:(CGPoint)point
{
#if PLATFORM(MACCATALYST)
    WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
    [self requestAsynchronousPositionInformationUpdate:request];
    if ([self _currentPositionInformationIsApproximatelyValidForRequest:request radiusForApproximation:2] && _positionInformation.isSelectable)
        return [WKTextPosition textPositionWithRect:_positionInformation.caretRect];
#endif
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
    _page->executeEditCommand("deleteBackward"_s);
}

- (BOOL)_shouldSimulateKeyboardInputOnTextInsertion
{
#if USE(TEXT_INTERACTION_ADDITIONS)
    return [self _shouldSimulateKeyboardInputOnTextInsertionInternal];
#else
    return NO;
#endif
}

// Inserts the given string, replacing any selected or marked text.
- (void)insertText:(NSString *)aStringValue
{
    auto* keyboard = [UIKeyboardImpl sharedInstance];

    WebKit::InsertTextOptions options;
    options.processingUserGesture = [keyboard respondsToSelector:@selector(isCallingInputDelegate)] && keyboard.isCallingInputDelegate;
    options.shouldSimulateKeyboardInput = [self _shouldSimulateKeyboardInputOnTextInsertion];
    _page->insertTextAsync(aStringValue, WebKit::EditingRange(), WTFMove(options));
}

- (void)insertText:(NSString *)aStringValue alternatives:(NSArray<NSString *> *)alternatives style:(UITextAlternativeStyle)style
{
    if (!alternatives.count)
        [self insertText:aStringValue];
    else {
        BOOL isLowConfidence = style == UITextAlternativeStyleLowConfidence;
        auto textAlternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:aStringValue alternativeStrings:alternatives isLowConfidence:isLowConfidence]);
        WebCore::TextAlternativeWithRange textAlternativeWithRange { textAlternatives.get(), NSMakeRange(0, aStringValue.length) };

        WebKit::InsertTextOptions options;
        options.shouldSimulateKeyboardInput = [self _shouldSimulateKeyboardInputOnTextInsertion];
        _page->insertDictatedTextAsync(aStringValue, { }, { textAlternativeWithRange }, WTFMove(options));
    }
}

- (BOOL)hasText
{
    auto& editorState = _page->editorState();
    return !editorState.isMissingPostLayoutData && editorState.postLayoutData().hasPlainText;
}

// end of UITextInput protocol implementation

static UITextAutocapitalizationType toUITextAutocapitalize(AutocapitalizeType webkitType)
{
    switch (webkitType) {
    case AutocapitalizeTypeDefault:
        return UITextAutocapitalizationTypeSentences;
    case AutocapitalizeTypeNone:
        return UITextAutocapitalizationTypeNone;
    case AutocapitalizeTypeWords:
        return UITextAutocapitalizationTypeWords;
    case AutocapitalizeTypeSentences:
        return UITextAutocapitalizationTypeSentences;
    case AutocapitalizeTypeAllCharacters:
        return UITextAutocapitalizationTypeAllCharacters;
    }

    return UITextAutocapitalizationTypeSentences;
}

static NSString *contentTypeFromFieldName(WebCore::AutofillFieldName fieldName)
{
    switch (fieldName) {
    case WebCore::AutofillFieldName::Name:
        return UITextContentTypeName;
    case WebCore::AutofillFieldName::HonorificPrefix:
        return UITextContentTypeNamePrefix;
    case WebCore::AutofillFieldName::GivenName:
        return UITextContentTypeMiddleName;
    case WebCore::AutofillFieldName::AdditionalName:
        return UITextContentTypeMiddleName;
    case WebCore::AutofillFieldName::FamilyName:
        return UITextContentTypeFamilyName;
    case WebCore::AutofillFieldName::HonorificSuffix:
        return UITextContentTypeNameSuffix;
    case WebCore::AutofillFieldName::Nickname:
        return UITextContentTypeNickname;
    case WebCore::AutofillFieldName::OrganizationTitle:
        return UITextContentTypeJobTitle;
    case WebCore::AutofillFieldName::Organization:
        return UITextContentTypeOrganizationName;
    case WebCore::AutofillFieldName::StreetAddress:
        return UITextContentTypeFullStreetAddress;
    case WebCore::AutofillFieldName::AddressLine1:
        return UITextContentTypeStreetAddressLine1;
    case WebCore::AutofillFieldName::AddressLine2:
        return UITextContentTypeStreetAddressLine2;
    case WebCore::AutofillFieldName::AddressLevel3:
        return UITextContentTypeSublocality;
    case WebCore::AutofillFieldName::AddressLevel2:
        return UITextContentTypeAddressCity;
    case WebCore::AutofillFieldName::AddressLevel1:
        return UITextContentTypeAddressState;
    case WebCore::AutofillFieldName::CountryName:
        return UITextContentTypeCountryName;
    case WebCore::AutofillFieldName::PostalCode:
        return UITextContentTypePostalCode;
    case WebCore::AutofillFieldName::Tel:
        return UITextContentTypeTelephoneNumber;
    case WebCore::AutofillFieldName::Email:
        return UITextContentTypeEmailAddress;
    case WebCore::AutofillFieldName::URL:
        return UITextContentTypeURL;
    case WebCore::AutofillFieldName::Username:
        return UITextContentTypeUsername;
    case WebCore::AutofillFieldName::None:
    case WebCore::AutofillFieldName::NewPassword:
    case WebCore::AutofillFieldName::CurrentPassword:
    case WebCore::AutofillFieldName::AddressLine3:
    case WebCore::AutofillFieldName::AddressLevel4:
    case WebCore::AutofillFieldName::Country:
    case WebCore::AutofillFieldName::CcName:
    case WebCore::AutofillFieldName::CcGivenName:
    case WebCore::AutofillFieldName::CcAdditionalName:
    case WebCore::AutofillFieldName::CcFamilyName:
    case WebCore::AutofillFieldName::CcNumber:
    case WebCore::AutofillFieldName::CcExp:
    case WebCore::AutofillFieldName::CcExpMonth:
    case WebCore::AutofillFieldName::CcExpYear:
    case WebCore::AutofillFieldName::CcCsc:
    case WebCore::AutofillFieldName::CcType:
    case WebCore::AutofillFieldName::TransactionCurrency:
    case WebCore::AutofillFieldName::TransactionAmount:
    case WebCore::AutofillFieldName::Language:
    case WebCore::AutofillFieldName::Bday:
    case WebCore::AutofillFieldName::BdayDay:
    case WebCore::AutofillFieldName::BdayMonth:
    case WebCore::AutofillFieldName::BdayYear:
    case WebCore::AutofillFieldName::Sex:
    case WebCore::AutofillFieldName::Photo:
    case WebCore::AutofillFieldName::TelCountryCode:
    case WebCore::AutofillFieldName::TelNational:
    case WebCore::AutofillFieldName::TelAreaCode:
    case WebCore::AutofillFieldName::TelLocal:
    case WebCore::AutofillFieldName::TelLocalPrefix:
    case WebCore::AutofillFieldName::TelLocalSuffix:
    case WebCore::AutofillFieldName::TelExtension:
    case WebCore::AutofillFieldName::Impp:
        break;
    };

    return nil;
}

// UITextInputPrivate protocol
// Direct access to the (private) UITextInputTraits object.
- (UITextInputTraits *)textInputTraits
{
    if (!_traits)
        _traits = adoptNS([[UITextInputTraits alloc] init]);

#if USE(UIKIT_KEYBOARD_ADDITIONS)
    // Do not change traits when dismissing the keyboard.
    if (_isBlurringFocusedElement)
        return _traits.get();
#endif

    [_traits setSecureTextEntry:_focusedElementInformation.elementType == WebKit::InputType::Password || [_formInputSession forceSecureTextEntry]];
    [_traits setShortcutConversionType:_focusedElementInformation.elementType == WebKit::InputType::Password ? UITextShortcutConversionTypeNo : UITextShortcutConversionTypeDefault];

    switch (_focusedElementInformation.enterKeyHint) {
    case WebCore::EnterKeyHint::Enter:
        [_traits setReturnKeyType:UIReturnKeyDefault];
        break;
    case WebCore::EnterKeyHint::Done:
        [_traits setReturnKeyType:UIReturnKeyDone];
        break;
    case WebCore::EnterKeyHint::Go:
        [_traits setReturnKeyType:UIReturnKeyGo];
        break;
    case WebCore::EnterKeyHint::Next:
        [_traits setReturnKeyType:UIReturnKeyNext];
        break;
    case WebCore::EnterKeyHint::Search:
        [_traits setReturnKeyType:UIReturnKeySearch];
        break;
    case WebCore::EnterKeyHint::Send:
        [_traits setReturnKeyType:UIReturnKeySend];
        break;
    default: {
        if (!_focusedElementInformation.formAction.isEmpty())
            [_traits setReturnKeyType:_focusedElementInformation.elementType == WebKit::InputType::Search ? UIReturnKeySearch : UIReturnKeyGo];
    }
    }

    if (_focusedElementInformation.elementType == WebKit::InputType::Password || _focusedElementInformation.elementType == WebKit::InputType::Email || _focusedElementInformation.elementType == WebKit::InputType::URL || _focusedElementInformation.formAction.contains("login")) {
        [_traits setAutocapitalizationType:UITextAutocapitalizationTypeNone];
        [_traits setAutocorrectionType:UITextAutocorrectionTypeNo];
    } else {
        [_traits setAutocapitalizationType:toUITextAutocapitalize(_focusedElementInformation.autocapitalizeType)];
        [_traits setAutocorrectionType:_focusedElementInformation.isAutocorrect ? UITextAutocorrectionTypeYes : UITextAutocorrectionTypeNo];
    }

    if (!_focusedElementInformation.isSpellCheckingEnabled) {
        [_traits setSmartQuotesType:UITextSmartQuotesTypeNo];
        [_traits setSmartDashesType:UITextSmartDashesTypeNo];
    }

    switch (_focusedElementInformation.inputMode) {
    case WebCore::InputMode::None:
    case WebCore::InputMode::Unspecified:
        switch (_focusedElementInformation.elementType) {
        case WebKit::InputType::Phone:
            [_traits setKeyboardType:UIKeyboardTypePhonePad];
            break;
        case WebKit::InputType::URL:
            [_traits setKeyboardType:UIKeyboardTypeURL];
            break;
        case WebKit::InputType::Email:
            [_traits setKeyboardType:UIKeyboardTypeEmailAddress];
            break;
        case WebKit::InputType::Number:
            [_traits setKeyboardType:UIKeyboardTypeNumbersAndPunctuation];
            break;
        case WebKit::InputType::NumberPad:
            [_traits setKeyboardType:UIKeyboardTypeNumberPad];
            break;
        case WebKit::InputType::None:
        case WebKit::InputType::ContentEditable:
        case WebKit::InputType::Text:
        case WebKit::InputType::Password:
        case WebKit::InputType::TextArea:
        case WebKit::InputType::Search:
        case WebKit::InputType::Date:
        case WebKit::InputType::DateTime:
        case WebKit::InputType::DateTimeLocal:
        case WebKit::InputType::Month:
        case WebKit::InputType::Week:
        case WebKit::InputType::Time:
        case WebKit::InputType::Select:
        case WebKit::InputType::Drawing:
#if ENABLE(INPUT_TYPE_COLOR)
        case WebKit::InputType::Color:
#endif
            [_traits setKeyboardType:UIKeyboardTypeDefault];
        }
        break;
    case WebCore::InputMode::Text:
        [_traits setKeyboardType:UIKeyboardTypeDefault];
        break;
    case WebCore::InputMode::Telephone:
        [_traits setKeyboardType:UIKeyboardTypePhonePad];
        break;
    case WebCore::InputMode::Url:
        [_traits setKeyboardType:UIKeyboardTypeURL];
        break;
    case WebCore::InputMode::Email:
        [_traits setKeyboardType:UIKeyboardTypeEmailAddress];
        break;
    case WebCore::InputMode::Numeric:
        [_traits setKeyboardType:UIKeyboardTypeNumberPad];
        break;
    case WebCore::InputMode::Decimal:
        [_traits setKeyboardType:UIKeyboardTypeDecimalPad];
        break;
    case WebCore::InputMode::Search:
        [_traits setKeyboardType:UIKeyboardTypeWebSearch];
        break;
    }

    [_traits setTextContentType:contentTypeFromFieldName(_focusedElementInformation.autofillFieldName)];

    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::ContentEditable:
    case WebKit::InputType::TextArea:
        [_traits setIsSingleLineDocument:NO];
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
#endif
    case WebKit::InputType::Date:
    case WebKit::InputType::DateTime:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Drawing:
    case WebKit::InputType::Email:
    case WebKit::InputType::Month:
    case WebKit::InputType::Number:
    case WebKit::InputType::NumberPad:
    case WebKit::InputType::Password:
    case WebKit::InputType::Phone:
    case WebKit::InputType::Search:
    case WebKit::InputType::Select:
    case WebKit::InputType::Text:
    case WebKit::InputType::Time:
    case WebKit::InputType::URL:
    case WebKit::InputType::Week:
        [_traits setIsSingleLineDocument:YES];
        break;
    case WebKit::InputType::None:
        break;
    }

    [self _updateInteractionTintColor];

    return _traits.get();
}

- (UITextInteractionAssistant *)interactionAssistant
{
    return _textInteractionAssistant.get();
}

- (id<UISelectionInteractionAssistant>)selectionInteractionAssistant
{
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

- (BOOL)_isTextInputContextFocused:(_WKTextInputContext *)context
{
    ASSERT(context);
    // We ignore bounding rect changes as the bounding rect of the focused element is not kept up-to-date.
    return self._hasFocusedElement && context._textInputContext.isSameElement(_focusedElementInformation.elementContext);
}

- (void)_focusTextInputContext:(_WKTextInputContext *)context placeCaretAt:(CGPoint)point completionHandler:(void (^)(UIResponder<UITextInput> *))completionHandler
{
    ASSERT(context);
    // This function can be called more than once during a text interaction (e.g. <rdar://problem/59430806>).
    if (![self becomeFirstResponder]) {
        completionHandler(nil);
        return;
    }
    if ([self _isTextInputContextFocused:context]) {
        completionHandler(_focusedElementInformation.isReadOnly ? nil : self);
        return;
    }
    _usingGestureForSelection = YES;
    auto checkFocusedElement = [weakSelf = WeakObjCPtr<WKContentView> { self }, context = adoptNS([context copy]), completionHandler = makeBlockPtr(completionHandler)] (bool success) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completionHandler(nil);
            return;
        }
        bool isFocused = success && [strongSelf _isTextInputContextFocused:context.get()];
        bool isEditable = success && !strongSelf->_focusedElementInformation.isReadOnly;
        strongSelf->_textInteractionDidChangeFocusedElement |= isFocused;
        strongSelf->_usingGestureForSelection = NO;
        completionHandler(isFocused && isEditable ? strongSelf.get() : nil);
    };
    _page->focusTextInputContextAndPlaceCaret(context._textInputContext, WebCore::IntPoint { point }, WTFMove(checkFocusedElement));
}

- (void)_requestTextInputContextsInRect:(CGRect)rect completionHandler:(void (^)(NSArray<_WKTextInputContext *> *))completionHandler
{
#if ENABLE(EDITABLE_REGION)
    if (!self.webView._editable && !WebKit::mayContainEditableElementsInRect(self, rect)) {
        completionHandler(@[ ]);
        return;
    }
#endif
    _page->textInputContextsInRect(rect, [weakSelf = WeakObjCPtr<WKContentView>(self), completionHandler = makeBlockPtr(completionHandler)] (const Vector<WebCore::ElementContext>& contexts) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || contexts.isEmpty()) {
            completionHandler(@[ ]);
            return;
        }
        completionHandler(createNSArray(contexts, [] (const auto& context) {
            return adoptNS([[_WKTextInputContext alloc] _initWithTextInputContext:context]);
        }).get());
    });
}

- (void)_willBeginTextInteractionInTextInputContext:(_WKTextInputContext *)context
{
    ASSERT(context);
    _textInteractionDidChangeFocusedElement = NO;
    _page->setShouldRevealCurrentSelectionAfterInsertion(false);
    _page->setCanShowPlaceholder(context._textInputContext, false);
    _usingGestureForSelection = YES;
    _textInteractionIsHappening = YES;
}

- (void)_didFinishTextInteractionInTextInputContext:(_WKTextInputContext *)context
{
    ASSERT(context);
    _textInteractionIsHappening = NO;
    _usingGestureForSelection = NO;
    _page->setCanShowPlaceholder(context._textInputContext, true);
    if (_textInteractionDidChangeFocusedElement) {
        // Mark to zoom to reveal the newly focused element on the next editor state update.
        // Then tell the web process to reveal the current selection, which will send us (the
        // UI process) an editor state update.
        _page->setWaitingForPostLayoutEditorStateUpdateAfterFocusingElement(true);
        _textInteractionDidChangeFocusedElement = NO;
    }
    _page->setShouldRevealCurrentSelectionAfterInsertion(true);
}

#if USE(UIKIT_KEYBOARD_ADDITIONS)

- (void)modifierFlagsDidChangeFrom:(UIKeyModifierFlags)oldFlags to:(UIKeyModifierFlags)newFlags
{
    auto dispatchSyntheticFlagsChangedEvents = [&] (UIKeyModifierFlags flags, bool keyDown) {
        if (flags & UIKeyModifierShift)
            [self handleKeyWebEvent:adoptNS([[WKSyntheticFlagsChangedWebEvent alloc] initWithShiftState:keyDown]).get()];
        if (flags & UIKeyModifierAlphaShift)
            [self handleKeyWebEvent:adoptNS([[WKSyntheticFlagsChangedWebEvent alloc] initWithCapsLockState:keyDown]).get()];
    };

    UIKeyModifierFlags removedFlags = oldFlags & ~newFlags;
    UIKeyModifierFlags addedFlags = newFlags & ~oldFlags;
    if (removedFlags)
        dispatchSyntheticFlagsChangedEvents(removedFlags, false);
    if (addedFlags)
        dispatchSyntheticFlagsChangedEvents(addedFlags, true);
}

- (BOOL)shouldSuppressUpdateCandidateView
{
    return _candidateViewNeedsUpdate;
}

#endif

// Web events.
- (BOOL)requiresKeyEvents
{
    return YES;
}

- (void)_handleKeyUIEvent:(::UIEvent *)event
{
    bool isHardwareKeyboardEvent = !!event._hidEvent;
    // We only want to handle key event from the hardware keyboard when we are
    // first responder and we are not interacting with editable content.
    if ([self isFirstResponder] && isHardwareKeyboardEvent && (_inputPeripheral || !_page->editorState().isContentEditable)) {
        if ([_inputPeripheral respondsToSelector:@selector(handleKeyEvent:)]) {
            if ([_inputPeripheral handleKeyEvent:event])
                return;
        }
#if USE(UIKIT_KEYBOARD_ADDITIONS)
        if (!_seenHardwareKeyDownInNonEditableElement) {
            _seenHardwareKeyDownInNonEditableElement = YES;
            [self reloadInputViews];
        }
        [super _handleKeyUIEvent:event];
#else
        [self handleKeyEvent:event];
#endif
        return;
    }

    [super _handleKeyUIEvent:event];
}

- (void)generateSyntheticEditingCommand:(WebKit::SyntheticEditingCommandType)command
{
    _page->generateSyntheticEditingCommand(command);
}

#if !USE(UIKIT_KEYBOARD_ADDITIONS)
- (void)handleKeyEvent:(::UIEvent *)event
{
    // WebCore has already seen the event, no need for custom processing.
    if (event == _uiEventBeingResent)
        return;

    auto webEvent = adoptNS([[WKWebEvent alloc] initWithEvent:event]);
    
    [self handleKeyWebEvent:webEvent.get()];
}
#endif

- (void)handleKeyWebEvent:(::WebEvent *)theEvent
{
    _page->handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(theEvent, WebKit::NativeWebKeyboardEvent::HandledByInputMethod::No));
}

- (void)handleKeyWebEvent:(::WebEvent *)theEvent withCompletionHandler:(void (^)(::WebEvent *theEvent, BOOL wasHandled))completionHandler
{
    if (!isUIThread()) {
        RELEASE_LOG_FAULT(KeyHandling, "%s was invoked on a background thread.", __PRETTY_FUNCTION__);
        completionHandler(theEvent, NO);
        return;
    }

    [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];

    using HandledByInputMethod = WebKit::NativeWebKeyboardEvent::HandledByInputMethod;
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    auto* keyboard = [UIKeyboardImpl sharedInstance];
    if (_page->editorState().isContentEditable && [keyboard respondsToSelector:@selector(handleKeyInputMethodCommandForCurrentEvent)] && [keyboard handleKeyInputMethodCommandForCurrentEvent]) {
        completionHandler(theEvent, YES);
        _page->handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(theEvent, HandledByInputMethod::Yes));
        return;
    }
#endif
    _keyWebEventHandler = makeBlockPtr(completionHandler);
    _page->handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(theEvent, HandledByInputMethod::No));
}

- (void)_didHandleKeyEvent:(::WebEvent *)event eventWasHandled:(BOOL)eventWasHandled
{
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    if ([event isKindOfClass:[WKSyntheticFlagsChangedWebEvent class]])
        return;
#endif

    if (!(event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged))
        [_keyboardScrollingAnimator handleKeyEvent:event];
    
    if (auto handler = WTFMove(_keyWebEventHandler)) {
        handler(event, eventWasHandled);
        return;
    }

#if !USE(UIKIT_KEYBOARD_ADDITIONS)
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
#endif
}

- (BOOL)_interpretKeyEvent:(::WebEvent *)event isCharEvent:(BOOL)isCharEvent
{
    if (event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged)
        return NO;

    BOOL contentEditable = _page->editorState().isContentEditable;

    if (!contentEditable && event.isTabKey)
        return NO;

    if ([_keyboardScrollingAnimator beginWithEvent:event] || [_keyboardScrollingAnimator scrollTriggeringKeyIsPressed])
        return YES;

    UIKeyboardImpl *keyboard = [UIKeyboardImpl sharedInstance];

    if (!isCharEvent && [keyboard respondsToSelector:@selector(handleKeyTextCommandForCurrentEvent)] && [keyboard handleKeyTextCommandForCurrentEvent])
        return YES;
    if (isCharEvent && [keyboard respondsToSelector:@selector(handleKeyAppCommandForCurrentEvent)] && [keyboard handleKeyAppCommandForCurrentEvent])
        return YES;

    // Don't insert character for an unhandled Command-key key command. This matches iOS and Mac platform conventions.
    if (event.modifierFlags & WebEventFlagMaskCommandKey)
        return NO;

    NSString *characters = event.characters;
    if (!characters.length)
        return NO;

    switch ([characters characterAtIndex:0]) {
    case NSBackspaceCharacter:
    case NSDeleteCharacter:
        if (contentEditable) {
            [keyboard deleteFromInputWithFlags:event.keyboardFlags];
            return YES;
        }
        break;
    case NSEnterCharacter:
    case NSCarriageReturnCharacter:
        if (contentEditable && isCharEvent) {
            // Map \r from HW keyboard to \n to match the behavior of the soft keyboard.
            [keyboard addInputString:@"\n" withFlags:0 withInputManagerHint:nil];
            return YES;
        }
        break;
    default:
        if (contentEditable && isCharEvent) {
            [keyboard addInputString:event.characters withFlags:event.keyboardFlags withInputManagerHint:event.inputManagerHint];
            return YES;
        }
        break;
    }

    return NO;
}

- (void)dismissFilePicker
{
    [_fileUploadPanel dismiss];
}

- (BOOL)isScrollableForKeyboardScrollViewAnimator:(WKKeyboardScrollViewAnimator *)animator
{
    if (_page->editorState().isContentEditable)
        return NO;

    if (_focusedElementInformation.elementType == WebKit::InputType::Select)
        return NO;

    if (!self.webView.scrollView.scrollEnabled)
        return NO;

    return YES;
}

- (CGFloat)keyboardScrollViewAnimator:(WKKeyboardScrollViewAnimator *)animator distanceForIncrement:(WebKit::ScrollingIncrement)increment inDirection:(WebKit::ScrollingDirection)direction
{
    BOOL directionIsHorizontal = direction == WebKit::ScrollingDirection::Left || direction == WebKit::ScrollingDirection::Right;

    switch (increment) {
    case WebKit::ScrollingIncrement::Document: {
        CGSize documentSize = [self convertRect:self.bounds toView:self.webView].size;
        return directionIsHorizontal ? documentSize.width : documentSize.height;
    }
    case WebKit::ScrollingIncrement::Page: {
        CGSize pageSize = [self convertSize:CGSizeMake(0, WebCore::Scrollbar::pageStep(_page->unobscuredContentRect().height(), self.bounds.size.height)) toView:self.webView];
        return directionIsHorizontal ? pageSize.width : pageSize.height;
    }
    case WebKit::ScrollingIncrement::Line:
        return [self convertSize:CGSizeMake(0, WebCore::Scrollbar::pixelsPerLineStep()) toView:self.webView].height;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

- (void)keyboardScrollViewAnimatorWillScroll:(WKKeyboardScrollViewAnimator *)animator
{
    [self willStartZoomOrScroll];
}

- (void)keyboardScrollViewAnimatorDidFinishScrolling:(WKKeyboardScrollViewAnimator *)animator
{
    [_webView _didFinishScrolling];
}

- (void)executeEditCommandWithCallback:(NSString *)commandName
{
    // FIXME: Editing commands are not considered by WebKit as user initiated even if they are the result
    // of keydown or keyup. We need to query the keyboard to determine if this was called from the keyboard
    // or not to know whether to tell WebKit to treat this command as user initiated or not.
    [self beginSelectionChange];
    RetainPtr<WKContentView> view = self;
    _page->executeEditCommand(commandName, { }, [view] {
        [view endSelectionChange];
    });
}

- (void)_deleteByWord
{
    [self executeEditCommandWithCallback:@"deleteWordBackward"];
}

- (void)_deleteToStartOfLine
{
    [self executeEditCommandWithCallback:@"deleteToBeginningOfLine"];
}

- (void)_deleteToEndOfLine
{
    [self executeEditCommandWithCallback:@"deleteToEndOfLine"];
}

- (void)_deleteForwardAndNotify:(BOOL)notify
{
    [self executeEditCommandWithCallback:@"deleteForward"];
}

- (void)_deleteToEndOfParagraph
{
    [self executeEditCommandWithCallback:@"deleteToEndOfParagraph"];
}

- (void)_transpose
{
    [self executeEditCommandWithCallback:@"transpose"];
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
    [self executeEditCommandWithCallback:extending ? @"moveBackwardAndModifySelection" : @"moveBackward"];
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
    [self executeEditCommandWithCallback:extending ? @"moveForwardAndModifySelection" : @"moveForward"];
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
    return [self unscaledView];
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
    UIFont *font = _autocorrectionData.font.get();
    double zoomScale = self._contentZoomScale;
    return std::abs(zoomScale - 1) > FLT_EPSILON ? [font fontWithSize:font.pointSize * zoomScale] : font;
}

- (BOOL)hasSelection
{
    return NO;
}

- (BOOL)isPosition:(UITextPosition *)position atBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction
{
    if (granularity == UITextGranularityParagraph) {
        if (direction == UITextStorageDirectionBackward && [position isEqual:self.selectedTextRange.start])
            return _page->editorState().postLayoutData().selectionStartIsAtParagraphBoundary;

        if (direction == UITextStorageDirectionForward && [position isEqual:self.selectedTextRange.end])
            return _page->editorState().postLayoutData().selectionEndIsAtParagraphBoundary;
    }

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

- (void)_showKeyboard
{
    [self setUpTextSelectionAssistant];
    
    if (self.isFirstResponder && !_suppressSelectionAssistantReasons)
        [_textInteractionAssistant activateSelection];

#if !PLATFORM(WATCHOS)
    [self reloadInputViews];
#endif
}

- (void)_hideKeyboard
{
    self.inputDelegate = nil;
    [self setUpTextSelectionAssistant];
    
    [_textInteractionAssistant deactivateSelection];
    [_formAccessoryView hideAutoFillButton];

    // FIXME: Does it make sense to call -reloadInputViews on watchOS?
    [self reloadInputViews];
    if (_formAccessoryView)
        [self _updateAccessory];
}

- (const WebKit::FocusedElementInformation&)focusedElementInformation
{
    return _focusedElementInformation;
}

- (Vector<WebKit::OptionItem>&)focusedSelectElementOptions
{
    return _focusedElementInformation.selectOptions;
}

// Note that selectability is also affected by the CSS property user-select.
static bool mayContainSelectableText(WebKit::InputType type)
{
    switch (type) {
    case WebKit::InputType::None:
    // The following types have custom UI and do not look or behave like a text field.
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
#endif
    case WebKit::InputType::Date:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Drawing:
    case WebKit::InputType::Month:
    case WebKit::InputType::Select:
    case WebKit::InputType::Time:
        return false;
    // The following types look and behave like a text field.
    case WebKit::InputType::ContentEditable:
    case WebKit::InputType::Email:
    case WebKit::InputType::Number:
    case WebKit::InputType::NumberPad:
    case WebKit::InputType::Password:
    case WebKit::InputType::Phone:
    case WebKit::InputType::Search:
    case WebKit::InputType::Text:
    case WebKit::InputType::TextArea:
    case WebKit::InputType::URL:
    case WebKit::InputType::DateTime:
    case WebKit::InputType::Week:
        return true;
    }
}

static bool shouldShowKeyboardForElement(const WebKit::FocusedElementInformation& information)
{
    if (information.inputMode == WebCore::InputMode::None)
        return false;

    if (information.elementType == WebKit::InputType::Drawing)
        return false;

    if (mayContainSelectableText(information.elementType))
        return true;

    return !WebKit::currentUserInterfaceIdiomIsPad();
}

static WebCore::FloatRect rectToRevealWhenZoomingToFocusedElement(const WebKit::FocusedElementInformation& elementInfo, const WebKit::EditorState& editorState)
{
    WebCore::IntRect elementInteractionRect;
    if (elementInfo.interactionRect.contains(elementInfo.lastInteractionLocation))
        elementInteractionRect = { elementInfo.lastInteractionLocation, { 1, 1 } };

    if (!mayContainSelectableText(elementInfo.elementType))
        return elementInteractionRect;

    if (editorState.isMissingPostLayoutData) {
        ASSERT_NOT_REACHED();
        return elementInteractionRect;
    }

    if (editorState.selectionIsNone)
        return { };

    WebCore::FloatRect selectionBoundingRect;
    auto& postLayoutData = editorState.postLayoutData();
    if (editorState.selectionIsRange) {
        for (auto& rect : postLayoutData.selectionRects)
            selectionBoundingRect.unite(rect.rect());
    } else
        selectionBoundingRect = postLayoutData.caretRectAtStart;

    selectionBoundingRect.intersect(elementInfo.interactionRect);
    return selectionBoundingRect;
}

static RetainPtr<NSObject <WKFormPeripheral>> createInputPeripheralWithView(WebKit::InputType type, WKContentView *view)
{
#if PLATFORM(WATCHOS)
    UNUSED_PARAM(type);
    UNUSED_PARAM(view);
    return nil;
#else
    switch (type) {
    case WebKit::InputType::Select:
        return adoptNS([[WKFormSelectControl alloc] initWithView:view]);
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
        return adoptNS([[WKFormColorControl alloc] initWithView:view]);
#endif // ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Date:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Month:
    case WebKit::InputType::Time:
        return adoptNS([[WKDateTimeInputControl alloc] initWithView:view]);
    default:
        return nil;
    }
#endif
}

- (void)_elementDidFocus:(const WebKit::FocusedElementInformation&)information userIsInteracting:(BOOL)userIsInteracting blurPreviousNode:(BOOL)blurPreviousNode activityStateChanges:(OptionSet<WebCore::ActivityState::Flag>)activityStateChanges userObject:(NSObject <NSSecureCoding> *)userObject
{
    SetForScope<BOOL> isChangingFocusForScope { _isChangingFocus, self._hasFocusedElement };
    SetForScope<BOOL> isFocusingElementWithKeyboardForScope { _isFocusingElementWithKeyboard, shouldShowKeyboardForElement(information) };

    auto inputViewUpdateDeferrer = std::exchange(_inputViewUpdateDeferrer, nullptr);

    _didAccessoryTabInitiateFocus = _isChangingFocusUsingAccessoryTab;

    id <_WKInputDelegate> inputDelegate = [_webView _inputDelegate];
    RetainPtr<WKFocusedElementInfo> focusedElementInfo = adoptNS([[WKFocusedElementInfo alloc] initWithFocusedElementInformation:information isUserInitiated:userIsInteracting userObject:userObject]);

    _WKFocusStartsInputSessionPolicy startInputSessionPolicy = _WKFocusStartsInputSessionPolicyAuto;

    if ([inputDelegate respondsToSelector:@selector(_webView:focusShouldStartInputSession:)]) {
        if ([inputDelegate _webView:self.webView focusShouldStartInputSession:focusedElementInfo.get()])
            startInputSessionPolicy = _WKFocusStartsInputSessionPolicyAllow;
        else
            startInputSessionPolicy = _WKFocusStartsInputSessionPolicyDisallow;
    }

    if ([inputDelegate respondsToSelector:@selector(_webView:decidePolicyForFocusedElement:)])
        startInputSessionPolicy = [inputDelegate _webView:self.webView decidePolicyForFocusedElement:focusedElementInfo.get()];

    BOOL shouldShowInputView = [&] {
        switch (startInputSessionPolicy) {
        case _WKFocusStartsInputSessionPolicyAuto:
            // The default behavior is to allow node assistance if the user is interacting.
            // We also allow node assistance if the keyboard already is showing, unless we're in extra zoom mode.
            if (userIsInteracting)
                return YES;

            if (self.isFirstResponder || _becomingFirstResponder) {
                // When the software keyboard is being used to enter an url, only the focus activity state is changing.
                // In this case, auto focus on the page being navigated to should be disabled, unless a hardware
                // keyboard is attached.
                if (activityStateChanges && activityStateChanges != WebCore::ActivityState::IsFocused)
                    return YES;

#if PLATFORM(WATCHOS)
                if (_isChangingFocus && ![_focusedFormControlView isHidden])
                    return YES;
#else
                if (_isChangingFocus)
                    return YES;

                if ([UIKeyboard isInHardwareKeyboardMode])
                    return YES;
#endif
            }
            return NO;
        case _WKFocusStartsInputSessionPolicyAllow:
            return YES;
        case _WKFocusStartsInputSessionPolicyDisallow:
            return NO;
        default:
            ASSERT_NOT_REACHED();
            return NO;
        }
    }();

    if (blurPreviousNode) {
        // Defer view updates until the end of this function to avoid a noticeable flash when switching focus
        // between elements that require the keyboard.
        if (!inputViewUpdateDeferrer)
            inputViewUpdateDeferrer = makeUnique<WebKit::InputViewUpdateDeferrer>(self);
        [self _elementDidBlur];
    }

#if HAVE(PENCILKIT)
    if (information.elementType == WebKit::InputType::Drawing)
        [_drawingCoordinator installInkPickerForDrawing:information.embeddedViewID];
#endif

    if (!shouldShowInputView || information.elementType == WebKit::InputType::None) {
        _page->setIsShowingInputViewForFocusedElement(false);
        return;
    }

    _page->setIsShowingInputViewForFocusedElement(true);

    // FIXME: We should remove this check when we manage to send ElementDidFocus from the WebProcess
    // only when it is truly time to show the keyboard.
    if (_focusedElementInformation.elementType == information.elementType && _focusedElementInformation.interactionRect == information.interactionRect) {
        if (_inputPeripheral) {
            if (!self.isFirstResponder)
                [self becomeFirstResponder];
            [self accessoryOpen];
        }
        return;
    }

    [_webView _resetFocusPreservationCount];

    _focusRequiresStrongPasswordAssistance = NO;
    _additionalContextForStrongPasswordAssistance = nil;
    if ([inputDelegate respondsToSelector:@selector(_webView:focusRequiresStrongPasswordAssistance:)])
        _focusRequiresStrongPasswordAssistance = [inputDelegate _webView:self.webView focusRequiresStrongPasswordAssistance:focusedElementInfo.get()];

    if ([inputDelegate respondsToSelector:@selector(_webViewAdditionalContextForStrongPasswordAssistance:)])
        _additionalContextForStrongPasswordAssistance = [inputDelegate _webViewAdditionalContextForStrongPasswordAssistance:self.webView];
    else
        _additionalContextForStrongPasswordAssistance = @{ };

    bool delegateImplementsWillStartInputSession = [inputDelegate respondsToSelector:@selector(_webView:willStartInputSession:)];
    bool delegateImplementsDidStartInputSession = [inputDelegate respondsToSelector:@selector(_webView:didStartInputSession:)];

    if (delegateImplementsWillStartInputSession || delegateImplementsDidStartInputSession) {
        [_formInputSession invalidate];
        _formInputSession = adoptNS([[WKFormInputSession alloc] initWithContentView:self focusedElementInfo:focusedElementInfo.get() requiresStrongPasswordAssistance:_focusRequiresStrongPasswordAssistance]);
    }

    if (delegateImplementsWillStartInputSession)
        [inputDelegate _webView:self.webView willStartInputSession:_formInputSession.get()];

    BOOL isSelectable = mayContainSelectableText(information.elementType);
    BOOL editableChanged = [self setIsEditable:isSelectable];
    _focusedElementInformation = information;
    _traits = nil;

    if (![self isFirstResponder])
        [self becomeFirstResponder];

    _inputPeripheral = createInputPeripheralWithView(_focusedElementInformation.elementType, self);

#if PLATFORM(WATCHOS)
    [self addFocusedFormControlOverlay];
    if (!_isChangingFocus)
        [self presentViewControllerForCurrentFocusedElement];
#else
    [self reloadInputViews];
#endif

    if (isSelectable)
        [self _showKeyboard];

    // The custom fixed position rect behavior is affected by -isFocusingElement, so if that changes we need to recompute rects.
    if (editableChanged)
        [_webView _scheduleVisibleContentRectUpdate];

    // For elements that have selectable content (e.g. text field) we need to wait for the web process to send an up-to-date
    // selection rect before we can zoom and reveal the selection. Non-selectable elements (e.g. <select>) can be zoomed
    // immediately because they have no selection to reveal.
    BOOL needsEditorStateUpdate = mayContainSelectableText(_focusedElementInformation.elementType);
    if (needsEditorStateUpdate)
        _page->setWaitingForPostLayoutEditorStateUpdateAfterFocusingElement(true);
    else
        [self _zoomToRevealFocusedElement];

    [self _updateAccessory];

#if PLATFORM(WATCHOS)
    if (_isChangingFocus)
        [_focusedFormControlView reloadData:YES];
#endif

    // _inputPeripheral has been initialized in inputView called by reloadInputViews.
    [_inputPeripheral beginEditing];

    if (delegateImplementsDidStartInputSession)
        [inputDelegate _webView:self.webView didStartInputSession:_formInputSession.get()];
    
    [_webView didStartFormControlInteraction];
}

- (void)_elementDidBlur
{
    SetForScope<BOOL> isBlurringFocusedElementForScope { _isBlurringFocusedElement, YES };

#if HAVE(PENCILKIT)
    [_drawingCoordinator uninstallInkPicker];
#endif

#if USE(UIKIT_KEYBOARD_ADDITIONS)
    [self _endEditing];
#endif

    [_formInputSession invalidate];
    _formInputSession = nil;

#if ENABLE(DATALIST_ELEMENT)
    _dataListTextSuggestionsInputView = nil;
    _dataListTextSuggestions = nil;
#endif

    BOOL editableChanged = [self setIsEditable:NO];
    // FIXME: We should completely invalidate _focusedElementInformation here, instead of a subset of individual members.
    _focusedElementInformation.elementType = WebKit::InputType::None;
    _focusedElementInformation.shouldSynthesizeKeyEventsForEditing = false;
    _focusedElementInformation.shouldAvoidResizingWhenInputViewBoundsChange = false;
    _focusedElementInformation.shouldAvoidScrollingWhenFocusedContentIsVisible = false;
    _focusedElementInformation.shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation = false;
    _inputPeripheral = nil;
    _focusRequiresStrongPasswordAssistance = NO;
    _additionalContextForStrongPasswordAssistance = nil;

#if USE(UIKIT_KEYBOARD_ADDITIONS)
    // When defocusing an editable element reset a seen keydown before calling -_hideKeyboard so that we
    // re-evaluate whether we still need a keyboard when UIKit calls us back in -_requiresKeyboardWhenFirstResponder.
    if (editableChanged)
        _seenHardwareKeyDownInNonEditableElement = NO;
#endif

    [self _hideKeyboard];

#if PLATFORM(WATCHOS)
    [self dismissAllInputViewControllers:YES];
    if (!_isChangingFocus)
        [self removeFocusedFormControlOverlay];
#endif

    if (editableChanged) {
        // The custom fixed position rect behavior is affected by -isFocusingElement, so if that changes we need to recompute rects.
        [_webView _scheduleVisibleContentRectUpdate];

        [_webView didEndFormControlInteraction];
        _page->setIsShowingInputViewForFocusedElement(false);
    }

    _page->setWaitingForPostLayoutEditorStateUpdateAfterFocusingElement(false);

    if (!_isChangingFocus)
        _didAccessoryTabInitiateFocus = NO;
}

- (void)_updateInputContextAfterBlurringAndRefocusingElement
{
    if (!self._hasFocusedElement || !_suppressSelectionAssistantReasons)
        return;

    [UIKeyboardImpl.activeInstance updateForChangedSelection];
}

- (BOOL)shouldIgnoreKeyboardWillHideNotification
{
    // Ignore keyboard will hide notifications sent during rotation. They're just there for
    // backwards compatibility reasons and processing the will hide notification would
    // temporarily screw up the unobscured view area.
    if (UIPeripheralHost.sharedInstance.rotationState)
        return YES;

    if (_isChangingFocus && _isFocusingElementWithKeyboard)
        return YES;

    return NO;
}

- (void)_hardwareKeyboardAvailabilityChanged
{
#if USE(UIKIT_KEYBOARD_ADDITIONS)
    _seenHardwareKeyDownInNonEditableElement = NO;
#endif
    [self reloadInputViews];
}

- (void)_didUpdateInputMode:(WebCore::InputMode)mode
{
    if (!self.inputDelegate || !self._hasFocusedElement)
        return;

#if !PLATFORM(WATCHOS)
    _focusedElementInformation.inputMode = mode;
    [self reloadInputViews];
#endif
}

static BOOL allPasteboardItemOriginsMatchOrigin(UIPasteboard *pasteboard, const String& originIdentifier)
{
    if (originIdentifier.isEmpty())
        return NO;

    auto *indices = [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, [pasteboard numberOfItems])];
    auto *allCustomData = [pasteboard dataForPasteboardType:@(WebCore::PasteboardCustomData::cocoaType()) inItemSet:indices];
    if (!allCustomData.count)
        return NO;

    BOOL foundAtLeastOneMatchingIdentifier = NO;
    for (NSData *data in allCustomData) {
        if (!data.length)
            continue;

        auto buffer = WebCore::SharedBuffer::create(data);
        if (WebCore::PasteboardCustomData::fromSharedBuffer(buffer.get()).origin() != originIdentifier)
            return NO;

        foundAtLeastOneMatchingIdentifier = YES;
    }

    return foundAtLeastOneMatchingIdentifier;
}

- (void)_requestDOMPasteAccessWithElementRect:(const WebCore::IntRect&)elementRect originIdentifier:(const String&)originIdentifier completionHandler:(CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&)completionHandler
{
    if (auto existingCompletionHandler = std::exchange(_domPasteRequestHandler, WTFMove(completionHandler))) {
        ASSERT_NOT_REACHED();
        existingCompletionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
    }

    if (allPasteboardItemOriginsMatchOrigin(UIPasteboard.generalPasteboard, originIdentifier)) {
        [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::GrantedForCommand];
        return;
    }

    WebCore::IntRect menuControllerRect = elementRect;

    const CGFloat maximumElementWidth = 300;
    const CGFloat maximumElementHeight = 120;
    if (elementRect.isEmpty() || elementRect.width() > maximumElementWidth || elementRect.height() > maximumElementHeight) {
        const CGFloat interactionLocationMargin = 10;
        menuControllerRect = { WebCore::IntPoint(_lastInteractionLocation), { } };
        menuControllerRect.inflate(interactionLocationMargin);
    }

    [UIMenuController.sharedMenuController showMenuFromView:self rect:menuControllerRect];
}

- (void)_didUpdateEditorState
{
    [self _updateInitialWritingDirectionIfNecessary];

    // FIXME: If the initial writing direction just changed, we should wait until we get the next post-layout editor state
    // before zooming to reveal the selection rect.
    if (mayContainSelectableText(_focusedElementInformation.elementType))
        [self _zoomToRevealFocusedElement];
}

- (void)_updateInitialWritingDirectionIfNecessary
{
    if (!_page->isEditable())
        return;

    auto& editorState = _page->editorState();
    if (editorState.selectionIsNone || editorState.selectionIsRange)
        return;

    UIKeyboardImpl *keyboard = UIKeyboardImpl.activeInstance;
    if (keyboard.delegate != self)
        return;

    // Synchronize the keyboard's writing direction with the newly received EditorState.
    [keyboard setInitialDirection];
}

- (void)updateCurrentFocusedElementInformation:(Function<void(bool didUpdate)>&&)callback
{
    WeakObjCPtr<WKContentView> weakSelf { self };
    auto identifierBeforeUpdate = _focusedElementInformation.focusedElementIdentifier;
    _page->requestFocusedElementInformation([callback = WTFMove(callback), identifierBeforeUpdate, weakSelf] (auto& info, auto error) {
        if (!weakSelf || error != WebKit::CallbackBase::Error::None || info.focusedElementIdentifier != identifierBeforeUpdate) {
            // If the focused element may have changed in the meantime, don't overwrite focused element information.
            callback(false);
            return;
        }

        weakSelf.get()->_focusedElementInformation = info;
        callback(true);
    });
}

- (void)reloadContextViewForPresentedListViewController
{
#if PLATFORM(WATCHOS)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTextInputListViewController class]])
        [(WKTextInputListViewController *)_presentedFullScreenInputViewController.get() reloadContextView];
#endif
}

#if PLATFORM(WATCHOS)

- (void)addFocusedFormControlOverlay
{
    if (_focusedFormControlView)
        return;

    _activeFocusedStateRetainBlock = makeBlockPtr(self.webView._retainActiveFocusedState);

    _focusedFormControlView = adoptNS([[WKFocusedFormControlView alloc] initWithFrame:self.webView.bounds delegate:self]);
    [_focusedFormControlView hide:NO];
    [_webView addSubview:_focusedFormControlView.get()];
    [self setInputDelegate:_focusedFormControlView.get()];
}

- (void)removeFocusedFormControlOverlay
{
    if (!_focusedFormControlView)
        return;

    if (auto releaseActiveFocusState = WTFMove(_activeFocusedStateRetainBlock))
        releaseActiveFocusState();

    [_focusedFormControlView removeFromSuperview];
    _focusedFormControlView = nil;
    [self setInputDelegate:nil];
}

- (void)presentViewControllerForCurrentFocusedElement
{
    [self dismissAllInputViewControllers:NO];

    _shouldRestoreFirstResponderStatusAfterLosingFocus = self.isFirstResponder;
    UIViewController *presentingViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:self];

    ASSERT(!_presentedFullScreenInputViewController);

    BOOL prefersModalPresentation = NO;

    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::Select:
        _presentedFullScreenInputViewController = adoptNS([[WKSelectMenuListViewController alloc] initWithDelegate:self]);
        break;
    case WebKit::InputType::Time:
        // Time inputs are special, in that the only UI affordances for dismissal are push buttons rather than status bar chevrons.
        // As such, modal presentation and dismissal is preferred even if a navigation stack exists.
        prefersModalPresentation = YES;
        _presentedFullScreenInputViewController = adoptNS([[WKTimePickerViewController alloc] initWithDelegate:self]);
        break;
    case WebKit::InputType::Date:
        _presentedFullScreenInputViewController = adoptNS([[WKDatePickerViewController alloc] initWithDelegate:self]);
        break;
    case WebKit::InputType::None:
        break;
    default:
        _presentedFullScreenInputViewController = adoptNS([[WKTextInputListViewController alloc] initWithDelegate:self]);
        break;
    }

    ASSERT(_presentedFullScreenInputViewController);
    ASSERT(presentingViewController);

    if (!prefersModalPresentation && [presentingViewController isKindOfClass:[UINavigationController class]])
        _inputNavigationViewControllerForFullScreenInputs = (UINavigationController *)presentingViewController;
    else
        _inputNavigationViewControllerForFullScreenInputs = nil;

    // Present the input view controller on an existing navigation stack, if possible. If there is no navigation stack we can use, fall back to presenting modally.
    // This is because the HI specification (for certain scenarios) calls for navigation-style view controller presentation, but WKWebView can't make any guarantees
    // about clients' view controller hierarchies, so we can only try our best to avoid presenting modally. Clients can implicitly opt in to specced behavior by using
    // UINavigationController to present the web view.
    if (_inputNavigationViewControllerForFullScreenInputs)
        [_inputNavigationViewControllerForFullScreenInputs pushViewController:_presentedFullScreenInputViewController.get() animated:YES];
    else
        [presentingViewController presentViewController:_presentedFullScreenInputViewController.get() animated:YES completion:nil];

    // Presenting a fullscreen input view controller fully obscures the web view. Without taking this token, the web content process will get backgrounded.
    _page->process().startBackgroundActivityForFullscreenInput();

    [presentingViewController.transitionCoordinator animateAlongsideTransition:nil completion:[weakWebView = WeakObjCPtr<WKWebView>(_webView), controller = _presentedFullScreenInputViewController] (id <UIViewControllerTransitionCoordinatorContext>) {
        auto strongWebView = weakWebView.get();
        id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([strongWebView UIDelegate]);
        if ([uiDelegate respondsToSelector:@selector(_webView:didPresentFocusedElementViewController:)])
            [uiDelegate _webView:strongWebView.get() didPresentFocusedElementViewController:controller.get()];
    }];
}

- (void)dismissAllInputViewControllers:(BOOL)animated
{
    auto navigationController = WTFMove(_inputNavigationViewControllerForFullScreenInputs);
    auto presentedController = WTFMove(_presentedFullScreenInputViewController);

    if (!presentedController)
        return;

    if ([navigationController viewControllers].lastObject == presentedController.get())
        [navigationController popViewControllerAnimated:animated];
    else
        [presentedController dismissViewControllerAnimated:animated completion:nil];

    [[presentedController transitionCoordinator] animateAlongsideTransition:nil completion:[weakWebView = WeakObjCPtr<WKWebView>(_webView), controller = presentedController] (id <UIViewControllerTransitionCoordinatorContext>) {
        auto strongWebView = weakWebView.get();
        id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([strongWebView UIDelegate]);
        if ([uiDelegate respondsToSelector:@selector(_webView:didDismissFocusedElementViewController:)])
            [uiDelegate _webView:strongWebView.get() didDismissFocusedElementViewController:controller.get()];
    }];

    if (_shouldRestoreFirstResponderStatusAfterLosingFocus) {
        _shouldRestoreFirstResponderStatusAfterLosingFocus = NO;
        if (!self.isFirstResponder)
            [self becomeFirstResponder];
    }

    _page->process().endBackgroundActivityForFullscreenInput();
}

- (void)focusedFormControlViewDidSubmit:(WKFocusedFormControlView *)view
{
    [self insertText:@"\n"];
    _page->blurFocusedElement();
}

- (void)focusedFormControlViewDidCancel:(WKFocusedFormControlView *)view
{
    _page->blurFocusedElement();
}

- (void)focusedFormControlViewDidBeginEditing:(WKFocusedFormControlView *)view
{
    [self updateCurrentFocusedElementInformation:[weakSelf = WeakObjCPtr<WKContentView>(self)] (bool didUpdate) {
        if (!didUpdate)
            return;

        auto strongSelf = weakSelf.get();
        [strongSelf presentViewControllerForCurrentFocusedElement];
        [strongSelf->_focusedFormControlView hide:YES];
    }];
}

- (CGRect)rectForFocusedFormControlView:(WKFocusedFormControlView *)view
{
    return [self convertRect:_focusedElementInformation.interactionRect toView:view];
}

- (CGRect)nextRectForFocusedFormControlView:(WKFocusedFormControlView *)view
{
    if (!_focusedElementInformation.hasNextNode)
        return CGRectNull;

    return [self convertRect:_focusedElementInformation.nextNodeRect toView:view];
}

- (CGRect)previousRectForFocusedFormControlView:(WKFocusedFormControlView *)view
{
    if (!_focusedElementInformation.hasPreviousNode)
        return CGRectNull;

    return [self convertRect:_focusedElementInformation.previousNodeRect toView:view];
}

- (UIScrollView *)scrollViewForFocusedFormControlView:(WKFocusedFormControlView *)view
{
    return self._scroller;
}

- (NSString *)actionNameForFocusedFormControlView:(WKFocusedFormControlView *)view
{
    if (_focusedElementInformation.formAction.isEmpty())
        return nil;

    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::Select:
    case WebKit::InputType::Time:
    case WebKit::InputType::Date:
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
#endif
        return nil;
    case WebKit::InputType::Search:
        return WebCore::formControlSearchButtonTitle();
    default:
        return WebCore::formControlGoButtonTitle();
    }
}

- (void)focusedFormControlViewDidRequestNextNode:(WKFocusedFormControlView *)view
{
    if (_focusedElementInformation.hasNextNode)
        _page->focusNextFocusedElement(true);
}

- (void)focusedFormControlViewDidRequestPreviousNode:(WKFocusedFormControlView *)view
{
    if (_focusedElementInformation.hasPreviousNode)
        _page->focusNextFocusedElement(false);
}

- (BOOL)hasNextNodeForFocusedFormControlView:(WKFocusedFormControlView *)view
{
    return _focusedElementInformation.hasNextNode;
}

- (BOOL)hasPreviousNodeForFocusedFormControlView:(WKFocusedFormControlView *)view
{
    return _focusedElementInformation.hasPreviousNode;
}

- (void)focusedFormControllerDidUpdateSuggestions:(WKFocusedFormControlView *)view
{
    if (_isBlurringFocusedElement || ![_presentedFullScreenInputViewController isKindOfClass:[WKTextInputListViewController class]])
        return;

    [(WKTextInputListViewController *)_presentedFullScreenInputViewController reloadTextSuggestions];
}

#pragma mark - WKSelectMenuListViewControllerDelegate

- (void)selectMenu:(WKSelectMenuListViewController *)selectMenu didSelectItemAtIndex:(NSUInteger)index
{
    ASSERT(!_focusedElementInformation.isMultiSelect);
    _page->setFocusedElementSelectedIndex(index, false);
}

- (NSUInteger)numberOfItemsInSelectMenu:(WKSelectMenuListViewController *)selectMenu
{
    return self.focusedSelectElementOptions.size();
}

- (NSString *)selectMenu:(WKSelectMenuListViewController *)selectMenu displayTextForItemAtIndex:(NSUInteger)index
{
    auto& options = self.focusedSelectElementOptions;
    if (index >= options.size()) {
        ASSERT_NOT_REACHED();
        return @"";
    }

    return options[index].text;
}

- (void)selectMenu:(WKSelectMenuListViewController *)selectMenu didCheckItemAtIndex:(NSUInteger)index checked:(BOOL)checked
{
    ASSERT(_focusedElementInformation.isMultiSelect);
    if (index >= self.focusedSelectElementOptions.size()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& option = self.focusedSelectElementOptions[index];
    if (option.isSelected == checked) {
        ASSERT_NOT_REACHED();
        return;
    }

    _page->setFocusedElementSelectedIndex(index, true);
    option.isSelected = checked;
}

- (BOOL)selectMenuUsesMultipleSelection:(WKSelectMenuListViewController *)selectMenu
{
    return _focusedElementInformation.isMultiSelect;
}

- (BOOL)selectMenu:(WKSelectMenuListViewController *)selectMenu hasSelectedOptionAtIndex:(NSUInteger)index
{
    if (index >= self.focusedSelectElementOptions.size()) {
        ASSERT_NOT_REACHED();
        return NO;
    }

    return self.focusedSelectElementOptions[index].isSelected;
}

#endif // PLATFORM(WATCHOS)

- (void)_wheelChangedWithEvent:(UIEvent *)event
{
#if PLATFORM(WATCHOS)
    if ([_focusedFormControlView handleWheelEvent:event])
        return;
#endif
    [super _wheelChangedWithEvent:event];
}

- (void)_updateSelectionAssistantSuppressionState
{
    static const double minimumFocusedElementAreaForSuppressingSelectionAssistant = 4;

    auto& editorState = _page->editorState();
    if (editorState.isMissingPostLayoutData)
        return;

    BOOL editableRootIsTransparentOrFullyClipped = NO;
    BOOL focusedElementIsTooSmall = NO;
    if (!editorState.selectionIsNone) {
        auto& postLayoutData = editorState.postLayoutData();
        if (postLayoutData.editableRootIsTransparentOrFullyClipped)
            editableRootIsTransparentOrFullyClipped = YES;

        if (self._hasFocusedElement) {
            auto elementArea = postLayoutData.focusedElementRect.area<RecordOverflow>();
            if (!elementArea.hasOverflowed() && elementArea < minimumFocusedElementAreaForSuppressingSelectionAssistant)
                focusedElementIsTooSmall = YES;
        }
    }

    if (editableRootIsTransparentOrFullyClipped)
        [self _startSuppressingSelectionAssistantForReason:WebKit::EditableRootIsTransparentOrFullyClipped];
    else
        [self _stopSuppressingSelectionAssistantForReason:WebKit::EditableRootIsTransparentOrFullyClipped];

    if (focusedElementIsTooSmall)
        [self _startSuppressingSelectionAssistantForReason:WebKit::FocusedElementIsTooSmall];
    else
        [self _stopSuppressingSelectionAssistantForReason:WebKit::FocusedElementIsTooSmall];
}

- (void)_selectionChanged
{
    [self _updateSelectionAssistantSuppressionState];

    _selectionNeedsUpdate = YES;
    // If we are changing the selection with a gesture there is no need
    // to wait to paint the selection.
    if (_usingGestureForSelection)
        [self _updateChangedSelection];

#if USE(UIKIT_KEYBOARD_ADDITIONS)
    if (_candidateViewNeedsUpdate) {
        _candidateViewNeedsUpdate = NO;
        if ([self.inputDelegate respondsToSelector:@selector(layoutHasChanged)])
            [(id <UITextInputDelegatePrivate>)self.inputDelegate layoutHasChanged];
    }
#endif
    
    [_webView _didChangeEditorState];
}

- (void)selectWordForReplacement
{
    _page->extendSelection(WebCore::TextGranularity::WordGranularity);
}

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
- (void)replaceSelectionOffset:(NSInteger)selectionOffset length:(NSUInteger)length withAnnotatedString:(NSAttributedString *)annotatedString relativeReplacementRange:(NSRange)relativeReplacementRange
{
    _textCheckingController->replaceRelativeToSelection(annotatedString, selectionOffset, length, relativeReplacementRange.location, relativeReplacementRange.length);
}

- (void)removeAnnotation:(NSAttributedStringKey)annotationName forSelectionOffset:(NSInteger)selectionOffset length:(NSUInteger)length
{
    _textCheckingController->removeAnnotationRelativeToSelection(annotationName, selectionOffset, length);
}
#endif

- (void)_updateChangedSelection
{
    [self _updateChangedSelection:NO];
}

- (void)_updateChangedSelection:(BOOL)force
{
    auto& editorState = _page->editorState();
    if (editorState.isMissingPostLayoutData)
        return;

    auto& postLayoutData = editorState.postLayoutData();
    WebKit::WKSelectionDrawingInfo selectionDrawingInfo { editorState };
    if (force || selectionDrawingInfo != _lastSelectionDrawingInfo) {
        LOG_WITH_STREAM(Selection, stream << "_updateChangedSelection " << selectionDrawingInfo);

        _lastSelectionDrawingInfo = selectionDrawingInfo;

        // FIXME: We need to figure out what to do if the selection is changed by Javascript.
        if (_textInteractionAssistant) {
            _markedText = editorState.hasComposition ? postLayoutData.markedText : String { };
            [_textInteractionAssistant selectionChanged];
        }

        _selectionNeedsUpdate = NO;
        if (_shouldRestoreSelection) {
            [_textInteractionAssistant didEndScrollingOverflow];
            _shouldRestoreSelection = NO;
        }
    }

    if (postLayoutData.isStableStateUpdate && _needsDeferredEndScrollingSelectionUpdate && _page->inStableState()) {
        [[self selectionInteractionAssistant] showSelectionCommands];

        if (!_suppressSelectionAssistantReasons)
            [_textInteractionAssistant activateSelection];

        [_textInteractionAssistant didEndScrollingOverflow];

        _needsDeferredEndScrollingSelectionUpdate = NO;
    }
}

- (BOOL)shouldAllowHidingSelectionCommands
{
    ASSERT(_ignoreSelectionCommandFadeCount >= 0);
    return !_ignoreSelectionCommandFadeCount;
}

- (BOOL)supportsTextSelectionWithCharacterGranularity
{
    return YES;
}

- (BOOL)hasHiddenContentEditable
{
    return _suppressSelectionAssistantReasons.contains(WebKit::EditableRootIsTransparentOrFullyClipped);
}

- (BOOL)_shouldSuppressSelectionCommands
{
    return !!_suppressSelectionAssistantReasons;
}

- (void)_startSuppressingSelectionAssistantForReason:(WebKit::SuppressSelectionAssistantReason)reason
{
    bool wasSuppressingSelectionAssistant = !!_suppressSelectionAssistantReasons;
    _suppressSelectionAssistantReasons.add(reason);

    if (!wasSuppressingSelectionAssistant)
        [_textInteractionAssistant deactivateSelection];
}

- (void)_stopSuppressingSelectionAssistantForReason:(WebKit::SuppressSelectionAssistantReason)reason
{
    bool wasSuppressingSelectionAssistant = !!_suppressSelectionAssistantReasons;
    _suppressSelectionAssistantReasons.remove(reason);

    if (wasSuppressingSelectionAssistant && !_suppressSelectionAssistantReasons)
        [_textInteractionAssistant activateSelection];
}

#if ENABLE(DATALIST_ELEMENT)

- (UIView <WKFormControl> *)dataListTextSuggestionsInputView
{
    return _dataListTextSuggestionsInputView.get();
}

- (NSArray<UITextSuggestion *> *)dataListTextSuggestions
{
    return _dataListTextSuggestions.get();
}

- (void)setDataListTextSuggestionsInputView:(UIView <WKFormControl> *)suggestionsInputView
{
    if (_dataListTextSuggestionsInputView == suggestionsInputView)
        return;

    _dataListTextSuggestionsInputView = suggestionsInputView;

    if (![_formInputSession customInputView])
        [self reloadInputViews];
}

- (void)setDataListTextSuggestions:(NSArray<UITextSuggestion *> *)textSuggestions
{
    if (textSuggestions == _dataListTextSuggestions || [textSuggestions isEqualToArray:_dataListTextSuggestions.get()])
        return;

    _dataListTextSuggestions = textSuggestions;

    if (![_formInputSession suggestions].count)
        [self updateTextSuggestionsForInputDelegate];
}

#endif

- (void)updateTextSuggestionsForInputDelegate
{
    // Text suggestions vended from clients take precedence over text suggestions from a focused form control with a datalist.
    id <UITextInputSuggestionDelegate> inputDelegate = (id <UITextInputSuggestionDelegate>)self.inputDelegate;
    NSArray<UITextSuggestion *> *formInputSessionSuggestions = [_formInputSession suggestions];
    if (formInputSessionSuggestions.count) {
        [inputDelegate setSuggestions:formInputSessionSuggestions];
        return;
    }

#if ENABLE(DATALIST_ELEMENT)
    if ([_dataListTextSuggestions count]) {
        [inputDelegate setSuggestions:_dataListTextSuggestions.get()];
        return;
    }
#endif

    [inputDelegate setSuggestions:nil];
}

- (void)_showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect routeSharingPolicy:(WebCore::RouteSharingPolicy)routeSharingPolicy routingContextUID:(NSString *)routingContextUID
{
#if ENABLE(AIRPLAY_PICKER)
// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if (!_airPlayRoutePicker)
        _airPlayRoutePicker = adoptNS([[WKAirPlayRoutePicker alloc] init]);
    [_airPlayRoutePicker showFromView:self routeSharingPolicy:routeSharingPolicy routingContextUID:routingContextUID hasVideo:hasVideo];
#else
    if (!_airPlayRoutePicker)
        _airPlayRoutePicker = adoptNS([[WKAirPlayRoutePicker alloc] initWithView:self]);
    [_airPlayRoutePicker show:hasVideo fromRect:elementRect];
#endif
#endif
}

- (void)_showRunOpenPanel:(API::OpenPanelParameters*)parameters frameInfo:(const WebKit::FrameInfoData&)frameInfo resultListener:(WebKit::WebOpenPanelResultListenerProxy*)listener
{
    ASSERT(!_fileUploadPanel);
    if (_fileUploadPanel)
        return;

    Class ownClass = self.class;
    Class panelClass = nil;
    if ([ownClass respondsToSelector:@selector(_fileUploadPanelClass)])
        panelClass = [ownClass _fileUploadPanelClass];
    if (!panelClass)
        panelClass = [WKFileUploadPanel class];

    _frameInfoForFileUploadPanel = frameInfo;
    _fileUploadPanel = adoptNS([[panelClass alloc] initWithView:self]);
    [_fileUploadPanel setDelegate:self];
    [_fileUploadPanel presentWithParameters:parameters resultListener:listener];
}

- (void)fileUploadPanelDidDismiss:(WKFileUploadPanel *)fileUploadPanel
{
    ASSERT(_fileUploadPanel.get() == fileUploadPanel);
    
    [_fileUploadPanel setDelegate:nil];
    _fileUploadPanel = nil;
}

- (BOOL)fileUploadPanelDestinationIsManaged:(WKFileUploadPanel *)fileUploadPanel
{
    ASSERT(_fileUploadPanel.get() == fileUploadPanel);

    auto webView = _webView.get();
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([webView UIDelegate]);
    return [uiDelegate respondsToSelector:@selector(_webView:fileUploadPanelContentIsManagedWithInitiatingFrame:)]
        && [uiDelegate _webView:webView.get() fileUploadPanelContentIsManagedWithInitiatingFrame:wrapper(API::FrameInfo::create(WTFMove(_frameInfoForFileUploadPanel), _page.get()))];
}

- (void)_showShareSheet:(const WebCore::ShareDataWithParsedURL&)data inRect:(WTF::Optional<WebCore::FloatRect>)rect completionHandler:(CompletionHandler<void(bool)>&&)completionHandler
{
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if (_shareSheet)
        [_shareSheet dismiss];
    
    _shareSheet = adoptNS([[WKShareSheet alloc] initWithView:self.webView]);
    [_shareSheet setDelegate:self];

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    if (!rect) {
        if (auto lastMouseLocation = [_mouseGestureRecognizer lastMouseLocation]) {
            auto hoverLocationInWebView = [self convertPoint:*lastMouseLocation toView:self.webView];
            rect = WebCore::FloatRect(hoverLocationInWebView.x, hoverLocationInWebView.y, 1, 1);
        }
    }
#endif
    
    [_shareSheet presentWithParameters:data inRect:rect completionHandler:WTFMove(completionHandler)];
#endif
}

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
- (void)shareSheetDidDismiss:(WKShareSheet *)shareSheet
{
    ASSERT(_shareSheet == shareSheet);
    
    [_shareSheet setDelegate:nil];
    _shareSheet = nil;
}
#endif

- (NSString *)inputLabelText
{
    if (!_focusedElementInformation.label.isEmpty())
        return _focusedElementInformation.label;

    if (!_focusedElementInformation.ariaLabel.isEmpty())
        return _focusedElementInformation.ariaLabel;

    if (!_focusedElementInformation.title.isEmpty())
        return _focusedElementInformation.title;

    return _focusedElementInformation.placeholder;
}

#pragma mark - UITextInputMultiDocument

- (BOOL)_restoreFocusWithToken:(id <NSCopying, NSSecureCoding>)token
{
    if (_focusStateStack.isEmpty()) {
        ASSERT_NOT_REACHED();
        return NO;
    }

    if (_focusStateStack.takeLast())
        [_webView _decrementFocusPreservationCount];

    // FIXME: Our current behavior in -_restoreFocusWithToken: does not force the web view to become first responder
    // by refocusing the currently focused element. As such, we return NO here so that UIKit will tell WKContentView
    // to become first responder in the future.
    return NO;
}

- (void)preserveFocus
{
    [_webView _incrementFocusPreservationCount];
}

- (void)releaseFocus
{
    [_webView _decrementFocusPreservationCount];
}

- (void)_preserveFocusWithToken:(id <NSCopying, NSSecureCoding>)token destructively:(BOOL)destructively
{
    if (!_inputPeripheral) {
        [_webView _incrementFocusPreservationCount];
        _focusStateStack.append(true);
    } else
        _focusStateStack.append(false);
}

#pragma mark - Implementation of UIWebTouchEventsGestureRecognizerDelegate.

- (BOOL)gestureRecognizer:(UIWebTouchEventsGestureRecognizer *)gestureRecognizer shouldIgnoreWebTouchWithEvent:(UIEvent *)event
{
    _touchEventsCanPreventNativeGestures = YES;

    return [self gestureRecognizer:gestureRecognizer isInterruptingMomentumScrollingWithEvent:event];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer isInterruptingMomentumScrollingWithEvent:(UIEvent *)event
{
    NSSet<UITouch *> *touches = [event touchesForGestureRecognizer:gestureRecognizer];
    for (UITouch *touch in touches) {
        if ([touch.view isKindOfClass:[UIScrollView class]] && [(UIScrollView *)touch.view _isInterruptingDeceleration])
            return YES;
    }
    return self._scroller._isInterruptingDeceleration;
}

- (BOOL)isAnyTouchOverActiveArea:(NSSet *)touches
{
    return YES;
}

#pragma mark - Implementation of WKActionSheetAssistantDelegate.

- (Optional<WebKit::InteractionInformationAtPosition>)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    WebKit::InteractionInformationRequest request(_positionInformation.request.point);
    request.includeSnapshot = true;
    request.includeLinkIndicator = assistant.needsLinkIndicator;
    request.linkIndicatorShouldHaveLegacyMargins = !self._shouldUseContextMenus;
    if (![self ensurePositionInformationIsUpToDate:request])
        return WTF::nullopt;

    return _positionInformation;
}

- (void)updatePositionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    _hasValidPositionInformation = NO;
    WebKit::InteractionInformationRequest request(_positionInformation.request.point);
    request.includeSnapshot = true;
    request.includeLinkIndicator = assistant.needsLinkIndicator;
    request.linkIndicatorShouldHaveLegacyMargins = !self._shouldUseContextMenus;

    [self requestAsynchronousPositionInformationUpdate:request];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant performAction:(WebKit::SheetAction)action
{
    _page->performActionOnElement((uint32_t)action);
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant openElementAtLocation:(CGPoint)location
{
    [self _attemptClickAtLocation:location modifierFlags:0];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithURL:(NSURL *)url rect:(CGRect)boundingRect
{
    WebCore::ShareDataWithParsedURL shareData;
    shareData.url = { url };
    [self _showShareSheet:shareData inRect: { [self convertRect:boundingRect toView:self.webView] } completionHandler:[] (bool success) { }];
}

#if HAVE(APP_LINKS)
- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeAppLinkActionsForElement:(_WKActivatedElementInfo *)element
{
    return _page->uiClient().shouldIncludeAppLinkActionsForElement(element);
}
#endif

- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant showCustomSheetForElement:(_WKActivatedElementInfo *)element
{
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    
    if ([uiDelegate respondsToSelector:@selector(_webView:showCustomSheetForElement:)]) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([uiDelegate _webView:self.webView showCustomSheetForElement:element]) {
#if ENABLE(DATA_INTERACTION)
            BOOL shouldCancelAllTouches = !_dragDropInteractionState.dragSession();
#else
            BOOL shouldCancelAllTouches = YES;
#endif

            // Prevent tap-and-hold and panning.
            if (shouldCancelAllTouches)
                [UIApp _cancelAllTouches];

            return YES;
        }
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    return NO;
}

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
- (CGRect)unoccludedWindowBoundsForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    UIEdgeInsets contentInset = [[_webView scrollView] adjustedContentInset];
    CGRect rect = UIEdgeInsetsInsetRect([_webView bounds], contentInset);
    return [_webView convertRect:rect toView:[self window]];
}
#endif

- (RetainPtr<NSArray>)actionSheetAssistant:(WKActionSheetAssistant *)assistant decideActionsForElement:(_WKActivatedElementInfo *)element defaultActions:(RetainPtr<NSArray>)defaultActions
{
    return _page->uiClient().actionsForElement(element, WTFMove(defaultActions));
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant willStartInteractionWithElement:(_WKActivatedElementInfo *)element
{
    _page->startInteractionWithPositionInformation(_positionInformation);
}

- (void)actionSheetAssistantDidStopInteraction:(WKActionSheetAssistant *)assistant
{
    _page->stopInteraction();
}

- (NSDictionary *)dataDetectionContextForPositionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation
{
    RetainPtr<NSMutableDictionary> context;
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([uiDelegate respondsToSelector:@selector(_dataDetectionContextForWebView:)])
        context = adoptNS([[uiDelegate _dataDetectionContextForWebView:self.webView] mutableCopy]);
    
    if (!context)
        context = adoptNS([[NSMutableDictionary alloc] init]);

#if ENABLE(DATA_DETECTION)
    if (!positionInformation.textBefore.isEmpty())
        context.get()[getkDataDetectorsLeadingText()] = positionInformation.textBefore;
    if (!positionInformation.textAfter.isEmpty())
        context.get()[getkDataDetectorsTrailingText()] = positionInformation.textAfter;

    CGRect sourceRect;
    if (positionInformation.isLink)
        sourceRect = positionInformation.linkIndicator.textBoundingRectInRootViewCoordinates;
    else
        sourceRect = positionInformation.bounds;

    CGRect frameInContainerViewCoordinates = [self convertRect:sourceRect toView:self.containerForContextMenuHintPreviews];
    return [getDDContextMenuActionClass() updateContext:context.get() withSourceRect:frameInContainerViewCoordinates];
#else
    return context.autorelease();
#endif
}

- (NSDictionary *)dataDetectionContextForActionSheetAssistant:(WKActionSheetAssistant *)assistant positionInformation:(const WebKit::InteractionInformationAtPosition&)positionInformation
{
    return [self dataDetectionContextForPositionInformation:positionInformation];
}

- (NSString *)selectedTextForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    return [self selectedText];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant getAlternateURLForImage:(UIImage *)image completion:(void (^)(NSURL *alternateURL, NSDictionary *userInfo))completion
{
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([uiDelegate respondsToSelector:@selector(_webView:getAlternateURLFromImage:completionHandler:)]) {
        [uiDelegate _webView:self.webView getAlternateURLFromImage:image completionHandler:^(NSURL *alternateURL, NSDictionary *userInfo) {
            completion(alternateURL, userInfo);
        }];
    } else
        completion(nil, nil);
}

#if USE(UICONTEXTMENU)

- (UITargetedPreview *)createTargetedContextMenuHintForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    return [self _createTargetedContextMenuHintPreviewIfPossible];
}

- (void)removeContextMenuViewIfPossibleForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    [self _removeContextMenuViewIfPossible];
}

#endif // USE(UICONTEXTMENU)

- (BOOL)_shouldUseContextMenus
{
#if HAVE(LINK_PREVIEW) && USE(UICONTEXTMENU)
    return linkedOnOrAfter(WebKit::SDKVersion::FirstThatHasUIContextMenuInteraction);
#endif
    return NO;
}

- (BOOL)_shouldAvoidResizingWhenInputViewBoundsChange
{
    return _focusedElementInformation.shouldAvoidResizingWhenInputViewBoundsChange;
}

- (BOOL)_shouldAvoidScrollingWhenFocusedContentIsVisible
{
    return _focusedElementInformation.shouldAvoidScrollingWhenFocusedContentIsVisible;
}

- (BOOL)_shouldUseLegacySelectPopoverDismissalBehavior
{
    if (!WebKit::currentUserInterfaceIdiomIsPad())
        return NO;

    if (_focusedElementInformation.elementType != WebKit::InputType::Select)
        return NO;

    if (!_focusedElementInformation.shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation)
        return NO;

    return WebCore::IOSApplication::isDataActivation();
}

- (RetainPtr<UIView>)_createPreviewContainerWithLayerName:(NSString *)layerName
{
    auto container = adoptNS([[UIView alloc] init]);
    [container layer].anchorPoint = CGPointZero;
    [container layer].name = layerName;
    [_interactionViewsContainerView addSubview:container.get()];
    return container;
}

- (UIView *)containerForDropPreviews
{
    if (!_dropPreviewContainerView)
        _dropPreviewContainerView = [self _createPreviewContainerWithLayerName:@"Drop Preview Container"];

    ASSERT([_dropPreviewContainerView superview]);
    [_dropPreviewContainerView setHidden:NO];
    return _dropPreviewContainerView.get();
}

- (UIView *)containerForDragPreviews
{
    if (!_dragPreviewContainerView)
        _dragPreviewContainerView = [self _createPreviewContainerWithLayerName:@"Drag Preview Container"];

    ASSERT([_dragPreviewContainerView superview]);
    [_dragPreviewContainerView setHidden:NO];
    return _dragPreviewContainerView.get();
}

- (UIView *)containerForContextMenuHintPreviews
{
    if (!_contextMenuHintContainerView)
        _contextMenuHintContainerView = [self _createPreviewContainerWithLayerName:@"Context Menu Hint Preview Container"];

    ASSERT([_contextMenuHintContainerView superview]);
    [_contextMenuHintContainerView setHidden:NO];
    return _contextMenuHintContainerView.get();
}

- (void)_hideTargetedPreviewContainerViews
{
    [_dropPreviewContainerView setHidden:YES];
    [_dragPreviewContainerView setHidden:YES];
    [_contextMenuHintContainerView setHidden:YES];
}

#pragma mark - WKDeferringGestureRecognizerDelegate

- (BOOL)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer shouldDeferGesturesAfterBeginningTouchesWithEvent:(UIEvent *)event
{
    return ![self gestureRecognizer:deferringGestureRecognizer isInterruptingMomentumScrollingWithEvent:event];
}

- (BOOL)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer shouldDeferGesturesAfterEndingTouchesWithEvent:(UIEvent *)event
{
    return _page->isHandlingPreventableTouchStart();
}

- (BOOL)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer shouldDeferOtherGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
#if ENABLE(IOS_TOUCH_EVENTS)
    if ([_webView _isNavigationSwipeGestureRecognizer:gestureRecognizer])
        return NO;

    auto webView = _webView.getAutoreleased();
    auto view = gestureRecognizer.view;
    BOOL gestureIsInstalledOnOrUnderWebView = NO;
    while (view) {
        if (view == webView) {
            gestureIsInstalledOnOrUnderWebView = YES;
            break;
        }
        view = view.superview;
    }

    if (!gestureIsInstalledOnOrUnderWebView)
        return NO;

    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return NO;

    if (gestureRecognizer == _touchEventGestureRecognizer)
        return NO;

    auto mayDelayResetOfContainingSubgraph = [&](UIGestureRecognizer *gesture) -> BOOL {
#if USE(UICONTEXTMENU) && HAVE(LINK_PREVIEW)
        if (gesture == [_contextMenuInteraction gestureRecognizerForFailureRelationships])
            return YES;
#endif

#if ENABLE(DRAG_SUPPORT)
        if (gesture.delegate == [_dragInteraction _initiationDriver])
            return YES;
#endif

        if ([gesture isKindOfClass:tapAndAHalfRecognizerClass()])
            return YES;

        if (gesture == [_textInteractionAssistant loupeGesture])
            return YES;

        if ([gesture isKindOfClass:UITapGestureRecognizer.class]) {
            UITapGestureRecognizer *tapGesture = (UITapGestureRecognizer *)gesture;
            return tapGesture.numberOfTapsRequired > 1 && tapGesture.numberOfTouchesRequired < 2;
        }

        return NO;
    };

    if (gestureRecognizer == _doubleTapGestureRecognizer || gestureRecognizer == _singleTapGestureRecognizer)
        return deferringGestureRecognizer == _deferringGestureRecognizerForSyntheticTapGestures;

    if (mayDelayResetOfContainingSubgraph(gestureRecognizer))
        return deferringGestureRecognizer == _deferringGestureRecognizerForDelayedResettableGestures;

    return deferringGestureRecognizer == _deferringGestureRecognizerForImmediatelyResettableGestures;
#else
    UNUSED_PARAM(deferringGestureRecognizer);
    UNUSED_PARAM(gestureRecognizer);
    return NO;
#endif
}

#if ENABLE(DRAG_SUPPORT)

static BOOL shouldEnableDragInteractionForPolicy(_WKDragInteractionPolicy policy)
{
    switch (policy) {
    case _WKDragInteractionPolicyAlwaysEnable:
        return YES;
    case _WKDragInteractionPolicyAlwaysDisable:
        return NO;
    default:
        return [UIDragInteraction isEnabledByDefault];
    }
}

- (void)_didChangeDragInteractionPolicy
{
    [_dragInteraction setEnabled:shouldEnableDragInteractionForPolicy(self.webView._dragInteractionPolicy)];
}

- (NSTimeInterval)dragLiftDelay
{
    static const NSTimeInterval mediumDragLiftDelay = 0.5;
    static const NSTimeInterval longDragLiftDelay = 0.65;
    auto dragLiftDelay = self.webView.configuration._dragLiftDelay;
    if (dragLiftDelay == _WKDragLiftDelayMedium)
        return mediumDragLiftDelay;
    if (dragLiftDelay == _WKDragLiftDelayLong)
        return longDragLiftDelay;
    return _UIDragInteractionDefaultLiftDelay();
}

- (id <WKUIDelegatePrivate>)webViewUIDelegate
{
    return (id <WKUIDelegatePrivate>)[_webView UIDelegate];
}

- (void)setUpDragAndDropInteractions
{
    _dragInteraction = adoptNS([[UIDragInteraction alloc] initWithDelegate:self]);
    _dropInteraction = adoptNS([[UIDropInteraction alloc] initWithDelegate:self]);
    [_dragInteraction _setLiftDelay:self.dragLiftDelay];
    [_dragInteraction setEnabled:shouldEnableDragInteractionForPolicy(self.webView._dragInteractionPolicy)];
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [_dragInteraction _setAllowsPointerDragBeforeLiftDelay:NO];
#endif

    [self addInteraction:_dragInteraction.get()];
    [self addInteraction:_dropInteraction.get()];
}

- (void)teardownDragAndDropInteractions
{
    if (_dragInteraction)
        [self removeInteraction:_dragInteraction.get()];

    if (_dropInteraction)
        [self removeInteraction:_dropInteraction.get()];

    _dragInteraction = nil;
    _dropInteraction = nil;

    [self cleanUpDragSourceSessionState];
}

- (void)_startDrag:(RetainPtr<CGImageRef>)image item:(const WebCore::DragItem&)item
{
    ASSERT(item.sourceAction != WebCore::DragSourceActionNone);

    if (item.promisedAttachmentInfo)
        [self _prepareToDragPromisedAttachment:item.promisedAttachmentInfo];

    auto dragImage = adoptNS([[UIImage alloc] initWithCGImage:image.get() scale:_page->deviceScaleFactor() orientation:UIImageOrientationUp]);
    _dragDropInteractionState.stageDragItem(item, dragImage.get());
}

- (void)_didHandleAdditionalDragItemsRequest:(BOOL)added
{
    auto completion = _dragDropInteractionState.takeAddDragItemCompletionBlock();
    if (!completion)
        return;

    auto *registrationLists = [[WebItemProviderPasteboard sharedInstance] takeRegistrationLists];
    if (!added || ![registrationLists count] || !_dragDropInteractionState.hasStagedDragSource()) {
        _dragDropInteractionState.clearStagedDragSource();
        completion(@[ ]);
        return;
    }

    auto stagedDragSource = _dragDropInteractionState.stagedDragSource();
    NSArray *dragItemsToAdd = [self _itemsForBeginningOrAddingToSessionWithRegistrationLists:registrationLists stagedDragSource:stagedDragSource];

    RELEASE_LOG(DragAndDrop, "Drag session: %p adding %tu items", _dragDropInteractionState.dragSession(), dragItemsToAdd.count);
    _dragDropInteractionState.clearStagedDragSource(dragItemsToAdd.count ? WebKit::DragDropInteractionState::DidBecomeActive::Yes : WebKit::DragDropInteractionState::DidBecomeActive::No);

    completion(dragItemsToAdd);

    if (dragItemsToAdd.count)
        _page->didStartDrag();
}

- (void)_didHandleDragStartRequest:(BOOL)started
{
    BlockPtr<void()> savedCompletionBlock = _dragDropInteractionState.takeDragStartCompletionBlock();
    ASSERT(savedCompletionBlock);

    RELEASE_LOG(DragAndDrop, "Handling drag start request (started: %d, completion block: %p)", started, savedCompletionBlock.get());
    if (savedCompletionBlock)
        savedCompletionBlock();

    if (!_dragDropInteractionState.dragSession().items.count) {
        auto positionForDragEnd = WebCore::roundedIntPoint(_dragDropInteractionState.adjustedPositionForDragEnd());
        [self cleanUpDragSourceSessionState];
        if (started) {
            // A client of the Objective C SPI or UIKit might have prevented the drag from beginning entirely in the UI process, in which case
            // we need to balance the `dragstart` event with a `dragend`.
            _page->dragEnded(positionForDragEnd, positionForDragEnd, WebCore::DragOperationNone);
        }
    }
}

- (void)computeClientAndGlobalPointsForDropSession:(id <UIDropSession>)session outClientPoint:(CGPoint *)outClientPoint outGlobalPoint:(CGPoint *)outGlobalPoint
{
    // FIXME: This makes the behavior of drag events on iOS consistent with other synthetic mouse events on iOS (see WebPage::completeSyntheticClick).
    // However, we should experiment with making the client position relative to the window and the global position in document coordinates. See
    // https://bugs.webkit.org/show_bug.cgi?id=173855 for more details.
    auto locationInContentView = [session locationInView:self];
    if (outClientPoint)
        *outClientPoint = locationInContentView;

    if (outGlobalPoint)
        *outGlobalPoint = locationInContentView;
}

static UIDropOperation dropOperationForWebCoreDragOperation(WebCore::DragOperation operation)
{
    if (operation & WebCore::DragOperationMove)
        return UIDropOperationMove;

    if (operation & WebCore::DragOperationCopy)
        return UIDropOperationCopy;

    return UIDropOperationCancel;
}

- (WebCore::DragData)dragDataForDropSession:(id <UIDropSession>)session dragDestinationAction:(WKDragDestinationAction)dragDestinationAction
{
    CGPoint global;
    CGPoint client;
    [self computeClientAndGlobalPointsForDropSession:session outClientPoint:&client outGlobalPoint:&global];

    WebCore::DragOperation dragOperationMask = static_cast<WebCore::DragOperation>(session.allowsMoveOperation ? WebCore::DragOperationEvery : (WebCore::DragOperationEvery & ~WebCore::DragOperationMove));
    auto dragDestinationActionMask = OptionSet<WebCore::DragDestinationAction>::fromRaw(dragDestinationAction);
    return { session, WebCore::roundedIntPoint(client), WebCore::roundedIntPoint(global), dragOperationMask, WebCore::DragApplicationNone, dragDestinationActionMask };
}

- (void)cleanUpDragSourceSessionState
{
    if (_waitingForEditDragSnapshot)
        return;

    if (_dragDropInteractionState.dragSession() || _dragDropInteractionState.isPerformingDrop())
        RELEASE_LOG(DragAndDrop, "Cleaning up dragging state (has pending operation: %d)", [[WebItemProviderPasteboard sharedInstance] hasPendingOperation]);

    if (![[WebItemProviderPasteboard sharedInstance] hasPendingOperation]) {
        // If we're performing a drag operation, don't clear out the pasteboard yet, since another web view may still require access to it.
        // The pasteboard will be cleared after the last client is finished performing a drag operation using the item providers.
        [[WebItemProviderPasteboard sharedInstance] setItemProviders:nil];
    }

    [[WebItemProviderPasteboard sharedInstance] clearRegistrationLists];
    [self _restoreCalloutBarIfNeeded];

    [std::exchange(_dragPreviewContainerView, nil) removeFromSuperview];
    [std::exchange(_visibleContentViewSnapshot, nil) removeFromSuperview];
    [_editDropCaretView remove];
    _editDropCaretView = nil;
    _shouldRestoreCalloutBarAfterDrop = NO;

    _dragDropInteractionState.dragAndDropSessionsDidEnd();
    _dragDropInteractionState = { };
}

static NSArray<NSItemProvider *> *extractItemProvidersFromDragItems(NSArray<UIDragItem *> *dragItems)
{
    NSMutableArray<NSItemProvider *> *providers = [NSMutableArray array];
    for (UIDragItem *item in dragItems) {
        if (NSItemProvider *provider = item.itemProvider)
            [providers addObject:provider];
    }
    return providers;
}

static NSArray<NSItemProvider *> *extractItemProvidersFromDropSession(id <UIDropSession> session)
{
    return extractItemProvidersFromDragItems(session.items);
}

- (void)_willReceiveEditDragSnapshot
{
    _waitingForEditDragSnapshot = YES;
}

- (void)_didReceiveEditDragSnapshot:(Optional<WebCore::TextIndicatorData>)data
{
    _waitingForEditDragSnapshot = NO;

    [self _deliverDelayedDropPreviewIfPossible:data];
    [self cleanUpDragSourceSessionState];

    if (auto action = WTFMove(_actionToPerformAfterReceivingEditDragSnapshot))
        action();
}

- (void)_deliverDelayedDropPreviewIfPossible:(Optional<WebCore::TextIndicatorData>)data
{
    if (!_visibleContentViewSnapshot)
        return;

    if (!data)
        return;

    if (!data->contentImage)
        return;

    auto snapshotWithoutSelection = data->contentImageWithoutSelection;
    if (!snapshotWithoutSelection)
        return;

    auto unselectedSnapshotImage = snapshotWithoutSelection->nativeImage();
    if (!unselectedSnapshotImage)
        return;

    if (!_dropAnimationCount)
        return;

    auto unselectedContentImageForEditDrag = adoptNS([[UIImage alloc] initWithCGImage:unselectedSnapshotImage.get() scale:_page->deviceScaleFactor() orientation:UIImageOrientationUp]);
    _unselectedContentSnapshot = adoptNS([[UIImageView alloc] initWithImage:unselectedContentImageForEditDrag.get()]);
    [_unselectedContentSnapshot setFrame:data->contentImageWithoutSelectionRectInRootViewCoordinates];

    [self insertSubview:_unselectedContentSnapshot.get() belowSubview:_visibleContentViewSnapshot.get()];
    _dragDropInteractionState.deliverDelayedDropPreview(self, self.containerForDropPreviews, data.value());
}

- (void)_didPerformDragOperation:(BOOL)handled
{
    RELEASE_LOG(DragAndDrop, "Finished performing drag controller operation (handled: %d)", handled);
    [[WebItemProviderPasteboard sharedInstance] decrementPendingOperationCount];
    id <UIDropSession> dropSession = _dragDropInteractionState.dropSession();
    if ([self.webViewUIDelegate respondsToSelector:@selector(_webView:dataInteractionOperationWasHandled:forSession:itemProviders:)])
        [self.webViewUIDelegate _webView:self.webView dataInteractionOperationWasHandled:handled forSession:dropSession itemProviders:[WebItemProviderPasteboard sharedInstance].itemProviders];

    CGPoint global;
    CGPoint client;
    [self computeClientAndGlobalPointsForDropSession:dropSession outClientPoint:&client outGlobalPoint:&global];
    [self cleanUpDragSourceSessionState];
    _page->dragEnded(WebCore::roundedIntPoint(client), WebCore::roundedIntPoint(global), _page->currentDragOperation());
}

- (void)_didChangeDragCaretRect:(CGRect)previousRect currentRect:(CGRect)rect
{
    BOOL previousRectIsEmpty = CGRectIsEmpty(previousRect);
    BOOL currentRectIsEmpty = CGRectIsEmpty(rect);
    if (previousRectIsEmpty && currentRectIsEmpty)
        return;

    if (previousRectIsEmpty) {
        _editDropCaretView = adoptNS([[_UITextDragCaretView alloc] initWithTextInputView:self]);
        [_editDropCaretView insertAtPosition:[WKTextPosition textPositionWithRect:rect]];
        return;
    }

    if (currentRectIsEmpty) {
        [_editDropCaretView remove];
        _editDropCaretView = nil;
        return;
    }

    [_editDropCaretView updateToPosition:[WKTextPosition textPositionWithRect:rect]];
}

- (void)_prepareToDragPromisedAttachment:(const WebCore::PromisedAttachmentInfo&)info
{
    auto session = retainPtr(_dragDropInteractionState.dragSession());
    if (!session) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto numberOfAdditionalTypes = info.additionalTypes.size();
    ASSERT(numberOfAdditionalTypes == info.additionalData.size());

    RELEASE_LOG(DragAndDrop, "Drag session: %p preparing to drag blob: %s with attachment identifier: %s", session.get(), info.blobURL.string().utf8().data(), info.attachmentIdentifier.utf8().data());

    NSString *utiType = info.contentType;
    NSString *fileName = info.fileName;
    if (auto attachment = _page->attachmentForIdentifier(info.attachmentIdentifier)) {
        utiType = attachment->utiType();
        fileName = attachment->fileName();
    }

    auto registrationList = adoptNS([[WebItemProviderRegistrationInfoList alloc] init]);
    [registrationList setPreferredPresentationStyle:WebPreferredPresentationStyleAttachment];
    if ([fileName length])
        [registrationList setSuggestedName:fileName];
    if (numberOfAdditionalTypes == info.additionalData.size() && numberOfAdditionalTypes) {
        for (size_t index = 0; index < numberOfAdditionalTypes; ++index) {
            auto nsData = info.additionalData[index]->createNSData();
            [registrationList addData:nsData.get() forType:info.additionalTypes[index]];
        }
    }

    [registrationList addPromisedType:utiType fileCallback:[session = WTFMove(session), weakSelf = WeakObjCPtr<WKContentView>(self), info] (WebItemProviderFileCallback callback) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            callback(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorWebViewInvalidated userInfo:nil]);
            return;
        }

        NSString *temporaryBlobDirectory = FileSystem::createTemporaryDirectory(@"blobs");
        NSURL *destinationURL = [NSURL fileURLWithPath:[temporaryBlobDirectory stringByAppendingPathComponent:[NSUUID UUID].UUIDString] isDirectory:NO];

        auto attachment = strongSelf->_page->attachmentForIdentifier(info.attachmentIdentifier);
        if (attachment && attachment->fileWrapper()) {
            RELEASE_LOG(DragAndDrop, "Drag session: %p delivering promised attachment: %s at path: %@", session.get(), info.attachmentIdentifier.utf8().data(), destinationURL.path);
            NSError *fileWrapperError = nil;
            if ([attachment->fileWrapper() writeToURL:destinationURL options:0 originalContentsURL:nil error:&fileWrapperError])
                callback(destinationURL, nil);
            else
                callback(nil, fileWrapperError);
        } else
            callback(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorWebViewInvalidated userInfo:nil]);

        [ensureLocalDragSessionContext(session.get()) addTemporaryDirectory:temporaryBlobDirectory];
    }];

    WebItemProviderPasteboard *pasteboard = [WebItemProviderPasteboard sharedInstance];
    pasteboard.itemProviders = @[ [registrationList itemProvider] ];
    [pasteboard stageRegistrationLists:@[ registrationList.get() ]];
}

- (WKDragDestinationAction)_dragDestinationActionForDropSession:(id <UIDropSession>)session
{
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:dragDestinationActionMaskForDraggingInfo:)])
        return [uiDelegate _webView:self.webView dragDestinationActionMaskForDraggingInfo:session];

    return WKDragDestinationActionAny & ~WKDragDestinationActionLoad;
}

- (WebCore::DragSourceAction)_allowedDragSourceActions
{
    auto allowedActions = WebCore::DragSourceActionAny;
    if (!self.isFirstResponder || !_suppressSelectionAssistantReasons.isEmpty()) {
        // Don't allow starting a drag on a selection when selection views are not visible.
        allowedActions = static_cast<WebCore::DragSourceAction>(allowedActions & ~WebCore::DragSourceActionSelection);
    }
    return allowedActions;
}

- (id <UIDragDropSession>)currentDragOrDropSession
{
    if (_dragDropInteractionState.dropSession())
        return _dragDropInteractionState.dropSession();
    return _dragDropInteractionState.dragSession();
}

- (void)_restoreCalloutBarIfNeeded
{
    if (!_shouldRestoreCalloutBarAfterDrop)
        return;

    // FIXME: This SPI should be renamed in UIKit to reflect a more general purpose of revealing hidden interaction assistant controls.
    [_textInteractionAssistant didEndScrollingOverflow];
    _shouldRestoreCalloutBarAfterDrop = NO;
}

- (NSArray<UIDragItem *> *)_itemsForBeginningOrAddingToSessionWithRegistrationLists:(NSArray<WebItemProviderRegistrationInfoList *> *)registrationLists stagedDragSource:(const WebKit::DragSourceState&)stagedDragSource
{
    if (!registrationLists.count)
        return @[ ];

    NSMutableArray *adjustedItemProviders = [NSMutableArray array];
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:adjustedDataInteractionItemProvidersForItemProvider:representingObjects:additionalData:)]) {
        // FIXME: We should consider a new UI delegate hook that accepts a list of item providers, so we don't need to invoke this delegate method repeatedly for multiple items.
        for (WebItemProviderRegistrationInfoList *list in registrationLists) {
            NSItemProvider *defaultItemProvider = list.itemProvider;
            if (!defaultItemProvider)
                continue;

            auto representingObjects = adoptNS([[NSMutableArray alloc] init]);
            auto additionalData = adoptNS([[NSMutableDictionary alloc] init]);
            [list enumerateItems:[representingObjects, additionalData] (id <WebItemProviderRegistrar> item, NSUInteger) {
                if ([item respondsToSelector:@selector(representingObjectForClient)])
                    [representingObjects addObject:item.representingObjectForClient];
                if ([item respondsToSelector:@selector(typeIdentifierForClient)] && [item respondsToSelector:@selector(dataForClient)])
                    [additionalData setObject:item.dataForClient forKey:item.typeIdentifierForClient];
            }];
            NSArray *adjustedItems = [uiDelegate _webView:self.webView adjustedDataInteractionItemProvidersForItemProvider:defaultItemProvider representingObjects:representingObjects.get() additionalData:additionalData.get()];
            if (adjustedItems.count)
                [adjustedItemProviders addObjectsFromArray:adjustedItems];
        }
    } else {
        for (WebItemProviderRegistrationInfoList *list in registrationLists) {
            if (auto *defaultItemProvider = list.itemProvider)
                [adjustedItemProviders addObject:defaultItemProvider];
        }
    }

    NSMutableArray *dragItems = [NSMutableArray arrayWithCapacity:adjustedItemProviders.count];
    for (NSItemProvider *itemProvider in adjustedItemProviders) {
        auto item = adoptNS([[UIDragItem alloc] initWithItemProvider:itemProvider]);
        [item _setPrivateLocalContext:@(stagedDragSource.itemIdentifier)];
        [dragItems addObject:item.get()];
    }

    return dragItems;
}

- (void)cancelActiveTextInteractionGestures
{
    [[_textInteractionAssistant loupeGesture] _wk_cancel];
    [[_textInteractionAssistant forcePressGesture] _wk_cancel];
}

- (UIView *)textEffectsWindow
{
    return [UITextEffectsWindow sharedTextEffectsWindowForWindowScene:self.window.windowScene];
}

- (NSDictionary *)_autofillContext
{
    BOOL provideStrongPasswordAssistance = _focusRequiresStrongPasswordAssistance && _focusedElementInformation.elementType == WebKit::InputType::Password;
    if (!self._hasFocusedElement || (!_focusedElementInformation.acceptsAutofilledLoginCredentials && !provideStrongPasswordAssistance))
        return nil;

    if (provideStrongPasswordAssistance)
        return @{ @"_automaticPasswordKeyboard" : @YES, @"strongPasswordAdditionalContext" : _additionalContextForStrongPasswordAssistance.get() };

    NSURL *platformURL = _focusedElementInformation.representingPageURL;
    if (platformURL)
        return @{ @"_WebViewURL" : platformURL };

    return nil;
}

- (BOOL)supportsImagePaste
{
    return mayContainSelectableText(_focusedElementInformation.elementType);
}

#if USE(UICONTEXTMENU)
static RetainPtr<UIImage> uiImageForImage(WebCore::Image* image)
{
    if (!image)
        return nil;

    auto cgImage = image->nativeImage();
    if (!cgImage)
        return nil;

    return adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);
}

// FIXME: This should be merged with createTargetedDragPreview in DragDropInteractionState.
static RetainPtr<UITargetedPreview> createTargetedPreview(UIImage *image, UIView *rootView, UIView *previewContainer, const WebCore::FloatRect& frameInRootViewCoordinates, const Vector<WebCore::FloatRect>& clippingRectsInFrameCoordinates, UIColor *backgroundColor)
{
    if (frameInRootViewCoordinates.isEmpty() || !image || !previewContainer.window)
        return nil;

    WebCore::FloatRect frameInContainerCoordinates = [rootView convertRect:frameInRootViewCoordinates toView:previewContainer];
    if (frameInContainerCoordinates.isEmpty())
        return nil;

    auto scalingRatio = frameInContainerCoordinates.size() / frameInRootViewCoordinates.size();
    auto clippingRectValuesInFrameCoordinates = createNSArray(clippingRectsInFrameCoordinates, [&] (WebCore::FloatRect rect) {
        rect.scale(scalingRatio);
        return [NSValue valueWithCGRect:rect];
    });

    RetainPtr<UIPreviewParameters> parameters;
    if ([clippingRectValuesInFrameCoordinates count])
        parameters = adoptNS([[UIPreviewParameters alloc] initWithTextLineRects:clippingRectValuesInFrameCoordinates.get()]);
    else
        parameters = adoptNS([[UIPreviewParameters alloc] init]);

    [parameters setBackgroundColor:(backgroundColor ?: [UIColor clearColor])];

    CGPoint centerInContainerCoordinates = { CGRectGetMidX(frameInContainerCoordinates), CGRectGetMidY(frameInContainerCoordinates) };
    auto target = adoptNS([[UIPreviewTarget alloc] initWithContainer:previewContainer center:centerInContainerCoordinates]);

    auto imageView = adoptNS([[UIImageView alloc] initWithImage:image]);
    [imageView setFrame:frameInContainerCoordinates];
    return adoptNS([[UITargetedPreview alloc] initWithView:imageView.get() parameters:parameters.get() target:target.get()]);
}

static RetainPtr<UITargetedPreview> createFallbackTargetedPreview(UIView *rootView, UIView *containerView, const WebCore::FloatRect& frameInRootViewCoordinates)
{
    if (!containerView.window)
        return nil;

    if (frameInRootViewCoordinates.isEmpty())
        return nil;

    auto parameters = adoptNS([[UIPreviewParameters alloc] init]);
    UIView *snapshotView = [rootView resizableSnapshotViewFromRect:frameInRootViewCoordinates afterScreenUpdates:NO withCapInsets:UIEdgeInsetsZero];

    CGRect frameInContainerViewCoordinates = [rootView convertRect:frameInRootViewCoordinates toView:containerView];

    if (CGRectIsEmpty(frameInContainerViewCoordinates))
        return nil;

    snapshotView.frame = frameInContainerViewCoordinates;

    CGPoint centerInContainerViewCoordinates = CGPointMake(CGRectGetMidX(frameInContainerViewCoordinates), CGRectGetMidY(frameInContainerViewCoordinates));
    auto target = adoptNS([[UIPreviewTarget alloc] initWithContainer:containerView center:centerInContainerViewCoordinates]);

    return adoptNS([[UITargetedPreview alloc] initWithView:snapshotView parameters:parameters.get() target:target.get()]);
}

- (UITargetedPreview *)_createTargetedContextMenuHintPreviewIfPossible
{
    RetainPtr<UITargetedPreview> targetedPreview;

    if (_positionInformation.isLink && _positionInformation.linkIndicator.contentImage) {
        auto indicator = _positionInformation.linkIndicator;
        auto textIndicatorImage = uiImageForImage(indicator.contentImage.get());
        targetedPreview = createTargetedPreview(textIndicatorImage.get(), self, self.containerForContextMenuHintPreviews, indicator.textBoundingRectInRootViewCoordinates, indicator.textRectsInBoundingRectCoordinates, [UIColor colorWithCGColor:cachedCGColor(indicator.estimatedBackgroundColor)]);
    } else if ((_positionInformation.isAttachment || _positionInformation.isImage) && _positionInformation.image) {
        auto cgImage = _positionInformation.image->makeCGImageCopy();
        auto image = adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);
        targetedPreview = createTargetedPreview(image.get(), self, self.containerForContextMenuHintPreviews, _positionInformation.bounds, { }, nil);
    }

    if (!targetedPreview)
        targetedPreview = createFallbackTargetedPreview(self, self.containerForContextMenuHintPreviews, _positionInformation.bounds);

    if (_positionInformation.containerScrollingNodeID) {
        UIScrollView *positionTrackingView = self.webView.scrollView;
        if (auto* scrollingCoordinator = _page->scrollingCoordinatorProxy())
            positionTrackingView = scrollingCoordinator->scrollViewForScrollingNodeID(_positionInformation.containerScrollingNodeID);

        if ([targetedPreview respondsToSelector:@selector(_setOverridePositionTrackingView:)])
            [targetedPreview _setOverridePositionTrackingView:positionTrackingView];
    }

    _contextMenuInteractionTargetedPreview = WTFMove(targetedPreview);
    return _contextMenuInteractionTargetedPreview.get();
}

- (void)_removeContextMenuViewIfPossible
{
#if HAVE(LINK_PREVIEW)
    // If a new _contextMenuElementInfo is installed, we've started another interaction,
    // and removing the hint container view will cause the animation to break.
    if (_contextMenuElementInfo)
        return;
#endif
#if ENABLE(DATA_DETECTION)
    // We are also using this container for the action sheet assistant...
    if ([_actionSheetAssistant hasContextMenuInteraction])
        return;
#endif
    // and for the file upload panel...
    if (_fileUploadPanel)
        return;
    
    // and for the date/time picker.
    if ([self dateTimeInputControl])
        return;
    
    [std::exchange(_contextMenuHintContainerView, nil) removeFromSuperview];
}

#endif // USE(UICONTEXTMENU)

#if HAVE(UI_WK_DOCUMENT_CONTEXT)

static inline OptionSet<WebKit::DocumentEditingContextRequest::Options> toWebDocumentRequestOptions(UIWKDocumentRequestFlags flags)
{
    OptionSet<WebKit::DocumentEditingContextRequest::Options> options;

    if (flags & UIWKDocumentRequestText)
        options.add(WebKit::DocumentEditingContextRequest::Options::Text);
    if (flags & UIWKDocumentRequestAttributed)
        options.add(WebKit::DocumentEditingContextRequest::Options::AttributedText);
    if (flags & UIWKDocumentRequestRects)
        options.add(WebKit::DocumentEditingContextRequest::Options::Rects);
    if (flags & UIWKDocumentRequestSpatial)
        options.add(WebKit::DocumentEditingContextRequest::Options::Spatial);
    if (flags & UIWKDocumentRequestAnnotation)
        options.add(WebKit::DocumentEditingContextRequest::Options::Annotation);
    if (flags & UIWKDocumentRequestMarkedTextRects)
        options.add(WebKit::DocumentEditingContextRequest::Options::MarkedTextRects);

    return options;
}

static WebKit::DocumentEditingContextRequest toWebRequest(UIWKDocumentRequest *request)
{
    WebKit::DocumentEditingContextRequest webRequest = {
        .options = toWebDocumentRequestOptions(request.flags),
        .surroundingGranularity = toWKTextGranularity(request.surroundingGranularity),
        .granularityCount = request.granularityCount,
        .rect = request.documentRect
    };

    if (auto textInputContext = dynamic_objc_cast<_WKTextInputContext>(request.inputElementIdentifier))
        webRequest.textInputContext = [textInputContext _textInputContext];

    return webRequest;
}

- (void)adjustSelectionWithDelta:(NSRange)deltaRange completionHandler:(void (^)(void))completionHandler
{
    // UIKit is putting casted signed integers into NSRange. Cast them back to reveal any negative values.
    _page->updateSelectionWithDelta(static_cast<int64_t>(deltaRange.location), static_cast<int64_t>(deltaRange.length), [capturedCompletionHandler = makeBlockPtr(completionHandler)] {
        capturedCompletionHandler();
    });
}

- (void)requestDocumentContext:(UIWKDocumentRequest *)request completionHandler:(void (^)(UIWKDocumentContext *))completionHandler
{
    auto webRequest = toWebRequest(request);
    OptionSet<WebKit::DocumentEditingContextRequest::Options> options = webRequest.options;
    _page->requestDocumentEditingContext(webRequest, [capturedCompletionHandler = makeBlockPtr(completionHandler), options] (WebKit::DocumentEditingContext editingContext) {
        capturedCompletionHandler(editingContext.toPlatformContext(options));
    });
}

- (void)selectPositionAtPoint:(CGPoint)point withContextRequest:(UIWKDocumentRequest *)request completionHandler:(void (^)(UIWKDocumentContext *))completionHandler
{
    // FIXME: Reduce to 1 message.
    [self selectPositionAtPoint:point completionHandler:^{
        [self requestDocumentContext:request completionHandler:^(UIWKDocumentContext *context) {
            completionHandler(context);
        }];
    }];
}

#endif

- (void)insertTextPlaceholderWithSize:(CGSize)size completionHandler:(void (^)(UITextPlaceholder *))completionHandler
{
    _page->insertTextPlaceholder(WebCore::IntSize { size }, [weakSelf = WeakObjCPtr<WKContentView>(self), completionHandler = makeBlockPtr(completionHandler)](const Optional<WebCore::ElementContext>& placeholder) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || ![strongSelf webView] || !placeholder) {
            completionHandler(nil);
            return;
        }
        WebCore::ElementContext placeholderToUse { *placeholder };
        placeholderToUse.boundingRect = [strongSelf convertRect:placeholderToUse.boundingRect fromView:[strongSelf webView]];
        completionHandler([[[WKTextPlaceholder alloc] initWithElementContext:placeholderToUse] autorelease]);
    });
}

- (void)removeTextPlaceholder:(UITextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText completionHandler:(void (^)(void))completionHandler
{
    // FIXME: Implement support for willInsertText. See <https://bugs.webkit.org/show_bug.cgi?id=208747>.
    if (auto* wkTextPlaceholder = dynamic_objc_cast<WKTextPlaceholder>(placeholder))
        _page->removeTextPlaceholder(wkTextPlaceholder.elementContext, makeBlockPtr(completionHandler));
}

static Vector<WebCore::IntSize> sizesOfPlaceholderElementsToInsertWhenDroppingItems(NSArray<NSItemProvider *> *itemProviders)
{
    Vector<WebCore::IntSize> sizes;
    for (NSItemProvider *item in itemProviders) {
        if (!WebCore::MIMETypeRegistry::isSupportedImageMIMEType(WebCore::MIMETypeFromUTI(item.web_fileUploadContentTypes.firstObject)))
            return { };

        WebCore::IntSize presentationSize(item.preferredPresentationSize);
        if (presentationSize.isEmpty())
            return { };

        sizes.append(WTFMove(presentationSize));
    }
    return sizes;
}

- (BOOL)_handleDropByInsertingImagePlaceholders:(NSArray<NSItemProvider *> *)itemProviders session:(id <UIDropSession>)session
{
    if (!self.webView._editable)
        return NO;

    if (_dragDropInteractionState.dragSession())
        return NO;

    if (session.items.count != itemProviders.count)
        return NO;

    auto imagePlaceholderSizes = sizesOfPlaceholderElementsToInsertWhenDroppingItems(itemProviders);
    if (imagePlaceholderSizes.isEmpty())
        return NO;

    RELEASE_LOG(DragAndDrop, "Inserting dropped image placeholders for session: %p", session);

    _page->insertDroppedImagePlaceholders(imagePlaceholderSizes, [protectedSelf = retainPtr(self), dragItems = retainPtr(session.items)] (auto& placeholderRects, auto data) {
        auto& state = protectedSelf->_dragDropInteractionState;
        if (!data || !protectedSelf->_dropAnimationCount) {
            RELEASE_LOG(DragAndDrop, "Failed to animate image placeholders: missing text indicator data.");
            state.clearAllDelayedItemPreviewProviders();
            return;
        }

        auto snapshotWithoutSelection = data->contentImageWithoutSelection;
        if (!snapshotWithoutSelection) {
            RELEASE_LOG(DragAndDrop, "Failed to animate image placeholders: missing unselected content image.");
            state.clearAllDelayedItemPreviewProviders();
            return;
        }

        auto unselectedSnapshotImage = snapshotWithoutSelection->nativeImage();
        if (!unselectedSnapshotImage) {
            RELEASE_LOG(DragAndDrop, "Failed to animate image placeholders: could not decode unselected content image.");
            state.clearAllDelayedItemPreviewProviders();
            return;
        }

        auto unselectedContentImageForEditDrag = adoptNS([[UIImage alloc] initWithCGImage:unselectedSnapshotImage.get() scale:protectedSelf->_page->deviceScaleFactor() orientation:UIImageOrientationUp]);
        auto snapshotView = adoptNS([[UIImageView alloc] initWithImage:unselectedContentImageForEditDrag.get()]);
        [snapshotView setFrame:data->contentImageWithoutSelectionRectInRootViewCoordinates];
        [protectedSelf addSubview:snapshotView.get()];
        protectedSelf->_unselectedContentSnapshot = WTFMove(snapshotView);
        state.deliverDelayedDropPreview(protectedSelf.get(), [protectedSelf unobscuredContentRect], dragItems.get(), placeholderRects);
    });

    return YES;
}

#pragma mark - UIDragInteractionDelegate

- (BOOL)_dragInteraction:(UIDragInteraction *)interaction shouldDelayCompetingGestureRecognizer:(UIGestureRecognizer *)competingGestureRecognizer
{
    if (_highlightLongPressGestureRecognizer == competingGestureRecognizer) {
        // Since 3D touch still recognizes alongside the drag lift, and also requires the highlight long press
        // gesture to be active to support cancelling when `touchstart` is prevented, we should also allow the
        // highlight long press to recognize simultaneously, and manually cancel it when the drag lift is
        // recognized (see _dragInteraction:prepareForSession:completion:).
        return NO;
    }
    return [competingGestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]];
}

- (NSInteger)_dragInteraction:(UIDragInteraction *)interaction dataOwnerForSession:(id <UIDragSession>)session
{
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    NSInteger dataOwner = 0;
    if ([uiDelegate respondsToSelector:@selector(_webView:dataOwnerForDragSession:)])
        dataOwner = [uiDelegate _webView:self.webView dataOwnerForDragSession:session];
    return dataOwner;
}

- (void)_dragInteraction:(UIDragInteraction *)interaction itemsForAddingToSession:(id <UIDragSession>)session withTouchAtPoint:(CGPoint)point completion:(void(^)(NSArray<UIDragItem *> *))completion
{
    if (!_dragDropInteractionState.shouldRequestAdditionalItemForDragSession(session)) {
        completion(@[ ]);
        return;
    }

    _dragDropInteractionState.dragSessionWillRequestAdditionalItem(completion);
    _page->requestAdditionalItemsForDragSession(WebCore::roundedIntPoint(point), WebCore::roundedIntPoint(point), self._allowedDragSourceActions);
}

- (void)_dragInteraction:(UIDragInteraction *)interaction prepareForSession:(id <UIDragSession>)session completion:(dispatch_block_t)completion
{
    [self _cancelLongPressGestureRecognizer];

    RELEASE_LOG(DragAndDrop, "Preparing for drag session: %p", session);
    if (self.currentDragOrDropSession) {
        // FIXME: Support multiple simultaneous drag sessions in the future.
        RELEASE_LOG(DragAndDrop, "Drag session failed: %p (a current drag session already exists)", session);
        completion();
        return;
    }

    [self cleanUpDragSourceSessionState];

    _dragDropInteractionState.prepareForDragSession(session, completion);

    auto dragOrigin = WebCore::roundedIntPoint([session locationInView:self]);
    _page->requestDragStart(dragOrigin, WebCore::roundedIntPoint([self convertPoint:dragOrigin toView:self.window]), self._allowedDragSourceActions);

    RELEASE_LOG(DragAndDrop, "Drag session requested: %p at origin: {%d, %d}", session, dragOrigin.x(), dragOrigin.y());
}

- (NSArray<UIDragItem *> *)dragInteraction:(UIDragInteraction *)interaction itemsForBeginningSession:(id <UIDragSession>)session
{
    ASSERT(interaction == _dragInteraction);
    RELEASE_LOG(DragAndDrop, "Drag items requested for session: %p", session);
    if (_dragDropInteractionState.dragSession() != session) {
        RELEASE_LOG(DragAndDrop, "Drag session failed: %p (delegate session does not match %p)", session, _dragDropInteractionState.dragSession());
        return @[ ];
    }

    if (!_dragDropInteractionState.hasStagedDragSource()) {
        RELEASE_LOG(DragAndDrop, "Drag session failed: %p (missing staged drag source)", session);
        return @[ ];
    }

    auto stagedDragSource = _dragDropInteractionState.stagedDragSource();
    auto *registrationLists = [[WebItemProviderPasteboard sharedInstance] takeRegistrationLists];
    NSArray *dragItems = [self _itemsForBeginningOrAddingToSessionWithRegistrationLists:registrationLists stagedDragSource:stagedDragSource];
    if (![dragItems count])
        _page->dragCancelled();

    RELEASE_LOG(DragAndDrop, "Drag session: %p starting with %tu items", session, [dragItems count]);
    _dragDropInteractionState.clearStagedDragSource([dragItems count] ? WebKit::DragDropInteractionState::DidBecomeActive::Yes : WebKit::DragDropInteractionState::DidBecomeActive::No);

    return dragItems;
}

- (UITargetedDragPreview *)dragInteraction:(UIDragInteraction *)interaction previewForLiftingItem:(UIDragItem *)item session:(id <UIDragSession>)session
{
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:previewForLiftingItem:session:)]) {
        UITargetedDragPreview *overriddenPreview = [uiDelegate _webView:self.webView previewForLiftingItem:item session:session];
        if (overriddenPreview)
            return overriddenPreview;
    }
    return _dragDropInteractionState.previewForDragItem(item, self, self.containerForDragPreviews);
}

- (void)dragInteraction:(UIDragInteraction *)interaction willAnimateLiftWithAnimator:(id <UIDragAnimating>)animator session:(id <UIDragSession>)session
{
    RELEASE_LOG(DragAndDrop, "Drag session willAnimateLiftWithAnimator: %p", session);
    if (_dragDropInteractionState.anyActiveDragSourceIs(WebCore::DragSourceActionSelection)) {
        [self cancelActiveTextInteractionGestures];
        if (!_shouldRestoreCalloutBarAfterDrop) {
            // FIXME: This SPI should be renamed in UIKit to reflect a more general purpose of hiding interaction assistant controls.
            [_textInteractionAssistant willStartScrollingOverflow];
            _shouldRestoreCalloutBarAfterDrop = YES;
        }
    }

    auto positionForDragEnd = WebCore::roundedIntPoint(_dragDropInteractionState.adjustedPositionForDragEnd());
    RetainPtr<WKContentView> protectedSelf(self);
    [animator addCompletion:[session, positionForDragEnd, protectedSelf, page = _page] (UIViewAnimatingPosition finalPosition) {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(session);
#endif
        if (finalPosition == UIViewAnimatingPositionStart) {
            RELEASE_LOG(DragAndDrop, "Drag session ended at start: %p", session);
            // The lift was canceled, so -dropInteraction:sessionDidEnd: will never be invoked. This is the last chance to clean up.
            [protectedSelf cleanUpDragSourceSessionState];
            page->dragEnded(positionForDragEnd, positionForDragEnd, WebCore::DragOperationNone);
        }
#if !RELEASE_LOG_DISABLED
        else
            RELEASE_LOG(DragAndDrop, "Drag session did not end at start: %p", session);
#endif
    }];
}

- (void)dragInteraction:(UIDragInteraction *)interaction sessionWillBegin:(id <UIDragSession>)session
{
    RELEASE_LOG(DragAndDrop, "Drag session beginning: %p", session);
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:dataInteraction:sessionWillBegin:)])
        [uiDelegate _webView:self.webView dataInteraction:interaction sessionWillBegin:session];

    [_actionSheetAssistant cleanupSheet];
    _dragDropInteractionState.dragSessionWillBegin();
    _page->didStartDrag();
}

- (void)dragInteraction:(UIDragInteraction *)interaction session:(id <UIDragSession>)session didEndWithOperation:(UIDropOperation)operation
{
    RELEASE_LOG(DragAndDrop, "Drag session ended: %p (with operation: %tu, performing operation: %d, began dragging: %d)", session, operation, _dragDropInteractionState.isPerformingDrop(), _dragDropInteractionState.didBeginDragging());

    [self _restoreCalloutBarIfNeeded];

    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:dataInteraction:session:didEndWithOperation:)])
        [uiDelegate _webView:self.webView dataInteraction:interaction session:session didEndWithOperation:operation];

    if (_dragDropInteractionState.isPerformingDrop())
        return;

    [self cleanUpDragSourceSessionState];
    _page->dragEnded(WebCore::roundedIntPoint(_dragDropInteractionState.adjustedPositionForDragEnd()), WebCore::roundedIntPoint(_dragDropInteractionState.adjustedPositionForDragEnd()), operation);
}

- (UITargetedDragPreview *)dragInteraction:(UIDragInteraction *)interaction previewForCancellingItem:(UIDragItem *)item withDefault:(UITargetedDragPreview *)defaultPreview
{
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:previewForCancellingItem:withDefault:)]) {
        UITargetedDragPreview *overriddenPreview = [uiDelegate _webView:self.webView previewForCancellingItem:item withDefault:defaultPreview];
        if (overriddenPreview)
            return overriddenPreview;
    }
    return _dragDropInteractionState.previewForDragItem(item, self, self.unscaledView);
}

- (BOOL)_dragInteraction:(UIDragInteraction *)interaction item:(UIDragItem *)item shouldDelaySetDownAnimationWithCompletion:(void(^)(void))completion
{
    _dragDropInteractionState.dragSessionWillDelaySetDownAnimation(completion);
    return YES;
}

- (void)dragInteraction:(UIDragInteraction *)interaction item:(UIDragItem *)item willAnimateCancelWithAnimator:(id <UIDragAnimating>)animator
{
    RELEASE_LOG(DragAndDrop, "Drag interaction willAnimateCancelWithAnimator");
    [animator addCompletion:[protectedSelf = retainPtr(self), page = _page] (UIViewAnimatingPosition finalPosition) {
        RELEASE_LOG(DragAndDrop, "Drag interaction willAnimateCancelWithAnimator (animation completion block fired)");
        page->dragCancelled();
        if (auto completion = protectedSelf->_dragDropInteractionState.takeDragCancelSetDownBlock()) {
            page->callAfterNextPresentationUpdate([completion] (WebKit::CallbackBase::Error) {
                completion();
            });
        }
    }];
}

- (void)dragInteraction:(UIDragInteraction *)interaction sessionDidTransferItems:(id <UIDragSession>)session
{
    [existingLocalDragSessionContext(session) cleanUpTemporaryDirectories];
}

#pragma mark - UIDropInteractionDelegate

- (NSInteger)_dropInteraction:(UIDropInteraction *)interaction dataOwnerForSession:(id <UIDropSession>)session
{
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    NSInteger dataOwner = 0;
    if ([uiDelegate respondsToSelector:@selector(_webView:dataOwnerForDropSession:)])
        dataOwner = [uiDelegate _webView:self.webView dataOwnerForDropSession:session];
    return dataOwner;
}

- (BOOL)dropInteraction:(UIDropInteraction *)interaction canHandleSession:(id<UIDropSession>)session
{
    // FIXME: Support multiple simultaneous drop sessions in the future.
    id <UIDragDropSession> dragOrDropSession = self.currentDragOrDropSession;
    RELEASE_LOG(DragAndDrop, "Can handle drag session: %p with local session: %p existing session: %p?", session, session.localDragSession, dragOrDropSession);

    return !dragOrDropSession || session.localDragSession == dragOrDropSession;
}

- (void)dropInteraction:(UIDropInteraction *)interaction sessionDidEnter:(id <UIDropSession>)session
{
    RELEASE_LOG(DragAndDrop, "Drop session entered: %p with %tu items", session, session.items.count);
    auto dragData = [self dragDataForDropSession:session dragDestinationAction:[self _dragDestinationActionForDropSession:session]];

    _dragDropInteractionState.dropSessionDidEnterOrUpdate(session, dragData);

    [[WebItemProviderPasteboard sharedInstance] setItemProviders:extractItemProvidersFromDropSession(session)];
    _page->dragEntered(dragData, WebCore::Pasteboard::nameOfDragPasteboard());
}

- (UIDropProposal *)dropInteraction:(UIDropInteraction *)interaction sessionDidUpdate:(id <UIDropSession>)session
{
    [[WebItemProviderPasteboard sharedInstance] setItemProviders:extractItemProvidersFromDropSession(session)];

    auto dragData = [self dragDataForDropSession:session dragDestinationAction:[self _dragDestinationActionForDropSession:session]];
    _page->dragUpdated(dragData, WebCore::Pasteboard::nameOfDragPasteboard());
    _dragDropInteractionState.dropSessionDidEnterOrUpdate(session, dragData);

    auto delegate = self.webViewUIDelegate;
    auto operation = dropOperationForWebCoreDragOperation(_page->currentDragOperation());
    if ([delegate respondsToSelector:@selector(_webView:willUpdateDataInteractionOperationToOperation:forSession:)])
        operation = static_cast<UIDropOperation>([delegate _webView:self.webView willUpdateDataInteractionOperationToOperation:operation forSession:session]);

    auto proposal = adoptNS([[UIDropProposal alloc] initWithDropOperation:static_cast<UIDropOperation>(operation)]);
    auto dragHandlingMethod = _page->currentDragHandlingMethod();
    if (dragHandlingMethod == WebCore::DragHandlingMethod::EditPlainText || dragHandlingMethod == WebCore::DragHandlingMethod::EditRichText) {
        // When dragging near the top or bottom edges of an editable element, enabling precision drop mode may result in the drag session hit-testing outside of the editable
        // element, causing the drag to no longer be accepted. This in turn disables precision drop mode, which causes the drag session to hit-test inside of the editable
        // element again, which enables precision mode, thus continuing the cycle. To avoid precision mode thrashing, we forbid precision mode when dragging near the top or
        // bottom of the editable element.
        auto minimumDistanceFromVerticalEdgeForPreciseDrop = 25 / self.webView.scrollView.zoomScale;
        [proposal setPrecise:CGRectContainsPoint(CGRectInset(_page->currentDragCaretEditableElementRect(), 0, minimumDistanceFromVerticalEdgeForPreciseDrop), [session locationInView:self])];
    } else
        [proposal setPrecise:NO];

    if ([delegate respondsToSelector:@selector(_webView:willUpdateDropProposalToProposal:forSession:)])
        proposal = [delegate _webView:self.webView willUpdateDropProposalToProposal:proposal.get() forSession:session];

    return proposal.autorelease();
}

- (void)dropInteraction:(UIDropInteraction *)interaction sessionDidExit:(id <UIDropSession>)session
{
    RELEASE_LOG(DragAndDrop, "Drop session exited: %p with %tu items", session, session.items.count);
    [[WebItemProviderPasteboard sharedInstance] setItemProviders:extractItemProvidersFromDropSession(session)];

    auto dragData = [self dragDataForDropSession:session dragDestinationAction:WKDragDestinationActionAny];
    _page->dragExited(dragData, WebCore::Pasteboard::nameOfDragPasteboard());
    _page->resetCurrentDragInformation();

    _dragDropInteractionState.dropSessionDidExit();
}

- (void)dropInteraction:(UIDropInteraction *)interaction performDrop:(id <UIDropSession>)session
{
    NSArray <NSItemProvider *> *itemProviders = extractItemProvidersFromDropSession(session);
    id <WKUIDelegatePrivate> uiDelegate = self.webViewUIDelegate;
    if ([uiDelegate respondsToSelector:@selector(_webView:performDataInteractionOperationWithItemProviders:)]) {
        if ([uiDelegate _webView:self.webView performDataInteractionOperationWithItemProviders:itemProviders])
            return;
    }

    if ([uiDelegate respondsToSelector:@selector(_webView:willPerformDropWithSession:)]) {
        itemProviders = extractItemProvidersFromDragItems([uiDelegate _webView:self.webView willPerformDropWithSession:session]);
        if (!itemProviders.count)
            return;
    }

    _dragDropInteractionState.dropSessionWillPerformDrop();

    [[WebItemProviderPasteboard sharedInstance] setItemProviders:itemProviders];
    [[WebItemProviderPasteboard sharedInstance] incrementPendingOperationCount];
    auto dragData = [self dragDataForDropSession:session dragDestinationAction:WKDragDestinationActionAny];
    BOOL shouldSnapshotView = ![self _handleDropByInsertingImagePlaceholders:itemProviders session:session];

    RELEASE_LOG(DragAndDrop, "Loading data from %tu item providers for session: %p", itemProviders.count, session);
    // Always loading content from the item provider ensures that the web process will be allowed to call back in to the UI
    // process to access pasteboard contents at a later time. Ideally, we only need to do this work if we're over a file input
    // or the page prevented default on `dragover`, but without this, dropping into a normal editable areas will fail due to
    // item providers not loading any data.
    RetainPtr<WKContentView> retainedSelf(self);
    [[WebItemProviderPasteboard sharedInstance] doAfterLoadingProvidedContentIntoFileURLs:[retainedSelf, capturedDragData = WTFMove(dragData), shouldSnapshotView] (NSArray *fileURLs) mutable {
        RELEASE_LOG(DragAndDrop, "Loaded data into %tu files", fileURLs.count);
        Vector<String> filenames;
        for (NSURL *fileURL in fileURLs)
            filenames.append([fileURL path]);
        capturedDragData.setFileNames(filenames);

        WebKit::SandboxExtension::Handle sandboxExtensionHandle;
        WebKit::SandboxExtension::HandleArray sandboxExtensionForUpload;
        auto dragPasteboardName = WebCore::Pasteboard::nameOfDragPasteboard();
        retainedSelf->_page->grantAccessToCurrentPasteboardData(dragPasteboardName);
        retainedSelf->_page->createSandboxExtensionsIfNeeded(filenames, sandboxExtensionHandle, sandboxExtensionForUpload);
        retainedSelf->_page->performDragOperation(capturedDragData, dragPasteboardName, WTFMove(sandboxExtensionHandle), WTFMove(sandboxExtensionForUpload));
        if (shouldSnapshotView) {
            retainedSelf->_visibleContentViewSnapshot = [retainedSelf snapshotViewAfterScreenUpdates:NO];
            [retainedSelf->_visibleContentViewSnapshot setFrame:[retainedSelf bounds]];
            [retainedSelf addSubview:retainedSelf->_visibleContentViewSnapshot.get()];
        }
    }];
}

- (void)dropInteraction:(UIDropInteraction *)interaction item:(UIDragItem *)item willAnimateDropWithAnimator:(id <UIDragAnimating>)animator
{
    _dropAnimationCount++;
    [animator addCompletion:[strongSelf = retainPtr(self)] (UIViewAnimatingPosition) {
        if (!--strongSelf->_dropAnimationCount)
            [std::exchange(strongSelf->_unselectedContentSnapshot, nil) removeFromSuperview];
    }];
}

- (void)dropInteraction:(UIDropInteraction *)interaction concludeDrop:(id <UIDropSession>)session
{
    [std::exchange(_dropPreviewContainerView, nil) removeFromSuperview];
    [std::exchange(_visibleContentViewSnapshot, nil) removeFromSuperview];
    [std::exchange(_unselectedContentSnapshot, nil) removeFromSuperview];
    _dragDropInteractionState.clearAllDelayedItemPreviewProviders();
    _page->didConcludeDrop();
}

- (UITargetedDragPreview *)dropInteraction:(UIDropInteraction *)interaction previewForDroppingItem:(UIDragItem *)item withDefault:(UITargetedDragPreview *)defaultPreview
{
    _dragDropInteractionState.setDefaultDropPreview(item, defaultPreview);

    CGRect caretRect = _page->currentDragCaretRect();
    if (CGRectIsEmpty(caretRect))
        return nil;

    UIView *textEffectsWindow = self.textEffectsWindow;
    auto caretRectInWindowCoordinates = [self convertRect:caretRect toView:textEffectsWindow];
    auto caretCenterInWindowCoordinates = CGPointMake(CGRectGetMidX(caretRectInWindowCoordinates), CGRectGetMidY(caretRectInWindowCoordinates));
    auto targetPreviewCenterInWindowCoordinates = CGPointMake(caretCenterInWindowCoordinates.x + defaultPreview.size.width / 2, caretCenterInWindowCoordinates.y + defaultPreview.size.height / 2);
    auto target = adoptNS([[UIDragPreviewTarget alloc] initWithContainer:textEffectsWindow center:targetPreviewCenterInWindowCoordinates transform:CGAffineTransformIdentity]);
    return [defaultPreview retargetedPreviewWithTarget:target.get()];
}

- (void)_dropInteraction:(UIDropInteraction *)interaction delayedPreviewProviderForDroppingItem:(UIDragItem *)item previewProvider:(void(^)(UITargetedDragPreview *preview))previewProvider
{
    // FIXME: This doesn't currently handle multiple items in a drop session.
    _dragDropInteractionState.prepareForDelayedDropPreview(item, previewProvider);
}

- (void)dropInteraction:(UIDropInteraction *)interaction sessionDidEnd:(id <UIDropSession>)session
{
    RELEASE_LOG(DragAndDrop, "Drop session ended: %p (performing operation: %d, began dragging: %d)", session, _dragDropInteractionState.isPerformingDrop(), _dragDropInteractionState.didBeginDragging());
    if (_dragDropInteractionState.isPerformingDrop()) {
        // In the case where we are performing a drop, wait until after the drop is handled in the web process to reset drag and drop interaction state.
        return;
    }

    if (_dragDropInteractionState.didBeginDragging()) {
        // In the case where the content view is a source of drag items, wait until -dragInteraction:session:didEndWithOperation: to reset drag and drop interaction state.
        return;
    }

    CGPoint global;
    CGPoint client;
    [self computeClientAndGlobalPointsForDropSession:session outClientPoint:&client outGlobalPoint:&global];
    [self cleanUpDragSourceSessionState];
    _page->dragEnded(WebCore::roundedIntPoint(client), WebCore::roundedIntPoint(global), WebCore::DragOperationNone);
}

#endif

#if PLATFORM(WATCHOS)

- (void)dismissQuickboardViewControllerAndRevealFocusedFormOverlayIfNecessary:(PUICQuickboardViewController *)quickboard
{
    BOOL shouldRevealFocusOverlay = NO;
    // In the case where there's nothing the user could potentially do besides dismiss the overlay, we can just automatically without asking the delegate.
    if ([self.webView._inputDelegate respondsToSelector:@selector(_webView:shouldRevealFocusOverlayForInputSession:)]
        && ([self actionNameForFocusedFormControlView:_focusedFormControlView.get()] || _focusedElementInformation.hasNextNode || _focusedElementInformation.hasPreviousNode))
        shouldRevealFocusOverlay = [self.webView._inputDelegate _webView:self.webView shouldRevealFocusOverlayForInputSession:_formInputSession.get()];

    if (shouldRevealFocusOverlay) {
        [_focusedFormControlView show:NO];
        [self updateCurrentFocusedElementInformation:[weakSelf = WeakObjCPtr<WKContentView>(self)] (bool didUpdate) {
            if (!didUpdate)
                return;

            auto focusedFormController = weakSelf.get()->_focusedFormControlView;
            [focusedFormController reloadData:YES];
            [focusedFormController engageFocusedFormControlNavigation];
        }];
    } else
        _page->blurFocusedElement();

    // The Quickboard view controller passed into this delegate method is not necessarily the view controller we originally presented;
    // this happens in the case when the user chooses an input method (e.g. scribble) and a new Quickboard view controller is presented.
    if (quickboard != _presentedFullScreenInputViewController)
        [quickboard dismissViewControllerAnimated:YES completion:nil];

    [self dismissAllInputViewControllers:quickboard == _presentedFullScreenInputViewController];
}

#pragma mark - PUICQuickboardViewControllerDelegate

- (void)quickboard:(PUICQuickboardViewController *)quickboard textEntered:(NSAttributedString *)attributedText
{
    if (attributedText)
        _page->setTextAsync(attributedText.string);

    [self dismissQuickboardViewControllerAndRevealFocusedFormOverlayIfNecessary:quickboard];
}

- (void)quickboardInputCancelled:(PUICQuickboardViewController *)quickboard
{
    [self dismissQuickboardViewControllerAndRevealFocusedFormOverlayIfNecessary:quickboard];
}

#pragma mark - WKQuickboardViewControllerDelegate

- (CGFloat)viewController:(PUICQuickboardViewController *)controller inputContextViewHeightForSize:(CGSize)size
{
    id <_WKInputDelegate> delegate = self.webView._inputDelegate;
    if (![delegate respondsToSelector:@selector(_webView:focusedElementContextViewHeightForFittingSize:inputSession:)])
        return 0;

    return [delegate _webView:self.webView focusedElementContextViewHeightForFittingSize:size inputSession:_formInputSession.get()];
}

- (BOOL)allowsLanguageSelectionMenuForListViewController:(PUICQuickboardViewController *)controller
{
    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::ContentEditable:
    case WebKit::InputType::Text:
    case WebKit::InputType::TextArea:
    case WebKit::InputType::Search:
    case WebKit::InputType::Email:
    case WebKit::InputType::URL:
        return YES;
    default:
        return NO;
    }
}

- (UIView *)inputContextViewForViewController:(PUICQuickboardViewController *)controller
{
    id <_WKInputDelegate> delegate = self.webView._inputDelegate;
    if (![delegate respondsToSelector:@selector(_webView:focusedElementContextViewForInputSession:)])
        return nil;

    return [delegate _webView:self.webView focusedElementContextViewForInputSession:_formInputSession.get()];
}

- (NSString *)inputLabelTextForViewController:(PUICQuickboardViewController *)controller
{
    return [self inputLabelText];
}

- (NSString *)initialValueForViewController:(PUICQuickboardViewController *)controller
{
    return _focusedElementInformation.value;
}

- (BOOL)shouldDisplayInputContextViewForListViewController:(PUICQuickboardViewController *)controller
{
    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::ContentEditable:
    case WebKit::InputType::Text:
    case WebKit::InputType::Password:
    case WebKit::InputType::TextArea:
    case WebKit::InputType::Search:
    case WebKit::InputType::Email:
    case WebKit::InputType::URL:
    case WebKit::InputType::Phone:
        return YES;
    default:
        return NO;
    }
}

#pragma mark - WKTextInputListViewControllerDelegate

- (WKNumberPadInputMode)numericInputModeForListViewController:(WKTextInputListViewController *)controller
{
    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::Phone:
        return WKNumberPadInputModeTelephone;
    case WebKit::InputType::Number:
        return WKNumberPadInputModeNumbersAndSymbols;
    case WebKit::InputType::NumberPad:
        return WKNumberPadInputModeNumbersOnly;
    default:
        return WKNumberPadInputModeNone;
    }
}

- (NSString *)textContentTypeForListViewController:(WKTextInputListViewController *)controller
{
    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::Password:
        return UITextContentTypePassword;
    case WebKit::InputType::URL:
        return UITextContentTypeURL;
    case WebKit::InputType::Email:
        return UITextContentTypeEmailAddress;
    case WebKit::InputType::Phone:
        return UITextContentTypeTelephoneNumber;
    default:
        // The element type alone is insufficient to infer content type; fall back to autofill data.
        if (NSString *contentType = contentTypeFromFieldName(_focusedElementInformation.autofillFieldName))
            return contentType;

        if (_focusedElementInformation.isAutofillableUsernameField)
            return UITextContentTypeUsername;

        return nil;
    }
}

- (NSArray<UITextSuggestion *> *)textSuggestionsForListViewController:(WKTextInputListViewController *)controller
{
    return [_focusedFormControlView suggestions];
}

- (void)listViewController:(WKTextInputListViewController *)controller didSelectTextSuggestion:(UITextSuggestion *)suggestion
{
    [self insertTextSuggestion:suggestion];
    [self dismissQuickboardViewControllerAndRevealFocusedFormOverlayIfNecessary:controller];
}

- (BOOL)allowsDictationInputForListViewController:(PUICQuickboardViewController *)controller
{
    return _focusedElementInformation.elementType != WebKit::InputType::Password;
}

#endif // PLATFORM(WATCHOS)

#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
- (void)_lookupGestureRecognized:(UIGestureRecognizer *)gestureRecognizer
{
    NSPoint locationInViewCoordinates = [gestureRecognizer locationInView:self];
    _page->performDictionaryLookupAtLocation(WebCore::FloatPoint(locationInViewCoordinates));
}
#endif

#if HAVE(PENCILKIT)
- (WKDrawingCoordinator *)_drawingCoordinator
{
    if (!_drawingCoordinator)
        _drawingCoordinator = adoptNS([[WKDrawingCoordinator alloc] initWithContentView:self]);
    return _drawingCoordinator.get();
}
#endif // HAVE(PENCILKIT)

- (void)setContinuousSpellCheckingEnabled:(BOOL)enabled
{
    if (WebKit::TextChecker::setContinuousSpellCheckingEnabled(enabled))
        _page->process().updateTextCheckerState();
}

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

- (BOOL)shouldUseMouseGestureRecognizer
{
    static const BOOL shouldUseMouseGestureRecognizer = []() -> BOOL {
        // System apps will always be linked on the current OS, so
        // check them before the linked-on-or-after.

        // <rdar://problem/59521967> iAd Video does not respond to mouse events, only touch events
        if (WebCore::IOSApplication::isNews() || WebCore::IOSApplication::isStocks())
            return NO;

        if (WebKit::linkedOnOrAfter(WebKit::SDKVersion::FirstThatSendsNativeMouseEvents))
            return YES;

        // <rdar://problem/59155220> Some Feedly UI does not respond to mouse events, only touch events
        if (WebCore::IOSApplication::isFeedly())
            return NO;

        // <rdar://problem/62273077> "Pocket City" does not respond to mouse events, only touch events
        if (WebCore::IOSApplication::isPocketCity())
            return NO;

        // <rdar://problem/62694519> "Essential Skeleton" does not respond to mouse events, only touch events
        if (WebCore::IOSApplication::isEssentialSkeleton())
            return NO;

        return YES;
    }();

    switch (_mouseEventPolicy) {
    case WebCore::MouseEventPolicy::Default:
        break;
#if ENABLE(IOS_TOUCH_EVENTS)
    case WebCore::MouseEventPolicy::SynthesizeTouchEvents:
        return NO;
#endif
    }

    return shouldUseMouseGestureRecognizer;
}

- (void)setUpMouseGestureRecognizer
{
    _mouseGestureRecognizer = adoptNS([[WKMouseGestureRecognizer alloc] initWithTarget:self action:@selector(mouseGestureRecognizerChanged:)]);
    [_mouseGestureRecognizer setDelegate:self];
    [self _configureMouseGestureRecognizer];
    [self addGestureRecognizer:_mouseGestureRecognizer.get()];
}

- (void)mouseGestureRecognizerChanged:(WKMouseGestureRecognizer *)gestureRecognizer
{
    if (!_page->hasRunningProcess())
        return;

    auto event = gestureRecognizer.lastMouseEvent;
    if (!event)
        return;

    if (event->type() == WebKit::WebEvent::MouseDown)
        _layerTreeTransactionIdAtLastInteractionStart = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).lastCommittedLayerTreeTransactionID();

    _page->handleMouseEvent(*event);
}

- (void)_configureMouseGestureRecognizer
{
    [_mouseGestureRecognizer setEnabled:[self shouldUseMouseGestureRecognizer]];
}

- (void)_setMouseEventPolicy:(WebCore::MouseEventPolicy)policy
{
    _mouseEventPolicy = policy;
    [self _configureMouseGestureRecognizer];
}

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#if HAVE(UI_CURSOR_INTERACTION)

- (void)setUpCursorInteraction
{
    _cursorInteraction = adoptNS([[_UICursorInteraction alloc] initWithDelegate:self]);
    [_cursorInteraction _setPausesCursorUpdatesWhilePanning:NO];

    [self addInteraction:_cursorInteraction.get()];
}

- (void)_cursorInteraction:(_UICursorInteraction *)interaction regionForLocation:(CGPoint)location defaultRegion:(_UICursorRegion *)defaultRegion completion:(void(^)(_UICursorRegion *region))completion
{
    uint64_t interactionRequestID = ++_currentCursorInteractionRequestID;

    WebKit::InteractionInformationRequest interactionInformationRequest;
    interactionInformationRequest.point = WebCore::roundedIntPoint(location);
    interactionInformationRequest.includeCaretContext = true;

    BOOL didSynchronouslyReplyWithApproximation = false;
    if (![self _currentPositionInformationIsValidForRequest:interactionInformationRequest] && self.webView._editable && !_positionInformation.shouldNotUseIBeamInEditableContent) {
        didSynchronouslyReplyWithApproximation = true;
        completion([_UICursorRegion regionWithIdentifier:editableCursorRegionIdentifier rect:self.bounds]);
    }

    [self doAfterPositionInformationUpdate:^(WebKit::InteractionInformationAtPosition interactionInformation) {
        if (interactionRequestID < _currentCursorInteractionRequestID)
            return;

        if (didSynchronouslyReplyWithApproximation) {
            [interaction invalidate];
            return;
        }

        completion([self cursorRegionForPositionInformation:interactionInformation point:location]);
    } forRequest:interactionInformationRequest];
}

- (_UICursorRegion *)cursorRegionForPositionInformation:(WebKit::InteractionInformationAtPosition&)interactionInformation point:(CGPoint)location
{
    WebCore::FloatRect expandedLineRect = enclosingIntRect(interactionInformation.lineCaretExtent);

    // Pad lines of text in order to avoid switching back to the dot cursor between lines.
    // This matches the value that UIKit uses.
    expandedLineRect.inflateY(10);

    if (interactionInformation.cursor) {
        WebCore::Cursor::Type cursorType = interactionInformation.cursor->type();
        if (cursorType == WebCore::Cursor::Hand)
            return [_UICursorRegion regionWithIdentifier:cursorRegionIdentifier rect:interactionInformation.bounds];

        if (cursorType == WebCore::Cursor::IBeam && expandedLineRect.contains(location))
            return [_UICursorRegion regionWithIdentifier:cursorRegionIdentifier rect:expandedLineRect];
    }

    if (self.webView._editable) {
        if (expandedLineRect.contains(location))
            return [_UICursorRegion regionWithIdentifier:cursorRegionIdentifier rect:expandedLineRect];
        return [_UICursorRegion regionWithIdentifier:editableCursorRegionIdentifier rect:self.bounds];
    }

    return nil;
}

- (_UICursorStyle *)cursorInteraction:(_UICursorInteraction *)interaction styleForRegion:(_UICursorRegion *)region modifiers:(UIKeyModifierFlags)modifiers
{
    double scaleFactor = self._contentZoomScale;

    _UICursorStyle *(^iBeamCursor)(void) = ^{
        float beamLength = _positionInformation.caretHeight * scaleFactor;
        UIAxis iBeamConstraintAxes = UIAxisVertical;

        // If the I-beam is so large that the magnetism is hard to fight, we should not apply any magnetism.
        if (beamLength > [UITextInteraction _maximumBeamSnappingLength])
            iBeamConstraintAxes = UIAxisNeither;

        // If the region is the size of the view, we should not apply any magnetism.
        if ([region.identifier isEqual:editableCursorRegionIdentifier])
            iBeamConstraintAxes = UIAxisNeither;

        return [_UICursorStyle styleWithCursor:[_UICursor beamWithPreferredLength:beamLength axis:UIAxisVertical] constrainedAxes:iBeamConstraintAxes];
    };

    if (self.webView._editable) {
        if (_positionInformation.shouldNotUseIBeamInEditableContent)
            return nil;
        return iBeamCursor();
    }

    if (_positionInformation.cursor && [region.identifier isEqual:cursorRegionIdentifier]) {
        WebCore::Cursor::Type cursorType = _positionInformation.cursor->type();

        if (cursorType == WebCore::Cursor::Hand)
            return [_UICursorStyle styleWithCursor:[_UICursor linkCursor] constrainedAxes:UIAxisNeither];

        if (cursorType == WebCore::Cursor::IBeam && _positionInformation.lineCaretExtent.contains(_positionInformation.request.point))
            return iBeamCursor();
    }

    ASSERT_NOT_REACHED();
    return nil;
}

#endif // HAVE(UI_CURSOR_INTERACTION)

#if ENABLE(ATTACHMENT_ELEMENT)

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

static RetainPtr<NSItemProvider> createItemProvider(const WebKit::WebPageProxy& page, const WebCore::PromisedAttachmentInfo& info)
{
    auto numberOfAdditionalTypes = info.additionalTypes.size();
    ASSERT(numberOfAdditionalTypes == info.additionalData.size());

    auto attachment = page.attachmentForIdentifier(info.attachmentIdentifier);
    if (!attachment)
        return { };

    NSString *utiType = attachment->utiType();
    if (![utiType length])
        return { };

    auto fileWrapper = retainPtr(attachment->fileWrapper());
    if (!fileWrapper)
        return { };

    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];

    NSString *fileName = attachment->fileName();
    if ([fileName length])
        [item setSuggestedName:fileName];

    if (numberOfAdditionalTypes == info.additionalData.size() && numberOfAdditionalTypes) {
        for (size_t index = 0; index < numberOfAdditionalTypes; ++index) {
            auto nsData = info.additionalData[index]->createNSData();
            [item registerDataRepresentationForTypeIdentifier:info.additionalTypes[index] visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[nsData](void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
                completionHandler(nsData.get(), nil);
                return nil;
            }];
        }
    }

    [item registerDataRepresentationForTypeIdentifier:utiType visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[fileWrapper](void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        if (auto nsData = retainPtr([fileWrapper serializedRepresentation]))
            completionHandler(nsData.get(), nil);
        else
            completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);
        return nil;
    }];

    return item;
}

#endif // !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)

- (void)_writePromisedAttachmentToPasteboard:(WebCore::PromisedAttachmentInfo&&)info
{
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    if (auto item = createItemProvider(*_page, WTFMove(info)))
        UIPasteboard.generalPasteboard.itemProviders = @[ item.get() ];
#else
    UNUSED_PARAM(info);
#endif
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

@end

@implementation WKContentView (WKTesting)

- (void)_doAfterResettingSingleTapGesture:(dispatch_block_t)action
{
    if ([_singleTapGestureRecognizer state] != UIGestureRecognizerStateEnded) {
        action();
        return;
    }
    _actionsToPerformAfterResettingSingleTapGestureRecognizer.append(makeBlockPtr(action));
}

- (void)_doAfterReceivingEditDragSnapshotForTesting:(dispatch_block_t)action
{
#if ENABLE(DRAG_SUPPORT)
    ASSERT(!_actionToPerformAfterReceivingEditDragSnapshot);
    if (_waitingForEditDragSnapshot) {
        _actionToPerformAfterReceivingEditDragSnapshot = action;
        return;
    }
#endif
    action();
}

- (WKDateTimeInputControl *)dateTimeInputControl
{
    if ([_inputPeripheral isKindOfClass:WKDateTimeInputControl.class])
        return (WKDateTimeInputControl *)_inputPeripheral.get();
    return nil;
}

- (void)_simulateTextEntered:(NSString *)text
{
#if PLATFORM(WATCHOS)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTextInputListViewController class]])
        [(WKTextInputListViewController *)_presentedFullScreenInputViewController.get() enterText:text];
#else
    [self insertText:text];
#endif
}

- (void)_simulateElementAction:(_WKElementActionType)actionType atLocation:(CGPoint)location
{
    RetainPtr<WKContentView> protectedSelf = self;
    [self doAfterPositionInformationUpdate:[actionType, protectedSelf] (WebKit::InteractionInformationAtPosition info) {
        _WKElementAction *action = [_WKElementAction _elementActionWithType:actionType assistant:protectedSelf->_actionSheetAssistant.get()];
        _WKActivatedElementInfo *elementInfo = [_WKActivatedElementInfo activatedElementInfoWithInteractionInformationAtPosition:info userInfo:nil];
        [action runActionWithElementInfo:elementInfo];
    } forRequest:WebKit::InteractionInformationRequest(WebCore::roundedIntPoint(location))];
}

- (void)_simulateLongPressActionAtLocation:(CGPoint)location
{
    RetainPtr<WKContentView> protectedSelf = self;
    [self doAfterPositionInformationUpdate:[protectedSelf] (WebKit::InteractionInformationAtPosition) {
        if (SEL action = [protectedSelf _actionForLongPress])
            [protectedSelf performSelector:action];
    } forRequest:WebKit::InteractionInformationRequest(WebCore::roundedIntPoint(location))];
}

- (void)selectFormAccessoryPickerRow:(NSInteger)rowIndex
{
#if PLATFORM(WATCHOS)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKSelectMenuListViewController class]])
        [(WKSelectMenuListViewController *)_presentedFullScreenInputViewController.get() selectItemAtIndex:rowIndex];
#else
    if ([_inputPeripheral isKindOfClass:[WKFormSelectControl class]])
        [(WKFormSelectControl *)_inputPeripheral selectRow:rowIndex inComponent:0 extendingSelection:NO];
#endif
}

- (BOOL)selectFormAccessoryHasCheckedItemAtRow:(long)rowIndex
{
#if !PLATFORM(WATCHOS)
    if ([_inputPeripheral isKindOfClass:[WKFormSelectControl self]])
        return [(WKFormSelectControl *)_inputPeripheral selectFormAccessoryHasCheckedItemAtRow:rowIndex];
#endif
    return NO;
}

- (NSString *)textContentTypeForTesting
{
#if PLATFORM(WATCHOS)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTextInputListViewController class]])
        return [self textContentTypeForListViewController:(WKTextInputListViewController *)_presentedFullScreenInputViewController.get()];
#endif
    return self.textInputTraits.textContentType;
}

- (NSString *)selectFormPopoverTitle
{
    if (![_inputPeripheral isKindOfClass:[WKFormSelectControl class]])
        return nil;

    return [(WKFormSelectControl *)_inputPeripheral selectFormPopoverTitle];
}

- (NSString *)formInputLabel
{
#if PLATFORM(WATCHOS)
    if (_presentedFullScreenInputViewController)
        return [self inputLabelTextForViewController:(id)_presentedFullScreenInputViewController.get()];
#endif
    return nil;
}

- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute
{
#if PLATFORM(WATCHOS)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTimePickerViewController class]])
        [(WKTimePickerViewController *)_presentedFullScreenInputViewController.get() setHour:hour minute:minute];
#elif PLATFORM(IOS_FAMILY)
    if ([_inputPeripheral isKindOfClass:[WKDateTimeInputControl class]])
        [(WKDateTimeInputControl *)_inputPeripheral.get() setTimePickerHour:hour minute:minute];
#endif
}

- (double)timePickerValueHour
{
#if PLATFORM(IOS_FAMILY)
    if ([_inputPeripheral isKindOfClass:[WKDateTimeInputControl class]])
        return [(WKDateTimeInputControl *)_inputPeripheral.get() timePickerValueHour];
#endif
    return -1;
}

- (double)timePickerValueMinute
{
#if PLATFORM(IOS_FAMILY)
    if ([_inputPeripheral isKindOfClass:[WKDateTimeInputControl class]])
        return [(WKDateTimeInputControl *)_inputPeripheral.get() timePickerValueMinute];
#endif
    return -1;
}

- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem
{
    if ([userInterfaceItem isEqualToString:@"actionSheet"])
        return @{ userInterfaceItem: [_actionSheetAssistant currentAvailableActionTitles] };

#if HAVE(LINK_PREVIEW)
    if ([userInterfaceItem isEqualToString:@"contextMenu"]) {
        if (self._shouldUseContextMenus)
            return @{ userInterfaceItem: @{
                @"url": _positionInformation.url.isValid() ? WTF::userVisibleString(_positionInformation.url) : @"",
                @"isLink": [NSNumber numberWithBool:_positionInformation.isLink],
                @"isImage": [NSNumber numberWithBool:_positionInformation.isImage],
                @"imageURL": _positionInformation.imageURL.isValid() ? WTF::userVisibleString(_positionInformation.imageURL) : @""
            } };
        NSString *url = [_previewItemController previewData][UIPreviewDataLink];
        return @{ userInterfaceItem: @{
            @"url": url,
            @"isLink": [NSNumber numberWithBool:_positionInformation.isLink],
            @"isImage": [NSNumber numberWithBool:_positionInformation.isImage],
            @"imageURL": _positionInformation.imageURL.isValid() ? WTF::userVisibleString(_positionInformation.imageURL) : @""
        } };
    }
#endif

    if ([userInterfaceItem isEqualToString:@"fileUploadPanelMenu"]) {
        if (!_fileUploadPanel)
            return @{ userInterfaceItem: @[] };
        return @{ userInterfaceItem: [_fileUploadPanel currentAvailableActionTitles] };
    }
    
    return nil;
}

@end

#if HAVE(LINK_PREVIEW)

static NSString *previewIdentifierForElementAction(_WKElementAction *action)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch (action.type) {
    case _WKElementActionTypeOpen:
        return WKPreviewActionItemIdentifierOpen;
    case _WKElementActionTypeCopy:
        return WKPreviewActionItemIdentifierCopy;
#if !defined(TARGET_OS_IOS) || TARGET_OS_IOS
    case _WKElementActionTypeAddToReadingList:
        return WKPreviewActionItemIdentifierAddToReadingList;
#endif
    case _WKElementActionTypeShare:
        return WKPreviewActionItemIdentifierShare;
    default:
        return nil;
    }
    ALLOW_DEPRECATED_DECLARATIONS_END
    ASSERT_NOT_REACHED();
    return nil;
}

@implementation WKContentView (WKInteractionPreview)

- (void)_registerPreview
{
    if (!self.webView.allowsLinkPreview)
        return;

#if USE(UICONTEXTMENU)
    if (self._shouldUseContextMenus) {
        _contextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
        _contextMenuHasRequestedLegacyData = NO;
        [self addInteraction:_contextMenuInteraction.get()];

        if (id<_UIClickInteractionDriving> driver = self.webView.configuration._clickInteractionDriverForTesting) {
            _UIClickInteraction *previewClickInteraction = [[_contextMenuInteraction presentationInteraction] previewClickInteraction];
            [previewClickInteraction setDriver:driver];
            [driver setDelegate:(id<_UIClickInteractionDriverDelegate>)previewClickInteraction];
        }
        return;
    }
#endif

    _previewItemController = adoptNS([[UIPreviewItemController alloc] initWithView:self]);
    [_previewItemController setDelegate:self];
    _previewGestureRecognizer = _previewItemController.get().presentationGestureRecognizer;
    if ([_previewItemController respondsToSelector:@selector(presentationSecondaryGestureRecognizer)])
        _previewSecondaryGestureRecognizer = _previewItemController.get().presentationSecondaryGestureRecognizer;
}

- (void)_unregisterPreview
{
#if USE(UICONTEXTMENU)
    if (self._shouldUseContextMenus) {
        if (!_contextMenuInteraction)
            return;

        [self removeInteraction:_contextMenuInteraction.get()];
        _contextMenuInteraction = nil;
        return;
    }
#endif

    [_previewItemController setDelegate:nil];
    _previewGestureRecognizer = nil;
    _previewSecondaryGestureRecognizer = nil;
    _previewItemController = nil;
}

#if USE(UICONTEXTMENU)

static bool needsDeprecatedPreviewAPI(id<WKUIDelegate> delegate)
{
    // FIXME: Replace these with booleans in UIDelegate.h.
    // Note that we explicitly do not test for @selector(_webView:contextMenuDidEndForElement:)
    // and @selector(webView:contextMenuWillPresentForElement) since the methods are used by MobileSafari
    // to manage state despite the app not moving to the new API.

    return delegate
    && ![delegate respondsToSelector:@selector(_webView:contextMenuConfigurationForElement:completionHandler:)]
    && ![delegate respondsToSelector:@selector(webView:contextMenuConfigurationForElement:completionHandler:)]
    && ![delegate respondsToSelector:@selector(_webView:contextMenuForElement:willCommitWithAnimator:)]
    && ![delegate respondsToSelector:@selector(webView:contextMenuForElement:willCommitWithAnimator:)]
    && ![delegate respondsToSelector:@selector(_webView:contextMenuWillPresentForElement:)]
    && ![delegate respondsToSelector:@selector(webView:contextMenuDidEndForElement:)]
    && ([delegate respondsToSelector:@selector(webView:shouldPreviewElement:)]
        || [delegate respondsToSelector:@selector(webView:previewingViewControllerForElement:defaultActions:)]
        || [delegate respondsToSelector:@selector(webView:commitPreviewingViewController:)]
        || [delegate respondsToSelector:@selector(_webView:previewViewControllerForURL:defaultActions:elementInfo:)]
        || [delegate respondsToSelector:@selector(_webView:previewViewControllerForURL:)]
        || [delegate respondsToSelector:@selector(_webView:previewViewControllerForImage:alternateURL:defaultActions:elementInfo:)]);
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
static NSArray<WKPreviewAction *> *wkLegacyPreviewActionsFromElementActions(NSArray<_WKElementAction *> *elementActions, _WKActivatedElementInfo *elementInfo)
{
    NSMutableArray<WKPreviewAction *> *previewActions = [NSMutableArray arrayWithCapacity:[elementActions count]];
    for (_WKElementAction *elementAction in elementActions) {
        WKPreviewAction *previewAction = [WKPreviewAction actionWithIdentifier:previewIdentifierForElementAction(elementAction) title:elementAction.title style:UIPreviewActionStyleDefault handler:^(UIPreviewAction *, UIViewController *) {
            [elementAction runActionWithElementInfo:elementInfo];
        }];
        previewAction.image = [_WKElementAction imageForElementActionType:elementAction.type];
        [previewActions addObject:previewAction];
    }
    return previewActions;
}

static UIAction *uiActionForLegacyPreviewAction(UIPreviewAction *previewAction, UIViewController *previewViewController)
{
    // UIPreviewActionItem.image is SPI, so no external clients will be able
    // to provide glyphs for actions <rdar://problem/50151855>.
    // However, they should migrate to the new API.

    return [UIAction actionWithTitle:previewAction.title image:previewAction.image identifier:nil handler:^(UIAction *action) {
        previewAction.handler(previewAction, previewViewController);
    }];
}
ALLOW_DEPRECATED_DECLARATIONS_END

static NSArray<UIMenuElement *> *menuElementsFromLegacyPreview(UIViewController *previewViewController)
{
    if (!previewViewController)
        return nil;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSArray<id<UIPreviewActionItem>> *previewActions = previewViewController.previewActionItems;
    if (!previewActions || ![previewActions count])
        return nil;

    auto actions = [NSMutableArray arrayWithCapacity:previewActions.count];

    for (UIPreviewAction *previewAction in previewActions)
        [actions addObject:uiActionForLegacyPreviewAction(previewAction, previewViewController)];
    ALLOW_DEPRECATED_DECLARATIONS_END

    return actions;
}

static NSArray<UIMenuElement *> *menuElementsFromDefaultActions(const RetainPtr<NSArray>& defaultElementActions, RetainPtr<_WKActivatedElementInfo> elementInfo)
{
    if (!defaultElementActions || !defaultElementActions.get().count)
        return nil;

    auto actions = [NSMutableArray arrayWithCapacity:defaultElementActions.get().count];

    for (_WKElementAction *elementAction in defaultElementActions.get())
        [actions addObject:[elementAction uiActionForElementInfo:elementInfo.get()]];

    return actions;
}

static UIMenu *menuFromLegacyPreviewOrDefaultActions(UIViewController *previewViewController, const RetainPtr<NSArray>& defaultElementActions, RetainPtr<_WKActivatedElementInfo> elementInfo, NSString *title = nil)
{
    auto actions = menuElementsFromLegacyPreview(previewViewController);
    if (!actions)
        actions = menuElementsFromDefaultActions(defaultElementActions, elementInfo);

    return [UIMenu menuWithTitle:title children:actions];
}

- (void)assignLegacyDataForContextMenuInteraction
{
    ASSERT(!_contextMenuHasRequestedLegacyData);
    if (_contextMenuHasRequestedLegacyData)
        return;
    _contextMenuHasRequestedLegacyData = YES;

    if (!_webView)
        return;
    auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>(self.webView.UIDelegate);
    if (!uiDelegate)
        return;

    const auto& url = _positionInformation.url;

    _page->startInteractionWithPositionInformation(_positionInformation);

    UIViewController *previewViewController = nil;

    auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithInteractionInformationAtPosition:_positionInformation userInfo:nil]);

    if (_positionInformation.isLink) {
        _longPressCanClick = NO;

        RetainPtr<NSArray<_WKElementAction *>> defaultActionsFromAssistant = [_actionSheetAssistant defaultActionsForLinkSheet:elementInfo.get()];

        // FIXME: Animated images go here.

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([uiDelegate respondsToSelector:@selector(webView:previewingViewControllerForElement:defaultActions:)]) {
            auto defaultActions = wkLegacyPreviewActionsFromElementActions(defaultActionsFromAssistant.get(), elementInfo.get());
            auto previewElementInfo = adoptNS([[WKPreviewElementInfo alloc] _initWithLinkURL:url]);
            // FIXME: Clients using this legacy API will always show their previewViewController and ignore _showLinkPreviews.
            previewViewController = [uiDelegate webView:self.webView previewingViewControllerForElement:previewElementInfo.get() defaultActions:defaultActions];
        } else if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForURL:defaultActions:elementInfo:)])
            previewViewController = [uiDelegate _webView:self.webView previewViewControllerForURL:url defaultActions:defaultActionsFromAssistant.get() elementInfo:elementInfo.get()];
        else if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForURL:)])
            previewViewController = [uiDelegate _webView:self.webView previewViewControllerForURL:url];
        ALLOW_DEPRECATED_DECLARATIONS_END

        // Previously, UIPreviewItemController would detect the case where there was no previewViewController
        // and create one. We need to replicate this code for the new API.
        if (!previewViewController || [(NSURL *)url iTunesStoreURL]) {
            auto ddContextMenuActionClass = getDDContextMenuActionClass();
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            NSDictionary *context = [self dataDetectionContextForPositionInformation:_positionInformation];
            RetainPtr<UIContextMenuConfiguration> dataDetectorsResult = [ddContextMenuActionClass contextMenuConfigurationForURL:url identifier:_positionInformation.dataDetectorIdentifier selectedText:self.selectedText results:_positionInformation.dataDetectorResults.get() inView:self context:context menuIdentifier:nil];
            if (_showLinkPreviews && dataDetectorsResult && dataDetectorsResult.get().previewProvider)
                _contextMenuLegacyPreviewController = dataDetectorsResult.get().previewProvider();
            if (dataDetectorsResult && dataDetectorsResult.get().actionProvider) {
                auto menuElements = menuElementsFromDefaultActions(defaultActionsFromAssistant, elementInfo);
                _contextMenuLegacyMenu = dataDetectorsResult.get().actionProvider(menuElements);
            }
            END_BLOCK_OBJC_EXCEPTIONS;
            return;
        }

        _contextMenuLegacyMenu = menuFromLegacyPreviewOrDefaultActions(previewViewController, defaultActionsFromAssistant, elementInfo);

    } else if (_positionInformation.isImage) {
        NSURL *nsURL = (NSURL *)url;
        RetainPtr<NSDictionary> imageInfo;
        ASSERT(_positionInformation.image);
        auto cgImage = _positionInformation.image->makeCGImageCopy();
        auto uiImage = adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);

        if ([uiDelegate respondsToSelector:@selector(_webView:alternateURLFromImage:userInfo:)]) {
            NSDictionary *userInfo;
            nsURL = [uiDelegate _webView:self.webView alternateURLFromImage:uiImage.get() userInfo:&userInfo];
            imageInfo = userInfo;
        }

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([uiDelegate respondsToSelector:@selector(_webView:willPreviewImageWithURL:)])
            [uiDelegate _webView:self.webView willPreviewImageWithURL:_positionInformation.imageURL];

        RetainPtr<NSArray<_WKElementAction *>> defaultActionsFromAssistant = [_actionSheetAssistant defaultActionsForImageSheet:elementInfo.get()];

        if (imageInfo && [uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForImage:alternateURL:defaultActions:elementInfo:)])
            previewViewController = [uiDelegate _webView:self.webView previewViewControllerForImage:uiImage.get() alternateURL:nsURL defaultActions:defaultActionsFromAssistant.get() elementInfo:elementInfo.get()];
        else
            previewViewController = [[WKImagePreviewViewController alloc] initWithCGImage:cgImage defaultActions:defaultActionsFromAssistant.get() elementInfo:elementInfo.get()];
        ALLOW_DEPRECATED_DECLARATIONS_END

        _contextMenuLegacyMenu = menuFromLegacyPreviewOrDefaultActions(previewViewController, defaultActionsFromAssistant, elementInfo, _positionInformation.title);
    }

    _contextMenuLegacyPreviewController = previewViewController;
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
    // Required to conform to UIContextMenuInteractionDelegate, but SPI version should be called instead.
    ASSERT_NOT_REACHED();
    return nil;
}

- (void)_contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location completion:(void(^)(UIContextMenuConfiguration *))completion
{
    if (!_webView)
        return completion(nil);

    if (!self.webView.configuration._longPressActionsEnabled)
        return completion(nil);

    [_webView _didShowContextMenu];
    
    _showLinkPreviews = true;
    if (NSNumber *value = [[NSUserDefaults standardUserDefaults] objectForKey:webkitShowLinkPreviewsPreferenceKey])
        _showLinkPreviews = value.boolValue;

    const auto position = [interaction locationInView:self];
    WebKit::InteractionInformationRequest request { WebCore::roundedIntPoint(position) };
    request.includeSnapshot = true;
    request.includeLinkIndicator = true;
    request.linkIndicatorShouldHaveLegacyMargins = !self._shouldUseContextMenus;

    [self doAfterPositionInformationUpdate:[weakSelf = WeakObjCPtr<WKContentView>(self), completion = makeBlockPtr(completion)] (WebKit::InteractionInformationAtPosition) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return completion(nil);
        [strongSelf continueContextMenuInteraction:completion.get()];
    } forRequest:request];
}

- (void)continueContextMenuInteraction:(void(^)(UIContextMenuConfiguration *))continueWithContextMenuConfiguration
{
    if (!self.window)
        return continueWithContextMenuConfiguration(nil);

    if (!_positionInformation.touchCalloutEnabled)
        return continueWithContextMenuConfiguration(nil);

    if (!_positionInformation.isLink && !_positionInformation.isImage && !_positionInformation.isAttachment)
        return continueWithContextMenuConfiguration(nil);

    URL linkURL = _positionInformation.url;

    if (_positionInformation.isLink && linkURL.isEmpty())
        return continueWithContextMenuConfiguration(nil);

    auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>(self.webView.UIDelegate);

    if (needsDeprecatedPreviewAPI(uiDelegate)) {
        if (_positionInformation.isLink) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if ([uiDelegate respondsToSelector:@selector(webView:shouldPreviewElement:)]) {
                auto previewElementInfo = adoptNS([[WKPreviewElementInfo alloc] _initWithLinkURL:linkURL]);
                if (![uiDelegate webView:self.webView shouldPreviewElement:previewElementInfo.get()])
                    return continueWithContextMenuConfiguration(nil);
            }
            ALLOW_DEPRECATED_DECLARATIONS_END

            // FIXME: Support JavaScript urls here. But make sure they don't show a preview.
            // <rdar://problem/50572283>
            if (!linkURL.protocolIsInHTTPFamily()) {
#if ENABLE(DATA_DETECTION)
                if (!WebCore::DataDetection::canBePresentedByDataDetectors(linkURL))
                    return continueWithContextMenuConfiguration(nil);
#endif
            }
        }

        _contextMenuLegacyPreviewController = nullptr;
        _contextMenuLegacyMenu = nullptr;
        _contextMenuHasRequestedLegacyData = NO;
        _contextMenuActionProviderDelegateNeedsOverride = NO;

        UIContextMenuActionProvider actionMenuProvider = [weakSelf = WeakObjCPtr<WKContentView>(self)] (NSArray<UIMenuElement *> *) -> UIMenu * {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return nil;
            if (!strongSelf->_contextMenuHasRequestedLegacyData)
                [strongSelf assignLegacyDataForContextMenuInteraction];

            return strongSelf->_contextMenuLegacyMenu.get();
        };

        UIContextMenuContentPreviewProvider contentPreviewProvider = [weakSelf = WeakObjCPtr<WKContentView>(self)] () -> UIViewController * {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return nil;
            if (!strongSelf->_contextMenuHasRequestedLegacyData)
                [strongSelf assignLegacyDataForContextMenuInteraction];

            return strongSelf->_contextMenuLegacyPreviewController.get();
        };

        _page->startInteractionWithPositionInformation(_positionInformation);

        continueWithContextMenuConfiguration([UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:contentPreviewProvider actionProvider:actionMenuProvider]);
        return;
    }

#if ENABLE(DATA_DETECTION)
    if ([(NSURL *)linkURL iTunesStoreURL]) {
        [self continueContextMenuInteractionWithDataDetectors:continueWithContextMenuConfiguration];
        return;
    }
#endif

    auto completionBlock = makeBlockPtr([continueWithContextMenuConfiguration = makeBlockPtr(continueWithContextMenuConfiguration), linkURL = WTFMove(linkURL), weakSelf = WeakObjCPtr<WKContentView>(self)] (UIContextMenuConfiguration *configurationFromWKUIDelegate) mutable {

        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            continueWithContextMenuConfiguration(nil);
            return;
        }

        if (configurationFromWKUIDelegate) {
            strongSelf->_page->startInteractionWithPositionInformation(strongSelf->_positionInformation);
            strongSelf->_contextMenuActionProviderDelegateNeedsOverride = YES;
            continueWithContextMenuConfiguration(configurationFromWKUIDelegate);
            return;
        }

        if (strongSelf->_positionInformation.isImage && !strongSelf->_positionInformation.isLink) {
            ASSERT(strongSelf->_positionInformation.image);
            auto cgImage = strongSelf->_positionInformation.image->makeCGImageCopy();

            strongSelf->_contextMenuActionProviderDelegateNeedsOverride = NO;

            auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithInteractionInformationAtPosition:strongSelf->_positionInformation userInfo:nil]);

            UIContextMenuActionProvider actionMenuProvider = [weakSelf, elementInfo] (NSArray<UIMenuElement *> *) -> UIMenu * {
                auto strongSelf = weakSelf.get();
                if (!strongSelf)
                    return nil;

                RetainPtr<NSArray<_WKElementAction *>> defaultActionsFromAssistant = [strongSelf->_actionSheetAssistant defaultActionsForImageSheet:elementInfo.get()];
                auto actions = menuElementsFromDefaultActions(defaultActionsFromAssistant, elementInfo);
                return [UIMenu menuWithTitle:strongSelf->_positionInformation.title children:actions];
            };

            UIContextMenuContentPreviewProvider contentPreviewProvider = [weakSelf, cgImage, elementInfo] () -> UIViewController * {
                auto strongSelf = weakSelf.get();
                if (!strongSelf)
                    return nil;

                return [[[WKImagePreviewViewController alloc] initWithCGImage:cgImage defaultActions:nil elementInfo:elementInfo.get()] autorelease];
            };

            continueWithContextMenuConfiguration([UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:contentPreviewProvider actionProvider:actionMenuProvider]);
            return;
        }

        // At this point we have an object we might want to show a context menu for, but the
        // client was unable to handle it. Before giving up, we ask DataDetectors.

        strongSelf->_contextMenuElementInfo = nil;

#if ENABLE(DATA_DETECTION)
        // FIXME: Support JavaScript urls here. But make sure they don't show a preview.
        // <rdar://problem/50572283>
        if (!linkURL.protocolIsInHTTPFamily() && !WebCore::DataDetection::canBePresentedByDataDetectors(linkURL)) {
            continueWithContextMenuConfiguration(nil);
            return;
        }

        [strongSelf continueContextMenuInteractionWithDataDetectors:continueWithContextMenuConfiguration.get()];
        return;
#else
        continueWithContextMenuConfiguration(nil);
#endif
    });

    _contextMenuActionProviderDelegateNeedsOverride = NO;
    _contextMenuElementInfo = wrapper(API::ContextMenuElementInfo::create(_positionInformation, nil));

    if (_positionInformation.isImage && _positionInformation.url.isNull() && [uiDelegate respondsToSelector:@selector(_webView:alternateURLFromImage:userInfo:)]) {
        UIImage *uiImage = [[_contextMenuElementInfo _activatedElementInfo] image];
        NSDictionary *userInfo = nil;
        NSURL *nsURL = [uiDelegate _webView:self.webView alternateURLFromImage:uiImage userInfo:&userInfo];
        _positionInformation.url = nsURL;
        _contextMenuElementInfo = wrapper(API::ContextMenuElementInfo::create(_positionInformation, userInfo));
    }

    if (_positionInformation.isLink && [uiDelegate respondsToSelector:@selector(webView:contextMenuConfigurationForElement:completionHandler:)]) {
        auto checker = WebKit::CompletionHandlerCallChecker::create(uiDelegate, @selector(webView:contextMenuConfigurationForElement:completionHandler:));
        [uiDelegate webView:self.webView contextMenuConfigurationForElement:_contextMenuElementInfo.get() completionHandler:makeBlockPtr([completionBlock = WTFMove(completionBlock), checker = WTFMove(checker)] (UIContextMenuConfiguration *configuration) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionBlock(configuration);
        }).get()];
    } else if ([uiDelegate respondsToSelector:@selector(_webView:contextMenuConfigurationForElement:completionHandler:)]) {
        auto checker = WebKit::CompletionHandlerCallChecker::create(uiDelegate, @selector(_webView:contextMenuConfigurationForElement:completionHandler:));
        [uiDelegate _webView:self.webView contextMenuConfigurationForElement:_contextMenuElementInfo.get() completionHandler:makeBlockPtr([completionBlock = WTFMove(completionBlock), checker = WTFMove(checker)] (UIContextMenuConfiguration *configuration) {
            if (checker->completionHandlerHasBeenCalled())
                return;
            checker->didCallCompletionHandler();
            completionBlock(configuration);
        }).get()];
    } else
        completionBlock(nil);
}

#if ENABLE(DATA_DETECTION)
- (void)continueContextMenuInteractionWithDataDetectors:(void(^)(UIContextMenuConfiguration *))continueWithContextMenuConfiguration
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    auto ddContextMenuActionClass = getDDContextMenuActionClass();
    URL linkURL = _positionInformation.url;
    NSDictionary *context = [self dataDetectionContextForPositionInformation:_positionInformation];
    UIContextMenuConfiguration *configurationFromDD = [ddContextMenuActionClass contextMenuConfigurationForURL:linkURL identifier:_positionInformation.dataDetectorIdentifier selectedText:[self selectedText] results:_positionInformation.dataDetectorResults.get() inView:self context:context menuIdentifier:nil];
    _contextMenuActionProviderDelegateNeedsOverride = YES;
    _page->startInteractionWithPositionInformation(_positionInformation);
    continueWithContextMenuConfiguration(configurationFromDD);
    END_BLOCK_OBJC_EXCEPTIONS;
}
#endif

- (NSArray<UIMenuElement *> *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction overrideSuggestedActionsForConfiguration:(UIContextMenuConfiguration *)configuration
{
    // If we're here we're in the legacy path, which ignores the suggested actions anyway.
    if (!_contextMenuActionProviderDelegateNeedsOverride)
        return nil;

    return [_actionSheetAssistant suggestedActionsForContextMenuWithPositionInformation:_positionInformation];
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    [self _startSuppressingSelectionAssistantForReason:WebKit::InteractionIsHappening];
    [self _cancelTouchEventGestureRecognizer];
    return [self _createTargetedContextMenuHintPreviewIfPossible];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
{
    if (!_webView)
        return;
    auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>(self.webView.UIDelegate);
    if (!uiDelegate)
        return;
    if ([uiDelegate respondsToSelector:@selector(webView:contextMenuWillPresentForElement:)])
        [uiDelegate webView:self.webView contextMenuWillPresentForElement:_contextMenuElementInfo.get()];
    else if ([uiDelegate respondsToSelector:@selector(_webView:contextMenuWillPresentForElement:)]) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [uiDelegate _webView:self.webView contextMenuWillPresentForElement:_contextMenuElementInfo.get()];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForDismissingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    return std::exchange(_contextMenuInteractionTargetedPreview, nil).autorelease();
}

- (nullable _UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
#if defined(DD_CONTEXT_MENU_SPI_VERSION) && DD_CONTEXT_MENU_SPI_VERSION >= 2
    if ([configuration isKindOfClass:getDDContextMenuConfigurationClass()]) {
        DDContextMenuConfiguration *ddConfiguration = static_cast<DDContextMenuConfiguration *>(configuration);

        if (ddConfiguration.prefersActionMenuStyle) {
            _UIContextMenuStyle *style = [_UIContextMenuStyle defaultStyle];
            style.preferredLayout = _UIContextMenuLayoutActionsOnly;
            return style;
        }
    }
#endif

    return nil;
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willPerformPreviewActionForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionCommitAnimating>)animator
{
    if (!_webView)
        return;

    [self _stopSuppressingSelectionAssistantForReason:WebKit::InteractionIsHappening];

    auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>(self.webView.UIDelegate);
    if (!uiDelegate)
        return;

    if (needsDeprecatedPreviewAPI(uiDelegate)) {

        if (_positionInformation.isImage) {
            if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewedImageWithURL:)]) {
                const auto& imageURL = _positionInformation.imageURL;
                if (imageURL.isEmpty() || !(imageURL.protocolIsInHTTPFamily() || imageURL.protocolIs("data")))
                    return;
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                [uiDelegate _webView:self.webView commitPreviewedImageWithURL:(NSURL *)imageURL];
                ALLOW_DEPRECATED_DECLARATIONS_END
            }
            return;
        }

        if ([uiDelegate respondsToSelector:@selector(webView:commitPreviewingViewController:)]) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto viewController = _contextMenuLegacyPreviewController.get())
                [uiDelegate webView:self.webView commitPreviewingViewController:viewController];
            ALLOW_DEPRECATED_DECLARATIONS_END
            return;
        }

        if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewedViewController:)]) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto viewController = _contextMenuLegacyPreviewController.get())
                [uiDelegate _webView:self.webView commitPreviewedViewController:viewController];
            ALLOW_DEPRECATED_DECLARATIONS_END
            return;
        }

        return;
    }

    if ([uiDelegate respondsToSelector:@selector(webView:contextMenuForElement:willCommitWithAnimator:)])
        [uiDelegate webView:self.webView contextMenuForElement:_contextMenuElementInfo.get() willCommitWithAnimator:animator];
    else if ([uiDelegate respondsToSelector:@selector(_webView:contextMenuForElement:willCommitWithAnimator:)]) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [uiDelegate _webView:self.webView contextMenuForElement:_contextMenuElementInfo.get() willCommitWithAnimator:animator];
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

#if defined(DD_CONTEXT_MENU_SPI_VERSION) && DD_CONTEXT_MENU_SPI_VERSION >= 2
    if ([configuration isKindOfClass:getDDContextMenuConfigurationClass()]) {
        DDContextMenuConfiguration *ddConfiguration = static_cast<DDContextMenuConfiguration *>(configuration);

        BOOL shouldExpandPreview = NO;
        RetainPtr<UIViewController> presentedViewController;

#if defined(DD_CONTEXT_MENU_SPI_VERSION) && DD_CONTEXT_MENU_SPI_VERSION >= 3
        shouldExpandPreview = !!ddConfiguration.interactionViewControllerProvider;
        if (shouldExpandPreview)
            presentedViewController = ddConfiguration.interactionViewControllerProvider();
#else
        shouldExpandPreview = ddConfiguration.expandPreviewOnInteraction;
        presentedViewController = animator.previewViewController;
#endif

        if (shouldExpandPreview) {
            animator.preferredCommitStyle = UIContextMenuInteractionCommitStylePop;

            // We will re-present modally on the same VC that is currently presenting the preview in a context menu.
            RetainPtr<UIViewController> presentingViewController = animator.previewViewController.presentingViewController;

            [animator addAnimations:^{
                [presentingViewController presentViewController:presentedViewController.get() animated:NO completion:nil];
            }];
            return;
        }

        if (NSURL *interactionURL = ddConfiguration.interactionURL) {
            animator.preferredCommitStyle = UIContextMenuInteractionCommitStylePop;

            [animator addAnimations:^{
                [[UIApplication sharedApplication] openURL:interactionURL withCompletionHandler:nil];
            }];
            return;
        }
    }
#endif
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(nullable id<UIContextMenuInteractionAnimating>)animator
{
    if (!_webView)
        return;

    [self _stopSuppressingSelectionAssistantForReason:WebKit::InteractionIsHappening];

    // FIXME: This delegate is being called more than once by UIKit. <rdar://problem/51550291>
    // This conditional avoids the WKUIDelegate being called twice too.
    if (_contextMenuElementInfo) {
        auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>(self.webView.UIDelegate);
        if ([uiDelegate respondsToSelector:@selector(webView:contextMenuDidEndForElement:)])
            [uiDelegate webView:self.webView contextMenuDidEndForElement:_contextMenuElementInfo.get()];
        else if ([uiDelegate respondsToSelector:@selector(_webView:contextMenuDidEndForElement:)]) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [uiDelegate _webView:self.webView contextMenuDidEndForElement:_contextMenuElementInfo.get()];
            ALLOW_DEPRECATED_DECLARATIONS_END
        }
    }

    _page->stopInteraction();

    _contextMenuLegacyPreviewController = nullptr;
    _contextMenuLegacyMenu = nullptr;
    _contextMenuHasRequestedLegacyData = NO;
    _contextMenuElementInfo = nullptr;

    [animator addCompletion:[weakSelf = WeakObjCPtr<WKContentView>(self)] () {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;
        [strongSelf _removeContextMenuViewIfPossible];
    }];
}

#endif // USE(UICONTEXTMENU)

- (BOOL)_interactionShouldBeginFromPreviewItemController:(UIPreviewItemController *)controller forPosition:(CGPoint)position
{
    if (!_longPressCanClick)
        return NO;

    WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(position));
    request.includeSnapshot = true;
    request.includeLinkIndicator = true;
    request.linkIndicatorShouldHaveLegacyMargins = !self._shouldUseContextMenus;
    if (![self ensurePositionInformationIsUpToDate:request])
        return NO;
    if (!_positionInformation.isLink && !_positionInformation.isImage && !_positionInformation.isAttachment)
        return NO;

    const URL& linkURL = _positionInformation.url;
    if (_positionInformation.isLink) {
        id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([uiDelegate respondsToSelector:@selector(webView:shouldPreviewElement:)]) {
            auto previewElementInfo = adoptNS([[WKPreviewElementInfo alloc] _initWithLinkURL:(NSURL *)linkURL]);
            return [uiDelegate webView:self.webView shouldPreviewElement:previewElementInfo.get()];
        }
        ALLOW_DEPRECATED_DECLARATIONS_END
        if (linkURL.isEmpty())
            return NO;
        if (linkURL.protocolIsInHTTPFamily())
            return YES;
#if ENABLE(DATA_DETECTION)
        if (WebCore::DataDetection::canBePresentedByDataDetectors(linkURL))
            return YES;
#endif
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
    BOOL respondsToAttachmentListForWebViewSourceIsManaged = [uiDelegate respondsToSelector:@selector(_attachmentListForWebView:sourceIsManaged:)];
    BOOL supportsAttachmentPreview = ([uiDelegate respondsToSelector:@selector(_attachmentListForWebView:)] || respondsToAttachmentListForWebViewSourceIsManaged)
        && [uiDelegate respondsToSelector:@selector(_webView:indexIntoAttachmentListForElement:)];
    BOOL canShowAttachmentPreview = (_positionInformation.isAttachment || _positionInformation.isImage) && supportsAttachmentPreview;
    BOOL isDataDetectorLink = NO;
#if ENABLE(DATA_DETECTION)
    isDataDetectorLink = _positionInformation.isDataDetectorLink;
#endif

    if (canShowImagePreview && _positionInformation.isAnimatedImage) {
        canShowImagePreview = NO;
        canShowLinkPreview = YES;
        useImageURLForLink = YES;
    }

    if (!canShowLinkPreview && !canShowImagePreview && !canShowAttachmentPreview)
        return nil;

    const URL& linkURL = _positionInformation.url;
    if (!useImageURLForLink && (linkURL.isEmpty() || (!linkURL.protocolIsInHTTPFamily() && !isDataDetectorLink))) {
        if (canShowLinkPreview && !canShowImagePreview)
            return nil;
        canShowLinkPreview = NO;
    }

    NSMutableDictionary *dataForPreview = [[[NSMutableDictionary alloc] init] autorelease];
    if (canShowLinkPreview) {
        *type = UIPreviewItemTypeLink;
        if (useImageURLForLink)
            dataForPreview[UIPreviewDataLink] = (NSURL *)_positionInformation.imageURL;
        else
            dataForPreview[UIPreviewDataLink] = (NSURL *)linkURL;
#if ENABLE(DATA_DETECTION)
        if (isDataDetectorLink) {
            NSDictionary *context = nil;
            if ([uiDelegate respondsToSelector:@selector(_dataDetectionContextForWebView:)])
                context = [uiDelegate _dataDetectionContextForWebView:self.webView];

            DDDetectionController *controller = [getDDDetectionControllerClass() sharedController];
            NSDictionary *newContext = nil;
            RetainPtr<NSMutableDictionary> extendedContext;
            DDResultRef ddResult = [controller resultForURL:dataForPreview[UIPreviewDataLink] identifier:_positionInformation.dataDetectorIdentifier selectedText:[self selectedText] results:_positionInformation.dataDetectorResults.get() context:context extendedContext:&newContext];
            if (ddResult)
                dataForPreview[UIPreviewDataDDResult] = (__bridge id)ddResult;
            if (!_positionInformation.textBefore.isEmpty() || !_positionInformation.textAfter.isEmpty()) {
                extendedContext = adoptNS([@{
                    getkDataDetectorsLeadingText() : _positionInformation.textBefore,
                    getkDataDetectorsTrailingText() : _positionInformation.textAfter,
                } mutableCopy]);
                
                if (newContext)
                    [extendedContext addEntriesFromDictionary:newContext];
                newContext = extendedContext.get();
            }
            if (newContext)
                dataForPreview[UIPreviewDataDDContext] = newContext;
        }
#endif // ENABLE(DATA_DETECTION)
    } else if (canShowImagePreview) {
        *type = UIPreviewItemTypeImage;
        dataForPreview[UIPreviewDataLink] = (NSURL *)_positionInformation.imageURL;
    } else if (canShowAttachmentPreview) {
        *type = UIPreviewItemTypeAttachment;
        auto element = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeAttachment URL:(NSURL *)linkURL imageURL:(NSURL *)_positionInformation.imageURL location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:nil]);
        NSUInteger index = [uiDelegate _webView:self.webView indexIntoAttachmentListForElement:element.get()];
        if (index != NSNotFound) {
            BOOL sourceIsManaged = NO;
            if (respondsToAttachmentListForWebViewSourceIsManaged)
                dataForPreview[UIPreviewDataAttachmentList] = [uiDelegate _attachmentListForWebView:self.webView sourceIsManaged:&sourceIsManaged];
            else
                dataForPreview[UIPreviewDataAttachmentList] = [uiDelegate _attachmentListForWebView:self.webView];
            dataForPreview[UIPreviewDataAttachmentIndex] = [NSNumber numberWithUnsignedInteger:index];
            dataForPreview[UIPreviewDataAttachmentListIsContentManaged] = [NSNumber numberWithBool:sourceIsManaged];
        }
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
    
    [_webView _didShowContextMenu];

    NSURL *targetURL = controller.previewData[UIPreviewDataLink];
    URL coreTargetURL = targetURL;
    bool isValidURLForImagePreview = !coreTargetURL.isEmpty() && (coreTargetURL.protocolIsInHTTPFamily() || coreTargetURL.protocolIs("data"));

    if ([_previewItemController type] == UIPreviewItemTypeLink) {
        _longPressCanClick = NO;
        _page->startInteractionWithPositionInformation(_positionInformation);

        // Treat animated images like a link preview
        if (isValidURLForImagePreview && _positionInformation.isAnimatedImage) {
            RetainPtr<_WKActivatedElementInfo> animatedImageElementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:targetURL imageURL:nil location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:_positionInformation.image.get()]);

            if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForAnimatedImageAtURL:defaultActions:elementInfo:imageSize:)]) {
                RetainPtr<NSArray> actions = [_actionSheetAssistant defaultActionsForImageSheet:animatedImageElementInfo.get()];
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                return [uiDelegate _webView:self.webView previewViewControllerForAnimatedImageAtURL:targetURL defaultActions:actions.get() elementInfo:animatedImageElementInfo.get() imageSize:_positionInformation.image->size()];
                ALLOW_DEPRECATED_DECLARATIONS_END
            }
        }

        RetainPtr<_WKActivatedElementInfo> elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeLink URL:targetURL imageURL:nil location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:_positionInformation.image.get()]);

        auto actions = [_actionSheetAssistant defaultActionsForLinkSheet:elementInfo.get()];
        if ([uiDelegate respondsToSelector:@selector(webView:previewingViewControllerForElement:defaultActions:)]) {
            auto previewActions = adoptNS([[NSMutableArray alloc] init]);
            for (_WKElementAction *elementAction in actions.get()) {
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                WKPreviewAction *previewAction = [WKPreviewAction actionWithIdentifier:previewIdentifierForElementAction(elementAction) title:[elementAction title] style:UIPreviewActionStyleDefault handler:^(UIPreviewAction *, UIViewController *) {
                    [elementAction runActionWithElementInfo:elementInfo.get()];
                }];
                ALLOW_DEPRECATED_DECLARATIONS_END
                [previewActions addObject:previewAction];
            }
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto previewElementInfo = adoptNS([[WKPreviewElementInfo alloc] _initWithLinkURL:targetURL]);
            if (UIViewController *controller = [uiDelegate webView:self.webView previewingViewControllerForElement:previewElementInfo.get() defaultActions:previewActions.get()])
                return controller;
            ALLOW_DEPRECATED_DECLARATIONS_END
        }

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForURL:defaultActions:elementInfo:)])
            return [uiDelegate _webView:self.webView previewViewControllerForURL:targetURL defaultActions:actions.get() elementInfo:elementInfo.get()];

        if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForURL:)])
            return [uiDelegate _webView:self.webView previewViewControllerForURL:targetURL];
        ALLOW_DEPRECATED_DECLARATIONS_END

        return nil;
    }

    if ([_previewItemController type] == UIPreviewItemTypeImage) {
        if (!isValidURLForImagePreview)
            return nil;

        RetainPtr<NSURL> alternateURL = targetURL;
        RetainPtr<NSDictionary> imageInfo;
        RetainPtr<CGImageRef> cgImage = _positionInformation.image->makeCGImageCopy();
        RetainPtr<UIImage> uiImage = adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);
        if ([uiDelegate respondsToSelector:@selector(_webView:alternateURLFromImage:userInfo:)]) {
            NSDictionary *userInfo;
            alternateURL = [uiDelegate _webView:self.webView alternateURLFromImage:uiImage.get() userInfo:&userInfo];
            imageInfo = userInfo;
        }

        RetainPtr<_WKActivatedElementInfo> elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:alternateURL.get() imageURL:nil location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:_positionInformation.image.get() userInfo:imageInfo.get()]);
        _page->startInteractionWithPositionInformation(_positionInformation);

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if ([uiDelegate respondsToSelector:@selector(_webView:willPreviewImageWithURL:)])
            [uiDelegate _webView:self.webView willPreviewImageWithURL:targetURL];
        ALLOW_DEPRECATED_DECLARATIONS_END

        auto defaultActions = [_actionSheetAssistant defaultActionsForImageSheet:elementInfo.get()];
        if (imageInfo && [uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForImage:alternateURL:defaultActions:elementInfo:)]) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            UIViewController *previewViewController = [uiDelegate _webView:self.webView previewViewControllerForImage:uiImage.get() alternateURL:alternateURL.get() defaultActions:defaultActions.get() elementInfo:elementInfo.get()];
            ALLOW_DEPRECATED_DECLARATIONS_END
            if (previewViewController)
                return previewViewController;
        }

        return [[[WKImagePreviewViewController alloc] initWithCGImage:cgImage defaultActions:defaultActions elementInfo:elementInfo] autorelease];
    }

    return nil;
}

- (void)_previewItemController:(UIPreviewItemController *)controller commitPreview:(UIViewController *)viewController
{
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([_previewItemController type] == UIPreviewItemTypeImage) {
        if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewedImageWithURL:)]) {
            const URL& imageURL = _positionInformation.imageURL;
            if (imageURL.isEmpty() || !(imageURL.protocolIsInHTTPFamily() || imageURL.protocolIs("data")))
                return;
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [uiDelegate _webView:self.webView commitPreviewedImageWithURL:(NSURL *)imageURL];
            ALLOW_DEPRECATED_DECLARATIONS_END
            return;
        }
        return;
    }

    if ([uiDelegate respondsToSelector:@selector(webView:commitPreviewingViewController:)]) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [uiDelegate webView:self.webView commitPreviewingViewController:viewController];
        ALLOW_DEPRECATED_DECLARATIONS_END
        return;
    }

    if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewedViewController:)]) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [uiDelegate _webView:self.webView commitPreviewedViewController:viewController];
        ALLOW_DEPRECATED_DECLARATIONS_END
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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if ([uiDelegate respondsToSelector:@selector(_webView:didDismissPreviewViewController:committing:)])
        [uiDelegate _webView:self.webView didDismissPreviewViewController:viewController committing:committing];
    else if ([uiDelegate respondsToSelector:@selector(_webView:didDismissPreviewViewController:)])
        [uiDelegate _webView:self.webView didDismissPreviewViewController:viewController];
    ALLOW_DEPRECATED_DECLARATIONS_END

    [_webView _didDismissContextMenu];
}

- (UIImage *)_presentationSnapshotForPreviewItemController:(UIPreviewItemController *)controller
{
    if (!_positionInformation.linkIndicator.contentImage)
        return nullptr;
    return [[[UIImage alloc] initWithCGImage:_positionInformation.linkIndicator.contentImage->nativeImage().get()] autorelease];
}

- (NSArray *)_presentationRectsForPreviewItemController:(UIPreviewItemController *)controller
{
    if (_positionInformation.linkIndicator.contentImage) {
        auto origin = _positionInformation.linkIndicator.textBoundingRectInRootViewCoordinates.location();
        return createNSArray(_positionInformation.linkIndicator.textRectsInBoundingRectCoordinates, [&] (CGRect rect) {
            return [NSValue valueWithCGRect:CGRectOffset(rect, origin.x(), origin.y())];
        }).autorelease();
    } else {
        float marginInPx = 4 * _page->deviceScaleFactor();
        return @[[NSValue valueWithCGRect:CGRectInset(_positionInformation.bounds, -marginInPx, -marginInPx)]];
    }
}

- (void)_previewItemControllerDidCancelPreview:(UIPreviewItemController *)controller
{
    _longPressCanClick = NO;
    
    [_webView _didDismissContextMenu];
}

@end

#endif // HAVE(LINK_PREVIEW)

// UITextRange and UITextPosition implementations for WK2
// FIXME: Move these out into separate files.

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
    [_selectionRects release];
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

@implementation WKAutocorrectionRects

+ (WKAutocorrectionRects *)autocorrectionRectsWithFirstCGRect:(CGRect)firstRect lastCGRect:(CGRect)lastRect
{
    auto rects = adoptNS([[WKAutocorrectionRects alloc] init]);
    [rects setFirstRect:firstRect];
    [rects setLastRect:lastRect];
    return rects.autorelease();
}

@end

@implementation WKAutocorrectionContext

+ (WKAutocorrectionContext *)emptyAutocorrectionContext
{
    return [self autocorrectionContextWithWebContext:WebKit::WebAutocorrectionContext { }];
}

+ (WKAutocorrectionContext *)autocorrectionContextWithWebContext:(const WebKit::WebAutocorrectionContext&)webCorrection
{
    auto correction = adoptNS([[WKAutocorrectionContext alloc] init]);
    [correction setContextBeforeSelection:nsStringNilIfEmpty(webCorrection.contextBefore)];
    [correction setSelectedText:nsStringNilIfEmpty(webCorrection.selectedText)];
    [correction setMarkedText:nsStringNilIfEmpty(webCorrection.markedText)];
    [correction setContextAfterSelection:nsStringNilIfEmpty(webCorrection.contextAfter)];
    [correction setRangeInMarkedText:webCorrection.markedTextRange];
    return correction.autorelease();
}

@end

#endif // PLATFORM(IOS_FAMILY)
