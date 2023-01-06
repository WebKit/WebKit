/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#import "WKFullScreenWindowControllerIOS.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(FULLSCREEN_API)

#import "UIKitSPI.h"
#import "VideoFullscreenManagerProxy.h"
#import "WKFullScreenViewController.h"
#import "WKFullscreenStackView.h"
#import "WKWebView.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <Foundation/Foundation.h>
#import <Security/SecCertificate.h>
#import <Security/SecTrust.h>
#import <UIKit/UIVisualEffectView.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/VideoFullscreenInterfaceAVKit.h>
#import <WebCore/VideoFullscreenModel.h>
#import <WebCore/ViewportArguments.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSStringSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/cocoa/URLFormattingSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

#if !HAVE(URL_FORMATTING)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(LinkPresentation)
#endif

#if HAVE(UIKIT_WEBKIT_INTERNALS)
#include <WebKitAdditions/WKFullscreenWindowControllerAdditions.h>
#endif

namespace WebKit {
using namespace WebKit;
using namespace WebCore;

static CGSize sizeExpandedToSize(CGSize initial, CGSize other)
{
    return CGSizeMake(std::max(initial.width, other.width),  std::max(initial.height, other.height));
}

static CGRect safeInlineRect(CGRect initial, CGSize parentSize)
{
    if (initial.origin.y > parentSize.height || initial.origin.y < -initial.size.height || initial.origin.x > parentSize.width || initial.origin.x < -initial.size.width)
        return CGRectMake(parentSize.width / 2, parentSize.height / 2, 1, 1);
    return initial;
}

static void replaceViewWithView(UIView *view, UIView *otherView)
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [otherView setFrame:[view frame]];
    [otherView setAutoresizingMask:[view autoresizingMask]];
    [[view superview] insertSubview:otherView aboveSubview:view];
    [view removeFromSuperview];
    [CATransaction commit];
}

enum FullScreenState : NSInteger {
    NotInFullScreen,
    WaitingToEnterFullScreen,
    EnteringFullScreen,
    InFullScreen,
    WaitingToExitFullScreen,
    ExitingFullScreen,
};

struct WKWebViewState {
    float _savedTopContentInset = 0.0;
    CGFloat _savedPageScale = 1;
    CGFloat _savedViewScale = 1.0;
    CGFloat _savedZoomScale = 1;
    UIEdgeInsets _savedEdgeInset = UIEdgeInsetsZero;
    UIEdgeInsets _savedObscuredInsets = UIEdgeInsetsZero;
    UIEdgeInsets _savedScrollIndicatorInsets = UIEdgeInsetsZero;
    CGPoint _savedContentOffset = CGPointZero;
    CGFloat _savedMinimumZoomScale = 1;
    CGFloat _savedMaximumZoomScale = 1;
    BOOL _savedBouncesZoom = NO;
    BOOL _savedForceAlwaysUserScalable = NO;
    CGFloat _savedMinimumEffectiveDeviceWidth = 0;

    void applyTo(WKWebView* webView)
    {
        [webView _setPageScale:_savedPageScale withOrigin:CGPointMake(0, 0)];
        [webView _setObscuredInsets:_savedObscuredInsets];
        [[webView scrollView] setContentInset:_savedEdgeInset];
        [[webView scrollView] setContentOffset:_savedContentOffset];
        [[webView scrollView] setScrollIndicatorInsets:_savedScrollIndicatorInsets];
        if (auto page = webView._page) {
            page->setTopContentInset(_savedTopContentInset);
            page->setForceAlwaysUserScalable(_savedForceAlwaysUserScalable);
        }
        [webView _setViewScale:_savedViewScale];
        [[webView scrollView] setZoomScale:_savedZoomScale];
        webView.scrollView.minimumZoomScale = _savedMinimumZoomScale;
        webView.scrollView.maximumZoomScale = _savedMaximumZoomScale;
        webView.scrollView.bouncesZoom = _savedBouncesZoom;
        webView._minimumEffectiveDeviceWidth = _savedMinimumEffectiveDeviceWidth;
    }

    void store(WKWebView* webView)
    {
        _savedPageScale = [webView _pageScale];
        _savedObscuredInsets = [webView _obscuredInsets];
        _savedEdgeInset = [[webView scrollView] contentInset];
        _savedContentOffset = [[webView scrollView] contentOffset];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        _savedScrollIndicatorInsets = [[webView scrollView] scrollIndicatorInsets];
ALLOW_DEPRECATED_DECLARATIONS_END
        if (auto page = webView._page) {
            _savedTopContentInset = page->topContentInset();
            _savedForceAlwaysUserScalable = page->forceAlwaysUserScalable();
        }
        _savedViewScale = [webView _viewScale];
        _savedZoomScale = [[webView scrollView] zoomScale];
        _savedMinimumZoomScale = webView.scrollView.minimumZoomScale;
        _savedMaximumZoomScale = webView.scrollView.maximumZoomScale;
        _savedBouncesZoom = webView.scrollView.bouncesZoom;
        _savedMinimumEffectiveDeviceWidth = webView._minimumEffectiveDeviceWidth;
    }
};

} // namespace WebKit

static constexpr NSTimeInterval kAnimationDuration = 0.2;
#if HAVE(UIKIT_WEBKIT_INTERNALS)
static constexpr CGFloat kFullScreenWindowCornerRadius = 12;
#endif

#pragma mark -

@interface WKFullscreenAnimationController : NSObject <UIViewControllerAnimatedTransitioning, UIViewControllerInteractiveTransitioning>
@property (retain, nonatomic) UIViewController* viewController;
@property (nonatomic) CGRect initialFrame;
@property (nonatomic) CGRect finalFrame;
@property (nonatomic, getter=isAnimatingIn) BOOL animatingIn;
@property (readonly, nonatomic) id<UIViewControllerContextTransitioning> context;
@end

@implementation WKFullscreenAnimationController {
    CGRect _initialMaskViewBounds;
    CGRect _finalMaskViewBounds;
    CGAffineTransform _initialAnimatingViewTransform;
    CGAffineTransform _finalAnimatingViewTransform;
    CGPoint _initialMaskViewCenter;
    CGPoint _finalMaskViewCenter;
    RetainPtr<UIView> _maskView;
    RetainPtr<UIView> _animatingView;
    RetainPtr<id<UIViewControllerContextTransitioning>> _context;
    CGFloat _initialBackgroundAlpha;
    CGFloat _finalBackgroundAlpha;
}

- (void)dealloc
{
    [_viewController release];
    [super dealloc];
}

- (id<UIViewControllerContextTransitioning>)context
{
    return _context.get();
}

- (void)_createViewsForTransitionContext:(id<UIViewControllerContextTransitioning>)transitionContext
{
    _maskView = adoptNS([[UIView alloc] init]);
    [_maskView setBackgroundColor:[UIColor blackColor]];
    [_maskView setBounds:_initialMaskViewBounds];
    [_maskView setCenter:_initialMaskViewCenter];
    [_animatingView setMaskView:_maskView.get()];
    [_animatingView setTransform:_initialAnimatingViewTransform];

    UIView *containerView = [transitionContext containerView];
    [containerView addSubview:_animatingView.get()];
}

- (NSTimeInterval)transitionDuration:(id<UIViewControllerContextTransitioning>)transitionContext
{
    return kAnimationDuration;
}

- (void)configureInitialAndFinalStatesForTransition:(id<UIViewControllerContextTransitioning>)transitionContext
{
    _context = transitionContext;
    UIView *fromView = [transitionContext viewForKey:UITransitionContextFromViewKey];
    UIView *toView = [transitionContext viewForKey:UITransitionContextToViewKey];

    CGRect inlineFrame = _animatingIn ? _initialFrame : _finalFrame;
    CGRect fullscreenFrame = _animatingIn ? _finalFrame : _initialFrame;
    _animatingView = _animatingIn ? toView : fromView;

    CGRect boundsRect = WebCore::largestRectWithAspectRatioInsideRect(WebCore::FloatRect(inlineFrame).size().aspectRatio(), fullscreenFrame);
    boundsRect.origin = CGPointZero;

    _initialMaskViewBounds = _animatingIn ? boundsRect : [_animatingView bounds];
    _initialMaskViewCenter = CGPointMake(CGRectGetMidX([_animatingView bounds]), CGRectGetMidY([_animatingView bounds]));
    _finalMaskViewBounds = _animatingIn ? [_animatingView bounds] : boundsRect;
    _finalMaskViewCenter = CGPointMake(CGRectGetMidX([_animatingView bounds]), CGRectGetMidY([_animatingView bounds]));

    WebCore::FloatRect scaleRect = WebCore::smallestRectWithAspectRatioAroundRect(WebCore::FloatRect(fullscreenFrame).size().aspectRatio(), inlineFrame);
    CGAffineTransform scaleTransform = CGAffineTransformMakeScale(scaleRect.width() / fullscreenFrame.size.width, scaleRect.height() / fullscreenFrame.size.height);
    CGAffineTransform translateTransform = CGAffineTransformMakeTranslation(CGRectGetMidX(inlineFrame) - CGRectGetMidX(fullscreenFrame), CGRectGetMidY(inlineFrame) - CGRectGetMidY(fullscreenFrame));

    CGAffineTransform finalTransform = CGAffineTransformConcat(scaleTransform, translateTransform);
    _initialAnimatingViewTransform = _animatingIn ? finalTransform : CGAffineTransformIdentity;
    _finalAnimatingViewTransform = _animatingIn ? CGAffineTransformIdentity : finalTransform;

    _initialBackgroundAlpha = _animatingIn ? 0 : 1;
    _finalBackgroundAlpha = _animatingIn ? 1 : 0;
}

- (void)animateTransition:(id<UIViewControllerContextTransitioning>)transitionContext
{
    [self configureInitialAndFinalStatesForTransition:transitionContext];
    [self _createViewsForTransitionContext:transitionContext];

    UIWindow *window = [transitionContext containerView].window;
    window.backgroundColor = [UIColor colorWithWhite:0 alpha:_initialBackgroundAlpha];

    [UIView animateWithDuration:kAnimationDuration delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        [self updateWithProgress:1];
    } completion:^(BOOL finished) {
        [self animationEnded:![transitionContext transitionWasCancelled]];
        [transitionContext completeTransition:![transitionContext transitionWasCancelled]];
    }];
}

- (void)animationEnded:(BOOL)transitionCompleted
{
    if (([self isAnimatingIn] && !transitionCompleted) || (![self isAnimatingIn] && transitionCompleted))
        [_animatingView removeFromSuperview];

    [_animatingView setMaskView:nil];
    _maskView = nil;
    _animatingView = nil;
}

- (void)startInteractiveTransition:(id <UIViewControllerContextTransitioning>)transitionContext
{
    [self configureInitialAndFinalStatesForTransition:transitionContext];
    [self _createViewsForTransitionContext:transitionContext];
}

- (void)updateWithProgress:(CGFloat)progress
{
    CGAffineTransform progressTransform = _initialAnimatingViewTransform;
    progressTransform.a += progress * (_finalAnimatingViewTransform.a - _initialAnimatingViewTransform.a);
    progressTransform.b += progress * (_finalAnimatingViewTransform.b - _initialAnimatingViewTransform.b);
    progressTransform.c += progress * (_finalAnimatingViewTransform.c - _initialAnimatingViewTransform.c);
    progressTransform.d += progress * (_finalAnimatingViewTransform.d - _initialAnimatingViewTransform.d);
    progressTransform.tx += progress * (_finalAnimatingViewTransform.tx - _initialAnimatingViewTransform.tx);
    progressTransform.ty += progress * (_finalAnimatingViewTransform.ty - _initialAnimatingViewTransform.ty);
    [_animatingView setTransform:progressTransform];

    CGRect progressBounds = _initialMaskViewBounds;
    progressBounds.origin.x += progress * (_finalMaskViewBounds.origin.x - _initialMaskViewBounds.origin.x);
    progressBounds.origin.y += progress * (_finalMaskViewBounds.origin.y - _initialMaskViewBounds.origin.y);
    progressBounds.size.width += progress * (_finalMaskViewBounds.size.width - _initialMaskViewBounds.size.width);
    progressBounds.size.height += progress * (_finalMaskViewBounds.size.height - _initialMaskViewBounds.size.height);
    [_maskView setBounds:progressBounds];

    CGPoint progressCenter = _initialMaskViewCenter;
    progressCenter.x += progress * (_finalMaskViewCenter.x - _finalMaskViewCenter.x);
    progressCenter.y += progress * (_finalMaskViewCenter.y - _finalMaskViewCenter.y);
    [_maskView setCenter:progressCenter];

    UIWindow *window = [_animatingView window];
    window.backgroundColor = [UIColor colorWithWhite:0 alpha:_initialBackgroundAlpha + progress * (_finalBackgroundAlpha - _initialBackgroundAlpha)];
}

- (void)updateWithProgress:(CGFloat)progress scale:(CGFloat)scale translation:(CGSize)translation anchor:(CGPoint)anchor
{
    CGAffineTransform progressTransform = _initialAnimatingViewTransform;
    progressTransform = CGAffineTransformScale(progressTransform, scale, scale);
    progressTransform = CGAffineTransformTranslate(progressTransform, translation.width, translation.height);
    [_animatingView setTransform:progressTransform];

    UIWindow *window = [_animatingView window];
    window.backgroundColor = [UIColor colorWithWhite:0 alpha:_initialBackgroundAlpha + progress * (_finalBackgroundAlpha - _initialBackgroundAlpha)];
}

- (void)updateWithProgress:(CGFloat)progress translation:(CGSize)translation anchor:(CGPoint)anchor
{
    CGAffineTransform progressTransform = _initialAnimatingViewTransform;
    progressTransform.a += progress * (_finalAnimatingViewTransform.a - _initialAnimatingViewTransform.a);
    progressTransform.b += progress * (_finalAnimatingViewTransform.b - _initialAnimatingViewTransform.b);
    progressTransform.c += progress * (_finalAnimatingViewTransform.c - _initialAnimatingViewTransform.c);
    progressTransform.d += progress * (_finalAnimatingViewTransform.d - _initialAnimatingViewTransform.d);
    progressTransform.tx += _initialAnimatingViewTransform.tx + translation.width;
    progressTransform.ty += _initialAnimatingViewTransform.ty + translation.height;
    [_animatingView setTransform:progressTransform];

    UIWindow *window = [_animatingView window];
    window.backgroundColor = [UIColor colorWithWhite:0 alpha:_initialBackgroundAlpha + progress * (_finalBackgroundAlpha - _initialBackgroundAlpha)];
}

- (void)end:(BOOL)cancelled {
    if (cancelled) {
        [UIView animateWithDuration:kAnimationDuration animations:^{
            [self updateWithProgress:0];
        } completion:^(BOOL finished) {
            [_context cancelInteractiveTransition];
            [_context completeTransition:NO];
        }];
    } else {
        [UIView animateWithDuration:kAnimationDuration animations:^{
            [self updateWithProgress:1];
        } completion:^(BOOL finished) {
            [_context finishInteractiveTransition];
            [_context completeTransition:YES];
        }];
    }
}

@end

#pragma mark -

@interface WKFullScreenInteractiveTransition : NSObject<UIViewControllerInteractiveTransitioning>
- (id)initWithAnimator:(WKFullscreenAnimationController *)animator anchor:(CGPoint)point;
@property (nonatomic, readonly) WKFullscreenAnimationController *animator;
@end

@implementation WKFullScreenInteractiveTransition {
    RetainPtr<WKFullscreenAnimationController> _animator;
    RetainPtr<id<UIViewControllerContextTransitioning>> _context;
    CGPoint _anchor;
}

- (id)initWithAnimator:(WKFullscreenAnimationController *)animator anchor:(CGPoint)point
{
    if (!(self = [super init]))
        return nil;

    _animator = animator;
    _anchor = point;
    return self;
}

- (WKFullscreenAnimationController *)animator
{
    return _animator.get();
}

- (BOOL)wantsInteractiveStart
{
    return YES;
}

- (void)startInteractiveTransition:(id <UIViewControllerContextTransitioning>)transitionContext
{
    _context = transitionContext;
    [_animator startInteractiveTransition:transitionContext];
}

- (void)updateInteractiveTransition:(CGFloat)progress withTranslation:(CGSize)translation
{
    [_animator updateWithProgress:progress translation:translation anchor:_anchor];
    [_context updateInteractiveTransition:progress];
}

- (void)updateInteractiveTransition:(CGFloat)progress withScale:(CGFloat)scale andTranslation:(CGSize)translation
{
    [_animator updateWithProgress:progress scale:scale translation:translation anchor:_anchor];
    [_context updateInteractiveTransition:progress];
}

- (void)cancelInteractiveTransition
{
    [_animator end:YES];
}

- (void)finishInteractiveTransition
{
    [_animator end:NO];
}
@end

#pragma mark -

@interface WKFullScreenWindowController () <UIGestureRecognizerDelegate>
@property (weak, nonatomic) WKWebView *_webView; // Cannot be retained, see <rdar://problem/14884666>.
- (void)placeholderWillMoveToSuperview:(UIView *)superview;
@end

#pragma mark -

@interface WKFullScreenPlaceholderView : UIView
@property (weak, nonatomic) WKFullScreenWindowController *parent;
@end

@implementation WKFullScreenPlaceholderView
- (void)willMoveToSuperview:(UIView *)newSuperview
{
    [super viewWillMoveToSuperview:newSuperview];
    [self.parent placeholderWillMoveToSuperview:newSuperview];
}
@end

#pragma mark -

@interface WKFullScreenWindowController (VideoFullscreenManagerProxyClient)
- (void)didEnterPictureInPicture;
- (void)didExitPictureInPicture;
@end

#pragma mark -

@implementation WKFullScreenWindowController {
    RetainPtr<WKFullScreenPlaceholderView> _webViewPlaceholder;

    WebKit::FullScreenState _fullScreenState;
    WebKit::WKWebViewState _viewState;

    RetainPtr<UIWindow> _window;
    RetainPtr<UIViewController> _rootViewController;

    RetainPtr<UIViewController> _viewControllerForPresentation;
    RetainPtr<WKFullScreenViewController> _fullscreenViewController;
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    RetainPtr<UISwipeGestureRecognizer> _startDismissGestureRecognizer;
    RetainPtr<UIPanGestureRecognizer> _interactivePanDismissGestureRecognizer;
    RetainPtr<UIPinchGestureRecognizer> _interactivePinchDismissGestureRecognizer;
    RetainPtr<WKFullScreenInteractiveTransition> _interactiveDismissTransitionCoordinator;
#endif
#if ENABLE(FULLSCREEN_WINDOW_EFFECTS)
    RetainPtr<UIWindow> _lastKnownParentWindow;
    RetainPtr<WKFullScreenParentWindowState> _parentWindowState;
#endif

    std::unique_ptr<WebKit::VideoFullscreenManagerProxy::VideoInPictureInPictureDidChangeObserver> _pipObserver;
    BOOL _shouldReturnToFullscreenFromPictureInPicture;
    BOOL _enterFullscreenNeedsExitPictureInPicture;
    BOOL _returnToFullscreenFromPictureInPicture;
    BOOL _blocksReturnToFullscreenFromPictureInPicture;

    CGRect _initialFrame;
    CGRect _finalFrame;
    CGSize _originalWindowSize;

    RetainPtr<NSString> _EVOrganizationName;
    BOOL _EVOrganizationNameIsValid;
    BOOL _inInteractiveDismiss;
    BOOL _exitRequested;
    BOOL _enterRequested;
    BOOL _exitingFullScreen;

    RetainPtr<id> _notificationListener;
}

#pragma mark -
#pragma mark Initialization
- (id)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    self._webView = webView;

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication]];

    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [super dealloc];
}

#pragma mark -
#pragma mark Accessors

- (BOOL)isFullScreen
{
    return _fullScreenState == WebKit::WaitingToEnterFullScreen
        || _fullScreenState == WebKit::EnteringFullScreen
        || _fullScreenState == WebKit::InFullScreen;
}

- (UIView *)webViewPlaceholder
{
    return _webViewPlaceholder.get();
}

#pragma mark -
#pragma mark External Interface

- (void)setSupportedOrientations:(UIInterfaceOrientationMask)orientations
{
    [_fullscreenViewController setSupportedOrientations:orientations];
}

- (void)resetSupportedOrientations
{
    [_fullscreenViewController resetSupportedOrientations];
}

- (void)enterFullScreen:(CGSize)videoDimensions
{
    if ([self isFullScreen])
        return;

    RetainPtr<WKWebView> webView = self._webView;
    auto page = [webView _page];
    auto* manager = self._manager;
    if (!page || !manager)
        return;

    [self _invalidateEVOrganizationName];

#if ENABLE(FULLSCREEN_WINDOW_EFFECTS)
    _lastKnownParentWindow = [webView window];
    _parentWindowState = adoptNS([[WKFullScreenParentWindowState alloc] initWithWindow:_lastKnownParentWindow.get()]);
#endif
    _fullScreenState = WebKit::WaitingToEnterFullScreen;
    _blocksReturnToFullscreenFromPictureInPicture = manager->blocksReturnToFullscreenFromPictureInPicture();
    _originalWindowSize = [webView window].frame.size;

    _window = adoptNS([[UIWindow alloc] initWithWindowScene:[[webView window] windowScene]]);
    [_window setBackgroundColor:[UIColor clearColor]];
    [_window setWindowLevel:UIWindowLevelNormal - 1];
    [_window setHidden:NO];
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    auto screenSize = page->overrideScreenSize();
    CGFloat preferredWidth = screenSize.width();
    CGFloat preferredHeight = screenSize.height();
    CGFloat preferredAspectRatio = preferredWidth / preferredHeight;
    CGFloat videoAspectRatio = videoDimensions.height ? (videoDimensions.width / videoDimensions.height) : preferredAspectRatio;

    CGFloat targetWidth = preferredWidth;
    CGFloat targetHeight = preferredHeight;
    if (videoDimensions.height && videoDimensions.width) {
        if (videoAspectRatio > preferredAspectRatio)
            targetHeight = videoDimensions.height * preferredWidth / videoDimensions.width;
        else
            targetWidth = videoDimensions.width * preferredHeight / videoDimensions.height;
    }

    [_window setFrame:CGRectMake(0, 0, floorf(targetWidth), floorf(targetHeight))];
    [_window setAlpha:0];
    [_window setClipsToBounds:YES];
    [_window _setContinuousCornerRadius:kFullScreenWindowCornerRadius];
    [_window setNeedsLayout];
    [_window layoutIfNeeded];
#endif

    _rootViewController = adoptNS([[UIViewController alloc] init]);
    _rootViewController.get().view = adoptNS([[UIView alloc] initWithFrame:_window.get().bounds]).get();
    _rootViewController.get().view.backgroundColor = [UIColor clearColor];
    _rootViewController.get().view.autoresizingMask = (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
    [_rootViewController setModalPresentationStyle:UIModalPresentationCustom];
    [_rootViewController setTransitioningDelegate:self];

    _window.get().rootViewController = _rootViewController.get();

    _fullscreenViewController = adoptNS([[WKFullScreenViewController alloc] initWithWebView:webView.get()]);
    [_fullscreenViewController setModalPresentationStyle:UIModalPresentationCustom];
    [_fullscreenViewController setTransitioningDelegate:self];
    [_fullscreenViewController setModalPresentationCapturesStatusBarAppearance:YES];
    [_fullscreenViewController setTarget:self];
    [_fullscreenViewController setExitFullScreenAction:@selector(requestExitFullScreen)];
    _fullscreenViewController.get().view.frame = _rootViewController.get().view.bounds;
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    [_fullscreenViewController hideMediaControls:manager->isVideoElementWithControls()];
#endif
    [self _updateLocationInfo];

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    _startDismissGestureRecognizer = adoptNS([[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(_startToDismissFullscreenChanged:)]);
    [_startDismissGestureRecognizer setDelegate:self];
    [_startDismissGestureRecognizer setCancelsTouchesInView:YES];
    [_startDismissGestureRecognizer setNumberOfTouchesRequired:1];
    [_startDismissGestureRecognizer setDirection:UISwipeGestureRecognizerDirectionDown];
    [_fullscreenViewController.get().view addGestureRecognizer:_startDismissGestureRecognizer.get()];

    _interactivePanDismissGestureRecognizer = adoptNS([[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(_interactiveDismissChanged:)]);
    [_interactivePanDismissGestureRecognizer setDelegate:self];
    [_interactivePanDismissGestureRecognizer setCancelsTouchesInView:NO];
    [_fullscreenViewController.get().view addGestureRecognizer:_interactivePanDismissGestureRecognizer.get()];

    _interactivePinchDismissGestureRecognizer = adoptNS([[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(_interactivePinchDismissChanged:)]);
    [_interactivePinchDismissGestureRecognizer setDelegate:self];
    [_interactivePinchDismissGestureRecognizer setCancelsTouchesInView:NO];
    [_fullscreenViewController.get().view addGestureRecognizer:_interactivePinchDismissGestureRecognizer.get()];
#endif // ENABLE(FULLSCREEN_DISMISSAL_GESTURES)

    manager->saveScrollPosition();

    page->setSuppressVisibilityUpdates(true);

    _viewState.store(webView.get());

    _webViewPlaceholder = adoptNS([[WKFullScreenPlaceholderView alloc] init]);
    [_webViewPlaceholder setParent:self];
    [[_webViewPlaceholder layer] setName:@"Fullscreen Placeholder View"];

    WKSnapshotConfiguration* config = nil;
    [webView takeSnapshotWithConfiguration:config completionHandler:^(UIImage * snapshotImage, NSError * error) {
        RetainPtr<WKWebView> webView = self._webView;
        auto page = [self._webView _page];
        if (!page)
            return;

        [CATransaction begin];
        [CATransaction setDisableActions:YES];

        [[_webViewPlaceholder layer] setContents:(id)[snapshotImage CGImage]];
        WebKit::replaceViewWithView(webView.get(), _webViewPlaceholder.get());

        [webView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
        [webView setFrame:[_window bounds]];
        [webView _overrideLayoutParametersWithMinimumLayoutSize:[_window bounds].size maximumUnobscuredSizeOverride:[_window bounds].size];
        [_window insertSubview:webView.get() atIndex:0];
        [webView setNeedsLayout];
        [webView layoutIfNeeded];

        if (auto* manager = self._manager)
            manager->setAnimatingFullScreen(true);

        WebCore::ViewportArguments arguments { WebCore::ViewportArguments::CSSDeviceAdaptation };
        arguments.zoom = 1;
        arguments.minZoom = 1;
        arguments.maxZoom = 1;
        arguments.userZoom = 1;
        page->setOverrideViewportArguments(arguments);

        page->forceRepaint([protectedSelf = retainPtr(self), self] {
            if (_exitRequested) {
                _exitRequested = NO;
                [self _exitFullscreenImmediately];
                return;
            }

            if (auto* manager = [protectedSelf _manager]) {
                manager->willEnterFullScreen();
                return;
            }

            ASSERT_NOT_REACHED();
            [self _exitFullscreenImmediately];
        });

        [CATransaction commit];
    }];
}

- (void)beganEnterFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WebKit::WaitingToEnterFullScreen)
        return;
    _fullScreenState = WebKit::EnteringFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;

    _initialFrame.size = WebKit::sizeExpandedToSize(_initialFrame.size, CGSizeMake(1, 1));
    _finalFrame.size = WebKit::sizeExpandedToSize(_finalFrame.size, CGSizeMake(1, 1));
    _initialFrame = WebKit::safeInlineRect(_initialFrame, [_rootViewController view].frame.size);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    RetainPtr<WKWebView> webView = self._webView;
    [webView removeFromSuperview];

    [_window setWindowLevel:UIWindowLevelNormal];
    [_window makeKeyAndVisible];
    [_fullscreenViewController setPrefersStatusBarHidden:NO];
    [_fullscreenViewController showUI];

    [CATransaction commit];

#if ENABLE(FULLSCREEN_WINDOW_EFFECTS)
    configureFullscreenTransition(_lastKnownParentWindow.get(), _window.get());
    BOOL shouldAnimateEnterFullscreenTransition = NO;
#else
    // NOTE: In this state, there is already a AVKit fullscreen presentation; we want to
    // animate into position under the AVKit fullscreen, then after that presentation
    // completes, exit AVKit fullscreen.
    BOOL shouldAnimateEnterFullscreenTransition = !_returnToFullscreenFromPictureInPicture;
#endif

    [_rootViewController presentViewController:_fullscreenViewController.get() animated:shouldAnimateEnterFullscreenTransition completion:^{
        _fullScreenState = WebKit::InFullScreen;

        if (_exitRequested) {
            _exitRequested = NO;
            [self _exitFullscreenImmediately];
            return;
        }

        WebKit::WKWebViewState().applyTo(webView.get());
        auto page = [self._webView _page];
        auto* manager = self._manager;

        if (page && manager) {
            [self._webView becomeFirstResponder];
            manager->didEnterFullScreen();
            manager->setAnimatingFullScreen(false);
            page->setSuppressVisibilityUpdates(false);

            [_fullscreenViewController showBanner];

#if ENABLE(FULLSCREEN_WINDOW_EFFECTS)
            CompletionHandler<void()> completionHandler = []() { };
            performFullscreenTransition(_lastKnownParentWindow.get(), _window.get(), _parentWindowState.get(), true, WTFMove(completionHandler));
#endif

            if (auto* videoFullscreenManager = self._videoFullscreenManager) {
                if (!_pipObserver) {
                    _pipObserver = WTF::makeUnique<WebKit::VideoFullscreenManagerProxy::VideoInPictureInPictureDidChangeObserver>([self] (bool inPiP) {
                        if (inPiP)
                            [self didEnterPictureInPicture];
                        else
                            [self didExitPictureInPicture];
                    });
                    videoFullscreenManager->addVideoInPictureInPictureDidChangeObserver(*_pipObserver);
                }
                if (auto* videoFullscreenInterface = videoFullscreenManager ? videoFullscreenManager->controlsManagerInterface() : nullptr) {
                    if (_returnToFullscreenFromPictureInPicture)
                        videoFullscreenInterface->preparedToReturnToStandby();
                    else if (videoFullscreenInterface->inPictureInPicture()) {
                        if (auto* model = videoFullscreenInterface->videoFullscreenModel()) {
                            _enterFullscreenNeedsExitPictureInPicture = YES;
                            model->requestFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone);
                        }
                    }
                }
            }

            _returnToFullscreenFromPictureInPicture = NO;

            return;
        }

        ASSERT_NOT_REACHED();
        [self _exitFullscreenImmediately];
    }];
}

- (void)requestRestoreFullScreen
{
    if (_fullScreenState != WebKit::NotInFullScreen)
        return;

    // Switch the active tab if needed
    if (auto* page = [self._webView _page].get())
        page->fullscreenMayReturnToInline();

    if (auto* manager = self._manager) {
        manager->requestRestoreFullScreen();
        return;
    }

    ASSERT_NOT_REACHED();
}

- (void)requestExitFullScreen
{
    if (_fullScreenState != WebKit::InFullScreen) {
        _exitRequested = YES;
        return;
    }

    if (auto* manager = self._manager) {
        manager->requestExitFullScreen();
        _exitingFullScreen = YES;
        return;
    }

    ASSERT_NOT_REACHED();
    [self _exitFullscreenImmediately];
}

- (void)exitFullScreen
{
    if (_fullScreenState == WebKit::NotInFullScreen)
        return;

    if (_fullScreenState < WebKit::InFullScreen) {
        _exitRequested = YES;
        return;
    }
    _fullScreenState = WebKit::WaitingToExitFullScreen;
    _exitingFullScreen = YES;

    if (auto* manager = self._manager) {
        manager->setAnimatingFullScreen(true);
        manager->willExitFullScreen();
        return;
    }

    ASSERT_NOT_REACHED();
    [self _exitFullscreenImmediately];
}

- (void)beganExitFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WebKit::WaitingToExitFullScreen)
        return;
    _fullScreenState = WebKit::ExitingFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;

    _initialFrame.size = WebKit::sizeExpandedToSize(_initialFrame.size, CGSizeMake(1, 1));
    _finalFrame.size = WebKit::sizeExpandedToSize(_finalFrame.size, CGSizeMake(1, 1));
    _finalFrame = WebKit::safeInlineRect(_finalFrame, [_rootViewController view].frame.size);

    if (auto page = [self._webView _page])
        page->setSuppressVisibilityUpdates(true);

    CompletionHandler<void()> completionHandler = [strongSelf = retainPtr(self), self] () mutable {
        [_fullscreenViewController setPrefersStatusBarHidden:NO];

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
        if (_interactiveDismissTransitionCoordinator) {
            [_interactiveDismissTransitionCoordinator finishInteractiveTransition];
            _interactiveDismissTransitionCoordinator = nil;
            return;
        }
#endif

        [self _dismissFullscreenViewController];
    };

    completionHandler();
}

- (void)_reinsertWebViewUnderPlaceholder
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    RetainPtr<WKWebView> webView = self._webView;
    [[_webViewPlaceholder superview] insertSubview:webView.get() belowSubview:_webViewPlaceholder.get()];
    [webView setFrame:[_webViewPlaceholder frame]];
    [webView setAutoresizingMask:[_webViewPlaceholder autoresizingMask]];

    [[webView window] makeKeyAndVisible];
    [webView becomeFirstResponder];

    _viewState.applyTo(webView.get());
    if (auto page = [webView _page])
        page->setOverrideViewportArguments(std::nullopt);

    [webView setNeedsLayout];
    [webView layoutIfNeeded];

    [CATransaction commit];
}

- (void)_completedExitFullScreen
{
    if (_fullScreenState != WebKit::ExitingFullScreen)
        return;
    _fullScreenState = WebKit::NotInFullScreen;

    [self _reinsertWebViewUnderPlaceholder];

    if (auto* manager = self._manager) {
        manager->restoreScrollPosition();
        manager->setAnimatingFullScreen(false);
        manager->didExitFullScreen();
    }

    auto* videoFullscreenInterface = self._videoFullscreenManager ? self._videoFullscreenManager->controlsManagerInterface() : nullptr;
    _shouldReturnToFullscreenFromPictureInPicture = videoFullscreenInterface && videoFullscreenInterface->inPictureInPicture();

    [_window setHidden:YES];
    _window = nil;

#if ENABLE(FULLSCREEN_WINDOW_EFFECTS)
    _lastKnownParentWindow = nil;
    _parentWindowState = nil;
#endif

    CompletionHandler<void()> completionHandler([protectedSelf = retainPtr(self), self] {
        _webViewPlaceholder.get().parent = nil;
        [_webViewPlaceholder removeFromSuperview];

        if (auto page = [self._webView _page]) {
            page->setSuppressVisibilityUpdates(false);
            page->setNeedsDOMWindowResizeEvent();
        }

        _exitRequested = NO;
        _exitingFullScreen = NO;
        if (_enterRequested) {
            _enterRequested = NO;
            [self requestRestoreFullScreen];
        }
    });

    auto* page = [self._webView _page].get();
    if (page && page->isViewFocused())
        page->forceRepaint(WTFMove(completionHandler));
    else
        completionHandler();

    [_fullscreenViewController setPrefersStatusBarHidden:YES];
    [_fullscreenViewController invalidate];
    _fullscreenViewController = nil;
}

- (void)close
{
    [self _exitFullscreenImmediately];
    self._webView = nil;
}

- (void)webViewDidRemoveFromSuperviewWhileInFullscreen
{
    if (_fullScreenState == WebKit::InFullScreen && self._webView.window != _window.get())
        [self _exitFullscreenImmediately];
}

- (void)videoControlsManagerDidChange
{
    if (_fullscreenViewController)
        [_fullscreenViewController videoControlsManagerDidChange];
}

- (void)placeholderWillMoveToSuperview:(UIView *)superview
{
    if (superview)
        return;

    RunLoop::main().dispatch([self, strongSelf = retainPtr(self)] {
        if ([_webViewPlaceholder superview] == nil && [_webViewPlaceholder parent] == self)
            [self close];
    });
}

- (void)didEnterPictureInPicture
{
    _shouldReturnToFullscreenFromPictureInPicture = !_blocksReturnToFullscreenFromPictureInPicture;

    if (_fullScreenState == WebKit::InFullScreen)
        [self requestExitFullScreen];
}

- (void)didExitPictureInPicture
{
    if (!_enterFullscreenNeedsExitPictureInPicture && _shouldReturnToFullscreenFromPictureInPicture) {
        auto* videoFullscreenInterface = self._videoFullscreenManager ? self._videoFullscreenManager->controlsManagerInterface() : nullptr;
        if (videoFullscreenInterface && videoFullscreenInterface->returningToStandby()) {
            if (!_exitingFullScreen) {
                if (_fullScreenState == WebKit::InFullScreen)
                    videoFullscreenInterface->preparedToReturnToStandby();
                else
                    [self requestRestoreFullScreen];
            } else
                _enterRequested = YES;

            _returnToFullscreenFromPictureInPicture = YES;
            return;
        }
    }

    _enterFullscreenNeedsExitPictureInPicture = NO;
}

#pragma mark -
#pragma mark UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

#pragma mark -
#pragma mark UIViewControllerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)animationControllerForPresentedController:(UIViewController *)presented presentingController:(UIViewController *)presenting sourceController:(UIViewController *)source
{
    RetainPtr<WKFullscreenAnimationController> animationController = adoptNS([[WKFullscreenAnimationController alloc] init]);
    [animationController setViewController:presented];
    [animationController setInitialFrame:_initialFrame];
    [animationController setFinalFrame:_finalFrame];
    [animationController setAnimatingIn:YES];
    return animationController.autorelease();
}

- (id<UIViewControllerAnimatedTransitioning>)animationControllerForDismissedController:(UIViewController *)dismissed
{
    CGRect initialFrame = _initialFrame;
    CGRect finalFrame = _finalFrame;

    // Because we're not calling "requestExitFullscreen()" at the beginning of an interactive animation,
    // the _initialFrame and _finalFrame values are left over from when we entered fullscreen.
    if (_inInteractiveDismiss)
        std::swap(initialFrame, finalFrame);

    RetainPtr<WKFullscreenAnimationController> animationController = adoptNS([[WKFullscreenAnimationController alloc] init]);
    [animationController setViewController:dismissed];
    [animationController setInitialFrame:initialFrame];
    [animationController setFinalFrame:finalFrame];
    [animationController setAnimatingIn:NO];
    return animationController.autorelease();
}

- (id<UIViewControllerInteractiveTransitioning>)interactionControllerForDismissal:(id<UIViewControllerAnimatedTransitioning>)animator
{
    if (!_inInteractiveDismiss)
        return nil;

    if (![animator isKindOfClass:[WKFullscreenAnimationController class]])
        return nil;

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    if (!_interactiveDismissTransitionCoordinator) {
        CGPoint anchor = CGPointZero;
        if (_interactivePanDismissGestureRecognizer.get().state == UIGestureRecognizerStateBegan)
            anchor = [_interactivePanDismissGestureRecognizer locationInView:_fullscreenViewController.get().view];
        else if (_interactivePinchDismissGestureRecognizer.get().state == UIGestureRecognizerStateBegan)
            anchor = [_interactivePinchDismissGestureRecognizer locationInView:_fullscreenViewController.get().view];
        _interactiveDismissTransitionCoordinator = adoptNS([[WKFullScreenInteractiveTransition alloc] initWithAnimator:(WKFullscreenAnimationController *)animator anchor:anchor]);
    }

    return _interactiveDismissTransitionCoordinator.get();
#else
    return nil;
#endif
}

#pragma mark -
#pragma mark Internal Interface

- (void)_exitFullscreenImmediately
{
    if (_fullScreenState == WebKit::NotInFullScreen)
        return;

    _shouldReturnToFullscreenFromPictureInPicture = false;
    _exitRequested = NO;
    _exitingFullScreen = NO;
    _fullScreenState = WebKit::NotInFullScreen;
    _shouldReturnToFullscreenFromPictureInPicture = false;

    auto* page = [self._webView _page].get();
    if (page)
        page->setSuppressVisibilityUpdates(true);

    [self _reinsertWebViewUnderPlaceholder];

    if (auto* manager = self._manager) {
        manager->requestExitFullScreen();
        manager->setAnimatingFullScreen(true);
        manager->willExitFullScreen();
        manager->restoreScrollPosition();
        manager->setAnimatingFullScreen(false);
        manager->didExitFullScreen();
    }

    [_webViewPlaceholder removeFromSuperview];
    _webViewPlaceholder = nil;
    [_window setHidden:YES];
    _window = nil;

    if (page) {
        page->setSuppressVisibilityUpdates(false);
        page->setNeedsDOMWindowResizeEvent();
    }

    [_fullscreenViewController setPrefersStatusBarHidden:YES];
    [_fullscreenViewController invalidate];
    _fullscreenViewController = nil;
}

- (void)_invalidateEVOrganizationName
{
    _EVOrganizationName = nil;
    _EVOrganizationNameIsValid = NO;
}

- (BOOL)_isSecure
{
    return self._webView.hasOnlySecureContent;
}

- (SecTrustRef)_serverTrust
{
    return self._webView.serverTrust;
}

- (NSString *)_EVOrganizationName
{
    if (!self._isSecure)
        return nil;

    if (_EVOrganizationNameIsValid)
        return _EVOrganizationName.get();

    ASSERT(!_EVOrganizationName.get());
    _EVOrganizationNameIsValid = YES;

    SecTrustRef trust = [self _serverTrust];
    if (!trust)
        return nil;

    auto infoDictionary = adoptCF(SecTrustCopyInfo(trust));
    // If SecTrustCopyInfo returned NULL then it's likely that the SecTrustRef has not been evaluated
    // and the only way to get the information we need is to call SecTrustEvaluate ourselves.
    if (!infoDictionary) {
        if (!SecTrustEvaluateWithError(trust, nullptr))
            return nil;
        infoDictionary = adoptCF(SecTrustCopyInfo(trust));
        if (!infoDictionary)
            return nil;
    }

    // Make sure that the EV certificate is valid against our certificate chain.
    id hasEV = [(__bridge NSDictionary *)infoDictionary.get() objectForKey:(__bridge NSString *)kSecTrustInfoExtendedValidationKey];
    if (![hasEV isKindOfClass:[NSValue class]] || ![hasEV boolValue])
        return nil;

    // Make sure that we could contact revocation server and it is still valid.
    id isNotRevoked = [(__bridge NSDictionary *)infoDictionary.get() objectForKey:(__bridge NSString *)kSecTrustInfoRevocationKey];
    if (![isNotRevoked isKindOfClass:[NSValue class]] || ![isNotRevoked boolValue])
        return nil;

    _EVOrganizationName = [(__bridge NSDictionary *)infoDictionary.get() objectForKey:(__bridge NSString *)kSecTrustInfoCompanyNameKey];
    return _EVOrganizationName.get();
}

- (void)_updateLocationInfo
{
    NSURL* url = self._webView._committedURL;

    NSString *EVOrganizationName = [self _EVOrganizationName];
    BOOL showsEVOrganizationName = [EVOrganizationName length] > 0;

    NSString *domain = nil;

#if HAVE(URL_FORMATTING)
    domain = [url _lp_simplifiedDisplayString];
#else
    if (LinkPresentationLibrary())
        domain = [url _lp_simplifiedDisplayString];
    else
        domain = WTF::userVisibleString(url);
#endif

    NSString *text = nil;
    if ([[url scheme] caseInsensitiveCompare:@"data"] == NSOrderedSame)
        text = @"data:";
    else if (showsEVOrganizationName)
        text = EVOrganizationName;
    else
        text = domain;

    [_fullscreenViewController setLocation:text];
}

- (WebKit::WebFullScreenManagerProxy*)_manager
{
    if (auto page = [self._webView _page])
        return page->fullScreenManager();
    return nullptr;
}

- (WebKit::VideoFullscreenManagerProxy*)_videoFullscreenManager
{
    if (auto page = [self._webView _page])
        return page->videoFullscreenManager();
    return nullptr;
}

- (void)_startToDismissFullscreenChanged:(id)sender
{
    if (_inInteractiveDismiss)
        return;
    _inInteractiveDismiss = true;
    [self _dismissFullscreenViewController];
}

- (void)_dismissFullscreenViewController
{
    if (!_fullscreenViewController) {
        [self _completedExitFullScreen];
        return;
    }

#if ENABLE(FULLSCREEN_WINDOW_EFFECTS)

    configureFullscreenTransition(_lastKnownParentWindow.get(), _window.get());

    CompletionHandler<void()> completionHandler = [strongSelf = retainPtr(self), self] () {
        [self _completedExitFullScreen];
    };

    performFullscreenTransition(_lastKnownParentWindow.get(), _window.get(), _parentWindowState.get(), false, WTFMove(completionHandler));

#else

    [_fullscreenViewController setAnimating:YES];
    [_fullscreenViewController dismissViewControllerAnimated:YES completion:^{
        if (![self._webView _page])
            return;

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
        if (_interactiveDismissTransitionCoordinator.get().animator.context.transitionWasCancelled)
            [_fullscreenViewController setAnimating:NO];
        else
            [self _completedExitFullScreen];

        _interactiveDismissTransitionCoordinator = nil;
#else
        [self _completedExitFullScreen];
#endif
    }];

#endif // ENABLE(FULLSCREEN_WINDOW_EFFECTS)
}

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
- (void)_interactiveDismissChanged:(id)sender
{
    if (!_inInteractiveDismiss)
        return;

    auto pinchState = [_interactivePinchDismissGestureRecognizer state];
    if (pinchState > UIGestureRecognizerStatePossible && pinchState <= UIGestureRecognizerStateEnded)
        return;

    CGPoint translation = [_interactivePanDismissGestureRecognizer translationInView:_fullscreenViewController.get().view];
    CGPoint velocity = [_interactivePanDismissGestureRecognizer velocityInView:_fullscreenViewController.get().view];
    CGFloat progress = translation.y / (_fullscreenViewController.get().view.bounds.size.height / 2);
    progress = std::min(1., std::max(0., progress));

    if (_interactivePanDismissGestureRecognizer.get().state == UIGestureRecognizerStateEnded) {
        _inInteractiveDismiss = false;

        if (progress > 0.25 || (progress > 0 && velocity.y > 5))
            [self requestExitFullScreen];
        else
            [_interactiveDismissTransitionCoordinator cancelInteractiveTransition];
        return;
    }

    [_interactiveDismissTransitionCoordinator updateInteractiveTransition:progress withTranslation:CGSizeMake(translation.x, translation.y)];
}

- (void)_interactivePinchDismissChanged:(id)sender
{
    CGFloat scale = [_interactivePinchDismissGestureRecognizer scale];
    CGFloat velocity = [_interactivePinchDismissGestureRecognizer velocity];
    CGFloat progress = std::min(1., std::max(0., 1 - scale));

    CGPoint translation = CGPointZero;
    auto panState = [_interactivePanDismissGestureRecognizer state];
    if (panState > UIGestureRecognizerStatePossible && panState <= UIGestureRecognizerStateEnded)
        translation = [_interactivePanDismissGestureRecognizer translationInView:_fullscreenViewController.get().view];

    if (_interactivePinchDismissGestureRecognizer.get().state == UIGestureRecognizerStateEnded) {
        _inInteractiveDismiss = false;
        if ((progress > 0.05 && velocity < 0.) || velocity < -2.5)
            [self requestExitFullScreen];
        else
            [_interactiveDismissTransitionCoordinator cancelInteractiveTransition];
        return;
    }

    [_interactiveDismissTransitionCoordinator updateInteractiveTransition:progress withScale:scale andTranslation:CGSizeMake(translation.x, translation.y)];
}
#endif // ENABLE(FULLSCREEN_DISMISSAL_GESTURES)

- (void)_applicationDidBecomeActive:(NSNotification*)notification
{
    [_fullscreenViewController showBanner];
}

@end


#endif // PLATFORM(IOS_FAMILY) && ENABLE(FULLSCREEN_API)
