/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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
#import "WKInteractionView.h"

#import "InteractionInformationAtPosition.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebTouchEvent.h"
#import "WKActionSheetAssistant.h"
#import "WKBase.h"
#import "WKGestureTypes.h"
#import "WebEvent.h"
#import "WebIOSEventFactory.h"
#import "WebPageMessages.h"
#import "WebProcessProxy.h"
#import <DataDetectorsUI/DDDetectionController.h>
#import <UIKit/UIFont_Private.h>
#import <UIKit/UIGestureRecognizer_Private.h>
#import <UIKit/UIKeyboardImpl.h>
#import <UIKit/UILongPressGestureRecognizer_Private.h>
#import <UIKit/UITapGestureRecognizer_Private.h>
#import <UIKit/UITextInteractionAssistant_Private.h>
#import <UIKit/UIWebDocumentView.h>
#import <UIKit/UIWebScrollView.h>
#import <UIKit/_UIHighlightView.h>
#import <UIKit/_UIWebHighlightLongPressGestureRecognizer.h>
#import <WebCore/Color.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebEvent.h>
#import <WebKit/WebSelectionRect.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

SOFT_LINK_PRIVATE_FRAMEWORK(DataDetectorsUI)
SOFT_LINK_CLASS(DataDetectorsUI, DDDetectionController)

using namespace WebCore;
using namespace WebKit;

static const float highlightDelay = 0.12;
static const float tapAndHoldDelay  = 0.75;

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

@interface WKAutocorrectionRects : UIWKAutocorrectionRects {
    CGRect _firstRect;
    CGRect _lastRect;
}
+ (WKAutocorrectionRects *)autocorrectionRectsWithRects:(CGRect)firstRect lastRect:(CGRect)lastRect;

@end

@interface WKAutocorrectionContext : UIWKAutocorrectionContext {
    NSString *_contextBeforeSelection;
    NSString *_selectedText;
    NSString *_markedText;
    NSString *_contextAfterSelection;
    NSRange _rangeInMarkedText;
}

+ (WKAutocorrectionContext *)autocorrectionContextWithData:(NSString *)beforeText markedText:(NSString *)markedText selectedText:(NSString *)selectedText afterText:(NSString *)afterText selectedRangeInMarkedText:(NSRange)range;
@end

@interface UITextInteractionAssistant (UITextInteractionAssistant_Internal)
// FIXME: this needs to be moved from the internal header to the private.
- (id)initWithView:(UIResponder <UITextInput> *)view;
- (void)selectWord;
@end

typedef void (^UIWKAutocorrectionCompletionHandler)(UIWKAutocorrectionRects *rectsForInput);
typedef void (^UIWKAutocorrectionContextHandler)(UIWKAutocorrectionContext *autocorrectionContext);

struct WKAutoCorrectionData{
    String fontName;
    CGFloat fontSize;
    uint64_t fontTraits;
    CGRect textFirstRect;
    CGRect textLastRect;
    UIWKAutocorrectionCompletionHandler autocorrectionHandler;
    UIWKAutocorrectionContextHandler autocorrectionContextHandler;
};

@interface WKInteractionView (Private)
@property (readonly, nonatomic) WKAutoCorrectionData *autocorrectionData;
@end

@implementation WKInteractionView {
    RetainPtr<UIWebTouchEventsGestureRecognizer> _touchEventGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _singleTapGestureRecognizer;
    RetainPtr<_UIWebHighlightLongPressGestureRecognizer> _highlightLongPressGestureRecognizer;
    RetainPtr<UILongPressGestureRecognizer> _longPressGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _doubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _twoFingerDoubleTapGestureRecognizer;
    RetainPtr<UIPanGestureRecognizer> _twoFingerPanGestureRecognizer;

    RetainPtr<UIWKTextInteractionAssistant> _textSelectionAssistant;
    RetainPtr<UIWKSelectionAssistant> _webSelectionAssistant;

    UITextInputTraits *_traits;
    BOOL _isEditable;
    UIWebFormAccessory *_accessory;
    id <UITextInputDelegate> _inputDelegate;
    BOOL _showingTextStyleOptions;

    __weak UIWebScrollView *_scrollView;
    RefPtr<WebPageProxy> _page;

    RetainPtr<_UIHighlightView> _highlightView;
    uint64_t _latestTapHighlightID;
    BOOL _isTapHighlightIDValid;
    WKAutoCorrectionData _autocorrectionData;
    RetainPtr<NSString> _markedText;
    InteractionInformationAtPosition _positionInformation;
    BOOL _hasValidPositionInformation;
    RetainPtr<WKActionSheetAssistant> _actionSheetAssistant;
}

@synthesize inputDelegate = _inputDelegate;

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _touchEventGestureRecognizer = adoptNS([[UIWebTouchEventsGestureRecognizer alloc] initWithTarget:self action:@selector(_webTouchEventsRecognized:) touchDelegate:self]);
        [_touchEventGestureRecognizer setDelegate:self];
        [self addGestureRecognizer:_touchEventGestureRecognizer.get()];

        _singleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_singleTapRecognized:)]);
        [_singleTapGestureRecognizer setDelegate:self];
        [self addGestureRecognizer:_singleTapGestureRecognizer.get()];

        _doubleTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_doubleTapRecognized:)]);
        [_doubleTapGestureRecognizer setNumberOfTapsRequired:2];
        [_doubleTapGestureRecognizer setDelegate:self];
        [self addGestureRecognizer:_doubleTapGestureRecognizer.get()];
        [_singleTapGestureRecognizer requireOtherGestureToFail:_doubleTapGestureRecognizer.get()];

        _highlightLongPressGestureRecognizer = adoptNS([[_UIWebHighlightLongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_highlightLongPressRecognized:)]);
        [_highlightLongPressGestureRecognizer setDelay:highlightDelay];
        [_highlightLongPressGestureRecognizer setDelegate:self];
        [self addGestureRecognizer:_highlightLongPressGestureRecognizer.get()];

        _longPressGestureRecognizer = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_longPressRecognized:)]);
        [_longPressGestureRecognizer setDelay:tapAndHoldDelay];
        [_longPressGestureRecognizer setDelegate:self];
        [self addGestureRecognizer:_longPressGestureRecognizer.get()];

        _twoFingerPanGestureRecognizer = adoptNS([[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(_twoFingerPanRecognized:)]);
        [_twoFingerPanGestureRecognizer setMinimumNumberOfTouches:2];
        [_twoFingerPanGestureRecognizer setMaximumNumberOfTouches:2];
        [_twoFingerPanGestureRecognizer setDelegate:self];
        [self addGestureRecognizer:_twoFingerPanGestureRecognizer.get()];

        [self setUserInteractionEnabled:YES];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_resetShowingTextStyle:) name:UIMenuControllerDidHideMenuNotification object:nil];
        _showingTextStyleOptions = NO;
    }

    // FIXME: This should be called when we get notified that loading has completed.
    [self useSelectionAssistantWithMode:UIWebSelectionModeWeb];

    _actionSheetAssistant = adoptNS([[WKActionSheetAssistant alloc] initWithView:self]);
    return self;
}

- (void)dealloc
{
    _webSelectionAssistant = nil;
    _textSelectionAssistant = nil;
    _actionSheetAssistant = nil;
    [_touchEventGestureRecognizer setDelegate:nil];
    [_singleTapGestureRecognizer setDelegate:nil];
    [_doubleTapGestureRecognizer setDelegate:nil];
    [_highlightLongPressGestureRecognizer setDelegate:nil];
    [_longPressGestureRecognizer setDelegate:nil];
    [_twoFingerPanGestureRecognizer setDelegate:nil];

    [_accessory release];
    [super dealloc];
}

- (void)setScrollView:(UIWebScrollView *)scrollView
{
    _scrollView = scrollView;
}

- (void)setPage:(PassRefPtr<WebKit::WebPageProxy>)page
{
    _page = page;
    [_actionSheetAssistant setPage:_page];
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
    _page->blurAssistedNode();
    [self _cancelInteraction];
    [_webSelectionAssistant resignedFirstResponder];

    return [super resignFirstResponder];
}

- (void)_webTouchEventsRecognized:(UIWebTouchEventsGestureRecognizer *)gestureRecognizer
{
    NativeWebTouchEvent nativeWebTouchEvent(gestureRecognizer);

    // FIXME: this kind of event delivery is supposed to be only used for testing. We must switch to asynchronous handling.
    _page->setShouldSendEventsSynchronously(true);
    _page->handleTouchEvent(nativeWebTouchEvent);
    _page->setShouldSendEventsSynchronously(false);
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

- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius
{
    if (!_isTapHighlightIDValid || _latestTapHighlightID != requestID)
        return;

    const CGFloat UIWebViewMinimumHighlightRadius = 2.0;
    if (!_highlightView) {
        _highlightView = adoptNS([[_UIHighlightView alloc] initWithFrame:CGRectZero]);
        [_highlightView setOpaque:NO];
        [_highlightView setCornerRadius:UIWebViewMinimumHighlightRadius];
    }
    [self addSubview:_highlightView.get()];

    RetainPtr<UIColor> highlightUIKitColor = adoptNS([[UIColor alloc] initWithRed:(color.red() / 255.0) green:(color.green() / 255.0) blue:(color.blue() / 255.0) alpha:(color.alpha() / 255.0)]);
    [_highlightView setColor:highlightUIKitColor.get()];

    bool allHighlightRectsAreRectilinear = true;
    const size_t quadCount = highlightedQuads.size();
    RetainPtr<NSMutableArray> rects = adoptNS([[NSMutableArray alloc] initWithCapacity:static_cast<const NSUInteger>(quadCount)]);
    for (size_t i = 0; i < quadCount; ++i) {
        const FloatQuad& quad = highlightedQuads[i];
        if (quad.isRectilinear()) {
            CGRect rect = CGRectInset(quad.boundingBox(), -UIWebViewMinimumHighlightRadius, -UIWebViewMinimumHighlightRadius);
            [rects addObject:[NSValue valueWithCGRect:rect]];
        } else {
            allHighlightRectsAreRectilinear = false;
            rects.clear();
            break;
        }
    }

    // FIXME: WebKit1 uses the visibleRect. Using the whole frame from the page seems overkill.
    CGRect boundaryRect = [self frame];
    if (allHighlightRectsAreRectilinear)
        [_highlightView setFrames:rects.get() boundaryRect:boundaryRect];
    else {
        RetainPtr<NSMutableArray> quads = adoptNS([[NSMutableArray alloc] initWithCapacity:static_cast<const NSUInteger>(quadCount)]);
        for (size_t i = 0; i < quadCount; ++i) {
            const FloatQuad& quad = highlightedQuads[i];
            FloatQuad extendedQuad = inflateQuad(quad, UIWebViewMinimumHighlightRadius);
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p1()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p2()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p3()]];
            [quads addObject:[NSValue valueWithCGPoint:extendedQuad.p4()]];
        }
        [_highlightView setQuads:quads.get() boundaryRect:boundaryRect];
    }

    RetainPtr<NSMutableArray> borderRadii = adoptNS([[NSMutableArray alloc] initWithCapacity:4]);
    [borderRadii addObject:[NSValue valueWithCGSize:CGSizeMake(topLeftRadius.width() + UIWebViewMinimumHighlightRadius, topLeftRadius.height() + UIWebViewMinimumHighlightRadius)]];
    [borderRadii addObject:[NSValue valueWithCGSize:CGSizeMake(topRightRadius.width() + UIWebViewMinimumHighlightRadius, topRightRadius.height() + UIWebViewMinimumHighlightRadius)]];
    [borderRadii addObject:[NSValue valueWithCGSize:CGSizeMake(bottomLeftRadius.width() + UIWebViewMinimumHighlightRadius, bottomLeftRadius.height() + UIWebViewMinimumHighlightRadius)]];
    [borderRadii addObject:[NSValue valueWithCGSize:CGSizeMake(bottomRightRadius.width() + UIWebViewMinimumHighlightRadius, bottomRightRadius.height() + UIWebViewMinimumHighlightRadius)]];
    [_highlightView setCornerRadii:borderRadii.get()];
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

- (BOOL)_requiresKeyboardResetOnReload
{
    return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)preventingGestureRecognizer canPreventGestureRecognizer:(UIGestureRecognizer *)preventedGestureRecognizer
{
    // A long-press gesture can not be recognized while panning, but a pan can be recognized
    // during a long-press gesture.
    BOOL shouldNotPreventPanGesture = preventingGestureRecognizer == _highlightLongPressGestureRecognizer || preventingGestureRecognizer == _longPressGestureRecognizer;
    return !(shouldNotPreventPanGesture && [preventedGestureRecognizer isKindOfClass:NSClassFromString(@"UIScrollViewPanGestureRecognizer")]);
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

    if (gestureRecognizer == _twoFingerPanGestureRecognizer) {
        notImplemented();
    }

    return YES;
}

- (void)_cancelInteraction
{
    _isTapHighlightIDValid = NO;
    [_highlightView removeFromSuperview];
}

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point
{
    [self ensurePositionInformationIsUpToDate:point];
    // FIXME: This check needs to be extended to include other elements.
    return _positionInformation.clickableElementName != "IMG" && _positionInformation.clickableElementName != "A" && !_positionInformation.selectionRects.isEmpty();
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

    switch ([gestureRecognizer state]) {
    case UIGestureRecognizerStateBegan:
        _page->tapHighlightAtPosition([gestureRecognizer startPoint], _latestTapHighlightID);
        _isTapHighlightIDValid = YES;
        break;
    case UIGestureRecognizerStateEnded:
        if (!_positionInformation.clickableElementName.isEmpty())
            [self _attemptClickAtLocation:[gestureRecognizer startPoint]];
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

    if ([gestureRecognizer state] == UIGestureRecognizerStateBegan) {
        SEL action = [self _actionForLongPress];
        if (action)
            [self performSelector:action];
    }
}

- (void)_singleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    ASSERT(gestureRecognizer == _singleTapGestureRecognizer);

    if (_webSelectionAssistant && ![_webSelectionAssistant shouldHandleSingleTapAtPoint:gestureRecognizer.location])
        return;

    [_webSelectionAssistant clearSelection];

    [self _attemptClickAtLocation:[gestureRecognizer location]];
}

- (void)_doubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    // FIXME: Add implementation.
}

- (void)_twoFingerDoubleTapRecognized:(UITapGestureRecognizer *)gestureRecognizer
{
    // FIXME: Add implementation.
}

- (void)_twoFingerPanRecognized:(UIPanGestureRecognizer *)gestureRecognizer
{
    // FIXME: Add implementation.
}

- (void)_attemptClickAtLocation:(CGPoint)location
{
    // FIXME: Ideally, we should always provide some visual feedback on click. If a short tap did not trigger the
    // tap highlight, we should show one based on a timer if we commit the synthetic mouse events.
    [self _cancelInteraction];

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

- (void)_positionInformationDidChange:(const InteractionInformationAtPosition&)info
{
    _positionInformation = info;
    _hasValidPositionInformation = YES;
    if (_actionSheetAssistant)
        [_actionSheetAssistant updateSheetPosition];
}

- (UIView *)inputAccessoryView
{
    if (!_isEditable)
        return nil;
    
    if (!_accessory) {
        _accessory = [[UIWebFormAccessory alloc] init];
        _accessory.delegate = self;
    }
    
    return _accessory;
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
    // FIXME: To be implemented.
}

- (void)_promptForReplace:(id)sender
{
    // FIXME: To be implemented.
}

- (void)replace:(id)sender
{
    // FIXME: To be implemented.
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender
{
    BOOL hasWebSelection = _webSelectionAssistant && !CGRectIsEmpty(_webSelectionAssistant.get().selectionFrame);

    if (action == @selector(_showTextStyleOptions:))
        return _page->editorState().isContentRichlyEditable && _page->editorState().selectionIsRange && !_showingTextStyleOptions;
    if (_showingTextStyleOptions)
        return (action == @selector(toggleBoldface:) || action == @selector(toggleItalics:) || action == @selector(toggleUnderline:));
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
        // FIXME: need to implement, returning NO always for now.
        return NO;
    }

    if (action == @selector(_promptForReplace:)) {
        // FIXME: need to implement
        return NO;
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

- (void)_performAction:(WKSheetActions)action
{
    _page->performActionOnElement((uint32_t)action);
}

- (void)copy:(id)sender
{
    _page->executeEditCommand(ASCIILiteral("copy"));
}

- (void)cut:(id)sender
{
    [self.inputDelegate textWillChange:self];

    _page->executeEditCommand(ASCIILiteral("cut"));

    [self.inputDelegate textDidChange:self];
}

- (void)paste:(id)sender
{
    [self.inputDelegate textWillChange:self];
    
    _page->executeEditCommand(ASCIILiteral("paste"));
    
    [self.inputDelegate textDidChange:self];
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

    _page->executeEditCommand(ASCIILiteral("toggleBold"));
}

- (void)toggleItalics:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    _page->executeEditCommand(ASCIILiteral("toggleItalic"));
}

- (void)toggleUnderline:(id)sender
{
    if (!_page->editorState().isContentRichlyEditable)
        return;

    _page->executeEditCommand(ASCIILiteral("toggleUnderline"));
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

static void selectedString(WKStringRef string, WKErrorRef error, void* context)
{
    if (error)
        return;
    if (!string)
        return;

    NSString *convertedString = toImpl(string)->string();
    WKInteractionView* view = static_cast<WKInteractionView*>(context);
    ASSERT(view);
    [view _showDictionary:convertedString];
}

- (void)_define:(id)sender
{
    _page->getSelectionOrContentsAsString(StringCallback::create(self, selectedString));
}

// UIWKInteractionViewProtocol

static inline WKGestureType toWKGestureType(UIWKGestureType gestureType)
{
    switch (gestureType) {
    case UIWKGestureLoupe:
        return WKGestureLoupe;
    case UIWKGestureOneFingerTap:
        return WKGestureOneFingerTap;
    case UIWKGestureTapAndAHalf:
        return WKGestureTapAndAHalf;
    case UIWKGestureDoubleTap:
        return WKGestureDoubleTap;
    case UIWKGestureTapAndHalf:
        return WKGestureTapAndHalf;
    case UIWKGestureDoubleTapInUneditable:
        return WKGestureDoubleTapInUneditable;
    case UIWKGestureOneFingerTapInUneditable:
        return WKGestureOneFingerTapInUneditable;
    case UIWKGestureOneFingerTapSelectsAll:
        return WKGestureOneFingerTapSelectsAll;
    case UIWKGestureOneFingerDoubleTap:
        return WKGestureOneFingerDoubleTap;
    case UIWKGestureOneFingerTripleTap:
        return WKGestureOneFingerTripleTap;
    case UIWKGestureTwoFingerSingleTap:
        return WKGestureTwoFingerSingleTap;
    case UIWKGestureTwoFingerRangedSelectGesture:
        return WKGestureTwoFingerRangedSelectGesture;
    case UIWKGestureTapOnLinkWithGesture:
        return WKGestureTapOnLinkWithGesture;
    case UIWKGestureMakeWebSelection:
        return WKGestureMakeWebSelection;
    }
    ASSERT_NOT_REACHED();
    return WKGestureLoupe;
}

static inline UIWKGestureType toUIWKGestureType(WKGestureType gestureType)
{
    switch (gestureType) {
    case WKGestureLoupe:
        return UIWKGestureLoupe;
    case WKGestureOneFingerTap:
        return UIWKGestureOneFingerTap;
    case WKGestureTapAndAHalf:
        return UIWKGestureTapAndAHalf;
    case WKGestureDoubleTap:
        return UIWKGestureDoubleTap;
    case WKGestureTapAndHalf:
        return UIWKGestureTapAndHalf;
    case WKGestureDoubleTapInUneditable:
        return UIWKGestureDoubleTapInUneditable;
    case WKGestureOneFingerTapInUneditable:
        return UIWKGestureOneFingerTapInUneditable;
    case WKGestureOneFingerTapSelectsAll:
        return UIWKGestureOneFingerTapSelectsAll;
    case WKGestureOneFingerDoubleTap:
        return UIWKGestureOneFingerDoubleTap;
    case WKGestureOneFingerTripleTap:
        return UIWKGestureOneFingerTripleTap;
    case WKGestureTwoFingerSingleTap:
        return UIWKGestureTwoFingerSingleTap;
    case WKGestureTwoFingerRangedSelectGesture:
        return UIWKGestureTwoFingerRangedSelectGesture;
    case WKGestureTapOnLinkWithGesture:
        return UIWKGestureTapOnLinkWithGesture;
    case WKGestureMakeWebSelection:
        return UIWKGestureMakeWebSelection;
    }
}

static inline WKSelectionTouch toWKSelectionTouch(UIWKSelectionTouch touch)
{
    switch (touch) {
    case UIWKSelectionTouchStarted:
        return WKSelectionTouchStarted;
    case UIWKSelectionTouchMoved:
        return WKSelectionTouchMoved;
    case UIWKSelectionTouchEnded:
        return WKSelectionTouchEnded;
    case UIWKSelectionTouchEndedMovingForward:
        return WKSelectionTouchEndedMovingForward;
    case UIWKSelectionTouchEndedMovingBackward:
        return WKSelectionTouchEndedMovingBackward;
    case UIWKSelectionTouchEndedNotMoving:
        return WKSelectionTouchEndedNotMoving;
    }
    ASSERT_NOT_REACHED();
    return WKSelectionTouchEnded;
}

static inline UIWKSelectionTouch toUIWKSelectionTouch(WKSelectionTouch touch)
{
    switch (touch) {
    case WKSelectionTouchStarted:
        return UIWKSelectionTouchStarted;
    case WKSelectionTouchMoved:
        return UIWKSelectionTouchMoved;
    case WKSelectionTouchEnded:
        return UIWKSelectionTouchEnded;
    case WKSelectionTouchEndedMovingForward:
        return UIWKSelectionTouchEndedMovingForward;
    case WKSelectionTouchEndedMovingBackward:
        return UIWKSelectionTouchEndedMovingBackward;
    case WKSelectionTouchEndedNotMoving:
        return UIWKSelectionTouchEndedNotMoving;
    }
}

static inline WKGestureRecognizerState toWKGestureRecognizerState(UIGestureRecognizerState state)
{
    switch (state) {
    case UIGestureRecognizerStatePossible:
        return WKGestureRecognizerStatePossible;
    case UIGestureRecognizerStateBegan:
        return WKGestureRecognizerStateBegan;
    case UIGestureRecognizerStateChanged:
        return WKGestureRecognizerStateChanged;
    case UIGestureRecognizerStateCancelled:
        return WKGestureRecognizerStateCancelled;
    case UIGestureRecognizerStateEnded:
        return WKGestureRecognizerStateEnded;
    case UIGestureRecognizerStateFailed:
        return WKGestureRecognizerStateFailed;
    }
}

static inline UIGestureRecognizerState toUIGestureRecognizerState(WKGestureRecognizerState state)
{
    switch (state) {
    case WKGestureRecognizerStatePossible:
        return UIGestureRecognizerStatePossible;
    case WKGestureRecognizerStateBegan:
        return UIGestureRecognizerStateBegan;
    case WKGestureRecognizerStateChanged:
        return UIGestureRecognizerStateChanged;
    case WKGestureRecognizerStateCancelled:
        return UIGestureRecognizerStateCancelled;
    case WKGestureRecognizerStateEnded:
        return UIGestureRecognizerStateEnded;
    case WKGestureRecognizerStateFailed:
        return UIGestureRecognizerStateFailed;
    }
}

static void selectionChangedWithGesture(const WebCore::IntPoint& point, uint32_t gestureType, uint32_t gestureState, uint32_t flags, WKErrorRef error, void* context)
{
    if (error) {
        ASSERT_NOT_REACHED();
        return;
    }
    WKInteractionView *view = static_cast<WKInteractionView*>(context);
    ASSERT(view);
    // FIXME: need to pass flags to selectionChangedWithGestureAt.
    if ([view webSelectionAssistant])
        [(UIWKSelectionAssistant *)[view webSelectionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType((WKGestureType)gestureType) withState:toUIGestureRecognizerState(static_cast<WKGestureRecognizerState>(gestureState))];
    else
        [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithGestureAt:(CGPoint)point withGesture:toUIWKGestureType((WKGestureType)gestureType) withState:toUIGestureRecognizerState(static_cast<WKGestureRecognizerState>(gestureState))];
}

static void selectionChangedWithTouch(const WebCore::IntPoint& point, uint32_t touch, WKErrorRef error, void* context)
{
    if (error) {
        ASSERT_NOT_REACHED();
        return;
    }
    WKInteractionView *view = static_cast<WKInteractionView*>(context);
    ASSERT(view);
    if ([view webSelectionAssistant])
        [(UIWKSelectionAssistant *)[view webSelectionAssistant] selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:toUIWKSelectionTouch((WKSelectionTouch)touch)];
    else
        [(UIWKTextInteractionAssistant *)[view interactionAssistant] selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:toUIWKSelectionTouch((WKSelectionTouch)touch)];
}

- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state
{
    _page->selectWithGesture(WebCore::IntPoint(point), CharacterGranularity, toWKGestureType(gestureType), toWKGestureRecognizerState(state), GestureCallback::create(self, selectionChangedWithGesture));
}

- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart
{
    _page->updateSelectionWithTouches(WebCore::IntPoint(point), toWKSelectionTouch(touch), baseIsStart, TouchesCallback::create(self, selectionChangedWithTouch));
}

- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState
{
    _page->selectWithTwoTouches(WebCore::IntPoint(from), WebCore::IntPoint(to), toWKGestureType(gestureType), toWKGestureRecognizerState(gestureState), GestureCallback::create(self, selectionChangedWithGesture));
}

- (WKAutoCorrectionData *)autocorrectionData
{
    return &_autocorrectionData;
}

static void autocorrectionData(const Vector<FloatRect>& rects, const String& fontName, double fontSize, uint64_t traits, WKErrorRef error, void* context)
{
    WKInteractionView* view = static_cast<WKInteractionView*>(context);
    ASSERT(view);
    CGRect firstRect = CGRectZero;
    CGRect lastRect = CGRectZero;
    if (rects.size()) {
        firstRect = rects[0];
        lastRect = rects[rects.size() - 1];
    }

    WKAutoCorrectionData *autocorrectionData = view.autocorrectionData;
    autocorrectionData->fontName = fontName;
    autocorrectionData->fontSize = fontSize;
    autocorrectionData->fontTraits = traits;
    autocorrectionData->textFirstRect = firstRect;
    autocorrectionData->textLastRect = lastRect;

    autocorrectionData->autocorrectionHandler(rects.size() ? [WKAutocorrectionRects autocorrectionRectsWithRects:firstRect lastRect:lastRect] : nil);
    [autocorrectionData->autocorrectionHandler release];
    autocorrectionData->autocorrectionHandler = nil;
}

// The completion handler can pass nil if input does not match the actual text preceding the insertion point.
- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler
{
    if (!input || ![input length]) {
        completionHandler(nil);
        return;
    }
    _autocorrectionData.autocorrectionHandler = [completionHandler copy];
    _page->requestAutocorrectionData(input, AutocorrectionDataCallback::create(self, autocorrectionData));
}

- (CGRect)textFirstRect
{
    return (_page->editorState().hasComposition) ? _page->editorState().firstMarkedRect : _autocorrectionData.textFirstRect;
}

- (CGRect)textLastRect
{
    return (_page->editorState().hasComposition) ? _page->editorState().lastMarkedRect : _autocorrectionData.textLastRect;
}

static void autocorrectionResult(WKStringRef correction, WKErrorRef error, void* context)
{
    WKInteractionView* view = static_cast<WKInteractionView*>(context);
    ASSERT(view);
    WKAutoCorrectionData *autocorrectionData = view.autocorrectionData;

    autocorrectionData->autocorrectionHandler(correction ? [WKAutocorrectionRects autocorrectionRectsWithRects:autocorrectionData->textFirstRect lastRect:autocorrectionData->textLastRect] : nil);
    [autocorrectionData->autocorrectionHandler release];
    autocorrectionData->autocorrectionHandler = nil;
}

// The completion handler should pass the rect of the correction text after replacing the input text, or nil if the replacement could not be performed.
- (void)applyAutocorrection:(NSString *)correction toString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForCorrection))completionHandler
{
    _autocorrectionData.autocorrectionHandler = [completionHandler copy];
    _page->applyAutocorrection(correction, input, StringCallback::create(self, autocorrectionResult));
}

static void autocorrectionContext(const String& beforeText, const String& markedText, const String& selectedText, const String& afterText, uint64_t location, uint64_t length, WKErrorRef error, void* context)
{
    WKInteractionView* view = static_cast<WKInteractionView*>(context);
    ASSERT(view);
    WKAutoCorrectionData *autocorrectionData = view.autocorrectionData;
    autocorrectionData->autocorrectionContextHandler([WKAutocorrectionContext autocorrectionContextWithData:beforeText markedText:markedText selectedText:selectedText afterText:afterText selectedRangeInMarkedText:NSMakeRange(location, length)]);
}

- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler
{
    // FIXME: Remove the synchronous call as soon as Keyboard removes locking of the main thread.
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
        _page->requestAutocorrectionContext(AutocorrectionContextCallback::create(self, autocorrectionContext));
    }
}

// UIWebFormAccessoryDelegate
- (void)accessoryDone
{
    [self resignFirstResponder];
}

- (void)accessoryTab:(BOOL)isNext
{
}

- (void)accessoryAutoFill
{
}

- (void)accessoryClear
{
}

- (void)_updateAccessory
{
    // FIXME: We need to initialize with values from the WebProcess.
    _accessory.nextEnabled = YES;
    _accessory.previousEnabled = YES;
    
    [_accessory setClearVisible:NO];

    // FIXME: hide or show the AutoFill button as needed.
}

// Keyboard interaction
// UITextInput protocol implementation

- (NSString *)textInRange:(UITextRange *)range
{
    return nil;
}

- (void)replaceRange:(UITextRange *)range withText:(NSString *)text
{
}

- (UITextRange *)selectedTextRange
{
    return [WKTextRange textRangeWithState:_page->editorState().selectionIsNone
                                   isRange:_page->editorState().selectionIsRange
                                isEditable:_page->editorState().isContentEditable
                                 startRect:_page->editorState().caretRectAtStart
                                   endRect:_page->editorState().caretRectAtEnd
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
    _page->setComposition(markedText, Vector<WebCore::CompositionUnderline>(), selectedRange.location, selectedRange.length, 0, 0);
}

- (void)unmarkText
{
    _markedText = nil;
    _page->confirmComposition();
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
    _page->insertText(aStringValue, NSNotFound, 0);
}

- (BOOL)hasText
{
    return YES;
}

// end of UITextInput protocol implementation

// UITextInputPrivate protocol
// Direct access to the (private) UITextInputTraits object.
- (UITextInputTraits *)textInputTraits
{
    if (!_traits)
        _traits = [[UITextInputTraits alloc] init];
    return _traits;
}

- (UITextInteractionAssistant *)interactionAssistant
{
    // This method should only be called when we're in UIWebSelectionModeTextOnly however it is
    // possible that it can be called while we are transitioning between modes.
    // assert(!_webSelectionAssistant);

    if (!_textSelectionAssistant)
        _textSelectionAssistant = [[UIWKTextInteractionAssistant alloc] initWithView:self];

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
    return YES;
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
    [self useSelectionAssistantWithMode:UIWebSelectionModeWeb];
}

- (void)_startAssistingNode
{
    _isEditable = YES;
    if (![self isFirstResponder])
        [self becomeFirstResponder];

    [self _startAssistingKeyboard];
    [self reloadInputViews];
    [self _updateAccessory];
}

- (void)_stopAssistingNode
{
    _isEditable = NO;

    [self _stopAssistingKeyboard];
    [self reloadInputViews];
    [self _updateAccessory];
}

- (void)_selectionChanged
{
    // FIXME: We need to figure out what to do if the selection is changed by Javascript.
    if (_textSelectionAssistant) {
        _markedText = (_page->editorState().hasComposition) ? _page->editorState().markedText : String();
        if (!_showingTextStyleOptions)
            [_textSelectionAssistant selectionChanged];
    } else
        [_webSelectionAssistant selectionChanged];
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
