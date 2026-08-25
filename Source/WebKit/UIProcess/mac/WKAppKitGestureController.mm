/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "WKAppKitGestureController.h"

#if HAVE(APPKIT_GESTURES_SUPPORT)

#import "AppKitSPI.h"
#import "IdentifierTypes.h"
#import "ImageAnalysisUtilities.h"
#import "InteractionInformationAtPosition.h"
#import "InteractionInformationRequest.h"
#import "NativeWebWheelEvent.h"
#import "PositionInformationManager.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "ScrollingAccelerationCurve.h"
#import "TextRecognitionUpdateResult.h"
#import "ViewGestureController.h"
#import "WKDeferringGestureRecognizer.h"
#import "WKWebView.h"
#import "WebEventModifier.h"
#import "WebEventType.h"
#import "WebKit-Swift.h"
#import "WebMouseEvent.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebViewImpl.h"
#import "WebWheelEvent.h"
#import <Carbon/Carbon.h>
#import <WebCore/Color.h>
#import <WebCore/ElementContext.h>
#import <WebCore/FloatPoint.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FloatSize.h>
#import <WebCore/IntPoint.h>
#import <WebCore/IntSize.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/PointerID.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/TextRecognitionResult.h>
#import <source_location>
#import <wtf/BlockPtr.h>
#import <wtf/CheckedPtr.h>
#import <wtf/MainThread.h>
#import <wtf/Markable.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RefCounted.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/UUID.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>

#define WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(pageID, fmt, ...) RELEASE_LOG(ViewGestures, "[pageProxyID=%llu] %s: " fmt, pageID, std::source_location::current().function_name(), ##__VA_ARGS__)
#define WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(pageID, fmt, ...) RELEASE_LOG_DEBUG(ViewGestures, "[pageProxyID=%llu] %s: " fmt, pageID, std::source_location::current().function_name(), ##__VA_ARGS__)
#define WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_ERROR(pageID, fmt, ...) RELEASE_LOG_ERROR(ViewGestures, "[pageProxyID=%llu] %s: " fmt, pageID, std::source_location::current().function_name(), ##__VA_ARGS__)

static WebCore::FloatSize translationInView(NSPanGestureRecognizer *gesture, WKWebView *view)
{
    auto translation = WebCore::toFloatSize(WebCore::FloatPoint { [gesture translationInView:view] });
    [gesture setTranslation:NSZeroPoint inView:view];
    return translation;
}

static WebKit::WebWheelEvent::Phase toWheelEventPhase(NSGestureRecognizerState state)
{
    using enum WebKit::WebWheelEvent::Phase;
    switch (state) {
    case NSGestureRecognizerStatePossible:
        return MayBegin;
    case NSGestureRecognizerStateBegan:
        return Began;
    case NSGestureRecognizerStateChanged:
        return Changed;
    case NSGestureRecognizerStateEnded:
        return Ended;
    case NSGestureRecognizerStateCancelled:
    case NSGestureRecognizerStateFailed:
        return Cancelled;
    default:
        ASSERT_NOT_REACHED();
        return None;
    }
}

static WebCore::FloatSize velocityInView(NSPanGestureRecognizer *gesture, WKWebView *view)
{
    return WebCore::toFloatSize(WebCore::FloatPoint { [gesture velocityInView:view] });
}

static WebCore::FloatSize toRawPlatformDelta(WebCore::FloatSize delta)
{
    // rawPlatformDelta uses IOHIDEvent coordinate conventions, which have the opposite
    // sign from WebKit's delta field. This matches WebEventFactory.mm which negates
    // IOHIDEventFieldScrollX/Y values when extracting rawPlatformDelta from real events.
    return -delta;
}

static bool representsDraggableElement(const WebKit::InteractionInformationAtPosition& info)
{
    return info.isLink || info.isImage || info.isAttachment || info.isDHTMLDraggable || info.isColorInput || info.prefersDraggingOverTextSelection;
}

static bool isAnalyzableImageForLiveText(const WebKit::InteractionInformationAtPosition& info)
{
    return info.isImage && info.image && info.hostImageOrVideoElementContext && !info.isAnimatedImage && !info.isContentEditable;
}

namespace WebKit {

// The outcome of the image-analysis preflight, used to resolve the two image-analysis deferring
// gestures. Over an image we defer both text selection and the drag / context-menu fallback until
// analysis finishes, then resolve them oppositely.
enum class ImageAnalysisDeferralOutcome : uint8_t {
    NotApplicable, // Not an image (or the press was abandoned): influence neither deferral.
    NoText, // Image without selectable text: prevent text selection; allow drag / context menu.
    FoundText, // Image with selectable text: allow text selection; prevent drag / context menu.
};

} // namespace WebKit

@interface WKAppKitGestureController (ImageAnalysisDeferralResolution)
- (void)_resolveImageAnalysisDeferralsWithOutcome:(WebKit::ImageAnalysisDeferralOutcome)outcome;
@end

namespace WebKit {

// Keeps the image-analysis deferrals open for the lifetime of an in-flight analysis. When the last
// reference is dropped (analysis finished or abandoned), it resolves both deferrals on the main run
// loop according to the recorded outcome. Mirrors the iOS ImageAnalysisGestureDeferralToken.
class ImageAnalysisGestureDeferralToken final : public RefCounted<ImageAnalysisGestureDeferralToken> {
public:
    static RefPtr<ImageAnalysisGestureDeferralToken> create(WKAppKitGestureController *controller)
    {
        return adoptRef(*new ImageAnalysisGestureDeferralToken(controller));
    }

    ~ImageAnalysisGestureDeferralToken()
    {
        ensureOnMainRunLoop([controller = m_controller, outcome = m_outcome] {
            if (RetainPtr strongController = controller.get())
                [strongController _resolveImageAnalysisDeferralsWithOutcome:outcome];
        });
    }

    void setOutcome(ImageAnalysisDeferralOutcome outcome) { m_outcome = outcome; }

private:
    explicit ImageAnalysisGestureDeferralToken(WKAppKitGestureController *controller)
        : m_controller(controller)
    {
    }

    WeakObjCPtr<WKAppKitGestureController> m_controller;
    ImageAnalysisDeferralOutcome m_outcome { ImageAnalysisDeferralOutcome::NotApplicable };
};

} // namespace WebKit

static NSString *gestureLogDescription(NSGestureRecognizer *gesture)
{
    return [WKAppKitGestureController loggingDescriptionForGestureRecognizer:gesture];
}

@interface WKAppKitGestureController () <NSGestureRecognizerDelegatePrivate>
@end

@implementation WKAppKitGestureController {
    WeakPtr<WebKit::WebPageProxy> _page;
    WeakPtr<WebKit::WebViewImpl> _viewImpl;

    RetainPtr<NSPanGestureRecognizer> _panGestureRecognizer;
    RetainPtr<NSPressGestureRecognizer> _mouseTrackingGestureRecognizer;
    RetainPtr<NSPressGestureRecognizer> _singleClickGestureRecognizer;
    RetainPtr<NSClickGestureRecognizer> _doubleClickGestureRecognizer;

    // Auxiliary gesture recognizers to support context menus.
    RetainPtr<NSPressGestureRecognizer> _secondaryClickGestureRecognizer;
    RetainPtr<WKDeferringGestureRecognizer> _secondaryClickDeferringGestureRecognizer;

    // Auxiliary gesture recognizers to support drag-and-drop.
    RetainPtr<NSGestureRecognizer> _textSelectionDragGesture;
    RetainPtr<WKDeferringGestureRecognizer> _dragDeferringGestureRecognizer;

    bool _isMomentumActive;
    bool _caughtDeceleratingScroll;
    bool _suppressNextPanScrollDelta;

    bool _potentialClickInProgress;
    bool _isClickHighlightIDValid;
    bool _hasHighlightForPotentialClick;
    bool _isExpectingFastClickCommit;
    bool _isSuppressingSingleClickGestureForTextSelection;

    std::optional<WebKit::TransactionID> _layerTreeTransactionIdAtLastInteractionStart;
    Markable<WebKit::ClickIdentifier> _latestClickID;
    WebCore::PointerID _commitPotentialClickPointerId;
    WebCore::FloatPoint _lastInteractionLocationInWebView;

    bool _mouseTrackingHasSentMouseDown;
    WebCore::FloatPoint _mouseTrackingStartLocationInWindow;

    RetainPtr<NSPressGestureRecognizer> _dragPressGestureRecognizer;
    RetainPtr<NSDraggingSession> _gestureDraggingSession;
    BlockPtr<void(NSDraggingSession *)> _textSelectionDragCompletionHandler;
    bool _dragGestureHasSentMouseDown;

    RetainPtr<NSPressGestureRecognizer> _imageAnalysisGestureRecognizer;
    RetainPtr<WKDeferringGestureRecognizer> _imageAnalysisTextSelectionDeferringGestureRecognizer;
    RetainPtr<WKDeferringGestureRecognizer> _imageAnalysisDragAndContextMenuDeferringGestureRecognizer;

    std::unique_ptr<WebKit::PositionInformationManager> _positionInformationManager;
    std::unique_ptr<WebKit::WKFastScrollTracker> _fastScrollTracker;
}

#if __has_include(<WebKitAdditions/WKAppKitGestureControllerAdditionsImpl.mm>)
#import <WebKitAdditions/WKAppKitGestureControllerAdditionsImpl.mm>
#elif __has_include(<WebKitAdditions/WKAppKitGestureControllerAdditions.mm>)
#import <WebKitAdditions/WKAppKitGestureControllerAdditions.mm>
#endif

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)viewImpl
{
    if (!(self = [super init]))
        return nil;

    _page = page.get();
    _viewImpl = viewImpl.get();
    _positionInformationManager = makeUnique<WebKit::PositionInformationManager>(page.get());
    _fastScrollTracker = makeUniqueWithoutFastMallocCheck<WebKit::WKFastScrollTracker>(WebKit::WKFastScrollTracker::init());

    [self setUpGestureRecognizers];
    [self addGesturesToWebView];
    [self enableGesturesIfNeeded];
    [self ensureGesturesAreNotArchived];

    return self;
}

- (WKWebView *)webView
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return nil;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return nil;

    return webView.getAutoreleased();
}

- (NSPanGestureRecognizer *)panGestureRecognizer
{
    return _panGestureRecognizer.get();
}

- (void)setPanGestureRecognizer:(NSPanGestureRecognizer *)recognizer
{
    _panGestureRecognizer = recognizer;
}

- (void)setUpGestureRecognizers
{
    [self setUpPanGestureRecognizer];
    [self setUpMouseTrackingGestureRecognizer];
    [self setUpSingleClickGestureRecognizer];
    [self setUpDoubleClickGestureRecognizer];

    [self setUpSecondaryClickGestureRecognizer];
    [self setUpSecondaryClickDeferringGestureRecognizer];

    [self setUpDragPressGestureRecognizer];
    [self setUpDragDeferringGestureRecognizer];

    [self setUpImageAnalysisGestureRecognizer];
    [self setUpImageAnalysisDeferringGestureRecognizers];
}

- (void)setUpMouseTrackingGestureRecognizer
{
    _mouseTrackingGestureRecognizer = adoptNS([[NSPressGestureRecognizer alloc] initWithTarget:self action:@selector(mouseTrackingGestureRecognized:)]);
    [self configureForMouseTracking:_mouseTrackingGestureRecognizer.get()];
    [_mouseTrackingGestureRecognizer setDelegate:self];
    [_mouseTrackingGestureRecognizer setName:@"WKMouseTrackingGesture"];
}

- (void)setUpSingleClickGestureRecognizer
{
    _singleClickGestureRecognizer = adoptNS([[NSPressGestureRecognizer alloc] initWithTarget:self action:@selector(singleClickGestureRecognized:)]);
    [self configureForSingleClick:_singleClickGestureRecognizer.get()];
    [_singleClickGestureRecognizer setDelegate:self];
    [_singleClickGestureRecognizer setName:@"WKSingleClickGesture"];
}

- (void)setUpDoubleClickGestureRecognizer
{
    _doubleClickGestureRecognizer = adoptNS([[NSClickGestureRecognizer alloc] initWithTarget:self action:@selector(doubleClickGestureRecognized:)]);
    [self configureForDoubleClick:_doubleClickGestureRecognizer.get()];
    [_doubleClickGestureRecognizer setDelegate:self];
    [_doubleClickGestureRecognizer setName:@"WKDoubleClickGesture"];
}

- (void)setUpSecondaryClickGestureRecognizer
{
    _secondaryClickGestureRecognizer = adoptNS([[NSPressGestureRecognizer alloc] initWithTarget:self action:@selector(secondaryClickGestureRecognized:)]);
    [self configureForSecondaryClick:_secondaryClickGestureRecognizer.get()];
    [_secondaryClickGestureRecognizer setCancelPastAllowableMovement:YES];
    [_secondaryClickGestureRecognizer setDelegate:self];
    [_secondaryClickGestureRecognizer setName:@"WKSecondaryClickGesture"];
}

- (void)setUpSecondaryClickDeferringGestureRecognizer
{
    _secondaryClickDeferringGestureRecognizer = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [self configureForSecondaryClickDeferral:_secondaryClickDeferringGestureRecognizer];
    [_secondaryClickDeferringGestureRecognizer setDelegate:self];
    [_secondaryClickDeferringGestureRecognizer setName:@"WKSecondaryClickDeferringGesture"];
}

- (void)setUpDragPressGestureRecognizer
{
    _dragPressGestureRecognizer = adoptNS([[NSPressGestureRecognizer alloc] initWithTarget:self action:@selector(dragPressGestureRecognized:)]);
    [self configureForDragPress:_dragPressGestureRecognizer];
    [_dragPressGestureRecognizer setDelegate:self];
    [_dragPressGestureRecognizer setName:@"WKDragPressGesture"];
}

- (void)setUpDragDeferringGestureRecognizer
{
    _dragDeferringGestureRecognizer = adoptNS([[WKDeferringGestureRecognizer alloc] initWithDeferringGestureDelegate:self]);
    [self configureForDragDeferral:_dragDeferringGestureRecognizer];
    [_dragDeferringGestureRecognizer setDelegate:self];
    [_dragDeferringGestureRecognizer setName:@"WKDragDeferringGesture"];
}

- (void)setUpImageAnalysisGestureRecognizer
{
    _imageAnalysisGestureRecognizer = adoptNS([[NSPressGestureRecognizer alloc] initWithTarget:self action:@selector(imageAnalysisGestureRecognized:)]);
    [self configureForImageAnalysis:_imageAnalysisGestureRecognizer];
    [_imageAnalysisGestureRecognizer setDelegate:self];
    [_imageAnalysisGestureRecognizer setName:@"WKImageAnalysisGesture"];
}

- (void)setUpImageAnalysisDeferringGestureRecognizers
{
    // One deferral holds text selection over an image until analysis resolves; the other holds the
    // drag-press / secondary-click recognizers. See the Image Analysis design note for why both exist
    // and why they resolve to opposite outcomes.
    _imageAnalysisTextSelectionDeferringGestureRecognizer = [self makeImageAnalysisDeferringGestureRecognizerWithName:@"WKImageAnalysisTextSelectionDeferringGesture"];
    _imageAnalysisDragAndContextMenuDeferringGestureRecognizer = [self makeImageAnalysisDeferringGestureRecognizerWithName:@"WKImageAnalysisDragAndContextMenuDeferringGesture"];
}

- (void)addGesturesToWebView
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    [webView addGestureRecognizer:_panGestureRecognizer.get()];
    [webView addGestureRecognizer:_mouseTrackingGestureRecognizer.get()];
    [webView addGestureRecognizer:_singleClickGestureRecognizer.get()];
    [webView addGestureRecognizer:_doubleClickGestureRecognizer.get()];

    [webView addGestureRecognizer:_secondaryClickGestureRecognizer.get()];
    [webView addGestureRecognizer:_secondaryClickDeferringGestureRecognizer.get()];

    [webView addGestureRecognizer:_dragPressGestureRecognizer.get()];
    [webView addGestureRecognizer:_dragDeferringGestureRecognizer.get()];

    [webView addGestureRecognizer:_imageAnalysisGestureRecognizer.get()];
    [webView addGestureRecognizer:_imageAnalysisTextSelectionDeferringGestureRecognizer.get()];
    [webView addGestureRecognizer:_imageAnalysisDragAndContextMenuDeferringGestureRecognizer.get()];
}

- (void)enableGesturesIfNeeded
{
    [self enableGestureIfNeeded:_panGestureRecognizer.get()];
    [self enableGestureIfNeeded:_mouseTrackingGestureRecognizer.get()];
    [self enableGestureIfNeeded:_singleClickGestureRecognizer.get()];
    [self enableGestureIfNeeded:_doubleClickGestureRecognizer.get()];
    [self enableGestureIfNeeded:_secondaryClickGestureRecognizer.get()];
    [self enableGestureIfNeeded:_dragPressGestureRecognizer.get()];
    [self enableGestureIfNeeded:_imageAnalysisGestureRecognizer.get()];

    // The deferring gesture recognizers are intentionally not enabled.
}

- (void)ensureGesturesAreNotArchived
{
    // The set of gestures managed by WKAppKitGestureController are configured
    // and installed freshly for every web view initialization, and as such we
    // want to avoid duplicate sets when views are decoded.

    [_panGestureRecognizer setShouldBeArchived:NO];
    [_mouseTrackingGestureRecognizer setShouldBeArchived:NO];
    [_singleClickGestureRecognizer setShouldBeArchived:NO];
    [_doubleClickGestureRecognizer setShouldBeArchived:NO];

    [_secondaryClickGestureRecognizer setShouldBeArchived:NO];
    [_secondaryClickDeferringGestureRecognizer setShouldBeArchived:NO];

    [_dragPressGestureRecognizer setShouldBeArchived:NO];
    [_dragDeferringGestureRecognizer setShouldBeArchived:NO];

    [_imageAnalysisGestureRecognizer setShouldBeArchived:NO];
    [_imageAnalysisTextSelectionDeferringGestureRecognizer setShouldBeArchived:NO];
    [_imageAnalysisDragAndContextMenuDeferringGestureRecognizer setShouldBeArchived:NO];
}

- (void)enableGestureIfNeeded:(NSGestureRecognizer *)gesture
{
    RefPtr page = _page.get();
    if (!page)
        return;

    bool gestureEnabled = protect(page->preferences())->useAppKitGestures();

    if (gesture == _imageAnalysisGestureRecognizer)
        gestureEnabled = gestureEnabled && WebKit::isLiveTextAvailableAndEnabled();

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "%@ setEnabled:%d", gestureLogDescription(gesture), static_cast<int>(gestureEnabled));
    [gesture setEnabled:gestureEnabled];
}

- (void)beginSuppressingSingleClickGestureForTextSelection
{
    RefPtr page = _page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Begin suppressing single-click gesture for text selection");

    _isSuppressingSingleClickGestureForTextSelection = true;

    [self _handleClickCancelled];
    page->cancelPotentialClick();
}

- (void)endSuppressingSingleClickGestureForTextSelection
{
    RefPtr page = _page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "End suppressing single-click gesture for text selection");

    _isSuppressingSingleClickGestureForTextSelection = false;
}

#pragma mark - Gesture Recognition

- (void)panGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "%@", gestureLogDescription(gesture));

    RELEASE_ASSERT(_panGestureRecognizer == gesture);

    if (viewImpl->ignoresAllEvents()) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "Ignored gesture");
        return;
    }

    if ([gesture state] == NSGestureRecognizerStateBegan) {
        viewImpl->dismissContentRelativeChildWindowsWithAnimation(false);
        _fastScrollTracker->didStartGesture([gesture locationInView:nil]);
    }

#if HAVE(NSREFRESHCONTROLLER)
    viewImpl->updateRefreshControllerForPanGesture([gesture state]);
#endif

    [self sendWheelEventForGesture:_panGestureRecognizer];
    [self startMomentumIfNeededForGesture:_panGestureRecognizer];

    switch (gesture.state) {
    case NSGestureRecognizerStateEnded:
    case NSGestureRecognizerStateCancelled:
    case NSGestureRecognizerStateFailed:
        [self _resetCaughtDeceleratingScroll];
        break;
    default:
        break;
    }
}

- (void)singleClickGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "%@", gestureLogDescription(gesture));

    RELEASE_ASSERT(_singleClickGestureRecognizer == gesture);

    if (_caughtDeceleratingScroll) {
        // This gesture is interrupting a decelerating scroll; it should stop the scroll (and may
        // turn into a pan) but must never perform a click.
        switch (gesture.state) {
        case NSGestureRecognizerStateEnded:
        case NSGestureRecognizerStateCancelled:
        case NSGestureRecognizerStateFailed:
            [self _resetCaughtDeceleratingScroll];
            [self _handleClickCancelled];
            break;
        default:
            break;
        }
        return;
    }

    // Clicks aren't delivered to NSButton's built-in click gesture
    // recognizer when a parent view's GR recognizes first, so we
    // forward the click manually.
    if (gesture.state == NSGestureRecognizerStateBegan) {
        WebCore::FloatPoint location { [gesture locationInView:webView.get()] };
        if (RetainPtr hitView = viewImpl->hitTestPDFHUD(location); hitView && viewImpl->isViewVisible(hitView.get())) {
            if (RetainPtr hudButton = dynamic_objc_cast<NSButton>(hitView))
                [hudButton performClick:nil];
            gesture.state = NSGestureRecognizerStateCancelled;
            return;
        }
    }

    switch (gesture.state) {
    case NSGestureRecognizerStateBegan:
        [self _handleClickBegan:gesture];
        break;
    case NSGestureRecognizerStateEnded:
        [self _handleClickEnded:gesture];
        break;
    case NSGestureRecognizerStateCancelled:
    case NSGestureRecognizerStateFailed:
        [self _handleClickCancelled];
        break;
    default:
        break;
    }
}

- (void)doubleClickGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "%@", gestureLogDescription(gesture));

    RELEASE_ASSERT(_doubleClickGestureRecognizer == gesture);

    viewImpl->dismissContentRelativeChildWindowsWithAnimation(false);

    auto magnificationOrigin = [webView convertPoint:[gesture locationInView:nil] fromView:nil];
    protect(viewImpl->ensureGestureController())->handleSmartMagnificationGesture(magnificationOrigin);
}

- (void)secondaryClickGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "%@", gestureLogDescription(gesture));

    RELEASE_ASSERT(_secondaryClickGestureRecognizer == gesture);

    if (gesture.state == NSGestureRecognizerStateBegan) {
        [self _handleClickCancelled];
        return;
    }

    if (gesture.state != NSGestureRecognizerStateEnded)
        return;

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto modifierFlags = [gesture modifierFlags];
ALLOW_NEW_API_WITHOUT_GUARDS_END
    auto location = [gesture locationInView:nil];
    auto windowNumber = viewImpl->windowNumber();

    RetainPtr mouseDown = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown location:location modifierFlags:modifierFlags timestamp:GetCurrentEventTime() windowNumber:windowNumber context:NULL eventNumber:0 clickCount:1 pressure:1.0];
    viewImpl->mouseDown(mouseDown.get(), WebKit::WebEventInputSource::Automation);

    RetainPtr mouseUp = [NSEvent mouseEventWithType:NSEventTypeRightMouseUp location:location modifierFlags:modifierFlags timestamp:GetCurrentEventTime() windowNumber:windowNumber context:NULL eventNumber:0 clickCount:1 pressure:0.0];
    viewImpl->mouseUp(mouseUp.get(), WebKit::WebEventInputSource::Automation);
}

- (void)mouseTrackingGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl) {
        ASSERT_NOT_REACHED();
        return;
    }

    RetainPtr webView = viewImpl->view();
    if (!webView) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr page = _page.get();
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "%@", gestureLogDescription(gesture));

    if (_dragGestureHasSentMouseDown) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "Exiting early because _dragGestureHasSentMouseDown is true");
        return;
    }

    if (_isSuppressingSingleClickGestureForTextSelection) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "Exiting early because _isSuppressingSingleClickGestureForTextSelection is true");
        return;
    }

    RELEASE_ASSERT(_mouseTrackingGestureRecognizer == gesture);

    if (viewImpl->ignoresAllEvents())
        return;

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto modifierFlags = [gesture modifierFlags];
ALLOW_NEW_API_WITHOUT_GUARDS_END
    WebCore::FloatPoint locationInWindow { [gesture locationInView:nil] };
    auto windowNumber = viewImpl->windowNumber();
    auto timestamp = GetCurrentEventTime();

    switch (gesture.state) {
    case NSGestureRecognizerStateBegan:
        _mouseTrackingHasSentMouseDown = false;
        _mouseTrackingStartLocationInWindow = locationInWindow;
        break;

    case NSGestureRecognizerStateChanged: {
        if (!_mouseTrackingHasSentMouseDown) {
            // Either the synthetic single-click path or this mouse-tracking path delivers a mouse
            // down for a given interaction, but never both. An event that stays within the single-click
            // gesture's allowable movement is a click: that gesture stays alive and, when it ends, the
            // synthetic click path delivers the mouse down, mouse up, and click (plus pointer events).
            // Only once the event moves past that threshold — at which point the single-click gesture
            // cancels and this becomes a drag — does mouse tracking take over event delivery. Sending a
            // mouse down here for a stationary click would deliver a second mouse down to the content,
            // which should be avoided.
            auto movementInWindow = locationInWindow - _mouseTrackingStartLocationInWindow;
            auto allowableMovement = [_singleClickGestureRecognizer allowableMovement];
            if (std::abs(movementInWindow.width()) <= allowableMovement && std::abs(movementInWindow.height()) <= allowableMovement)
                break;

            RetainPtr mouseDown = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                location:_mouseTrackingStartLocationInWindow
                modifierFlags:modifierFlags
                timestamp:timestamp
                windowNumber:windowNumber
                context:nil
                eventNumber:0
                clickCount:1
                pressure:1.0];
            viewImpl->mouseDown(mouseDown.get(), WebKit::WebEventInputSource::Automation, WebCore::PlatformMouseEvent::CanInitiateDrag::No);
            _mouseTrackingHasSentMouseDown = true;
        }

        RetainPtr mouseDragged = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
            location:locationInWindow
            modifierFlags:modifierFlags
            timestamp:timestamp
            windowNumber:windowNumber
            context:nil
            eventNumber:0
            clickCount:1
            pressure:1.0];
        viewImpl->mouseDragged(mouseDragged.get(), WebKit::WebEventInputSource::Automation, WebCore::PlatformMouseEvent::CanInitiateDrag::No);
        break;
    }

    case NSGestureRecognizerStateEnded: {
        if (_mouseTrackingHasSentMouseDown) {
            RetainPtr mouseUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                location:locationInWindow
                modifierFlags:modifierFlags
                timestamp:timestamp
                windowNumber:windowNumber
                context:nil
                eventNumber:0
                clickCount:1
                pressure:0.0];
            viewImpl->mouseUp(mouseUp.get(), WebKit::WebEventInputSource::Automation, WebCore::PlatformMouseEvent::CanInitiateDrag::No);
        }
        [[fallthrough]];
    }

    case NSGestureRecognizerStateCancelled:
    case NSGestureRecognizerStateFailed:
        _mouseTrackingHasSentMouseDown = false;
        break;

    default:
        break;
    }
}

#pragma mark - Image Analysis

// Gesture-driven Live Text is arbitrated by three recognizers, plus the deferral token above:
//
//   1. _imageAnalysisGestureRecognizer (the "preflight") -- a settled 0.1s press that kicks off Vision
//      image analysis. It's a passive observer of the interaction it measures: it recognizes
//      simultaneously with everything, prevents nothing, and is always allowed to begin.
//   2. _imageAnalysisTextSelectionDeferringGestureRecognizer -- holds the text-selection gestures over an image until
//      analysis resolves.
//   3. _imageAnalysisDragAndContextMenuDeferringGestureRecognizer -- holds the drag-press and secondary-click
//      (context menu) recognizers over an image until analysis resolves.
//
// The two deferrals resolve to *opposite* booleans (see -_resolveImageAnalysisDeferralsWithOutcome:):
// on "text found" we must ALLOW text selection while PREVENTING drag / context menu (Live Text wins),
// and the reverse on "no text". A single WKDeferringGestureRecognizer can't express that -- its
// -endDeferralShouldPreventGestures: applies one boolean (state = .ended / .failed) to everything it
// defers -- so the two opposite outcomes require two separate deferrals.
//
// iOS needs no fallback deferral because its drag / context menu are UIKit interactions gated by a
// ProceedWithTextSelectionInImage callback. macOS drag-press / secondary-click are NSGestureRecognizers
// on independent timers, so they must be *deferred* to keep them from firing before analysis resolves.

- (void)imageAnalysisGestureRecognized:(NSGestureRecognizer *)gesture
{
    RELEASE_ASSERT(_imageAnalysisGestureRecognizer == gesture);

    if (gesture.state != NSGestureRecognizerStateBegan)
        return;

    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    if (viewImpl->ignoresAllEvents())
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "%@", gestureLogDescription(gesture));

    WebKit::InteractionInformationRequest request { WebCore::IntPoint { [gesture locationInView:webView.get()] } };
    request.includeImageData = true;

    // The token keeps the image-analysis deferral open until the async analysis chain finishes (or is
    // abandoned). Capturing it through each stage means the deferral resolves exactly when the last
    // stage completes; dropping it early (e.g. not an image) resolves it as "allow text selection".
    _positionInformationManager->doAfterUpdate(request, [weakSelf = WeakObjCPtr<WKAppKitGestureController>(self), gestureDeferralToken = WebKit::ImageAnalysisGestureDeferralToken::create(self)](const auto& optionalInfo) mutable {
        if (!optionalInfo)
            return;

        const auto& info = *optionalInfo;

        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        CheckedPtr viewImpl = strongSelf->_viewImpl.get();
        if (!viewImpl)
            return;

        RefPtr page = strongSelf->_page.get();
        if (!page)
            return;

        if (!isAnalyzableImageForLiveText(info)) {
            // Not an analyzable image: leave both image-analysis deferrals with the default
            // NotApplicable outcome, so text selection and the drag / context-menu fallback each
            // resolve on their own (non-image) merits.
            return;
        }

        // This dereference is guaranteed to be non-nil due to the `isAnalyzableImageForLiveText` check above.
        RetainPtr cgImage = info.image->createPlatformImage();
        if (!cgImage) {
            // An image we can't rasterize: treat it as an image without text so the press falls
            // through to drag / context menu.
            gestureDeferralToken->setOutcome(WebKit::ImageAnalysisDeferralOutcome::NoText);
            return;
        }

        RELEASE_LOG(ImageAnalysis, "Image analysis preflight gesture initiated.");

        const auto requestLocation = info.request.point;
        // This dereference is guaranteed to be non-nil due to the `isAnalyzableImageForLiveText` check above.
        const auto elementContext = *info.hostImageOrVideoElementContext;

        const auto analyzerRequest = WebKit::createImageAnalyzerRequest(cgImage.get(), VKAnalysisTypeText);
        const auto startTime = MonotonicTime::now();
        viewImpl->processImageAnalyzerRequest(analyzerRequest.get(), [weakSelf, elementContext, requestLocation, gestureDeferralToken = WTF::move(gestureDeferralToken), startTime](RetainPtr<VKCImageAnalysis>&& result, NSError *) mutable {
            RetainPtr strongSelf = weakSelf.get();
            if (!strongSelf)
                return;

            RefPtr page = strongSelf->_page.get();
            if (!page)
                return;

            auto hasTextResults = [result hasResultsForAnalysisTypes:VKAnalysisTypeText];
            RELEASE_LOG(ImageAnalysis, "Image analysis completed in %.0f ms (found text? %d)", (MonotonicTime::now() - startTime).milliseconds(), hasTextResults);

            page->updateWithTextRecognitionResult(WebKit::makeTextRecognitionResult(result.get()), elementContext, requestLocation, [gestureDeferralToken = WTF::move(gestureDeferralToken)](const auto& updateResult) mutable {
                // Text found and injected as an image overlay -> allow the deferred text selection and
                // prevent the drag / context-menu fallback (Live Text wins). Otherwise -> the reverse.
                gestureDeferralToken->setOutcome(updateResult == WebKit::TextRecognitionUpdateResult::Text
                    ? WebKit::ImageAnalysisDeferralOutcome::FoundText
                    : WebKit::ImageAnalysisDeferralOutcome::NoText);
            });
        });
    });
}

- (void)_resolveImageAnalysisDeferralsWithOutcome:(WebKit::ImageAnalysisDeferralOutcome)outcome
{
    // The text-selection deferral is prevented unless text was found; the drag / context-menu fallback
    // deferral is the mirror image. NotApplicable means "not an analyzable image," so neither deferral
    // should have any opinion -- release both.
    BOOL preventTextSelection = outcome == WebKit::ImageAnalysisDeferralOutcome::NoText;
    BOOL preventDragAndContextMenu = outcome == WebKit::ImageAnalysisDeferralOutcome::FoundText;

    // Only resolve a deferral that's still deferring. A deferral may already be resolved by the time the
    // analysis token drops -- e.g. it failed on lift (immediatelyFailsAfterActionEnd) before slow
    // analysis finished -- and re-setting an already-terminal gesture state would be a no-op at best.
    if ([_imageAnalysisTextSelectionDeferringGestureRecognizer state] == NSGestureRecognizerStatePossible)
        [_imageAnalysisTextSelectionDeferringGestureRecognizer endDeferralShouldPreventGestures:preventTextSelection];

    if ([_imageAnalysisDragAndContextMenuDeferringGestureRecognizer state] == NSGestureRecognizerStatePossible)
        [_imageAnalysisDragAndContextMenuDeferringGestureRecognizer endDeferralShouldPreventGestures:preventDragAndContextMenu];
}

#pragma mark - WKDeferringGestureRecognizerDelegate

- (BOOL)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer shouldDeferOtherGestureRecognizer:(NSGestureRecognizer *)gestureRecognizer
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return NO;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return NO;

    // Live Text (see the Image Analysis design note): the fallback deferral holds the drag-press /
    // secondary-click recognizers -- not text selection -- until analysis resolves, so a slow analysis
    // can't let drag-press fire and drag the image before Live Text has a chance to win.
    if (deferringGestureRecognizer == _imageAnalysisDragAndContextMenuDeferringGestureRecognizer)
        return gestureRecognizer == _dragPressGestureRecognizer || gestureRecognizer == _secondaryClickGestureRecognizer;

    for (NSGestureRecognizer *textSelectionGesture in [[webView textSelectionManager] gesturesForFailureRequirements]) {
        if (gestureRecognizer == textSelectionGesture)
            return YES;
    }

    return NO;
}

- (BOOL)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer shouldDeferGesturesForEventThatWillBeginAction:(NSEvent *)event
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return NO;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return NO;

    NSPoint locationInView = [webView convertPoint:[event locationInWindow] fromView:nil];

    const auto isInScrollbar = [self _isPointInScrollbar:locationInView];

    if (!isInScrollbar && viewImpl->isTextSelectedAtPoint(locationInView)) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "deferral: not deferring; text already selected at %@", NSStringFromPoint(locationInView));
        return NO;
    }

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "deferral: deferring; awaiting position info at %@", NSStringFromPoint(locationInView));

    WebKit::InteractionInformationRequest request { WebCore::IntPoint { locationInView } };
    _positionInformationManager->doAfterUpdate(request, [weakSelf = WeakObjCPtr<WKAppKitGestureController>(self), weakDeferring = WeakObjCPtr<WKDeferringGestureRecognizer>(deferringGestureRecognizer), isInScrollbar](const auto& optionalInfo) {
        if (!optionalInfo)
            return;

        const auto& info = *optionalInfo;

        RetainPtr strongSelf = weakSelf.get();
        RetainPtr strongDeferring = weakDeferring.get();
        if (!strongSelf || !strongDeferring)
            return;

        auto deferralState = [strongDeferring state];
        if (deferralState != NSGestureRecognizerStatePossible) {
            WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { strongSelf->_page.get() }->logIdentifier(),
                "deferral: position info arrived after deferring gesture exited Possible (state=%ld); skipping resolution", static_cast<long>(deferralState));
            return;
        }

        const auto shouldPreventGestures = [&]() -> std::optional<bool> {
            const auto overLiveTextImage = WebKit::isLiveTextAvailableAndEnabled() && info.isImage;

            // Over a scrollbar: drive the scrollbar, not text selection -- prevent the deferred gestures
            // so the mouse-tracking -> `Scrollbar::mouseDown` path wins.
            if (isInScrollbar) {
                WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { strongSelf->_page.get() }->logIdentifier(), "deferral resolved: over scrollbar; preventing text-selection gestures");
                return true;
            }

            if (strongDeferring == strongSelf->_imageAnalysisTextSelectionDeferringGestureRecognizer
                || strongDeferring == strongSelf->_imageAnalysisDragAndContextMenuDeferringGestureRecognizer) {
                // Image-analysis deferrals (see the Image Analysis design note): over an image with Live
                // Text available, keep deferring (std::nullopt) until the preflight's analysis token
                // resolves both deferrals (see -_resolveImageAnalysisDeferralsWithOutcome:); otherwise this
                // deferral has no opinion, so release. We only have basic hit-test info here (this request
                // does not fetch image data), so key off info.isImage; the preflight does the full
                // analyzability check.
                if (overLiveTextImage)
                    return std::nullopt;

                return false;
            }

            if (strongDeferring == strongSelf->_dragDeferringGestureRecognizer) {
                const auto isDraggable = representsDraggableElement(info);
                WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { strongSelf->_page.get() }->logIdentifier(), "deferral resolved: isDraggable=%d (link=%d image=%d attachment=%d dhtml=%d color=%d prefersDrag=%d)", isDraggable, info.isLink, info.isImage, info.isAttachment, info.isDHTMLDraggable, info.isColorInput, info.prefersDraggingOverTextSelection);
                return isDraggable && !overLiveTextImage;
            }

            if (strongDeferring == strongSelf->_secondaryClickDeferringGestureRecognizer) {
                const auto isSelectable = info.isSelectable();
                WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { strongSelf->_page.get() }->logIdentifier(), "Resolved deferral: isSelectable=%d", isSelectable);
                return !isSelectable && !overLiveTextImage;
            }

            RELEASE_ASSERT_NOT_REACHED();
        }();

        if (shouldPreventGestures)
            [strongDeferring endDeferralShouldPreventGestures:*shouldPreventGestures];
    });

    return YES;
}

- (void)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer didEndActionWithEvent:(NSEvent *)event
{
    if (deferringGestureRecognizer.state != NSGestureRecognizerStatePossible)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(RefPtr { _page.get() }->logIdentifier(), "deferral: press ended before position info arrived; unblocking text selection");
    _positionInformationManager->abandonOutstandingRequest();

    [deferringGestureRecognizer endDeferralShouldPreventGestures:NO];
}

- (void)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer didTransitionToState:(NSGestureRecognizerState)state
{
}

#pragma mark - Position Information

- (void)didCommitLoadForMainFrame
{
    _positionInformationManager->invalidate();
}

- (void)positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info
{
    // Resolving queued callbacks can re-enter into client code (e.g. UI delegate callbacks), which
    // may release the WKWebView and tear `self` down before this method returns.
    RetainPtr protectedSelf = self;
    _positionInformationManager->didChange(info);
}

- (BOOL)_isPointInScrollbar:(NSPoint)locationInViewCoordinates
{
    CheckedPtr viewImpl = _viewImpl.get();
    return viewImpl && viewImpl->isPointInScrollbar(locationInViewCoordinates);
}

- (BOOL)_secondaryClickShouldBeginAtLocation:(NSPoint)locationInViewCoordinates
{
    WebKit::InteractionInformationRequest request { WebCore::IntPoint { locationInViewCoordinates } };

    bool requestIsValid = _positionInformationManager->currentIsValid(request);
    bool isSelectable = _positionInformationManager->currentInformation().isSelectable();
    bool isOverSelectableText = _positionInformationManager->currentInformation().isOverSelectableText;

    // The secondary click owns selectable points that are not over actual text (e.g. the page
    // background). Over a run of selectable text, the text selection manager should win so that a
    // long press selects a word instead of synthesizing a context menu.
    bool shouldBegin = requestIsValid && isSelectable && !isOverSelectableText;

    if (!requestIsValid)
        _positionInformationManager->invalidate();

    return shouldBegin;
}

- (BOOL)_positionInformationRequestIsValidAtLocation:(NSPoint)locationInViewCoordinates withRadius:(NSInteger)radius
{
    WebKit::InteractionInformationRequest request { WebCore::IntPoint { locationInViewCoordinates } };
    return _positionInformationManager->currentIsApproximatelyValid(request, radius);
}

- (BOOL)_dragPressShouldBeginAtLocation:(NSPoint)locationInViewCoordinates
{
    int radius = static_cast<int>(std::ceil([_dragPressGestureRecognizer allowableMovement]));

    const auto& information = _positionInformationManager->currentInformation();

    // FIXME: Migrate to requestDragStart: IPC for an authoritative decision.
    // The heuristic below approximates DragController::draggableElement() by consulting the same element-type and style signals.
    bool isDraggable = representsDraggableElement(information);
    bool requestIsValid = [self _positionInformationRequestIsValidAtLocation:locationInViewCoordinates withRadius:radius];
    bool shouldDrag = requestIsValid && isDraggable;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(
        RefPtr { _page.get() }->logIdentifier(),
        "Drag-press shouldBegin → %d (hasInfo=%d link=%d image=%d attachment=%d dhtml=%d color=%d prefersDrag=%d radius=%d)",
        shouldDrag,
        _positionInformationManager->hasValidCurrentInformation(),
        information.isLink,
        information.isImage,
        information.isAttachment,
        information.isDHTMLDraggable,
        information.isColorInput,
        information.prefersDraggingOverTextSelection,
        radius
    );

    if (!requestIsValid)
        _positionInformationManager->invalidate();

    return shouldDrag;
}

- (BOOL)_panShouldBeginAtLocation:(NSPoint)locationInViewCoordinates
{
    static constexpr int panPositionInformationToleranceRadius = 15;
    bool requestIsValid = [self _positionInformationRequestIsValidAtLocation:locationInViewCoordinates withRadius:panPositionInformationToleranceRadius];

    const auto& information = _positionInformationManager->currentInformation();

    // FIXME: (rdar://181964604) Because of this logic, vertically scrolling over these elements likely will not work.
    bool prefersInteraction = information.isRangeInput || information.isARIASlider;
    bool yieldToContent = requestIsValid && prefersInteraction;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(
        RefPtr { _page.get() }->logIdentifier(),
        "Pan shouldBegin → %d (hasInfo=%d valid=%d prefersInteraction=%d)",
        !yieldToContent,
        _positionInformationManager->hasValidCurrentInformation(),
        requestIsValid,
        prefersInteraction
    );

    return !yieldToContent;
}

#pragma mark - Drag Handling

- (void)dragPressGestureRecognized:(NSGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "%@", gestureLogDescription(gesture));

    if (_dragPressGestureRecognizer != gesture)
        return;

    if (viewImpl->ignoresAllEvents()) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "Ignored gesture");
        return;
    }

    ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    auto modifierFlags = [gesture modifierFlags];
    ALLOW_NEW_API_WITHOUT_GUARDS_END
    NSPoint locationInWindow = [gesture locationInView:nil];
    auto windowNumber = viewImpl->windowNumber();
    auto timestamp = GetCurrentEventTime();

    switch (gesture.state) {
    case NSGestureRecognizerStateBegan: {
        [self _handleClickCancelled];
        _dragGestureHasSentMouseDown = false;

        RetainPtr mouseDown = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:locationInWindow modifierFlags:modifierFlags timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:1.0];
        viewImpl->mouseDown(mouseDown.get(), WebKit::WebEventInputSource::Automation, WebCore::PlatformMouseEvent::CanInitiateDrag::Yes);
        _dragGestureHasSentMouseDown = true;
        break;
    }
    case NSGestureRecognizerStateChanged: {
        if (!_dragGestureHasSentMouseDown)
            break;
        // Drive WebCore's drag-initiation hysteresis. Once the session exists, AppKit tracks the
        // gesture itself and WebCore is driven by the platform drag callbacks, so we stop feeding it.
        if (!_gestureDraggingSession) {
            RetainPtr mouseDragged = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged location:locationInWindow modifierFlags:modifierFlags timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:1.0];
            viewImpl->mouseDragged(mouseDragged, WebKit::WebEventInputSource::Automation, WebCore::PlatformMouseEvent::CanInitiateDrag::Yes);
        }
        break;
    }
    case NSGestureRecognizerStateEnded:
    case NSGestureRecognizerStateCancelled:
    case NSGestureRecognizerStateFailed: {
        if (!_dragGestureHasSentMouseDown)
            break;

        RetainPtr mouseUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:locationInWindow modifierFlags:modifierFlags timestamp:timestamp windowNumber:windowNumber context:nil eventNumber:0 clickCount:1 pressure:0.0];
        viewImpl->mouseUp(mouseUp.get(), WebKit::WebEventInputSource::Automation, WebCore::PlatformMouseEvent::CanInitiateDrag::Yes);

        // We do not clear gesture drag state here since startDrag() may still be in flight via IPC.
        // State is cleared in draggingSessionEnded: (normal completion) or in startDrag() when
        // beginDraggingSessionWithItems:gesture: returns nil (gesture ended before session started).
        break;
    }
    default:
        break;
    }
}

#pragma mark - Click Handling

- (void)_handleClickBegan:(NSGestureRecognizer *)gesture
{
    if (_isSuppressingSingleClickGestureForTextSelection)
        return;

    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    WebCore::FloatPoint position = [gesture locationInView:webView.get()];
    _lastInteractionLocationInWebView = position;

    if (RefPtr drawingArea = page->drawingArea()) {
        if (RefPtr remoteDrawingArea = dynamicDowncast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*drawingArea))
            _layerTreeTransactionIdAtLastInteractionStart = remoteDrawingArea->lastCommittedMainFrameLayerTreeTransactionID();
    }

    _latestClickID = WebKit::ClickIdentifier::generate();
    _potentialClickInProgress = true;
    _isClickHighlightIDValid = true;
    _isExpectingFastClickCommit = ![_doubleClickGestureRecognizer isEnabled];

    page->potentialClickAtPosition(std::nullopt, WebCore::FloatPoint(position), false, *_latestClickID, WebKit::WebEventInputSource::Automation);
}

- (void)_handleClickEnded:(NSGestureRecognizer *)gesture
{
    if (!_potentialClickInProgress)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    [self _endPotentialClickAndEnableDoubleClickGesturesIfNecessary];

    _commitPotentialClickPointerId = WebCore::mousePointerID;

    if (!_layerTreeTransactionIdAtLastInteractionStart) {
        [self _handleClickCancelled];
        return;
    }

    page->commitPotentialClick(std::nullopt, { }, *_layerTreeTransactionIdAtLastInteractionStart, _commitPotentialClickPointerId);
}

- (void)_handleClickCancelled
{
    if (!_potentialClickInProgress)
        return;

    _potentialClickInProgress = false;
    _isClickHighlightIDValid = false;

    if (RefPtr page = _page.get())
        page->cancelPotentialClick();
}

- (void)_endPotentialClickAndEnableDoubleClickGesturesIfNecessary
{
    _potentialClickInProgress = false;
    [self _setDoubleClickGesturesEnabled:YES];
}

- (void)_setDoubleClickGesturesEnabled:(BOOL)enabled
{
    [_doubleClickGestureRecognizer setEnabled:enabled];
}

#if ENABLE(TWO_PHASE_CLICKS)

#pragma mark - Two-Phase Click Response Handlers

- (BOOL)isPotentialClickInProgress
{
    return _potentialClickInProgress;
}

- (void)didGetClickHighlightForRequest:(WebKit::ClickIdentifier)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius nodeHasBuiltInClickHandling:(BOOL)nodeHasBuiltInClickHandling
{
    RefPtr page = _page.get();
    if (!page)
        return;

    if (!_isClickHighlightIDValid || _latestClickID != requestID)
        return;

    _isClickHighlightIDValid = false;
    _hasHighlightForPotentialClick = _potentialClickInProgress;

    // FIXME: Bring up support for click highlighting here.

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Received click highlight for request %llu, nodeHasBuiltInClickHandling=%d", requestID.toUInt64(), nodeHasBuiltInClickHandling);
}

- (void)disableDoubleClickGesturesDuringClickIfNecessary:(WebKit::ClickIdentifier)requestID
{
    if (_latestClickID != requestID)
        return;

    [self _setDoubleClickGesturesEnabled:NO];
}

- (void)commitPotentialClickFailed
{
    RefPtr page = _page.get();
    if (!page)
        return;

    _commitPotentialClickPointerId = 0;
    [self _handleClickCancelled];

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_ERROR(page->logIdentifier(), "Commit potential click failed");
}

- (void)didCompleteSyntheticClick
{
    RefPtr page = _page.get();
    if (!page)
        return;

    _commitPotentialClickPointerId = 0;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Synthetic click completed");
}

- (void)didHandleClickAsHover
{
    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Click was handled as hover");
}

- (void)didNotHandleClickAsClick:(const WebCore::IntPoint&)point
{
    RefPtr page = _page.get();
    if (!page)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Click at (%d, %d) was not handled as click", point.x(), point.y());

    // FIXME: Consider smart magnification here if a double-click is pending and the point hasn't moved significantly.
}

#endif

#pragma mark - Wheel Event Handling

- (void)sendWheelEventForGesture:(NSPanGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    auto timestamp = MonotonicTime::fromRawSeconds([gesture timestamp]);
    WebCore::IntPoint position { [gesture locationInView:webView.get()] };
    auto globalPosition { WebCore::globalPoint([gesture locationInView:nil], [webView window]) };
    auto gestureDelta { translationInView(gesture, webView.get()) };

    if (std::exchange(_suppressNextPanScrollDelta, false))
        gestureDelta = { };

    auto wheelTicks { gestureDelta.scaled(1. / static_cast<float>(WebCore::Scrollbar::pixelsPerLineStep())) };
    auto granularity = WebKit::WebWheelEvent::Granularity::ScrollByPixelWheelEvent;
    bool directionInvertedFromDevice = false;
    auto phase = toWheelEventPhase(gesture.state);
    auto momentumPhase = WebKit::WebWheelEvent::Phase::None;
    bool hasPreciseScrollingDeltas = true;
    uint32_t scrollCount = 1;
    auto unacceleratedScrollingDelta = gestureDelta;
    auto ioHIDEventTimestamp = timestamp;
    std::optional<WebCore::FloatSize> rawPlatformDelta;
    auto momentumEndType = WebKit::WebWheelEvent::MomentumEndType::Unknown;

    WebKit::WebWheelEvent wheelEvent {
        { WebKit::WebEventType::Wheel, { }, timestamp, WTF::UUID::createVersion4() },
        WebCore::IntPoint { position },
        WebCore::IntPoint { globalPosition },
        gestureDelta,
        wheelTicks,
        granularity,
        directionInvertedFromDevice,
        phase,
        momentumPhase,
        hasPreciseScrollingDeltas,
        scrollCount,
        unacceleratedScrollingDelta,
        ioHIDEventTimestamp,
        rawPlatformDelta,
        momentumEndType,
        WebKit::WebEventInputSource::Automation
    };

    WebKit::NativeWebWheelEvent nativeEvent { wheelEvent };

    if (viewImpl->allowsBackForwardNavigationGestures() && protect(viewImpl->ensureGestureController())->handleScrollWheelEvent(nativeEvent)) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(page->logIdentifier(), "View gesture controller handled gesture");
        return;
    }

    page->handleNativeWheelEvent(nativeEvent);
}

#pragma mark - Momentum Handling

- (void)startMomentumIfNeededForGesture:(NSPanGestureRecognizer *)gesture
{
    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    if (gesture.state != NSGestureRecognizerStateEnded)
        return;

    auto velocity = velocityInView(gesture, webView.get());
    auto velocityMagnitude = std::max(std::abs(velocity.width()), std::abs(velocity.height()));
    static constexpr float minimumVelocityForMomentum = 20;
    if (velocityMagnitude < minimumVelocityForMomentum)
        return;

    auto timestamp = MonotonicTime::fromRawSeconds([gesture timestamp]);
    WebCore::IntPoint position { [gesture locationInView:webView.get()] };
    auto globalPosition = WebCore::globalPoint([gesture locationInView:nil], [webView window]);

    auto fastScrollMultiplier = _fastScrollTracker->update([gesture locationInView:nil], velocity, [gesture timestamp]);

    WebKit::WebWheelEvent momentumEvent {
        { WebKit::WebEventType::Wheel, { }, timestamp, WTF::UUID::createVersion4() },
        position,
        WebCore::IntPoint { globalPosition },
        WebCore::FloatSize { },
        WebCore::FloatSize { },
        WebKit::WebWheelEvent::Granularity::ScrollByPixelWheelEvent,
        false,
        WebKit::WebWheelEvent::Phase::None,
        WebKit::WebWheelEvent::Phase::Began,
        true,
        1,
        WebCore::FloatSize { },
        timestamp,
        std::nullopt,
        WebKit::WebWheelEvent::MomentumEndType::Unknown,
        WebKit::WebEventInputSource::Automation,
        static_cast<float>(fastScrollMultiplier),
    };
    WebKit::NativeWebWheelEvent nativeMomentumEvent { momentumEvent };

    nativeMomentumEvent.setRawPlatformDelta([&nativeMomentumEvent, velocity] {
        static constexpr WebCore::FramesPerSecond fallbackMomentumFrameRate { 60 };
        auto momentumFrameRate = WebKit::ScrollingAccelerationCurve::fromNativeWheelEvent(nativeMomentumEvent)
            .or_else([] {
                return WebKit::ScrollingAccelerationCurve::fallbackCurve();
            }).transform([](const auto& curve) {
                return curve.frameRate();
            }).value_or(fallbackMomentumFrameRate);
        auto initialMomentumDelta = velocity / momentumFrameRate;
        return toRawPlatformDelta(initialMomentumDelta);
    }());

    page->handleNativeWheelEvent(nativeMomentumEvent);
    _isMomentumActive = true;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Started momentum scrolling with velocity %.2f pts/s", velocityMagnitude);
}

- (void)interruptMomentumIfNeeded
{
    if (!std::exchange(_isMomentumActive, false))
        return;

    RefPtr page = _page.get();
    if (!page)
        return;

    _fastScrollTracker->didCatchMomentum();
    _caughtDeceleratingScroll = true;
    _suppressNextPanScrollDelta = true;

    page->interruptSyntheticMomentumScrolling();
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(page->logIdentifier(), "Interrupted momentum scrolling");
}

- (void)didEndSyntheticMomentumScrolling
{
    _isMomentumActive = false;
    [self _resetCaughtDeceleratingScroll];
}

- (void)_resetCaughtDeceleratingScroll
{
    _caughtDeceleratingScroll = false;
    _suppressNextPanScrollDelta = false;
}

#pragma mark - Drag Gesture State

- (NSGestureRecognizer *)activeDragGestureRecognizer
{
    if (_textSelectionDragGesture)
        return _textSelectionDragGesture.get();
    if (_dragGestureHasSentMouseDown)
        return _dragPressGestureRecognizer.get();
    return nil;
}

- (void)setTextSelectionDragGesture:(NSGestureRecognizer *)gesture completionHandler:(void (^)(NSDraggingSession *))completionHandler
{
    if (_textSelectionDragGesture) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_ERROR(RefPtr { _page.get() }->logIdentifier(),
            "Replacing prior text-selection drag gesture %@ (completion handler set: %d); prior drag never reached setGestureDraggingSession:",
            gestureLogDescription(_textSelectionDragGesture), !!_textSelectionDragCompletionHandler);
        ASSERT_NOT_REACHED();
    }

    _textSelectionDragGesture = gesture;
    _textSelectionDragCompletionHandler = makeBlockPtr(completionHandler);
}

- (void)setGestureDraggingSession:(NSDraggingSession *)session
{
    _gestureDraggingSession = session;
    if (!_gestureDraggingSession)
        return;

    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG(RefPtr { _page.get() }->logIdentifier(), "Drag session began");
    if (auto handler = std::exchange(_textSelectionDragCompletionHandler, nullptr))
        handler(_gestureDraggingSession);
}

- (void)clearGestureDragState
{
    _gestureDraggingSession = nil;
    _textSelectionDragGesture = nil;
    _textSelectionDragCompletionHandler = nullptr;
    _dragGestureHasSentMouseDown = false;
}

- (void)reset
{
    [self clearGestureDragState];
    [self _handleClickCancelled];
    _mouseTrackingHasSentMouseDown = false;
    _isMomentumActive = false;
    [self _resetCaughtDeceleratingScroll];
    _fastScrollTracker->reset();
    _isSuppressingSingleClickGestureForTextSelection = false;
    _latestClickID.reset();
    _layerTreeTransactionIdAtLastInteractionStart.reset();
    _positionInformationManager->reset();
}

#pragma mark - NSGestureRecognizerDelegate

static BOOL isBuiltInScrollViewPanGestureRecognizer(NSGestureRecognizer *recognizer)
{
    static Class scrollViewPanGestureClass = NSClassFromString(@"NSScrollViewPanGestureRecognizer");
    return [recognizer isKindOfClass:scrollViewPanGestureClass];
}

static inline bool isSamePair(NSGestureRecognizer *a, NSGestureRecognizer *b, NSGestureRecognizer *x, NSGestureRecognizer *y)
{
    return (a == x && b == y) || (b == x && a == y);
}

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer
{
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "Gesture: %@, Other gesture: %@", gestureLogDescription(gestureRecognizer), gestureLogDescription(otherGestureRecognizer));

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _singleClickGestureRecognizer.get(), _panGestureRecognizer.get()))
        return YES;

    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class] || [otherGestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return YES;

    // Live Text (see the Image Analysis design note): the preflight is a passive observer, so it
    // recognizes simultaneously with everything -- it must never block, or be blocked by, the
    // interaction it measures.
    if (gestureRecognizer == _imageAnalysisGestureRecognizer || otherGestureRecognizer == _imageAnalysisGestureRecognizer)
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _mouseTrackingGestureRecognizer.get(), _singleClickGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _dragPressGestureRecognizer.get(), _singleClickGestureRecognizer.get()))
        return YES;

    if (isSamePair(gestureRecognizer, otherGestureRecognizer, _dragPressGestureRecognizer.get(), _mouseTrackingGestureRecognizer.get()))
        return YES;

    if (gestureRecognizer == _singleClickGestureRecognizer
        && isBuiltInScrollViewPanGestureRecognizer(otherGestureRecognizer)
        && [otherGestureRecognizer.view isKindOfClass:NSScrollView.class])
        return YES;

    // Don't prevent the scrollbar from scrolling even if the window resize recognizer is active.
    if (gestureRecognizer == _mouseTrackingGestureRecognizer && isSystemWindowResizeGestureRecognizer(otherGestureRecognizer))
        return YES;

    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return NO;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return NO;

    // Allow the single click or mouse tracking GRs to be simultaneously
    // recognized with any of those from the text selection manager.
    for (NSGestureRecognizer *gestureForFailureRequirements in [[webView textSelectionManager] gesturesForFailureRequirements]) {
        if (isSamePair(gestureRecognizer, otherGestureRecognizer, _singleClickGestureRecognizer.get(), gestureForFailureRequirements))
            return YES;
        if (isSamePair(gestureRecognizer, otherGestureRecognizer, _mouseTrackingGestureRecognizer.get(), gestureForFailureRequirements))
            return YES;
    }

    return NO;
}

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer
{
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "Gesture: %@, Other gesture: %@", gestureLogDescription(gestureRecognizer), gestureLogDescription(otherGestureRecognizer));

    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return NO;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return NO;

    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)gestureRecognizer shouldDeferGestureRecognizer:otherGestureRecognizer];

    // Fail any gestures from the text selection manager if the secondary click GR handles them.
    for (NSGestureRecognizer *gestureForFailureRequirements in [[webView textSelectionManager] gesturesForFailureRequirements]) {
        if (gestureRecognizer == _secondaryClickGestureRecognizer && otherGestureRecognizer == gestureForFailureRequirements)
            return YES;
    }

    return NO;
}

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer
{
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "Gesture: %@, Other gesture: %@", gestureLogDescription(gestureRecognizer), gestureLogDescription(otherGestureRecognizer));

    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return NO;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return NO;

    if (gestureRecognizer == _singleClickGestureRecognizer && otherGestureRecognizer == _doubleClickGestureRecognizer)
        return YES;

    if (gestureRecognizer == _mouseTrackingGestureRecognizer && otherGestureRecognizer == _panGestureRecognizer) {
        bool panCanScroll = [self panGestureRecognizerCanScroll];
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "Mouse tracking requires pan to fail: %d", panCanScroll);
        return panCanScroll;
    }

    return NO;
}

- (BOOL)gestureRecognizerShouldBegin:(NSGestureRecognizer *)gestureRecognizer
{
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "Gesture: %@", gestureLogDescription(gestureRecognizer));

    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return NO;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return NO;

    // While catching a decelerating scroll, only select gestures are allowed to begin:
    // - single click, so it can reset the interruption state
    // - pan, so it can continue with successive scrolls
    if (_caughtDeceleratingScroll) {
        if (gestureRecognizer == _singleClickGestureRecognizer)
            return YES;
        if (gestureRecognizer != _panGestureRecognizer)
            return NO;
    }

    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return YES;

    NSPoint locationInViewCoordinates = [gestureRecognizer locationInView:webView];

    // An event over a scrollbar is a scrollbar interaction; only the mouse-tracking gesture (which drives
    // `Scrollbar::mouseDown` -> thumb drag) should handle it. The AppKit text-selection/context-menu gestures
    // are handled separately in the deferral delegate.
    if ([self _isPointInScrollbar:locationInViewCoordinates] && gestureRecognizer != _mouseTrackingGestureRecognizer) {
        WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "Denying gesture over scrollbar: %@", gestureLogDescription(gestureRecognizer));
        return NO;
    }

    if (gestureRecognizer == _doubleClickGestureRecognizer)
        return viewImpl->allowsMagnification();

    // Live Text (see the Image Analysis design note): the preflight is a passive observer and is only
    // enabled when Live Text is available, so it's always allowed to begin to measure the press.
    if (gestureRecognizer == _imageAnalysisGestureRecognizer)
        return YES;

    if (gestureRecognizer == _secondaryClickGestureRecognizer)
        return [self _secondaryClickShouldBeginAtLocation:locationInViewCoordinates];

    if (gestureRecognizer == _dragPressGestureRecognizer)
        return [self _dragPressShouldBeginAtLocation:locationInViewCoordinates];

    if (gestureRecognizer == _panGestureRecognizer)
        return [self _panShouldBeginAtLocation:locationInViewCoordinates];

    if (gestureRecognizer == _singleClickGestureRecognizer)
        return !viewImpl->isTextSelectedAtPoint(locationInViewCoordinates);

    if (gestureRecognizer == _mouseTrackingGestureRecognizer)
        return !viewImpl->isTextSelectedAtPoint(locationInViewCoordinates);

    return YES;
}

- (BOOL)_isScrollOrZoomGestureRecognizer:(NSGestureRecognizer *)gesture
{
    return gesture == _panGestureRecognizer || isBuiltInScrollViewPanGestureRecognizer(gesture) || [gesture isKindOfClass:[NSMagnificationGestureRecognizer class]];
}

- (BOOL)_gestureRecognizer:(NSGestureRecognizer *)preventingGestureRecognizer canPreventGestureRecognizer:(NSGestureRecognizer *)preventedGestureRecognizer
{
    WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG(RefPtr { _page.get() }->logIdentifier(), "Preventing gesture: %@, Prevented gesture: %@", gestureLogDescription(preventingGestureRecognizer), gestureLogDescription(preventedGestureRecognizer));

    CheckedPtr viewImpl = _viewImpl.get();
    if (!viewImpl)
        return NO;

    RetainPtr webView = viewImpl->view();
    if (!webView)
        return NO;

    // None of our gesture recognizers may prevent an enclosing scroll view's pan (or any other
    // scroll/zoom) gesture, so that a scroll can always be handed off to the enclosing scroll view
    // e.g. a scroll over a draggable <img> in a non-scrollable web view.
    if ([self _isScrollOrZoomGestureRecognizer:preventedGestureRecognizer])
        return NO;

    // Live Text (see the Image Analysis design note): the preflight is a passive observer, so it prevents
    // nothing.
    if (preventingGestureRecognizer == _imageAnalysisGestureRecognizer)
        return NO;

    // Live Text: when the fallback deferral recognizes to block drag / context menu (text found), it must
    // only affect the drag-press and secondary-click recognizers it defers -- never the text-selection
    // gestures, which Live Text needs to proceed. (Its hold over drag-press / secondary-click is enforced
    // by the deferral failure-requirement, not by this prevention hook.)
    if (preventingGestureRecognizer == _imageAnalysisDragAndContextMenuDeferringGestureRecognizer)
        return preventedGestureRecognizer == _dragPressGestureRecognizer || preventedGestureRecognizer == _secondaryClickGestureRecognizer;

    bool isOurClickGesture = preventingGestureRecognizer == _singleClickGestureRecognizer
        || preventingGestureRecognizer == _secondaryClickGestureRecognizer
        || preventingGestureRecognizer == _mouseTrackingGestureRecognizer
        || preventingGestureRecognizer == _dragPressGestureRecognizer;

    if (!isOurClickGesture)
        return YES;

    // Don't let other click gestures prevent the secondary click GR; it must be allowed to fire its
    // press timer (0.72s) without being short-circuited by gestures that recognize earlier
    // (e.g. single click and mouse-tracking, which both transition to Began at mouse-down).
    if (preventedGestureRecognizer == _secondaryClickGestureRecognizer)
        return NO;

    // Don't let our click gestures prevent text selection manager gestures;
    // they should be allowed to recognize simultaneously (per shouldRecognizeSimultaneouslyWithGestureRecognizer:).
    for (NSGestureRecognizer *textSelectionGesture in [[webView textSelectionManager] gesturesForFailureRequirements]) {
        if (preventedGestureRecognizer == textSelectionGesture)
            return NO;
    }

    return YES;
}

@end

#undef WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_ERROR
#undef WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG_DEBUG
#undef WK_APPKIT_GESTURE_CONTROLLER_RELEASE_LOG

#endif // HAVE(APPKIT_GESTURES_SUPPORT)
