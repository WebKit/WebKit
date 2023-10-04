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
#import <objc/runtime.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSStringSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/cocoa/URLFormattingSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

#if PLATFORM(VISION)
#import "MRUIKitSPI.h"
#endif

#if !HAVE(URL_FORMATTING)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(LinkPresentation)
#endif

namespace WebKit {
using namespace WebKit;
using namespace WebCore;

static constexpr float ZoomForFullscreenWindow = 1.0;
#if PLATFORM(VISION)
static constexpr float ZoomForVisionFullscreenVideoWindow = 1.36;
#endif

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

#if PLATFORM(VISION)

static bool useSpatialFullScreenTransition()
{
    return [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomVision;
}

static void resizeScene(UIWindowScene *scene, CGSize size, CompletionHandler<void()>&& completionHandler)
{
    if (size.width) {
        CGSize minimumSize = scene.sizeRestrictions.minimumSize;
        minimumSize.height = floorf(minimumSize.width * size.height / size.width);

        if (size.width >= minimumSize.width && size.height >= minimumSize.height)
            scene.sizeRestrictions.minimumSize = minimumSize;
        else
            scene.sizeRestrictions.minimumSize = size;
    } else
        scene.sizeRestrictions.minimumSize = size;

    [UIView animateWithDuration:0 animations:^{
        [scene mrui_requestResizeToSize:size options:nil completion:makeBlockPtr([completionHandler = WTFMove(completionHandler)](CGSize sizeReceived, NSError *error) mutable {
            completionHandler();
        }).get()];
    } completion:nil];
}

#endif

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

namespace WTF {
template<typename>
struct LogArgument;

template<>
struct LogArgument<WebKit::FullScreenState> {
    static String toString(WebKit::FullScreenState state)
    {
        switch (state) {
        case WebKit::NotInFullScreen: return "NotInFullScreen"_s;
        case WebKit::WaitingToEnterFullScreen: return "WaitingToEnterFullScreen"_s;
        case WebKit::EnteringFullScreen: return "EnteringFullScreen"_s;
        case WebKit::InFullScreen: return "InFullScreen"_s;
        case WebKit::WaitingToExitFullScreen: return "WaitingToExitFullScreen"_s;
        case WebKit::ExitingFullScreen: return "ExitingFullScreen"_s;
        }
    }
};
} // namespace WTF

static constexpr NSTimeInterval kAnimationDuration = 0.2;
#if PLATFORM(VISION)
static constexpr CGFloat kFullScreenWindowCornerRadius = 12;
static constexpr CGFloat kDarknessAnimationDuration = 0.6;
static constexpr CGFloat kOutgoingWindowFadeDuration = 0.4;
static constexpr CGFloat kOutgoingWindowZOffset = -150;
static constexpr CGFloat kIncomingWindowFadeDuration = 0.6;
static constexpr CGFloat kIncomingWindowFadeDelay = 0.2;
static constexpr CGFloat kIncomingWindowZOffset = -170;
static constexpr CGFloat kWindowTranslationDuration = 0.6;
static constexpr NSString *kPrefersFullScreenDimmingKey = @"WebKitPrefersFullScreenDimming";
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

#if PLATFORM(VISION)

@interface WKFixedSizeWindow : UIWindow
@end

@implementation WKFixedSizeWindow

- (BOOL)_shouldResizeWithScene
{
    return NO;
}

@end

@interface WKMRUIPlatterOrnamentProperties : NSObject

@property (nonatomic, readonly) CGFloat depthDisplacement;
@property (nonatomic, readonly) CGFloat windowAlpha;
@property (nonatomic, readonly) CGPoint offset2D;

- (instancetype)initWithOrnament:(MRUIPlatterOrnament *)ornament;

@end

@implementation WKMRUIPlatterOrnamentProperties

- (instancetype)initWithOrnament:(MRUIPlatterOrnament *)ornament
{
    if (!(self = [super init]))
        return nil;

    _depthDisplacement = ornament._depthDisplacement;
    _windowAlpha = ornament.viewController.view.window.alpha;
    _offset2D = ornament.offset2D;

    return self;
}

@end

@interface WKFullScreenParentWindowState : NSObject

@property (nonatomic, readonly) CATransform3D transform3D;
@property (nonatomic, readonly) Class windowClass;
@property (nonatomic, readonly) CGSize sceneSize;
@property (nonatomic, readonly) CGSize sceneMinimumSize;
@property (nonatomic, readonly) RSSSceneChromeOptions sceneChromeOptions;
@property (nonatomic, readonly) MRUISceneResizingBehavior sceneResizingBehavior;
@property (nonatomic, readonly) MRUIDarknessPreference preferredDarkness;

@property (nonatomic, readonly) NSMapTable<MRUIPlatterOrnament *, WKMRUIPlatterOrnamentProperties *> *ornamentProperties;

- (id)initWithWindow:(UIWindow *)window;

@end

@implementation WKFullScreenParentWindowState {
    RetainPtr<NSMapTable<MRUIPlatterOrnament *, WKMRUIPlatterOrnamentProperties *>> _ornamentProperties;
}

- (id)initWithWindow:(UIWindow *)window
{
    if (!(self = [super init]))
        return nil;

    _transform3D = window.transform3D;
    _windowClass = object_getClass(window);
    _preferredDarkness = UIApplication.sharedApplication.mrui_activeStage.preferredDarkness;

    UIWindowScene *windowScene = window.windowScene;
    _sceneSize = windowScene.coordinateSpace.bounds.size;
    _sceneMinimumSize = windowScene.sizeRestrictions.minimumSize;
    _sceneChromeOptions = windowScene.mrui_placement.preferredChromeOptions;
    _sceneResizingBehavior = windowScene.mrui_placement.preferredResizingBehavior;

    _ornamentProperties = [NSMapTable weakToStrongObjectsMapTable];

    MRUIPlatterOrnamentManager *ornamentManager = windowScene._mrui_platterOrnamentManager;
    for (MRUIPlatterOrnament *ornament in ornamentManager.ornaments) {
        auto properties = adoptNS([[WKMRUIPlatterOrnamentProperties alloc] initWithOrnament:ornament]);
        [_ornamentProperties setObject:properties.get() forKey:ornament];
    }

    return self;
}

- (NSMapTable<MRUIPlatterOrnament *, WKMRUIPlatterOrnamentProperties *> *)ornamentProperties
{
    return _ornamentProperties.get();
}

@end

@interface WKFullscreenWindowSceneDelegate : NSObject <MRUIWindowSceneDelegate>
- (instancetype)initWithController:(WKFullScreenWindowController *)controller originalDelegate:(id<UISceneDelegate>)originalDelegate;
- (id<UISceneDelegate>)originalDelegate;
@end

@implementation WKFullscreenWindowSceneDelegate {
    RetainPtr<WKFullScreenWindowController> _controller;
    RetainPtr<id<UISceneDelegate>> _originalDelegate;
}

- (instancetype)initWithController:(WKFullScreenWindowController *)controller originalDelegate:(id<UISceneDelegate>)originalDelegate
{
    if (!(self = [super init]))
        return nil;

    _controller = controller;
    _originalDelegate = originalDelegate;

    return self;
}

- (id<UISceneDelegate>)originalDelegate
{
    return _originalDelegate.get();
}

- (BOOL)windowScene:(UIWindowScene *)windowScene shouldCloseForReason:(MRUICloseWindowSceneReason)reason
{
    [_controller requestExitFullScreen];
    return NO;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    NSMethodSignature *signature = [super methodSignatureForSelector:aSelector];
    if (!signature)
        signature = [(NSObject *)_originalDelegate.get() methodSignatureForSelector:aSelector];
    return signature;
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    return [super respondsToSelector:aSelector] || [_originalDelegate respondsToSelector:aSelector];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation
{
    if ([_originalDelegate respondsToSelector:[anInvocation selector]]) {
        [anInvocation invokeWithTarget:_originalDelegate.get()];
        return;
    }

    [super forwardInvocation:anInvocation];
}

- (id)forwardingTargetForSelector:(SEL)aSelector
{
    if ([_originalDelegate respondsToSelector:aSelector])
        return _originalDelegate.get();
    return nil;
}

@end

#endif // PLATFORM(VISION)

#pragma mark -

@interface WKFullScreenWindowController () <UIGestureRecognizerDelegate, WKFullScreenViewControllerDelegate>
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
    [super willMoveToSuperview:newSuperview];
    [self.parent placeholderWillMoveToSuperview:newSuperview];
}
@end

#pragma mark -

@interface WKFullScreenWindowController (VideoFullscreenManagerProxyClient)
- (void)didEnterPictureInPicture;
- (void)didExitPictureInPicture;
@end

#pragma mark -

#if !RELEASE_LOG_DISABLED
@interface WKFullScreenWindowController (Logging)
@property (readonly, nonatomic) const void* logIdentifier;
@property (readonly, nonatomic) const Logger* loggerPtr;
@property (readonly, nonatomic) WTFLogChannel* logChannel;
@end
#endif

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
#if PLATFORM(VISION)
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
#if !RELEASE_LOG_DISABLED
    RefPtr<Logger> _logger;
    const void* _logIdentifier;
#endif
}

#pragma mark -
#pragma mark Initialization
- (id)initWithWebView:(WKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    self._webView = webView;
#if !RELEASE_LOG_DISABLED
    if (auto webPage = RefPtr { [webView _page].get() }) {
        _logger = &webPage->logger();
        _logIdentifier = webPage->logIdentifier();
    }
#endif

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

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, WebCore::FloatSize { videoDimensions });

    [self _invalidateEVOrganizationName];

#if PLATFORM(VISION)
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
#if PLATFORM(VISION)
    if (WebKit::useSpatialFullScreenTransition()) {
        auto screenSize = page->overrideScreenSize();
        CGFloat preferredWidth = screenSize.width();
        CGFloat preferredHeight = screenSize.height();

        CGFloat targetWidth = preferredWidth;
        CGFloat targetHeight = preferredHeight;
        if (videoDimensions.height && videoDimensions.width) {
            CGFloat preferredAspectRatio = preferredWidth / preferredHeight;
            CGFloat videoAspectRatio = videoDimensions.height ? (videoDimensions.width / videoDimensions.height) : preferredAspectRatio;
            if (videoAspectRatio > preferredAspectRatio)
                targetHeight = videoDimensions.height * preferredWidth / videoDimensions.width;
            else
                targetWidth = videoDimensions.width * preferredHeight / videoDimensions.height;
        }

        [_window setFrame:CGRectMake(0, 0, floorf(targetWidth), floorf(targetHeight))];
        [_window setAlpha:0];
        [_window setClipsToBounds:YES];
        [_window _setContinuousCornerRadius:kFullScreenWindowCornerRadius];
        [_window _setPreferredGroundingShadowVisibility:_UIPlatterGroundingShadowVisibilityVisible];
        [_window setNeedsLayout];
        [_window layoutIfNeeded];
    }
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
    [_fullscreenViewController setDelegate:self];
    _fullscreenViewController.get().view.frame = _rootViewController.get().view.bounds;
#if PLATFORM(VISION)
    [_fullscreenViewController hideCustomControls:manager->isVideoElement()];
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
    __block auto logIdentifier = OBJC_LOGIDENTIFIER;
    [webView takeSnapshotWithConfiguration:config completionHandler:^(UIImage * snapshotImage, NSError * error) {
        RetainPtr<WKWebView> webView = self._webView;
        auto page = [self._webView _page];
        if (!page)
            return;

        OBJC_ALWAYS_LOG(logIdentifier, "snapshot completed");
        [CATransaction begin];
        [CATransaction setDisableActions:YES];

        [[_webViewPlaceholder layer] setContents:(id)[snapshotImage CGImage]];
        WebKit::replaceViewWithView(webView.get(), _webViewPlaceholder.get());

        [webView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
        [webView setFrame:[_window bounds]];
        [webView _setMinimumEffectiveDeviceWidth:0];
        [webView _setViewScale:1.f];
        [webView _overrideLayoutParametersWithMinimumLayoutSize:[_window bounds].size maximumUnobscuredSizeOverride:[_window bounds].size];
        [_window insertSubview:webView.get() atIndex:0];
        [webView setNeedsLayout];
        [webView layoutIfNeeded];

        if (auto* manager = self._manager)
            manager->setAnimatingFullScreen(true);

        WebCore::ViewportArguments arguments { WebCore::ViewportArguments::Type::CSSDeviceAdaptation };
        arguments.zoom = WebKit::ZoomForFullscreenWindow;
        arguments.minZoom = WebKit::ZoomForFullscreenWindow;
        arguments.maxZoom = WebKit::ZoomForFullscreenWindow;
        arguments.userZoom = WebKit::ZoomForFullscreenWindow;
#if PLATFORM(VISION)
        if (manager->isVideoElement()) {
            arguments.zoom = WebKit::ZoomForVisionFullscreenVideoWindow;
            arguments.minZoom = WebKit::ZoomForVisionFullscreenVideoWindow;
            arguments.maxZoom = WebKit::ZoomForVisionFullscreenVideoWindow;
            arguments.userZoom = WebKit::ZoomForVisionFullscreenVideoWindow;
        }
#endif
        page->setOverrideViewportArguments(arguments);

        page->forceRepaint([protectedSelf = retainPtr(self), self, logIdentifier = logIdentifier] {
            if (_exitRequested) {
                _exitRequested = NO;
                OBJC_ERROR_LOG(logIdentifier, "repaint completed, but exit requested");
                [self _exitFullscreenImmediately];
                return;
            }

            if (![protectedSelf _manager]) {
                ASSERT_NOT_REACHED();
                OBJC_ERROR_LOG(logIdentifier, "repaint completed, but manager missing");
                [self _exitFullscreenImmediately];
                return;
            }

            [self._webView _doAfterNextVisibleContentRectAndPresentationUpdate:makeBlockPtr([self, protectedSelf, logIdentifier] {
                if (auto* manager = [protectedSelf _manager]) {
                    OBJC_ALWAYS_LOG(logIdentifier, "presentation updated");
                    manager->willEnterFullScreen();
                    return;
                }

                ASSERT_NOT_REACHED();
                OBJC_ERROR_LOG(logIdentifier, "presentation updated, but manager missing");
                [protectedSelf _exitFullscreenImmediately];
            }).get()];
        });

        [CATransaction commit];
    }];
}

- (void)beganEnterFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WebKit::WaitingToEnterFullScreen) {
        OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != WaitingToEnterFullScreen, dropping");
        return;
    }
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, WebCore::FloatRect { initialFrame }, ", ", WebCore::FloatRect { finalFrame });
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

    // NOTE: In this state, there is already a AVKit fullscreen presentation; we want to
    // animate into position under the AVKit fullscreen, then after that presentation
    // completes, exit AVKit fullscreen.
    BOOL shouldAnimateEnterFullscreenTransition = !_returnToFullscreenFromPictureInPicture;
#if PLATFORM(VISION)
    if (WebKit::useSpatialFullScreenTransition()) {
        [self _configureSpatialFullScreenTransition];
        shouldAnimateEnterFullscreenTransition = NO;
    }
#endif

    __block auto logIdentifier = OBJC_LOGIDENTIFIER;
    [_rootViewController presentViewController:_fullscreenViewController.get() animated:shouldAnimateEnterFullscreenTransition completion:^{
        _fullScreenState = WebKit::InFullScreen;

        if (_exitRequested) {
            _exitRequested = NO;
            OBJC_ERROR_LOG(logIdentifier, "presentation completed, but exit requested");
            [self _exitFullscreenImmediately];
            return;
        }

        WebKit::WKWebViewState().applyTo(webView.get());
        auto page = [self._webView _page];
        auto* manager = self._manager;

        if (page && manager) {
            OBJC_ALWAYS_LOG(logIdentifier, "presentation completed");

            [self._webView becomeFirstResponder];
            manager->didEnterFullScreen();
            manager->setAnimatingFullScreen(false);
            page->setSuppressVisibilityUpdates(false);

            [_fullscreenViewController showBanner];

#if PLATFORM(VISION)
            if (WebKit::useSpatialFullScreenTransition()) {
                CompletionHandler<void()> completionHandler = []() { };
                [self _performSpatialFullScreenTransition:YES completionHandler:WTFMove(completionHandler)];
            }
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
                        if (auto model = videoFullscreenInterface->videoFullscreenModel()) {
                            _enterFullscreenNeedsExitPictureInPicture = YES;
                            model->requestFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone);
                        }
                    }
                }
            }

            _returnToFullscreenFromPictureInPicture = NO;

            return;
        }

        OBJC_ERROR_LOG(logIdentifier, "presentation completed, but page or manager missing");
        ASSERT_NOT_REACHED();
        [self _exitFullscreenImmediately];
    }];
}

- (void)requestRestoreFullScreen
{
    if (_fullScreenState != WebKit::NotInFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != NotInFullScreen, dropping");
        return;
    }

    // Switch the active tab if needed
    if (auto* page = [self._webView _page].get())
        page->fullscreenMayReturnToInline();

    if (auto* manager = self._manager) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
        manager->requestRestoreFullScreen();
        return;
    }

    OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "manager missing, dropping");
    ASSERT_NOT_REACHED();
}

- (void)requestExitFullScreen
{
    if (_fullScreenState != WebKit::InFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState,  " != InFullScreen");
        _exitRequested = YES;
        return;
    }

    if (auto* manager = self._manager) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
        manager->requestExitFullScreen();
        _exitingFullScreen = YES;
        return;
    }

    OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "manager missing");
    ASSERT_NOT_REACHED();
    [self _exitFullscreenImmediately];
}

- (void)exitFullScreen
{
    if (_fullScreenState == WebKit::NotInFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, ", dropping");
        return;
    }

    if (_fullScreenState < WebKit::InFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " < InFullScreen");
        _exitRequested = YES;
        return;
    }
    _fullScreenState = WebKit::WaitingToExitFullScreen;
    _exitingFullScreen = YES;

    if (auto* manager = self._manager) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
        manager->setAnimatingFullScreen(true);
        manager->willExitFullScreen();
        return;
    }

    OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "manager missing");
    ASSERT_NOT_REACHED();
    [self _exitFullscreenImmediately];
}

- (void)beganExitFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WebKit::WaitingToExitFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != WaitingToExitFullScreen, dropping");
        return;
    }
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, WebCore::FloatRect { initialFrame }, ", ", WebCore::FloatRect { finalFrame });

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
    if (_fullScreenState != WebKit::ExitingFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != ExitingFullScreen, dropping");
        return;
    }
    _fullScreenState = WebKit::NotInFullScreen;
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

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

#if PLATFORM(VISION)
    _lastKnownParentWindow = nil;
    _parentWindowState = nil;
#endif

    CompletionHandler<void()> completionHandler([protectedSelf = retainPtr(self), self, logIdentifier = OBJC_LOGIDENTIFIER] {
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
            OBJC_ALWAYS_LOG(logIdentifier, "repaint completed, enter requested");
            [self requestRestoreFullScreen];
        } else
            OBJC_ALWAYS_LOG(logIdentifier, "repaint completed");
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
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
    [self _exitFullscreenImmediately];
    self._webView = nil;
}

- (void)webViewDidRemoveFromSuperviewWhileInFullscreen
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState);
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
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState);
    _shouldReturnToFullscreenFromPictureInPicture = !_blocksReturnToFullscreenFromPictureInPicture;

    if (_fullScreenState == WebKit::InFullScreen)
        [self requestExitFullScreen];
}

- (void)didExitPictureInPicture
{
    if (!_enterFullscreenNeedsExitPictureInPicture && _shouldReturnToFullscreenFromPictureInPicture) {
        auto* videoFullscreenInterface = self._videoFullscreenManager ? self._videoFullscreenManager->controlsManagerInterface() : nullptr;
        if (videoFullscreenInterface && videoFullscreenInterface->returningToStandby()) {
            OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, "returning to standby");
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

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
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
    if (_fullScreenState == WebKit::NotInFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, ", dropping");
        return;
    }

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

    _shouldReturnToFullscreenFromPictureInPicture = false;
    _exitRequested = NO;
    _exitingFullScreen = NO;
    _fullScreenState = WebKit::NotInFullScreen;
    _shouldReturnToFullscreenFromPictureInPicture = false;

    auto* page = [self._webView _page].get();
    if (page)
        page->setSuppressVisibilityUpdates(true);

#if PLATFORM(VISION)
    if (WebKit::useSpatialFullScreenTransition()) {
        [UIView performWithoutAnimation:^{
            CompletionHandler<void()> completionHandler = []() { };
            [self _performSpatialFullScreenTransition:NO completionHandler:WTFMove(completionHandler)];
        }];
    }
#endif

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

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
    _inInteractiveDismiss = true;
    [self _dismissFullscreenViewController];
}

- (void)_dismissFullscreenViewController
{
    if (!_fullscreenViewController) {
        OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "no fullscreenViewController");
        [self _completedExitFullScreen];
        return;
    }
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

#if PLATFORM(VISION)
    if (WebKit::useSpatialFullScreenTransition()) {
        [self _configureSpatialFullScreenTransition];

        CompletionHandler<void()> completionHandler = [strongSelf = retainPtr(self), self] () {
            [self _completedExitFullScreen];
        };

        [self _performSpatialFullScreenTransition:NO completionHandler:WTFMove(completionHandler)];
        return;
    }
#endif // ENABLE(VISION)

    [_fullscreenViewController setAnimating:YES];
    __block auto logIdentifier = OBJC_LOGIDENTIFIER;
    [_fullscreenViewController dismissViewControllerAnimated:YES completion:^{
        if (![self._webView _page])
            return;

        OBJC_ALWAYS_LOG(logIdentifier, "dismiss completed");
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
}

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
- (void)_interactiveDismissChanged:(id)sender
{
    if (!_inInteractiveDismiss)
        return;

    auto pinchState = [_interactivePinchDismissGestureRecognizer state];
    if (pinchState > UIGestureRecognizerStatePossible && pinchState <= UIGestureRecognizerStateEnded) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, "pinch state: ", pinchState, ", dropping");
        return;
    }

    CGPoint translation = [_interactivePanDismissGestureRecognizer translationInView:_fullscreenViewController.get().view];
    CGPoint velocity = [_interactivePanDismissGestureRecognizer velocityInView:_fullscreenViewController.get().view];
    CGFloat progress = translation.y / (_fullscreenViewController.get().view.bounds.size.height / 2);
    progress = std::min(1., std::max(0., progress));

    if (_interactivePanDismissGestureRecognizer.get().state == UIGestureRecognizerStateEnded) {
        _inInteractiveDismiss = false;

        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, "ended");
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
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, "ended");
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

#if PLATFORM(VISION)

- (BOOL)_sceneDimmingEnabled
{
    if (auto page = [self._webView _page])
        return page->preferences().fullscreenSceneDimmingEnabled();
    return NO;
}

- (BOOL)_sceneAspectRatioLockingEnabled
{
    if (auto page = [self._webView _page])
        return page->preferences().fullscreenSceneAspectRatioLockingEnabled();
    return YES;
}

- (BOOL)prefersSceneDimming
{
    if (![self _sceneDimmingEnabled])
        return NO;

    if (NSNumber *value = [[NSUserDefaults standardUserDefaults] objectForKey:kPrefersFullScreenDimmingKey])
        return value.boolValue;

    return YES;
}

- (void)_configureSpatialFullScreenTransition
{
    CGRect originalWindowBounds = [_lastKnownParentWindow bounds];
    CGRect fullscreenWindowBounds = [_window bounds];

    CGSize sceneSize = CGRectUnion(originalWindowBounds, fullscreenWindowBounds).size;

    CGRect adjustedOriginalWindowFrame = originalWindowBounds;
    adjustedOriginalWindowFrame.origin.x = (sceneSize.width - CGRectGetWidth(adjustedOriginalWindowFrame)) / 2;
    adjustedOriginalWindowFrame.origin.y = (sceneSize.height - CGRectGetHeight(adjustedOriginalWindowFrame)) / 2;

    CGRect adjustedFullscreenWindowFrame = fullscreenWindowBounds;
    adjustedFullscreenWindowFrame.origin.x = (sceneSize.width - CGRectGetWidth(adjustedFullscreenWindowFrame)) / 2;
    adjustedFullscreenWindowFrame.origin.y = (sceneSize.height  - CGRectGetHeight(adjustedFullscreenWindowFrame)) / 2;

    // Workaround rdar://103936581. Since `originalWindow` is not owned by WebKit, it is currently
    // not possible to prevent it from participating in scene resize, as it is being faded out.
    // Temporarily change the object's class to override scene resize behavior.
    object_setClass(_lastKnownParentWindow.get(), [WKFixedSizeWindow class]);
    object_setClass(_window.get(), [WKFixedSizeWindow class]);

    UIWindowScene *scene = [_lastKnownParentWindow windowScene] ?: [_window windowScene];

    scene.mrui_placement.preferredChromeOptions = RSSSceneChromeOptionsNone;

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
    WebKit::resizeScene(scene, sceneSize, [strongSelf = retainPtr(self), self, adjustedOriginalWindowFrame, adjustedFullscreenWindowFrame, logIdentifier = OBJC_LOGIDENTIFIER]() {
        OBJC_ALWAYS_LOG(logIdentifier, "resize completed");
        [_lastKnownParentWindow setFrame:adjustedOriginalWindowFrame];
        [_window setFrame:adjustedFullscreenWindowFrame];

        [self _updateOrnamentOffsetsForTemporarySceneSize:[_window windowScene].coordinateSpace.bounds.size];
    });
}

- (void)_updateOrnamentOffsetsForTemporarySceneSize:(CGSize)newSceneSize
{
    CGSize originalSceneSize = [_parentWindowState sceneSize];

    CGFloat sceneWidthDifference = newSceneSize.width - originalSceneSize.width;
    CGFloat sceneHeightDifference = newSceneSize.height - originalSceneSize.height;

    // The temporary scene size will always be greater than or equal to the original scene size,
    // and the position of the original scene will always be centered relative to the new size.

    ASSERT(sceneWidthDifference >= 0);
    ASSERT(sceneHeightDifference >= 0);

    for (MRUIPlatterOrnament *ornament in [_parentWindowState ornamentProperties]) {
        CGPoint originalOffset2D = [[[_parentWindowState ornamentProperties] objectForKey:ornament] offset2D];
        ornament.offset2D = CGPointMake(
            originalOffset2D.x + sceneWidthDifference * (0.5 - ornament.sceneAnchorPoint.x),
            originalOffset2D.y + sceneHeightDifference * (0.5 - ornament.sceneAnchorPoint.y)
        );
    }
}

- (void)_setOrnamentsHidden:(BOOL)hidden
{
    for (MRUIPlatterOrnament *ornament in [_parentWindowState ornamentProperties]) {
        if (hidden)
            ornament.viewController.view.window.alpha = 0.0;
        else {
            CGFloat originalAlpha = [[[_parentWindowState ornamentProperties] objectForKey:ornament] windowAlpha];
            ornament.viewController.view.window.alpha = originalAlpha;
        }
    }
}

- (void)_performSpatialFullScreenTransition:(BOOL)enter completionHandler:(CompletionHandler<void()>&&)completionHandler
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, enter);
    WKFullScreenWindowController *controller = self;
    UIWindow *inWindow = enter ? _window.get() : _lastKnownParentWindow.get();
    UIWindow *outWindow = enter ? _lastKnownParentWindow.get() : _window.get();
    WKFullScreenParentWindowState *originalState = _parentWindowState.get();

    inWindow.transform3D = CATransform3DTranslate(originalState.transform3D, 0, 0, kIncomingWindowZOffset);

    MRUIStage *stage = UIApplication.sharedApplication.mrui_activeStage;
    if (self.prefersSceneDimming
        || (!enter && stage.preferredDarkness != originalState.preferredDarkness)) {
        [UIView animateWithDuration:kDarknessAnimationDuration animations:^{
            stage.preferredDarkness = enter ? MRUIDarknessPreferenceDark : originalState.preferredDarkness;
        } completion:nil];
    }

    [UIView animateWithDuration:kOutgoingWindowFadeDuration delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        if (enter)
            [self _setOrnamentsHidden:YES];

        outWindow.alpha = 0;
    } completion:nil];

    [UIView animateWithDuration:kWindowTranslationDuration delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        outWindow.transform3D = CATransform3DTranslate(outWindow.transform3D, 0, 0, kOutgoingWindowZOffset);
    } completion:nil];

    [UIView animateWithDuration:kWindowTranslationDuration delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        inWindow.transform3D = originalState.transform3D;
    } completion:nil];

    for (MRUIPlatterOrnament *ornament in originalState.ornamentProperties) {
        CGFloat originalDepth = [[originalState.ornamentProperties objectForKey:ornament] depthDisplacement];
        CGFloat finalDepth = originalDepth;
        if (enter)
            finalDepth += kOutgoingWindowZOffset;
        else
            [ornament _setDepthDisplacement:originalDepth + kIncomingWindowZOffset];

        [UIView animateWithDuration:kWindowTranslationDuration delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
            [ornament _setDepthDisplacement:finalDepth];
        } completion:nil];
    }

    auto completion = makeBlockPtr([controller = retainPtr(controller), inWindow = retainPtr(inWindow), originalState = retainPtr(originalState), enter, completionHandler = WTFMove(completionHandler)] (BOOL finished) mutable {
        WebKit::resizeScene([inWindow windowScene], [inWindow bounds].size, [controller, inWindow, originalState, enter, completionHandler = WTFMove(completionHandler)]() mutable {
            Class inWindowClass = enter ? [UIWindow class] : [originalState windowClass];
            object_setClass(inWindow.get(), inWindowClass);

            UIWindowScene *scene = [inWindow windowScene];

            [inWindow setFrame:scene.coordinateSpace.bounds];

            if (enter) {
                if ([controller _sceneAspectRatioLockingEnabled])
                    scene.mrui_placement.preferredResizingBehavior = MRUISceneResizingBehaviorUniform;
                scene.delegate = adoptNS([[WKFullscreenWindowSceneDelegate alloc] initWithController:controller.get() originalDelegate:scene.delegate]).get();
            } else {
                scene.sizeRestrictions.minimumSize = [originalState sceneMinimumSize];
                scene.mrui_placement.preferredResizingBehavior = [originalState sceneResizingBehavior];
                if (auto delegate = dynamic_objc_cast<WKFullscreenWindowSceneDelegate>(scene.delegate))
                    scene.delegate = [delegate originalDelegate];

                for (MRUIPlatterOrnament *ornament in [originalState ornamentProperties])
                    ornament.offset2D = [[[originalState ornamentProperties] objectForKey:ornament] offset2D];
            }

            scene.mrui_placement.preferredChromeOptions = [originalState sceneChromeOptions];

            completionHandler();
        });
    });

    [UIView animateWithDuration:kIncomingWindowFadeDuration delay:kIncomingWindowFadeDelay options:UIViewAnimationOptionCurveEaseInOut animations:^{
        if (!enter)
            [self _setOrnamentsHidden:NO];

        inWindow.alpha = 1;
    } completion:completion.get()];
}

- (void)toggleSceneDimming
{
    BOOL updatedPrefersSceneDimming = !self.prefersSceneDimming;

    [[NSUserDefaults standardUserDefaults] setBool:updatedPrefersSceneDimming forKey:kPrefersFullScreenDimmingKey];

    if (self.isFullScreen) {
        MRUIStage *stage = UIApplication.sharedApplication.mrui_activeStage;
        stage.preferredDarkness = updatedPrefersSceneDimming ? MRUIDarknessPreferenceDark : [_parentWindowState preferredDarkness];
    }
}

#endif // PLATFORM(VISION)

- (void)showUI
{
#if PLATFORM(VISION)
    UIWindowScene *scene = [_window windowScene];
    scene.mrui_placement.preferredChromeOptions = [_parentWindowState sceneChromeOptions];
#endif
}

- (void)hideUI
{
#if PLATFORM(VISION)
    UIWindowScene *scene = [_window windowScene];
    scene.mrui_placement.preferredChromeOptions = RSSSceneChromeOptionsNone;
#endif
}

@end

#if !RELEASE_LOG_DISABLED
@implementation WKFullScreenWindowController (Logging)
- (const void*)logIdentifier
{
    return _logIdentifier;
}

- (const Logger*)loggerPtr
{
    return _logger.get();
}

- (WTFLogChannel*)logChannel
{
    return &WebKit2LogFullscreen;
}
@end
#endif


#endif // PLATFORM(IOS_FAMILY) && ENABLE(FULLSCREEN_API)
