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

#import "APIFullscreenClient.h"
#import "UIKitSPI.h"
#import "VideoPresentationManagerProxy.h"
#import "WKFullScreenViewController.h"
#import "WKFullscreenStackView.h"
#import "WKPreviewWindowController.h"
#import "WKScrollView.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebView.h"
#import "WKWebViewIOS.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import <Foundation/Foundation.h>
#import <Security/SecCertificate.h>
#import <Security/SecTrust.h>
#import <UIKit/UIVisualEffectView.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/IntRect.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/Timer.h>
#import <WebCore/VideoPresentationInterfaceAVKitLegacy.h>
#import <WebCore/VideoPresentationInterfaceTVOS.h>
#import <WebCore/VideoPresentationModel.h>
#import <WebCore/ViewportArguments.h>
#import <objc/runtime.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/NSStringSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <pal/spi/cocoa/URLFormattingSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

#if PLATFORM(VISION)
#import "MRUIKitSPI.h"
#endif

#if ENABLE(SCENE_GEOMETRY_UPDATE)
#import "UIWindowScene+Extras.h"
#endif

#import "WebKitSwiftSoftLink.h"

#if !HAVE(URL_FORMATTING)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(LinkPresentation)
#endif

static constexpr Seconds DefaultWatchdogTimerInterval = 1_s;

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

static void resizeScene(UIWindowScene *scene, CGSize size, BOOL useDefaultSceneGeometry, BOOL updateSceneGeometryEnabled, CompletionHandler<void()>&& completionHandler)
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
#if ENABLE(SCENE_GEOMETRY_UPDATE)
        if (updateSceneGeometryEnabled)
            [scene setUsesDefaultGeometry:useDefaultSceneGeometry];
#endif
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

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WKFullScreenWindowControllerIOSAdditions.mm>)
#import <WebKitAdditions/WKFullScreenWindowControllerIOSAdditions.mm>
#else
static constexpr auto baseScale = 1;
static constexpr auto baseMinimumEffectiveDeviceWidth = 0;
#endif

struct WKWebViewState {
    WebCore::FloatBoxExtent _savedWebPageObscuredContentInsets;
    CGFloat _savedPageScale = baseScale;
    CGFloat _savedViewScale = 1.0;
    CGFloat _savedZoomScale = baseScale;
    CGFloat _savedContentZoomScale = 1;
    BOOL _savedContentInsetWasExternallyOverridden = NO;
    UIEdgeInsets _savedEdgeInset = UIEdgeInsetsZero;
    BOOL _savedHaveSetObscuredInsets = NO;
    UIEdgeInsets _savedObscuredInsets = UIEdgeInsetsZero;
    UIRectEdge _savedObscuredInsetEdgesAffectedBySafeArea = UIRectEdgeAll;
    UIEdgeInsets _savedScrollIndicatorInsets = UIEdgeInsetsZero;
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    BOOL _savedContentInsetAdjustmentBehaviorWasExternallyOverridden = NO;
#endif
    UIScrollViewContentInsetAdjustmentBehavior _savedContentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentAutomatic;
    CGPoint _savedContentOffset = CGPointZero;
    BOOL _savedBouncesZoom = NO;
    BOOL _savedForceAlwaysUserScalable = NO;
    CGFloat _savedMinimumEffectiveDeviceWidth = baseMinimumEffectiveDeviceWidth;
    BOOL _savedHaveSetUnobscuredSafeAreaInsets = NO;
    UIEdgeInsets _savedUnobscuredSafeAreaInsets = UIEdgeInsetsZero;
    BOOL _savedHasOverriddenLayoutParameters = NO;
    CGSize _savedMinimumUnobscuredSizeOverride = CGSizeZero;
    CGSize _savedMaximumUnobscuredSizeOverride = CGSizeZero;

    void applyTo(WKWebView* webView)
    {
        [webView _setPageScale:_savedPageScale withOrigin:CGPointMake(0, 0)];
        if (_savedHaveSetObscuredInsets)
            webView._obscuredInsets = _savedObscuredInsets;
        else
            [webView _resetObscuredInsets];

        webView._obscuredInsetEdgesAffectedBySafeArea = _savedObscuredInsetEdgesAffectedBySafeArea;

        auto* scrollView = (WKScrollView *)[webView scrollView];
        if (_savedContentInsetWasExternallyOverridden)
            scrollView.contentInset = _savedEdgeInset;
        else
            [scrollView _resetContentInset];

        scrollView.contentOffset = _savedContentOffset;
        scrollView.scrollIndicatorInsets = _savedScrollIndicatorInsets;

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        if (_savedContentInsetAdjustmentBehaviorWasExternallyOverridden)
            scrollView.contentInsetAdjustmentBehavior = _savedContentInsetAdjustmentBehavior;
        else
            [webView _resetScrollViewInsetAdjustmentBehavior];
#endif

        if (_savedHaveSetUnobscuredSafeAreaInsets)
            webView._unobscuredSafeAreaInsets = _savedUnobscuredSafeAreaInsets;
        else
            [webView _resetUnobscuredSafeAreaInsets];

        if (_savedHasOverriddenLayoutParameters)
            [webView _overrideLayoutParametersWithMinimumLayoutSize:_savedMinimumUnobscuredSizeOverride minimumUnobscuredSizeOverride:_savedMinimumUnobscuredSizeOverride maximumUnobscuredSizeOverride:_savedMaximumUnobscuredSizeOverride];
        else
            [webView _clearOverrideLayoutParameters];

        if (auto page = webView._page) {
            page->setObscuredContentInsets(_savedWebPageObscuredContentInsets);
            page->setForceAlwaysUserScalable(_savedForceAlwaysUserScalable);
        }
        [webView _setViewScale:_savedViewScale];
        scrollView.bouncesZoom = _savedBouncesZoom;
        webView._minimumEffectiveDeviceWidth = _savedMinimumEffectiveDeviceWidth;
    }

    void store(WKWebView* webView)
    {
        auto* scrollView = [webView _wkScrollView];
        _savedPageScale = webView._pageScale;
        _savedHaveSetObscuredInsets = webView._haveSetObscuredInsets;
        _savedObscuredInsets = webView._obscuredInsets;
        _savedObscuredInsetEdgesAffectedBySafeArea = webView._obscuredInsetEdgesAffectedBySafeArea;
        _savedContentInsetWasExternallyOverridden = scrollView._contentInsetWasExternallyOverridden;
        _savedEdgeInset = scrollView.contentInset;
        _savedContentOffset = scrollView.contentOffset;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        _savedScrollIndicatorInsets = scrollView.scrollIndicatorInsets;
ALLOW_DEPRECATED_DECLARATIONS_END
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
        _savedContentInsetAdjustmentBehaviorWasExternallyOverridden = scrollView._contentInsetAdjustmentBehaviorWasExternallyOverridden;
#endif
        if (auto page = webView._page) {
            _savedWebPageObscuredContentInsets = page->obscuredContentInsets();
            _savedForceAlwaysUserScalable = page->forceAlwaysUserScalable();
        }
        _savedViewScale = webView._viewScale;
        _savedZoomScale = scrollView.zoomScale;
        _savedContentZoomScale = webView._contentZoomScale;
        _savedBouncesZoom = scrollView.bouncesZoom;
        _savedMinimumEffectiveDeviceWidth = webView._minimumEffectiveDeviceWidth;
        _savedHaveSetUnobscuredSafeAreaInsets = webView._haveSetUnobscuredSafeAreaInsets;
        _savedUnobscuredSafeAreaInsets = webView._unobscuredSafeAreaInsets;
        _savedHasOverriddenLayoutParameters = webView._hasOverriddenLayoutParameters;
        _savedMaximumUnobscuredSizeOverride = webView._maximumUnobscuredSizeOverride;
        _savedMinimumUnobscuredSizeOverride = webView._minimumUnobscuredSizeOverride;
    }
};

#if PLATFORM(VISION)

class FullScreenWindowControllerVideoPresentationModelClient final : WebCore::VideoPresentationModelClient, public CanMakeCheckedPtr<FullScreenWindowControllerVideoPresentationModelClient> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(FullScreenWindowControllerVideoPresentationModelClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FullScreenWindowControllerVideoPresentationModelClient);
public:
    explicit FullScreenWindowControllerVideoPresentationModelClient(WKFullScreenWindowController *windowController)
        : m_windowController { windowController }
    {
    }

    void setInterface(WebCore::VideoPresentationInterfaceIOS* interface)
    {
        if (m_interface == interface)
            return;

        if (m_interface && m_interface->videoPresentationModel())
            m_interface->videoPresentationModel()->removeClient(*this);
        m_interface = interface;
        if (m_interface && m_interface->videoPresentationModel())
            m_interface->videoPresentationModel()->addClient(*this);
    }

    WebCore::VideoPresentationInterfaceIOS* interface() const
    {
        return m_interface.get();
    }

    ~FullScreenWindowControllerVideoPresentationModelClient()
    {
        if (m_interface && m_interface->videoPresentationModel())
            m_interface->videoPresentationModel()->removeClient(*this);
    }

private:
    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    // VideoPresentationModelClient
    void fullscreenModeChanged(HTMLMediaElementEnums::VideoFullscreenMode) final
    {
        [m_windowController.get() bestVideoFullscreenModeChanged];
    }

    WeakObjCPtr<WKFullScreenWindowController> m_windowController;
    RefPtr<WebCore::VideoPresentationInterfaceIOS> m_interface;
};

#endif // PLATFORM(VISION)

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
@property (nonatomic, assign) BOOL prefersAutoDimming;

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
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _preferredDarkness = UIApplication.sharedApplication.mrui_activeStage.preferredDarkness;
ALLOW_DEPRECATED_DECLARATIONS_END

    UIWindowScene *windowScene = window.windowScene;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _sceneSize = windowScene.coordinateSpace.bounds.size;
ALLOW_DEPRECATED_DECLARATIONS_END
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

#if ENABLE(QUICKLOOK_FULLSCREEN)
@interface WKFullScreenWindowController (WKPreviewWindowControllerDelegate) <WKPreviewWindowControllerDelegate>
- (void)previewWindowControllerDidClose:(id)previewWindowController;
@end
#endif

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

@interface WKFullScreenWindowController (VideoPresentationManagerProxyClient)
- (void)didEnterPictureInPicture;
- (void)didExitPictureInPicture;
- (void)didEnterVideoFullscreen;
- (void)didExitVideoFullscreen;
@end

#pragma mark -

#if !RELEASE_LOG_DISABLED
@interface WKFullScreenWindowController (Logging)
@property (readonly, nonatomic) uint64_t logIdentifier;
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
    RetainPtr<WKFullScreenInteractiveTransition> _interactiveDismissTransitionCoordinator;
#endif
#if PLATFORM(VISION)
    RetainPtr<UIWindow> _lastKnownParentWindow;
    RetainPtr<WKFullScreenParentWindowState> _parentWindowState;
#if ENABLE(QUICKLOOK_FULLSCREEN)
    RetainPtr<WKPreviewWindowController> _previewWindowController;
    bool _isUsingQuickLook;
    CGSize _imageDimensions;
#endif // QUICKLOOK_FULLSCREEN
#endif

    RefPtr<WebKit::VideoPresentationManagerProxy::VideoInPictureInPictureDidChangeObserver> _pipObserver;
    BOOL _shouldReturnToFullscreenFromPictureInPicture;
    BOOL _enterFullscreenNeedsExitPictureInPicture;
    BOOL _returnToFullscreenFromPictureInPicture;
    BOOL _blocksReturnToFullscreenFromPictureInPicture;

    CGRect _initialFrame;
    CGRect _finalFrame;
    std::unique_ptr<WebCore::Timer> _watchdogTimer;
    CGSize _originalWindowSize;

    RetainPtr<NSString> _EVOrganizationName;
    BOOL _EVOrganizationNameIsValid;
    BOOL _inInteractiveDismiss;
    BOOL _exitRequested;
    BOOL _enterRequested;
    BOOL _exitingFullScreen;

    RetainPtr<id> _notificationListener;

#if PLATFORM(VISION)
    const std::unique_ptr<WebKit::FullScreenWindowControllerVideoPresentationModelClient> _bestVideoPresentationModelClient;
#endif

#if !RELEASE_LOG_DISABLED
    RefPtr<Logger> _logger;
    uint64_t _logIdentifier;
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
        _logger = webPage->logger();
        _logIdentifier = webPage->logIdentifier();
    }
#endif
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication]];

#if PLATFORM(VISION)
    lazyInitialize(_bestVideoPresentationModelClient, makeUnique<WebKit::FullScreenWindowControllerVideoPresentationModelClient>(self));
#endif

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
        || _fullScreenState == WebKit::InFullScreen
        || _fullScreenState == WebKit::WaitingToExitFullScreen
        || _fullScreenState == WebKit::ExitingFullScreen;
}

- (UIView *)webViewPlaceholder
{
    return _webViewPlaceholder.get();
}

- (WKFullScreenViewController *)fullScreenViewController
{
    return _fullscreenViewController.get();
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
- (BOOL)isUsingQuickLook
{
    return _isUsingQuickLook;
}

- (CGSize)imageDimensions
{
    return _imageDimensions;
}
#endif

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

- (void)enterFullScreen:(CGSize)mediaDimensions completionHandler:(CompletionHandler<void(bool)>&&)completionHandler
{
    if (self.isFullScreen)
        return completionHandler(false);

    RefPtr page = self._webView._page.get();
    if (!page)
        return completionHandler(false);

    _fullScreenState = WebKit::WaitingToEnterFullScreen;

    WeakObjCPtr<WKFullScreenWindowController> weakSelf { self };
    page->fullscreenClient().requestPresentingViewController([logIdentifier = OBJC_LOGIDENTIFIER, self, weakSelf = WTFMove(weakSelf), mediaDimensions, completionHandler = WTFMove(completionHandler)] (UIViewController *viewController, NSError *error) mutable {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(false);

        if (error) {
            OBJC_ERROR_LOG(logIdentifier, "request for window scene failed with error: ", error);
            [self _exitFullscreenImmediately];
            return completionHandler(false);
        }

        if (_exitRequested) {
            OBJC_ALWAYS_LOG(logIdentifier, "received window scene but exit requested");
            [self _exitFullscreenImmediately];
            return completionHandler(false);
        }

        UIWindowScene *windowScene;
        if (UIWindowScene *presentingWindowScene = viewController.view.window.windowScene) {
            OBJC_ALWAYS_LOG(logIdentifier, "using window scene from presenting view controller");
            windowScene = presentingWindowScene;
        } else {
            OBJC_ALWAYS_LOG(logIdentifier, "using window scene from web view");
            windowScene = self._webView.window.windowScene;
        }

        if (!windowScene) {
            OBJC_ERROR_LOG(logIdentifier, "failed to find a window scene");
            [self _exitFullscreenImmediately];
            return completionHandler(false);
        }

        [self _enterFullScreen:mediaDimensions windowScene:windowScene completionHandler:WTFMove(completionHandler)];
    });
}

- (void)didEnterVideoFullscreen
{
#if PLATFORM(VISION)
    if (self.isFullScreen)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        UIApplication.sharedApplication.mrui_activeStage.preferredDarkness = MRUIDarknessPreferenceUnspecified;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

- (void)didExitVideoFullscreen
{
#if PLATFORM(VISION)
    if (!self.isFullScreen || !WebKit::useSpatialFullScreenTransition())
        return;

    bool prefersAutoDimming = true;
    if (RefPtr videoPresentationManager = [self _videoPresentationManager]) {
        if (RefPtr bestVideo = videoPresentationManager->bestVideoForElementFullscreen())
            prefersAutoDimming = bestVideo->playbackSessionModel()->prefersAutoDimming();
    }
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    UIApplication.sharedApplication.mrui_activeStage.preferredDarkness = prefersAutoDimming ? MRUIDarknessPreferenceDark : MRUIDarknessPreferenceUnspecified;
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

- (void)_enterFullScreen:(CGSize)mediaDimensions windowScene:(UIWindowScene *)windowScene completionHandler:(CompletionHandler<void(bool)>&&)completionHandler
{
    ASSERT(_fullScreenState == WebKit::WaitingToEnterFullScreen);
    ASSERT(!_exitRequested);
    ASSERT(windowScene);

    RetainPtr<WKWebView> webView = self._webView;
    auto page = [webView _page];
    auto* manager = self._manager;
    if (!page || !manager)
        return completionHandler(false);

#if ENABLE(QUICKLOOK_FULLSCREEN)
#if ENABLE(UNENTITLED_QUICKLOOK_FULLSCREEN)
    _isUsingQuickLook = manager->isImageElement();
#else
    _isUsingQuickLook = manager->isImageElement() && WTF::processHasEntitlement("com.apple.surfboard.chrome-customization"_s);
#endif // ENABLE(UNENTITLED_QUICKLOOK_FULLSCREEN)

    if (_isUsingQuickLook) {
        _imageDimensions = mediaDimensions;
        _fullScreenState = WebKit::WaitingToEnterFullScreen;

        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, "(QL) presentation updated");

        manager->prepareQuickLookImageURL([strongSelf = retainPtr(self), self, window = retainPtr([webView window]), completionHandler = WTFMove(completionHandler), logIdentifier = OBJC_LOGIDENTIFIER] (URL&& url) mutable {
            UIWindowScene *scene = [window windowScene];
            _previewWindowController = adoptNS([WebKit::allocWKPreviewWindowControllerInstance() initWithURL:url.createNSURL().get() sceneID:scene._sceneIdentifier]);
            [_previewWindowController setDelegate:self];
            [_previewWindowController presentWindowWithCompletionHandler:^{ }];
            _fullScreenState = WebKit::InFullScreen;

            OBJC_ALWAYS_LOG(logIdentifier, "(QL) presentation completed");
            return completionHandler(true);
        });
        return;
    }
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, WebCore::FloatSize { mediaDimensions });

    [self _invalidateEVOrganizationName];

#if PLATFORM(VISION)
    _lastKnownParentWindow = [webView window];
    _parentWindowState = adoptNS([[WKFullScreenParentWindowState alloc] initWithWindow:_lastKnownParentWindow.get()]);
#endif
    _blocksReturnToFullscreenFromPictureInPicture = manager->blocksReturnToFullscreenFromPictureInPicture();
    _originalWindowSize = [webView window].frame.size;

    _window = adoptNS([[UIWindow alloc] initWithWindowScene:windowScene]);
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
        if (mediaDimensions.height && mediaDimensions.width) {
            CGFloat preferredAspectRatio = preferredWidth / preferredHeight;
            CGFloat videoAspectRatio = mediaDimensions.height ? (mediaDimensions.width / mediaDimensions.height) : preferredAspectRatio;
            if (videoAspectRatio > preferredAspectRatio)
                targetHeight = mediaDimensions.height * preferredWidth / mediaDimensions.width;
            else
                targetWidth = mediaDimensions.width * preferredHeight / mediaDimensions.height;
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
#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
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
#endif // ENABLE(FULLSCREEN_DISMISSAL_GESTURES)

    page->setSuppressVisibilityUpdates(true);
    page->startDeferringResizeEvents();
    page->startDeferringScrollEvents();
    page->startDeferringIntersectionObservations();

    _viewState.store(webView.get());

    _webViewPlaceholder = adoptNS([[WKFullScreenPlaceholderView alloc] init]);
    [_webViewPlaceholder setParent:self];
    [[_webViewPlaceholder layer] setName:@"Fullscreen Placeholder View"];

    WKSnapshotConfiguration* config = nil;
    auto logIdentifier = OBJC_LOGIDENTIFIER;
    [webView takeSnapshotWithConfiguration:config completionHandler:makeBlockPtr([self, protectedSelf = RetainPtr { self }, logIdentifier, completionHandler = WTFMove(completionHandler)] (UIImage * snapshotImage, NSError * error) mutable {
        RetainPtr<WKWebView> webView = self._webView;
        auto page = [self._webView _page];
        if (!page)
            return completionHandler(false);

        OBJC_ALWAYS_LOG(logIdentifier, "snapshot completed");
        [CATransaction begin];
        [CATransaction setDisableActions:YES];

        [[_webViewPlaceholder layer] setContents:(id)[snapshotImage CGImage]];
        WebKit::replaceViewWithView(webView.get(), _webViewPlaceholder.get());

        [webView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
        // FIXME (267139): Changing the web view's frame should ideally only be done by the fullscreen view controller.
        // However, this adjustment is currently necessary to ensure that the layout parameters exposed to the web
        // during the "fullscreenchange" event reflect the fullscreen size.
        [webView setFrame:[_window bounds]];
        // FIXME: Is it necessary to set minimum effective device width and view scale here, given that we immediately
        // apply the default web view state (which sets both parameters anyways)?
        [webView _setMinimumEffectiveDeviceWidth:0];
        [webView _setViewScale:1.f];
        [webView _setForcesInitialScaleFactor:YES];
        [webView _resetContentOffset];
        [_window insertSubview:webView.get() atIndex:0];
        WebKit::WKWebViewState().applyTo(webView.get());
        [webView setNeedsLayout];
        [webView layoutIfNeeded];

        if (auto* manager = self._manager)
            manager->setAnimatingFullScreen(true);

        page->updateRenderingWithForcedRepaint([protectedSelf, self, logIdentifier = logIdentifier, completionHandler = WTFMove(completionHandler)] mutable {
            if (_exitRequested) {
                _exitRequested = NO;
                OBJC_ERROR_LOG(logIdentifier, "repaint completed, but exit requested");
                [self _exitFullscreenImmediately];
                return completionHandler(false);
            }

            if (![protectedSelf _manager]) {
                ASSERT_NOT_REACHED();
                OBJC_ERROR_LOG(logIdentifier, "repaint completed, but manager missing");
                [self _exitFullscreenImmediately];
                return completionHandler(false);
            }

            [self._webView _doAfterNextVisibleContentRectAndPresentationUpdate:makeBlockPtr([self, protectedSelf, logIdentifier, completionHandler = WTFMove(completionHandler)] mutable {
                OBJC_ALWAYS_LOG(logIdentifier, "presentation updated");
                WebKit::WKWebViewState().applyTo(self._webView);
                return completionHandler(true);
            }).get()];
        });

        [CATransaction commit];
    }).get()];
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
- (void)updateImageSource
{
    RetainPtr<WKWebView> webView = self._webView;
    auto* manager = self._manager;
    if (!manager)
        return;

    manager->prepareQuickLookImageURL([strongSelf = retainPtr(self), self, window = retainPtr([webView window]), logIdentifier = OBJC_LOGIDENTIFIER](URL&& url) mutable {
        [_previewWindowController updateImage:url.createNSURL().get() completionHandler:^{ }];
    });
}
#endif

- (void)beganEnterFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame completionHandler:(CompletionHandler<void(bool)>&&)completionHandler
{
    if (_fullScreenState != WebKit::WaitingToEnterFullScreen) {
        OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != WaitingToEnterFullScreen, dropping");
        return completionHandler(false);
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

    [_rootViewController presentViewController:_fullscreenViewController.get() animated:shouldAnimateEnterFullscreenTransition completion:makeBlockPtr([self, weakSelf = WeakObjCPtr { self }, completionHandler = WTFMove(completionHandler), logIdentifier = OBJC_LOGIDENTIFIER] mutable {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf)
            return completionHandler(false);

        _fullScreenState = WebKit::InFullScreen;

        if (_exitRequested) {
            _exitRequested = NO;
            OBJC_ERROR_LOG(logIdentifier, "presentation completed, but exit requested");
            [self _exitFullscreenImmediately];
            return completionHandler(false);
        }

        auto page = [self._webView _page];
        auto* manager = self._manager;

        if (page && manager) {
            OBJC_ALWAYS_LOG(logIdentifier, "presentation completed");

            [self._webView becomeFirstResponder];
            completionHandler(true);
            manager->setAnimatingFullScreen(false);
            page->setSuppressVisibilityUpdates(false);
            page->flushDeferredResizeEvents();
            page->flushDeferredScrollEvents();
            page->flushDeferredIntersectionObservations();

            [_fullscreenViewController showBanner];

#if PLATFORM(VISION)
            if (WebKit::useSpatialFullScreenTransition()) {
                CompletionHandler<void()> completionHandler = [protectedSelf = RetainPtr { self }]() {
                    // We may have lost key status during the transition into fullscreen
                    [protectedSelf->_window makeKeyAndVisible];
                };
                [self _performSpatialFullScreenTransition:YES completionHandler:WTFMove(completionHandler)];
            }
#endif

            if (auto* videoPresentationManager = self._videoPresentationManager) {
                if (!_pipObserver) {
                    _pipObserver = WebKit::VideoPresentationManagerProxy::VideoInPictureInPictureDidChangeObserver::create([weakSelf = WeakObjCPtr { self }] (bool inPiP) {
                        RetainPtr strongSelf = weakSelf.get();
                        if (!strongSelf)
                            return;
                        if (inPiP)
                            [strongSelf didEnterPictureInPicture];
                        else
                            [strongSelf didExitPictureInPicture];
                    });
                    videoPresentationManager->addVideoInPictureInPictureDidChangeObserver(*_pipObserver);
                }
                if (RefPtr videoPresentationInterface = videoPresentationManager ? videoPresentationManager->returningToStandbyInterface() : nullptr) {
                    if (_returnToFullscreenFromPictureInPicture)
                        videoPresentationInterface->preparedToReturnToStandby();
                    else if (videoPresentationInterface->inPictureInPicture()) {
                        if (RefPtr model = videoPresentationInterface->videoPresentationModel()) {
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
        completionHandler(false);
        [self _exitFullscreenImmediately];
    }).get()];
}

- (void)requestRestoreFullScreen:(CompletionHandler<void(bool)>&&)completionHandler
{
    if (_fullScreenState != WebKit::NotInFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != NotInFullScreen, dropping");
        return completionHandler(false);
    }

    // Switch the active tab if needed
    if (auto* page = [self._webView _page].get())
        page->fullscreenMayReturnToInline();

    if (auto* manager = self._manager) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
        manager->requestRestoreFullScreen(WTFMove(completionHandler));
        return;
    }

    OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "manager missing, dropping");
    ASSERT_NOT_REACHED();
    completionHandler(false);
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

- (void)exitFullScreen:(CompletionHandler<void()>&&)completionHandler
{
    [self _cancelWatchdogTimer];

    if (_fullScreenState == WebKit::NotInFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, ", dropping");
        return completionHandler();
    }

    if (_fullScreenState < WebKit::InFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " < InFullScreen");
        _exitRequested = YES;
        return completionHandler();
    }

#if ENABLE(QUICKLOOK_FULLSCREEN)
    if (_isUsingQuickLook) {
        _fullScreenState = WebKit::NotInFullScreen;

        if (_previewWindowController) {
            [_previewWindowController dismissWindowWithCompletionHandler:^{ }];
            [_previewWindowController setDelegate:nil];
            _previewWindowController = nil;
        }

        if (self._manager) {
            OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
            completionHandler();
            return;
        }

        OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "manager missing");
        ASSERT_NOT_REACHED();
        [self _exitFullscreenImmediately];
        return completionHandler();
    }
#endif

    [self._webView _beginAnimatedFullScreenExit];

    _fullScreenState = WebKit::WaitingToExitFullScreen;
    _exitingFullScreen = YES;

    if (auto* manager = self._manager) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
        manager->setAnimatingFullScreen(true);
        [self _startWatchdogTimer];
        return completionHandler();
    }

    OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "manager missing");
    ASSERT_NOT_REACHED();
    [self _exitFullscreenImmediately];
    completionHandler();
}

- (void)beganExitFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame completionHandler:(CompletionHandler<void()>&&)completionHandler
{
    if (_fullScreenState != WebKit::WaitingToExitFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != WaitingToExitFullScreen, dropping");
        return completionHandler();
    }
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, WebCore::FloatRect { initialFrame }, ", ", WebCore::FloatRect { finalFrame });

    _fullScreenState = WebKit::ExitingFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;

    _initialFrame.size = WebKit::sizeExpandedToSize(_initialFrame.size, CGSizeMake(1, 1));
    _finalFrame.size = WebKit::sizeExpandedToSize(_finalFrame.size, CGSizeMake(1, 1));
    _finalFrame = WebKit::safeInlineRect(_finalFrame, [_rootViewController view].frame.size);

    if (auto page = [self._webView _page]) {
        page->setSuppressVisibilityUpdates(true);
        page->startDeferringResizeEvents();
        page->startDeferringScrollEvents();
    }

    [_fullscreenViewController setPrefersStatusBarHidden:NO];

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    if (_interactiveDismissTransitionCoordinator) {
        [_interactiveDismissTransitionCoordinator finishInteractiveTransition];
        _interactiveDismissTransitionCoordinator = nil;
        return completionHandler();
    }
#endif

    [self _dismissFullscreenViewController:WTFMove(completionHandler)];
}

- (void)_reinsertWebViewUnderPlaceholder
{
    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    RetainPtr<WKWebView> webView = self._webView;
    [[_webViewPlaceholder superview] insertSubview:webView.get() belowSubview:_webViewPlaceholder.get()];
    [webView setFrame:[_webViewPlaceholder frame]];
    [webView setAutoresizingMask:[_webViewPlaceholder autoresizingMask]];

    [webView becomeFirstResponder];

    [webView _setForcesInitialScaleFactor:NO];
    _viewState.applyTo(webView.get());
    if (auto page = [webView _page])
        page->setOverrideViewportArguments(std::nullopt);

    [webView setNeedsLayout];
    [webView layoutIfNeeded];

    [webView _endAnimatedFullScreenExit];

    [CATransaction commit];
}

- (void)_completedExitFullScreen:(CompletionHandler<void()>&&)completionHandler
{
    [self _cancelWatchdogTimer];

    if (_fullScreenState != WebKit::ExitingFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, " != ExitingFullScreen, dropping");
        return completionHandler();
    }
    _fullScreenState = WebKit::NotInFullScreen;
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    bool windowWasKey = [_window isKeyWindow];

    [self _reinsertWebViewUnderPlaceholder];

    if (auto* manager = self._manager)
        manager->setAnimatingFullScreen(false);
    completionHandler();

    if (auto page = [self._webView _page]) {
        page->flushDeferredResizeEvents();
        page->flushDeferredScrollEvents();
        page->flushDeferredIntersectionObservations();
    }

    RefPtr videoPresentationInterface = self._videoPresentationManager ? self._videoPresentationManager->controlsManagerInterface() : nullptr;
    _shouldReturnToFullscreenFromPictureInPicture = videoPresentationInterface && videoPresentationInterface->inPictureInPicture();

    [_window setHidden:YES];
    _window = nil;

#if PLATFORM(VISION)
    if (!self._isBestVideoInFullScreen) {
        _lastKnownParentWindow = nil;
        _parentWindowState = nil;
    }
#endif

    CompletionHandler<void()> completionHandlerAfterRenderingUpdateIfFocused([protectedSelf = retainPtr(self), self, windowWasKey, logIdentifier = OBJC_LOGIDENTIFIER] {
        _webViewPlaceholder.get().parent = nil;
        [_webViewPlaceholder removeFromSuperview];

        if (auto page = [self._webView _page]) {
            page->setSuppressVisibilityUpdates(false);
            page->setNeedsDOMWindowResizeEvent();
            page->flushDeferredResizeEvents();
            page->flushDeferredScrollEvents();
            page->flushDeferredIntersectionObservations();
        }

        _exitRequested = NO;
        _exitingFullScreen = NO;
        if (_enterRequested) {
            _enterRequested = NO;
            OBJC_ALWAYS_LOG(logIdentifier, "repaint completed, enter requested");
            [self requestRestoreFullScreen:[] (bool) { }];
        } else
            OBJC_ALWAYS_LOG(logIdentifier, "repaint completed");

        if (windowWasKey)
            [self._webView.window makeKeyWindow];
        [CATransaction commit];
    });

    auto* page = [self._webView _page].get();
    if (page && page->isViewFocused())
        page->updateRenderingWithForcedRepaint(WTFMove(completionHandlerAfterRenderingUpdateIfFocused));
    else
        completionHandlerAfterRenderingUpdateIfFocused();

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

- (void)videosInElementFullscreenChanged
{
    if (_fullscreenViewController)
        [_fullscreenViewController videosInElementFullscreenChanged];
}

- (void)placeholderWillMoveToSuperview:(UIView *)superview
{
#if !PLATFORM(APPLETV)
    if (superview)
        return;

    RunLoop::mainSingleton().dispatch([self, strongSelf = retainPtr(self)] {
        if ([_webViewPlaceholder superview] == nil && [_webViewPlaceholder parent] == self)
            [self close];
    });
#endif
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
        RefPtr videoPresentationInterface = self._videoPresentationManager ? self._videoPresentationManager->returningToStandbyInterface() : nullptr;
        if (videoPresentationInterface && videoPresentationInterface->returningToStandby()) {
            OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, "returning to standby");
            if (!_exitingFullScreen) {
                if (_fullScreenState == WebKit::InFullScreen)
                    videoPresentationInterface->preparedToReturnToStandby();
                else {
                    auto completion = [videoPresentationInterface = Ref { *videoPresentationInterface }] (bool succeeded) {
                        if (!succeeded)
                            videoPresentationInterface->failedToRestoreFullscreen();
                    };

                    [self requestRestoreFullScreen:WTFMove(completion)];
                }
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
        _interactiveDismissTransitionCoordinator = adoptNS([[WKFullScreenInteractiveTransition alloc] initWithAnimator:(WKFullscreenAnimationController *)animator anchor:anchor]);
    }

    return _interactiveDismissTransitionCoordinator.get();
#else
    return nil;
#endif
}

- (void)_startWatchdogTimer
{
    // If the page doesn't respond in DefaultWatchdogTimerInterval seconds, it could be because
    // the WebProcess has hung, so exit anyway.
    if (!_watchdogTimer) {
        _watchdogTimer = makeUnique<WebCore::Timer>([weakSelf = WeakObjCPtr { self }] {
            RetainPtr strongSelf = weakSelf.get();
            if (strongSelf)
                [strongSelf _exitFullscreenImmediately];
        });
        _watchdogTimer->startOneShot(DefaultWatchdogTimerInterval);
    }
}

- (void)_cancelWatchdogTimer
{
    if (!_watchdogTimer)
        return;
    _watchdogTimer->stop();
    _watchdogTimer = nullptr;
}

#pragma mark -
#pragma mark Internal Interface

- (void)_exitFullscreenImmediately
{
    [self _cancelWatchdogTimer];

    if (_fullScreenState == WebKit::NotInFullScreen) {
        OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, _fullScreenState, ", dropping");
        return;
    }

    RetainPtr webView = [self _webView];
#if HAVE(LIQUID_GLASS)
    [webView _removeReasonToHideTopScrollPocket:WebKit::HideScrollPocketReason::FullScreen];
#endif

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

    _shouldReturnToFullscreenFromPictureInPicture = false;
    _exitRequested = NO;
    _exitingFullScreen = NO;
    _fullScreenState = WebKit::NotInFullScreen;
    _shouldReturnToFullscreenFromPictureInPicture = false;

    RefPtr page = [webView _page].get();
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

    if (auto* manager = self._manager)
        manager->requestExitFullScreen();

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

- (WebKit::VideoPresentationManagerProxy*)_videoPresentationManager
{
    if (auto page = [self._webView _page])
        return page->videoPresentationManager();
    return nullptr;
}

- (void)_startToDismissFullscreenChanged:(id)sender
{
    if (_inInteractiveDismiss)
        return;

    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);
    _inInteractiveDismiss = true;
    [self requestExitFullScreen];
    [self _dismissFullscreenViewController:[] { }];
}

- (void)_dismissFullscreenViewController:(CompletionHandler<void()>&&)completionHandler
{
    if (!_fullscreenViewController) {
        OBJC_ERROR_LOG(OBJC_LOGIDENTIFIER, "no fullscreenViewController");
        [self _completedExitFullScreen:WTFMove(completionHandler)];
        return;
    }
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER);

#if PLATFORM(VISION)
    if (WebKit::useSpatialFullScreenTransition()) {
        [self _configureSpatialFullScreenTransition];

        [self _performSpatialFullScreenTransition:NO completionHandler:[self, strongSelf = retainPtr(self), completionHandler = WTFMove(completionHandler)] mutable {
            [self _completedExitFullScreen:WTFMove(completionHandler)];
        }];
        return;
    }
#endif // PLATFORM(VISION)

    [_fullscreenViewController setAnimating:YES];
    [_fullscreenViewController dismissViewControllerAnimated:YES completion:makeBlockPtr([self, weakSelf = WeakObjCPtr { self }, completionHandler = WTFMove(completionHandler), logIdentifier = OBJC_LOGIDENTIFIER] mutable {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf || ![strongSelf.get()._webView _page])
            return completionHandler();

        OBJC_ALWAYS_LOG(logIdentifier, "dismiss completed");
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
        if (_interactiveDismissTransitionCoordinator.get().animator.context.transitionWasCancelled)
            [_fullscreenViewController setAnimating:NO];
        else
            [strongSelf _completedExitFullScreen:WTFMove(completionHandler)];

        _interactiveDismissTransitionCoordinator = nil;
#else
        [strongSelf _completedExitFullScreen:WTFMove(completionHandler)];
#endif
    }).get()];
}

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
- (void)_interactiveDismissChanged:(id)sender
{
    if (!_inInteractiveDismiss)
        return;

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

    if (RefPtr videoPresentationManager = [self _videoPresentationManager]) {
        if (RefPtr bestVideo = videoPresentationManager->bestVideoForElementFullscreen())
            return bestVideo->playbackSessionModel()->prefersAutoDimming();
    }

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
    WebKit::resizeScene(scene, sceneSize, NO, NO, [strongSelf = retainPtr(self), self, adjustedOriginalWindowFrame, adjustedFullscreenWindowFrame, logIdentifier = OBJC_LOGIDENTIFIER]() {
        OBJC_ALWAYS_LOG(logIdentifier, "resize completed");
        [_lastKnownParentWindow setFrame:adjustedOriginalWindowFrame];
        [_window setFrame:adjustedFullscreenWindowFrame];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [self _updateOrnamentOffsetsForTemporarySceneSize:[_window windowScene].coordinateSpace.bounds.size];
ALLOW_DEPRECATED_DECLARATIONS_END
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

- (BOOL)_isBestVideoInFullScreen
{
    return _bestVideoPresentationModelClient->interface() && _bestVideoPresentationModelClient->interface()->hasMode(WebCore::MediaPlayerEnums::VideoFullscreenModeStandard);
}

- (BOOL)_shouldShowOrnaments
{
    if (self._isBestVideoInFullScreen)
        return NO;

    // FIXME: It would be simpler to check self.isFullScreen here, but _fullScreenState is set to
    // NotInFullScreen after showing ornaments when exiting via -_dismissFullscreenViewController,
    // but *before* showing ornaments when exiting via -_exitFullscreenImmediately. We should make
    // these two paths behave consistently.
    switch (_fullScreenState) {
    case WebKit::ExitingFullScreen:
    case WebKit::NotInFullScreen:
        break;
    case WebKit::WaitingToEnterFullScreen:
    case WebKit::EnteringFullScreen:
    case WebKit::InFullScreen:
    case WebKit::WaitingToExitFullScreen:
        return NO;
    }

    if (!_parentWindowState)
        return NO;

    return YES;
}

- (void)_performSpatialFullScreenTransition:(BOOL)enter completionHandler:(CompletionHandler<void()>&&)completionHandler
{
    OBJC_ALWAYS_LOG(OBJC_LOGIDENTIFIER, enter);
    WKFullScreenWindowController *controller = self;
    UIWindow *inWindow = enter ? _window.get() : _lastKnownParentWindow.get();
    UIWindow *outWindow = enter ? _lastKnownParentWindow.get() : _window.get();
    WKFullScreenParentWindowState *originalState = _parentWindowState.get();

    inWindow.transform3D = CATransform3DTranslate(originalState.transform3D, 0, 0, kIncomingWindowZOffset);

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    MRUIStage *stage = UIApplication.sharedApplication.mrui_activeStage;
    MRUIDarknessPreference targetDarkness = enter ? (self.prefersSceneDimming ? MRUIDarknessPreferenceDark : originalState.preferredDarkness) : originalState.preferredDarkness;

    if (stage.preferredDarkness != targetDarkness) {
        [UIView animateWithDuration:kDarknessAnimationDuration animations:^{
            stage.preferredDarkness = targetDarkness;
        } completion:nil];
    }
ALLOW_DEPRECATED_DECLARATIONS_END

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

    BOOL allowSceneGeometryUpdates = NO;
#if ENABLE(SCENE_GEOMETRY_UPDATE)
    // TODO: https://bugs.webkit.org/show_bug.cgi?id=303664
    if (auto page = [self._webView _page])
        allowSceneGeometryUpdates = page->preferences().updateSceneGeometryEnabled();
#endif

    auto completion = makeBlockPtr([controller = retainPtr(controller), inWindow = retainPtr(inWindow), originalState = retainPtr(originalState), enter, allowSceneGeometryUpdates, completionHandler = WTFMove(completionHandler)] (BOOL finished) mutable {
        WebKit::resizeScene([inWindow windowScene], [inWindow bounds].size, !enter, allowSceneGeometryUpdates, [controller, inWindow, originalState, enter, completionHandler = WTFMove(completionHandler)] mutable {
            Class inWindowClass = enter ? [UIWindow class] : [originalState windowClass];
            object_setClass(inWindow.get(), inWindowClass);

            UIWindowScene *scene = [inWindow windowScene];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            [inWindow setFrame:scene.coordinateSpace.bounds];
ALLOW_DEPRECATED_DECLARATIONS_END

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

                // Dismiss the fullscreen view controller on exit to ensure hit-test redirection does not occur.
                [controller->_fullscreenViewController dismissViewControllerAnimated:NO completion:nil];
            }

            scene.mrui_placement.preferredChromeOptions = [originalState sceneChromeOptions];

            completionHandler();
        });
    });

    [UIView animateWithDuration:kIncomingWindowFadeDuration delay:kIncomingWindowFadeDelay options:UIViewAnimationOptionCurveEaseInOut animations:^{
        if (!enter && self._shouldShowOrnaments)
            [self _setOrnamentsHidden:NO];

        inWindow.alpha = 1;
    } completion:completion.get()];
}

- (void)toggleSceneDimming
{
    BOOL updatedPrefersSceneDimming = !self.prefersSceneDimming;

    if (RefPtr videoPresentationManager = [self _videoPresentationManager]) {
        if (RefPtr bestVideo = videoPresentationManager->bestVideoForElementFullscreen())
            bestVideo->playbackSessionModel()->setPrefersAutoDimming(updatedPrefersSceneDimming);
    }

    if (self.isFullScreen) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        MRUIDarknessPreference target = updatedPrefersSceneDimming ? MRUIDarknessPreferenceDark : (_parentWindowState ? [_parentWindowState preferredDarkness] : MRUIDarknessPreferenceUnspecified);
        MRUIStage *stage = UIApplication.sharedApplication.mrui_activeStage;
        stage.preferredDarkness = target;
ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

- (void)bestVideoFullscreenModeChanged
{
    if (!self._shouldShowOrnaments)
        return;

    [UIView animateWithDuration:kIncomingWindowFadeDuration delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        [self _setOrnamentsHidden:NO];
    } completion:^(BOOL) {
        UIWindowScene *scene = [_lastKnownParentWindow windowScene];
        scene.mrui_placement.preferredChromeOptions = [_parentWindowState sceneChromeOptions];
        scene.mrui_placement.preferredResizingBehavior = [_parentWindowState sceneResizingBehavior];
        scene.sizeRestrictions.minimumSize = [_parentWindowState sceneMinimumSize];

        _lastKnownParentWindow = nil;
        _parentWindowState = nil;
        _bestVideoPresentationModelClient->setInterface(nullptr);
    }];
}

#endif // PLATFORM(VISION)

- (void)showUI
{
#if PLATFORM(VISION)

#if ENABLE(QUICKLOOK_FULLSCREEN)
    if (_isUsingQuickLook)
        return;
#endif // QUICKLOOK_FULLSCREEN

    UIWindowScene *scene = [_window windowScene];
    scene.mrui_placement.preferredChromeOptions = [_parentWindowState sceneChromeOptions];
#endif
}

- (void)hideUI
{
#if PLATFORM(VISION)

#if ENABLE(QUICKLOOK_FULLSCREEN)
    if (_isUsingQuickLook)
        return;
#endif // QUICKLOOK_FULLSCREEN

    UIWindowScene *scene = [_window windowScene];
    scene.mrui_placement.preferredChromeOptions = RSSSceneChromeOptionsNone;
#endif
}

- (void)fullScreenViewControllerDidInvalidate:(WKFullScreenViewController *)fullScreenViewController
{
#if PLATFORM(VISION)
    if (!self._isBestVideoInFullScreen)
        _bestVideoPresentationModelClient->setInterface(nullptr);
#endif
}

#if PLATFORM(VISION)
- (void)fullScreenViewController:(WKFullScreenViewController *)fullScreenViewController bestVideoPresentationInterfaceDidChange:(nullable WebCore::PlatformVideoPresentationInterface*)bestVideoPresentationInterface
{
    _bestVideoPresentationModelClient->setInterface(bestVideoPresentationInterface);
}
#endif

- (void)didCleanupFullscreen
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (self.isFullScreen) {
        [_fullscreenViewController showUI];
        [_fullscreenViewController configureEnvironmentPickerOrFullscreenVideoButtonView];
    }
#endif
}

@end

#if ENABLE(QUICKLOOK_FULLSCREEN)

@implementation WKFullScreenWindowController (WKPreviewWindowControllerDelegate)
- (void)previewWindowControllerDidClose:(id)previewWindowController
{
    if (previewWindowController != _previewWindowController)
        return;

    [self requestExitFullScreen];
}
@end

#endif // ENABLE(QUICKLOOK_FULLSCREEN)

#if !RELEASE_LOG_DISABLED
@implementation WKFullScreenWindowController (Logging)
- (uint64_t)logIdentifier
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
