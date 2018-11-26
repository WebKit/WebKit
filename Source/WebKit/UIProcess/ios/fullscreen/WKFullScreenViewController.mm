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
#import "WKWebViewInternal.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;
using namespace WebKit;

static const NSTimeInterval showHideAnimationDuration = 0.1;
static const NSTimeInterval pipHideAnimationDuration = 0.2;
static const NSTimeInterval autoHideDelay = 4.0;
static const double requiredScore = 0.1;

@class WKFullscreenStackView;

@interface WKFullScreenViewController (VideoFullscreenClientCallbacks)
- (void)willEnterPictureInPicture;
- (void)didEnterPictureInPicture;
- (void)failedToEnterPictureInPicture;
@end

class WKFullScreenViewControllerPlaybackSessionModelClient : PlaybackSessionModelClient {
public:
    void setParent(WKFullScreenViewController *parent) { m_parent = parent; }

    void rateChanged(bool isPlaying, float) override
    {
        m_parent.playing = isPlaying;
    }

    void isPictureInPictureSupportedChanged(bool) override
    {
    }

    void pictureInPictureActiveChanged(bool active) override
    {
        m_parent.pictureInPictureActive = active;
    }

    void setInterface(PlaybackSessionInterfaceAVKit* interface)
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
    WKFullScreenViewController *m_parent { nullptr };
    RefPtr<PlaybackSessionInterfaceAVKit> m_interface;
};

class WKFullScreenViewControllerVideoFullscreenModelClient : VideoFullscreenModelClient {
public:
    void setParent(WKFullScreenViewController *parent) { m_parent = parent; }

    void setInterface(VideoFullscreenInterfaceAVKit* interface)
    {
        if (m_interface == interface)
            return;

        if (m_interface && m_interface->videoFullscreenModel())
            m_interface->videoFullscreenModel()->removeClient(*this);
        m_interface = interface;
        if (m_interface && m_interface->videoFullscreenModel())
            m_interface->videoFullscreenModel()->addClient(*this);
    }

    VideoFullscreenInterfaceAVKit* interface() const { return m_interface.get(); }

    void willEnterPictureInPicture() final
    {
        [m_parent willEnterPictureInPicture];
    }

    void didEnterPictureInPicture() final
    {
        [m_parent didEnterPictureInPicture];
    }

    void failedToEnterPictureInPicture() final
    {
        [m_parent failedToEnterPictureInPicture];
    }

private:
    WKFullScreenViewController *m_parent { nullptr };
    RefPtr<VideoFullscreenInterfaceAVKit> m_interface;
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

#pragma mark - WKFullScreenViewController

@interface WKFullScreenViewController () <UIGestureRecognizerDelegate, UIToolbarDelegate>
@property (weak, nonatomic) WKWebView *_webView; // Cannot be retained, see <rdar://problem/14884666>.
@property (readonly, nonatomic) WebFullScreenManagerProxy* _manager;
@property (readonly, nonatomic) WebCore::FloatBoxExtent _effectiveFullscreenInsets;
@end

@implementation WKFullScreenViewController {
    RetainPtr<UILongPressGestureRecognizer> _touchGestureRecognizer;
    RetainPtr<UIView> _animatingView;
    RetainPtr<WKFullscreenStackView> _stackView;
    RetainPtr<_WKExtrinsicButton> _cancelButton;
    RetainPtr<_WKExtrinsicButton> _pipButton;
    RetainPtr<UIButton> _locationButton;
    RetainPtr<UILayoutGuide> _topGuide;
    RetainPtr<NSLayoutConstraint> _topConstraint;
    WebKit::FullscreenTouchSecheuristic _secheuristic;
    WKFullScreenViewControllerPlaybackSessionModelClient _playbackClient;
    WKFullScreenViewControllerVideoFullscreenModelClient _videoFullscreenClient;
    CGFloat _nonZeroStatusBarHeight;
}

@synthesize prefersStatusBarHidden=_prefersStatusBarHidden;
@synthesize prefersHomeIndicatorAutoHidden=_prefersHomeIndicatorAutoHidden;

#pragma mark - External Interface

- (id)initWithWebView:(WKWebView *)webView
{
    self = [super init];
    if (!self)
        return nil;

    _nonZeroStatusBarHeight = UIApplication.sharedApplication.statusBarFrame.size.height;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_statusBarFrameDidChange:) name:UIApplicationDidChangeStatusBarFrameNotification object:nil];
    _secheuristic.setRampUpSpeed(Seconds(0.25));
    _secheuristic.setRampDownSpeed(Seconds(1.));
    _secheuristic.setXWeight(0);
    _secheuristic.setGamma(0.1);
    _secheuristic.setGammaCutoff(0.08);

    self._webView = webView;

    _playbackClient.setParent(self);
    _videoFullscreenClient.setParent(self);

    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _playbackClient.setParent(nullptr);
    _playbackClient.setInterface(nullptr);
    _videoFullscreenClient.setParent(nullptr);
    _videoFullscreenClient.setInterface(nullptr);

    [_target release];
    [_location release];

    [super dealloc];
}

- (void)showUI
{
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

- (void)videoControlsManagerDidChange
{
    WebPageProxy* page = [self._webView _page];
    auto* videoFullscreenManager = page ? page->videoFullscreenManager() : nullptr;
    auto* videoFullscreenInterface = videoFullscreenManager ? videoFullscreenManager->controlsManagerInterface() : nullptr;
    auto* playbackSessionInterface = videoFullscreenInterface ? &videoFullscreenInterface->playbackSessionInterface() : nullptr;

    _playbackClient.setInterface(playbackSessionInterface);
    _videoFullscreenClient.setInterface(videoFullscreenInterface);

    PlaybackSessionModel* playbackSessionModel = playbackSessionInterface ? playbackSessionInterface->playbackSessionModel() : nullptr;
    self.playing = playbackSessionModel ? playbackSessionModel->isPlaying() : NO;
    [_pipButton setHidden:!playbackSessionModel];
}

- (void)setPrefersStatusBarHidden:(BOOL)value
{
    _prefersStatusBarHidden = value;
    [self setNeedsStatusBarAppearanceUpdate];
    [self _updateWebViewFullscreenInsets];
}

- (void)setPrefersHomeIndicatorAutoHidden:(BOOL)value
{
    _prefersHomeIndicatorAutoHidden = value;
    [self setNeedsUpdateOfHomeIndicatorAutoHidden];
}

- (void)setPlaying:(BOOL)isPlaying
{
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
    if (_pictureInPictureActive == active)
        return;

    _pictureInPictureActive = active;
    [_pipButton setSelected:active];
}

- (void)setAnimating:(BOOL)animating
{
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

- (void)willEnterPictureInPicture
{
    auto* interface = _videoFullscreenClient.interface();
    if (!interface || !interface->pictureInPictureWasStartedWhenEnteringBackground())
        return;

    [UIView animateWithDuration:pipHideAnimationDuration animations:^{
        _animatingView.get().alpha = 0;
    }];
}

- (void)didEnterPictureInPicture
{
    [self _cancelAction:self];
}

- (void)failedToEnterPictureInPicture
{
    auto* interface = _videoFullscreenClient.interface();
    if (!interface || !interface->pictureInPictureWasStartedWhenEnteringBackground())
        return;

    [UIView animateWithDuration:pipHideAnimationDuration animations:^{
        _animatingView.get().alpha = 1;
    }];
}

#pragma mark - UIViewController Overrides

- (void)loadView
{
    [self setView:adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]).get()];
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.backgroundColor = [UIColor blackColor];

    _animatingView = adoptNS([[UIView alloc] initWithFrame:self.view.bounds]);
    _animatingView.get().autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:_animatingView.get()];

    _cancelButton = [_WKExtrinsicButton buttonWithType:UIButtonTypeSystem];
    [_cancelButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_cancelButton setAdjustsImageWhenHighlighted:NO];
    [_cancelButton setExtrinsicContentSize:CGSizeMake(60.0, 47.0)];
    NSBundle *bundle = [NSBundle bundleForClass:self.class];
    UIImage *doneImage = [UIImage imageNamed:@"Done" inBundle:bundle compatibleWithTraitCollection:nil];
    [_cancelButton setImage:[doneImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    [_cancelButton setTintColor:[UIColor whiteColor]];
    [_cancelButton sizeToFit];
    [_cancelButton addTarget:self action:@selector(_cancelAction:) forControlEvents:UIControlEventTouchUpInside];

    _pipButton = [_WKExtrinsicButton buttonWithType:UIButtonTypeSystem];
    [_pipButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_pipButton setAdjustsImageWhenHighlighted:NO];
    [_pipButton setExtrinsicContentSize:CGSizeMake(60.0, 47.0)];
    UIImage *startPiPImage = [UIImage imageNamed:@"StartPictureInPictureButton" inBundle:bundle compatibleWithTraitCollection:nil];
    UIImage *stopPiPImage = [UIImage imageNamed:@"StopPictureInPictureButton" inBundle:bundle compatibleWithTraitCollection:nil];
    [_pipButton setImage:[startPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    [_pipButton setImage:[stopPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateSelected];
    [_pipButton setTintColor:[UIColor whiteColor]];
    [_pipButton sizeToFit];
    [_pipButton addTarget:self action:@selector(_togglePiPAction:) forControlEvents:UIControlEventTouchUpInside];

    _stackView = adoptNS([[WKFullscreenStackView alloc] init]);
    [_stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_stackView addArrangedSubview:_cancelButton.get() applyingMaterialStyle:AVBackgroundViewMaterialStyleSecondary tintEffectStyle:AVBackgroundViewTintEffectStyleSecondary];
    [_stackView addArrangedSubview:_pipButton.get() applyingMaterialStyle:AVBackgroundViewMaterialStylePrimary tintEffectStyle:AVBackgroundViewTintEffectStyleSecondary];
    [_animatingView addSubview:_stackView.get()];

    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    UILayoutGuide *margins = self.view.layoutMarginsGuide;

    _topGuide = adoptNS([[UILayoutGuide alloc] init]);
    [self.view addLayoutGuide:_topGuide.get()];
    NSLayoutAnchor *topAnchor = [_topGuide topAnchor];
    _topConstraint = [topAnchor constraintEqualToAnchor:safeArea.topAnchor];
    [NSLayoutConstraint activateConstraints:@[
        _topConstraint.get(),
        [[_stackView topAnchor] constraintEqualToAnchor:topAnchor],
        [[_stackView leadingAnchor] constraintEqualToAnchor:margins.leadingAnchor],
    ]];

    [_stackView setAlpha:0];
    [_stackView setHidden:YES];
    [self videoControlsManagerDidChange];

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
        [self._webView _setInterfaceOrientationOverride:[UIApp statusBarOrientation]];
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
- (WebFullScreenManagerProxy*)_manager
{
    if (auto* page = [self._webView _page])
        return page->fullScreenManager();
    return nullptr;
}

@dynamic _effectiveFullscreenInsets;
- (WebCore::FloatBoxExtent)_effectiveFullscreenInsets
{
    auto safeAreaInsets = self.view.safeAreaInsets;
    WebCore::FloatBoxExtent insets { safeAreaInsets.top, safeAreaInsets.right, safeAreaInsets.bottom, safeAreaInsets.left };

    CGRect cancelFrame = _cancelButton.get().frame;
    CGPoint maxXY = CGPointMake(CGRectGetMaxX(cancelFrame), CGRectGetMaxY(cancelFrame));
    insets.setTop([_cancelButton convertPoint:maxXY toView:self.view].y);
    return insets;
}

- (void)_cancelAction:(id)sender
{
    [[self target] performSelector:[self action]];
}

- (void)_togglePiPAction:(id)sender
{
    WebPageProxy* page = [self._webView _page];
    if (!page)
        return;

    PlaybackSessionManagerProxy* playbackSessionManager = page->playbackSessionManager();
    if (!playbackSessionManager)
        return;

    PlatformPlaybackSessionInterface* playbackSessionInterface = playbackSessionManager->controlsManagerInterface();
    if (!playbackSessionInterface)
        return;

    PlaybackSessionModel* playbackSessionModel = playbackSessionInterface->playbackSessionModel();
    if (!playbackSessionModel)
        return;

    playbackSessionModel->togglePictureInPicture();
}

- (void)_touchDetected:(id)sender
{
    if ([_touchGestureRecognizer state] == UIGestureRecognizerStateBegan || [_touchGestureRecognizer state] == UIGestureRecognizerStateEnded) {
        double score = _secheuristic.scoreOfNextTouch([_touchGestureRecognizer locationInView:self.view]);
        if (score > requiredScore)
            [self _showPhishingAlert];
    }
    if (!self.animating)
        [self showUI];
}

- (void)_statusBarFrameDidChange:(NSNotificationCenter *)notification
{
    CGFloat height = UIApplication.sharedApplication.statusBarFrame.size.height;
    if (!height || height == _nonZeroStatusBarHeight)
        return;

    _nonZeroStatusBarHeight = height;
    [self _updateWebViewFullscreenInsets];
}

- (void)_updateWebViewFullscreenInsets
{
    if (auto* manager = self._manager)
        manager->setFullscreenInsets(self._effectiveFullscreenInsets);
}

- (void)_showPhishingAlert
{
    NSString *alertTitle = WEB_UI_STRING("It looks like you are typing while in full screen", "Full Screen Deceptive Website Warning Sheet Title");
    NSString *alertMessage = [NSString stringWithFormat:WEB_UI_STRING("Typing is not allowed in full screen websites. “%@” may be showing a fake keyboard to trick you into disclosing personal or financial information.", "Full Screen Deceptive Website Warning Sheet Content Text"), (NSString *)self.location];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:alertTitle message:alertMessage preferredStyle:UIAlertControllerStyleAlert];

    if (auto* page = [self._webView _page])
        page->suspendActiveDOMObjectsAndAnimations();

    UIAlertAction* exitAction = [UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("Exit Full Screen", "Exit Full Screen (Element Full Screen)", "Full Screen Deceptive Website Exit Action") style:UIAlertActionStyleCancel handler:^(UIAlertAction * action) {
        [self _cancelAction:action];
        if (auto* page = [self._webView _page])
            page->resumeActiveDOMObjectsAndAnimations();
    }];

    UIAlertAction* stayAction = [UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("Stay in Full Screen", "Stay in Full Screen (Element Full Screen)", "Full Screen Deceptive Website Stay Action") style:UIAlertActionStyleDefault handler:^(UIAlertAction * action) {
        if (auto* page = [self._webView _page])
            page->resumeActiveDOMObjectsAndAnimations();
        _secheuristic.reset();
    }];

    [alert addAction:exitAction];
    [alert addAction:stayAction];
    [self presentViewController:alert animated:YES completion:nil];
}

@end

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
