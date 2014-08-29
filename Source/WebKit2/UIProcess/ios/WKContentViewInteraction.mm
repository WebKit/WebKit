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

#import "EditingRange.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebTouchEvent.h"
#import "SmartMagnificationController.h"
#import "WKActionSheetAssistant.h"
#import "WKFormInputControl.h"
#import "WKFormSelectControl.h"
#import "WKInspectorNodeSearchGestureRecognizer.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebEvent.h"
#import "WebIOSEventFactory.h"
#import "WebPageMessages.h"
#import "WebProcessProxy.h"
#import "_WKFormDelegate.h"
#import "_WKFormInputSession.h"
#import <CoreText/CTFontDescriptor.h>
#import <DataDetectorsUI/DDDetectionController.h>
#import <TextInput/TI_NSStringExtras.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIFont_Private.h>
#import <UIKit/UIGestureRecognizer_Private.h>
#import <UIKit/UIKeyboardImpl.h>
#import <UIKit/UIKeyboardIntl.h>
#import <UIKit/UILongPressGestureRecognizer_Private.h>
#import <UIKit/UITapGestureRecognizer_Private.h>
#import <UIKit/UITextInteractionAssistant_Private.h>
#import <UIKit/UIWebDocumentView.h> // FIXME: should not include this header.
#import <UIKit/_UIHighlightView.h>
#import <UIKit/_UIWebHighlightLongPressGestureRecognizer.h>
#import <WebCore/Color.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebEvent.h>
#import <WebKit/WebSelectionRect.h> // FIXME: WK2 should not include WebKit headers!
#import <wtf/RetainPtr.h>

SOFT_LINK_PRIVATE_FRAMEWORK(DataDetectorsUI)
SOFT_LINK_CLASS(DataDetectorsUI, DDDetectionController)

using namespace WebCore;
using namespace WebKit;

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
@end

@interface UIWKSelectionAssistant (StagingToRemove)
- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
@end

@interface UIKeyboardImpl (StagingToRemove)
- (void)didHandleWebKeyEvent;
@end

@interface UIView (UIViewInternalHack)
+ (BOOL)_addCompletion:(void(^)(BOOL))completion;
@end

@interface WKFormInputSession : NSObject <_WKFormInputSession>

- (instancetype)initWithContentView:(WKContentView *)view userObject:(NSObject <NSSecureCoding> *)userObject;
- (void)invalidate;

@end

@implementation WKFormInputSession {
    WKContentView *_contentView;
    RetainPtr<NSObject <NSSecureCoding>> _userObject;
}

- (instancetype)initWithContentView:(WKContentView *)view userObject:(NSObject <NSSecureCoding> *)userObject
{
    if (!(self = [super init]))
        return nil;

    _contentView = view;
    _userObject = userObject;

    return self;
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
}

- (void)invalidate
{
    _contentView = nil;
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

- (void)setupInteraction
{
    if (!_interactionViewsContainerView) {
        _interactionViewsContainerView = adoptNS([[UIView alloc] init]);
        [_interactionViewsContainerView setOpaque:NO];
        [_interactionViewsContainerView layer].anchorPoint = CGPointZero;
        [self.superview addSubview:_interactionViewsContainerView.get()];
    }

    [self.layer addObserver:self forKeyPath:@"transform" options:NSKeyValueObservingOptionInitial context:nil];

    _touchEventGestureRecognizer = adoptNS([[UIWebTouchEventsGestureRecognizer alloc] initWithTarget:self action:@selector(_webTouchEventsRecognized:) touchDelegate:self]);
    [_touchEventGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_touchEventGestureRecognizer.get()];

    _singleTapGestureRecognizer = adoptNS([[WKSyntheticClickTapGestureRecognizer alloc] initWithTarget:self action:@selector(_singleTapCommited:)]);
    [_singleTapGestureRecognizer setDelegate:self];
    [_singleTapGestureRecognizer setGestureRecognizedTarget:self action:@selector(_singleTapRecognized:)];
    [_singleTapGestureRecognizer setResetTarget:self action:@selector(_singleTapDidReset:)];
    [self addGestureRecognizer:_singleTapGestureRecognizer.get()];

    _doubleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_doubleTapRecognized:)]);
    [_doubleTapGestureRecognizer setNumberOfTapsRequired:2];
    [_doubleTapGestureRecognizer setDelegate:self];
    [self addGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [_singleTapGestureRecognizer requireOtherGestureToFail:_doubleTapGestureRecognizer.get()];

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
    [self addGestureRecognizer:_longPressGestureRecognizer.get()];

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_resetShowingTextStyle:) name:UIMenuControllerDidHideMenuNotification object:nil];
    _showingTextStyleOptions = NO;

    // FIXME: This should be called when we get notified that loading has completed.
    [self useSelectionAssistantWithMode:toUIWebSelectionMode([[_webView configuration] selectionGranularity])];
    
    _actionSheetAssistant = adoptNS([[WKActionSheetAssistant alloc] initWithView:self]);
    _smartMagnificationController = std::make_unique<SmartMagnificationController>(self);
}

- (void)cleanupInteraction
{
    _webSelectionAssistant = nil;
    _textSelectionAssistant = nil;
    _actionSheetAssistant = nil;
    _smartMagnificationController = nil;
    _didAccessoryTabInitiateFocus = NO;
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
    [self removeGestureRecognizer:_longPressGestureRecognizer.get()];
    [self removeGestureRecognizer:_doubleTapGestureRecognizer.get()];
    [self removeGestureRecognizer:_twoFingerDoubleTapGestureRecognizer.get()];
}

- (void)_addDefaultGestureRecognizers
{
    [self addGestureRecognizer:_touchEventGestureRecognizer.get()];
    [self addGestureRecognizer:_singleTapGestureRecognizer.get()];
    [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];
    [self addGestureRecognizer:_longPressGestureRecognizer.get()];
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
    [self _updateChangedSelection];
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
    // We might want to return something else
    // if we decide to enable/disable interaction programmatically.
    return YES;
}

- (BOOL)resignFirstResponder
{
    // FIXME: Maybe we should call resignFirstResponder on the superclass
    // and do nothing if the return value is NO.
    // We need to complete the editing operation before we blur the element.
    [_inputPeripheral endEditing];
    _page->blurAssistedNode();
    [self _cancelInteraction];
    [_webSelectionAssistant resignedFirstResponder];

    return [super resignFirstResponder];
}

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

- (void)_webTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsNativeGesture
{
    if (preventsNativeGesture) {
        _canSendTouchEventsAsynchronously = YES;
        [_touchEventGestureRecognizer setDefaultPrevented:YES];
    }
}

static inline bool highlightedQuadsAreSmallerThanRect(const Vector<FloatQuad>& quads, const FloatRect& rect)
{
    for (size_t i = 0; i < quads.size(); ++i) {
        FloatRect boundingBox = quads[i].boundingBox();
        if (boundingBox.width() > rect.width() || boundingBox.height() > rect.height())
            return false;
    }
    return true;
}

static NSValue *nsSizeForTapHighlightBorderRadius(WebCore::IntSize borderRadius)
{
    return [NSValue valueWithCGSize:CGSizeMake(borderRadius.width() + minimumTapHighlightRadius, borderRadius.height() + minimumTapHighlightRadius)];
}

- (void)_updateTapHighlight
{
    if (![_highlightView superview])
        return;

    {
        RetainPtr<UIColor> highlightUIKitColor = adoptNS([[UIColor alloc] initWithCGColor:cachedCGColor(_tapHighlightInformation.color, WebCore::ColorSpaceDeviceRGB)]);
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
            CGRect pixelAlignedRect = static_cast<CGRect>(enclosingRectExtendedToDevicePixels(boundingBox, deviceScaleFactor));
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
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.topLeftRadius)];
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.topRightRadius)];
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.bottomLeftRadius)];
    [borderRadii addObject:nsSizeForTapHighlightBorderRadius(_tapHighlightInformation.bottomRightRadius)];
    [_highlightView setCornerRadii:borderRadii.get()];
}

- (void)_showTapHighlight
{
    if (!highlightedQuadsAreSmallerThanRect(_tapHighlightInformation.quads, _page->unobscuredContentRect()))
        return;

    if (!_highlightView) {
        _highlightView = adoptNS([[_UIHighlightView alloc] initWithFrame:CGRectZero]);
        [_highlightView setOpaque:NO];
        [_highlightView setCornerRadius:minimumTapHighlightRadius];
    }
    [_highlightView layer].opacity = 1;
    [_interactionViewsContainerView addSubview:_highlightView.get()];
    [self _updateTapHighlight];
}

- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius
{
    if (!_isTapHighlightIDValid || _latestTapHighlightID != requestID)
        return;

    _isTapHighlightIDValid = NO;

    _tapHighlightInformation.color = color;
    _tapHighlightInformation.quads = highlightedQuads;
    _tapHighlightInformation.topLeftRadius = topLeftRadius;
    _tapHighlightInformation.topRightRadius = topRightRadius;
    _tapHighlightInformation.bottomLeftRadius = bottomLeftRadius;
    _tapHighlightInformation.bottomRightRadius = bottomRightRadius;

    if (_potentialTapInProgress) {
        _hasTapHighlightForPotentialTap = YES;
        return;
    }

    [self _showTapHighlight];
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

    return [_inputPeripheral assistantView];
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
    if (_positionInformation.clickableElementName == "IMG")
        return @selector(_showImageSheet);
    else if (_positionInformation.clickableElementName == "A") {
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

- (void)_updatePositionInformation
{
    _hasValidPositionInformation = NO;
    _page->requestPositionInformation(_positionInformation.point);
}

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
            if (_positionInformation.clickableElementName.isNull())
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
    [UIView animateWithDuration:0.1
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
    unsigned size = _page->editorState().selectionRects.size();
    if (!size)
        return nil;

    NSMutableArray *webRects = [NSMutableArray arrayWithCapacity:size];
    for (unsigned i = 0; i < size; i++) {
        const WebCore::SelectionRect& coreRect = _page->editorState().selectionRects[i];
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
        cancelPotentialTapIfNecessary(self);
        _page->tapHighlightAtPosition([gestureRecognizer startPoint], ++_latestTapHighlightID);
        _isTapHighlightIDValid = YES;
        break;
    case UIGestureRecognizerStateEnded:
        if (!_positionInformation.clickableElementName.isEmpty()) {
            [self _attemptClickAtLocation:[gestureRecognizer startPoint]];
            [self _finishInteraction];
        } else
            [self _cancelInteraction];
        break;
    case UIGestureRecognizerStateCancelled:
        [self _cancelInteraction];
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
            [UIApp _cancelAllTouches];
        }
    }
}

- (void)_singleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);
    ASSERT(!_potentialTapInProgress);

    _page->potentialTapAtPosition(gestureRecognizer.location, ++_latestTapHighlightID);
    _potentialTapInProgress = YES;
    _isTapHighlightIDValid = YES;
}

static void cancelPotentialTapIfNecessary(WKContentView* contentView)
{
    if (contentView->_potentialTapInProgress) {
        contentView->_potentialTapInProgress = NO;
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
    if (!_page->editorState().isContentEditable)
        [_webSelectionAssistant clearSelection];

    _lastInteractionLocation = gestureRecognizer.location;

    _potentialTapInProgress = NO;

    if (_hasTapHighlightForPotentialTap) {
        [self _showTapHighlight];
        _hasTapHighlightForPotentialTap = NO;
    }

    _page->commitPotentialTap();

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

    _page->process().send(Messages::WebPage::HandleTap(IntPoint(location)), _page->pageID());
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

        [_textSelectionAssistant activateSelection];
    }
}

- (void)clearSelection
{
    _page->clearSelection();
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
        return YES;
    case InputType::ContentEditable:
    case InputType::TextArea:
        return YES;
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

- (UIView *)inputAccessoryView
{
    if (![self requiresAccessoryView])
        return nil;

    if (!_formAccessoryView) {
        _formAccessoryView = adoptNS([[UIWebFormAccessory alloc] init]);
        [_formAccessoryView setDelegate:self];
    }
    
    return _formAccessoryView.get();
}

- (NSArray *)supportedPasteboardTypesForCurrentSelection
{
    if (_page->editorState().selectionIsNone)
        return nil;
    
    static NSMutableArray *richTypes = nil;
    static NSMutableArray *plainTextTypes = nil;
    if (!plainTextTypes) {
        plainTextTypes = [[NSMutableArray alloc] init];
        // FIXME: should add [plainTextTypes addObject:(id)kUTTypeURL];
        // when we figure out how to share this type between WebCore and WebKit2
        [plainTextTypes addObjectsFromArray:UIPasteboardTypeListString];

        richTypes = [[NSMutableArray alloc] init];
        // FIXME: should add [richTypes addObject:(PasteboardTypes::WebArchivePboardType)];
        // when we figure out how to share this type between WebCore and WebKit2
        [richTypes addObjectsFromArray:UIPasteboardTypeListImage];
        [richTypes addObjectsFromArray:plainTextTypes];
    }

    return (_page->editorState().isContentRichlyEditable) ? richTypes : plainTextTypes;
}

- (void)_addShortcut:(id)sender
{
    if (_textSelectionAssistant && [_textSelectionAssistant respondsToSelector:@selector(showTextServiceFor:fromRect:)])
        [_textSelectionAssistant showTextServiceFor:[self selectedText] fromRect:_page->editorState().selectionRects[0].rect()];
    else if (_webSelectionAssistant && [_webSelectionAssistant respondsToSelector:@selector(showTextServiceFor:fromRect:)])
        [_webSelectionAssistant showTextServiceFor:[self selectedText] fromRect:_page->editorState().selectionRects[0].rect()];
}

- (NSString *)selectedText
{
    return (NSString *)_page->editorState().wordAtSelection;
}

- (BOOL)isReplaceAllowed
{
    return _page->editorState().isReplaceAllowed;
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
    if (_page->editorState().wordAtSelection.isEmpty())
        return;

    if ([_textSelectionAssistant respondsToSelector:@selector(scheduleReplacementsForText:)])
        [_textSelectionAssistant scheduleReplacementsForText:_page->editorState().wordAtSelection];
}

- (void)_transliterateChinese:(id)sender
{
    if ([_textSelectionAssistant respondsToSelector:@selector(scheduleChineseTransliterationForText:)])
        [_textSelectionAssistant scheduleChineseTransliterationForText:_page->editorState().wordAtSelection];
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

    CTFontSymbolicTraits symbolicTraits = 0;
    if (_page->editorState().typingAttributes & AttributeBold)
        symbolicTraits |= kCTFontBoldTrait;
    if (_page->editorState().typingAttributes & AttributeItalics)
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
    
    if (_page->editorState().typingAttributes & AttributeUnderline)
        [result setObject:[NSNumber numberWithInt:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];

    return result;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender
{
    BOOL hasWebSelection = _webSelectionAssistant && !CGRectIsEmpty(_webSelectionAssistant.get().selectionFrame);

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

        NSUInteger textLength = _page->editorState().selectedTextLength;
        // FIXME: We should be calling UIReferenceLibraryViewController to check if the length is
        // acceptable, but the interface takes a string.
        // <rdar://problem/15254406>
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
        if (!_page->editorState().selectionIsRange || !_page->editorState().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        if ([[self selectedText] _containsCJScriptsOnly])
            return NO;
        return YES;
    }

    if (action == @selector(_transliterateChinese:)) {
        if (!_page->editorState().selectionIsRange || !_page->editorState().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
            return NO;
        return UIKeyboardEnabledInputModesAllowChineseTransliterationForText([self selectedText]);
    }

    if (action == @selector(_reanalyze:)) {
        if (!_page->editorState().selectionIsRange || !_page->editorState().isReplaceAllowed || ![[UIKeyboardImpl activeInstance] autocorrectSpellingEnabled])
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

- (void)_performAction:(SheetAction)action
{
    _page->performActionOnElement((uint32_t)action);
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
    CGRect presentationRect = _page->editorState().selectionRects[0].rect();
    if (_textSelectionAssistant)
        [_textSelectionAssistant showDictionaryFor:text fromRect:presentationRect];
    else
        [_webSelectionAssistant showDictionaryFor:text fromRect:presentationRect];
}

- (void)_define:(id)sender
{
    _page->getSelectionOrContentsAsString([self](const String& string, CallbackBase::Error error) {
        if (error != CallbackBase::Error::None)
            return;
        if (!string)
            return;

        [self _showDictionary:string];
    });
}

- (void)accessibilityRetrieveSpeakSelectionContent
{
    _page->getSelectionOrContentsAsString([self](const String& string, CallbackBase::Error error) {
        if (error != CallbackBase::Error::None)
            return;
        if ([self respondsToSelector:@selector(accessibilitySpeakSelectionSetContent:)])
            [self accessibilitySpeakSelectionSetContent:string];
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

static void selectionChangedWithGesture(WKContentView *view, const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, CallbackBase::Error error)
{
    if (error != CallbackBase::Error::None) {
        ASSERT_NOT_REACHED();
        return;
    }
    if ([view webSelectionAssistant])
        [(UIWKSelectionAssistant *)[view webSelectionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType((GestureType)gestureType) withState:toUIGestureRecognizerState(static_cast<GestureRecognizerState>(gestureState)) withFlags:(toUIWKSelectionFlags((SelectionFlags)flags))];
    else
        [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType((GestureType)gestureType) withState:toUIGestureRecognizerState(static_cast<GestureRecognizerState>(gestureState)) withFlags:(toUIWKSelectionFlags((SelectionFlags)flags))];
}

static void selectionChangedWithTouch(WKContentView *view, const WebCore::IntPoint& point, uint32_t touch, CallbackBase::Error error)
{
    if (error != CallbackBase::Error::None) {
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

- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state
{
    _usingGestureForSelection = YES;
    _page->selectWithGesture(WebCore::IntPoint(point), CharacterGranularity, static_cast<uint32_t>(toGestureType(gestureType)), static_cast<uint32_t>(toGestureRecognizerState(state)), [self, state](const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, CallbackBase::Error error) {
        selectionChangedWithGesture(self, point, gestureType, gestureState, flags, error);
        if (state == UIGestureRecognizerStateEnded || state == UIGestureRecognizerStateCancelled)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart
{
    _usingGestureForSelection = YES;
    _page->updateSelectionWithTouches(WebCore::IntPoint(point), static_cast<uint32_t>(toSelectionTouch(touch)), baseIsStart, [self, touch](const WebCore::IntPoint& point, uint32_t touch, CallbackBase::Error error) {
        selectionChangedWithTouch(self, point, touch, error);
        if (touch != UIWKSelectionTouchStarted && touch != UIWKSelectionTouchMoved)
            _usingGestureForSelection = NO;
    });
}

- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState
{
    _usingGestureForSelection = YES;
    _page->selectWithTwoTouches(WebCore::IntPoint(from), WebCore::IntPoint(to), static_cast<uint32_t>(toGestureType(gestureType)), static_cast<uint32_t>(toGestureRecognizerState(gestureState)), [self, gestureState](const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, CallbackBase::Error error) {
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
    _page->moveSelectionByOffset(offset, [self](CallbackBase::Error) {
        [self endSelectionChange];
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

    _autocorrectionData.autocorrectionHandler = [completionHandler copy];
    _page->requestAutocorrectionData(input, [self](const Vector<FloatRect>& rects, const String& fontName, double fontSize, uint64_t traits, CallbackBase::Error) {
        CGRect firstRect = CGRectZero;
        CGRect lastRect = CGRectZero;
        if (rects.size()) {
            firstRect = rects[0];
            lastRect = rects[rects.size() - 1];
        }
        
        _autocorrectionData.fontName = fontName;
        _autocorrectionData.fontSize = fontSize;
        _autocorrectionData.fontTraits = traits;
        _autocorrectionData.textFirstRect = firstRect;
        _autocorrectionData.textLastRect = lastRect;

        _autocorrectionData.autocorrectionHandler(rects.size() ? [WKAutocorrectionRects autocorrectionRectsWithRects:firstRect lastRect:lastRect] : nil);
        [_autocorrectionData.autocorrectionHandler release];
        _autocorrectionData.autocorrectionHandler = nil;
    });
}

- (UTF32Char)_characterBeforeCaretSelection
{
    return _page->editorState().characterBeforeSelection;
}

- (UTF32Char)_characterInRelationToCaretSelection:(int)amount
{
    switch (amount) {
    case 0:
        return _page->editorState().characterAfterSelection;
    case -1:
        return _page->editorState().characterBeforeSelection;
    case -2:
        return _page->editorState().twoCharacterBeforeSelection;
    default:
        return 0;
    }
}

- (BOOL)_selectionAtDocumentStart
{
    return !_page->editorState().characterBeforeSelection;
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

    _page->requestDictationContext([dictationHandler](const String& selectedText, const String& beforeText, const String& afterText, CallbackBase::Error) {
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
    _page->applyAutocorrection(correction, input, [self](const String& string, CallbackBase::Error error) {
        _autocorrectionData.autocorrectionHandler(!string.isNull() ? [WKAutocorrectionRects autocorrectionRectsWithRects:_autocorrectionData.textFirstRect lastRect:_autocorrectionData.textLastRect] : nil);
        [_autocorrectionData.autocorrectionHandler release];
        _autocorrectionData.autocorrectionHandler = nil;
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
        _page->requestAutocorrectionContext([self](const String& beforeText, const String& markedText, const String& selectedText, const String& afterText, uint64_t location, uint64_t length, CallbackBase::Error) {
            _autocorrectionData.autocorrectionContextHandler([WKAutocorrectionContext autocorrectionContextWithData:beforeText markedText:markedText selectedText:selectedText afterText:afterText selectedRangeInMarkedText:NSMakeRange(location, length)]);
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
    return @[[UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:0 action:@selector(_nextAccessoryTab:)],
             [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:UIKeyModifierShift action:@selector(_prevAccessoryTab:)]];
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
    _page->focusNextAssistedNode(isNext);
}

- (void)accessoryAutoFill
{
    id <_WKFormDelegate> formDelegate = [_webView _formDelegate];
    if ([formDelegate respondsToSelector:@selector(_webView:accessoryViewCustomButtonTappedInFormInputSession:)])
        [formDelegate _webView:_webView accessoryViewCustomButtonTappedInFormInputSession:_formInputSession.get()];
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
    FloatRect startRect = _page->editorState().caretRectAtStart;
    FloatRect endRect = _page->editorState().caretRectAtEnd;
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
                        selectedTextLength:_page->editorState().selectedTextLength];
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

- (void)handleKeyWebEvent:(WebIOSEvent *)theEvent
{
    _page->handleKeyboardEvent(NativeWebKeyboardEvent(theEvent));
}

- (void)_didHandleKeyEvent:(WebIOSEvent *)event
{
    if (event.type == WebEventKeyDown) {
        // FIXME: This is only for staging purposes.
        if ([[UIKeyboardImpl sharedInstance] respondsToSelector:@selector(didHandleWebKeyEvent)])
            [[UIKeyboardImpl sharedInstance] didHandleWebKeyEvent];
    }
}

- (BOOL)_interpretKeyEvent:(WebIOSEvent *)event isCharEvent:(BOOL)isCharEvent
{
    static const unsigned kWebEnterKey = 0x0003;
    static const unsigned kWebBackspaceKey = 0x0008;
    static const unsigned kWebReturnKey = 0x000D;
    static const unsigned kWebDeleteKey = 0x007F;
    static const unsigned kWebLeftArrowKey = 0x00AC;
    static const unsigned kWebUpArrowKey = 0x00AD;
    static const unsigned kWebRightArrowKey = 0x00AE;
    static const unsigned kWebDownArrowKey = 0x00AF;
    static const unsigned kWebDeleteForwardKey = 0xF728;

    if (!_page->editorState().isContentEditable && event.isTabKey)
        return NO;

    BOOL shift = event.modifierFlags & WebEventFlagMaskShift;

    switch (event.characterSet) {
    case WebEventCharacterSetSymbol: {
        String command;
        NSString *characters = [event charactersIgnoringModifiers];
        if ([characters length] == 0)
            break;
        switch ([characters characterAtIndex:0]) {
        case kWebLeftArrowKey:
            command = shift ? ASCIILiteral("moveLeftAndModifySelection") :  ASCIILiteral("moveLeft");
            break;

        case kWebUpArrowKey:
            command = shift ? ASCIILiteral("moveUpAndModifySelection") :  ASCIILiteral("moveUp");
            break;

        case kWebRightArrowKey:
            command = shift ? ASCIILiteral("moveRightAndModifySelection") :  ASCIILiteral("moveRight");
            break;

        case kWebDownArrowKey:
            command = shift ? ASCIILiteral("moveDownAndModifySelection") :  ASCIILiteral("moveDown");
            break;
        }
        if (!command.isEmpty()) {
            _page->executeEditCommand(command);
            return YES;
        }
        break;
    }
    case WebEventCharacterSetASCII:
    case WebEventCharacterSetUnicode: {
        NSString *characters = [event characters];
        if ([characters length] == 0)
            break;
        switch ([characters characterAtIndex:0]) {
        case kWebBackspaceKey:
        case kWebDeleteKey:
            [[UIKeyboardImpl sharedInstance] deleteFromInput];
            return YES;

        case kWebEnterKey:
        case kWebReturnKey:
            if (isCharEvent) {
                // Map \r from HW keyboard to \n to match the behavior of the soft keyboard.
                [[UIKeyboardImpl sharedInstance] addInputString:@"\n" withFlags:0];
                return YES;
            }
            return NO;

        case kWebDeleteForwardKey:
            _page->executeEditCommand(ASCIILiteral("deleteForward"));
            return YES;

        default: {
            if (isCharEvent) {
                [[UIKeyboardImpl sharedInstance] addInputString:event.characters withFlags:event.keyboardFlags];
                return YES;
            }
            return NO;
        }
    }
        break;
    }
    default:
        return NO;
    }

    return NO;
}

- (void)executeEditCommandWithCallback:(NSString *)commandName
{
    [self beginSelectionChange];
    _page->executeEditCommand(commandName, [self](CallbackBase::Error) {
        [self endSelectionChange];
    });
}

- (UITextInputArrowKeyHistory *)_moveUp:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveUp"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveDown:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveDown"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveLeft:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveLeft"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveRight:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveRight"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfWord:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveWordBackward"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfParagraph:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveToStartOfParagraph"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfLine:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveToStartOfLine"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToStartOfDocument:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveToBeginningOfDocument"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfWord:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveWordForward"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfParagraph:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveToEndOfParagraph"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfLine:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveToEndOfLine"];
    return nil;
}

- (UITextInputArrowKeyHistory *)_moveToEndOfDocument:(BOOL) extending withHistory:(UITextInputArrowKeyHistory *)history
{
    [self executeEditCommandWithCallback:@"moveToEndOfDocument"];
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

- (void)insertDictationResult:(NSArray *)dictationResult withCorrectionIdentifier:(id)correctionIdentifier
{
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
    return _page->editorState().hasContent;
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
}

- (void)_stopAssistingKeyboard
{
    [self useSelectionAssistantWithMode:toUIWebSelectionMode([[_webView configuration] selectionGranularity])];
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
    return _formAccessoryView.get();
}

- (void)_startAssistingNode:(const AssistedNodeInformation&)information userIsInteracting:(BOOL)userIsInteracting blurPreviousNode:(BOOL)blurPreviousNode userObject:(NSObject <NSSecureCoding> *)userObject
{
    if (!userIsInteracting && !_textSelectionAssistant)
        return;

    if (blurPreviousNode)
        [self _stopAssistingNode];

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

    [self reloadInputViews];
    [self _displayFormNodeInputView];

    // _inputPeripheral has been initialized in inputView called by reloadInputViews.
    [_inputPeripheral beginEditing];

    id <_WKFormDelegate> formDelegate = [_webView _formDelegate];
    if ([formDelegate respondsToSelector:@selector(_webView:didStartInputSession:)]) {
        _formInputSession = adoptNS([[WKFormInputSession alloc] initWithContentView:self userObject:userObject]);
        [formDelegate _webView:_webView didStartInputSession:_formInputSession.get()];
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
    if (!_selectionNeedsUpdate)
        return;

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

@end

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
