/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) && ENABLE(FULLSCREEN_API)
#import "WKFullScreenWindowControllerIOS.h"

#import "UIKitSPI.h"
#import "WKWebView.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
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
#import <WebCore/WebCoreNSURLExtras.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <pal/spi/cocoa/LinkPresentationSPI.h>
#import <pal/spi/cocoa/NSStringSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

using namespace WebKit;
using namespace WebCore;

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(LinkPresentation)

namespace WebKit {

static const NSTimeInterval showHideAnimationDuration = 0.1;
static const NSTimeInterval autoHideDelay = 4.0;

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

    void applyTo(WKWebView* webView)
    {
        [webView _setPageScale:_savedPageScale withOrigin:CGPointMake(0, 0)];
        [webView _setObscuredInsets:_savedObscuredInsets];
        [[webView scrollView] setContentInset:_savedEdgeInset];
        [[webView scrollView] setContentOffset:_savedContentOffset];
        [[webView scrollView] setScrollIndicatorInsets:_savedScrollIndicatorInsets];
        [webView _page]->setTopContentInset(_savedTopContentInset);
        [webView _setViewScale:_savedViewScale];
        [[webView scrollView] setZoomScale:_savedZoomScale];
    }
    
    void store(WKWebView* webView)
    {
        _savedPageScale = [webView _pageScale];
        _savedObscuredInsets = [webView _obscuredInsets];
        _savedEdgeInset = [[webView scrollView] contentInset];
        _savedContentOffset = [[webView scrollView] contentOffset];
        _savedScrollIndicatorInsets = [[webView scrollView] scrollIndicatorInsets];
        _savedTopContentInset = [webView _page]->topContentInset();
        _savedViewScale = [webView _viewScale];
        _savedZoomScale = [[webView scrollView] zoomScale];
    }
};
    
} // namespace WebKit


@interface _WKFullScreenViewController : UIViewController <UIGestureRecognizerDelegate>
@property (retain, nonatomic) NSArray *savedConstraints;
@property (retain, nonatomic) UIView *contentView;
@property (retain, nonatomic) id target;
@property (assign, nonatomic) SEL action;
@end

@implementation _WKFullScreenViewController {
    RetainPtr<UIView> _backgroundView;
    RetainPtr<UILongPressGestureRecognizer> _touchGestureRecognizer;
    RetainPtr<UIButton> _cancelButton;
    RetainPtr<UIButton> _locationButton;
    RetainPtr<UIVisualEffectView> _visualEffectView;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [super dealloc];
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    [coordinator animateAlongsideTransition: ^(id<UIViewControllerTransitionCoordinatorContext> context) {

        void (^webViewUpdateBlock)() = ^{
            [(WKWebView *)[self contentView] _overrideLayoutParametersWithMinimumLayoutSize:size maximumUnobscuredSizeOverride:size];
        };

        [(WKWebView *)[self contentView] _beginAnimatedResizeWithUpdates:webViewUpdateBlock];
        [(WKWebView *)[self contentView] _setInterfaceOrientationOverride:[UIApp statusBarOrientation]];
    } completion:^(id <UIViewControllerTransitionCoordinatorContext>context) {
        [(WKWebView *)[self contentView] _endAnimatedResize];
    }];
}

+ (void)configureView:(UIView *)view withBackgroundFillOfColor:(UIColor *)fillColor opacity:(CGFloat)opacity filter:(NSString *)filter
{
    _UIVisualEffectLayerConfig *baseLayerConfig = [_UIVisualEffectLayerConfig layerWithFillColor:fillColor opacity:opacity filterType:filter];
    [[[_UIVisualEffectConfig configWithContentConfig:baseLayerConfig] contentConfig] configureLayerView:view];
}

- (void)_updateTransparencyOfVisualEffectView:(UIVisualEffectView *)visualEffectView
{
    RetainPtr<UIVisualEffect> visualEffect;

    if (UIAccessibilityIsReduceTransparencyEnabled()) {
        visualEffect = [UIVisualEffect emptyEffect];
        [[visualEffectView contentView] setBackgroundColor:[UIColor colorWithRed:(43.0 / 255.0) green:(46.0 / 255.0) blue:(48.0 / 255.0) alpha:1.0]];
    } else {
        RetainPtr<UIColorEffect> saturationEffect = [UIColorEffect colorEffectSaturate:1.8];
        RetainPtr<UIBlurEffect> blurEffect = [UIBlurEffect effectWithBlurRadius:UIRoundToScreenScale(17.5, [UIScreen mainScreen])];
        RetainPtr<UIVisualEffect> combinedEffects = [UIVisualEffect effectCombiningEffects:@[blurEffect.get(), saturationEffect.get()]];
        visualEffect = combinedEffects;
        [[visualEffectView contentView] setBackgroundColor:nil];
    }

    [visualEffectView setEffect:visualEffect.get()];
}

- (UIVisualEffectView *)visualEffectViewWithFrame:(CGRect)frame
{
    RetainPtr<UIVisualEffectView> visualEffectView = adoptNS([[UIVisualEffectView alloc] initWithEffect:[UIVisualEffect emptyEffect]]);
    [self _updateTransparencyOfVisualEffectView:visualEffectView.get()];

    RetainPtr<UIView> backLayerTintView = adoptNS([[UIView alloc] initWithFrame:[visualEffectView bounds]]);
    [backLayerTintView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [backLayerTintView setUserInteractionEnabled:NO];
    [[self class] configureView:backLayerTintView.get() withBackgroundFillOfColor:[UIColor colorWithWhite:0.0 alpha:0.55] opacity:1.0 filter:kCAFilterNormalBlendMode];
    [[visualEffectView contentView] addSubview:backLayerTintView.get()];

    RetainPtr<UIView> topLayerTintView = adoptNS([[UIView alloc] initWithFrame:[visualEffectView bounds]]);
    [topLayerTintView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [topLayerTintView setUserInteractionEnabled:NO];
    [[self class] configureView:topLayerTintView.get() withBackgroundFillOfColor:[UIColor colorWithWhite:1.0 alpha:0.14] opacity:1.0 filter:kCAFilterNormalBlendMode];
    [[visualEffectView contentView] addSubview:topLayerTintView.get()];

    [self _updateTransparencyOfVisualEffectView:visualEffectView.get()];

    return visualEffectView.autorelease();
}

static UIEdgeInsets mirrorEdgeInsets(UIEdgeInsets insets)
{
    return UIEdgeInsetsMake(insets.top, insets.right, insets.bottom, insets.left);
}

- (void)_updateLayoutMargins
{
    UIView *view = [self view];
    [view setPreservesSuperviewLayoutMargins:NO];

    UIEdgeInsets targetInsets = [view safeAreaInsets];
    targetInsets.top = std::max(targetInsets.top, 20.0);
    [view setLayoutMargins:targetInsets];
}

- (void)viewDidLayoutSubviews
{
    [self _updateLayoutMargins];
}

-  (void)setLocation:(NSString *)locationName secure:(BOOL)secure trustedName:(BOOL)trustedName trustedSite:(BOOL)trustedSite
{
    UIColor *greenTint = [UIColor colorWithRed:100 / 255.0 green:175 / 255.0 blue:99 / 255.0 alpha:1.0];
    UIColor *whiteTint = [UIColor whiteColor];

    float hPadding = 14;
    NSString *lockImageName = @"LockMini";

    float lockSpacing = secure ? 10 : 0;

    UIEdgeInsets locationContentEdgeInsets = UIEdgeInsetsMake(0, hPadding+lockSpacing, 0, hPadding);
    UIEdgeInsets locationImageEdgeInsets = UIEdgeInsetsMake(0, -lockSpacing, 0, 0);

    if ([UIView userInterfaceLayoutDirectionForSemanticContentAttribute:[[self view] semanticContentAttribute]] == UIUserInterfaceLayoutDirectionRightToLeft) {
        locationContentEdgeInsets = mirrorEdgeInsets(locationContentEdgeInsets);
        locationImageEdgeInsets = mirrorEdgeInsets(locationImageEdgeInsets);
        [_locationButton setContentHorizontalAlignment:UIControlContentHorizontalAlignmentRight];
    }

    [_locationButton setTitleColor:(trustedName ? greenTint : whiteTint) forState:UIControlStateNormal];

    if (secure) {
        NSBundle *bundle = [NSBundle bundleForClass:[WKFullScreenWindowController class]];
        UIImage *lockImage = [UIImage imageNamed:lockImageName inBundle:bundle compatibleWithTraitCollection:nil];
        [_locationButton setImage:[lockImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
        [[_locationButton imageView] setTintColor:trustedSite ? greenTint : whiteTint];
        [_locationButton setTintColor:trustedSite ? greenTint : whiteTint];
    } else
        [_locationButton setImage:nil forState:UIControlStateNormal];

    [_locationButton setContentEdgeInsets:locationContentEdgeInsets];
    [_locationButton setImageEdgeInsets:locationImageEdgeInsets];
    [_locationButton setTitle:locationName forState:UIControlStateNormal];
    [[_locationButton titleLabel] setLineBreakMode:NSLineBreakByTruncatingTail];
    [[_locationButton titleLabel] setAdjustsFontSizeToFitWidth:NO];
}

- (void)createSubviews
{
    _visualEffectView = [self visualEffectViewWithFrame:CGRectMake(0, 0, 20, 20)];

    [_visualEffectView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [[self view] addSubview:_visualEffectView.get()];

    _cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_cancelButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_cancelButton setAdjustsImageWhenHighlighted:NO];
    [_cancelButton setBackgroundColor: [UIColor blackColor]];
    [[_cancelButton layer] setCompositingFilter:[CAFilter filterWithType:kCAFilterPlusL]];
    [_cancelButton setTitle:WEB_UI_STRING("Done", "Text of button that exits element fullscreen.") forState:UIControlStateNormal];
    [_cancelButton setTintColor:[UIColor whiteColor]];
    [_cancelButton addTarget:self action:@selector(cancelAction:) forControlEvents:UIControlEventTouchUpInside];

    [[self view] addSubview:_cancelButton.get()];

    _locationButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_locationButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_locationButton setAdjustsImageWhenHighlighted:NO];
    [_locationButton setBackgroundColor:[UIColor blackColor]];
    [[_locationButton layer] setCompositingFilter:[CAFilter filterWithType:kCAFilterPlusL]];
    [[self view] addSubview:_locationButton.get()];

    UILayoutGuide* containerGuide = [[self view] layoutMarginsGuide];

    [[[_visualEffectView leftAnchor] constraintEqualToAnchor:[[self view] leftAnchor]] setActive:YES];
    [[[_visualEffectView rightAnchor] constraintEqualToAnchor:[[self view] rightAnchor]] setActive:YES];
    [[[_visualEffectView topAnchor] constraintEqualToAnchor:[[self view] topAnchor]] setActive:YES];
    [[[_visualEffectView heightAnchor] constraintGreaterThanOrEqualToConstant:20] setActive:YES];

    NSLayoutConstraint *bottom = [[_visualEffectView bottomAnchor] constraintEqualToAnchor:[containerGuide topAnchor]];
    [bottom setPriority:UILayoutPriorityRequired - 1];
    [bottom setActive:YES];

    [[[_cancelButton leadingAnchor] constraintEqualToAnchor:[containerGuide leadingAnchor]] setActive:YES];
    [[[_cancelButton topAnchor] constraintEqualToAnchor:[[self view] topAnchor]] setActive:YES];
    [[[_cancelButton bottomAnchor] constraintEqualToAnchor:[_visualEffectView bottomAnchor]] setActive:YES];

    [[[_locationButton heightAnchor] constraintEqualToConstant:20] setActive:YES];
    [[[_locationButton bottomAnchor] constraintEqualToAnchor:[_visualEffectView bottomAnchor]] setActive:YES];
    [[[_locationButton leadingAnchor] constraintGreaterThanOrEqualToAnchor:[_cancelButton trailingAnchor]] setActive:YES];
    [[[_locationButton trailingAnchor] constraintLessThanOrEqualToAnchor:[[self view] trailingAnchor]] setActive:YES];
    NSLayoutConstraint *centeringConstraint = [[_locationButton centerXAnchor] constraintEqualToAnchor:[[self view] centerXAnchor]];
    [centeringConstraint setPriority:UILayoutPriorityDefaultLow];
    [centeringConstraint setActive:YES];

    [_visualEffectView setAlpha:0];
    [_cancelButton setAlpha:0];
    [_locationButton setAlpha:0];

    [_visualEffectView setHidden:YES];
    [_cancelButton setHidden:YES];
    [_locationButton setHidden:YES];
}

- (void)loadView
{
    [self setView:adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]).get()];
    [[self view] setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];

    _touchGestureRecognizer = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(showCancelButton:)]);
    [_touchGestureRecognizer setDelegate:self];
    [_touchGestureRecognizer setCancelsTouchesInView:NO];
    [_touchGestureRecognizer setMinimumPressDuration:0];
    [[self view] addGestureRecognizer:_touchGestureRecognizer.get()];
    [self createSubviews];
}

- (void)viewWillAppear:(BOOL)animated
{
    [[self contentView] setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [[self contentView] setFrame:[[self view] bounds]];
    [[self view] insertSubview:[self contentView] atIndex:0];
}

- (void)viewDidAppear:(BOOL)animated
{
    [self _updateLayoutMargins];
}

- (void)cancelAction:(id)sender
{
    [[self target] performSelector:[self action]];
}

- (void)hideCancelButton
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideCancelButton) object:nil];
    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [_visualEffectView setAlpha:0];
        [_cancelButton setAlpha:0];
        [_locationButton setAlpha:0];
    } completion:^(BOOL finished){
        if (finished) {
            [_cancelButton setHidden:YES];
            [_locationButton setHidden:YES];
            [_visualEffectView setHidden:YES];
        }
    }];
}

- (void)showCancelButton:(id)sender
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideCancelButton) object:nil];
    [self performSelector:@selector(hideCancelButton) withObject:nil afterDelay:autoHideDelay];
    [UIView animateWithDuration:showHideAnimationDuration animations: ^{
        [_visualEffectView setHidden:NO];
        [_cancelButton setHidden:NO];
        [_locationButton setHidden:NO];
        [_visualEffectView setAlpha:1];
        [_cancelButton setAlpha:1];
        [_locationButton setAlpha:1];
    }];
}

- (void)setTarget:(id)target action:(SEL)action
{
    [self setTarget:target];
    [self setAction:action];
}

// MARK - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

@end

@interface _WKFullscreenRootViewController : UIViewController
@end

@implementation _WKFullscreenRootViewController {
    BOOL _showsStatusBar;
}

- (void)setShowsStatusBar:(BOOL)value
{
    _showsStatusBar = value;
    [self setNeedsStatusBarAppearanceUpdate];
}

- (BOOL)prefersStatusBarHidden
{
    return !_showsStatusBar;
}

@end

@interface WKFullscreenAnimationController : NSObject <UIViewControllerAnimatedTransitioning>
@property (retain, nonatomic) UIViewController* viewController;
@property (nonatomic) CGRect initialFrame;
@property (nonatomic) CGRect finalFrame;
@property (nonatomic, getter=isAnimatingIn) BOOL animatingIn;
@end

@implementation WKFullscreenAnimationController

- (NSTimeInterval)transitionDuration:(id<UIViewControllerContextTransitioning>)transitionContext
{
    const NSTimeInterval animationDuration = 0.2;
    return animationDuration;
}

- (void)animateTransition:(id<UIViewControllerContextTransitioning>)transitionContext
{
    UIView *containerView = [transitionContext containerView];
    UIView *fromView = [transitionContext viewForKey:UITransitionContextFromViewKey];
    UIView *toView = [transitionContext viewForKey:UITransitionContextToViewKey];

    CGRect inlineFrame = _animatingIn ? _initialFrame : _finalFrame;
    CGRect fullscreenFrame = _animatingIn ? _finalFrame : _initialFrame;
    UIView *animatingView = _animatingIn ? toView : fromView;
    
    CGRect boundsRect = largestRectWithAspectRatioInsideRect(FloatRect(inlineFrame).size().aspectRatio(), fullscreenFrame);
    boundsRect.origin = CGPointZero;
    RetainPtr<UIView> maskView = adoptNS([[UIView alloc] init]);
    [maskView setBackgroundColor:[UIColor blackColor]];
    [maskView setBounds:_animatingIn ? boundsRect : [animatingView bounds]];
    [maskView setCenter:CGPointMake(CGRectGetMidX([animatingView bounds]), CGRectGetMidY([animatingView bounds]))];
    [animatingView setMaskView:maskView.get()];
    
    FloatRect scaleRect = smallestRectWithAspectRatioAroundRect(FloatRect(fullscreenFrame).size().aspectRatio(), inlineFrame);
    CGAffineTransform scaleTransform = CGAffineTransformMakeScale(scaleRect.width() / fullscreenFrame.size.width, scaleRect.height() / fullscreenFrame.size.height);
    CGAffineTransform translateTransform = CGAffineTransformMakeTranslation(CGRectGetMidX(inlineFrame) - CGRectGetMidX(fullscreenFrame), CGRectGetMidY(inlineFrame) - CGRectGetMidY(fullscreenFrame));
    
    CGAffineTransform finalTransform = CGAffineTransformConcat(scaleTransform, translateTransform);

    [animatingView setTransform:_animatingIn ? finalTransform : CGAffineTransformIdentity];
    
    [containerView addSubview:animatingView];

    [UIView animateWithDuration:[self transitionDuration:transitionContext] delay:0 options:UIViewAnimationOptionCurveEaseInOut animations:^{
        [animatingView setTransform:_animatingIn ? CGAffineTransformIdentity : finalTransform];
        [maskView setBounds:_animatingIn ? animatingView.bounds : boundsRect];
        [maskView setCenter:CGPointMake(CGRectGetMidX([animatingView bounds]), CGRectGetMidY([animatingView bounds]))];
    } completion:^(BOOL finished){
        BOOL success = ![transitionContext transitionWasCancelled];

        if (([self isAnimatingIn] && !success) || (![self isAnimatingIn] && success))
            [animatingView removeFromSuperview];

        [transitionContext completeTransition:success];
        [animatingView setMaskView:nil];
    }];
}

- (void)animationEnded:(BOOL)transitionCompleted
{
}

@end

@implementation WKFullScreenWindowController {
    WKWebView *_webView; // Cannot be retained, see <rdar://problem/14884666>.
    RetainPtr<UIView> _webViewPlaceholder;

    FullScreenState _fullScreenState;
    WKWebViewState _viewState;

    RetainPtr<UIWindow> _window;
    RetainPtr<_WKFullscreenRootViewController> _rootViewController;

    RefPtr<WebKit::VoidCallback> _repaintCallback;
    RetainPtr<UIViewController> _viewControllerForPresentation;
    RetainPtr<_WKFullScreenViewController> _fullscreenViewController;

    CGRect _initialFrame;
    CGRect _finalFrame;

    RetainPtr<NSString> _EVOrganizationName;
    BOOL _EVOrganizationNameIsValid;

}

#pragma mark -
#pragma mark Initialization
- (id)initWithWebView:(WKWebView *)webView
{
    if (![super init])
        return nil;
    _webView = webView;

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
    return _fullScreenState == WaitingToEnterFullScreen
        || _fullScreenState == EnteringFullScreen
        || _fullScreenState == InFullScreen;
}

- (WebCoreFullScreenPlaceholderView *)webViewPlaceholder
{
    return nil;
}

#pragma mark -
#pragma mark Exposed Interface

- (void)_invalidateEVOrganizationName
{
    _EVOrganizationName = nil;
    _EVOrganizationNameIsValid = NO;
}

- (BOOL)isSecure
{
    return _webView.hasOnlySecureContent;
}

- (SecTrustRef)_serverTrust
{
    return _webView.serverTrust;
}

- (NSString *)_EVOrganizationName
{
    if (!self.isSecure)
        return nil;

    if (_EVOrganizationNameIsValid)
        return _EVOrganizationName.get();

    ASSERT(!_EVOrganizationName.get());
    _EVOrganizationNameIsValid = YES;

    SecTrustRef trust = [self _serverTrust];
    if (!trust)
        return nil;

    NSDictionary *infoDictionary = [(__bridge NSDictionary *)SecTrustCopyInfo(trust) autorelease];
    // If SecTrustCopyInfo returned NULL then it's likely that the SecTrustRef has not been evaluated
    // and the only way to get the information we need is to call SecTrustEvaluate ourselves.
    if (!infoDictionary) {
        OSStatus err = SecTrustEvaluate(trust, NULL);
        if (err == noErr)
            infoDictionary = [(__bridge NSDictionary *)SecTrustCopyInfo(trust) autorelease];
        if (!infoDictionary)
            return nil;
    }

    // Make sure that the EV certificate is valid against our certificate chain.
    id hasEV = [infoDictionary objectForKey:(__bridge NSString *)kSecTrustInfoExtendedValidationKey];
    if (![hasEV isKindOfClass:[NSValue class]] || ![hasEV boolValue])
        return nil;

    // Make sure that we could contact revocation server and it is still valid.
    id isNotRevoked = [infoDictionary objectForKey:(__bridge NSString *)kSecTrustInfoRevocationKey];
    if (![isNotRevoked isKindOfClass:[NSValue class]] || ![isNotRevoked boolValue])
        return nil;

    _EVOrganizationName = [infoDictionary objectForKey:(__bridge NSString *)kSecTrustInfoCompanyNameKey];
    return _EVOrganizationName.get();
}

- (void)updateLocationInfo
{
    NSURL* url = _webView._committedURL;

    NSString *EVOrganizationName = [self _EVOrganizationName];
    BOOL showsEVOrganizationName = [EVOrganizationName length] > 0;

    NSString *domain = nil;

    if (LinkPresentationLibrary())
        domain = [url _lp_simplifiedDisplayString];
    else
        domain = userVisibleString(url);

    NSString *text = nil;
    if ([[url scheme] caseInsensitiveCompare:@"data"] == NSOrderedSame)
        text = @"data:";
    else if (showsEVOrganizationName)
        text = EVOrganizationName;
    else
        text = domain;

    [_fullscreenViewController setLocation:text secure:self.isSecure trustedName:showsEVOrganizationName trustedSite:!!EVOrganizationName];
}

- (void)enterFullScreen
{
    if ([self isFullScreen])
        return;

    [self _invalidateEVOrganizationName];

    _fullScreenState = WaitingToEnterFullScreen;

    _window = adoptNS([[UIWindow alloc] init]);
    [_window setBackgroundColor:[UIColor clearColor]];
    _rootViewController = adoptNS([[_WKFullscreenRootViewController alloc] init]);
    [_window setRootViewController:_rootViewController.get()];
    [[_window rootViewController] setView:adoptNS([[UIView alloc] initWithFrame:[_window bounds]]).get()];
    [[[_window rootViewController] view] setBackgroundColor:[UIColor clearColor]];
    [[[_window rootViewController] view] setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [_window setWindowLevel:UIWindowLevelNormal - 1];
    [_window setHidden:NO];
    _viewControllerForPresentation = [_window rootViewController];

    _fullscreenViewController = adoptNS([[_WKFullScreenViewController alloc] init]);
    [_fullscreenViewController setTransitioningDelegate:self];
    [_fullscreenViewController setModalPresentationStyle:UIModalPresentationCustom];
    [_fullscreenViewController setTarget:self action:@selector(requestExitFullScreen)];
    [[_fullscreenViewController view] setFrame:[[_viewControllerForPresentation view] bounds]];
    [self updateLocationInfo];

    [self _manager]->saveScrollPosition();

    [_webView _page]->setSuppressVisibilityUpdates(true);

    _viewState.store(_webView);

    _webViewPlaceholder = adoptNS([[UIView alloc] init]);
    [[_webViewPlaceholder layer] setName:@"Fullscreen Placeholder Vfiew"];

    WKSnapshotConfiguration* config = nil;
    [_webView takeSnapshotWithConfiguration:config completionHandler:^(UIImage * snapshotImage, NSError * error){
        if (![_webView _page])
            return;

        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        
        [[_webViewPlaceholder layer] setContents:(id)[snapshotImage CGImage]];
        replaceViewWithView(_webView, _webViewPlaceholder.get());

        WKWebViewState().applyTo(_webView);
        
        [_webView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
        [_webView setFrame:[_window bounds]];
        [_window insertSubview:_webView atIndex:0];
        [_webView _overrideLayoutParametersWithMinimumLayoutSize:[_window bounds].size maximumUnobscuredSizeOverride:[_window bounds].size];

        [_webView setNeedsLayout];
        [_webView layoutIfNeeded];
        
        [self _manager]->setAnimatingFullScreen(true);

        _repaintCallback = VoidCallback::create([protectedSelf = retainPtr(self), self](WebKit::CallbackBase::Error) {
            _repaintCallback = nullptr;
            if (![_webView _page])
                return;

            [protectedSelf _manager]->willEnterFullScreen();
        });
        [_webView _page]->forceRepaint(_repaintCallback.copyRef());

        [CATransaction commit];
    }];
}

- (void)beganEnterFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WaitingToEnterFullScreen)
        return;
    _fullScreenState = EnteringFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [_webView removeFromSuperview];
    [_fullscreenViewController setContentView:_webView];

    [_window setWindowLevel:UIWindowLevelNormal];
    [_window makeKeyAndVisible];
    [_rootViewController setShowsStatusBar:YES];

    [CATransaction commit];

    [_viewControllerForPresentation presentViewController:_fullscreenViewController.get() animated:YES completion:^{
        [self completedEnterFullScreen];
        [_rootViewController setShowsStatusBar:NO];
    }];
}

- (void)completedEnterFullScreen
{
    if (![_webView _page])
        return;

    _fullScreenState = InFullScreen;

    [self _manager]->didEnterFullScreen();
    [self _manager]->setAnimatingFullScreen(false);

    [_webView _page]->setSuppressVisibilityUpdates(false);
}

- (void)exitFullScreen
{
    if (![self isFullScreen])
        return;
    _fullScreenState = WaitingToExitFullScreen;

    [self _manager]->setAnimatingFullScreen(true);
    [self _manager]->willExitFullScreen();
}

- (void)requestExitFullScreen
{
    [self _manager]->requestExitFullScreen();
}

- (void)beganExitFullScreenWithInitialFrame:(CGRect)initialFrame finalFrame:(CGRect)finalFrame
{
    if (_fullScreenState != WaitingToExitFullScreen)
        return;
    _fullScreenState = ExitingFullScreen;

    _initialFrame = initialFrame;
    _finalFrame = finalFrame;
    
    [_webView _page]->setSuppressVisibilityUpdates(true);

    [_rootViewController setShowsStatusBar:YES];
    [_fullscreenViewController dismissViewControllerAnimated:YES completion:^{
        if (![_webView _page])
            return;

        [self completedExitFullScreen];
        [_rootViewController setShowsStatusBar:NO];
    }];
}

- (void)completedExitFullScreen
{
    if (_fullScreenState != ExitingFullScreen)
        return;
    _fullScreenState = NotInFullScreen;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [[_webViewPlaceholder superview] insertSubview:_webView belowSubview:_webViewPlaceholder.get()];
    [_webView setFrame:[_webViewPlaceholder frame]];
    [_webView setAutoresizingMask:[_webViewPlaceholder autoresizingMask]];

    [[_webView window] makeKeyAndVisible];

    _viewState.applyTo(_webView);

    [_webView setNeedsLayout];
    [_webView layoutIfNeeded];

    [CATransaction commit];

    [_window setHidden:YES];
    _window = nil;

    [self _manager]->setAnimatingFullScreen(false);
    [self _manager]->didExitFullScreen();

    if (_repaintCallback) {
        _repaintCallback->invalidate(WebKit::CallbackBase::Error::OwnerWasInvalidated);
        ASSERT(!_repaintCallback);
    }

    _repaintCallback = VoidCallback::create([protectedSelf = retainPtr(self), self](WebKit::CallbackBase::Error) {
        _repaintCallback = nullptr;
        [_webViewPlaceholder removeFromSuperview];

        if (![_webView _page])
            return;

        [_webView _page]->setSuppressVisibilityUpdates(false);
    });

    [_webView _page]->forceRepaint(_repaintCallback.copyRef());
}

- (void)exitFullscreenImmediately
{
    if (![self isFullScreen])
        return;

    if (![_webView _page])
        return;

    [self _manager]->requestExitFullScreen();
    [self exitFullScreen];
    _fullScreenState = ExitingFullScreen;
    [self completedExitFullScreen];
    replaceViewWithView(_webViewPlaceholder.get(), _webView);
    [_webView _page]->setSuppressVisibilityUpdates(false);
    [self _manager]->didExitFullScreen();
    [self _manager]->setAnimatingFullScreen(false);
    _webViewPlaceholder = nil;
}

- (void)close
{
    [self exitFullscreenImmediately];
    _webView = nil;
}

- (void)webViewDidRemoveFromSuperviewWhileInFullscreen
{
    if (_fullScreenState == InFullScreen && _webView.window != _window.get())
        [self exitFullscreenImmediately];
}

#pragma mark -
#pragma mark Internal Interface

- (WebFullScreenManagerProxy*)_manager
{
    if (![_webView _page])
        return nullptr;
    return [_webView _page]->fullScreenManager();
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
    RetainPtr<WKFullscreenAnimationController> animationController = adoptNS([[WKFullscreenAnimationController alloc] init]);
    [animationController setViewController:dismissed];
    [animationController setInitialFrame:_initialFrame];
    [animationController setFinalFrame:_finalFrame];
    [animationController setAnimatingIn:NO];
    return animationController.autorelease();
}

@end

#endif // PLATFORM(IOS) && ENABLE(FULLSCREEN_API)
