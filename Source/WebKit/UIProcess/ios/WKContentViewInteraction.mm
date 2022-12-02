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
#import "CocoaImage.h"
#import "CompletionHandlerCallChecker.h"
#import "DocumentEditingContext.h"
#import "ImageAnalysisUtilities.h"
#import "InputViewUpdateDeferrer.h"
#import "InsertTextOptions.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebTouchEvent.h"
#import "PageClient.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteLayerTreeViews.h"
#import "RemoteScrollingCoordinatorProxyIOS.h"
#import "RevealItem.h"
#import "SmartMagnificationController.h"
#import "TextChecker.h"
#import "TextInputSPI.h"
#import "TextRecognitionUpdateResult.h"
#import "UIKitSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKActionSheetAssistant.h"
#import "WKContextMenuElementInfoInternal.h"
#import "WKContextMenuElementInfoPrivate.h"
#import "WKDatePickerViewController.h"
#import "WKDateTimeInputControl.h"
#import "WKError.h"
#import "WKFocusedFormControlView.h"
#import "WKFormSelectControl.h"
#import "WKFrameInfoInternal.h"
#import "WKHighlightLongPressGestureRecognizer.h"
#import "WKImageAnalysisGestureRecognizer.h"
#import "WKImagePreviewViewController.h"
#import "WKInspectorNodeSearchGestureRecognizer.h"
#import "WKMouseGestureRecognizer.h"
#import "WKNSURLExtras.h"
#import "WKPreviewActionItemIdentifiers.h"
#import "WKPreviewActionItemInternal.h"
#import "WKPreviewElementInfoInternal.h"
#import "WKQuickboardViewControllerDelegate.h"
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
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebAutocorrectionContext.h"
#import "WebAutocorrectionData.h"
#import "WebDataListSuggestionsDropdownIOS.h"
#import "WebEvent.h"
#import "WebFoundTextRange.h"
#import "WebIOSEventFactory.h"
#import "WebPageMessages.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKDragActionsInternal.h"
#import "_WKElementAction.h"
#import "_WKElementActionInternal.h"
#import "_WKFocusedElementInfo.h"
#import "_WKInputDelegate.h"
#import "_WKTextInputContextInternal.h"
#import <CoreText/CTFont.h>
#import <CoreText/CTFontDescriptor.h>
#import <MobileCoreServices/UTCoreTypes.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <WebCore/AppHighlight.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/CompositionHighlight.h>
#import <WebCore/DOMPasteAccess.h>
#import <WebCore/DataDetection.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FloatRect.h>
#import <WebCore/FontAttributeChanges.h>
#import <WebCore/InputMode.h>
#import <WebCore/KeyEventCodesIOS.h>
#import <WebCore/KeyboardScroll.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/Path.h>
#import <WebCore/PathUtilities.h>
#import <WebCore/PromisedAttachmentInfo.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ScrollTypes.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/ShareData.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIndicatorWindow.h>
#import <WebCore/TextRecognitionResult.h>
#import <WebCore/TouchAction.h>
#import <WebCore/UTIRegistry.h>
#import <WebCore/UTIUtilities.h>
#import <WebCore/VisibleSelection.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/WebEvent.h>
#import <WebCore/WebTextIndicatorLayer.h>
#import <WebCore/WritingDirection.h>
#import <WebKit/WebSelectionRect.h> // FIXME: WebKit should not include WebKitLegacy headers!
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/DataDetectorsCoreSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/ios/BarcodeSupportSPI.h>
#import <pal/spi/ios/DataDetectorsUISPI.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <pal/spi/ios/ManagedConfigurationSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/Scope.h>
#import <wtf/SetForScope.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
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

#if HAVE(PEPPER_UI_CORE)
#import "PepperUICoreSPI.h"
#endif

#if HAVE(AVKIT)
#import <pal/spi/cocoa/AVKitSPI.h>
#endif

#import <pal/cocoa/VisionKitCoreSoftLink.h>
#import <pal/ios/ManagedConfigurationSoftLink.h>
#import <pal/ios/QuickLookSoftLink.h>

#if HAVE(LINK_PREVIEW) && USE(UICONTEXTMENU)
static NSString * const webkitShowLinkPreviewsPreferenceKey = @"WebKitShowLinkPreviews";
#endif

#if HAVE(UI_POINTER_INTERACTION)
static NSString * const pointerRegionIdentifier = @"WKPointerRegion";
static NSString * const editablePointerRegionIdentifier = @"WKEditablePointerRegion";

@interface WKContentView (WKUIPointerInteractionDelegate) <UIPointerInteractionDelegate_ForWebKitOnly>
@end
#endif

#if HAVE(PENCILKIT_TEXT_INPUT)
@interface WKContentView (WKUIIndirectScribbleInteractionDelegate) <UIIndirectScribbleInteractionDelegate>
@end
#endif

#if HAVE(PEPPER_UI_CORE)
#if HAVE(QUICKBOARD_CONTROLLER)
@interface WKContentView (QuickboardControllerSupport) <PUICQuickboardControllerDelegate>
@end
#endif // HAVE(QUICKBOARD_CONTROLLER)

@interface WKContentView (WatchSupport) <WKFocusedFormControlViewDelegate, WKSelectMenuListViewControllerDelegate, WKTextInputListViewControllerDelegate>
@end
#endif // HAVE(PEPPER_UI_CORE)

static void *WKContentViewKVOTransformContext = &WKContentViewKVOTransformContext;

#if ENABLE(IMAGE_ANALYSIS)

@interface WKContentView (ImageAnalysis) <WKImageAnalysisGestureRecognizerDelegate>
@end

#if USE(QUICK_LOOK)

@interface WKContentView (ImageAnalysisPreview) <QLPreviewControllerDelegate, QLPreviewControllerDataSource, QLPreviewItemDataProvider>
@end

#endif // USE(QUICK_LOOK)

#endif // ENABLE(IMAGE_ANALYSIS)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

@interface WKContentView (ImageAnalysisInteraction) <VKCImageAnalysisInteractionDelegate>
@end

#endif

#if ENABLE(IMAGE_ANALYSIS)

static bool canAttemptTextRecognitionForNonImageElements(const WebKit::InteractionInformationAtPosition& information, const WebKit::WebPreferences& preferences)
{
    return preferences.textRecognitionInVideosEnabled() && information.isPausedVideo;
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

@interface AVPlayerViewController (Staging_86237428)
- (void)setImageAnalysis:(CocoaImageAnalysis *)analysis;
@end

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#endif // ENABLE(IMAGE_ANALYSIS)

namespace WebKit {
using namespace WebCore;

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
    if (!editorState.postLayoutData)
        return;
    auto& postLayoutData = *editorState.postLayoutData;
    auto& visualData = *editorState.visualData;
    caretRect = visualData.caretRectAtEnd;
    caretColor = postLayoutData.caretColor;
    selectionGeometries = visualData.selectionGeometries;
    selectionClipRect = visualData.selectionClipRect;
}

inline bool operator==(const WKSelectionDrawingInfo& a, const WKSelectionDrawingInfo& b)
{
    if (a.type != b.type)
        return false;

    if (a.type == WKSelectionDrawingInfo::SelectionType::Range) {
        if (a.caretRect != b.caretRect)
            return false;

        if (a.caretColor != b.caretColor)
            return false;

        if (a.selectionGeometries.size() != b.selectionGeometries.size())
            return false;

        for (unsigned i = 0; i < a.selectionGeometries.size(); ++i) {
            auto& aGeometry = a.selectionGeometries[i];
            auto& bGeometry = b.selectionGeometries[i];
            auto behavior = aGeometry.behavior();
            if (behavior != bGeometry.behavior())
                return false;

            if (behavior == WebCore::SelectionRenderingBehavior::CoalesceBoundingRects && aGeometry.rect() != bGeometry.rect())
                return false;

            if (behavior == WebCore::SelectionRenderingBehavior::UseIndividualQuads && aGeometry.quad() != bGeometry.quad())
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
    stream.dumpProperty("caret color", info.caretColor);
    stream.dumpProperty("selection geometries", info.selectionGeometries);
    stream.dumpProperty("selection clip rect", info.selectionClipRect);
    return stream;
}

#if ENABLE(IMAGE_ANALYSIS)

class ImageAnalysisGestureDeferralToken final : public RefCounted<ImageAnalysisGestureDeferralToken> {
public:
    static RefPtr<ImageAnalysisGestureDeferralToken> create(WKContentView *view)
    {
        return adoptRef(*new ImageAnalysisGestureDeferralToken(view));
    }

    ~ImageAnalysisGestureDeferralToken()
    {
        if (auto view = m_view.get())
            [view _endImageAnalysisGestureDeferral:m_shouldPreventTextSelection ? WebKit::ShouldPreventGestures::Yes : WebKit::ShouldPreventGestures::No];
    }

    void setShouldPreventTextSelection()
    {
        m_shouldPreventTextSelection = true;
    }

private:
    ImageAnalysisGestureDeferralToken(WKContentView *view)
        : m_view(view)
    {
    }

    WeakObjCPtr<WKContentView> m_view;
    bool m_shouldPreventTextSelection { false };
};

#endif // ENABLE(IMAGE_ANALYSIS)

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

#if HAVE(UIFINDINTERACTION)

@interface WKFoundTextRange : UITextRange

@property (nonatomic) NSUInteger location;
@property (nonatomic) NSUInteger length;
@property (nonatomic, copy) NSString *frameIdentifier;
@property (nonatomic) NSUInteger order;

+ (WKFoundTextRange *)foundTextRangeWithWebFoundTextRange:(WebKit::WebFoundTextRange)range;

- (WebKit::WebFoundTextRange)webFoundTextRange;

@end

@interface WKFoundTextPosition : UITextPosition

@property (nonatomic) NSUInteger offset;
@property (nonatomic) NSUInteger order;

+ (WKFoundTextPosition *)textPositionWithOffset:(NSUInteger)offset order:(NSUInteger)order;

@end

#endif

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
    if (!WebKit::currentUserInterfaceIdiomIsSmallScreen())
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

    session.localContext = adoptNS([[WKDragSessionContext alloc] init]).get();
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

- (BOOL)_wk_isTapAndAHalf
{
    static dispatch_once_t onceToken;
    static Class tapAndAHalfGestureRecognizerClass;
    dispatch_once(&onceToken, ^{
        tapAndAHalfGestureRecognizerClass = NSClassFromString(@"UITapAndAHalfRecognizer");
    });
    return [self isKindOfClass:tapAndAHalfGestureRecognizerClass];
}

@end

@interface WKTargetedPreviewContainer : UIView
- (instancetype)initWithContentView:(WKContentView *)contentView NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;
@end

@implementation WKTargetedPreviewContainer {
    __weak WKContentView *_contentView;
}

- (instancetype)initWithContentView:(WKContentView *)contentView
{
    if (!(self = [super initWithFrame:CGRectZero]))
        return nil;

    _contentView = contentView;
    return self;
}

- (void)_didRemoveSubview:(UIView *)subview
{
    [super _didRemoveSubview:subview];

    if (self.subviews.count)
        return;

#if USE(UICONTEXTMENU)
    [_contentView _targetedPreviewContainerDidRemoveLastSubview:self];
#endif
}

@end

@interface WKContentView (WKInteractionPrivate)
- (void)accessibilitySpeakSelectionSetContent:(NSString *)string;
- (NSArray *)webSelectionRectsForSelectionGeometries:(const Vector<WebCore::SelectionGeometry>&)selectionRects;
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

- (void)_createAndConfigureHighlightLongPressGestureRecognizer
{
    _highlightLongPressGestureRecognizer = adoptNS([[WKHighlightLongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_highlightLongPressRecognized:)]);
    [_highlightLongPressGestureRecognizer setDelay:highlightDelay];
    [_highlightLongPressGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
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

    [self.layer addObserver:self forKeyPath:@"transform" options:NSKeyValueObservingOptionInitial context:WKContentViewKVOTransformContext];

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

    _touchStartDeferringGestureRecognizerForImmediatelyResettableGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_touchStartDeferringGestureRecognizerForImmediatelyResettableGestures setName:@"Deferrer for touch start (immediate reset)"];

    _touchStartDeferringGestureRecognizerForDelayedResettableGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_touchStartDeferringGestureRecognizerForDelayedResettableGestures setName:@"Deferrer for touch start (delayed reset)"];

    _touchStartDeferringGestureRecognizerForSyntheticTapGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_touchStartDeferringGestureRecognizerForSyntheticTapGestures setName:@"Deferrer for touch start (synthetic tap)"];

    _touchEndDeferringGestureRecognizerForImmediatelyResettableGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_touchEndDeferringGestureRecognizerForImmediatelyResettableGestures setName:@"Deferrer for touch end (immediate reset)"];

    _touchEndDeferringGestureRecognizerForDelayedResettableGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_touchEndDeferringGestureRecognizerForDelayedResettableGestures setName:@"Deferrer for touch end (delayed reset)"];

    _touchEndDeferringGestureRecognizerForSyntheticTapGestures = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_touchEndDeferringGestureRecognizerForSyntheticTapGestures setName:@"Deferrer for touch end (synthetic tap)"];

    _touchMoveDeferringGestureRecognizer = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_touchMoveDeferringGestureRecognizer setName:@"Deferrer for touch move"];

#if ENABLE(IMAGE_ANALYSIS)
    _imageAnalysisDeferringGestureRecognizer = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [_imageAnalysisDeferringGestureRecognizer setName:@"Deferrer for image analysis"];
    [_imageAnalysisDeferringGestureRecognizer setImmediatelyFailsAfterTouchEnd:YES];
    [_imageAnalysisDeferringGestureRecognizer setEnabled:WebKit::isLiveTextAvailableAndEnabled()];
#endif

    for (WKDeferringGestureRecognizer *gesture in self.deferringGestures) {
        gesture.delegate = self;
        [self addGestureRecognizer:gesture];
    }

    if (_gestureRecognizerConsistencyEnforcer)
        _gestureRecognizerConsistencyEnforcer->reset();

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

    [self _createAndConfigureHighlightLongPressGestureRecognizer];
    [self _createAndConfigureLongPressGestureRecognizer];

#if HAVE(LINK_PREVIEW)
    [self _updateLongPressAndHighlightLongPressGestures];
#endif

#if ENABLE(DRAG_SUPPORT)
    [self setUpDragAndDropInteractions];
#endif

#if HAVE(UI_POINTER_INTERACTION)
    [self setUpPointerInteraction];
#endif

#if HAVE(PENCILKIT_TEXT_INPUT)
    [self setUpScribbleInteraction];
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

    _touchActionGestureRecognizer = adoptNS([[WKTouchActionGestureRecognizer alloc] initWithTouchActionDelegate:self]);
    [self addGestureRecognizer:_touchActionGestureRecognizer.get()];

    // FIXME: This should be called when we get notified that loading has completed.
    [self setUpTextSelectionAssistant];

#if HAVE(LINK_PREVIEW)
    [self _registerPreview];
#endif

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [center addObserver:self selector:@selector(_willHideMenu:) name:UIMenuControllerWillHideMenuNotification object:nil];
    [center addObserver:self selector:@selector(_didHideMenu:) name:UIMenuControllerDidHideMenuNotification object:nil];
    ALLOW_DEPRECATED_DECLARATIONS_END
    
    [center addObserver:self selector:@selector(_keyboardDidRequestDismissal:) name:UIKeyboardPrivateDidRequestDismissalNotification object:nil];

    _showingTextStyleOptions = NO;
    
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
#if USE(UICONTEXTMENU)
    _isDisplayingContextMenuWithAnimation = NO;
#endif

#if USE(UICONTEXTMENU) && ENABLE(IMAGE_ANALYSIS)
    _contextMenuWasTriggeredByImageAnalysisTimeout = NO;
#endif

#if ENABLE(DATALIST_ELEMENT)
    _dataListTextSuggestionsInputView = nil;
    _dataListTextSuggestions = nil;
#endif

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    _textCheckingController = makeUnique<WebKit::TextCheckingController>(*_page);
#endif

    _autocorrectionContextNeedsUpdate = YES;

    _page->process().updateTextCheckerState();
    _page->setScreenIsBeingCaptured([[[self window] screen] isCaptured]);

#if ENABLE(IMAGE_ANALYSIS)
    [self _setUpImageAnalysis];
#endif

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
    _lastOutstandingPositionInformationRequest = std::nullopt;
    _isWaitingOnPositionInformation = NO;

    _focusRequiresStrongPasswordAssistance = NO;
    _additionalContextForStrongPasswordAssistance = nil;
    _waitingForEditDragSnapshot = NO;

    _autocorrectionContextNeedsUpdate = YES;
    _lastAutocorrectionContext = { };

    _candidateViewNeedsUpdate = NO;
    _seenHardwareKeyDownInNonEditableElement = NO;

    _textInteractionDidChangeFocusedElement = NO;
    _activeTextInteractionCount = 0;

    _treatAsContentEditableUntilNextEditorStateUpdate = NO;

#if USE(UICONTEXTMENU) && ENABLE(IMAGE_ANALYSIS)
    _contextMenuWasTriggeredByImageAnalysisTimeout = NO;
#endif

    if (_interactionViewsContainerView) {
        [self.layer removeObserver:self forKeyPath:@"transform" context:WKContentViewKVOTransformContext];
        [_interactionViewsContainerView removeFromSuperview];
        _interactionViewsContainerView = nil;
    }

    _waitingForKeyboardToStartAnimatingInAfterElementFocus = NO;
    _lastInsertedCharacterToOverrideCharacterBeforeSelection = std::nullopt;

    [_touchEventGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_touchEventGestureRecognizer.get()];

    for (WKDeferringGestureRecognizer *gesture in self.deferringGestures) {
        gesture.delegate = nil;
        [self removeGestureRecognizer:gesture];
    }

    if (_gestureRecognizerConsistencyEnforcer)
        _gestureRecognizerConsistencyEnforcer->reset();

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [_mouseGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_mouseGestureRecognizer.get()];
#if ENABLE(PENCIL_HOVER)
    [_pencilHoverGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_pencilHoverGestureRecognizer.get()];
#endif
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

    [self removeGestureRecognizer:_touchActionGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionLeftSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionRightSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionUpSwipeGestureRecognizer.get()];
    [self removeGestureRecognizer:_touchActionDownSwipeGestureRecognizer.get()];

    _layerTreeTransactionIdAtLastInteractionStart = { };

#if USE(UICONTEXTMENU)
    [self _removeContextMenuHintContainerIfPossible];
#endif // USE(UICONTEXTMENU)

#if ENABLE(DRAG_SUPPORT)
    [existingLocalDragSessionContext(_dragDropInteractionState.dragSession()) cleanUpTemporaryDirectories];
    [self teardownDragAndDropInteractions];
#endif

#if HAVE(UI_POINTER_INTERACTION)
    [self removeInteraction:_pointerInteraction.get()];
    _pointerInteraction = nil;
#endif

    [self resetShouldZoomToFocusRectAfterShowingKeyboard];

#if HAVE(PENCILKIT_TEXT_INPUT)
    [self cleanUpScribbleInteraction];
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

#if ENABLE(DATALIST_ELEMENT)
    _dataListTextSuggestionsInputView = nil;
    _dataListTextSuggestions = nil;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    [self _tearDownImageAnalysis];
#endif

    [self _removeContainerForContextMenuHintPreviews];
    [self _removeContainerForDragPreviews];
    [self _removeContainerForDropPreviews];
    [self unsuppressSoftwareKeyboardUsingLastAutocorrectionContextIfNeeded];

    _hasSetUpInteractions = NO;
    _suppressSelectionAssistantReasons = { };

    [self _resetPanningPreventionFlags];
    [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::DeniedForGesture];
    [self _cancelPendingKeyEventHandler];

    _cachedSelectedTextRange = nil;
}

- (void)cleanUpInteractionPreviewContainers
{
    [self _removeContainerForContextMenuHintPreviews];
}

- (void)_cancelPendingKeyEventHandler
{
    if (!_page)
        return;

    ASSERT_IMPLIES(_keyWebEventHandler, _page->hasQueuedKeyEvent());
    if (!_page->hasQueuedKeyEvent())
        return;

    if (auto keyEventHandler = std::exchange(_keyWebEventHandler, nil))
        keyEventHandler(_page->firstQueuedKeyEvent().nativeEvent(), NO);
}

- (void)_removeDefaultGestureRecognizers
{
    for (WKDeferringGestureRecognizer *gesture in self.deferringGestures)
        [self removeGestureRecognizer:gesture];
    [self removeGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self removeGestureRecognizer:_singleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
    [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_nonBlockingDoubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_doubleTapGestureRecognizerForDoubleClick.get()];
    [self removeGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_twoFingerSingleTapGestureRecognizer.get()];
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [self removeGestureRecognizer:_mouseGestureRecognizer.get()];
#if ENABLE(PENCIL_HOVER)
    [self removeGestureRecognizer:_pencilHoverGestureRecognizer.get()];
#endif
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
    for (WKDeferringGestureRecognizer *gesture in self.deferringGestures)
        [self addGestureRecognizer:gesture];
    [self addGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self addGestureRecognizer:_singleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
    [self addGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_nonBlockingDoubleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_doubleTapGestureRecognizerForDoubleClick.get()];
    [self addGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_twoFingerSingleTapGestureRecognizer.get()];
#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    [self addGestureRecognizer:_mouseGestureRecognizer.get()];
#if ENABLE(PENCIL_HOVER)
    [self addGestureRecognizer:_pencilHoverGestureRecognizer.get()];
#endif
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
    if (context != WKContentViewKVOTransformContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

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
    return editorState.hasPostLayoutData() && editorState.selectionIsRange && editorState.postLayoutData->insideFixedPosition;
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
        SetForScope becomingFirstResponder { _becomingFirstResponder, YES };
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

        if ([self canShowNonEmptySelectionView] || (!_suppressSelectionAssistantReasons && _activeTextInteractionCount))
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

        auto shouldBlurFocusedElement = [&] {
            if (_keyboardDidRequestDismissal)
                return true;

            if (self._shouldUseLegacySelectPopoverDismissalBehavior)
                return true;

            if (reason == EndEditingReasonAccessoryDone) {
                if (_focusRequiresStrongPasswordAssistance)
                    return true;

                if (WebKit::currentUserInterfaceIdiomIsSmallScreen())
                    return true;
            }

            return false;
        };

        if (shouldBlurFocusedElement()) {
            _page->blurFocusedElement();
            // Don't wait for WebPageProxy::blurFocusedElement() to round-trip back to us to hide the keyboard
            // because we know that the user explicitly requested us to do so.
            [self _elementDidBlur];
        }
    }

    [self _cancelInteraction];

    BOOL shouldDeactivateSelection = [&]() -> BOOL {
#if PLATFORM(MACCATALYST)
        if (reason == EndEditingReasonResigningFirstResponder) {
            // This logic is based on a similar check on macOS (in WebViewImpl::resignFirstResponder()) to
            // maintain the active selection when resigning first responder, if the new responder is in a
            // modal popover or panel.
            // FIXME: Ideally, this would additionally check that the new first responder corresponds to
            // a view underneath this view controller; however, there doesn't seem to be any way of doing
            // so at the moment. In lieu of this, we can at least check that the web view itself isn't
            // inside the popover.
            auto *controller = [UIViewController _viewControllerForFullScreenPresentationFromView:self];
            return [self isDescendantOfView:controller.viewIfLoaded] || controller.modalPresentationStyle != UIModalPresentationPopover;
        }
#endif // PLATFORM(MACCATALYST)
        return YES;
    }();

    if (shouldDeactivateSelection)
        [_textInteractionAssistant deactivateSelection];

    [self _resetInputViewDeferral];
}

- (BOOL)resignFirstResponderForWebView
{
    // FIXME: Maybe we should call resignFirstResponder on the superclass
    // and do nothing if the return value is NO.

    SetForScope resigningFirstResponderScope { _resigningFirstResponder, YES };

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
            RunLoop::main().dispatch([weakHandler = WeakObjCPtr<id>(_keyWebEventHandler.get()), weakSelf = WeakObjCPtr<WKContentView>(self)] {
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
                if (!page->hasQueuedKeyEvent())
                    return;
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

- (std::optional<unsigned>)activeTouchIdentifierForGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    NSMapTable<NSNumber *, UITouch *> *activeTouches = [_touchEventGestureRecognizer activeTouchesByIdentifier];
    for (NSNumber *touchIdentifier in activeTouches) {
        UITouch *touch = [activeTouches objectForKey:touchIdentifier];
        if ([touch.gestureRecognizers containsObject:gestureRecognizer])
            return [touchIdentifier unsignedIntValue];
    }
    return std::nullopt;
}

- (BOOL)_touchEventsMustRequireGestureRecognizerToFail:(UIGestureRecognizer *)gestureRecognizer
{
    auto webView = self.webView;
    if ([webView _isNavigationSwipeGestureRecognizer:gestureRecognizer])
        return YES;

    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([webView UIDelegate]);
    return gestureRecognizer.view == webView && [uiDelegate respondsToSelector:@selector(_webView:touchEventsMustRequireGestureRecognizerToFail:)]
        && [uiDelegate _webView:webView touchEventsMustRequireGestureRecognizerToFail:gestureRecognizer];
}

- (void)_webTouchEventsRecognized:(UIWebTouchEventsGestureRecognizer *)gestureRecognizer
{
    if (!_page->hasRunningProcess())
        return;

    const auto& lastTouchEvent = *gestureRecognizer.lastTouchEvent;
    _lastInteractionLocation = lastTouchEvent.locationInDocumentCoordinates;

    if (lastTouchEvent.type == UIWebTouchEventTouchBegin) {
        if (!_failedTouchStartDeferringGestures)
            _failedTouchStartDeferringGestures = { { } };

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
    WebKit::NativeWebTouchEvent nativeWebTouchEvent { &lastTouchEvent, gestureRecognizer.modifierFlags };
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

        if (nativeWebTouchEvent.isPotentialTap() && self.hasHiddenContentEditable && self._hasFocusedElement && !self.window.keyWindow)
            [self.window makeKeyWindow];

        auto stopDeferringNativeGesturesIfNeeded = [] (WKDeferringGestureRecognizer *gestureRecognizer) {
            if (gestureRecognizer.state == UIGestureRecognizerStatePossible)
                gestureRecognizer.state = UIGestureRecognizerStateFailed;
        };

        if (!_page->isHandlingPreventableTouchStart()) {
            for (WKDeferringGestureRecognizer *gestureRecognizer in self._touchStartDeferringGestures)
                stopDeferringNativeGesturesIfNeeded(gestureRecognizer);
        }

        if (!_page->isHandlingPreventableTouchMove())
            stopDeferringNativeGesturesIfNeeded(_touchMoveDeferringGestureRecognizer.get());

        if (!_page->isHandlingPreventableTouchEnd()) {
            for (WKDeferringGestureRecognizer *gestureRecognizer in self._touchEndDeferringGestures)
                stopDeferringNativeGesturesIfNeeded(gestureRecognizer);
        }

        _failedTouchStartDeferringGestures = std::nullopt;
    }
#endif // ENABLE(TOUCH_EVENTS)
}

#if ENABLE(TOUCH_EVENTS)
- (void)_handleTouchActionsForTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent
{
    auto* scrollingCoordinator = downcast<WebKit::RemoteScrollingCoordinatorProxyIOS>(_page->scrollingCoordinatorProxy());
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
    if (gestureRecognizer != _mouseGestureRecognizer && [_mouseGestureRecognizer mouseTouch] == touch && touch._isPointerTouch)
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

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceivePress:(UIPress *)press
{
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

- (UIWebTouchEventsGestureRecognizer *)touchEventGestureRecognizer
{
    return _touchEventGestureRecognizer.get();
}

- (WebKit::GestureRecognizerConsistencyEnforcer&)gestureRecognizerConsistencyEnforcer
{
    if (!_gestureRecognizerConsistencyEnforcer)
        _gestureRecognizerConsistencyEnforcer = makeUnique<WebKit::GestureRecognizerConsistencyEnforcer>(self);

    return *_gestureRecognizerConsistencyEnforcer;
}

- (NSArray<WKDeferringGestureRecognizer *> *)deferringGestures
{
    auto gestures = [NSMutableArray arrayWithCapacity:7];
    [gestures addObjectsFromArray:self._touchStartDeferringGestures];
    [gestures addObjectsFromArray:self._touchEndDeferringGestures];
    if (_touchMoveDeferringGestureRecognizer)
        [gestures addObject:_touchMoveDeferringGestureRecognizer.get()];
#if ENABLE(IMAGE_ANALYSIS)
    if (_imageAnalysisDeferringGestureRecognizer)
        [gestures addObject:_imageAnalysisDeferringGestureRecognizer.get()];
#endif
    return gestures;
}

- (NSArray<WKDeferringGestureRecognizer *> *)_touchStartDeferringGestures
{
    WKDeferringGestureRecognizer *recognizers[3];
    NSUInteger count = 0;
    auto add = [&] (const RetainPtr<WKDeferringGestureRecognizer>& recognizer) {
        if (recognizer)
            recognizers[count++] = recognizer.get();
    };
    add(_touchStartDeferringGestureRecognizerForImmediatelyResettableGestures);
    add(_touchStartDeferringGestureRecognizerForDelayedResettableGestures);
    add(_touchStartDeferringGestureRecognizerForSyntheticTapGestures);
    return [NSArray arrayWithObjects:recognizers count:count];
}

- (NSArray<WKDeferringGestureRecognizer *> *)_touchEndDeferringGestures
{
    WKDeferringGestureRecognizer *recognizers[3];
    NSUInteger count = 0;
    auto add = [&] (const RetainPtr<WKDeferringGestureRecognizer>& recognizer) {
        if (recognizer)
            recognizers[count++] = recognizer.get();
    };
    add(_touchEndDeferringGestureRecognizerForImmediatelyResettableGestures);
    add(_touchEndDeferringGestureRecognizerForDelayedResettableGestures);
    add(_touchEndDeferringGestureRecognizerForSyntheticTapGestures);
    return [NSArray arrayWithObjects:recognizers count:count];
}

- (void)_doneDeferringTouchStart:(BOOL)preventNativeGestures
{
    for (WKDeferringGestureRecognizer *gestureRecognizer in self._touchStartDeferringGestures) {
        [gestureRecognizer endDeferral:preventNativeGestures ? WebKit::ShouldPreventGestures::Yes : WebKit::ShouldPreventGestures::No];
        if (_failedTouchStartDeferringGestures && !preventNativeGestures)
            _failedTouchStartDeferringGestures->add(gestureRecognizer);
    }
}

- (void)_doneDeferringTouchMove:(BOOL)preventNativeGestures
{
    [_touchMoveDeferringGestureRecognizer endDeferral:preventNativeGestures ? WebKit::ShouldPreventGestures::Yes : WebKit::ShouldPreventGestures::No];
}

- (void)_doneDeferringTouchEnd:(BOOL)preventNativeGestures
{
    for (WKDeferringGestureRecognizer *gesture in self._touchEndDeferringGestures)
        [gesture endDeferral:preventNativeGestures ? WebKit::ShouldPreventGestures::Yes : WebKit::ShouldPreventGestures::No];
}

- (BOOL)_isTouchStartDeferringGesture:(WKDeferringGestureRecognizer *)gesture
{
    return gesture == _touchStartDeferringGestureRecognizerForSyntheticTapGestures || gesture == _touchStartDeferringGestureRecognizerForImmediatelyResettableGestures
        || gesture == _touchStartDeferringGestureRecognizerForDelayedResettableGestures;
}

- (BOOL)_isTouchEndDeferringGesture:(WKDeferringGestureRecognizer *)gesture
{
    return gesture == _touchEndDeferringGestureRecognizerForSyntheticTapGestures || gesture == _touchEndDeferringGestureRecognizerForImmediatelyResettableGestures
        || gesture == _touchEndDeferringGestureRecognizerForDelayedResettableGestures;
}

static NSValue *nsSizeForTapHighlightBorderRadius(WebCore::IntSize borderRadius, CGFloat borderRadiusScale)
{
    return [NSValue valueWithCGSize:CGSizeMake((borderRadius.width() * borderRadiusScale) + minimumTapHighlightRadius, (borderRadius.height() * borderRadiusScale) + minimumTapHighlightRadius)];
}

- (void)_updateTapHighlight
{
    if (![_highlightView superview])
        return;

    [_highlightView setColor:cocoaColor(_tapHighlightInformation.color).get()];

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

- (CGRect)tapHighlightViewRect
{
    return [_highlightView frame];
}

- (UIGestureRecognizer *)imageAnalysisGestureRecognizer
{
#if ENABLE(IMAGE_ANALYSIS)
    return _imageAnalysisGestureRecognizer.get();
#else
    return nil;
#endif
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

- (void)_didGetTapHighlightForRequest:(WebKit::TapIdentifier)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling
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

- (void)_disableDoubleTapGesturesDuringTapIfNecessary:(WebKit::TapIdentifier)requestID
{
    if (_latestTapID != requestID)
        return;

    [self _setDoubleTapGesturesEnabled:NO];
}

- (void)_handleSmartMagnificationInformationForPotentialTap:(WebKit::TapIdentifier)requestID renderRect:(const WebCore::FloatRect&)renderRect fitEntireRect:(BOOL)fitEntireRect viewportMinimumScale:(double)viewportMinimumScale viewportMaximumScale:(double)viewportMaximumScale nodeIsRootLevel:(BOOL)nodeIsRootLevel
{
    const auto& preferences = _page->preferences();

    ASSERT(preferences.fasterClicksEnabled());
    if (!_potentialTapInProgress)
        return;

    // We check both the system preference and the page preference, because we only want this
    // to apply in "desktop" mode.
    if (preferences.preferFasterClickOverDoubleTap() && _page->preferFasterClickOverDoubleTap()) {
        RELEASE_LOG(ViewGestures, "Potential tap found an element and fast taps are preferred. Trigger click. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
        if (preferences.zoomOnDoubleTapWhenRoot() && nodeIsRootLevel) {
            RELEASE_LOG(ViewGestures, "The click handler was on a root-level element, so don't disable double-tap. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
            return;
        }

        if (preferences.alwaysZoomOnDoubleTap()) {
            RELEASE_LOG(ViewGestures, "DTTZ is forced on, so don't disable double-tap. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
            return;
        }

        RELEASE_LOG(ViewGestures, "Give preference to click by disabling double-tap. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
        [self _setDoubleTapGesturesEnabled:NO];
        return;
    }

    auto currentScale = self._contentZoomScale;
    auto targetScale = _smartMagnificationController->zoomFactorForTargetRect(renderRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale);
    if (std::min(targetScale, currentScale) / std::max(targetScale, currentScale) > fasterTapSignificantZoomThreshold) {
        RELEASE_LOG(ViewGestures, "Potential tap would not cause a significant zoom. Trigger click. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
        [self _setDoubleTapGesturesEnabled:NO];
        return;
    }
    RELEASE_LOG(ViewGestures, "Potential tap may cause significant zoom. Wait. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
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
    [self _updateFrameOfContainerForContextMenuHintPreviewsIfNeeded];

    [self _cancelLongPressGestureRecognizer];
    [self _cancelInteraction];
}

- (void)_scrollingNodeScrollingWillBegin:(WebCore::ScrollingNodeID)scrollingNodeID
{
    [_textInteractionAssistant willStartScrollingOverflow];
}

- (void)_scrollingNodeScrollingDidEnd:(WebCore::ScrollingNodeID)scrollingNodeID
{
    // If scrolling ends before we've received a selection update,
    // we postpone showing the selection until the update is received.
    if (!_selectionNeedsUpdate) {
        _shouldRestoreSelection = YES;
        return;
    }
    [self _updateChangedSelection];
    [_textInteractionAssistant didEndScrollingOverflow];

    UIScrollView *targetScrollView = nil;
    if (auto* scrollingCoordinator = downcast<WebKit::RemoteScrollingCoordinatorProxyIOS>(_page->scrollingCoordinatorProxy()))
        targetScrollView = scrollingCoordinator->scrollViewForScrollingNodeID(scrollingNodeID);

    [_webView _didFinishScrolling:targetScrollView];
}

- (BOOL)shouldShowAutomaticKeyboardUI
{
    if (_focusedElementInformation.inputMode == WebCore::InputMode::None)
        return NO;

    if (_focusedElementInformation.isFocusingWithDataListDropdown && !UIKeyboard.isInHardwareKeyboardMode)
        return NO;

    return [self _shouldShowAutomaticKeyboardUIIgnoringInputMode];
}

- (BOOL)_shouldShowAutomaticKeyboardUIIgnoringInputMode
{
    if (_focusedElementInformation.isReadOnly)
        return NO;

    switch (_focusedElementInformation.elementType) {
    case WebKit::InputType::None:
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
#endif
    case WebKit::InputType::Drawing:
    case WebKit::InputType::Date:
    case WebKit::InputType::Month:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Time:
        return NO;
    case WebKit::InputType::Select: {
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
        if (self._shouldUseContextMenusForFormControls)
            return NO;
#endif
        return WebKit::currentUserInterfaceIdiomIsSmallScreen();
    }
    default:
        return YES;
    }
    return NO;
}

- (BOOL)_disableAutomaticKeyboardUI
{
    // Always enable automatic keyboard UI if we are not the first responder to avoid
    // interfering with other focused views (e.g. Find-in-page).
    return [self isFirstResponder] && ![self shouldShowAutomaticKeyboardUI];
}

- (BOOL)_requiresKeyboardWhenFirstResponder
{
    if ([_webView _isEditable] && !self._disableAutomaticKeyboardUI)
        return YES;
    // FIXME: We should add the logic to handle keyboard visibility during focus redirects.
    return [self _shouldShowAutomaticKeyboardUIIgnoringInputMode] || _seenHardwareKeyDownInNonEditableElement;
}

- (BOOL)_requiresKeyboardResetOnReload
{
    return YES;
}

- (WebCore::FloatRect)rectToRevealWhenZoomingToFocusedElement
{
    WebCore::IntRect elementInteractionRect;
    if (_focusedElementInformation.interactionRect.contains(_focusedElementInformation.lastInteractionLocation))
        elementInteractionRect = { _focusedElementInformation.lastInteractionLocation, { 1, 1 } };

    if (!mayContainSelectableText(_focusedElementInformation.elementType))
        return elementInteractionRect;

    if (!_page->editorState().hasPostLayoutAndVisualData())
        return elementInteractionRect;

    auto boundingRect = _page->selectionBoundingRectInRootViewCoordinates();
    boundingRect.intersect(_focusedElementInformation.interactionRect);
    return boundingRect;
}

- (void)_keyboardWillShow
{
    _waitingForKeyboardToStartAnimatingInAfterElementFocus = NO;
}

- (void)_keyboardDidShow
{
    [self _zoomToFocusRectAfterShowingKeyboardIfNeeded];

#if USE(UICONTEXTMENU)
    [_fileUploadPanel repositionContextMenuIfNeeded];
#endif
}

- (void)_zoomToFocusRectAfterShowingKeyboardIfNeeded
{
    if (!_shouldZoomToFocusRectAfterShowingKeyboard)
        return;

    // FIXME: This deferred call to -_zoomToRevealFocusedElement works around the fact that Mail compose
    // disables automatic content inset adjustment using the keyboard height, and instead has logic to
    // explicitly set WKScrollView's contentScrollInset after receiving UIKeyboardDidShowNotification.
    // This means that if we -_zoomToRevealFocusedElement immediately after focusing the body field in
    // Mail, we won't take the keyboard height into account when scrolling.
    // Mitigate this by deferring the call to -_zoomToRevealFocusedElement in this case until after the
    // keyboard has finished animating. We can revert this once rdar://87733414 is fixed.
    [self resetShouldZoomToFocusRectAfterShowingKeyboard];
    [self performSelector:@selector(_zoomToRevealFocusedElement) withObject:nil afterDelay:0];
}

- (void)_zoomToRevealFocusedElement
{
    if (_focusedElementInformation.preventScroll)
        return;

    if (_suppressSelectionAssistantReasons || _activeTextInteractionCount)
        return;

    if (!self._scroller.firstResponderKeyboardAvoidanceEnabled
        && (_page->isKeyboardAnimatingIn() || _waitingForKeyboardToStartAnimatingInAfterElementFocus)) {
        _shouldZoomToFocusRectAfterShowingKeyboard = YES;
        return;
    }

    // In case user scaling is force enabled, do not use that scaling when zooming in with an input field.
    // Zooming above the page's default scale factor should only happen when the user performs it.
    [self _zoomToFocusRect:_focusedElementInformation.interactionRect
        selectionRect:_didAccessoryTabInitiateFocus ? WebCore::FloatRect() : self.rectToRevealWhenZoomingToFocusedElement
        fontSize:_focusedElementInformation.nodeFontSize
        minimumScale:_focusedElementInformation.minimumScaleFactor
        maximumScale:_focusedElementInformation.maximumScaleFactorIgnoringAlwaysScalable
        allowScaling:_focusedElementInformation.allowsUserScalingIgnoringAlwaysScalable && WebKit::currentUserInterfaceIdiomIsSmallScreen()
        forceScroll:[self requiresAccessoryView]];
}

- (void)resetShouldZoomToFocusRectAfterShowingKeyboard
{
    _shouldZoomToFocusRectAfterShowingKeyboard = NO;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_zoomToRevealFocusedElement) object:nil];
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
    if (_page->waitingForPostLayoutEditorStateUpdateAfterFocusingElement())
        return _focusedElementInformation.interactionRect;

    if (!_page->editorState().visualData->selectionClipRect.isEmpty())
        return _page->editorState().visualData->selectionClipRect;

    return CGRectNull;
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

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WKContentViewInteractionAdditions_SimultaneousRecognitionExtras.mm>)
#import <WebKitAdditions/WKContentViewInteractionAdditions_SimultaneousRecognitionExtras.mm>
#else
- (BOOL)_shouldAdditionallyRecognizeGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer simultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return NO;
}
#endif

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer*)otherGestureRecognizer
{
    for (WKDeferringGestureRecognizer *gesture in self.deferringGestures) {
        if (isSamePair(gestureRecognizer, otherGestureRecognizer, _touchEventGestureRecognizer.get(), gesture))
            return YES;
    }

    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class] && [otherGestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return YES;

#if ENABLE(IMAGE_ANALYSIS)
    if (gestureRecognizer == _imageAnalysisDeferringGestureRecognizer)
        return ![self shouldDeferGestureDueToImageAnalysis:otherGestureRecognizer];

    if (otherGestureRecognizer == _imageAnalysisDeferringGestureRecognizer)
        return ![self shouldDeferGestureDueToImageAnalysis:gestureRecognizer];
#endif

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

        if (gestureRecognizer._wk_isTapAndAHalf || otherGestureRecognizer._wk_isTapAndAHalf)
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

#if ENABLE(IMAGE_ANALYSIS)
    if (gestureRecognizer == _imageAnalysisGestureRecognizer || gestureRecognizer == _imageAnalysisTimeoutGestureRecognizer)
        return YES;
#endif

    if ([self _shouldAdditionallyRecognizeGestureRecognizer:gestureRecognizer simultaneouslyWithGestureRecognizer:otherGestureRecognizer])
        return YES;

    return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if (gestureRecognizer == _touchEventGestureRecognizer && [self _touchEventsMustRequireGestureRecognizerToFail:otherGestureRecognizer])
        return YES;

    if ([otherGestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)otherGestureRecognizer shouldDeferGestureRecognizer:gestureRecognizer];

#if USE(UICONTEXTMENU) && ENABLE(IMAGE_ANALYSIS)
    if (gestureRecognizer == _imageAnalysisTimeoutGestureRecognizer && otherGestureRecognizer == [self.contextMenuInteraction gestureRecognizerForFailureRelationships])
        return YES;
#endif

    return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)gestureRecognizer shouldDeferGestureRecognizer:otherGestureRecognizer];

#if USE(UICONTEXTMENU) && ENABLE(IMAGE_ANALYSIS)
    if (gestureRecognizer == [self.contextMenuInteraction gestureRecognizerForFailureRelationships] && otherGestureRecognizer == _imageAnalysisTimeoutGestureRecognizer)
        return YES;
#endif

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

    auto element = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeAttachment URL:(NSURL *)_positionInformation.url imageURL:(NSURL *)_positionInformation.imageURL location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:nil imageMIMEType:_positionInformation.imageMIMEType]);
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

    _lastOutstandingPositionInformationRequest = request;

    _page->requestPositionInformation(request);
}

- (BOOL)_currentPositionInformationIsValidForRequest:(const WebKit::InteractionInformationRequest&)request
{
    return _hasValidPositionInformation && _positionInformation.request.isValidForRequest(request);
}

- (BOOL)_hasValidOutstandingPositionInformationRequest:(const WebKit::InteractionInformationRequest&)request
{
    return _lastOutstandingPositionInformationRequest && _lastOutstandingPositionInformationRequest->isValidForRequest(request);
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

        _pendingPositionInformationHandlers[index] = std::nullopt;

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
    for (auto& rectInfo : _lastSelectionDrawingInfo.selectionGeometries) {
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
    if (_lastSelectionDrawingInfo.selectionGeometries.isEmpty())
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
        if (![self _currentPositionInformationIsApproximatelyValidForRequest:request radiusForApproximation:[_doubleTapGestureRecognizerForDoubleClick allowableMovement]]) {
            if (![self ensurePositionInformationIsUpToDate:request])
                return NO;
        }
        return _positionInformation.nodeAtPositionHasDoubleClickHandler.value_or(false);
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
    [self _fadeTapHighlightViewIfNeeded];
}

- (void)_fadeTapHighlightViewIfNeeded
{
    if (![_highlightView superview] || _isTapHighlightFading)
        return;

    _isTapHighlightFading = YES;
    CGFloat tapHighlightFadeDuration = _showDebugTapHighlightsForFastClicking ? 0.25 : 0.1;
    [UIView animateWithDuration:tapHighlightFadeDuration
        animations:^{
            [_highlightView layer].opacity = 0;
        }
        completion:^(BOOL finished) {
            if (finished)
                [_highlightView removeFromSuperview];
            _isTapHighlightFading = NO;
        }];
}

- (BOOL)canShowNonEmptySelectionView
{
    if (_suppressSelectionAssistantReasons)
        return NO;

    auto& state = _page->editorState();
    return state.hasPostLayoutData() && !state.selectionIsNone;
}

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([_imageAnalysisInteraction interactableItemExistsAtPoint:point])
        return NO;
#endif

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return NO;
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!_page->preferences().textInteractionEnabled())
        return NO;

    if (_suppressSelectionAssistantReasons)
        return NO;

    if (_inspectorNodeSearchEnabled)
        return NO;

    WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
    if (![self ensurePositionInformationIsUpToDate:request])
        return NO;

#if ENABLE(IMAGE_ANALYSIS)
    if (_elementPendingImageAnalysis && _positionInformation.hostImageOrVideoElementContext == _elementPendingImageAnalysis)
        return YES;
#endif

    return _positionInformation.isSelectable();
}

- (BOOL)pointIsNearMarkedText:(CGPoint)point
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([_imageAnalysisInteraction interactableItemExistsAtPoint:point])
        return NO;
#endif

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return NO;
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!_page->preferences().textInteractionEnabled())
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
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([_imageAnalysisInteraction interactableItemExistsAtPoint:point])
        return NO;
#endif

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return NO;
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!_page->preferences().textInteractionEnabled())
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
                return _page->editorState().selectionIsRange;
            }
        }
    }

    WebKit::InteractionInformationRequest request(WebCore::roundedIntPoint(point));
    if (![self ensurePositionInformationIsUpToDate:request])
        return NO;

    if (gesture == UIWKGestureLoupe && _positionInformation.selectability == WebKit::InteractionInformationAtPosition::Selectability::UnselectableDueToUserSelectNone)
        return NO;

#if ENABLE(DATALIST_ELEMENT)
    if (_positionInformation.preventTextInteraction)
        return NO;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    if (_elementPendingImageAnalysis && _positionInformation.hostImageOrVideoElementContext == _elementPendingImageAnalysis)
        return YES;
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

- (NSArray *)webSelectionRectsForSelectionGeometries:(const Vector<WebCore::SelectionGeometry>&)selectionGeometries
{
    if (selectionGeometries.isEmpty())
        return nil;

    return createNSArray(selectionGeometries, [] (auto& geometry) {
        auto webRect = [WebSelectionRect selectionRect];
        webRect.rect = geometry.rect();
        webRect.writingDirection = geometry.direction() == WebCore::TextDirection::LTR ? WKWritingDirectionLeftToRight : WKWritingDirectionRightToLeft;
        webRect.isLineBreak = geometry.isLineBreak();
        webRect.isFirstOnLine = geometry.isFirstOnLine();
        webRect.isLastOnLine = geometry.isLastOnLine();
        webRect.containsStart = geometry.containsStart();
        webRect.containsEnd = geometry.containsEnd();
        webRect.isInFixedPosition = geometry.isInFixedPosition();
        webRect.isHorizontal = geometry.isHorizontal();
        return webRect;
    }).autorelease();
}

- (NSArray *)webSelectionRects
{
    if (!_page->editorState().hasPostLayoutAndVisualData() || _page->editorState().selectionIsNone)
        return nil;
    const auto& selectionGeometries = _page->editorState().visualData->selectionGeometries;
    return [self webSelectionRectsForSelectionGeometries:selectionGeometries];
}

- (WebKit::TapIdentifier)nextTapIdentifier
{
    _latestTapID = WebKit::TapIdentifier::generate();
    return _latestTapID;
}

- (void)_highlightLongPressRecognized:(UILongPressGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _highlightLongPressGestureRecognizer);
    [self _resetIsDoubleTapPending];

    auto startPoint = [gestureRecognizer startPoint];

    _lastInteractionLocation = startPoint;

    switch ([gestureRecognizer state]) {
    case UIGestureRecognizerStateBegan:
        _longPressCanClick = YES;
        cancelPotentialTapIfNecessary(self);
        _page->tapHighlightAtPosition(startPoint, [self nextTapIdentifier]);
        _isTapHighlightIDValid = YES;
        break;
    case UIGestureRecognizerStateEnded:
        if (_longPressCanClick && _positionInformation.isElement) {
            [self _attemptSyntheticClickAtLocation:startPoint modifierFlags:gestureRecognizer.modifierFlags];
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
    _page->handleDoubleTapForDoubleClickAtPoint(WebCore::IntPoint([gestureRecognizer locationInView:self]), WebKit::webEventModifierFlags(gestureRecognizer.modifierFlags), _layerTreeTransactionIdAtLastInteractionStart);
}

- (void)_twoFingerSingleTapGestureRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    _isTapHighlightIDValid = YES;
    _isExpectingFastSingleTapCommit = YES;
    _page->handleTwoFingerTapAtPoint(WebCore::roundedIntPoint([gestureRecognizer locationInView:self]), WebKit::webEventModifierFlags(gestureRecognizer.modifierFlags | UIKeyModifierCommand), [self nextTapIdentifier]);
}

- (void)_longPressRecognized:(UILongPressGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _longPressGestureRecognizer);
    [self _resetIsDoubleTapPending];
    [self _cancelTouchEventGestureRecognizer];

    _page->didRecognizeLongPress();

    _lastInteractionLocation = [gestureRecognizer startPoint];

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
        RELEASE_LOG(ViewGestures, "ending potential tap - double taps are back. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());

        [self _setDoubleTapGesturesEnabled:YES];
    }

    RELEASE_LOG(ViewGestures, "Ending potential tap. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());

    _potentialTapInProgress = NO;
}

- (void)_singleTapIdentified:(UITapGestureRecognizer *)gestureRecognizer
{
    auto position = [gestureRecognizer locationInView:self];

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([self _handleTapOverImageAnalysisInteractionButton:position])
        return;
#endif

    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);
    ASSERT(!_potentialTapInProgress);
    [self _resetIsDoubleTapPending];

    [_inputPeripheral setSingleTapShouldEndEditing:[_inputPeripheral isEditing]];

    bool shouldRequestMagnificationInformation = _page->preferences().fasterClicksEnabled();
    if (shouldRequestMagnificationInformation)
        RELEASE_LOG(ViewGestures, "Single tap identified. Request details on potential zoom. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());

    _page->potentialTapAtPosition(position, shouldRequestMagnificationInformation, [self nextTapIdentifier]);
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
        if (_commitPotentialTapPointerId != pointerId)
            _page->touchWithIdentifierWasRemoved(pointerId);
    }

    if (!_isTapHighlightIDValid)
        [self _fadeTapHighlightViewIfNeeded];
}

- (void)_doubleTapDidFail:(UITapGestureRecognizer *)gestureRecognizer
{
    RELEASE_LOG(ViewGestures, "Double tap was not recognized. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
    ASSERT(gestureRecognizer == _doubleTapGestureRecognizer);
}

- (void)_commitPotentialTapFailed
{
    _page->touchWithIdentifierWasRemoved(_commitPotentialTapPointerId);
    _commitPotentialTapPointerId = 0;

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
    _page->touchWithIdentifierWasRemoved(_commitPotentialTapPointerId);
    _commitPotentialTapPointerId = 0;

    RELEASE_LOG(ViewGestures, "Synthetic click completed. (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());
    [self _resetInputViewDeferral];
}

- (void)_singleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);

    if (![self isFirstResponder])
        [self becomeFirstResponder];

    ASSERT(_potentialTapInProgress);

    _lastInteractionLocation = [gestureRecognizer locationInView:self];

    [self _endPotentialTapAndEnableDoubleTapGesturesIfNecessary];

    if (_hasTapHighlightForPotentialTap) {
        [self _showTapHighlight];
        _hasTapHighlightForPotentialTap = NO;
    }

    if ([_inputPeripheral singleTapShouldEndEditing])
        [_inputPeripheral endEditing];

    RELEASE_LOG(ViewGestures, "Single tap recognized - commit potential tap (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());

    WebCore::PointerID pointerId = WebCore::mousePointerID;
    if (auto* singleTapTouchIdentifier = [_singleTapGestureRecognizer lastActiveTouchIdentifier]) {
        pointerId = [singleTapTouchIdentifier unsignedIntValue];
        _commitPotentialTapPointerId = pointerId;
    }
    _page->commitPotentialTap(WebKit::webEventModifierFlags(gestureRecognizer.modifierFlags), _layerTreeTransactionIdAtLastInteractionStart, pointerId);

    if (!_isExpectingFastSingleTapCommit)
        [self _finishInteraction];

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if (![_imageAnalysisInteraction interactableItemExistsAtPoint:_lastInteractionLocation])
        [_imageAnalysisInteraction resetSelection];
#endif
}

- (void)_doubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    RELEASE_LOG(ViewGestures, "Identified a double tap (%p, pageProxyID=%llu)", self, _page->identifier().toUInt64());

    [self _resetIsDoubleTapPending];

    auto location = [gestureRecognizer locationInView:self];
    _lastInteractionLocation = location;
    _smartMagnificationController->handleSmartMagnificationGesture(location);
}

- (void)_resetIsDoubleTapPending
{
    _isDoubleTapPending = NO;
}

- (void)_nonBlockingDoubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    _lastInteractionLocation = [gestureRecognizer locationInView:self];
    _isDoubleTapPending = YES;
}

- (void)_twoFingerDoubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    [self _resetIsDoubleTapPending];

    auto location = [gestureRecognizer locationInView:self];
    _lastInteractionLocation = location;
    _smartMagnificationController->handleResetMagnificationGesture(location);
}

- (void)_attemptSyntheticClickAtLocation:(CGPoint)location modifierFlags:(UIKeyModifierFlags)modifierFlags
{
    if (![self isFirstResponder])
        [self becomeFirstResponder];

    [_inputPeripheral endEditing];
    _page->attemptSyntheticClick(location, WebKit::webEventModifierFlags(modifierFlags), _layerTreeTransactionIdAtLastInteractionStart);
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

- (void)_invalidateCurrentPositionInformation
{
    _hasValidPositionInformation = NO;
    _positionInformation = { };
}

- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info
{
    if (_lastOutstandingPositionInformationRequest && info.request.isValidForRequest(*_lastOutstandingPositionInformationRequest))
        _lastOutstandingPositionInformationRequest = std::nullopt;

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
    [_textInteractionAssistant willStartScrollingOrZooming];
    _page->setIsScrollingOrZooming(true);

#if HAVE(PEPPER_UI_CORE)
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
    if (!_needsDeferredEndScrollingSelectionUpdate)
        [_textInteractionAssistant didEndScrollingOrZooming];
    _page->setIsScrollingOrZooming(false);

    [self _resetPanningPreventionFlags];

#if HAVE(PEPPER_UI_CORE)
    [_focusedFormControlView engageFocusedFormControlNavigation];
#endif
}

- (BOOL)_elementTypeRequiresAccessoryView:(WebKit::InputType)type
{
    switch (type) {
    case WebKit::InputType::None:
#if ENABLE(INPUT_TYPE_COLOR)
    case WebKit::InputType::Color:
#endif
    case WebKit::InputType::Drawing:
    case WebKit::InputType::Date:
    case WebKit::InputType::DateTimeLocal:
    case WebKit::InputType::Month:
    case WebKit::InputType::Time:
        return NO;
    case WebKit::InputType::Select: {
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
        if (self._shouldUseContextMenusForFormControls)
            return NO;
#endif
        return WebKit::currentUserInterfaceIdiomIsSmallScreen();
    }
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
    case WebKit::InputType::Week:
        return WebKit::currentUserInterfaceIdiomIsSmallScreen();
    }
}

- (BOOL)requiresAccessoryView
{
    if ([_formInputSession accessoryViewShouldNotShow])
        return NO;

    if ([_formInputSession customInputAccessoryView])
        return YES;

    return [self _elementTypeRequiresAccessoryView:_focusedElementInformation.elementType];
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

    static NeverDestroyed supportedTypes = [] {
        auto types = adoptNS([[NSMutableArray alloc] init]);
        [types addObject:@(WebCore::PasteboardCustomData::cocoaType().characters())];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [types addObject:(id)kUTTypeURL];
        ALLOW_DEPRECATED_DECLARATIONS_END
        [types addObjectsFromArray:UIPasteboardTypeListString];
        return types;
    }();
    static NeverDestroyed supportedTypesWithImageAndWebArchive = [] {
        auto types = adoptNS([[NSMutableArray alloc] init]);
        [types addObject:WebCore::WebArchivePboardType];
        [types addObjectsFromArray:UIPasteboardTypeListImage];
        [types addObjectsFromArray:supportedTypes.get().get()];
        return types;
    }();

    if (_page->editorState().isContentRichlyEditable)
        return supportedTypesWithImageAndWebArchive.get().get();
    return supportedTypes.get().get();
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
    _page->getSelectionContext([view = retainPtr(self)](const String& selectedText, const String& textBefore, const String& textAfter) {
        if (!selectedText)
            return;

        auto& editorState = view->_page->editorState();
        if (!editorState.hasPostLayoutAndVisualData())
            return;
        auto& visualData = *editorState.visualData;
        CGRect presentationRect;
        if (editorState.selectionIsRange && !visualData.selectionGeometries.isEmpty())
            presentationRect = view->_page->selectionBoundingRectInRootViewCoordinates();
        else
            presentationRect = visualData.caretRectAtStart;
        
        String selectionContext = textBefore + selectedText + textAfter;
        NSRange selectedRangeInContext = NSMakeRange(textBefore.length(), selectedText.length());

        if (auto textSelectionAssistant = view->_textInteractionAssistant)
            [textSelectionAssistant lookup:selectionContext withRange:selectedRangeInContext fromRect:presentationRect];
    });
}

- (void)_shareForWebView:(id)sender
{
    RetainPtr<WKContentView> view = self;
    _page->getSelectionOrContentsAsString([view](const String& string) {
        if (!view->_textInteractionAssistant || !string || !view->_page->editorState().hasVisualData())
            return;

        auto& selectionGeometries = view->_page->editorState().visualData->selectionGeometries;
        if (selectionGeometries.isEmpty())
            return;

        [view->_textInteractionAssistant showShareSheetFor:string fromRect:selectionGeometries.first().rect()];
    });
}

- (void)_translateForWebView:(id)sender
{
    _page->getSelectionOrContentsAsString([weakSelf = WeakObjCPtr<WKContentView>(self)] (const String& string) {
        if (!weakSelf)
            return;

        if (string.isEmpty())
            return;

        auto strongSelf = weakSelf.get();
        if (!strongSelf->_page->editorState().hasVisualData())
            return;

        if (strongSelf->_page->editorState().visualData->selectionGeometries.isEmpty())
            return;

        if ([strongSelf->_textInteractionAssistant respondsToSelector:@selector(translate:fromRect:)])
            [strongSelf->_textInteractionAssistant translate:string fromRect:strongSelf->_page->selectionBoundingRectInRootViewCoordinates()];
    });
}

- (void)_addShortcutForWebView:(id)sender
{
    if (!_page->editorState().visualData)
        return;
    [_textInteractionAssistant showTextServiceFor:[self selectedText] fromRect:_page->editorState().visualData->selectionGeometries[0].rect()];
}

- (NSString *)selectedText
{
    if (!_page->editorState().postLayoutData)
        return nil;
    return (NSString *)_page->editorState().postLayoutData->wordAtSelection;
}

- (NSArray<NSTextAlternatives *> *)alternativesForSelectedText
{
    if (!_page->editorState().postLayoutData)
        return nil;
    auto& dictationContextsForSelection = _page->editorState().postLayoutData->dictationContextsForSelection;
    return createNSArray(dictationContextsForSelection, [&] (auto& dictationContext) {
        return _page->platformDictationAlternatives(dictationContext);
    }).autorelease();
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
    if (!_page->editorState().postLayoutData)
        return NO;
    return _page->editorState().postLayoutData->isReplaceAllowed;
}

- (void)replaceText:(NSString *)text withText:(NSString *)word
{
    _autocorrectionContextNeedsUpdate = YES;
    _page->replaceSelectedText(text, word);
}

- (void)selectWordBackward
{
    _autocorrectionContextNeedsUpdate = YES;
    _page->selectWordBackward();
}

- (void)_promptForReplaceForWebView:(id)sender
{
    if (!_page->editorState().postLayoutData)
        return;
    const auto& wordAtSelection = _page->editorState().postLayoutData->wordAtSelection;
    if (wordAtSelection.isEmpty())
        return;

    [_textInteractionAssistant scheduleReplacementsForText:wordAtSelection];
}

- (void)_transliterateChineseForWebView:(id)sender
{
    if (!_page->editorState().postLayoutData)
        return;
    [_textInteractionAssistant scheduleChineseTransliterationForText:_page->editorState().postLayoutData->wordAtSelection];
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

    if (NSString *textStyleAttribute = [font.fontDescriptor.fontAttributes objectForKey:UIFontDescriptorTextStyleAttribute])
        changes.setFontFamily(textStyleAttribute);

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
    WebCore::Color textColor(WebCore::roundAndClampToSRGBALossy(color.CGColor));
    _page->executeEditCommand("ForeColor"_s, WebCore::serializationForHTML(textColor));
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

    if (!_page->editorState().postLayoutData)
        return nil;
    auto typingAttributes = _page->editorState().postLayoutData->typingAttributes;
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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (!self.webView.configuration._textInteractionGesturesEnabled)
        return [UIColor clearColor];
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!_page->preferences().textInteractionEnabled())
        return [UIColor clearColor];

    if (_page->editorState().hasPostLayoutData()) {
        if (auto caretColor = _page->editorState().postLayoutData->caretColor; caretColor.isValid())
            return cocoaColor(caretColor).autorelease();
    }

    return [self _inheritedInteractionTintColor];
}

- (void)_updateInteractionTintColor:(UITextInputTraits *)traits
{
    [traits _setColorsToMatchTintColor:[self _cascadeInteractionTintColor]];
}

- (void)tintColorDidChange
{
    [super tintColorDidChange];

    BOOL shouldUpdateTextSelection = self.isFirstResponder && [self canShowNonEmptySelectionView];
    if (shouldUpdateTextSelection)
        [_textInteractionAssistant deactivateSelection];
    [self _updateInteractionTintColor:_traits.get()];
    if (shouldUpdateTextSelection)
        [_textInteractionAssistant activateSelection];
}

#if ENABLE(APP_HIGHLIGHTS)

- (BOOL)shouldAllowAppHighlightCreation
{
    auto editorState = _page->editorState();
    return editorState.selectionIsRange && !editorState.isContentEditable && !editorState.selectionIsRangeInsideImageOverlay;
}

#endif // ENABLE(APP_HIGHLIGHTS)

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

    if (action == @selector(_deleteByWord) || action == @selector(_deleteForwardByWord) || action == @selector(_deleteForwardAndNotify:) || action == @selector(_deleteToEndOfParagraph) || action == @selector(_deleteToStartOfLine)
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
    }

    if (action == @selector(copy:)) {
        if (editorState.isInPasswordField && !editorState.selectionIsRangeInAutoFilledAndViewableField)
            return NO;
        return editorState.selectionIsRange;
    }

    if (action == @selector(_define:)) {
        if (editorState.isInPasswordField || !editorState.selectionIsRange)
            return NO;

        NSUInteger textLength = editorState.postLayoutData ? editorState.postLayoutData->selectedTextLength : 0;
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

        return editorState.postLayoutData && editorState.postLayoutData->selectedTextLength > 0;
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
        if (!editorState.selectionIsRange || !editorState.postLayoutData || !editorState.postLayoutData->isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        if ([[self selectedText] _containsCJScriptsOnly])
            return NO;
        return YES;
    }

    if (action == @selector(_transliterateChinese:)) {
        if (!editorState.selectionIsRange || !editorState.postLayoutData || !editorState.postLayoutData->isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        return UIKeyboardEnabledInputModesAllowChineseTransliterationForText([self selectedText]);
    }

    if (action == @selector(_translate:))
        return !editorState.isInPasswordField && editorState.selectionIsRange;

    if (action == @selector(select:)) {
        // Disable select in password fields so that you can't see word boundaries.
        return !editorState.isInPasswordField && !editorState.selectionIsRange && self.hasContent;
    }

    if (action == @selector(selectAll:)) {
        BOOL isPreparingEditMenu = _isPreparingEditMenu;
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        isPreparingEditMenu |= [sender isKindOfClass:UIMenuController.class];
        ALLOW_DEPRECATED_DECLARATIONS_END
        if (isPreparingEditMenu) {
            // By platform convention we don't show Select All in the callout menu for a range selection.
            return !editorState.selectionIsRange && self.hasContent;
        }
        return YES;
    }

    if (action == @selector(replace:))
        return editorState.isContentEditable && !editorState.isInPasswordField;

    if (action == @selector(makeTextWritingDirectionLeftToRight:) || action == @selector(makeTextWritingDirectionRightToLeft:)) {
        if (!editorState.isContentEditable)
            return NO;

        if (!editorState.postLayoutData)
            return NO;
        auto baseWritingDirection = editorState.postLayoutData->baseWritingDirection;
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

#if ENABLE(IMAGE_ANALYSIS)
    if (action == @selector(captureTextFromCamera:)) {
        if (!mayContainSelectableText(_focusedElementInformation.elementType) || _focusedElementInformation.isReadOnly)
            return NO;

        BOOL isPreparingEditMenu = _isPreparingEditMenu;
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        isPreparingEditMenu |= [sender isKindOfClass:UIMenuController.class];
        ALLOW_DEPRECATED_DECLARATIONS_END
        if (isPreparingEditMenu && editorState.selectionIsRange)
            return NO;
    }
#endif // ENABLE(IMAGE_ANALYSIS)

#if HAVE(UIFINDINTERACTION)
    if (action == @selector(useSelectionForFind:) || action == @selector(_findSelected:)) {
        if (!self.webView._findInteractionEnabled)
            return NO;

        if (!editorState.selectionIsRange || !self.selectedText.length)
            return NO;

        return YES;
    }
#endif

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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (sender == UIMenuController.sharedMenuController && [self _handleDOMPasteRequestWithResult:WebCore::DOMPasteAccessResponse::GrantedForGesture])
        return;
    ALLOW_DEPRECATED_DECLARATIONS_END

    _autocorrectionContextNeedsUpdate = YES;
    _page->executeEditCommand("paste"_s);
}

- (void)_pasteAsQuotationForWebView:(id)sender
{
    _autocorrectionContextNeedsUpdate = YES;
    _page->executeEditCommand("PasteAsQuotation"_s);
}

- (void)selectForWebView:(id)sender
{
    if (!_page->preferences().textInteractionEnabled())
        return;

    _autocorrectionContextNeedsUpdate = YES;
    [_textInteractionAssistant selectWord];
    // We cannot use selectWord command, because we want to be able to select the word even when it is the last in the paragraph.
    _page->extendSelection(WebCore::TextGranularity::WordGranularity);
}

- (void)selectAllForWebView:(id)sender
{
    if (!_page->preferences().textInteractionEnabled())
        return;

    _autocorrectionContextNeedsUpdate = YES;
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
    if (!_page->editorState().postLayoutData)
        return;
    CGRect presentationRect = _page->editorState().visualData->selectionGeometries[0].rect();
    if (_textInteractionAssistant)
        [_textInteractionAssistant showDictionaryFor:text fromRect:presentationRect];
}

- (void)_defineForWebView:(id)sender
{
    [self _lookupForWebView:sender];
}

- (void)accessibilityRetrieveSpeakSelectionContent
{
    RetainPtr<WKContentView> view = self;
    RetainPtr<WKWebView> webView = _webView.get();
    _page->getSelectionOrContentsAsString([view, webView](const String& string) {
        [webView _accessibilityDidGetSpeakSelectionContent:string];
        if ([view respondsToSelector:@selector(accessibilitySpeakSelectionSetContent:)])
            [view accessibilitySpeakSelectionSetContent:string];
    });
}

- (void)_accessibilityRetrieveRectsEnclosingSelectionOffset:(NSInteger)offset withGranularity:(UITextGranularity)granularity
{
    _page->requestRectsForGranularityWithSelectionOffset(toWKTextGranularity(granularity), offset, [view = retainPtr(self), offset, granularity](const Vector<WebCore::SelectionGeometry>& selectionGeometries) {
        if ([view respondsToSelector:@selector(_accessibilityDidGetSelectionRects:withGranularity:atOffset:)])
            [view _accessibilityDidGetSelectionRects:[view webSelectionRectsForSelectionGeometries:selectionGeometries] withGranularity:granularity atOffset:offset];
    });
}

- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text
{
    [self _accessibilityRetrieveRectsAtSelectionOffset:offset withText:text completionHandler:nil];
}

- (void)_accessibilityRetrieveRectsAtSelectionOffset:(NSInteger)offset withText:(NSString *)text completionHandler:(void (^)(const Vector<WebCore::SelectionGeometry>& geometries))completionHandler
{
    RetainPtr<WKContentView> view = self;
    _page->requestRectsAtSelectionOffsetWithText(offset, text, [view, offset, capturedCompletionHandler = makeBlockPtr(completionHandler)](const Vector<WebCore::SelectionGeometry>& selectionGeometries) {
        if (capturedCompletionHandler)
            capturedCompletionHandler(selectionGeometries);

        if ([view respondsToSelector:@selector(_accessibilityDidGetSelectionRects:withGranularity:atOffset:)])
            [view _accessibilityDidGetSelectionRects:[view webSelectionRectsForSelectionGeometries:selectionGeometries] withGranularity:UITextGranularityWord atOffset:offset];
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

static UIPasteboardName pasteboardNameForAccessCategory(WebCore::DOMPasteAccessCategory pasteAccessCategory)
{
    switch (pasteAccessCategory) {
    case WebCore::DOMPasteAccessCategory::General:
    case WebCore::DOMPasteAccessCategory::Fonts:
        return UIPasteboardNameGeneral;
    }
}

static UIPasteboard *pasteboardForAccessCategory(WebCore::DOMPasteAccessCategory pasteAccessCategory)
{
    switch (pasteAccessCategory) {
    case WebCore::DOMPasteAccessCategory::General:
    case WebCore::DOMPasteAccessCategory::Fonts:
        return UIPasteboard.generalPasteboard;
    }
}

- (BOOL)_handleDOMPasteRequestWithResult:(WebCore::DOMPasteAccessResponse)response
{
    if (auto pasteAccessCategory = std::exchange(_domPasteRequestCategory, std::nullopt)) {
        if (response == WebCore::DOMPasteAccessResponse::GrantedForCommand || response == WebCore::DOMPasteAccessResponse::GrantedForGesture)
            _page->grantAccessToCurrentPasteboardData(pasteboardNameForAccessCategory(*pasteAccessCategory));
    }

    if (auto pasteHandler = WTFMove(_domPasteRequestHandler)) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [UIMenuController.sharedMenuController hideMenuFromView:self];
        ALLOW_DEPRECATED_DECLARATIONS_END
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
    case UIWKGestureOneFingerDoubleTap:
        return WebKit::GestureType::OneFingerDoubleTap;
    case UIWKGestureOneFingerTripleTap:
        return WebKit::GestureType::OneFingerTripleTap;
    case UIWKGestureTwoFingerSingleTap:
        return WebKit::GestureType::TwoFingerSingleTap;
    case UIWKGesturePhraseBoundary:
        return WebKit::GestureType::PhraseBoundary;
    default:
        ASSERT_NOT_REACHED();
        return WebKit::GestureType::Loupe;
    }
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
    case WebKit::GestureType::OneFingerDoubleTap:
        return UIWKGestureOneFingerDoubleTap;
    case WebKit::GestureType::OneFingerTripleTap:
        return UIWKGestureOneFingerTripleTap;
    case WebKit::GestureType::TwoFingerSingleTap:
        return UIWKGestureTwoFingerSingleTap;
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
    if (flags.contains(WebKit::SelectionFlipped))
        uiFlags |= UIWKSelectionFlipped;
    if (flags.contains(WebKit::PhraseBoundaryChanged))
        uiFlags |= UIWKPhraseBoundaryChanged;

    return static_cast<UIWKSelectionFlags>(uiFlags);
}

static inline OptionSet<WebKit::SelectionFlags> toSelectionFlags(UIWKSelectionFlags uiFlags)
{
    OptionSet<WebKit::SelectionFlags> flags;
    if (uiFlags & UIWKWordIsNearTap)
        flags.add(WebKit::WordIsNearTap);
    if (uiFlags & UIWKSelectionFlipped)
        flags.add(WebKit::SelectionFlipped);
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

static void selectionChangedWithGesture(WKContentView *view, const WebCore::IntPoint& point, WebKit::GestureType gestureType, WebKit::GestureRecognizerState gestureState, OptionSet<WebKit::SelectionFlags> flags)
{
    [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType(gestureType) withState:toUIGestureRecognizerState(gestureState) withFlags:toUIWKSelectionFlags(flags)];
}

static void selectionChangedWithTouch(WKContentView *view, const WebCore::IntPoint& point, WebKit::SelectionTouch touch, OptionSet<WebKit::SelectionFlags> flags)
{
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
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;
    _page->selectWithGesture(WebCore::IntPoint(point), toGestureType(gestureType), toGestureRecognizerState(state), self._hasFocusedElement, [self, strongSelf = retainPtr(self), state, flags](const WebCore::IntPoint& point, WebKit::GestureType gestureType, WebKit::GestureRecognizerState gestureState, OptionSet<WebKit::SelectionFlags> innerFlags) {
        selectionChangedWithGesture(self, point, gestureType, gestureState, toSelectionFlags(flags) | innerFlags);
        if (state == UIGestureRecognizerStateEnded || state == UIGestureRecognizerStateCancelled)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart withFlags:(UIWKSelectionFlags)flags
{
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;
    _page->updateSelectionWithTouches(WebCore::IntPoint(point), toSelectionTouch(touch), baseIsStart, [self, strongSelf = retainPtr(self), flags](const WebCore::IntPoint& point, WebKit::SelectionTouch touch, OptionSet<WebKit::SelectionFlags> innerFlags) {
        selectionChangedWithTouch(self, point, touch, toSelectionFlags(flags) | innerFlags);
        if (toUIWKSelectionTouch(touch) != UIWKSelectionTouchStarted && toUIWKSelectionTouch(touch) != UIWKSelectionTouchMoved)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState
{
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;
    _page->selectWithTwoTouches(WebCore::IntPoint(from), WebCore::IntPoint(to), toGestureType(gestureType), toGestureRecognizerState(gestureState), [self, strongSelf = retainPtr(self)](const WebCore::IntPoint& point, WebKit::GestureType gestureType, WebKit::GestureRecognizerState gestureState, OptionSet<WebKit::SelectionFlags> flags) {
        selectionChangedWithGesture(self, point, gestureType, gestureState, flags);
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

#if HAVE(UI_EDIT_MENU_INTERACTION)

- (void)requestPreferredArrowDirectionForEditMenuWithCompletionHandler:(void(^)(UIEditMenuArrowDirection))completion
{
    [self _requestEvasionRectsAboveSelectionIfNeeded:[completion = makeBlockPtr(completion)](const Vector<WebCore::FloatRect>& rects) {
        completion(rects.isEmpty() ? UIEditMenuArrowDirectionAutomatic : UIEditMenuArrowDirectionUp);
    }];
}

#endif

- (void)requestRectsToEvadeForSelectionCommandsWithCompletionHandler:(void(^)(NSArray<NSValue *> *rects))completion
{
    [self _requestEvasionRectsAboveSelectionIfNeeded:[completion = makeBlockPtr(completion)](const Vector<WebCore::FloatRect>& rects) {
        completion(createNSArray(rects).get());
    }];
}

- (void)_requestEvasionRectsAboveSelectionIfNeeded:(CompletionHandler<void(const Vector<WebCore::FloatRect>&)>&&)completion
{
    if ([self _shouldSuppressSelectionCommands]) {
        completion({ });
        return;
    }

    auto requestRectsToEvadeIfNeeded = [startTime = ApproximateTime::now(), weakSelf = WeakObjCPtr<WKContentView>(self), completion = WTFMove(completion)]() mutable {
        auto strongSelf = weakSelf.get();
        if (!strongSelf) {
            completion({ });
            return;
        }

        if ([strongSelf webView]._editable) {
            completion({ });
            return;
        }

        auto focusedElementType = strongSelf->_focusedElementInformation.elementType;
        if (focusedElementType != WebKit::InputType::ContentEditable && focusedElementType != WebKit::InputType::TextArea) {
            completion({ });
            return;
        }

        // Give the page some time to present custom editing UI before attempting to detect and evade it.
        auto delayBeforeShowingCalloutBar = std::max(0_s, 0.25_s - (ApproximateTime::now() - startTime));
        WorkQueue::main().dispatchAfter(delayBeforeShowingCalloutBar, [completion = WTFMove(completion), weakSelf]() mutable {
            auto strongSelf = weakSelf.get();
            if (!strongSelf) {
                completion({ });
                return;
            }

            if (!strongSelf->_page) {
                completion({ });
                return;
            }

            strongSelf->_page->requestEvasionRectsAboveSelection(WTFMove(completion));
        });
    };

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    [self doAfterComputingImageAnalysisResultsForBackgroundRemoval:WTFMove(requestRectsToEvadeIfNeeded)];
#else
    requestRectsToEvadeIfNeeded();
#endif
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

- (UIMenu *)removeBackgroundMenu
{
    if (!_page || !_page->preferences().removeBackgroundEnabled())
        return nil;

    if (!_page->editorState().hasPostLayoutData() || !_removeBackgroundData)
        return nil;

    if (_removeBackgroundData->element != _page->editorState().postLayoutData->selectedEditableImage)
        return nil;

    return [self menuWithInlineAction:WebCore::contextMenuItemTitleRemoveBackground() image:[UIImage _systemImageNamed:@"circle.rectangle.filled.pattern.diagonalline"] identifier:@"WKActionRemoveBackground" handler:[](WKContentView *view) {
        auto data = std::exchange(view->_removeBackgroundData, { });
        if (!data)
            return;

        auto [elementContext, image, preferredMIMEType] = *data;
        if (auto [data, type] = WebKit::imageDataForRemoveBackground(image.get(), preferredMIMEType.createCFString().get()); data)
            view->_page->replaceImageForRemoveBackground(elementContext, { String { type.get() } }, { static_cast<const uint8_t*>([data bytes]), [data length] });
    }];
}

- (void)doAfterComputingImageAnalysisResultsForBackgroundRemoval:(CompletionHandler<void()>&&)completion
{
    if (!_page->editorState().hasPostLayoutData()) {
        completion();
        return;
    }

    auto elementToAnalyze = _page->editorState().postLayoutData->selectedEditableImage;
    if (_removeBackgroundData && _removeBackgroundData->element == elementToAnalyze) {
        completion();
        return;
    }

    _removeBackgroundData = std::nullopt;

    if (!elementToAnalyze) {
        completion();
        return;
    }

    _page->shouldAllowRemoveBackground(*elementToAnalyze, [context = *elementToAnalyze, completion = WTFMove(completion), weakSelf = WeakObjCPtr<WKContentView>(self)](bool shouldAllow) mutable {
        auto strongSelf = weakSelf.get();
        if (!shouldAllow || !strongSelf) {
            completion();
            return;
        }

        strongSelf->_page->requestImageBitmap(context, [context, completion = WTFMove(completion), weakSelf = WTFMove(weakSelf)](auto& imageData, auto& sourceMIMEType) mutable {
            auto strongSelf = weakSelf.get();
            if (!strongSelf) {
                completion();
                return;
            }

            auto imageBitmap = WebKit::ShareableBitmap::create(imageData);
            if (!imageBitmap) {
                completion();
                return;
            }

            auto cgImage = imageBitmap->makeCGImage();
            if (!cgImage) {
                completion();
                return;
            }

            WebKit::requestBackgroundRemoval(cgImage.get(), [sourceMIMEType, context, completion = WTFMove(completion), weakSelf](CGImageRef result) mutable {
                auto strongSelf = weakSelf.get();
                if (!strongSelf) {
                    completion();
                    return;
                }

                if (result)
                    strongSelf->_removeBackgroundData = { { context, { result }, sourceMIMEType } };
                else
                    strongSelf->_removeBackgroundData = std::nullopt;
                completion();
            });
        });
    });
}

- (BOOL)_handleTapOverImageAnalysisInteractionButton:(CGPoint)position
{
    if (!_imageAnalysisInteraction)
        return NO;

    auto *hitButton = dynamic_objc_cast<UIButton>([self hitTest:position withEvent:nil]);
    UIButton *analysisButton = [_imageAnalysisInteraction analysisButton];
    // FIXME: Instead of a class check, this should be a straightforward equality check.
    // However, rdar://91828384 currently prevents us from doing so. We can simplify this logic
    // once the fix for that bug has landed.
    if (analysisButton && hitButton.class == analysisButton.class) {
        [_imageAnalysisInteraction setHighlightSelectableItems:![_imageAnalysisInteraction highlightSelectableItems]];
        return YES;
    }

    if ([_imageAnalysisActionButtons containsObject:hitButton]) {
        [hitButton sendActionsForControlEvents:UIControlEventTouchUpInside];
        return YES;
    }

    return NO;
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

- (void)selectPositionAtPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    [self _selectPositionAtPoint:point stayingWithinFocusedElement:self._hasFocusedElement completionHandler:completionHandler];
}

- (void)_selectPositionAtPoint:(CGPoint)point stayingWithinFocusedElement:(BOOL)stayingWithinFocusedElement completionHandler:(void (^)(void))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;

    _page->selectPositionAtPoint(WebCore::IntPoint(point), stayingWithinFocusedElement, [view = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
        view->_usingGestureForSelection = NO;
    });
}

- (void)selectPositionAtBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction fromPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;
    _page->selectPositionAtBoundaryWithDirection(WebCore::IntPoint(point), toWKTextGranularity(granularity), toWKSelectionDirection(direction), self._hasFocusedElement, [view = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)]() {
        completionHandler();
        view->_usingGestureForSelection = NO;
    });
}

- (void)moveSelectionAtBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction completionHandler:(void (^)(void))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;
    _page->moveSelectionAtBoundaryWithDirection(toWKTextGranularity(granularity), toWKSelectionDirection(direction), [view = retainPtr(self), completionHandler = makeBlockPtr(completionHandler)] {
        completionHandler();
        view->_usingGestureForSelection = NO;
    });
}

- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;
    ++_suppressNonEditableSingleTapTextInteractionCount;
    _page->selectTextWithGranularityAtPoint(WebCore::IntPoint(point), toWKTextGranularity(granularity), self._hasFocusedElement, [view = retainPtr(self), selectionHandler = makeBlockPtr(completionHandler)] {
        selectionHandler();
        view->_usingGestureForSelection = NO;
        --view->_suppressNonEditableSingleTapTextInteractionCount;
    });
}

- (void)beginSelectionInDirection:(UITextDirection)direction completionHandler:(void (^)(BOOL endIsMoving))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    _page->beginSelectionInDirection(toWKSelectionDirection(direction), [selectionHandler = makeBlockPtr(completionHandler)] (bool endIsMoving) {
        selectionHandler(endIsMoving);
    });
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point completionHandler:(void (^)(BOOL endIsMoving))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    auto respectSelectionAnchor = self.interactionAssistant._wk_hasFloatingCursor ? WebKit::RespectSelectionAnchor::Yes : WebKit::RespectSelectionAnchor::No;
    _page->updateSelectionWithExtentPoint(WebCore::IntPoint(point), self._hasFocusedElement, respectSelectionAnchor, [selectionHandler = makeBlockPtr(completionHandler)](bool endIsMoving) {
        selectionHandler(endIsMoving);
    });
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity completionHandler:(void (^)(BOOL selectionEndIsMoving))completionHandler
{
    _autocorrectionContextNeedsUpdate = YES;
    ++_suppressNonEditableSingleTapTextInteractionCount;
    _page->updateSelectionWithExtentPointAndBoundary(WebCore::IntPoint(point), toWKTextGranularity(granularity), self._hasFocusedElement, [completionHandler = makeBlockPtr(completionHandler), protectedSelf = retainPtr(self)] (bool endIsMoving) {
        completionHandler(endIsMoving);
        --protectedSelf->_suppressNonEditableSingleTapTextInteractionCount;
    });
}

- (UTF32Char)_characterInRelationToCaretSelection:(int)amount
{
    if (amount == -1 && _lastInsertedCharacterToOverrideCharacterBeforeSelection)
        return *_lastInsertedCharacterToOverrideCharacterBeforeSelection;

    auto& state = _page->editorState();
    if (!state.isContentEditable || state.selectionIsNone || state.selectionIsRange || !state.postLayoutData)
        return 0;

    switch (amount) {
    case 0:
        return state.postLayoutData->characterAfterSelection;
    case -1:
        return state.postLayoutData->characterBeforeSelection;
    case -2:
        return state.postLayoutData->twoCharacterBeforeSelection;
    default:
        return 0;
    }
}

- (BOOL)_selectionAtDocumentStart
{
    if (!_page->editorState().postLayoutData)
        return NO;
    return !_page->editorState().postLayoutData->characterBeforeSelection;
}

- (CGRect)textFirstRect
{
    auto& editorState = _page->editorState();
    if (editorState.hasComposition) {
        if (!editorState.hasVisualData())
            return CGRectZero;
        auto& markedTextRects = editorState.visualData->markedTextRects;
        return markedTextRects.isEmpty() ? CGRectZero : markedTextRects.first().rect();
    }
    return _autocorrectionData.textFirstRect;
}

- (CGRect)textLastRect
{
    auto& editorState = _page->editorState();
    if (editorState.hasComposition) {
        if (!editorState.hasVisualData())
            return CGRectZero;
        auto& markedTextRects = editorState.visualData->markedTextRects;
        return markedTextRects.isEmpty() ? CGRectZero : markedTextRects.last().rect();
    }
    return _autocorrectionData.textLastRect;
}

- (void)willInsertFinalDictationResult
{
    _page->willInsertFinalDictationResult();
}

- (void)didInsertFinalDictationResult
{
    _page->didInsertFinalDictationResult();
}

- (void)replaceDictatedText:(NSString*)oldText withText:(NSString *)newText
{
    if (_isHidingKeyboard && _isChangingFocus)
        return;

    _autocorrectionContextNeedsUpdate = YES;
    _page->replaceDictatedText(oldText, newText);
}

- (void)requestDictationContext:(void (^)(NSString *selectedText, NSString *beforeText, NSString *afterText))completionHandler
{
    _page->requestDictationContext([dictationHandler = makeBlockPtr(completionHandler)](const String& selectedText, const String& beforeText, const String& afterText) {
        dictationHandler(selectedText, beforeText, afterText);
    });
}

// The completion handler should pass the rect of the correction text after replacing the input text, or nil if the replacement could not be performed.
- (void)applyAutocorrection:(NSString *)correction toString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForCorrection))completionHandler
{
    if ([self _disableAutomaticKeyboardUI]) {
        if (completionHandler)
            completionHandler(nil);
        return;
    }

    // FIXME: Remove the synchronous call when <rdar://problem/16207002> is fixed.
    const bool useSyncRequest = true;

    if (useSyncRequest) {
        if (completionHandler)
            completionHandler(_page->applyAutocorrection(correction, input) ? [WKAutocorrectionRects autocorrectionRectsWithFirstCGRect:_autocorrectionData.textFirstRect lastCGRect:_autocorrectionData.textLastRect] : nil);
        return;
    }

    _page->applyAutocorrection(correction, input, [view = retainPtr(self), completion = makeBlockPtr(completionHandler)](auto& string) {
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

    if ([self _disableAutomaticKeyboardUI]) {
        completionHandler(WKAutocorrectionContext.emptyAutocorrectionContext);
        return;
    }

    if (!_page->hasRunningProcess()) {
        completionHandler(WKAutocorrectionContext.emptyAutocorrectionContext);
        return;
    }

    bool respondWithLastKnownAutocorrectionContext = ([&] {
        if (_page->shouldAvoidSynchronouslyWaitingToPreventDeadlock())
            return true;

        if (_domPasteRequestHandler)
            return true;

        if (_isUnsuppressingSoftwareKeyboardUsingLastAutocorrectionContext)
            return true;

        return !_autocorrectionContextNeedsUpdate;
    })();

    if (respondWithLastKnownAutocorrectionContext) {
        completionHandler([WKAutocorrectionContext autocorrectionContextWithWebContext:_lastAutocorrectionContext]);
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

    if (!useSyncRequest)
        return;

    if (!_page->process().connection()->waitForAndDispatchImmediately<Messages::WebPageProxy::HandleAutocorrectionContext>(_page->webPageID(), 1_s, IPC::WaitForOption::DispatchIncomingSyncMessagesWhileWaiting))
        RELEASE_LOG(TextInput, "Timed out while waiting for autocorrection context.");

    if (_autocorrectionContextNeedsUpdate)
        [self _cancelPendingAutocorrectionContextHandler];
    else
        [self _invokePendingAutocorrectionContextHandler:[WKAutocorrectionContext autocorrectionContextWithWebContext:_lastAutocorrectionContext]];
}

- (void)_handleAutocorrectionContext:(const WebKit::WebAutocorrectionContext&)context
{
    _lastAutocorrectionContext = context;
    _autocorrectionContextNeedsUpdate = NO;
    [self unsuppressSoftwareKeyboardUsingLastAutocorrectionContextIfNeeded];
}

- (void)updateSoftwareKeyboardSuppressionStateFromWebView
{
    BOOL webViewIsSuppressingSoftwareKeyboard = [_webView _suppressSoftwareKeyboard];
    if (webViewIsSuppressingSoftwareKeyboard) {
        _unsuppressSoftwareKeyboardAfterNextAutocorrectionContextUpdate = NO;
        self._suppressSoftwareKeyboard = webViewIsSuppressingSoftwareKeyboard;
        return;
    }

    if (self._suppressSoftwareKeyboard == webViewIsSuppressingSoftwareKeyboard)
        return;

    if (!std::exchange(_unsuppressSoftwareKeyboardAfterNextAutocorrectionContextUpdate, YES))
        _page->requestAutocorrectionContext();
}

- (void)unsuppressSoftwareKeyboardUsingLastAutocorrectionContextIfNeeded
{
    if (!std::exchange(_unsuppressSoftwareKeyboardAfterNextAutocorrectionContextUpdate, NO))
        return;

    SetForScope unsuppressSoftwareKeyboardScope { _isUnsuppressingSoftwareKeyboardUsingLastAutocorrectionContext, YES };
    self._suppressSoftwareKeyboard = NO;
}

- (void)runModalJavaScriptDialog:(CompletionHandler<void()>&&)callback
{
    if (_isFocusingElementWithKeyboard)
        _pendingRunModalJavaScriptDialogCallback = WTFMove(callback);
    else
        callback();
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
#if ENABLE(IMAGE_ANALYSIS)
    [self _cancelImageAnalysis];
#endif
}

- (void)_didCommitLoadForMainFrame
{
    _seenHardwareKeyDownInNonEditableElement = NO;

    [self _elementDidBlur];
    [self _cancelLongPressGestureRecognizer];
    [self _removeContainerForContextMenuHintPreviews];
    [self _removeContainerForDragPreviews];
    [self _removeContainerForDropPreviews];
    [_webView _didCommitLoadForMainFrame];

    _textInteractionDidChangeFocusedElement = NO;
    _activeTextInteractionCount = 0;
    _treatAsContentEditableUntilNextEditorStateUpdate = NO;
    [self _invalidateCurrentPositionInformation];

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    [self uninstallImageAnalysisInteraction];
#endif
}

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
        if (completionHandler)
            completionHandler([protectedSelf becomeFirstResponder]);
    });
}

- (WebCore::Color)_tapHighlightColorForFastClick:(BOOL)forFastClick
{
    ASSERT(_showDebugTapHighlightsForFastClicking);
    return forFastClick ? WebCore::SRGBA<uint8_t> { 0, 225, 0, 127 } : WebCore::SRGBA<uint8_t> { 225, 0, 0, 127 };
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
    _page->setFocusedElementValue(_focusedElementInformation.elementContext, { });
}

- (void)accessoryDone
{
    [self stopRelinquishingFirstResponderToFocusedElement];
    [self endEditingAndUpdateFocusAppearanceWithReason:EndEditingReasonAccessoryDone];
    _page->setIsShowingInputViewForFocusedElement(false);
}

- (void)updateFocusedElementValue:(NSString *)value
{
    _page->setFocusedElementValue(_focusedElementInformation.elementContext, value);
    _focusedElementInformation.value = value;
}

- (void)updateFocusedElementValueAsColor:(UIColor *)value
{
    WebCore::Color color(WebCore::roundAndClampToSRGBALossy(value.CGColor));
    String valueAsString = WebCore::serializationForHTML(color);

    _page->setFocusedElementValue(_focusedElementInformation.elementContext, valueAsString);
    _focusedElementInformation.value = valueAsString;
    _focusedElementInformation.colorValue = color;
}

- (void)updateFocusedElementSelectedIndex:(uint32_t)index allowsMultipleSelection:(bool)allowsMultipleSelection
{
    _page->setFocusedElementSelectedIndex(_focusedElementInformation.elementContext, index, allowsMultipleSelection);
}

- (void)updateFocusedElementFocusedWithDataListDropdown:(BOOL)value
{
    _focusedElementInformation.isFocusingWithDataListDropdown = value;
    [self reloadInputViews];
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
    if (WebKit::defaultAlternateFormControlDesignEnabled())
        return nil;

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

    if (!WebKit::currentUserInterfaceIdiomIsSmallScreen()) {
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
    _autocorrectionContextNeedsUpdate = YES;
    _selectionChangeNestingLevel++;

    [self.inputDelegate selectionWillChange:self];
}

- (void)endSelectionChange
{
    [self.inputDelegate selectionDidChange:self];

    if (_selectionChangeNestingLevel) {
        // FIXME (228083): Some layout tests end up triggering unbalanced calls to -endSelectionChange.
        // We should investigate why this happens, (ideally) prevent it from happening, and then assert
        // that `_selectionChangeNestingLevel` is nonzero when calling -endSelectionChange.
        _selectionChangeNestingLevel--;
    }
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
    _autocorrectionContextNeedsUpdate = YES;
    // FIXME: Replace NSClassFromString with actual class as soon as UIKit submitted the new class into the iOS SDK.
    if ([textSuggestion isKindOfClass:[UITextAutofillSuggestion class]]) {
        _page->autofillLoginCredentials([(UITextAutofillSuggestion *)textSuggestion username], [(UITextAutofillSuggestion *)textSuggestion password]);
        return;
    }
#if ENABLE(DATALIST_ELEMENT)
    if ([textSuggestion isKindOfClass:[WKDataListTextSuggestion class]]) {
        _page->setFocusedElementValue(_focusedElementInformation.elementContext, [textSuggestion inputText]);
        return;
    }
#endif
    id <_WKInputDelegate> inputDelegate = [_webView _inputDelegate];
    if ([inputDelegate respondsToSelector:@selector(_webView:insertTextSuggestion:inInputSession:)])
        [inputDelegate _webView:self.webView insertTextSuggestion:textSuggestion inInputSession:_formInputSession.get()];
}

- (NSString *)textInRange:(UITextRange *)range
{
    if (!_page)
        return nil;

    auto& editorState = _page->editorState();
    if (self.selectedTextRange == range && editorState.hasPostLayoutData() && editorState.selectionIsRange)
        return editorState.postLayoutData->wordAtSelection;

    return nil;
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text
{
}

static NSArray<WKTextSelectionRect *> *textSelectionRects(const Vector<WebCore::SelectionGeometry>& rects, CGFloat scaleFactor)
{
    return createNSArray(rects, [scaleFactor] (auto& rect) {
        return adoptNS([[WKTextSelectionRect alloc] initWithSelectionGeometry:rect scaleFactor:scaleFactor]);
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
    if (!hasSelection || !editorState.hasPostLayoutAndVisualData())
        return nil;

    auto isRange = editorState.selectionIsRange;
    auto isContentEditable = editorState.isContentEditable;
    // UIKit does not expect caret selections in non-editable content.
    if (!isContentEditable && !isRange)
        return nil;

    if (_cachedSelectedTextRange)
        return _cachedSelectedTextRange.get();

    auto caretStartRect = [self _scaledCaretRectForSelectionStart:_page->editorState().visualData->caretRectAtStart];
    auto caretEndRect = [self _scaledCaretRectForSelectionEnd:_page->editorState().visualData->caretRectAtEnd];
    auto selectionRects = textSelectionRects(_page->editorState().visualData->selectionGeometries, self._contentZoomScale);
    auto selectedTextLength = editorState.postLayoutData->selectedTextLength;

    _cachedSelectedTextRange = [WKTextRange textRangeWithState:!hasSelection isRange:isRange isEditable:isContentEditable startRect:caretStartRect endRect:caretEndRect selectionRects:selectionRects selectedTextLength:selectedTextLength];
    return _cachedSelectedTextRange.get();
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
    if (!hasComposition || !editorState.hasPostLayoutAndVisualData())
        return nil;
    auto& postLayoutData = *editorState.postLayoutData;
    auto& visualData = *editorState.visualData;
    auto unscaledCaretRectAtStart = visualData.markedTextCaretRectAtStart;
    auto unscaledCaretRectAtEnd = visualData.markedTextCaretRectAtEnd;
    auto isRange = unscaledCaretRectAtStart != unscaledCaretRectAtEnd;
    auto isContentEditable = editorState.isContentEditable;
    auto caretStartRect = [self _scaledCaretRectForSelectionStart:unscaledCaretRectAtStart];
    auto caretEndRect = [self _scaledCaretRectForSelectionEnd:unscaledCaretRectAtEnd];
    auto selectionRects = textSelectionRects(visualData.markedTextRects, self._contentZoomScale);
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
            highlightColor = WebCore::colorFromCocoaColor(uiColor);
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
    _autocorrectionContextNeedsUpdate = YES;
    _candidateViewNeedsUpdate = !self.hasMarkedText;
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

- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)to
{
#if HAVE(UIFINDINTERACTION)
    if ([from isKindOfClass:[WKFoundTextPosition class]] && [to isKindOfClass:[WKFoundTextPosition class]]) {
        WKFoundTextPosition *fromPosition = (WKFoundTextPosition *)from;
        WKFoundTextPosition *toPosition = (WKFoundTextPosition *)to;

        if (fromPosition.order == toPosition.order)
            return fromPosition.offset - toPosition.offset;

        return fromPosition.order - toPosition.order;
    }
#endif

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
    if ([self _currentPositionInformationIsApproximatelyValidForRequest:request radiusForApproximation:2] && _positionInformation.isSelectable())
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
#if HAVE(PENCILKIT_TEXT_INPUT)
    return [_scribbleInteraction isHandlingWriting];
#else
    return NO;
#endif
}

// Inserts the given string, replacing any selected or marked text.
- (void)insertText:(NSString *)aStringValue
{
    if (_isHidingKeyboard && _isChangingFocus)
        return;

    auto* keyboard = [UIKeyboardImpl sharedInstance];

    WebKit::InsertTextOptions options;
    options.processingUserGesture = keyboard.isCallingInputDelegate;
    options.shouldSimulateKeyboardInput = [self _shouldSimulateKeyboardInputOnTextInsertion];
    _page->insertTextAsync(aStringValue, WebKit::EditingRange(), WTFMove(options));

    if (_focusedElementInformation.autocapitalizeType == WebCore::AutocapitalizeType::Words && aStringValue.length) {
        _lastInsertedCharacterToOverrideCharacterBeforeSelection = [aStringValue characterAtIndex:aStringValue.length - 1];
        _page->scheduleFullEditorStateUpdate();
    }

    _autocorrectionContextNeedsUpdate = YES;
}

- (void)insertText:(NSString *)aStringValue alternatives:(NSArray<NSString *> *)alternatives style:(UITextAlternativeStyle)style
{
    if (!alternatives.count) {
        [self insertText:aStringValue];
        return;
    }

    BOOL isLowConfidence = style == UITextAlternativeStyleLowConfidence;
    auto textAlternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:aStringValue alternativeStrings:alternatives isLowConfidence:isLowConfidence]);
    WebCore::TextAlternativeWithRange textAlternativeWithRange { textAlternatives.get(), NSMakeRange(0, aStringValue.length) };

    WebKit::InsertTextOptions options;
    options.shouldSimulateKeyboardInput = [self _shouldSimulateKeyboardInputOnTextInsertion];
    _page->insertDictatedTextAsync(aStringValue, { }, { textAlternativeWithRange }, WTFMove(options));

    _autocorrectionContextNeedsUpdate = YES;
}

- (BOOL)hasText
{
    auto& editorState = _page->editorState();
    return editorState.hasPostLayoutData() && editorState.postLayoutData->hasPlainText;
}

- (void)addTextAlternatives:(NSTextAlternatives *)alternatives
{
    _page->addDictationAlternative({ alternatives, NSMakeRange(0, alternatives.primaryString.length) });
}

- (void)removeEmojiAlternatives
{
    _page->dictationAlternativesAtSelection([weakSelf = WeakObjCPtr<WKContentView>(self)](auto&& contexts) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || !strongSelf->_page)
            return;

        RefPtr page = strongSelf->_page;
        Vector<WebCore::DictationContext> contextsToRemove;
        for (auto context : contexts) {
            auto alternatives = page->platformDictationAlternatives(context);
            if (!alternatives)
                continue;

            auto originalAlternatives = alternatives.alternativeStrings;
            auto nonEmojiAlternatives = [NSMutableArray arrayWithCapacity:originalAlternatives.count];
            for (NSString *alternative in originalAlternatives) {
                if (!alternative._containsEmojiOnly)
                    [nonEmojiAlternatives addObject:alternative];
            }

            if (nonEmojiAlternatives.count == originalAlternatives.count)
                continue;

            RetainPtr<NSTextAlternatives> replacement;
            if (nonEmojiAlternatives.count)
                replacement = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:alternatives.primaryString alternativeStrings:nonEmojiAlternatives isLowConfidence:alternatives.isLowConfidence]);
            else
                contextsToRemove.append(context);

            page->pageClient().replaceDictationAlternatives(replacement.get(), context);
        }
        page->clearDictationAlternatives(WTFMove(contextsToRemove));
    });
}

// end of UITextInput protocol implementation

static UITextAutocapitalizationType toUITextAutocapitalize(WebCore::AutocapitalizeType webkitType)
{
    switch (webkitType) {
    case WebCore::AutocapitalizeType::Default:
        return UITextAutocapitalizationTypeSentences;
    case WebCore::AutocapitalizeType::None:
        return UITextAutocapitalizationTypeNone;
    case WebCore::AutocapitalizeType::Words:
        return UITextAutocapitalizationTypeWords;
    case WebCore::AutocapitalizeType::Sentences:
        return UITextAutocapitalizationTypeSentences;
    case WebCore::AutocapitalizeType::AllCharacters:
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

    // Do not change traits when dismissing the keyboard.
    if (_isBlurringFocusedElement)
        return _traits.get();

    [self _updateTextInputTraits:_traits.get()];
    return _traits.get();
}

- (void)_updateTextInputTraits:(id <UITextInputTraits>)traits
{
    traits.secureTextEntry = _focusedElementInformation.elementType == WebKit::InputType::Password || [_formInputSession forceSecureTextEntry];

    switch (_focusedElementInformation.enterKeyHint) {
    case WebCore::EnterKeyHint::Enter:
        traits.returnKeyType = UIReturnKeyDefault;
        break;
    case WebCore::EnterKeyHint::Done:
        traits.returnKeyType = UIReturnKeyDone;
        break;
    case WebCore::EnterKeyHint::Go:
        traits.returnKeyType = UIReturnKeyGo;
        break;
    case WebCore::EnterKeyHint::Next:
        traits.returnKeyType = UIReturnKeyNext;
        break;
    case WebCore::EnterKeyHint::Search:
        traits.returnKeyType = UIReturnKeySearch;
        break;
    case WebCore::EnterKeyHint::Send:
        traits.returnKeyType = UIReturnKeySend;
        break;
    default: {
        if (!_focusedElementInformation.formAction.isEmpty())
            traits.returnKeyType = _focusedElementInformation.elementType == WebKit::InputType::Search ? UIReturnKeySearch : UIReturnKeyGo;
    }
    }

    BOOL disableAutocorrectAndAutocapitalize = _focusedElementInformation.elementType == WebKit::InputType::Password || _focusedElementInformation.elementType == WebKit::InputType::Email
        || _focusedElementInformation.elementType == WebKit::InputType::URL || _focusedElementInformation.formAction.contains("login"_s);
    if ([traits respondsToSelector:@selector(setAutocapitalizationType:)])
        traits.autocapitalizationType = disableAutocorrectAndAutocapitalize ? UITextAutocapitalizationTypeNone : toUITextAutocapitalize(_focusedElementInformation.autocapitalizeType);
    if ([traits respondsToSelector:@selector(setAutocorrectionType:)])
        traits.autocorrectionType = disableAutocorrectAndAutocapitalize ? UITextAutocorrectionTypeNo : (_focusedElementInformation.isAutocorrect ? UITextAutocorrectionTypeYes : UITextAutocorrectionTypeNo);

    if (!_focusedElementInformation.isSpellCheckingEnabled) {
        if ([traits respondsToSelector:@selector(setSmartQuotesType:)])
            traits.smartQuotesType = UITextSmartQuotesTypeNo;
        if ([traits respondsToSelector:@selector(setSmartDashesType:)])
            traits.smartDashesType = UITextSmartDashesTypeNo;
        if ([traits respondsToSelector:@selector(setSpellCheckingType:)])
            traits.spellCheckingType = UITextSpellCheckingTypeNo;
    }

    switch (_focusedElementInformation.inputMode) {
    case WebCore::InputMode::None:
    case WebCore::InputMode::Unspecified:
        switch (_focusedElementInformation.elementType) {
        case WebKit::InputType::Phone:
            traits.keyboardType = UIKeyboardTypePhonePad;
            break;
        case WebKit::InputType::URL:
            traits.keyboardType = UIKeyboardTypeURL;
            break;
        case WebKit::InputType::Email:
            traits.keyboardType = UIKeyboardTypeEmailAddress;
            break;
        case WebKit::InputType::Number:
            traits.keyboardType = UIKeyboardTypeNumbersAndPunctuation;
            break;
        case WebKit::InputType::NumberPad:
            traits.keyboardType = UIKeyboardTypeNumberPad;
            break;
        case WebKit::InputType::None:
        case WebKit::InputType::ContentEditable:
        case WebKit::InputType::Text:
        case WebKit::InputType::Password:
        case WebKit::InputType::TextArea:
        case WebKit::InputType::Search:
        case WebKit::InputType::Date:
        case WebKit::InputType::DateTimeLocal:
        case WebKit::InputType::Month:
        case WebKit::InputType::Week:
        case WebKit::InputType::Time:
        case WebKit::InputType::Select:
        case WebKit::InputType::Drawing:
#if ENABLE(INPUT_TYPE_COLOR)
        case WebKit::InputType::Color:
#endif
            traits.keyboardType = UIKeyboardTypeDefault;
        }
        break;
    case WebCore::InputMode::Text:
        traits.keyboardType = UIKeyboardTypeDefault;
        break;
    case WebCore::InputMode::Telephone:
        traits.keyboardType = UIKeyboardTypePhonePad;
        break;
    case WebCore::InputMode::Url:
        traits.keyboardType = UIKeyboardTypeURL;
        break;
    case WebCore::InputMode::Email:
        traits.keyboardType = UIKeyboardTypeEmailAddress;
        break;
    case WebCore::InputMode::Numeric:
        traits.keyboardType = UIKeyboardTypeNumberPad;
        break;
    case WebCore::InputMode::Decimal:
        traits.keyboardType = UIKeyboardTypeDecimalPad;
        break;
    case WebCore::InputMode::Search:
        traits.keyboardType = UIKeyboardTypeWebSearch;
        break;
    }

#if HAVE(PEPPER_UI_CORE)
    traits.textContentType = self.textContentTypeForQuickboard;
#else
    traits.textContentType = contentTypeFromFieldName(_focusedElementInformation.autofillFieldName);
#endif

    auto privateTraits = (id <UITextInputTraits_Private>)traits;
    if ([privateTraits respondsToSelector:@selector(setIsSingleLineDocument:)]) {
        switch (_focusedElementInformation.elementType) {
        case WebKit::InputType::ContentEditable:
        case WebKit::InputType::TextArea:
            privateTraits.isSingleLineDocument = NO;
            break;
#if ENABLE(INPUT_TYPE_COLOR)
        case WebKit::InputType::Color:
#endif
        case WebKit::InputType::Date:
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
            privateTraits.isSingleLineDocument = YES;
            break;
        case WebKit::InputType::None:
            break;
        }
    }

    if ([privateTraits respondsToSelector:@selector(setShortcutConversionType:)])
        privateTraits.shortcutConversionType = _focusedElementInformation.elementType == WebKit::InputType::Password ? UITextShortcutConversionTypeNo : UITextShortcutConversionTypeDefault;

    if ([traits isKindOfClass:UITextInputTraits.class])
        [self _updateInteractionTintColor:(UITextInputTraits *)traits];
}

- (UITextInteractionAssistant *)interactionAssistant
{
    return _textInteractionAssistant.get();
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
    _autocorrectionContextNeedsUpdate = YES;
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
    WebCore::FloatRect searchRect { rect };
#if ENABLE(EDITABLE_REGION)
    bool hitInteractionRect = self._hasFocusedElement && searchRect.inclusivelyIntersects(_focusedElementInformation.interactionRect);
    if (!self.webView._editable && !hitInteractionRect && !WebKit::mayContainEditableElementsInRect(self, rect)) {
        completionHandler(@[ ]);
        return;
    }
#endif
    _page->textInputContextsInRect(searchRect, [weakSelf = WeakObjCPtr<WKContentView>(self), completionHandler = makeBlockPtr(completionHandler)] (const Vector<WebCore::ElementContext>& contexts) {
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

    _page->setCanShowPlaceholder(context._textInputContext, false);

    ++_activeTextInteractionCount;
    if (_activeTextInteractionCount > 1)
        return;

    _textInteractionDidChangeFocusedElement = NO;
    _page->setShouldRevealCurrentSelectionAfterInsertion(false);
    _autocorrectionContextNeedsUpdate = YES;
    _usingGestureForSelection = YES;
}

- (void)_didFinishTextInteractionInTextInputContext:(_WKTextInputContext *)context
{
    ASSERT(context);

    _page->setCanShowPlaceholder(context._textInputContext, true);

    ASSERT(_activeTextInteractionCount > 0);
    --_activeTextInteractionCount;
    if (_activeTextInteractionCount)
        return;

    _usingGestureForSelection = NO;

    if (_textInteractionDidChangeFocusedElement) {
        // Mark to zoom to reveal the newly focused element on the next editor state update.
        // Then tell the web process to reveal the current selection, which will send us (the
        // UI process) an editor state update.
        _page->setWaitingForPostLayoutEditorStateUpdateAfterFocusingElement(true);
        _textInteractionDidChangeFocusedElement = NO;
    }

    _page->setShouldRevealCurrentSelectionAfterInsertion(true);
}

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

// Web events.
- (BOOL)requiresKeyEvents
{
    return YES;
}

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
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
        if (!_seenHardwareKeyDownInNonEditableElement) {
            _seenHardwareKeyDownInNonEditableElement = YES;
            [self reloadInputViews];
        }
        [super _handleKeyUIEvent:event];
        return;
    }

    [super _handleKeyUIEvent:event];
}
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

- (void)generateSyntheticEditingCommand:(WebKit::SyntheticEditingCommandType)command
{
    _page->generateSyntheticEditingCommand(command);
}

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
    auto* keyboard = [UIKeyboardImpl sharedInstance];
    if ((_page->editorState().isContentEditable || _treatAsContentEditableUntilNextEditorStateUpdate) && [keyboard handleKeyInputMethodCommandForCurrentEvent]) {
        completionHandler(theEvent, YES);
        _page->handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(theEvent, HandledByInputMethod::Yes));
        return;
    }

    if (_page->handleKeyboardEvent(WebKit::NativeWebKeyboardEvent(theEvent, HandledByInputMethod::No)))
        _keyWebEventHandler = makeBlockPtr(completionHandler);
    else
        completionHandler(theEvent, NO);
}

- (void)_didHandleKeyEvent:(::WebEvent *)event eventWasHandled:(BOOL)eventWasHandled
{
    if ([event isKindOfClass:[WKSyntheticFlagsChangedWebEvent class]])
        return;

    if (!(event.keyboardFlags & WebEventKeyboardInputModifierFlagsChanged))
        [_keyboardScrollingAnimator handleKeyEvent:event];
    
    if (auto handler = WTFMove(_keyWebEventHandler)) {
        handler(event, eventWasHandled);
        return;
    }
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

    if (!isCharEvent && [keyboard handleKeyTextCommandForCurrentEvent])
        return YES;
    if (isCharEvent && [keyboard handleKeyAppCommandForCurrentEvent])
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

- (NSArray<NSString *> *)filePickerAcceptedTypeIdentifiers
{
    if (!_fileUploadPanel)
        return @[];

    return [_fileUploadPanel acceptedTypeIdentifiers];
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

- (CGFloat)keyboardScrollViewAnimator:(WKKeyboardScrollViewAnimator *)animator distanceForIncrement:(WebCore::ScrollGranularity)increment inDirection:(WebCore::ScrollDirection)direction
{
    BOOL directionIsHorizontal = direction == WebCore::ScrollDirection::ScrollLeft || direction == WebCore::ScrollDirection::ScrollRight;

    switch (increment) {
    case WebCore::ScrollGranularity::Document: {
        CGSize documentSize = [self convertRect:self.bounds toView:self.webView].size;
        return directionIsHorizontal ? documentSize.width : documentSize.height;
    }
    case WebCore::ScrollGranularity::Page: {
        CGSize pageSize = [self convertSize:CGSizeMake(0, WebCore::Scrollbar::pageStep(_page->unobscuredContentRect().height(), self.bounds.size.height)) toView:self.webView];
        return directionIsHorizontal ? pageSize.width : pageSize.height;
    }
    case WebCore::ScrollGranularity::Line:
        return [self convertSize:CGSizeMake(0, WebCore::Scrollbar::pixelsPerLineStep()) toView:self.webView].height;
    case WebCore::ScrollGranularity::Pixel:
        return 0;
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
    [_webView _didFinishScrolling:self.webView.scrollView];
}

- (void)executeEditCommandWithCallback:(NSString *)commandName
{
    _autocorrectionContextNeedsUpdate = YES;
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

- (void)_deleteForwardByWord
{
    [self executeEditCommandWithCallback:@"deleteWordForward"];
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
    auto& editorState = _page->editorState();
    return !editorState.selectionIsNone && editorState.postLayoutData && editorState.postLayoutData->hasContent;
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
        if (!_page->editorState().postLayoutData)
            return NO;

        if (direction == UITextStorageDirectionBackward && [position isEqual:self.selectedTextRange.start])
            return _page->editorState().postLayoutData->selectionStartIsAtParagraphBoundary;

        if (direction == UITextStorageDirectionForward && [position isEqual:self.selectedTextRange.end])
            return _page->editorState().postLayoutData->selectionEndIsAtParagraphBoundary;
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
    SetForScope isHidingKeyboardScope { _isHidingKeyboard, YES };

    self.inputDelegate = nil;
    [self setUpTextSelectionAssistant];
    
    [_textInteractionAssistant deactivateSelection];
    [_formAccessoryView hideAutoFillButton];

    // FIXME: Does it make sense to call -reloadInputViews on watchOS?
    [self reloadInputViews];
    if (_formAccessoryView)
        [self _updateAccessory];
}

#if ENABLE(IOS_FORM_CONTROL_REFRESH)

- (BOOL)_formControlRefreshEnabled
{
    if (!_page)
        return NO;

    return _page->preferences().iOSFormControlRefreshEnabled();
}

#endif

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
    case WebKit::InputType::Week:
        return true;
    }
}

- (BOOL)_shouldShowKeyboardForElement:(const WebKit::FocusedElementInformation&)information
{
    if (information.inputMode == WebCore::InputMode::None)
        return NO;

    return [self _shouldShowKeyboardForElementIgnoringInputMode:information];
}

- (BOOL)_shouldShowKeyboardForElementIgnoringInputMode:(const WebKit::FocusedElementInformation&)information
{
    return mayContainSelectableText(information.elementType) || [self _elementTypeRequiresAccessoryView:information.elementType];
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
    SetForScope isChangingFocusForScope { _isChangingFocus, self._hasFocusedElement };
    SetForScope isFocusingElementWithKeyboardForScope { _isFocusingElementWithKeyboard, [self _shouldShowKeyboardForElement:information] };

    auto runModalJavaScriptDialogCallbackIfNeeded = makeScopeExit([&] {
        if (auto callback = std::exchange(_pendingRunModalJavaScriptDialogCallback, { }))
            callback();
    });

    auto inputViewUpdateDeferrer = std::exchange(_inputViewUpdateDeferrer, nullptr);

    _autocorrectionContextNeedsUpdate = YES;
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

#if HAVE(PEPPER_UI_CORE)
                if (_isChangingFocus && ![_focusedFormControlView isHidden])
                    return YES;
#else
                if (_isChangingFocus)
                    return YES;

                if ([self _shouldShowKeyboardForElementIgnoringInputMode:information] && UIKeyboard.isInHardwareKeyboardMode)
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

    // Do not present input peripherals if a validation message is being displayed.
    if (information.isFocusingWithValidationMessage && !_isFocusingElementWithKeyboard)
        shouldShowInputView = NO;

    if (blurPreviousNode) {
        // Defer view updates until the end of this function to avoid a noticeable flash when switching focus
        // between elements that require the keyboard.
        if (!inputViewUpdateDeferrer)
            inputViewUpdateDeferrer = makeUnique<WebKit::InputViewUpdateDeferrer>(self);
        [self _elementDidBlur];
    }

    if (!shouldShowInputView || information.elementType == WebKit::InputType::None) {
        _page->setIsShowingInputViewForFocusedElement(false);
        return;
    }

    _page->setIsShowingInputViewForFocusedElement(true);

    // FIXME: We should remove this check when we manage to send ElementDidFocus from the WebProcess
    // only when it is truly time to show the keyboard.
    if (self._hasFocusedElement && _focusedElementInformation.elementContext.isSameElement(information.elementContext)) {
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

    BOOL requiresKeyboard = mayContainSelectableText(information.elementType);
    BOOL editableChanged = [self setIsEditable:requiresKeyboard];
    _focusedElementInformation = information;
    _traits = nil;

    if (![self isFirstResponder])
        [self becomeFirstResponder];

    if (!_suppressSelectionAssistantReasons && requiresKeyboard && activityStateChanges.contains(WebCore::ActivityState::IsFocused)) {
        _treatAsContentEditableUntilNextEditorStateUpdate = YES;
        [_textInteractionAssistant activateSelection];
        _page->restoreSelectionInFocusedEditableElement();
        _page->scheduleFullEditorStateUpdate();
    }

    _inputPeripheral = createInputPeripheralWithView(_focusedElementInformation.elementType, self);
    _waitingForKeyboardToStartAnimatingInAfterElementFocus = requiresKeyboard;

#if HAVE(PEPPER_UI_CORE)
    [self addFocusedFormControlOverlay];
    if (!_isChangingFocus)
        [self presentViewControllerForCurrentFocusedElement];
#else
    [self reloadInputViews];
#endif

    if (requiresKeyboard) {
        [self _showKeyboard];
        if (!self.window.keyWindow)
            [self.window makeKeyWindow];
    }

    // The custom fixed position rect behavior is affected by -isFocusingElement, so if that changes we need to recompute rects.
    if (editableChanged)
        [_webView _scheduleVisibleContentRectUpdate];

    // For elements that have selectable content (e.g. text field) we need to wait for the web process to send an up-to-date
    // selection rect before we can zoom and reveal the selection. Non-selectable elements (e.g. <select>) can be zoomed
    // immediately because they have no selection to reveal.
    if (requiresKeyboard)
        _page->setWaitingForPostLayoutEditorStateUpdateAfterFocusingElement(true);
    else
        [self _zoomToRevealFocusedElement];

    [self _updateAccessory];

#if HAVE(PEPPER_UI_CORE)
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
    SetForScope isBlurringFocusedElementForScope { _isBlurringFocusedElement, YES };

    [self _endEditing];

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
    _focusedElementInformation.isFocusingWithValidationMessage = false;
    _focusedElementInformation.preventScroll = false;
    _inputPeripheral = nil;
    _focusRequiresStrongPasswordAssistance = NO;
    _autocorrectionContextNeedsUpdate = YES;
    _additionalContextForStrongPasswordAssistance = nil;
    _waitingForKeyboardToStartAnimatingInAfterElementFocus = NO;

    [self resetShouldZoomToFocusRectAfterShowingKeyboard];

    // When defocusing an editable element reset a seen keydown before calling -_hideKeyboard so that we
    // re-evaluate whether we still need a keyboard when UIKit calls us back in -_requiresKeyboardWhenFirstResponder.
    if (editableChanged)
        _seenHardwareKeyDownInNonEditableElement = NO;

    [self _hideKeyboard];

#if HAVE(PEPPER_UI_CORE)
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

    _lastInsertedCharacterToOverrideCharacterBeforeSelection = std::nullopt;
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
    _seenHardwareKeyDownInNonEditableElement = NO;
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
    auto *allCustomData = [pasteboard dataForPasteboardType:@(WebCore::PasteboardCustomData::cocoaType().characters()) inItemSet:indices];
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

- (void)_requestDOMPasteAccessForCategory:(WebCore::DOMPasteAccessCategory)pasteAccessCategory elementRect:(const WebCore::IntRect&)elementRect originIdentifier:(const String&)originIdentifier completionHandler:(CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&)completionHandler
{
    if (auto existingCompletionHandler = std::exchange(_domPasteRequestHandler, WTFMove(completionHandler))) {
        ASSERT_NOT_REACHED();
        existingCompletionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
    }

    _domPasteRequestCategory = pasteAccessCategory;

    if (allPasteboardItemOriginsMatchOrigin(pasteboardForAccessCategory(pasteAccessCategory), originIdentifier)) {
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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [UIMenuController.sharedMenuController showMenuFromView:self rect:menuControllerRect];
    ALLOW_DEPRECATED_DECLARATIONS_END
}

- (void)doAfterEditorStateUpdateAfterFocusingElement:(dispatch_block_t)block
{
    if (!_page->waitingForPostLayoutEditorStateUpdateAfterFocusingElement()) {
        block();
        return;
    }

    _actionsToPerformAfterEditorStateUpdate.append(makeBlockPtr(block));
}

- (void)_didUpdateEditorState
{
    [self _updateInitialWritingDirectionIfNecessary];

    // FIXME: If the initial writing direction just changed, we should wait until we get the next post-layout editor state
    // before zooming to reveal the selection rect.
    if (mayContainSelectableText(_focusedElementInformation.elementType))
        [self _zoomToRevealFocusedElement];

    _treatAsContentEditableUntilNextEditorStateUpdate = NO;

    for (auto block : std::exchange(_actionsToPerformAfterEditorStateUpdate, { }))
        block();
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
    auto identifierBeforeUpdate = _focusedElementInformation.identifier;
    _page->requestFocusedElementInformation([callback = WTFMove(callback), identifierBeforeUpdate, weakSelf] (auto& info) {
        if (!weakSelf || !info || info->identifier != identifierBeforeUpdate) {
            // If the focused element may have changed in the meantime, don't overwrite focused element information.
            callback(false);
            return;
        }

        weakSelf.get()->_focusedElementInformation = info.value();
        callback(true);
    });
}

- (void)reloadContextViewForPresentedListViewController
{
#if HAVE(PEPPER_UI_CORE)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTextInputListViewController class]])
        [(WKTextInputListViewController *)_presentedFullScreenInputViewController.get() reloadContextView];
#endif
}

#if HAVE(PEPPER_UI_CORE)

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

- (RetainPtr<PUICTextInputContext>)createQuickboardTextInputContext
{
    auto context = adoptNS([[PUICTextInputContext alloc] init]);
    [self _updateTextInputTraits:context.get()];
    [context setInitialText:_focusedElementInformation.value];
#if HAVE(QUICKBOARD_CONTROLLER)
    [context setAcceptsEmoji:YES];
    [context setShouldPresentModernTextInputUI:YES];
    [context setPlaceholder:self.inputLabelText];
#endif
    return context;
}

#if HAVE(QUICKBOARD_CONTROLLER)

- (RetainPtr<PUICQuickboardController>)_createQuickboardController:(UIViewController *)presentingViewController
{
    auto quickboardController = adoptNS([[PUICQuickboardController alloc] init]);
    auto context = self.createQuickboardTextInputContext;
    [quickboardController setQuickboardPresentingViewController:presentingViewController];
    [quickboardController setExcludedFromScreenCapture:[context isSecureTextEntry]];
    [quickboardController setTextInputContext:context.get()];
    [quickboardController setDelegate:self];

    return quickboardController;
}

static bool canUseQuickboardControllerFor(UITextContentType type)
{
    return [type isEqualToString:UITextContentTypePassword];
}

#endif // HAVE(QUICKBOARD_CONTROLLER)

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
    default: {
#if HAVE(QUICKBOARD_CONTROLLER)
        if (canUseQuickboardControllerFor(self.textContentTypeForQuickboard)) {
            _presentedQuickboardController = [self _createQuickboardController:presentingViewController];
            break;
        }
#endif
        _presentedFullScreenInputViewController = adoptNS([[WKTextInputListViewController alloc] initWithDelegate:self]);
        break;
    }
    }

#if HAVE(QUICKBOARD_CONTROLLER)
    ASSERT_IMPLIES(_presentedQuickboardController, !_presentedFullScreenInputViewController);
    ASSERT_IMPLIES(_presentedFullScreenInputViewController, !_presentedQuickboardController);
#endif // HAVE(QUICKBOARD_CONTROLLER)
    ASSERT(self._isPresentingFullScreenInputView);
    ASSERT(presentingViewController);

    if (!prefersModalPresentation && [presentingViewController isKindOfClass:[UINavigationController class]])
        _inputNavigationViewControllerForFullScreenInputs = (UINavigationController *)presentingViewController;
    else
        _inputNavigationViewControllerForFullScreenInputs = nil;

    RetainPtr<UIViewController> controller;
    if (_presentedFullScreenInputViewController) {
        // Present the input view controller on an existing navigation stack, if possible. If there is no navigation stack we can use, fall back to presenting modally.
        // This is because the HI specification (for certain scenarios) calls for navigation-style view controller presentation, but WKWebView can't make any guarantees
        // about clients' view controller hierarchies, so we can only try our best to avoid presenting modally. Clients can implicitly opt in to specced behavior by using
        // UINavigationController to present the web view.
        if (_inputNavigationViewControllerForFullScreenInputs)
            [_inputNavigationViewControllerForFullScreenInputs pushViewController:_presentedFullScreenInputViewController.get() animated:YES];
        else
            [presentingViewController presentViewController:_presentedFullScreenInputViewController.get() animated:YES completion:nil];
        controller = _presentedFullScreenInputViewController.get();
    }

#if HAVE(QUICKBOARD_CONTROLLER)
    if (_presentedQuickboardController) {
        [_presentedQuickboardController present];
        controller = [presentingViewController presentedViewController];
    }
#endif // HAVE(QUICKBOARD_CONTROLLER)

    // Presenting a fullscreen input view controller fully obscures the web view. Without taking this token, the web content process will get backgrounded.
    _page->process().startBackgroundActivityForFullscreenInput();

    // FIXME: PUICQuickboardController does not present its view controller immediately, since it asynchronously
    // establishes a connection to QuickboardViewService before presenting the remote view controller.
    // Fixing this requires a version of `-[PUICQuickboardController present]` that takes a completion handler.
    [presentingViewController.transitionCoordinator animateAlongsideTransition:nil completion:[weakWebView = WeakObjCPtr<WKWebView>(_webView), controller] (id <UIViewControllerTransitionCoordinatorContext>) {
        auto strongWebView = weakWebView.get();
        id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([strongWebView UIDelegate]);
        if ([uiDelegate respondsToSelector:@selector(_webView:didPresentFocusedElementViewController:)])
            [uiDelegate _webView:strongWebView.get() didPresentFocusedElementViewController:controller.get()];
    }];
}

- (BOOL)_isPresentingFullScreenInputView
{
#if HAVE(QUICKBOARD_CONTROLLER)
    if (_presentedQuickboardController)
        return YES;
#endif // HAVE(QUICKBOARD_CONTROLLER)
    return _presentedFullScreenInputViewController;
}

- (void)dismissAllInputViewControllers:(BOOL)animated
{
    auto navigationController = std::exchange(_inputNavigationViewControllerForFullScreenInputs, nil);

    if (!self._isPresentingFullScreenInputView) {
        ASSERT(!navigationController);
        return;
    }

    if (auto controller = std::exchange(_presentedFullScreenInputViewController, nil)) {
        auto dispatchDidDismiss = makeBlockPtr([weakWebView = WeakObjCPtr<WKWebView>(_webView), controller] {
            auto strongWebView = weakWebView.get();
            id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([strongWebView UIDelegate]);
            if ([uiDelegate respondsToSelector:@selector(_webView:didDismissFocusedElementViewController:)])
                [uiDelegate _webView:strongWebView.get() didDismissFocusedElementViewController:controller.get()];
        });

        if ([navigationController viewControllers].lastObject == controller.get()) {
            [navigationController popViewControllerAnimated:animated];
            [[controller transitionCoordinator] animateAlongsideTransition:nil completion:[dispatchDidDismiss = WTFMove(dispatchDidDismiss)] (id <UIViewControllerTransitionCoordinatorContext>) {
                dispatchDidDismiss();
            }];
        } else if (auto presentedViewController = retainPtr([controller presentedViewController])) {
            [presentedViewController dismissViewControllerAnimated:animated completion:[controller, animated, dispatchDidDismiss = WTFMove(dispatchDidDismiss)] {
                [controller dismissViewControllerAnimated:animated completion:dispatchDidDismiss.get()];
            }];
        } else
            [controller dismissViewControllerAnimated:animated completion:dispatchDidDismiss.get()];
    }

#if HAVE(QUICKBOARD_CONTROLLER)
    if (auto controller = std::exchange(_presentedQuickboardController, nil)) {
        auto presentedViewController = retainPtr([controller quickboardPresentingViewController].presentedViewController);
        [controller dismissWithCompletion:[weakWebView = WeakObjCPtr<WKWebView>(_webView), presentedViewController] {
            auto strongWebView = weakWebView.get();
            id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([strongWebView UIDelegate]);
            if ([uiDelegate respondsToSelector:@selector(_webView:didDismissFocusedElementViewController:)])
                [uiDelegate _webView:strongWebView.get() didDismissFocusedElementViewController:presentedViewController.get()];
        }];
    }
#endif // HAVE(QUICKBOARD_CONTROLLER)

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

    [(WKTextInputListViewController *)_presentedFullScreenInputViewController updateTextSuggestions:[_focusedFormControlView suggestions]];
}

#pragma mark - WKSelectMenuListViewControllerDelegate

- (void)selectMenu:(WKSelectMenuListViewController *)selectMenu didSelectItemAtIndex:(NSUInteger)index
{
    ASSERT(!_focusedElementInformation.isMultiSelect);
    [self updateFocusedElementSelectedIndex:index allowsMultipleSelection:false];
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

    [self updateFocusedElementSelectedIndex:index allowsMultipleSelection:true];
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

#if HAVE(QUICKBOARD_CONTROLLER)

#pragma mark - PUICQuickboardControllerDelegate

- (void)quickboardController:(PUICQuickboardController *)controller textInputValueDidChange:(NSAttributedString *)attributedText
{
    _page->setTextAsync(attributedText.string);
    [self dismissQuickboardViewControllerAndRevealFocusedFormOverlayIfNecessary:controller];
}

- (void)quickboardControllerTextInputValueCancelled:(PUICQuickboardController *)controller
{
    [self dismissQuickboardViewControllerAndRevealFocusedFormOverlayIfNecessary:controller];
}

#endif // HAVE(QUICKBOARD_CONTROLLER)

#endif // HAVE(PEPPER_UI_CORE)

- (void)_wheelChangedWithEvent:(UIEvent *)event
{
#if HAVE(PEPPER_UI_CORE)
    if ([_focusedFormControlView handleWheelEvent:event])
        return;
#endif
    [super _wheelChangedWithEvent:event];
}

- (void)_updateSelectionAssistantSuppressionState
{
    static const double minimumFocusedElementAreaForSuppressingSelectionAssistant = 4;

    auto& editorState = _page->editorState();
    if (!editorState.hasPostLayoutAndVisualData())
        return;

    BOOL editableRootIsTransparentOrFullyClipped = NO;
    BOOL focusedElementIsTooSmall = NO;
    if (!editorState.selectionIsNone) {
        auto& postLayoutData = *editorState.postLayoutData;
        auto& visualData = *editorState.visualData;
        if (postLayoutData.editableRootIsTransparentOrFullyClipped)
            editableRootIsTransparentOrFullyClipped = YES;

        if (self._hasFocusedElement) {
            auto elementArea = visualData.selectionClipRect.area<RecordOverflow>();
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
    _autocorrectionContextNeedsUpdate = YES;

    [self _updateSelectionAssistantSuppressionState];

    _cachedSelectedTextRange = nil;
    _selectionNeedsUpdate = YES;
    // If we are changing the selection with a gesture there is no need
    // to wait to paint the selection.
    if (_usingGestureForSelection)
        [self _updateChangedSelection];

    if (_candidateViewNeedsUpdate) {
        _candidateViewNeedsUpdate = NO;
        if ([self.inputDelegate respondsToSelector:@selector(layoutHasChanged)])
            [(id <UITextInputDelegatePrivate>)self.inputDelegate layoutHasChanged];
    }
    
    [_webView _didChangeEditorState];

    if (_page->editorState().hasPostLayoutAndVisualData()) {
        _lastInsertedCharacterToOverrideCharacterBeforeSelection = std::nullopt;
        if (!_usingGestureForSelection && _focusedElementInformation.autocapitalizeType == WebCore::AutocapitalizeType::Words)
            [UIKeyboardImpl.sharedInstance clearShiftState];

        if (!_usingGestureForSelection && !_selectionChangeNestingLevel && _page->editorState().triggeredByAccessibilitySelectionChange) {
            // Force UIKit to reload all EditorState-based UI; in particular, this includes text candidates.
            [self beginSelectionChange];
            [self endSelectionChange];
        }
    }
}

- (void)selectWordForReplacement
{
    [self beginSelectionChange];
    _page->extendSelectionForReplacement([weakSelf = WeakObjCPtr<WKContentView>(self)] {
        if (auto strongSelf = weakSelf.get())
            [strongSelf endSelectionChange];
    });
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
    if (!editorState.hasPostLayoutAndVisualData())
        return;

    auto& postLayoutData = *editorState.postLayoutData;
    WebKit::WKSelectionDrawingInfo selectionDrawingInfo { editorState };
    if (force || selectionDrawingInfo != _lastSelectionDrawingInfo) {
        LOG_WITH_STREAM(Selection, stream << "_updateChangedSelection " << selectionDrawingInfo);

        if (_lastSelectionDrawingInfo.caretColor != selectionDrawingInfo.caretColor) {
            // Force UIKit to update the background color of the selection caret to reflect the new -insertionPointColor.
            [[_textInteractionAssistant selectionView] tintColorDidChange];
        }

        _cachedSelectedTextRange = nil;
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
        auto firstResponder = self.firstResponder;
        if ((!firstResponder || self == firstResponder) && !_suppressSelectionAssistantReasons)
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

#if ENABLE(REVEAL)
- (void)requestRVItemInSelectedRangeWithCompletionHandler:(void(^)(RVItem *item))completionHandler
{
    _page->requestRVItemInCurrentSelectedRange([completionHandler = makeBlockPtr(completionHandler), weakSelf = WeakObjCPtr<WKContentView>(self)](const WebKit::RevealItem& item) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(nil);

        SetForScope editMenuPreparationScope { strongSelf->_isPreparingEditMenu, YES };
        completionHandler(item.item());
    });
}

- (void)prepareSelectionForContextMenuWithLocationInView:(CGPoint)locationInView completionHandler:(void(^)(BOOL shouldPresentMenu, RVItem *item))completionHandler
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    _removeBackgroundData = std::nullopt;
#endif

    _page->prepareSelectionForContextMenuWithLocationInView(WebCore::roundedIntPoint(locationInView), [weakSelf = WeakObjCPtr<WKContentView>(self), completionHandler = makeBlockPtr(completionHandler)](bool shouldPresentMenu, auto& item) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(false, nil);

        if (shouldPresentMenu && ![strongSelf _shouldSuppressSelectionCommands])
            [strongSelf->_textInteractionAssistant activateSelection];

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
        [strongSelf doAfterComputingImageAnalysisResultsForBackgroundRemoval:[completionHandler, shouldPresentMenu, item = RetainPtr { item.item() }, weakSelf]() mutable {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return completionHandler(false, nil);

            SetForScope editMenuPreparationScope { strongSelf->_isPreparingEditMenu, YES };
            completionHandler(shouldPresentMenu, item.get());
        }];
#else
        SetForScope editMenuPreparationScope { strongSelf->_isPreparingEditMenu, YES };
        completionHandler(shouldPresentMenu, item.item());
#endif
    });
}
#endif

- (BOOL)hasHiddenContentEditable
{
    return _suppressSelectionAssistantReasons.containsAny({ WebKit::EditableRootIsTransparentOrFullyClipped, WebKit::FocusedElementIsTooSmall });
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

    _frameInfoForFileUploadPanel = frameInfo;
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

- (BOOL)fileUploadPanelDestinationIsManaged:(WKFileUploadPanel *)fileUploadPanel
{
    ASSERT(_fileUploadPanel.get() == fileUploadPanel);

    auto webView = _webView.get();
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([webView UIDelegate]);
    return [uiDelegate respondsToSelector:@selector(_webView:fileUploadPanelContentIsManagedWithInitiatingFrame:)]
        && [uiDelegate _webView:webView.get() fileUploadPanelContentIsManagedWithInitiatingFrame:wrapper(API::FrameInfo::create(WTFMove(_frameInfoForFileUploadPanel), _page.get()))];
}

- (void)_showShareSheet:(const WebCore::ShareDataWithParsedURL&)data inRect:(std::optional<WebCore::FloatRect>)rect completionHandler:(CompletionHandler<void(bool)>&&)completionHandler
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

- (void)shareSheet:(WKShareSheet *)shareSheet willShowActivityItems:(NSArray *)activityItems
{
    ASSERT(_shareSheet == shareSheet);

    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([uiDelegate respondsToSelector:@selector(_webView:willShareActivityItems:)])
        [uiDelegate _webView:self.webView willShareActivityItems:activityItems];
}

#endif

- (void)_showContactPicker:(const WebCore::ContactsRequestData&)requestData completionHandler:(WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&)completionHandler
{
#if HAVE(CONTACTSUI)
    _contactPicker = adoptNS([[WKContactPicker alloc] initWithView:self.webView]);
    [_contactPicker setDelegate:self];
    [_contactPicker presentWithRequestData:requestData completionHandler:WTFMove(completionHandler)];
#else
    completionHandler(std::nullopt);
#endif
}

#if HAVE(CONTACTSUI)
- (void)contactPickerDidPresent:(WKContactPicker *)contactPicker
{
    ASSERT(_contactPicker == contactPicker);

    [_webView _didPresentContactPicker];
}

- (void)contactPickerDidDismiss:(WKContactPicker *)contactPicker
{
    ASSERT(_contactPicker == contactPicker);

    [_contactPicker setDelegate:nil];
    _contactPicker = nil;

    [_webView _didDismissContactPicker];
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

- (void)startRelinquishingFirstResponderToFocusedElement
{
    if (_isRelinquishingFirstResponderToFocusedElement)
        return;

    _isRelinquishingFirstResponderToFocusedElement = YES;
    [_webView _incrementFocusPreservationCount];
}

- (void)stopRelinquishingFirstResponderToFocusedElement
{
    if (!_isRelinquishingFirstResponderToFocusedElement)
        return;

    _isRelinquishingFirstResponderToFocusedElement = NO;
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

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([_imageAnalysisInteraction interactableItemExistsAtPoint:[gestureRecognizer locationInView:self]])
        return YES;
#endif

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

- (std::optional<WebKit::InteractionInformationAtPosition>)positionInformationForActionSheetAssistant:(WKActionSheetAssistant *)assistant
{
    WebKit::InteractionInformationRequest request(_positionInformation.request.point);
    request.includeSnapshot = true;
    request.includeLinkIndicator = assistant.needsLinkIndicator;
    request.linkIndicatorShouldHaveLegacyMargins = !self._shouldUseContextMenus;
    if (![self ensurePositionInformationIsUpToDate:request])
        return std::nullopt;

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
    [self _attemptSyntheticClickAtLocation:location modifierFlags:0];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithURL:(NSURL *)url rect:(CGRect)boundingRect
{
    WebCore::ShareDataWithParsedURL shareData;
    shareData.url = { url };
    shareData.originator = WebCore::ShareDataOriginator::User;
    [self _showShareSheet:shareData inRect: { [self convertRect:boundingRect toView:self.webView] } completionHandler:nil];
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant shareElementWithImage:(UIImage *)image rect:(CGRect)boundingRect
{
    WebCore::ShareDataWithParsedURL shareData;
    NSString* fileName = [NSString stringWithFormat:@"%@.png", (NSString*)WEB_UI_STRING("Shared Image", "Default name for the file created for a shared image with no explicit name.")];
    shareData.files = { { fileName, WebCore::SharedBuffer::create(UIImagePNGRepresentation(image)) } };
    shareData.originator = WebCore::ShareDataOriginator::User;
    [self _showShareSheet:shareData inRect: { [self convertRect:boundingRect toView:self.webView] } completionHandler:nil];
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
#if ENABLE(DRAG_SUPPORT)
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
    else if (!positionInformation.dataDetectorBounds.isEmpty())
        sourceRect = positionInformation.dataDetectorBounds;
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
    [self _removeContextMenuHintContainerIfPossible];
}

- (void)actionSheetAssistantDidShowContextMenu:(WKActionSheetAssistant *)assistant
{
    [_webView _didShowContextMenu];
}

- (void)actionSheetAssistantDidDismissContextMenu:(WKActionSheetAssistant *)assistant
{
    [_webView _didDismissContextMenu];
}

- (void)_targetedPreviewContainerDidRemoveLastSubview:(WKTargetedPreviewContainer *)containerView
{
    if (_contextMenuHintContainerView == containerView)
        [self _removeContextMenuHintContainerIfPossible];
}

#endif // USE(UICONTEXTMENU)

- (BOOL)_shouldUseContextMenus
{
#if HAVE(LINK_PREVIEW) && USE(UICONTEXTMENU)
    return linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::HasUIContextMenuInteraction);
#endif
    return NO;
}

- (BOOL)_shouldUseContextMenusForFormControls
{
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    return self._formControlRefreshEnabled && self._shouldUseContextMenus;
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
    if (WebKit::currentUserInterfaceIdiomIsSmallScreen())
        return NO;

    if (_focusedElementInformation.elementType != WebKit::InputType::Select)
        return NO;

    if (!_focusedElementInformation.shouldUseLegacySelectPopoverDismissalBehaviorInDataActivation)
        return NO;

    return WebCore::IOSApplication::isDataActivation();
}

#if ENABLE(IMAGE_ANALYSIS)

- (BOOL)shouldDeferGestureDueToImageAnalysis:(UIGestureRecognizer *)gesture
{
    return gesture == [_textInteractionAssistant loupeGesture] || gesture._wk_isTapAndAHalf || gesture == [_textInteractionAssistant forcePressGesture];
}

#endif // ENABLE(IMAGE_ANALYSIS)

static WebCore::DataOwnerType coreDataOwnerType(_UIDataOwner platformType)
{
    switch (platformType) {
    case _UIDataOwnerUser:
        return WebCore::DataOwnerType::User;
    case _UIDataOwnerEnterprise:
        return WebCore::DataOwnerType::Enterprise;
    case _UIDataOwnerShared:
        return WebCore::DataOwnerType::Shared;
    case _UIDataOwnerUndefined:
        return WebCore::DataOwnerType::Undefined;
    }
    ASSERT_NOT_REACHED();
    return WebCore::DataOwnerType::Undefined;
}

- (WebCore::DataOwnerType)_dataOwnerForPasteboard:(WebKit::PasteboardAccessIntent)intent
{
    auto specifiedType = [&] {
        if (intent == WebKit::PasteboardAccessIntent::Read)
            return coreDataOwnerType(self._dataOwnerForPaste);

        if (intent == WebKit::PasteboardAccessIntent::Write)
            return coreDataOwnerType(self._dataOwnerForCopy);

        ASSERT_NOT_REACHED();
        return WebCore::DataOwnerType::Undefined;
    }();

    if (specifiedType != WebCore::DataOwnerType::Undefined)
        return specifiedType;

#if !PLATFORM(MACCATALYST)
    if ([[PAL::getMCProfileConnectionClass() sharedConnection] isURLManaged:[_webView URL]])
        return WebCore::DataOwnerType::Enterprise;
#endif

    return WebCore::DataOwnerType::Undefined;
}

- (RetainPtr<WKTargetedPreviewContainer>)_createPreviewContainerWithLayerName:(NSString *)layerName
{
    auto container = adoptNS([[WKTargetedPreviewContainer alloc] initWithContentView:self]);
    [container layer].anchorPoint = CGPointZero;
    [container layer].name = layerName;
    return container;
}

- (UIView *)containerForDropPreviews
{
    if (!_dropPreviewContainerView) {
        _dropPreviewContainerView = [self _createPreviewContainerWithLayerName:@"Drop Preview Container"];
        [_interactionViewsContainerView addSubview:_dropPreviewContainerView.get()];
    }

    ASSERT([_dropPreviewContainerView superview]);
    return _dropPreviewContainerView.get();
}

- (void)_removeContainerForDropPreviews
{
    if (!_dropPreviewContainerView)
        return;

    [std::exchange(_dropPreviewContainerView, nil) removeFromSuperview];
}

- (UIView *)containerForDragPreviews
{
    if (!_dragPreviewContainerView) {
        _dragPreviewContainerView = [self _createPreviewContainerWithLayerName:@"Drag Preview Container"];
        [_interactionViewsContainerView addSubview:_dragPreviewContainerView.get()];
    }

    ASSERT([_dragPreviewContainerView superview]);
    return _dragPreviewContainerView.get();
}

- (void)_removeContainerForDragPreviews
{
    if (!_dragPreviewContainerView)
        return;

    [std::exchange(_dragPreviewContainerView, nil) removeFromSuperview];
}

- (UIView *)containerForContextMenuHintPreviews
{
    if (!_contextMenuHintContainerView) {
        _contextMenuHintContainerView = [self _createPreviewContainerWithLayerName:@"Context Menu Hint Preview Container"];

        RetainPtr<UIView> containerView;

        if (auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>(self.webView.UIDelegate)) {
            if ([uiDelegate respondsToSelector:@selector(_contextMenuHintPreviewContainerViewForWebView:)])
                containerView = [uiDelegate _contextMenuHintPreviewContainerViewForWebView:self.webView];
        }

        if (!containerView)
            containerView = _interactionViewsContainerView;

        [containerView addSubview:_contextMenuHintContainerView.get()];
    }

    ASSERT([_contextMenuHintContainerView superview]);
    return _contextMenuHintContainerView.get();
}

- (void)_removeContainerForContextMenuHintPreviews
{
    if (!_contextMenuHintContainerView)
        return;

    [std::exchange(_contextMenuHintContainerView, nil) removeFromSuperview];

    _scrollViewForTargetedPreview = nil;
    _scrollViewForTargetedPreviewInitialOffset = CGPointZero;
}

- (void)_updateFrameOfContainerForContextMenuHintPreviewsIfNeeded
{
    if (!_contextMenuHintContainerView)
        return;

    CGPoint newOffset = [_scrollViewForTargetedPreview convertPoint:CGPointZero toView:[_contextMenuHintContainerView superview]];

    CGRect frame = [_contextMenuHintContainerView frame];
    frame.origin.x = newOffset.x - _scrollViewForTargetedPreviewInitialOffset.x;
    frame.origin.y = newOffset.y - _scrollViewForTargetedPreviewInitialOffset.y;
    [_contextMenuHintContainerView setFrame:frame];
}

- (void)_updateTargetedPreviewScrollViewUsingContainerScrollingNodeID:(WebCore::ScrollingNodeID)scrollingNodeID
{
    if (scrollingNodeID) {
        if (auto* scrollingCoordinator = downcast<WebKit::RemoteScrollingCoordinatorProxyIOS>(_page->scrollingCoordinatorProxy())) {
            if (UIScrollView *scrollViewForScrollingNode = scrollingCoordinator->scrollViewForScrollingNodeID(scrollingNodeID))
                _scrollViewForTargetedPreview = scrollViewForScrollingNode;
        }
    }

    if (!_scrollViewForTargetedPreview)
        _scrollViewForTargetedPreview = self.webView.scrollView;

    _scrollViewForTargetedPreviewInitialOffset = [_scrollViewForTargetedPreview convertPoint:CGPointZero toView:[_contextMenuHintContainerView superview]];
}

#pragma mark - WKDeferringGestureRecognizerDelegate

- (WebKit::ShouldDeferGestures)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer willBeginTouchesWithEvent:(UIEvent *)event
{
    self.gestureRecognizerConsistencyEnforcer.beginTracking(deferringGestureRecognizer);

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([_imageAnalysisInteraction interactableItemExistsAtPoint:[deferringGestureRecognizer locationInView:self]])
        return WebKit::ShouldDeferGestures::No;
#endif

    return [self gestureRecognizer:deferringGestureRecognizer isInterruptingMomentumScrollingWithEvent:event] ? WebKit::ShouldDeferGestures::No : WebKit::ShouldDeferGestures::Yes;
}

- (void)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer didTransitionToState:(UIGestureRecognizerState)state
{
    if (state == UIGestureRecognizerStateEnded || state == UIGestureRecognizerStateFailed || state == UIGestureRecognizerStateCancelled)
        self.gestureRecognizerConsistencyEnforcer.endTracking(deferringGestureRecognizer);
}

- (void)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer didEndTouchesWithEvent:(UIEvent *)event
{
    self.gestureRecognizerConsistencyEnforcer.endTracking(deferringGestureRecognizer);

    if (deferringGestureRecognizer.state != UIGestureRecognizerStatePossible)
        return;

    if (_page->isHandlingPreventableTouchStart() && [self _isTouchStartDeferringGesture:deferringGestureRecognizer])
        return;

    if (_page->isHandlingPreventableTouchMove() && deferringGestureRecognizer == _touchMoveDeferringGestureRecognizer)
        return;

    if (_page->isHandlingPreventableTouchEnd() && [self _isTouchEndDeferringGesture:deferringGestureRecognizer])
        return;

    if ([_touchEventGestureRecognizer state] == UIGestureRecognizerStatePossible)
        return;

    // In the case where the touch event gesture recognizer has failed or ended already and we are not in the middle of handling
    // an asynchronous (but preventable) touch event, this is our last chance to lift the gesture "gate" by failing the deferring
    // gesture recognizer.
    deferringGestureRecognizer.state = UIGestureRecognizerStateFailed;
}

- (BOOL)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer shouldDeferOtherGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
#if ENABLE(IOS_TOUCH_EVENTS)
    if ([self _touchEventsMustRequireGestureRecognizerToFail:gestureRecognizer])
        return NO;

    if (_failedTouchStartDeferringGestures && _failedTouchStartDeferringGestures->contains(deferringGestureRecognizer)
        && deferringGestureRecognizer.state == UIGestureRecognizerStatePossible) {
        // This deferring gesture no longer has an oppportunity to defer native gestures (either because the touch region did not have any
        // active touch event listeners, or because any active touch event listeners on the page have already executed, and did not prevent
        // default). UIKit may have already reset the gesture to Possible state underneath us, in which case we still need to treat it as
        // if it has already failed; otherwise, we will incorrectly defer other gestures in the web view, such as scroll view pinching.
        return NO;
    }

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

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
    if (gestureRecognizer == _mouseGestureRecognizer)
        return NO;
#if ENABLE(PENCIL_HOVER)
    if (gestureRecognizer == _pencilHoverGestureRecognizer)
        return NO;
#endif
#endif

#if ENABLE(IMAGE_ANALYSIS)
    if (deferringGestureRecognizer == _imageAnalysisDeferringGestureRecognizer)
        return [self shouldDeferGestureDueToImageAnalysis:gestureRecognizer];
#endif

    if (deferringGestureRecognizer == _touchMoveDeferringGestureRecognizer)
        return [gestureRecognizer isKindOfClass:UIPanGestureRecognizer.class] || [gestureRecognizer isKindOfClass:UIPinchGestureRecognizer.class];

    BOOL mayDelayReset = [&]() -> BOOL {
#if USE(UICONTEXTMENU) && HAVE(LINK_PREVIEW)
        if (gestureRecognizer == [self.contextMenuInteraction gestureRecognizerForFailureRelationships])
            return YES;
#endif

#if HAVE(TEXT_INTERACTION_WITH_CONTEXT_MENU_INTERACTION)
        if (gestureRecognizer == [_textInteractionAssistant contextMenuInteraction].gestureRecognizerForFailureRelationships)
            return YES;
#endif

#if ENABLE(DRAG_SUPPORT)
        if (gestureRecognizer.delegate == [_dragInteraction _initiationDriver])
            return YES;
#endif

#if ENABLE(IMAGE_ANALYSIS)
        if (gestureRecognizer == _imageAnalysisGestureRecognizer || gestureRecognizer == _imageAnalysisTimeoutGestureRecognizer)
            return YES;
#endif

        if (gestureRecognizer._wk_isTapAndAHalf)
            return YES;

        if (gestureRecognizer == [_textInteractionAssistant loupeGesture])
            return YES;
        
        if (gestureRecognizer == _highlightLongPressGestureRecognizer)
            return YES;

        if (auto *tapGesture = dynamic_objc_cast<UITapGestureRecognizer>(gestureRecognizer))
            return tapGesture.numberOfTapsRequired > 1 && tapGesture.numberOfTouchesRequired < 2;

        return NO;
    }();

    BOOL isSyntheticTap = [gestureRecognizer isKindOfClass:WKSyntheticTapGestureRecognizer.class];
    if ([gestureRecognizer isKindOfClass:UITapGestureRecognizer.class]) {
        if (deferringGestureRecognizer == _touchEndDeferringGestureRecognizerForSyntheticTapGestures)
            return isSyntheticTap;

        if (deferringGestureRecognizer == _touchEndDeferringGestureRecognizerForDelayedResettableGestures)
            return !isSyntheticTap && mayDelayReset;

        if (deferringGestureRecognizer == _touchEndDeferringGestureRecognizerForImmediatelyResettableGestures)
            return !isSyntheticTap && !mayDelayReset;
    }

    if (isSyntheticTap)
        return deferringGestureRecognizer == _touchStartDeferringGestureRecognizerForSyntheticTapGestures;

    if (mayDelayReset)
        return deferringGestureRecognizer == _touchStartDeferringGestureRecognizerForDelayedResettableGestures;

    return deferringGestureRecognizer == _touchStartDeferringGestureRecognizerForImmediatelyResettableGestures;
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
    ASSERT(item.sourceAction);

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
            _page->dragEnded(positionForDragEnd, positionForDragEnd, { });
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

static UIDropOperation dropOperationForWebCoreDragOperation(std::optional<WebCore::DragOperation> operation)
{
    if (operation) {
        if (*operation == WebCore::DragOperation::Move)
            return UIDropOperationMove;
        if (*operation == WebCore::DragOperation::Copy)
            return UIDropOperationCopy;
    }
    return UIDropOperationCancel;
}

static std::optional<WebCore::DragOperation> coreDragOperationForUIDropOperation(UIDropOperation dropOperation)
{
    switch (dropOperation) {
    case UIDropOperationCancel:
        return std::nullopt;
    case UIDropOperationForbidden:
        return WebCore::DragOperation::Private;
    case UIDropOperationCopy:
        return WebCore::DragOperation::Copy;
    case UIDropOperationMove:
        return WebCore::DragOperation::Move;
    }
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

- (WebCore::DragData)dragDataForDropSession:(id <UIDropSession>)session dragDestinationAction:(WKDragDestinationAction)dragDestinationAction
{
    CGPoint global;
    CGPoint client;
    [self computeClientAndGlobalPointsForDropSession:session outClientPoint:&client outGlobalPoint:&global];

    auto dragOperationMask = WebCore::anyDragOperation();
    if (!session.allowsMoveOperation)
        dragOperationMask.remove(WebCore::DragOperation::Move);

    return {
        session,
        WebCore::roundedIntPoint(client),
        WebCore::roundedIntPoint(global),
        dragOperationMask,
        { },
        WebKit::coreDragDestinationActionMask(dragDestinationAction),
        _page->webPageID()
    };
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

    [self _removeContainerForDragPreviews];
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

- (void)_didReceiveEditDragSnapshot:(std::optional<WebCore::TextIndicatorData>)data
{
    _waitingForEditDragSnapshot = NO;

    [self _deliverDelayedDropPreviewIfPossible:data];
    [self cleanUpDragSourceSessionState];

    if (auto action = WTFMove(_actionToPerformAfterReceivingEditDragSnapshot))
        action();
}

- (void)_deliverDelayedDropPreviewIfPossible:(std::optional<WebCore::TextIndicatorData>)data
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

    auto unselectedContentImageForEditDrag = adoptNS([[UIImage alloc] initWithCGImage:unselectedSnapshotImage->platformImage().get() scale:_page->deviceScaleFactor() orientation:UIImageOrientationUp]);
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
    auto currentDragOperation = _page->currentDragOperation();
    _page->dragEnded(WebCore::roundedIntPoint(client), WebCore::roundedIntPoint(global), currentDragOperation ? *currentDragOperation : OptionSet<WebCore::DragOperation>({ }));
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

    RELEASE_LOG(DragAndDrop, "Drag session: %p preparing to drag with attachment identifier: %s", session.get(), info.attachmentIdentifier.utf8().data());

    NSString *utiType = nil;
    NSString *fileName = nil;
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

        if (auto attachment = strongSelf->_page->attachmentForIdentifier(info.attachmentIdentifier); attachment && !attachment->isEmpty()) {
            attachment->doWithFileWrapper([&](NSFileWrapper *fileWrapper) {
                RELEASE_LOG(DragAndDrop, "Drag session: %p delivering promised attachment: %s at path: %@", session.get(), info.attachmentIdentifier.utf8().data(), destinationURL.path);
                NSError *fileWrapperError = nil;
                if ([fileWrapper writeToURL:destinationURL options:0 originalContentsURL:nil error:&fileWrapperError])
                    callback(destinationURL, nil);
                else
                    callback(nil, fileWrapperError);
            });
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

- (OptionSet<WebCore::DragSourceAction>)_allowedDragSourceActions
{
    auto allowedActions = WebCore::anyDragSourceAction();
    if (!self.isFirstResponder || !_suppressSelectionAssistantReasons.isEmpty()) {
        // Don't allow starting a drag on a selection when selection views are not visible.
        allowedActions.remove(WebCore::DragSourceAction::Selection);
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
    if (!self._hasFocusedElement)
        return nil;

    auto context = adoptNS([[NSMutableDictionary alloc] init]);
    context.get()[@"_WKAutofillContextVersion"] = @(2);

    if (_focusRequiresStrongPasswordAssistance && _focusedElementInformation.elementType == WebKit::InputType::Password) {
        context.get()[@"_automaticPasswordKeyboard"] = @YES;
        context.get()[@"strongPasswordAdditionalContext"] = _additionalContextForStrongPasswordAssistance.get();
    } else if (_focusedElementInformation.acceptsAutofilledLoginCredentials)
        context.get()[@"_acceptsLoginCredentials"] = @YES;

    NSURL *platformURL = _focusedElementInformation.representingPageURL;
    if (platformURL)
        context.get()[@"_WebViewURL"] = platformURL;

    return context.autorelease();
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

    auto nativeImage = image->nativeImage();
    if (!nativeImage)
        return nil;

    return adoptNS([[UIImage alloc] initWithCGImage:nativeImage->platformImage().get()]);
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

static RetainPtr<UITargetedPreview> createFallbackTargetedPreview(UIView *rootView, UIView *containerView, const WebCore::FloatRect& frameInRootViewCoordinates, UIColor *backgroundColor)
{
    if (!containerView.window)
        return nil;

    if (frameInRootViewCoordinates.isEmpty())
        return nil;

    auto parameters = adoptNS([[UIPreviewParameters alloc] init]);
    if (backgroundColor)
        [parameters setBackgroundColor:backgroundColor];

    RetainPtr snapshotView = [rootView resizableSnapshotViewFromRect:frameInRootViewCoordinates afterScreenUpdates:NO withCapInsets:UIEdgeInsetsZero];
    if (!snapshotView)
        snapshotView = adoptNS([UIView new]);

    CGRect frameInContainerViewCoordinates = [rootView convertRect:frameInRootViewCoordinates toView:containerView];

    if (CGRectIsEmpty(frameInContainerViewCoordinates))
        return nil;

    [snapshotView setFrame:frameInContainerViewCoordinates];

    CGPoint centerInContainerViewCoordinates = CGPointMake(CGRectGetMidX(frameInContainerViewCoordinates), CGRectGetMidY(frameInContainerViewCoordinates));
    auto target = adoptNS([[UIPreviewTarget alloc] initWithContainer:containerView center:centerInContainerViewCoordinates]);

    return adoptNS([[UITargetedPreview alloc] initWithView:snapshotView.get() parameters:parameters.get() target:target.get()]);
}

- (UITargetedPreview *)_createTargetedContextMenuHintPreviewForFocusedElement
{
    auto backgroundColor = [&]() -> UIColor * {
        switch (_focusedElementInformation.elementType) {
        case WebKit::InputType::Date:
        case WebKit::InputType::Month:
        case WebKit::InputType::DateTimeLocal:
        case WebKit::InputType::Time:
            return UIColor.clearColor;
        default:
            return nil;
        }
    }();

    auto targetedPreview = createFallbackTargetedPreview(self, self.containerForContextMenuHintPreviews, _focusedElementInformation.interactionRect, backgroundColor);

    [self _updateTargetedPreviewScrollViewUsingContainerScrollingNodeID:_focusedElementInformation.containerScrollingNodeID];

    _contextMenuInteractionTargetedPreview = WTFMove(targetedPreview);
    return _contextMenuInteractionTargetedPreview.get();
}

- (BOOL)positionInformationHasImageOverlayDataDetector
{
    return _positionInformation.isImageOverlayText && [_positionInformation.dataDetectorResults count];
}

- (UITargetedPreview *)_createTargetedContextMenuHintPreviewIfPossible
{
    RetainPtr<UITargetedPreview> targetedPreview;

    if (_positionInformation.isLink && _positionInformation.linkIndicator.contentImage) {
        auto indicator = _positionInformation.linkIndicator;
        auto textIndicatorImage = uiImageForImage(indicator.contentImage.get());
        targetedPreview = createTargetedPreview(textIndicatorImage.get(), self, self.containerForContextMenuHintPreviews, indicator.textBoundingRectInRootViewCoordinates, indicator.textRectsInBoundingRectCoordinates, cocoaColor(indicator.estimatedBackgroundColor).get());
    } else if ((_positionInformation.isAttachment || _positionInformation.isImage) && _positionInformation.image) {
        auto cgImage = _positionInformation.image->makeCGImageCopy();
        auto image = adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);
        targetedPreview = createTargetedPreview(image.get(), self, self.containerForContextMenuHintPreviews, _positionInformation.bounds, { }, nil);
    }

    if (!targetedPreview) {
        auto boundsForFallbackPreview = self.positionInformationHasImageOverlayDataDetector ? _positionInformation.dataDetectorBounds : _positionInformation.bounds;
        targetedPreview = createFallbackTargetedPreview(self, self.containerForContextMenuHintPreviews, boundsForFallbackPreview, nil);
    }

    [self _updateTargetedPreviewScrollViewUsingContainerScrollingNodeID:_positionInformation.containerScrollingNodeID];

    _contextMenuInteractionTargetedPreview = WTFMove(targetedPreview);
    return _contextMenuInteractionTargetedPreview.get();
}

- (void)_removeContextMenuHintContainerIfPossible
{
#if HAVE(LINK_PREVIEW)
    // If a new _contextMenuElementInfo is installed, we've started another interaction,
    // and removing the hint container view will cause the animation to break.
    if (_contextMenuElementInfo)
        return;
#endif
    if (_isDisplayingContextMenuWithAnimation)
        return;
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

    if ([self selectControl])
        return;

    if ([_contextMenuHintContainerView subviews].count)
        return;

    [self _removeContainerForContextMenuHintPreviews];
}

- (void)presentContextMenu:(UIContextMenuInteraction *)contextMenuInteraction atLocation:(CGPoint) location
{
    if (!self.window)
        return;

    [contextMenuInteraction _presentMenuAtLocation:location];
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
    if (flags & UIWKDocumentRequestSpatialAndCurrentSelection)
        options.add(WebKit::DocumentEditingContextRequest::Options::SpatialAndCurrentSelection);

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
    _page->insertTextPlaceholder(WebCore::IntSize { size }, [weakSelf = WeakObjCPtr<WKContentView>(self), completionHandler = makeBlockPtr(completionHandler)](const std::optional<WebCore::ElementContext>& placeholder) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || ![strongSelf webView] || !placeholder) {
            completionHandler(nil);
            return;
        }
        WebCore::ElementContext placeholderToUse { *placeholder };
        placeholderToUse.boundingRect = [strongSelf convertRect:placeholderToUse.boundingRect fromView:[strongSelf webView]];
        completionHandler(adoptNS([[WKTextPlaceholder alloc] initWithElementContext:placeholderToUse]).get());
    });
}

- (void)removeTextPlaceholder:(UITextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText completionHandler:(void (^)(void))completionHandler
{
    // FIXME: Implement support for willInsertText. See <https://bugs.webkit.org/show_bug.cgi?id=208747>.
    if (auto* wkTextPlaceholder = dynamic_objc_cast<WKTextPlaceholder>(placeholder))
        _page->removeTextPlaceholder(wkTextPlaceholder.elementContext, makeBlockPtr(completionHandler));
    else
        completionHandler();
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

        auto unselectedContentImageForEditDrag = adoptNS([[UIImage alloc] initWithCGImage:unselectedSnapshotImage->platformImage().get() scale:protectedSelf->_page->deviceScaleFactor() orientation:UIImageOrientationUp]);
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
    RELEASE_LOG(DragAndDrop, "Preparing for drag session: %p", session);
    if (self.currentDragOrDropSession) {
        // FIXME: Support multiple simultaneous drag sessions in the future.
        RELEASE_LOG(DragAndDrop, "Drag session failed: %p (a current drag session already exists)", session);
        completion();
        return;
    }

    [self cleanUpDragSourceSessionState];

    auto prepareForSession = [weakSelf = WeakObjCPtr<WKContentView>(self), session = retainPtr(session), completion = makeBlockPtr(completion)] (WebKit::ProceedWithTextSelectionInImage proceedWithTextSelectionInImage) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || proceedWithTextSelectionInImage == WebKit::ProceedWithTextSelectionInImage::Yes)
            return;

        auto dragOrigin = [session locationInView:strongSelf.get()];
        strongSelf->_dragDropInteractionState.prepareForDragSession(session.get(), completion.get());
        strongSelf->_page->requestDragStart(WebCore::roundedIntPoint(dragOrigin), WebCore::roundedIntPoint([strongSelf convertPoint:dragOrigin toView:[strongSelf window]]), [strongSelf _allowedDragSourceActions]);
        RELEASE_LOG(DragAndDrop, "Drag session requested: %p at origin: {%.0f, %.0f}", session.get(), dragOrigin.x, dragOrigin.y);
    };

#if ENABLE(IMAGE_ANALYSIS)
    [self _doAfterPendingImageAnalysis:prepareForSession];
#else
    prepareForSession(WebKit::ProceedWithTextSelectionInImage::No);
#endif
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
    else
        [self _cancelLongPressGestureRecognizer];

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
    if (_dragDropInteractionState.anyActiveDragSourceContainsSelection()) {
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
            page->dragEnded(positionForDragEnd, positionForDragEnd, { });
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
    _page->dragEnded(WebCore::roundedIntPoint(_dragDropInteractionState.adjustedPositionForDragEnd()), WebCore::roundedIntPoint(_dragDropInteractionState.adjustedPositionForDragEnd()), coreDragOperationForUIDropOperation(operation));
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
    _isAnimatingDragCancel = YES;
    RELEASE_LOG(DragAndDrop, "Drag interaction willAnimateCancelWithAnimator");
    [animator addCompletion:[protectedSelf = retainPtr(self), page = _page] (UIViewAnimatingPosition finalPosition) {
        RELEASE_LOG(DragAndDrop, "Drag interaction willAnimateCancelWithAnimator (animation completion block fired)");
        page->dragCancelled();
        if (auto completion = protectedSelf->_dragDropInteractionState.takeDragCancelSetDownBlock()) {
            page->callAfterNextPresentationUpdate([completion, protectedSelf] (WebKit::CallbackBase::Error) {
                completion();
                protectedSelf->_isAnimatingDragCancel = NO;
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
        Vector<WebKit::SandboxExtension::Handle> sandboxExtensionForUpload;
        auto dragPasteboardName = WebCore::Pasteboard::nameOfDragPasteboard();
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
    [self _removeContainerForDropPreviews];
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
    _page->dragEnded(WebCore::roundedIntPoint(client), WebCore::roundedIntPoint(global), { });
}

#endif

#if HAVE(PEPPER_UI_CORE)

- (void)dismissQuickboardViewControllerAndRevealFocusedFormOverlayIfNecessary:(id)controller
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

    bool shouldDismissViewController = [controller isKindOfClass:UIViewController.class]
#if HAVE(QUICKBOARD_CONTROLLER)
        && !_presentedQuickboardController
#endif
        && controller != _presentedFullScreenInputViewController;
    // The Quickboard view controller passed into this delegate method is not necessarily the view controller we originally presented;
    // this happens in the case when the user chooses an input method (e.g. scribble) and a new Quickboard view controller is presented.
    if (shouldDismissViewController)
        [(UIViewController *)controller dismissViewControllerAnimated:YES completion:nil];

    [self dismissAllInputViewControllers:controller == _presentedFullScreenInputViewController];
}

- (UITextContentType)textContentTypeForQuickboard
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
        if (auto contentType = contentTypeFromFieldName(_focusedElementInformation.autofillFieldName))
            return contentType;

        if (_focusedElementInformation.isAutofillableUsernameField)
            return UITextContentTypeUsername;

        return nil;
    }
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

- (BOOL)allowsLanguageSelectionForListViewController:(PUICQuickboardViewController *)controller
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

- (PUICTextInputContext *)textInputContextForListViewController:(WKTextInputListViewController *)controller
{
    return self.createQuickboardTextInputContext.autorelease();
}

- (BOOL)allowsDictationInputForListViewController:(PUICQuickboardViewController *)controller
{
    return _focusedElementInformation.elementType != WebKit::InputType::Password;
}

#endif // HAVE(PEPPER_UI_CORE)

#if HAVE(LOOKUP_GESTURE_RECOGNIZER)
- (void)_lookupGestureRecognized:(UIGestureRecognizer *)gestureRecognizer
{
    NSPoint locationInViewCoordinates = [gestureRecognizer locationInView:self];
    _page->performDictionaryLookupAtLocation(WebCore::FloatPoint(locationInViewCoordinates));
}
#endif

- (void)buildMenuForWebViewWithBuilder:(id <UIMenuBuilder>)builder
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if (auto menu = self.removeBackgroundMenu)
        [builder insertSiblingMenu:menu beforeMenuForIdentifier:UIMenuFormat];
#endif

#if ENABLE(APP_HIGHLIGHTS)
    if (auto menu = self.appHighlightMenu)
        [builder insertChildMenu:menu atEndOfMenuForIdentifier:UIMenuRoot];
#endif
}

- (UIMenu *)menuWithInlineAction:(NSString *)title image:(UIImage *)image identifier:(NSString *)identifier handler:(Function<void(WKContentView *)>&&)handler
{
    auto action = [UIAction actionWithTitle:title image:image identifier:identifier handler:makeBlockPtr([handler = WTFMove(handler), weakSelf = WeakObjCPtr<WKContentView>(self)](UIAction *) mutable {
        if (auto strongSelf = weakSelf.get())
            handler(strongSelf.get());
    }).get()];
    return [UIMenu menuWithTitle:@"" image:nil identifier:nil options:UIMenuOptionsDisplayInline children:@[ action ]];
}

#if ENABLE(APP_HIGHLIGHTS)

- (UIMenu *)appHighlightMenu
{
    if (!_page->preferences().appHighlightsEnabled() || !_page->editorState().selectionIsRange || !self.shouldAllowAppHighlightCreation)
        return nil;

    bool isVisible = _page->appHighlightsVisibility();
    auto title = isVisible ? WebCore::contextMenuItemTagAddHighlightToCurrentQuickNote() : WebCore::contextMenuItemTagAddHighlightToNewQuickNote();
    return [self menuWithInlineAction:title image:nil identifier:@"WKActionCreateQuickNote" handler:[isVisible](WKContentView *view) mutable {
        view->_page->createAppHighlightInSelectedRange(isVisible ? WebCore::CreateNewGroupForHighlight::No : WebCore::CreateNewGroupForHighlight::Yes, WebCore::HighlightRequestOriginatedInApp::No);
    }];
}

#endif // ENABLE(APP_HIGHLIGHTS)

- (void)setContinuousSpellCheckingEnabled:(BOOL)enabled
{
    if (WebKit::TextChecker::setContinuousSpellCheckingEnabled(enabled))
        _page->process().updateTextCheckerState();
}

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

static BOOL applicationIsKnownToIgnoreMouseEvents(const char* &warningVersion)
{
    // System apps will always be linked on the current OS, so
    // check them before the linked-on-or-after.

    // <rdar://problem/59521967> iAd Video does not respond to mouse events, only touch events
    if (WebCore::IOSApplication::isNews() || WebCore::IOSApplication::isStocks()) {
        warningVersion = nullptr;
        return YES;
    }

    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SupportsiOSAppsOnMacOS)) {
        if (WebCore::IOSApplication::isFIFACompanion() // <rdar://problem/67093487>
            || WebCore::IOSApplication::isNoggin() // <rdar://problem/64830335>
            || WebCore::IOSApplication::isOKCupid() // <rdar://problem/65698496>
            || WebCore::IOSApplication::isJWLibrary() // <rdar://problem/68104852>
            || WebCore::IOSApplication::isPaperIO() // <rdar://problem/68738585>
            || WebCore::IOSApplication::isCrunchyroll()) { // <rdar://problem/66362029>
            warningVersion = "14.2";
            return YES;
        }
    }

    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::SendsNativeMouseEvents)) {
        if (WebCore::IOSApplication::isPocketCity() // <rdar://problem/62273077>
            || WebCore::IOSApplication::isEssentialSkeleton() // <rdar://problem/62694519>
            || WebCore::IOSApplication::isESPNFantasySports() // <rdar://problem/64671543>
            || WebCore::IOSApplication::isDoubleDown()) { // <rdar://problem/64668138>
            warningVersion = "13.4";
            return YES;
        }
    }

    return NO;
}

- (BOOL)shouldUseMouseGestureRecognizer
{
    static const BOOL shouldUseMouseGestureRecognizer = []() -> BOOL {
        const char* warningVersion = nullptr;
        BOOL knownToIgnoreMouseEvents = applicationIsKnownToIgnoreMouseEvents(warningVersion);

        if (knownToIgnoreMouseEvents && warningVersion)
            os_log_error(OS_LOG_DEFAULT, "WARNING: This application has been observed to ignore mouse events in web content; touch events will be sent until it is built against the iOS %s SDK, but after that, the web content must respect mouse or pointer events in addition to touch events in order to behave correctly when a trackpad or mouse is used.", warningVersion);

        return !knownToIgnoreMouseEvents;
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
#if ENABLE(PENCIL_HOVER)
    _pencilHoverGestureRecognizer = adoptNS([[WKMouseGestureRecognizer alloc] initWithTarget:self action:@selector(mouseGestureRecognizerChanged:)]);
    [_pencilHoverGestureRecognizer setDelegate:self];
#endif
    [self _configureMouseGestureRecognizer];
    
    [self addGestureRecognizer:_mouseGestureRecognizer.get()];
#if ENABLE(PENCIL_HOVER)
    [self addGestureRecognizer:_pencilHoverGestureRecognizer.get()];
#endif
}

- (void)mouseGestureRecognizerChanged:(WKMouseGestureRecognizer *)gestureRecognizer
{
    if (!_page->hasRunningProcess())
        return;

    auto event = gestureRecognizer.takeLastMouseEvent;
    if (!event)
        return;

    if (event->type() == WebKit::WebEvent::MouseDown) {
        _layerTreeTransactionIdAtLastInteractionStart = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).lastCommittedLayerTreeTransactionID();

        if (auto lastMouseLocation = gestureRecognizer.lastMouseLocation)
            _lastInteractionLocation = *lastMouseLocation;
    }

    if (event->type() == WebKit::WebEvent::MouseUp && self.hasHiddenContentEditable && self._hasFocusedElement && !self.window.keyWindow)
        [self.window makeKeyWindow];

    _page->handleMouseEvent(*event);
}

- (void)_configureMouseGestureRecognizer
{
    [_mouseGestureRecognizer setEnabled:[self shouldUseMouseGestureRecognizer]];
#if ENABLE(PENCIL_HOVER)
    [_pencilHoverGestureRecognizer setAllowedTouchTypes:@[ @(UITouchTypePencil)] ];
    [_mouseGestureRecognizer setAllowedTouchTypes:@[ @(UITouchTypeDirect), @(UITouchTypeIndirectPointer)] ];
    [_pencilHoverGestureRecognizer setEnabled:[self shouldUseMouseGestureRecognizer]];
#endif
}

- (void)_setMouseEventPolicy:(WebCore::MouseEventPolicy)policy
{
    _mouseEventPolicy = policy;
    [self _configureMouseGestureRecognizer];
}

#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

- (void)_showMediaControlsContextMenu:(WebCore::FloatRect&&)targetFrame items:(Vector<WebCore::MediaControlsContextMenuItem>&&)items completionHandler:(CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&)completionHandler
{
    [_actionSheetAssistant showMediaControlsContextMenu:WTFMove(targetFrame) items:WTFMove(items) completionHandler:WTFMove(completionHandler)];
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if HAVE(UI_POINTER_INTERACTION)

- (void)setUpPointerInteraction
{
    _pointerInteraction = adoptNS([[UIPointerInteraction alloc] initWithDelegate:self]);
    [_pointerInteraction _setPausesPointerUpdatesWhilePanning:NO];

    [self addInteraction:_pointerInteraction.get()];
}

- (void)_pointerInteraction:(UIPointerInteraction *)interaction regionForRequest:(UIPointerRegionRequest *)request defaultRegion:(UIPointerRegion *)defaultRegion completion:(void(^)(UIPointerRegion *region))completion
{
    WebKit::InteractionInformationRequest interactionInformationRequest;
    interactionInformationRequest.point = WebCore::roundedIntPoint(request.location);
    interactionInformationRequest.includeCaretContext = true;
    interactionInformationRequest.includeHasDoubleClickHandler = false;

    __block BOOL didSynchronouslyReplyWithApproximation = false;
    if (![self _currentPositionInformationIsValidForRequest:interactionInformationRequest] && self.webView._editable && !_positionInformation.shouldNotUseIBeamInEditableContent) {
        didSynchronouslyReplyWithApproximation = true;
        completion([UIPointerRegion regionWithRect:self.bounds identifier:editablePointerRegionIdentifier]);
    }

    // If we already have an outstanding interaction information request, defer this one until
    // we hear back, so that requests don't pile up if the Web Content process is slow.
    if (_hasOutstandingPointerInteractionRequest) {
        _deferredPointerInteractionRequest = std::make_pair(interactionInformationRequest, makeBlockPtr(completion));
        return;
    }

    _hasOutstandingPointerInteractionRequest = YES;

    __block BlockPtr<void(WebKit::InteractionInformationAtPosition, void(^)(UIPointerRegion *))> replyHandler;
    replyHandler = ^(WebKit::InteractionInformationAtPosition interactionInformation, void(^completion)(UIPointerRegion *region)) {
        if (!_deferredPointerInteractionRequest)
            _hasOutstandingPointerInteractionRequest = NO;

        if (didSynchronouslyReplyWithApproximation)
            [interaction invalidate];
        else
            completion([self pointerRegionForPositionInformation:interactionInformation point:request.location]);

        if (_deferredPointerInteractionRequest) {
            didSynchronouslyReplyWithApproximation = false;

            auto deferredRequest = std::exchange(_deferredPointerInteractionRequest, std::nullopt);
            [self doAfterPositionInformationUpdate:^(WebKit::InteractionInformationAtPosition interactionInformation) {
                replyHandler(interactionInformation, deferredRequest->second.get());
            } forRequest:deferredRequest->first];
            return;
        }
    };

    [self doAfterPositionInformationUpdate:^(WebKit::InteractionInformationAtPosition interactionInformation) {
        replyHandler(interactionInformation, completion);
    } forRequest:interactionInformationRequest];
}

- (UIPointerRegion *)pointerRegionForPositionInformation:(WebKit::InteractionInformationAtPosition&)interactionInformation point:(CGPoint)location
{
    WebCore::FloatRect expandedLineRect = enclosingIntRect(interactionInformation.lineCaretExtent);

    // Pad lines of text in order to avoid switching back to the dot cursor between lines.
    // This matches the value that UIKit uses.
    expandedLineRect.inflateY(10);

    if (interactionInformation.cursor) {
        WebCore::Cursor::Type cursorType = interactionInformation.cursor->type();
        if (cursorType == WebCore::Cursor::Hand)
            return [UIPointerRegion regionWithRect:interactionInformation.bounds identifier:pointerRegionIdentifier];

        if (cursorType == WebCore::Cursor::IBeam && expandedLineRect.contains(location))
            return [UIPointerRegion regionWithRect:expandedLineRect identifier:pointerRegionIdentifier];
    }

    if (self.webView._editable) {
        if (expandedLineRect.contains(location))
            return [UIPointerRegion regionWithRect:expandedLineRect identifier:pointerRegionIdentifier];
        return [UIPointerRegion regionWithRect:self.bounds identifier:editablePointerRegionIdentifier];
    }

    return [UIPointerRegion regionWithRect:self.bounds identifier:pointerRegionIdentifier];
}

- (UIPointerStyle *)pointerInteraction:(UIPointerInteraction *)interaction styleForRegion:(UIPointerRegion *)region
{
    double scaleFactor = self._contentZoomScale;

    UIPointerStyle *(^iBeamCursor)(void) = ^{
        float beamLength = _positionInformation.caretLength * scaleFactor;
        auto axisOrientation = _positionInformation.isVerticalWritingMode ? UIAxisHorizontal : UIAxisVertical;
        UIAxis iBeamConstraintAxes = _positionInformation.isVerticalWritingMode ? UIAxisHorizontal : UIAxisVertical;

        // If the I-beam is so large that the magnetism is hard to fight, we should not apply any magnetism.
        if (beamLength > [UITextInteraction _maximumBeamSnappingLength])
            iBeamConstraintAxes = UIAxisNeither;

        // If the region is the size of the view, we should not apply any magnetism.
        if ([region.identifier isEqual:editablePointerRegionIdentifier])
            iBeamConstraintAxes = UIAxisNeither;

        return [UIPointerStyle styleWithShape:[UIPointerShape beamWithPreferredLength:beamLength axis:axisOrientation] constrainedAxes:iBeamConstraintAxes];
    };

    if (self.webView._editable) {
        if (_positionInformation.shouldNotUseIBeamInEditableContent)
            return [UIPointerStyle systemPointerStyle];
        return iBeamCursor();
    }

    if (_positionInformation.cursor && [region.identifier isEqual:pointerRegionIdentifier]) {
        WebCore::Cursor::Type cursorType = _positionInformation.cursor->type();

        if (cursorType == WebCore::Cursor::Hand)
            return [UIPointerStyle systemPointerStyle];

        if (cursorType == WebCore::Cursor::IBeam && _positionInformation.lineCaretExtent.contains(_positionInformation.request.point))
            return iBeamCursor();
    }

    return [UIPointerStyle systemPointerStyle];
}

#endif // HAVE(UI_POINTER_INTERACTION)

#if HAVE(PENCILKIT_TEXT_INPUT)

- (void)setUpScribbleInteraction
{
    _scribbleInteraction = adoptNS([[UIIndirectScribbleInteraction alloc] initWithDelegate:self]);
    [self addInteraction:_scribbleInteraction.get()];
}

- (void)cleanUpScribbleInteraction
{
    [self removeInteraction:_scribbleInteraction.get()];
    _scribbleInteraction = nil;
}

- (_WKTextInputContext *)_textInputContextByScribbleIdentifier:(UIScribbleElementIdentifier)identifier
{
    _WKTextInputContext *textInputContext = (_WKTextInputContext *)identifier;
    if (![textInputContext isKindOfClass:_WKTextInputContext.class])
        return nil;
    auto elementContext = textInputContext._textInputContext;
    if (elementContext.webPageIdentifier != _page->webPageID())
        return nil;
    return textInputContext;
}

- (BOOL)_elementForTextInputContextIsFocused:(_WKTextInputContext *)context
{
    // We ignore bounding rect changes as the bounding rect of the focused element is not kept up-to-date.
    return self._hasFocusedElement && context && context._textInputContext.isSameElement(_focusedElementInformation.elementContext);
}

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction *)interaction requestElementsInRect:(CGRect)rect completion:(void(^)(NSArray<UIScribbleElementIdentifier> *))completion
{
    ASSERT(_scribbleInteraction.get() == interaction);
    [self _requestTextInputContextsInRect:rect completionHandler:completion];
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction *)interaction isElementFocused:(UIScribbleElementIdentifier)identifier
{
    ASSERT(_scribbleInteraction.get() == interaction);
    return [self _elementForTextInputContextIsFocused:[self _textInputContextByScribbleIdentifier:identifier]];
}

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction *)interaction focusElementIfNeeded:(UIScribbleElementIdentifier)identifier referencePoint:(CGPoint)initialPoint completion:(void (^)(UIResponder<UITextInput> *))completionBlock
{
    ASSERT(_scribbleInteraction.get() == interaction);
    if (auto *textInputContext = [self _textInputContextByScribbleIdentifier:identifier])
        [self _focusTextInputContext:textInputContext placeCaretAt:initialPoint completionHandler:completionBlock];
    else
        completionBlock(nil);
}

- (CGRect)indirectScribbleInteraction:(UIIndirectScribbleInteraction *)interaction frameForElement:(UIScribbleElementIdentifier)identifier
{
    ASSERT(_scribbleInteraction.get() == interaction);
    auto *textInputContext = [self _textInputContextByScribbleIdentifier:identifier];
    return textInputContext ? textInputContext.boundingRect : CGRectNull;
}

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction *)interaction willBeginWritingInElement:(UIScribbleElementIdentifier)identifier
{
    ASSERT(_scribbleInteraction.get() == interaction);
    if (auto *textInputContext = [self _textInputContextByScribbleIdentifier:identifier])
        [self _willBeginTextInteractionInTextInputContext:textInputContext];
}

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction *)interaction didFinishWritingInElement:(UIScribbleElementIdentifier)identifier
{
    ASSERT(_scribbleInteraction.get() == interaction);
    if (auto *textInputContext = [self _textInputContextByScribbleIdentifier:identifier])
        [self _didFinishTextInteractionInTextInputContext:textInputContext];
}

#endif // HAVE(PENCILKIT_TEXT_INPUT)

#if ENABLE(VIDEO_PRESENTATION_MODE)

- (void)_didEnterFullscreen
{
    [self _startSuppressingSelectionAssistantForReason:WebKit::SuppressSelectionAssistantReason::ShowingFullscreenVideo];
}

- (void)_didExitFullscreen
{
    [self _stopSuppressingSelectionAssistantForReason:WebKit::SuppressSelectionAssistantReason::ShowingFullscreenVideo];
}

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

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

    if (attachment->isEmpty())
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

    [item registerDataRepresentationForTypeIdentifier:utiType visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[attachment](void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        attachment->doWithFileWrapper([&](NSFileWrapper *fileWrapper) {
            if (auto nsData = RetainPtr { fileWrapper.serializedRepresentation })
                completionHandler(nsData.get(), nil);
            else
                completionHandler(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);
        });
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

#if ENABLE(IMAGE_ANALYSIS)

- (void)_endImageAnalysisGestureDeferral:(WebKit::ShouldPreventGestures)shouldPreventGestures
{
    [_imageAnalysisDeferringGestureRecognizer endDeferral:shouldPreventGestures];
}

- (void)_doAfterPendingImageAnalysis:(void(^)(WebKit::ProceedWithTextSelectionInImage))block
{
    if (self.hasPendingImageAnalysisRequest)
        _actionsToPerformAfterPendingImageAnalysis.append(makeBlockPtr(block));
    else
        block(WebKit::ProceedWithTextSelectionInImage::No);
}

- (void)_invokeAllActionsToPerformAfterPendingImageAnalysis:(WebKit::ProceedWithTextSelectionInImage)proceedWithTextSelectionInImage
{
    _pendingImageAnalysisRequestIdentifier = std::nullopt;
    _elementPendingImageAnalysis = std::nullopt;
    for (auto block : std::exchange(_actionsToPerformAfterPendingImageAnalysis, { }))
        block(proceedWithTextSelectionInImage);
}

#endif // ENABLE(IMAGE_ANALYSIS)

- (void)setUpTextIndicator:(Ref<WebCore::TextIndicator>)textIndicator
{
    if (_textIndicator == textIndicator.ptr())
        return;
    
    [self teardownTextIndicatorLayer];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(startFadeOut) object:nil];
    
    _textIndicator = textIndicator.ptr();

    CGRect frame = _textIndicator->textBoundingRectInRootViewCoordinates();
    _textIndicatorLayer = adoptNS([[WebTextIndicatorLayer alloc] initWithFrame:frame
        textIndicator:textIndicator margin:CGSizeZero offset:CGPointZero]);
    
    [[self layer] addSublayer:_textIndicatorLayer.get()];

    if (_textIndicator->presentationTransition() != WebCore::TextIndicatorPresentationTransition::None)
        [_textIndicatorLayer present];
    
    [self performSelector:@selector(startFadeOut) withObject:self afterDelay:WebCore::timeBeforeFadeStarts.value()];
}

- (void)clearTextIndicator:(WebCore::TextIndicatorDismissalAnimation)animation
{
    RefPtr<WebCore::TextIndicator> textIndicator = WTFMove(_textIndicator);
    
    if ([_textIndicatorLayer isFadingOut])
        return;

    if (textIndicator && [_textIndicatorLayer indicatorWantsManualAnimation:*textIndicator] && [_textIndicatorLayer hasCompletedAnimation] && animation == WebCore::TextIndicatorDismissalAnimation::FadeOut) {
        [self startFadeOut];
        return;
    }

    [self teardownTextIndicatorLayer];
}

- (void)setTextIndicatorAnimationProgress:(float)animationProgress
{
    if (!_textIndicator)
        return;

    [_textIndicatorLayer setAnimationProgress:animationProgress];
}

- (void)teardownTextIndicatorLayer
{
    [_textIndicatorLayer removeFromSuperlayer];
    _textIndicatorLayer = nil;
}

- (void)startFadeOut
{
    [_textIndicatorLayer setFadingOut:YES];
        
    [_textIndicatorLayer hideWithCompletionHandler:[weakSelf = WeakObjCPtr<WKContentView>(self)] {
        auto strongSelf = weakSelf.get();
        [strongSelf teardownTextIndicatorLayer];
    }];
}

#if HAVE(UIFINDINTERACTION)

- (void)useSelectionForFindForWebView:(id)sender
{
    if (!_page->hasSelectedRange())
        return;

    NSString *selectedText = self.selectedText;
    if (selectedText.length) {
        [[self.webView findInteraction] setSearchText:selectedText];
        UIFindInteraction._globalFindBuffer = selectedText;
    }
}

- (void)_findSelectedForWebView:(id)sender
{
    [self useSelectionForFindForWebView:sender];
    [self.webView find:sender];
}

- (void)performTextSearchWithQueryString:(NSString *)string usingOptions:(UITextSearchOptions *)options resultAggregator:(id<UITextSearchAggregator>)aggregator
{
    OptionSet<WebKit::FindOptions> findOptions;
    switch (options.wordMatchMethod) {
    case UITextSearchMatchMethodStartsWith:
        findOptions.add(WebKit::FindOptions::AtWordStarts);
        break;
    case UITextSearchMatchMethodFullWord:
        findOptions.add({ WebKit::FindOptions::AtWordStarts, WebKit::FindOptions::AtWordEnds });
        break;
    default:
        break;
    }

    if (options.stringCompareOptions & NSCaseInsensitiveSearch)
        findOptions.add(WebKit::FindOptions::CaseInsensitive);

    // The limit matches the limit set on existing WKWebView find API.
    constexpr auto maxMatches = 1000;
    _page->findTextRangesForStringMatches(string, findOptions, maxMatches, [string = retainPtr(string), aggregator = retainPtr(aggregator)](const Vector<WebKit::WebFoundTextRange> ranges) {
        for (auto& range : ranges) {
            WKFoundTextRange *textRange = [WKFoundTextRange foundTextRangeWithWebFoundTextRange:range];
            [aggregator foundRange:textRange forSearchString:string.get() inDocument:nil];
        }

        [aggregator finishedSearching];
    });
}

- (void)replaceFoundTextInRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document withText:(NSString *)replacementText
{
    if (!self.supportsTextReplacement)
        return;

    auto foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range);
    if (!foundTextRange)
        return;

    _page->replaceFoundTextRangeWithString([foundTextRange webFoundTextRange], replacementText);
}

- (void)decorateFoundTextRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document usingStyle:(UITextSearchFoundTextStyle)style
{
    auto foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range);
    if (!foundTextRange)
        return;

    auto decorationStyle = WebKit::FindDecorationStyle::Normal;
    if (style == UITextSearchFoundTextStyleFound)
        decorationStyle = WebKit::FindDecorationStyle::Found;
    else if (style == UITextSearchFoundTextStyleHighlighted)
        decorationStyle = WebKit::FindDecorationStyle::Highlighted;

    _page->decorateTextRangeWithStyle([foundTextRange webFoundTextRange], decorationStyle);
}

- (void)scrollRangeToVisible:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document
{
    if (auto foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range))
        _page->scrollTextRangeToVisible([foundTextRange webFoundTextRange]);
}

- (void)clearAllDecoratedFoundText
{
    _page->clearAllDecoratedFoundText();
}

- (void)didBeginTextSearchOperation
{
    _page->didBeginTextSearchOperation();
}

- (void)didEndTextSearchOperation
{
    _page->didEndTextSearchOperation();
}

- (BOOL)supportsTextReplacement
{
    return self.webView.supportsTextReplacement;
}

- (BOOL)supportsTextReplacementForWebView
{
    return self.webView._editable;
}

- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition inDocument:(UITextSearchDocumentIdentifier)document
{
    return [self offsetFromPosition:from toPosition:toPosition];
}

- (NSComparisonResult)compareFoundRange:(UITextRange *)fromRange toRange:(UITextRange *)toRange inDocument:(UITextSearchDocumentIdentifier)document
{
    NSInteger offset = [self offsetFromPosition:fromRange.start toPosition:toRange.start];

    if (offset < 0)
        return NSOrderedAscending;

    if (offset > 0)
        return NSOrderedDescending;

    return NSOrderedSame;
}

- (void)requestRectForFoundTextRange:(UITextRange *)range completionHandler:(void (^)(CGRect))completionHandler
{
    if (auto* foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range)) {
        _page->requestRectForFoundTextRange([foundTextRange webFoundTextRange], [completionHandler = makeBlockPtr(completionHandler)] (WebCore::FloatRect rect) {
            completionHandler(rect);
        });
        return;
    }

    completionHandler(CGRectZero);
}

#endif // HAVE(UIFINDINTERACTION)

#if ENABLE(IMAGE_ANALYSIS)

- (BOOL)hasSelectableTextForImageContextMenu
{
    return valueOrDefault(_imageAnalysisContextMenuActionData).hasSelectableText;
}

- (BOOL)hasVisualSearchResultsForImageContextMenu
{
    return valueOrDefault(_imageAnalysisContextMenuActionData).hasVisualSearchResults;
}

- (CGImageRef)copySubjectResultForImageContextMenu
{
    return valueOrDefault(_imageAnalysisContextMenuActionData).copySubjectResult.get();
}

- (UIMenu *)machineReadableCodeSubMenuForImageContextMenu
{
    return valueOrDefault(_imageAnalysisContextMenuActionData).machineReadableCodeMenu.get();
}

#if USE(QUICK_LOOK)

- (void)presentVisualSearchPreviewControllerForImage:(UIImage *)image imageURL:(NSURL *)imageURL title:(NSString *)title imageBounds:(CGRect)imageBounds appearanceActions:(QLPreviewControllerFirstTimeAppearanceActions)appearanceActions
{
    ASSERT(self.hasSelectableTextForImageContextMenu || self.hasVisualSearchResultsForImageContextMenu);

    ASSERT(!_visualSearchPreviewController);
    _visualSearchPreviewController = adoptNS([PAL::allocQLPreviewControllerInstance() init]);
    [_visualSearchPreviewController setDelegate:self];
    [_visualSearchPreviewController setDataSource:self];
    [_visualSearchPreviewController setAppearanceActions:appearanceActions];
    [_visualSearchPreviewController setModalPresentationStyle:UIModalPresentationOverFullScreen];

    ASSERT(!_visualSearchPreviewImage);
    _visualSearchPreviewImage = image;

    ASSERT(!_visualSearchPreviewTitle);
    _visualSearchPreviewTitle = title;

    ASSERT(!_visualSearchPreviewImageURL);
    _visualSearchPreviewImageURL = imageURL;

    _visualSearchPreviewImageBounds = imageBounds;

    UIViewController *currentPresentingViewController = [UIViewController _viewControllerForFullScreenPresentationFromView:self];
    [currentPresentingViewController presentViewController:_visualSearchPreviewController.get() animated:YES completion:nil];
}

#pragma mark - QLPreviewControllerDelegate

- (CGRect)previewController:(QLPreviewController *)controller frameForPreviewItem:(id <QLPreviewItem>)item inSourceView:(UIView **)outView
{
    *outView = self;
    return _visualSearchPreviewImageBounds;
}

- (UIImage *)previewController:(QLPreviewController *)controller transitionImageForPreviewItem:(id <QLPreviewItem>)item contentRect:(CGRect *)outContentRect
{
    *outContentRect = { CGPointZero, [self convertRect:_visualSearchPreviewImageBounds toView:nil].size };
    return _visualSearchPreviewImage.get();
}

- (void)previewControllerDidDismiss:(QLPreviewController *)controller
{
    ASSERT(controller == _visualSearchPreviewController);
    _visualSearchPreviewController.clear();
    _visualSearchPreviewImage.clear();
    _visualSearchPreviewTitle.clear();
    _visualSearchPreviewImageURL.clear();
}

#pragma mark - QLPreviewControllerDataSource

- (NSInteger)numberOfPreviewItemsInPreviewController:(QLPreviewController *)controller
{
    ASSERT(controller == _visualSearchPreviewController);
    return 1;
}

- (id <QLPreviewItem>)previewController:(QLPreviewController *)controller previewItemAtIndex:(NSInteger)index
{
    ASSERT(controller == _visualSearchPreviewController);
    ASSERT(!index);
    auto item = adoptNS([PAL::allocQLItemInstance() initWithDataProvider:self contentType:UTTypeTIFF.identifier previewTitle:_visualSearchPreviewTitle.get()]);
    if ([item respondsToSelector:@selector(setPreviewOptions:)]) {
        auto previewOptions = adoptNS([[NSMutableDictionary alloc] initWithCapacity:2]);
        if (_visualSearchPreviewImageURL)
            [previewOptions setObject:_visualSearchPreviewImageURL.get() forKey:@"imageURL"];
        if (auto pageURL = URL { _page->currentURL() }; !pageURL.isEmpty())
            [previewOptions setObject:pageURL forKey:@"pageURL"];
        if ([previewOptions count])
            [item setPreviewOptions:previewOptions.get()];
    }
    return item.autorelease();
}

#pragma mark - QLPreviewItemDataProvider

- (NSData *)provideDataForItem:(QLItem *)item
{
    ASSERT(_visualSearchPreviewImage);
    return WebKit::transcode([_visualSearchPreviewImage CGImage], (__bridge CFStringRef)UTTypeTIFF.identifier).autorelease();
}

#pragma mark - WKActionSheetAssistantDelegate

- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeShowTextActionForElement:(_WKActivatedElementInfo *)element
{
    return WebKit::isLiveTextAvailableAndEnabled() && self.hasSelectableTextForImageContextMenu;
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant showTextForImage:(UIImage *)image imageURL:(NSURL *)imageURL title:(NSString *)title imageBounds:(CGRect)imageBounds
{
    [self presentVisualSearchPreviewControllerForImage:image imageURL:imageURL title:title imageBounds:imageBounds appearanceActions:QLPreviewControllerFirstTimeAppearanceActionEnableVisualSearchDataDetection];
}

- (BOOL)actionSheetAssistant:(WKActionSheetAssistant *)assistant shouldIncludeLookUpImageActionForElement:(_WKActivatedElementInfo *)element
{
    return WebKit::isLiveTextAvailableAndEnabled() && self.hasVisualSearchResultsForImageContextMenu;
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant lookUpImage:(UIImage *)image imageURL:(NSURL *)imageURL title:(NSString *)title imageBounds:(CGRect)imageBounds
{
    [self presentVisualSearchPreviewControllerForImage:image imageURL:imageURL title:title imageBounds:imageBounds appearanceActions:QLPreviewControllerFirstTimeAppearanceActionEnableVisualSearchMode];
}

#endif // USE(QUICK_LOOK)

#pragma mark - Image Extraction

- (CocoaImageAnalyzer *)imageAnalyzer
{
    if (!_imageAnalyzer)
        _imageAnalyzer = WebKit::createImageAnalyzer();
    return _imageAnalyzer.get();
}

- (BOOL)hasPendingImageAnalysisRequest
{
    return !!_pendingImageAnalysisRequestIdentifier;
}

- (void)_setUpImageAnalysis
{
    if (!WebKit::isLiveTextAvailableAndEnabled())
        return;

    _pendingImageAnalysisRequestIdentifier = std::nullopt;
    _isProceedingWithTextSelectionInImage = NO;
    _elementPendingImageAnalysis = std::nullopt;
    _imageAnalysisGestureRecognizer = adoptNS([[WKImageAnalysisGestureRecognizer alloc] initWithImageAnalysisGestureDelegate:self]);
    [self addGestureRecognizer:_imageAnalysisGestureRecognizer.get()];
    _imageAnalysisTimeoutGestureRecognizer = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(imageAnalysisGestureDidTimeOut:)]);
    [_imageAnalysisTimeoutGestureRecognizer setMinimumPressDuration:2.0];
    [_imageAnalysisTimeoutGestureRecognizer setName:@"Image analysis timeout"];
    [_imageAnalysisTimeoutGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_imageAnalysisTimeoutGestureRecognizer.get()];
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    _removeBackgroundData = std::nullopt;
#endif
    _waitingForDynamicImageAnalysisContextMenuActions = NO;
    _imageAnalysisContextMenuActionData = std::nullopt;
}

- (void)_tearDownImageAnalysis
{
    if (!WebKit::isLiveTextAvailableAndEnabled())
        return;

    [_imageAnalysisGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_imageAnalysisGestureRecognizer.get()];
    _imageAnalysisGestureRecognizer = nil;
    [_imageAnalysisTimeoutGestureRecognizer setDelegate:nil];
    [self removeGestureRecognizer:_imageAnalysisTimeoutGestureRecognizer.get()];
    _imageAnalysisTimeoutGestureRecognizer = nil;
    _pendingImageAnalysisRequestIdentifier = std::nullopt;
    _isProceedingWithTextSelectionInImage = NO;
    _elementPendingImageAnalysis = std::nullopt;
    [std::exchange(_imageAnalyzer, nil) cancelAllRequests];
    [self _invokeAllActionsToPerformAfterPendingImageAnalysis:WebKit::ProceedWithTextSelectionInImage::No];
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    [self uninstallImageAnalysisInteraction];
    _removeBackgroundData = std::nullopt;
#endif
    _waitingForDynamicImageAnalysisContextMenuActions = NO;
    _imageAnalysisContextMenuActionData = std::nullopt;
}

- (void)_cancelImageAnalysis
{
    [_imageAnalyzer cancelAllRequests];
    RELEASE_LOG_IF(self.hasPendingImageAnalysisRequest, ImageAnalysis, "Image analysis request %" PRIu64 " cancelled.", _pendingImageAnalysisRequestIdentifier->toUInt64());
    _pendingImageAnalysisRequestIdentifier = std::nullopt;
    _isProceedingWithTextSelectionInImage = NO;
    _elementPendingImageAnalysis = std::nullopt;
}

- (RetainPtr<CocoaImageAnalyzerRequest>)createImageAnalyzerRequest:(VKAnalysisTypes)analysisTypes image:(CGImageRef)image imageURL:(NSURL *)imageURL
{
    auto request = WebKit::createImageAnalyzerRequest(image, analysisTypes);
    [request setImageURL:imageURL];
    [request setPageURL:[NSURL _web_URLWithWTFString:_page->currentURL()]];
    return request;
}

- (RetainPtr<CocoaImageAnalyzerRequest>)createImageAnalyzerRequest:(VKAnalysisTypes)analysisTypes image:(CGImageRef)image
{
    return [self createImageAnalyzerRequest:analysisTypes image:image imageURL:_positionInformation.imageURL];
}


- (void)updateImageAnalysisForContextMenuPresentation:(CocoaImageAnalysis *)analysis
{
#if USE(UICONTEXTMENU) && ENABLE(IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
    analysis.presentingViewControllerForMrcAction = [UIViewController _viewControllerForFullScreenPresentationFromView:self];
#endif
}


- (BOOL)validateImageAnalysisRequestIdentifier:(WebKit::ImageAnalysisRequestIdentifier)identifier
{
    if (_pendingImageAnalysisRequestIdentifier == identifier)
        return YES;

    if (!self.hasPendingImageAnalysisRequest) {
        // Only invoke deferred image analysis blocks if there is no ongoing request; otherwise, this would
        // cause these blocks to be invoked too early (i.e. in the middle of the new image analysis request).
        [self _invokeAllActionsToPerformAfterPendingImageAnalysis:WebKit::ProceedWithTextSelectionInImage::No];
    }

    RELEASE_LOG(ImageAnalysis, "Image analysis request %" PRIu64 " invalidated.", identifier.toUInt64());
    return NO;
}

- (void)requestTextRecognition:(NSURL *)imageURL imageData:(const WebKit::ShareableBitmapHandle&)imageData sourceLanguageIdentifier:(NSString *)sourceLanguageIdentifier targetLanguageIdentifier:(NSString *)targetLanguageIdentifier completionHandler:(CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&)completion
{
    auto imageBitmap = WebKit::ShareableBitmap::create(imageData);
    if (!imageBitmap) {
        completion({ });
        return;
    }

    auto cgImage = imageBitmap->makeCGImage();
    if (!cgImage) {
        completion({ });
        return;
    }

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if (targetLanguageIdentifier.length)
        return WebKit::requestVisualTranslation(self.imageAnalyzer, imageURL, sourceLanguageIdentifier, targetLanguageIdentifier, cgImage.get(), WTFMove(completion));
#else
    UNUSED_PARAM(sourceLanguageIdentifier);
    UNUSED_PARAM(targetLanguageIdentifier);
#endif

    auto request = [self createImageAnalyzerRequest:VKAnalysisTypeText image:cgImage.get()];
    [self.imageAnalyzer processRequest:request.get() progressHandler:nil completionHandler:makeBlockPtr([completion = WTFMove(completion)] (CocoaImageAnalysis *result, NSError *) mutable {
        completion(WebKit::makeTextRecognitionResult(result));
    }).get()];
}

#pragma mark - WKImageAnalysisGestureRecognizerDelegate

- (void)imageAnalysisGestureDidBegin:(WKImageAnalysisGestureRecognizer *)gestureRecognizer
{
    ASSERT(WebKit::isLiveTextAvailableAndEnabled());

    auto requestIdentifier = WebKit::ImageAnalysisRequestIdentifier::generate();

    [_imageAnalyzer cancelAllRequests];
    _pendingImageAnalysisRequestIdentifier = requestIdentifier;
    _isProceedingWithTextSelectionInImage = NO;
    _elementPendingImageAnalysis = std::nullopt;
    _waitingForDynamicImageAnalysisContextMenuActions = NO;
    _imageAnalysisContextMenuActionData = std::nullopt;

    WebKit::InteractionInformationRequest request { WebCore::roundedIntPoint([gestureRecognizer locationInView:self]) };
    request.includeImageData = true;
    [self doAfterPositionInformationUpdate:[requestIdentifier = WTFMove(requestIdentifier), weakSelf = WeakObjCPtr<WKContentView>(self), gestureDeferralToken = WebKit::ImageAnalysisGestureDeferralToken::create(self)] (WebKit::InteractionInformationAtPosition information) mutable {
        auto strongSelf = weakSelf.get();
        if (![strongSelf validateImageAnalysisRequestIdentifier:requestIdentifier])
            return;

        bool shouldAnalyzeImageAtLocation = ([&] {
            if (!information.isImage && !canAttemptTextRecognitionForNonImageElements(information, strongSelf->_page->preferences()))
                return false;

            if (!information.image)
                return false;

            if (!information.hostImageOrVideoElementContext)
                return false;

            if (information.isAnimatedImage)
                return false;

            if (information.isContentEditable)
                return false;

            return true;
        })();

        if (!strongSelf->_pendingImageAnalysisRequestIdentifier || !shouldAnalyzeImageAtLocation) {
            [strongSelf _invokeAllActionsToPerformAfterPendingImageAnalysis:WebKit::ProceedWithTextSelectionInImage::No];
            return;
        }

        auto cgImage = information.image->makeCGImageCopy();
        if (!cgImage) {
            [strongSelf _invokeAllActionsToPerformAfterPendingImageAnalysis:WebKit::ProceedWithTextSelectionInImage::No];
            return;
        }

        RELEASE_LOG(ImageAnalysis, "Image analysis preflight gesture initiated (request %" PRIu64 ").", requestIdentifier.toUInt64());

        strongSelf->_elementPendingImageAnalysis = information.hostImageOrVideoElementContext;

        auto requestLocation = information.request.point;
        WebCore::ElementContext elementContext = *information.hostImageOrVideoElementContext;

        auto requestForTextSelection = [strongSelf createImageAnalyzerRequest:VKAnalysisTypeText image:cgImage.get()];
        if (information.elementContainsImageOverlay) {
            [strongSelf _completeImageAnalysisRequestForContextMenu:cgImage.get() requestIdentifier:requestIdentifier hasTextResults:YES];
            return;
        }

        auto textAnalysisStartTime = MonotonicTime::now();
        [[strongSelf imageAnalyzer] processRequest:requestForTextSelection.get() progressHandler:nil completionHandler:[requestIdentifier = WTFMove(requestIdentifier), weakSelf, elementContext, requestLocation, cgImage, gestureDeferralToken, textAnalysisStartTime] (CocoaImageAnalysis *result, NSError *error) mutable {
            auto strongSelf = weakSelf.get();
            if (![strongSelf validateImageAnalysisRequestIdentifier:requestIdentifier])
                return;

            BOOL hasTextResults = [result hasResultsForAnalysisTypes:VKAnalysisTypeText];
            RELEASE_LOG(ImageAnalysis, "Image analysis completed in %.0f ms (request %" PRIu64 "; found text? %d)", (MonotonicTime::now() - textAnalysisStartTime).milliseconds(), requestIdentifier.toUInt64(), hasTextResults);

            strongSelf->_page->updateWithTextRecognitionResult(WebKit::makeTextRecognitionResult(result), elementContext, requestLocation, [requestIdentifier = WTFMove(requestIdentifier), weakSelf, hasTextResults, cgImage, gestureDeferralToken] (WebKit::TextRecognitionUpdateResult updateResult) mutable {
                auto strongSelf = weakSelf.get();
                if (![strongSelf validateImageAnalysisRequestIdentifier:requestIdentifier])
                    return;

                if (updateResult == WebKit::TextRecognitionUpdateResult::Text) {
                    strongSelf->_isProceedingWithTextSelectionInImage = YES;
                    [strongSelf _invokeAllActionsToPerformAfterPendingImageAnalysis:WebKit::ProceedWithTextSelectionInImage::Yes];
                    return;
                }

                gestureDeferralToken->setShouldPreventTextSelection();

                if (updateResult == WebKit::TextRecognitionUpdateResult::DataDetector) {
                    [strongSelf _invokeAllActionsToPerformAfterPendingImageAnalysis:WebKit::ProceedWithTextSelectionInImage::No];
                    return;
                }

                [strongSelf _completeImageAnalysisRequestForContextMenu:cgImage.get() requestIdentifier:requestIdentifier hasTextResults:hasTextResults];
            });
        }];
    } forRequest:request];
}

#if ENABLE(IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
static BOOL shouldUseMachineReadableCodeMenuFromImageAnalysisResult(CocoaImageAnalysis *result)
{
#if HAVE(BCS_LIVE_CAMERA_ONLY_ACTION_SPI)
    return [result.barcodeActions indexOfObjectPassingTest:^BOOL(BCSAction *action, NSUInteger index, BOOL *stop) {
        return [action respondsToSelector:@selector(isLiveCameraOnlyAction)] && [(id)action isLiveCameraOnlyAction];
    }] == NSNotFound;
#else // not HAVE(BCS_LIVE_CAMERA_ONLY_ACTION_SPI)
    return YES;
#endif // HAVE(BCS_LIVE_CAMERA_ONLY_ACTION_SPI)
}
#endif // ENABLE(IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)

- (void)_completeImageAnalysisRequestForContextMenu:(CGImageRef)image requestIdentifier:(WebKit::ImageAnalysisRequestIdentifier)requestIdentifier hasTextResults:(BOOL)hasTextResults
{
    _waitingForDynamicImageAnalysisContextMenuActions = YES;
    _imageAnalysisContextMenuActionData = std::nullopt;
    [self _invokeAllActionsToPerformAfterPendingImageAnalysis:WebKit::ProceedWithTextSelectionInImage::No];

    auto data = Box<WebKit::ImageAnalysisContextMenuActionData>::create();
    data->hasSelectableText = hasTextResults;

    auto weakSelf = WeakObjCPtr<WKContentView>(self);
    auto aggregator = MainRunLoopCallbackAggregator::create([weakSelf, data]() mutable {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || !strongSelf->_waitingForDynamicImageAnalysisContextMenuActions)
            return;

        strongSelf->_waitingForDynamicImageAnalysisContextMenuActions = NO;
        strongSelf->_imageAnalysisContextMenuActionData = WTFMove(*data);
        [[strongSelf contextMenuInteraction] updateVisibleMenuWithBlock:makeBlockPtr([strongSelf](UIMenu *menu) -> UIMenu * {
            __block auto indexOfPlaceholder = NSNotFound;
            __block BOOL foundCopyItem = NO;
            auto revealImageIdentifier = elementActionTypeToUIActionIdentifier(_WKElementActionTypeRevealImage);
            auto copyIdentifier = elementActionTypeToUIActionIdentifier(_WKElementActionTypeCopy);
            [menu.children enumerateObjectsUsingBlock:^(UIMenuElement *child, NSUInteger index, BOOL* stop) {
                auto *action = dynamic_objc_cast<UIAction>(child);
                if ([action.identifier isEqualToString:revealImageIdentifier] && (action.attributes & UIMenuElementAttributesHidden))
                    indexOfPlaceholder = index;
                else if ([action.identifier isEqualToString:copyIdentifier])
                    foundCopyItem = YES;
            }];

            if (indexOfPlaceholder == NSNotFound)
                return menu;

            auto adjustedChildren = adoptNS(menu.children.mutableCopy);
            auto replacements = adoptNS([NSMutableArray<UIMenuElement *> new]);

            auto *elementInfo = [_WKActivatedElementInfo activatedElementInfoWithInteractionInformationAtPosition:strongSelf->_positionInformation userInfo:nil];
            auto addAction = [&](_WKElementActionType action) {
                auto *elementAction = [_WKElementAction _elementActionWithType:action info:elementInfo assistant:strongSelf->_actionSheetAssistant.get()];
                [replacements addObject:[elementAction uiActionForElementInfo:elementInfo]];
            };

            if (foundCopyItem && [strongSelf copySubjectResultForImageContextMenu])
                addAction(_WKElementActionTypeCopyCroppedImage);

            if ([strongSelf hasSelectableTextForImageContextMenu])
                addAction(_WKElementActionTypeImageExtraction);

            if ([strongSelf hasVisualSearchResultsForImageContextMenu])
                addAction(_WKElementActionTypeRevealImage);

            if (UIMenu *subMenu = [strongSelf machineReadableCodeSubMenuForImageContextMenu])
                [replacements addObject:subMenu];

            RELEASE_LOG(ImageAnalysis, "Dynamically inserting %zu context menu action(s)", [replacements count]);
            [adjustedChildren replaceObjectsInRange:NSMakeRange(indexOfPlaceholder, 1) withObjectsFromArray:replacements.get()];
            return [menu menuByReplacingChildren:adjustedChildren.get()];
        }).get()];
    });

    auto request = [self createImageAnalyzerRequest:VKAnalysisTypeVisualSearch | VKAnalysisTypeMachineReadableCode | VKAnalysisTypeAppClip image:image];
    auto visualSearchAnalysisStartTime = MonotonicTime::now();
    [self.imageAnalyzer processRequest:request.get() progressHandler:nil completionHandler:[requestIdentifier = WTFMove(requestIdentifier), weakSelf, visualSearchAnalysisStartTime, aggregator = aggregator.copyRef(), data] (CocoaImageAnalysis *result, NSError *error) mutable {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

#if USE(QUICK_LOOK)
        BOOL hasVisualSearchResults = [result hasResultsForAnalysisTypes:VKAnalysisTypeVisualSearch];
        RELEASE_LOG(ImageAnalysis, "Image analysis completed in %.0f ms (request %" PRIu64 "; found visual search results? %d)", (MonotonicTime::now() - visualSearchAnalysisStartTime).milliseconds(), requestIdentifier.toUInt64(), hasVisualSearchResults);
#else
        UNUSED_PARAM(visualSearchAnalysisStartTime);
#endif
        if (!result || error)
            return;

#if ENABLE(IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
        if (shouldUseMachineReadableCodeMenuFromImageAnalysisResult(result))
            data->machineReadableCodeMenu = result.mrcMenu;
#endif
#if USE(QUICK_LOOK)
        data->hasVisualSearchResults = hasVisualSearchResults;
#endif
        [strongSelf updateImageAnalysisForContextMenuPresentation:result];
    }];

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if (_page->preferences().removeBackgroundEnabled()) {
        WebKit::requestBackgroundRemoval(image, [weakSelf = WeakObjCPtr<WKContentView>(self), aggregator = aggregator.copyRef(), data](CGImageRef result) mutable {
            if (auto strongSelf = weakSelf.get())
                data->copySubjectResult = result;
        });
    }
#endif
}

- (void)imageAnalysisGestureDidFail:(WKImageAnalysisGestureRecognizer *)gestureRecognizer
{
    [self _endImageAnalysisGestureDeferral:WebKit::ShouldPreventGestures::No];
}

- (void)imageAnalysisGestureDidTimeOut:(WKImageAnalysisGestureRecognizer *)gestureRecognizer
{
    if (!self._shouldUseContextMenus)
        return;

    if (self.hasPendingImageAnalysisRequest)
        return;

    if (gestureRecognizer.state != UIGestureRecognizerStateBegan)
        return;

    auto location = WebCore::roundedIntPoint([gestureRecognizer locationInView:self]);
    WebKit::InteractionInformationRequest request { location };
    request.includeImageData = true;
    [self doAfterPositionInformationUpdate:[weakSelf = WeakObjCPtr<WKContentView>(self), location] (WebKit::InteractionInformationAtPosition info) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        if (!info.image)
            return;

        if ((!info.isImageOverlayText || !info.isSelected) && !strongSelf->_isProceedingWithTextSelectionInImage)
            return;

        auto cgImage = info.image->makeCGImageCopy();
        if (!cgImage)
            return;

        RELEASE_LOG(ImageAnalysis, "Image analysis timeout gesture initiated.");
        // FIXME: We need to implement some way to cache image analysis results per element, so that we don't end up
        // making redundant image analysis requests for the same image data.

        auto data = Box<WebKit::ImageAnalysisContextMenuActionData>::create();
        auto aggregator = MainRunLoopCallbackAggregator::create([weakSelf, location, data]() mutable {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            strongSelf->_contextMenuWasTriggeredByImageAnalysisTimeout = YES;
            strongSelf->_imageAnalysisContextMenuActionData = WTFMove(*data);
            [strongSelf presentContextMenu:[strongSelf contextMenuInteraction] atLocation:location];
        });

        auto visualSearchAnalysisStartTime = MonotonicTime::now();
        auto requestForContextMenu = [strongSelf createImageAnalyzerRequest:VKAnalysisTypeVisualSearch | VKAnalysisTypeMachineReadableCode | VKAnalysisTypeAppClip image:cgImage.get()];
        [[strongSelf imageAnalyzer] processRequest:requestForContextMenu.get() progressHandler:nil completionHandler:[weakSelf, visualSearchAnalysisStartTime, aggregator = aggregator.copyRef(), data] (CocoaImageAnalysis *result, NSError *error) {
            auto strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

#if USE(QUICK_LOOK)
            BOOL hasVisualSearchResults = [result hasResultsForAnalysisTypes:VKAnalysisTypeVisualSearch];
            RELEASE_LOG(ImageAnalysis, "Image analysis completed in %.0f ms (found visual search results? %d)", (MonotonicTime::now() - visualSearchAnalysisStartTime).milliseconds(), hasVisualSearchResults);
            data->hasSelectableText = true;
            data->hasVisualSearchResults = hasVisualSearchResults;
#else
            UNUSED_PARAM(visualSearchAnalysisStartTime);
#endif
            [strongSelf updateImageAnalysisForContextMenuPresentation:result];
        }];

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
        if (strongSelf->_page->preferences().removeBackgroundEnabled()) {
            WebKit::requestBackgroundRemoval(cgImage.get(), [weakSelf, aggregator = aggregator.copyRef(), data](CGImageRef result) mutable {
                if (auto strongSelf = weakSelf.get())
                    data->copySubjectResult = result;
            });
        }
#endif
    } forRequest:request];
}

- (void)captureTextFromCameraForWebView:(id)sender
{
    [super captureTextFromCamera:sender];
}

#endif // ENABLE(IMAGE_ANALYSIS)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

- (BOOL)actionSheetAssistantShouldIncludeCopySubjectAction:(WKActionSheetAssistant *)assistant
{
    return !!self.copySubjectResultForImageContextMenu;
}

- (void)actionSheetAssistant:(WKActionSheetAssistant *)assistant copySubject:(UIImage *)image sourceMIMEType:(NSString *)sourceMIMEType
{
    if (!self.copySubjectResultForImageContextMenu)
        return;

    auto [data, type] = WebKit::imageDataForRemoveBackground(self.copySubjectResultForImageContextMenu, (__bridge CFStringRef)sourceMIMEType);
    if (!data)
        return;

    [UIPasteboard _performAsDataOwner:self._dataOwnerForCopy block:[data = WTFMove(data), type = WTFMove(type)] {
        [UIPasteboard.generalPasteboard setData:data.get() forPasteboardType:(__bridge NSString *)type.get()];
    }];
}

#pragma mark - VKCImageAnalysisInteractionDelegate

- (CGRect)contentsRectForImageAnalysisInteraction:(VKCImageAnalysisInteraction *)interaction
{
    auto unitInteractionRect = _imageAnalysisInteractionBounds;
    WebCore::FloatRect unobscuredRect = self.bounds;
    unitInteractionRect.moveBy(-unobscuredRect.location());
    unitInteractionRect.scale(1 / unobscuredRect.size());
    return unitInteractionRect;
}

- (BOOL)imageAnalysisInteraction:(VKCImageAnalysisInteraction *)interaction shouldBeginAtPoint:(CGPoint)point forAnalysisType:(VKImageAnalysisInteractionTypes)analysisType
{
    return interaction.hasActiveTextSelection || [interaction interactableItemExistsAtPoint:point];
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
constexpr auto analysisTypesForFullscreenVideo = VKAnalysisTypeAll & ~VKAnalysisTypeVisualSearch;
#endif

- (void)beginTextRecognitionForFullscreenVideo:(const WebKit::ShareableBitmapHandle&)imageData playerViewController:(AVPlayerViewController *)controller
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    ASSERT(_page->preferences().textRecognitionInVideosEnabled());

    if (_fullscreenVideoImageAnalysisRequestIdentifier)
        return;

    auto imageBitmap = WebKit::ShareableBitmap::create(imageData);
    if (!imageBitmap)
        return;

    auto cgImage = imageBitmap->makeCGImage();
    if (!cgImage)
        return;

    auto request = [self createImageAnalyzerRequest:analysisTypesForFullscreenVideo image:cgImage.get()];
    _fullscreenVideoImageAnalysisRequestIdentifier = [self.imageAnalyzer processRequest:request.get() progressHandler:nil completionHandler:makeBlockPtr([weakSelf = WeakObjCPtr<WKContentView>(self), controller = RetainPtr { controller }] (CocoaImageAnalysis *result, NSError *) mutable {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        strongSelf->_fullscreenVideoImageAnalysisRequestIdentifier = 0;

        if ([controller respondsToSelector:@selector(setImageAnalysis:)])
            [controller setImageAnalysis:result];
    }).get()];
#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
}

- (void)cancelTextRecognitionForFullscreenVideo:(AVPlayerViewController *)controller
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if (auto identifier = std::exchange(_fullscreenVideoImageAnalysisRequestIdentifier, 0))
        [_imageAnalyzer cancelRequestID:identifier];

    if ([controller respondsToSelector:@selector(setImageAnalysis:)])
        [controller setImageAnalysis:nil];
#endif
}

- (BOOL)isTextRecognitionInFullscreenVideoEnabled
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    return _page->preferences().textRecognitionInVideosEnabled();
#else
    return NO;
#endif
}

- (void)beginTextRecognitionForVideoInElementFullscreen:(const WebKit::ShareableBitmapHandle&)bitmapHandle bounds:(WebCore::FloatRect)bounds
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    auto imageBitmap = WebKit::ShareableBitmap::create(bitmapHandle);
    if (!imageBitmap)
        return;

    auto image = imageBitmap->makeCGImage();
    if (!image)
        return;

    auto request = WebKit::createImageAnalyzerRequest(image.get(), analysisTypesForFullscreenVideo);
    _fullscreenVideoImageAnalysisRequestIdentifier = [self.imageAnalyzer processRequest:request.get() progressHandler:nil completionHandler:[weakSelf = WeakObjCPtr<WKContentView>(self), bounds](CocoaImageAnalysis *result, NSError *error) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || !strongSelf->_fullscreenVideoImageAnalysisRequestIdentifier)
            return;

        strongSelf->_fullscreenVideoImageAnalysisRequestIdentifier = 0;
        if (error || !result)
            return;

        strongSelf->_imageAnalysisInteractionBounds = bounds;
        [strongSelf installImageAnalysisInteraction:result];
    }];
#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
}

- (void)cancelTextRecognitionForVideoInElementFullscreen
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    [self uninstallImageAnalysisInteraction];

    if (auto previousIdentifier = std::exchange(_fullscreenVideoImageAnalysisRequestIdentifier, 0))
        [self.imageAnalyzer cancelRequestID:previousIdentifier];
#endif
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

- (void)installImageAnalysisInteraction:(VKCImageAnalysis *)analysis
{
    if (!_imageAnalysisInteraction) {
        _imageAnalysisActionButtons = adoptNS([[NSMutableSet alloc] initWithCapacity:1]);
        _imageAnalysisInteraction = adoptNS([PAL::allocVKCImageAnalysisInteractionInstance() init]);
        [_imageAnalysisInteraction setDelegate:self];
        [_imageAnalysisInteraction setAnalysisButtonRequiresVisibleContentGating:YES];
        [_imageAnalysisInteraction setQuickActionConfigurationUpdateHandler:[weakSelf = WeakObjCPtr<WKContentView>(self)] (UIButton *button) {
            if (auto strongSelf = weakSelf.get())
                [strongSelf->_imageAnalysisActionButtons addObject:button];
        }];
        WebKit::setUpAdditionalImageAnalysisBehaviors(_imageAnalysisInteraction.get());
        [self addInteraction:_imageAnalysisInteraction.get()];
        RELEASE_LOG(ImageAnalysis, "Installing image analysis interaction at {{ %.0f, %.0f }, { %.0f, %.0f }}",
            _imageAnalysisInteractionBounds.x(), _imageAnalysisInteractionBounds.y(), _imageAnalysisInteractionBounds.width(), _imageAnalysisInteractionBounds.height());
    }
    [_imageAnalysisInteraction setAnalysis:analysis];
    [_imageAnalysisDeferringGestureRecognizer setEnabled:NO];
    [_imageAnalysisGestureRecognizer setEnabled:NO];
}

- (void)uninstallImageAnalysisInteraction
{
    if (!_imageAnalysisInteraction)
        return;

    RELEASE_LOG(ImageAnalysis, "Uninstalling image analysis interaction");

    [self removeInteraction:_imageAnalysisInteraction.get()];
    [_imageAnalysisInteraction setDelegate:nil];
    [_imageAnalysisInteraction setQuickActionConfigurationUpdateHandler:nil];
    _imageAnalysisInteraction = nil;
    _imageAnalysisActionButtons = nil;
    _imageAnalysisInteractionBounds = { };
    [_imageAnalysisDeferringGestureRecognizer setEnabled:WebKit::isLiveTextAvailableAndEnabled()];
    [_imageAnalysisGestureRecognizer setEnabled:WebKit::isLiveTextAvailableAndEnabled()];
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

- (BOOL)_shouldAvoidSecurityHeuristicScoreUpdates
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    return [_imageAnalysisInteraction hasActiveTextSelection];
#else
    return NO;
#endif
}

#pragma mark - _UITextInputTranslationSupport

- (BOOL)isImageBacked
{
    return _page && _page->editorState().selectionIsRangeInsideImageOverlay;
}

#if HAVE(UI_EDIT_MENU_INTERACTION)

- (void)willPresentEditMenuWithAnimator:(id<UIEditMenuInteractionAnimating>)animator
{
    auto delegate = self.webView.UIDelegate;
    if (![delegate respondsToSelector:@selector(webView:willPresentEditMenuWithAnimator:)])
        return;

    [delegate webView:self.webView willPresentEditMenuWithAnimator:animator];
}

- (void)willDismissEditMenuWithAnimator:(id<UIEditMenuInteractionAnimating>)animator
{
    auto delegate = self.webView.UIDelegate;
    if (![delegate respondsToSelector:@selector(webView:willDismissEditMenuWithAnimator:)])
        return;

    [delegate webView:self.webView willDismissEditMenuWithAnimator:animator];
}

#endif // HAVE(UI_EDIT_MENU_INTERACTION)

@end

@implementation WKContentView (WKTesting)

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

#if !PLATFORM(WATCHOS)
- (WKDateTimeInputControl *)dateTimeInputControl
{
    if ([_inputPeripheral isKindOfClass:WKDateTimeInputControl.class])
        return (WKDateTimeInputControl *)_inputPeripheral.get();
    return nil;
}
#endif

- (WKFormSelectControl *)selectControl
{
    if ([_inputPeripheral isKindOfClass:WKFormSelectControl.class])
        return (WKFormSelectControl *)_inputPeripheral.get();
    return nil;
}

#if ENABLE(DRAG_SUPPORT)

- (BOOL)isAnimatingDragCancel
{
    return _isAnimatingDragCancel;
}

#endif // ENABLE(DRAG_SUPPORT)

- (void)_simulateTextEntered:(NSString *)text
{
#if HAVE(PEPPER_UI_CORE)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTextInputListViewController class]]) {
        [(WKTextInputListViewController *)_presentedFullScreenInputViewController.get() enterText:text];
        return;
    }

#if HAVE(QUICKBOARD_CONTROLLER)
    if (_presentedQuickboardController) {
        id <PUICQuickboardControllerDelegate> delegate = [_presentedQuickboardController delegate];
        ASSERT(delegate == self);
        auto string = adoptNS([[NSAttributedString alloc] initWithString:text]);
        [delegate quickboardController:_presentedQuickboardController.get() textInputValueDidChange:string.get()];
        return;
    }
#endif // HAVE(QUICKBOARD_CONTROLLER)
#endif // HAVE(PEPPER_UI_CORE)

    [self insertText:text];
}

- (void)_simulateElementAction:(_WKElementActionType)actionType atLocation:(CGPoint)location
{
    _layerTreeTransactionIdAtLastInteractionStart = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).lastCommittedLayerTreeTransactionID();
    [self doAfterPositionInformationUpdate:[actionType, self, protectedSelf = retainPtr(self)] (WebKit::InteractionInformationAtPosition info) {
        _WKActivatedElementInfo *elementInfo = [_WKActivatedElementInfo activatedElementInfoWithInteractionInformationAtPosition:info userInfo:nil];
        _WKElementAction *action = [_WKElementAction _elementActionWithType:actionType info:elementInfo assistant:_actionSheetAssistant.get()];
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
#if HAVE(PEPPER_UI_CORE)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKSelectMenuListViewController class]])
        [(WKSelectMenuListViewController *)_presentedFullScreenInputViewController.get() selectItemAtIndex:rowIndex];
#else
    if ([_inputPeripheral isKindOfClass:[WKFormSelectControl class]])
        [(WKFormSelectControl *)_inputPeripheral selectRow:rowIndex inComponent:0 extendingSelection:NO];
#endif
}

- (BOOL)selectFormAccessoryHasCheckedItemAtRow:(long)rowIndex
{
#if !HAVE(PEPPER_UI_CORE)
    if ([_inputPeripheral isKindOfClass:[WKFormSelectControl self]])
        return [(WKFormSelectControl *)_inputPeripheral selectFormAccessoryHasCheckedItemAtRow:rowIndex];
#endif
    return NO;
}

- (void)setSelectedColorForColorPicker:(UIColor *)color
{
    if ([_inputPeripheral isKindOfClass:[WKFormColorControl class]])
        [(WKFormColorControl *)_inputPeripheral selectColor:color];
}

- (NSString *)textContentTypeForTesting
{
#if HAVE(PEPPER_UI_CORE)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTextInputListViewController class]])
        return [(WKTextInputListViewController *)_presentedFullScreenInputViewController textInputContext].textContentType;
#if HAVE(QUICKBOARD_CONTROLLER)
    if (_presentedQuickboardController)
        return [_presentedQuickboardController textInputContext].textContentType;
#endif // HAVE(QUICKBOARD_CONTROLLER)
#endif // HAVE(PEPPER_UI_CORE)
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
#if HAVE(PEPPER_UI_CORE)
    return [self inputLabelTextForViewController:_presentedFullScreenInputViewController.get()];
#else
    return nil;
#endif
}

- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute
{
#if HAVE(PEPPER_UI_CORE)
    if ([_presentedFullScreenInputViewController isKindOfClass:[WKTimePickerViewController class]])
        [(WKTimePickerViewController *)_presentedFullScreenInputViewController.get() setHour:hour minute:minute];
#else
    if ([_inputPeripheral isKindOfClass:[WKDateTimeInputControl class]])
        [(WKDateTimeInputControl *)_inputPeripheral.get() setTimePickerHour:hour minute:minute];
#endif
}

- (double)timePickerValueHour
{
#if !PLATFORM(WATCHOS)
    if ([_inputPeripheral isKindOfClass:[WKDateTimeInputControl class]])
        return [(WKDateTimeInputControl *)_inputPeripheral.get() timePickerValueHour];
#endif
    return -1;
}

- (double)timePickerValueMinute
{
#if !PLATFORM(WATCHOS)
    if ([_inputPeripheral isKindOfClass:[WKDateTimeInputControl class]])
        return [(WKDateTimeInputControl *)_inputPeripheral.get() timePickerValueMinute];
#endif
    return -1;
}

- (NSDictionary *)_contentsOfUserInterfaceItem:(NSString *)userInterfaceItem
{
    if ([userInterfaceItem isEqualToString:@"actionSheet"])
        return @{ userInterfaceItem: [_actionSheetAssistant currentlyAvailableActionTitles] };

#if HAVE(LINK_PREVIEW)
    if ([userInterfaceItem isEqualToString:@"contextMenu"]) {
        auto itemTitles = adoptNS([NSMutableArray<NSString *> new]);
        [self.contextMenuInteraction updateVisibleMenuWithBlock:[&itemTitles](UIMenu *menu) -> UIMenu * {
            for (UIMenuElement *child in menu.children)
                [itemTitles addObject:child.title];
            return menu;
        }];
        if (self._shouldUseContextMenus) {
            return @{ userInterfaceItem: @{
                @"url": _positionInformation.url.isValid() ? WTF::userVisibleString(_positionInformation.url) : @"",
                @"isLink": [NSNumber numberWithBool:_positionInformation.isLink],
                @"isImage": [NSNumber numberWithBool:_positionInformation.isImage],
                @"imageURL": _positionInformation.imageURL.isValid() ? WTF::userVisibleString(_positionInformation.imageURL) : @"",
                @"items": itemTitles.get()
            } };
        }
        NSString *url = [_previewItemController previewData][UIPreviewDataLink];
        return @{ userInterfaceItem: @{
            @"url": url,
            @"isLink": [NSNumber numberWithBool:_positionInformation.isLink],
            @"isImage": [NSNumber numberWithBool:_positionInformation.isImage],
            @"imageURL": _positionInformation.imageURL.isValid() ? WTF::userVisibleString(_positionInformation.imageURL) : @"",
            @"items": itemTitles.get()
        } };
    }
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    if ([userInterfaceItem isEqualToString:@"mediaControlsContextMenu"])
        return @{ userInterfaceItem: [_actionSheetAssistant currentlyAvailableMediaControlsContextMenuItems] };
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

    if ([userInterfaceItem isEqualToString:@"fileUploadPanelMenu"]) {
        if (!_fileUploadPanel)
            return @{ userInterfaceItem: @[] };
        return @{ userInterfaceItem: [_fileUploadPanel currentAvailableActionTitles] };
    }
    
    return nil;
}

- (void)_dismissContactPickerWithContacts:(NSArray *)contacts
{
#if HAVE(CONTACTSUI)
    [_contactPicker dismissWithContacts:contacts];
#endif
}

- (void)_simulateSelectionStart
{
    _usingGestureForSelection = YES;
    _lastSelectionDrawingInfo.type = WebKit::WKSelectionDrawingInfo::SelectionType::Range;
}

#if ENABLE(DATALIST_ELEMENT)
- (void)_selectDataListOption:(NSInteger)optionIndex
{
    [_dataListSuggestionsControl didSelectOptionAtIndex:optionIndex];
}

- (void)_setDataListSuggestionsControl:(WKDataListSuggestionsControl *)control
{
    _dataListSuggestionsControl = control;
}

- (BOOL)isShowingDataListSuggestions
{
    return [_dataListSuggestionsControl isShowingSuggestions];
}
#endif

- (UIWKTextInteractionAssistant *)textInteractionAssistant
{
    return _textInteractionAssistant.get();
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

#if HAVE(TEXT_INTERACTION_WITH_CONTEXT_MENU_INTERACTION)
- (UIContextMenuInteraction *)textInteractionAssistantContextMenuInteraction
{
    return [_textInteractionAssistant respondsToSelector:@selector(contextMenuInteraction)] ? _textInteractionAssistant.get().contextMenuInteraction : nil;
}
#endif // HAVE(TEXT_INTERACTION_WITH_CONTEXT_MENU_INTERACTION)

#if USE(UICONTEXTMENU)
- (UIContextMenuInteraction *)contextMenuInteraction
{
#if HAVE(TEXT_INTERACTION_WITH_CONTEXT_MENU_INTERACTION)
    return _contextMenuInteraction.get() ?: self.textInteractionAssistantContextMenuInteraction;
#else
    return _contextMenuInteraction.get();
#endif
}
#endif // USE(UICONTEXTMENU)

- (void)_registerPreview
{
    if (!self.webView.allowsLinkPreview)
        return;

#if USE(UICONTEXTMENU)
    if (self._shouldUseContextMenus) {
        _contextMenuHasRequestedLegacyData = NO;
#if HAVE(TEXT_INTERACTION_WITH_CONTEXT_MENU_INTERACTION)
        if (self.textInteractionAssistantContextMenuInteraction)
            _textInteractionAssistant.get().externalContextMenuInteractionDelegate = self;
        else
#endif
        {
            _contextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
            [self addInteraction:_contextMenuInteraction.get()];
        }

        if (id<_UIClickInteractionDriving> driver = self.webView.configuration._clickInteractionDriverForTesting)
            [self.contextMenuInteraction presentationInteraction].overrideDrivers = @[driver];
        return;
    }
#endif // USE(UICONTEXTMENU)

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

static NSMutableArray<UIMenuElement *> *menuElementsFromDefaultActions(const RetainPtr<NSArray>& defaultElementActions, RetainPtr<_WKActivatedElementInfo> elementInfo)
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

    RetainPtr<UIViewController> previewViewController;

    auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithInteractionInformationAtPosition:_positionInformation isUsingAlternateURLForImage:NO userInfo:nil]);

    ASSERT_IMPLIES(_positionInformation.isImage, _positionInformation.image);
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
            BEGIN_BLOCK_OBJC_EXCEPTIONS
            NSDictionary *context = [self dataDetectionContextForPositionInformation:_positionInformation];
            RetainPtr<UIContextMenuConfiguration> dataDetectorsResult = [ddContextMenuActionClass contextMenuConfigurationForURL:url identifier:_positionInformation.dataDetectorIdentifier selectedText:self.selectedText results:_positionInformation.dataDetectorResults.get() inView:self context:context menuIdentifier:nil];
            if (_showLinkPreviews && dataDetectorsResult && dataDetectorsResult.get().previewProvider)
                _contextMenuLegacyPreviewController = dataDetectorsResult.get().previewProvider();
            if (dataDetectorsResult && dataDetectorsResult.get().actionProvider) {
                auto menuElements = menuElementsFromDefaultActions(defaultActionsFromAssistant, elementInfo);
                _contextMenuLegacyMenu = dataDetectorsResult.get().actionProvider(menuElements);
            }
            END_BLOCK_OBJC_EXCEPTIONS
            return;
        }

        _contextMenuLegacyMenu = menuFromLegacyPreviewOrDefaultActions(previewViewController.get(), defaultActionsFromAssistant, elementInfo);

    } else if (_positionInformation.isImage && _positionInformation.image) {
        NSURL *nsURL = (NSURL *)url;
        RetainPtr<NSDictionary> imageInfo;
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
            previewViewController = adoptNS([[WKImagePreviewViewController alloc] initWithCGImage:cgImage defaultActions:defaultActionsFromAssistant.get() elementInfo:elementInfo.get()]);
        ALLOW_DEPRECATED_DECLARATIONS_END

        _contextMenuLegacyMenu = menuFromLegacyPreviewOrDefaultActions(previewViewController.get(), defaultActionsFromAssistant, elementInfo, _positionInformation.title);
    }

    _contextMenuLegacyPreviewController = WTFMove(previewViewController);
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
    // Required to conform to UIContextMenuInteractionDelegate, but SPI version should be called instead.
    ASSERT_NOT_REACHED();
    return nil;
}

- (void)_contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location completion:(void(^)(UIContextMenuConfiguration *))completion
{
#if ENABLE(IMAGE_ANALYSIS)
    BOOL triggeredByImageAnalysisTimeout = std::exchange(_contextMenuWasTriggeredByImageAnalysisTimeout, NO);
#else
    BOOL triggeredByImageAnalysisTimeout = NO;
#endif

    if (!_webView)
        return completion(nil);

    if (!self.webView.configuration._longPressActionsEnabled)
        return completion(nil);

    auto getConfigurationAndContinue = [weakSelf = WeakObjCPtr<WKContentView>(self), interaction = retainPtr(interaction), completion = makeBlockPtr(completion), triggeredByImageAnalysisTimeout] (WebKit::ProceedWithTextSelectionInImage proceedWithTextSelectionInImage) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || proceedWithTextSelectionInImage == WebKit::ProceedWithTextSelectionInImage::Yes) {
            completion(nil);
            return;
        }

        strongSelf->_showLinkPreviews = true;
        if (NSNumber *value = [[NSUserDefaults standardUserDefaults] objectForKey:webkitShowLinkPreviewsPreferenceKey])
            strongSelf->_showLinkPreviews = value.boolValue;

        WebKit::InteractionInformationRequest request { WebCore::roundedIntPoint([interaction locationInView:strongSelf.get()]) };
        request.includeSnapshot = true;
        request.includeLinkIndicator = true;
        request.disallowUserAgentShadowContent = triggeredByImageAnalysisTimeout;
        request.linkIndicatorShouldHaveLegacyMargins = ![strongSelf _shouldUseContextMenus];

        [strongSelf doAfterPositionInformationUpdate:[weakSelf = WeakObjCPtr<WKContentView>(strongSelf.get()), completion] (WebKit::InteractionInformationAtPosition) {
            if (auto strongSelf = weakSelf.get())
                [strongSelf continueContextMenuInteraction:completion.get()];
            else
                completion(nil);
        } forRequest:request];
    };

#if ENABLE(IMAGE_ANALYSIS)
    [self _doAfterPendingImageAnalysis:getConfigurationAndContinue];
#else
    getConfigurationAndContinue(WebKit::ProceedWithTextSelectionInImage::No);
#endif
}

- (UIAction *)placeholderForDynamicallyInsertedImageAnalysisActions
{
#if ENABLE(IMAGE_ANALYSIS)
    if (_waitingForDynamicImageAnalysisContextMenuActions) {
        // FIXME: This placeholder item warrants its own unique item identifier; however, to ensure that internal clients
        // that already allow image analysis items (e.g. Mail) continue to allow this placeholder item, we reuse an existing
        // image analysis identifier.
        RetainPtr placeholder = [UIAction actionWithTitle:@"" image:nil identifier:elementActionTypeToUIActionIdentifier(_WKElementActionTypeRevealImage) handler:^(UIAction *) { }];
        [placeholder setAttributes:UIMenuElementAttributesHidden];
        return placeholder.autorelease();
    }
#endif // ENABLE(IMAGE_ANALYSIS)
    return nil;
}

- (void)continueContextMenuInteraction:(void(^)(UIContextMenuConfiguration *))continueWithContextMenuConfiguration
{
    if (!self.window)
        return continueWithContextMenuConfiguration(nil);

    if (!_positionInformation.touchCalloutEnabled)
        return continueWithContextMenuConfiguration(nil);

    if (!_positionInformation.isLink && !_positionInformation.isImage && !_positionInformation.isAttachment && !self.positionInformationHasImageOverlayDataDetector)
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
        _contextMenuIsUsingAlternateURLForImage = NO;

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

        bool canShowHTTPLinkOrDataDetectorPreview = ([&] {
            if (linkURL.protocolIsInHTTPFamily())
                return true;

            if (WebCore::DataDetection::canBePresentedByDataDetectors(linkURL))
                return true;

            if ([strongSelf positionInformationHasImageOverlayDataDetector])
                return true;

            return false;
        })();

        ASSERT_IMPLIES(strongSelf->_positionInformation.isImage, strongSelf->_positionInformation.image);
        if (strongSelf->_positionInformation.isImage && strongSelf->_positionInformation.image && !canShowHTTPLinkOrDataDetectorPreview) {
            auto cgImage = strongSelf->_positionInformation.image->makeCGImageCopy();

            strongSelf->_contextMenuActionProviderDelegateNeedsOverride = NO;

            auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithInteractionInformationAtPosition:strongSelf->_positionInformation isUsingAlternateURLForImage:strongSelf->_contextMenuIsUsingAlternateURLForImage userInfo:nil]);

            UIContextMenuActionProvider actionMenuProvider = [weakSelf, elementInfo] (NSArray<UIMenuElement *> *) -> UIMenu * {
                auto strongSelf = weakSelf.get();
                if (!strongSelf)
                    return nil;

                RetainPtr<NSArray<_WKElementAction *>> defaultActionsFromAssistant = [strongSelf->_actionSheetAssistant defaultActionsForImageSheet:elementInfo.get()];
                auto actions = menuElementsFromDefaultActions(defaultActionsFromAssistant, elementInfo);
#if ENABLE(IMAGE_ANALYSIS)
                if (auto *placeholder = [strongSelf placeholderForDynamicallyInsertedImageAnalysisActions])
                    [actions addObject:placeholder];
                else if (UIMenu *menu = [strongSelf machineReadableCodeSubMenuForImageContextMenu])
                    [actions addObject:menu];
#endif // ENABLE(IMAGE_ANALYSIS)
                return [UIMenu menuWithTitle:strongSelf->_positionInformation.title children:actions];
            };

            UIContextMenuContentPreviewProvider contentPreviewProvider = [weakSelf, cgImage, elementInfo] () -> UIViewController * {
                auto strongSelf = weakSelf.get();
                if (!strongSelf)
                    return nil;

                auto uiDelegate = static_cast<id<WKUIDelegatePrivate>>([strongSelf webViewUIDelegate]);
                if ([uiDelegate respondsToSelector:@selector(_webView:contextMenuContentPreviewForElement:)]) {
                    if (UIViewController *previewViewController = [uiDelegate _webView:[strongSelf webView] contextMenuContentPreviewForElement:strongSelf->_contextMenuElementInfo.get()])
                        return previewViewController;
                }

                return adoptNS([[WKImagePreviewViewController alloc] initWithCGImage:cgImage defaultActions:nil elementInfo:elementInfo.get()]).autorelease();
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
        if (!canShowHTTPLinkOrDataDetectorPreview) {
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
    _contextMenuIsUsingAlternateURLForImage = NO;
    _contextMenuElementInfo = wrapper(API::ContextMenuElementInfo::create(_positionInformation, nil));

    if (_positionInformation.isImage && _positionInformation.url.isNull() && [uiDelegate respondsToSelector:@selector(_webView:alternateURLFromImage:userInfo:)]) {
        UIImage *uiImage = [[_contextMenuElementInfo _activatedElementInfo] image];
        NSDictionary *userInfo = nil;
        NSURL *nsURL = [uiDelegate _webView:self.webView alternateURLFromImage:uiImage userInfo:&userInfo];
        _positionInformation.url = nsURL;
        if (nsURL)
            _contextMenuIsUsingAlternateURLForImage = YES;
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
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto ddContextMenuActionClass = getDDContextMenuActionClass();
    auto context = retainPtr([self dataDetectionContextForPositionInformation:_positionInformation]);
    RetainPtr<UIContextMenuConfiguration> configurationFromDataDetectors;

    if (self.positionInformationHasImageOverlayDataDetector) {
        DDScannerResult *scannerResult = [_positionInformation.dataDetectorResults firstObject];
        configurationFromDataDetectors = [ddContextMenuActionClass contextMenuConfigurationWithResult:scannerResult.coreResult inView:self context:context.get() menuIdentifier:nil];
    } else {
        configurationFromDataDetectors = [ddContextMenuActionClass contextMenuConfigurationForURL:_positionInformation.url identifier:_positionInformation.dataDetectorIdentifier selectedText:[self selectedText] results:_positionInformation.dataDetectorResults.get() inView:self context:context.get() menuIdentifier:nil];
        _page->startInteractionWithPositionInformation(_positionInformation);
    }

    _contextMenuActionProviderDelegateNeedsOverride = YES;
    continueWithContextMenuConfiguration(configurationFromDataDetectors.get());
    END_BLOCK_OBJC_EXCEPTIONS
}

#endif // ENABLE(DATA_DETECTION)

- (NSArray<UIMenuElement *> *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction overrideSuggestedActionsForConfiguration:(UIContextMenuConfiguration *)configuration
{
    // If we're here we're in the legacy path, which ignores the suggested actions anyway.
    if (!_contextMenuActionProviderDelegateNeedsOverride)
        return nil;

    auto *suggestedActions = [_actionSheetAssistant suggestedActionsForContextMenuWithPositionInformation:_positionInformation];
    if (auto *placeholder = self.placeholderForDynamicallyInsertedImageAnalysisActions)
        [suggestedActions addObject:placeholder];
    return suggestedActions;
}

#if HAVE(UI_CONTEXT_MENU_PREVIEW_ITEM_IDENTIFIER)
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configuration:(UIContextMenuConfiguration *)configuration highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
#else
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
#endif
{
    [self _startSuppressingSelectionAssistantForReason:WebKit::InteractionIsHappening];
    [self _cancelTouchEventGestureRecognizer];
    return [self _createTargetedContextMenuHintPreviewIfPossible];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
{
    if (!_webView)
        return;

    _isDisplayingContextMenuWithAnimation = YES;
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKContentView>(self)] {
        if (auto strongSelf = weakSelf.get()) {
            ASSERT_IMPLIES(strongSelf->_isDisplayingContextMenuWithAnimation, [strongSelf->_contextMenuHintContainerView window]);
            strongSelf->_isDisplayingContextMenuWithAnimation = NO;
            [strongSelf->_webView _didShowContextMenu];
        }
    }];

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

#if HAVE(UI_CONTEXT_MENU_PREVIEW_ITEM_IDENTIFIER)
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configuration:(UIContextMenuConfiguration *)configuration dismissalPreviewForItemWithIdentifier:(id<NSCopying>)identifier
#else
- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForDismissingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
#endif
{
    return std::exchange(_contextMenuInteractionTargetedPreview, nil).autorelease();
}

- (_UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
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
                if (imageURL.isEmpty() || !(imageURL.protocolIsInHTTPFamily() || imageURL.protocolIs("data"_s)))
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

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id<UIContextMenuInteractionAnimating>)animator
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

        strongSelf->_isDisplayingContextMenuWithAnimation = NO;
        [strongSelf _removeContextMenuHintContainerIfPossible];
        [strongSelf->_webView _didDismissContextMenu];
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

    auto dataForPreview = adoptNS([[NSMutableDictionary alloc] init]);
    if (canShowLinkPreview) {
        *type = UIPreviewItemTypeLink;
        if (useImageURLForLink)
            dataForPreview.get()[UIPreviewDataLink] = (NSURL *)_positionInformation.imageURL;
        else
            dataForPreview.get()[UIPreviewDataLink] = (NSURL *)linkURL;
#if ENABLE(DATA_DETECTION)
        if (isDataDetectorLink) {
            NSDictionary *context = nil;
            if ([uiDelegate respondsToSelector:@selector(_dataDetectionContextForWebView:)])
                context = [uiDelegate _dataDetectionContextForWebView:self.webView];

            DDDetectionController *controller = [getDDDetectionControllerClass() sharedController];
            NSDictionary *newContext = nil;
            RetainPtr<NSMutableDictionary> extendedContext;
            DDResultRef ddResult = [controller resultForURL:dataForPreview.get()[UIPreviewDataLink] identifier:_positionInformation.dataDetectorIdentifier selectedText:[self selectedText] results:_positionInformation.dataDetectorResults.get() context:context extendedContext:&newContext];
            if (ddResult)
                dataForPreview.get()[UIPreviewDataDDResult] = (__bridge id)ddResult;
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
                dataForPreview.get()[UIPreviewDataDDContext] = newContext;
        }
#endif // ENABLE(DATA_DETECTION)
    } else if (canShowImagePreview) {
        *type = UIPreviewItemTypeImage;
        dataForPreview.get()[UIPreviewDataLink] = (NSURL *)_positionInformation.imageURL;
    } else if (canShowAttachmentPreview) {
        *type = UIPreviewItemTypeAttachment;
        auto element = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeAttachment URL:(NSURL *)linkURL imageURL:(NSURL *)_positionInformation.imageURL location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:nil imageMIMEType:_positionInformation.imageMIMEType]);
        NSUInteger index = [uiDelegate _webView:self.webView indexIntoAttachmentListForElement:element.get()];
        if (index != NSNotFound) {
            BOOL sourceIsManaged = NO;
            if (respondsToAttachmentListForWebViewSourceIsManaged)
                dataForPreview.get()[UIPreviewDataAttachmentList] = [uiDelegate _attachmentListForWebView:self.webView sourceIsManaged:&sourceIsManaged];
            else
                dataForPreview.get()[UIPreviewDataAttachmentList] = [uiDelegate _attachmentListForWebView:self.webView];
            dataForPreview.get()[UIPreviewDataAttachmentIndex] = [NSNumber numberWithUnsignedInteger:index];
            dataForPreview.get()[UIPreviewDataAttachmentListIsContentManaged] = [NSNumber numberWithBool:sourceIsManaged];
        }
    }

    return dataForPreview.autorelease();
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
    bool isValidURLForImagePreview = !coreTargetURL.isEmpty() && (coreTargetURL.protocolIsInHTTPFamily() || coreTargetURL.protocolIs("data"_s));

    if ([_previewItemController type] == UIPreviewItemTypeLink) {
        _longPressCanClick = NO;
        _page->startInteractionWithPositionInformation(_positionInformation);

        // Treat animated images like a link preview
        if (isValidURLForImagePreview && _positionInformation.isAnimatedImage) {
            auto animatedImageElementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:targetURL imageURL:nil location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:_positionInformation.image.get() imageMIMEType:_positionInformation.imageMIMEType]);

            if ([uiDelegate respondsToSelector:@selector(_webView:previewViewControllerForAnimatedImageAtURL:defaultActions:elementInfo:imageSize:)]) {
                RetainPtr<NSArray> actions = [_actionSheetAssistant defaultActionsForImageSheet:animatedImageElementInfo.get()];
                ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                return [uiDelegate _webView:self.webView previewViewControllerForAnimatedImageAtURL:targetURL defaultActions:actions.get() elementInfo:animatedImageElementInfo.get() imageSize:_positionInformation.image->size()];
                ALLOW_DEPRECATED_DECLARATIONS_END
            }
        }

        auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeLink URL:targetURL imageURL:nil location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:_positionInformation.image.get() imageMIMEType:_positionInformation.imageMIMEType]);

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

        auto elementInfo = adoptNS([[_WKActivatedElementInfo alloc] _initWithType:_WKActivatedElementTypeImage URL:alternateURL.get() imageURL:nil location:_positionInformation.request.point title:_positionInformation.title ID:_positionInformation.idAttribute rect:_positionInformation.bounds image:_positionInformation.image.get() imageMIMEType:_positionInformation.imageMIMEType userInfo:imageInfo.get()]);
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

        return adoptNS([[WKImagePreviewViewController alloc] initWithCGImage:cgImage defaultActions:defaultActions elementInfo:elementInfo]).autorelease();
    }

    return nil;
}

- (void)_previewItemController:(UIPreviewItemController *)controller commitPreview:(UIViewController *)viewController
{
    id <WKUIDelegatePrivate> uiDelegate = static_cast<id <WKUIDelegatePrivate>>([_webView UIDelegate]);
    if ([_previewItemController type] == UIPreviewItemTypeImage) {
        if ([uiDelegate respondsToSelector:@selector(_webView:commitPreviewedImageWithURL:)]) {
            const URL& imageURL = _positionInformation.imageURL;
            if (imageURL.isEmpty() || !(imageURL.protocolIsInHTTPFamily() || imageURL.protocolIs("data"_s)))
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

    auto nativeImage = _positionInformation.linkIndicator.contentImage->nativeImage();
    if (!nativeImage)
        return nullptr;

    return adoptNS([[UIImage alloc] initWithCGImage:nativeImage->platformImage().get()]).autorelease();
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
    auto range = adoptNS([[WKTextRange alloc] init]);
    [range setIsNone:isNone];
    [range setIsRange:isRange];
    [range setIsEditable:isEditable];
    [range setStartRect:startRect];
    [range setEndRect:endRect];
    [range setSelectedTextLength:selectedTextLength];
    [range setSelectionRects:selectionRects];
    return range.autorelease();
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
    auto pos = adoptNS([[WKTextPosition alloc] init]);
    [pos setPositionRect:positionRect];
    return pos.autorelease();
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

#if HAVE(UIFINDINTERACTION)

@implementation WKFoundTextRange


+ (WKFoundTextRange *)foundTextRangeWithWebFoundTextRange:(WebKit::WebFoundTextRange)webRange
{
    auto range = adoptNS([[WKFoundTextRange alloc] init]);
    [range setLocation:webRange.location];
    [range setLength:webRange.length];
    [range setFrameIdentifier:webRange.frameIdentifier];
    [range setOrder:webRange.order];
    return range.autorelease();
}

- (WKFoundTextPosition *)start
{
    WKFoundTextPosition *position = [WKFoundTextPosition textPositionWithOffset:self.location order:self.order];
    return position;
}

- (UITextPosition *)end
{
    WKFoundTextPosition *position = [WKFoundTextPosition textPositionWithOffset:(self.location + self.length) order:self.order];
    return position;
}

- (BOOL)isEmpty
{
    return NO;
}

- (WebKit::WebFoundTextRange)webFoundTextRange
{
    return { self.location, self.length, self.frameIdentifier, self.order };
}

@end

@implementation WKFoundTextPosition

+ (WKFoundTextPosition *)textPositionWithOffset:(NSUInteger)offset order:(NSUInteger)order
{
    auto pos = adoptNS([[WKFoundTextPosition alloc] init]);
    [pos setOffset:offset];
    [pos setOrder:order];
    return pos.autorelease();
}

@end

#endif

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
