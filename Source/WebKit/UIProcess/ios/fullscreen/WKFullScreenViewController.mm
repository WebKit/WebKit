/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
#import "WKFullScreenViewController.h"

#import "FullscreenTouchSecheuristic.h"
#import "PlaybackSessionManagerProxy.h"
#import "UIKitSPI.h"
#import "VideoFullscreenManagerProxy.h"
#import "WKFullscreenStackView.h"
#import "WKWebViewIOS.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

static const NSTimeInterval showHideAnimationDuration = 0.1;
static const NSTimeInterval pipHideAnimationDuration = 0.2;
static const NSTimeInterval autoHideDelay = 4.0;

@class WKFullscreenStackView;

class WKFullScreenViewControllerPlaybackSessionModelClient : WebCore::PlaybackSessionModelClient {
public:
    void setParent(WKFullScreenViewController *parent) { m_parent = parent; }

    void rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState> playbackState, double /* playbackRate */, double /* defaultPlaybackRate */) override
    {
        if (auto *controller = m_parent.getAutoreleased())
            controller.playing = playbackState.contains(WebCore::PlaybackSessionModel::PlaybackState::Playing);
    }

    void isPictureInPictureSupportedChanged(bool) override
    {
    }

    void pictureInPictureActiveChanged(bool active) override
    {
        if (auto *controller = m_parent.getAutoreleased())
            controller.pictureInPictureActive = active;
    }

    void setInterface(WebCore::PlaybackSessionInterfaceAVKit* interface)
    {
        if (m_interface == interface)
            return;

        if (m_interface && m_interface->playbackSessionModel())
            m_interface->playbackSessionModel()->removeClient(*this);
        m_interface = interface;
        if (m_interface && m_interface->playbackSessionModel())
            m_interface->playbackSessionModel()->addClient(*this);
    }

private:
    WeakObjCPtr<WKFullScreenViewController> m_parent;
    RefPtr<WebCore::PlaybackSessionInterfaceAVKit> m_interface;
};

#pragma mark - _WKExtrinsicButton

@interface _WKExtrinsicButton : UIButton
@property (assign, nonatomic) CGSize extrinsicContentSize;
@end

@implementation _WKExtrinsicButton
- (void)setExtrinsicContentSize:(CGSize)size
{
    _extrinsicContentSize = size;
    [self invalidateIntrinsicContentSize];
}

- (CGSize)intrinsicContentSize
{
    return _extrinsicContentSize;
}
@end

#pragma mark - _WKInsetLabel

@interface _WKInsetLabel : UILabel
@property (assign, nonatomic) UIEdgeInsets edgeInsets;
@end

@implementation _WKInsetLabel
- (void)drawTextInRect:(CGRect)rect
{
    [super drawTextInRect:UIEdgeInsetsInsetRect(rect, self.edgeInsets)];
}

- (CGSize)intrinsicContentSize
{
    auto intrinsicSize = [super intrinsicContentSize];
    intrinsicSize.width += self.edgeInsets.left + self.edgeInsets.right;
    intrinsicSize.height += self.edgeInsets.top + self.edgeInsets.bottom;
    return intrinsicSize;
}
@end

#pragma mark - WKFullScreenViewController

@interface WKFullScreenViewController () <UIGestureRecognizerDelegate, UIToolbarDelegate>
@property (weak, nonatomic) WKWebView *_webView; // Cannot be retained, see <rdar://problem/14884666>.
@property (readonly, nonatomic) WebKit::WebFullScreenManagerProxy* _manager;
@property (readonly, nonatomic) WebCore::FloatBoxExtent _effectiveFullscreenInsets;
@end

@implementation WKFullScreenViewController {
    BOOL _valid;
    RetainPtr<UILongPressGestureRecognizer> _touchGestureRecognizer;
    RetainPtr<UIView> _animatingView;
    RetainPtr<UIStackView> _stackView;
    RetainPtr<UIStackView> _banner;
    RetainPtr<_WKInsetLabel> _bannerLabel;
    RetainPtr<_WKExtrinsicButton> _cancelButton;
    RetainPtr<_WKExtrinsicButton> _pipButton;
    RetainPtr<UIButton> _locationButton;
    RetainPtr<UILayoutGuide> _topGuide;
    RetainPtr<NSLayoutConstraint> _topConstraint;
    String _location;
    WebKit::FullscreenTouchSecheuristic _secheuristic;
    WKFullScreenViewControllerPlaybackSessionModelClient _playbackClient;
    CGFloat _nonZeroStatusBarHeight;
    std::optional<UIInterfaceOrientationMask> _supportedOrientations;
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    BOOL m_shouldHideMediaControls;
#endif
}

@synthesize prefersStatusBarHidden = _prefersStatusBarHidden;
@synthesize prefersHomeIndicatorAutoHidden = _prefersHomeIndicatorAutoHidden;

#pragma mark - External Interface

- (id)initWithWebView:(WKWebView *)webView
{
    self = [super init];
    if (!self)
        return nil;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    _nonZeroStatusBarHeight = UIApplication.sharedApplication.statusBarFrame.size.height;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_statusBarFrameDidChange:) name:UIApplicationDidChangeStatusBarFrameNotification object:nil];
ALLOW_DEPRECATED_DECLARATIONS_END
    _secheuristic.setParameters(WebKit::FullscreenTouchSecheuristicParameters::iosParameters());
    self._webView = webView;

    _playbackClient.setParent(self);
    _valid = YES;
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    m_shouldHideMediaControls = NO;
#endif

    return self;
}

- (void)invalidate
{
    if (!_valid)
        return;

    _valid = NO;

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideBanner) object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    _playbackClient.setParent(nullptr);
    _playbackClient.setInterface(nullptr);
}

- (void)dealloc
{
    [self invalidate];

    [_target release];

    [super dealloc];
}

- (void)setSupportedOrientations:(UIInterfaceOrientationMask)supportedOrientations
{
    _supportedOrientations = supportedOrientations;
    [self setNeedsUpdateOfSupportedInterfaceOrientations];
}

- (void)resetSupportedOrientations
{
    _supportedOrientations = std::nullopt;
    [self setNeedsUpdateOfSupportedInterfaceOrientations];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    if (!_supportedOrientations)
        return [super supportedInterfaceOrientations];
    return *_supportedOrientations;
}

- (void)showUI
{
    ASSERT(_valid);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];

    if (_playing) {
        NSTimeInterval hideDelay = autoHideDelay;
        [self performSelector:@selector(hideUI) withObject:nil afterDelay:hideDelay];
    }
    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [_stackView setHidden:NO];
        [_stackView setAlpha:1];
        self.prefersStatusBarHidden = NO;
        self.prefersHomeIndicatorAutoHidden = NO;
        if (_topConstraint)
            [NSLayoutConstraint deactivateConstraints:@[_topConstraint.get()]];
        _topConstraint = [[_topGuide topAnchor] constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor];
        [_topConstraint setActive:YES];
        if (auto* manager = self._manager)
            manager->setFullscreenControlsHidden(false);
    }];
}

- (void)hideUI
{
    ASSERT(_valid);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
    [UIView animateWithDuration:showHideAnimationDuration animations:^{

        if (_topConstraint)
            [NSLayoutConstraint deactivateConstraints:@[_topConstraint.get()]];
        _topConstraint = [[_topGuide topAnchor] constraintEqualToAnchor:self.view.topAnchor constant:self.view.safeAreaInsets.top];
        [_topConstraint setActive:YES];
        [_stackView setAlpha:0];
        self.prefersStatusBarHidden = YES;
        self.prefersHomeIndicatorAutoHidden = YES;
        if (auto* manager = self._manager)
            manager->setFullscreenControlsHidden(true);
    } completion:^(BOOL finished) {
        if (!finished)
            return;

        [_stackView setHidden:YES];
    }];
}

- (void)showBanner
{
    ASSERT(_valid);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideBanner) object:nil];

    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [_banner setHidden:NO];
        [_banner setAlpha:1];
    }];

    [self performSelector:@selector(hideBanner) withObject:nil afterDelay:autoHideDelay];
}

- (void)hideBanner
{
    ASSERT(_valid);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideBanner) object:nil];
    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [_banner setAlpha:0];
    } completion:^(BOOL finished) {
        if (!finished)
            return;

        [_banner setHidden:YES];
    }];
}

- (void)videoControlsManagerDidChange
{
    ASSERT(_valid);
    auto page = [self._webView _page];
    auto* videoFullscreenManager = page ? page->videoFullscreenManager() : nullptr;
    auto* videoFullscreenInterface = videoFullscreenManager ? videoFullscreenManager->controlsManagerInterface() : nullptr;
    auto* playbackSessionInterface = videoFullscreenInterface ? &videoFullscreenInterface->playbackSessionInterface() : nullptr;

    _playbackClient.setInterface(playbackSessionInterface);

    WebCore::PlaybackSessionModel* playbackSessionModel = playbackSessionInterface ? playbackSessionInterface->playbackSessionModel() : nullptr;
    self.playing = playbackSessionModel ? playbackSessionModel->isPlaying() : NO;
    bool isPiPEnabled = false;
    if (auto page = [self._webView _page])
        isPiPEnabled = page->preferences().pictureInPictureAPIEnabled() && page->preferences().allowsPictureInPictureMediaPlayback();
    bool isPiPSupported = playbackSessionModel && playbackSessionModel->isPictureInPictureSupported();
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    [_cancelButton setHidden:m_shouldHideMediaControls];
    isPiPEnabled = !m_shouldHideMediaControls && isPiPEnabled;
#endif
    [_pipButton setHidden:!isPiPEnabled || !isPiPSupported];
}

- (void)setAnimatingViewAlpha:(CGFloat)alpha
{
    ASSERT(_valid);
    [UIView animateWithDuration:pipHideAnimationDuration animations:^{
        _animatingView.get().alpha = alpha;
    }];
}

#if HAVE(UIKIT_WEBKIT_INTERNALS)
- (void)hideMediaControls:(BOOL)hidden
{
    if (m_shouldHideMediaControls == hidden)
        return;

    m_shouldHideMediaControls = hidden;
    [self videoControlsManagerDidChange];
}
#endif // HAVE(UIKIT_WEBKIT_INTERNALS)

- (void)setPrefersStatusBarHidden:(BOOL)value
{
    ASSERT(_valid);
    _prefersStatusBarHidden = value;
    [self setNeedsStatusBarAppearanceUpdate];
    [self _updateWebViewFullscreenInsets];
}

- (void)setPrefersHomeIndicatorAutoHidden:(BOOL)value
{
    ASSERT(_valid);
    _prefersHomeIndicatorAutoHidden = value;
    [self setNeedsUpdateOfHomeIndicatorAutoHidden];
}

- (void)setPlaying:(BOOL)isPlaying
{
    ASSERT(_valid);
    if (_playing == isPlaying)
        return;

    _playing = isPlaying;

    if (![self viewIfLoaded])
        return;

    if (!_playing)
        [self showUI];
    else {
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
        NSTimeInterval hideDelay = autoHideDelay;
        [self performSelector:@selector(hideUI) withObject:nil afterDelay:hideDelay];
    }
}

- (void)setPictureInPictureActive:(BOOL)active
{
    ASSERT(_valid);
    if (_pictureInPictureActive == active)
        return;

    _pictureInPictureActive = active;
    [_pipButton setSelected:active];
}

- (void)setAnimating:(BOOL)animating
{
    ASSERT(_valid);
    if (_animating == animating)
        return;
    _animating = animating;

    if (![self viewIfLoaded])
        return

    [self setNeedsStatusBarAppearanceUpdate];

    if (_animating)
        [self hideUI];
    else
        [self showUI];
}

- (NSString *)location
{
    return _location;
}

- (void)setLocation:(NSString *)location
{
    _location = location;

    [_bannerLabel setText:[NSString stringWithFormat:WEB_UI_NSSTRING(@"”%@” is in full screen.\nSwipe down to exit.", "Full Screen Warning Banner Content Text"), (NSString *)self.location]];
}

#pragma mark - UIViewController Overrides

- (void)loadView
{
    CGSize buttonSize;
    UIImage *doneImage;
    UIImage *startPiPImage;
    UIImage *stopPiPImage;
    auto alternateFullScreenControlDesignEnabled = self._webView._page->preferences().alternateFullScreenControlDesignEnabled();
    
    if (alternateFullScreenControlDesignEnabled) {
        buttonSize = CGSizeMake(38.0, 38.0);
        doneImage = [UIImage systemImageNamed:@"arrow.down.right.and.arrow.up.left"];
        startPiPImage = [UIImage systemImageNamed:@"pip.enter"];
        stopPiPImage = [UIImage systemImageNamed:@"pip.exit"];
    } else {
        buttonSize = CGSizeMake(60.0, 47.0);
        NSBundle *bundle = [NSBundle bundleForClass:self.class];
        doneImage = [UIImage imageNamed:@"Done" inBundle:bundle compatibleWithTraitCollection:nil];
        startPiPImage = [UIImage imageNamed:@"StartPictureInPictureButton" inBundle:bundle compatibleWithTraitCollection:nil];
        stopPiPImage = [UIImage imageNamed:@"StopPictureInPictureButton" inBundle:bundle compatibleWithTraitCollection:nil];
    }
    
    [self setView:adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]).get()];
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.backgroundColor = [UIColor blackColor];

    _animatingView = adoptNS([[UIView alloc] initWithFrame:self.view.bounds]);
    _animatingView.get().autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:_animatingView.get()];

    _cancelButton = [_WKExtrinsicButton buttonWithType:UIButtonTypeSystem];
    [_cancelButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_cancelButton setAdjustsImageWhenHighlighted:NO];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [_cancelButton setExtrinsicContentSize:buttonSize];
    
    [_cancelButton setImage:[doneImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    [_cancelButton setTintColor:[UIColor whiteColor]];
    [_cancelButton sizeToFit];
    [_cancelButton addTarget:self action:@selector(_cancelAction:) forControlEvents:UIControlEventTouchUpInside];

    _pipButton = [_WKExtrinsicButton buttonWithType:UIButtonTypeSystem];
    [_pipButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [_pipButton setAdjustsImageWhenHighlighted:NO];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [_pipButton setExtrinsicContentSize:buttonSize];
    
    [_pipButton setImage:[startPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    [_pipButton setImage:[stopPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateSelected];
    [_pipButton setTintColor:[UIColor whiteColor]];
    [_pipButton sizeToFit];
    [_pipButton addTarget:self action:@selector(_togglePiPAction:) forControlEvents:UIControlEventTouchUpInside];

    if (alternateFullScreenControlDesignEnabled) {
        UIButtonConfiguration *cancelButtonConfiguration = [UIButtonConfiguration filledButtonConfiguration];
        // FIXME: this color specification should not be necessary.
        cancelButtonConfiguration.baseBackgroundColor = [UIColor colorWithWhite:1.0 alpha:0.15];
        [_cancelButton setConfiguration:cancelButtonConfiguration];
        
        UIButtonConfiguration *pipButtonConfiguration = [UIButtonConfiguration filledButtonConfiguration];
        // FIXME: this color specification should not be necessary.
        pipButtonConfiguration.baseBackgroundColor = [UIColor colorWithWhite:1.0 alpha:0.15];
        [_pipButton setConfiguration:pipButtonConfiguration];
        
        _stackView = adoptNS([[UIStackView alloc] init]);
        [_stackView addArrangedSubview:_cancelButton.get()];
        [_stackView addArrangedSubview:_pipButton.get()];
        [_stackView setSpacing:24.0];
    } else {
        RetainPtr<WKFullscreenStackView> stackView = adoptNS([[WKFullscreenStackView alloc] init]);
        [stackView addArrangedSubview:_cancelButton.get() applyingMaterialStyle:AVBackgroundViewMaterialStyleSecondary tintEffectStyle:AVBackgroundViewTintEffectStyleSecondary];
        [stackView addArrangedSubview:_pipButton.get() applyingMaterialStyle:AVBackgroundViewMaterialStylePrimary tintEffectStyle:AVBackgroundViewTintEffectStyleSecondary];
        _stackView = WTFMove(stackView);
    }

    [_stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_animatingView addSubview:_stackView.get()];

    _bannerLabel = adoptNS([[_WKInsetLabel alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    [_bannerLabel setEdgeInsets:UIEdgeInsetsMake(16, 16, 16, 16)];
    [_bannerLabel setBackgroundColor:[UIColor clearColor]];
    [_bannerLabel setNumberOfLines:0];
    [_bannerLabel setLineBreakMode:NSLineBreakByWordWrapping];
    [_bannerLabel setTextAlignment:NSTextAlignmentCenter];
    [_bannerLabel setText:[NSString stringWithFormat:WEB_UI_NSSTRING(@"”%@” is in full screen.\nSwipe down to exit.", "Full Screen Warning Banner Content Text"), (NSString *)self.location]];

    auto banner = adoptNS([[WKFullscreenStackView alloc] init]);
    [banner addArrangedSubview:_bannerLabel.get() applyingMaterialStyle:AVBackgroundViewMaterialStyleSecondary tintEffectStyle:AVBackgroundViewTintEffectStyleSecondary];
    _banner = WTFMove(banner);
    [_banner setTranslatesAutoresizingMaskIntoConstraints:NO];

    [_animatingView addSubview:_banner.get()];

    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    UILayoutGuide *margins = self.view.layoutMarginsGuide;

    _topGuide = adoptNS([[UILayoutGuide alloc] init]);
    [self.view addLayoutGuide:_topGuide.get()];
    NSLayoutYAxisAnchor *topAnchor = [_topGuide topAnchor];
    NSLayoutConstraint *stackViewToTopGuideConstraint;
    if (alternateFullScreenControlDesignEnabled)
        stackViewToTopGuideConstraint = [[_stackView topAnchor] constraintEqualToSystemSpacingBelowAnchor:topAnchor multiplier:2];
    else
        stackViewToTopGuideConstraint = [[_stackView topAnchor] constraintEqualToAnchor:topAnchor];
    _topConstraint = [topAnchor constraintEqualToAnchor:safeArea.topAnchor];
    [NSLayoutConstraint activateConstraints:@[
        _topConstraint.get(),
        stackViewToTopGuideConstraint,
        [[_banner centerYAnchor] constraintEqualToAnchor:margins.centerYAnchor],
        [[_banner centerXAnchor] constraintEqualToAnchor:margins.centerXAnchor],
        [[_banner widthAnchor] constraintEqualToAnchor:margins.widthAnchor],
        [[_stackView leadingAnchor] constraintEqualToAnchor:margins.leadingAnchor],
    ]];

    [_stackView setAlpha:0];
    [_stackView setHidden:YES];
    [self videoControlsManagerDidChange];

    [_banner setAlpha:0];
    [_banner setHidden:YES];

    _touchGestureRecognizer = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(_touchDetected:)]);
    [_touchGestureRecognizer setCancelsTouchesInView:NO];
    [_touchGestureRecognizer setMinimumPressDuration:0];
    [_touchGestureRecognizer setDelegate:self];
    [self.view addGestureRecognizer:_touchGestureRecognizer.get()];
}

- (void)viewWillAppear:(BOOL)animated
{
    self._webView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self._webView.frame = self.view.bounds;
    [_animatingView insertSubview:self._webView atIndex:0];

    if (auto* manager = self._manager)
        manager->setFullscreenAutoHideDuration(Seconds(showHideAnimationDuration));

    [super viewWillAppear:animated];
}

#if HAVE(UIKIT_WEBKIT_INTERNALS)
- (void)viewIsAppearing:(BOOL)animated
{
    self.view.clipsToBounds = YES;
    self.view._continuousCornerRadius = self.view.window._continuousCornerRadius;

    [super viewIsAppearing:animated];
}
#endif

- (void)viewDidLayoutSubviews
{
    [self _updateWebViewFullscreenInsets];
    _secheuristic.setSize(self.view.bounds.size);
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self._webView _beginAnimatedResizeWithUpdates:^{
            [self._webView _overrideLayoutParametersWithMinimumLayoutSize:size maximumUnobscuredSizeOverride:size];
        }];
 ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [self._webView _setInterfaceOrientationOverride:[UIApp statusBarOrientation]];
 ALLOW_DEPRECATED_DECLARATIONS_END
    } completion:^(id <UIViewControllerTransitionCoordinatorContext>context) {
        [self._webView _endAnimatedResize];
    }];
}

- (UIStatusBarStyle)preferredStatusBarStyle
{
    return UIStatusBarStyleLightContent;
}

- (BOOL)prefersStatusBarHidden
{
    return _animating || _prefersStatusBarHidden;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    if (!self.animating)
        [self showUI];
    return YES;
}

#pragma mark - Internal Interface

@dynamic _manager;
- (WebKit::WebFullScreenManagerProxy*)_manager
{
    ASSERT(_valid);
    if (auto page = [self._webView _page])
        return page->fullScreenManager();
    return nullptr;
}

@dynamic _effectiveFullscreenInsets;
- (WebCore::FloatBoxExtent)_effectiveFullscreenInsets
{
    ASSERT(_valid);
    auto safeAreaInsets = self.view.safeAreaInsets;
    WebCore::FloatBoxExtent insets { safeAreaInsets.top, safeAreaInsets.right, safeAreaInsets.bottom, safeAreaInsets.left };

    CGRect cancelFrame = _cancelButton.get().frame;
    CGPoint maxXY = CGPointMake(CGRectGetMaxX(cancelFrame), CGRectGetMaxY(cancelFrame));
    insets.setTop([_cancelButton convertPoint:maxXY toView:self.view].y);
    return insets;
}

- (void)_cancelAction:(id)sender
{
    ASSERT(_valid);
    [[self target] performSelector:[self exitFullScreenAction]];
}

- (void)_togglePiPAction:(id)sender
{
    ASSERT(_valid);
    auto page = [self._webView _page];
    if (!page)
        return;

    WebKit::PlaybackSessionManagerProxy* playbackSessionManager = page->playbackSessionManager();
    if (!playbackSessionManager)
        return;

    PlatformPlaybackSessionInterface* playbackSessionInterface = playbackSessionManager->controlsManagerInterface();
    if (!playbackSessionInterface)
        return;

    WebCore::PlaybackSessionModel* playbackSessionModel = playbackSessionInterface->playbackSessionModel();
    if (!playbackSessionModel)
        return;

    playbackSessionModel->togglePictureInPicture();
}

- (void)_touchDetected:(id)sender
{
    ASSERT(_valid);
    if ([_touchGestureRecognizer state] == UIGestureRecognizerStateEnded && !self._webView._shouldAvoidSecurityHeuristicScoreUpdates) {
        double score = _secheuristic.scoreOfNextTouch([_touchGestureRecognizer locationInView:self.view]);
        if (score > _secheuristic.requiredScore())
            [self _showPhishingAlert];
    }
    if (!self.animating)
        [self showUI];
}

- (void)_statusBarFrameDidChange:(NSNotificationCenter *)notification
{
    ASSERT(_valid);
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGFloat height = UIApplication.sharedApplication.statusBarFrame.size.height;
ALLOW_DEPRECATED_DECLARATIONS_END
    if (!height || height == _nonZeroStatusBarHeight)
        return;

    _nonZeroStatusBarHeight = height;
    [self _updateWebViewFullscreenInsets];
}

- (void)_updateWebViewFullscreenInsets
{
    ASSERT(_valid);
    if (auto* manager = self._manager)
        manager->setFullscreenInsets(self._effectiveFullscreenInsets);
}

- (void)_showPhishingAlert
{
    ASSERT(_valid);
    NSString *alertTitle = WEB_UI_STRING("It looks like you are typing while in full screen", "Full Screen Deceptive Website Warning Sheet Title");
    NSString *alertMessage = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Typing is not allowed in full screen websites. “%@” may be showing a fake keyboard to trick you into disclosing personal or financial information.", "Full Screen Deceptive Website Warning Sheet Content Text"), (NSString *)self.location];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:alertTitle message:alertMessage preferredStyle:UIAlertControllerStyleAlert];

    if (auto page = [self._webView _page]) {
        page->suspendAllMediaPlayback([] { });
        page->suspendActiveDOMObjectsAndAnimations();
    }

    UIAlertAction* exitAction = [UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("Exit Full Screen", "Exit Full Screen (Element Full Screen)", "Full Screen Deceptive Website Exit Action") style:UIAlertActionStyleCancel handler:^(UIAlertAction * action) {
        [self _cancelAction:action];
        if (auto page = [self._webView _page]) {
            page->resumeActiveDOMObjectsAndAnimations();
            page->resumeAllMediaPlayback([] { });
        }
    }];

    UIAlertAction* stayAction = [UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("Stay in Full Screen", "Stay in Full Screen (Element Full Screen)", "Full Screen Deceptive Website Stay Action") style:UIAlertActionStyleDefault handler:^(UIAlertAction * action) {
        if (auto page = [self._webView _page]) {
            page->resumeActiveDOMObjectsAndAnimations();
            page->resumeAllMediaPlayback([] { });
        }
        _secheuristic.reset();
    }];

    [alert addAction:exitAction];
    [alert addAction:stayAction];
    [self presentViewController:alert animated:YES completion:nil];
}

@end

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
