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

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS)
#import "WKFullScreenViewController.h"

#import "FullscreenTouchSecheuristic.h"
#import "PlaybackSessionManagerProxy.h"
#import "UIKitSPI.h"
#import "WKFullscreenStackView.h"
#import "WKWebViewInternal.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;
using namespace WebKit;

static const NSTimeInterval showHideAnimationDuration = 0.1;
static const NSTimeInterval autoHideDelay = 4.0;
static const double requiredScore = 0.1;

@class WKFullscreenStackView;

class WKFullScreenViewControllerPlaybackSessionModelClient : PlaybackSessionModelClient {
public:
    void setParent(WKFullScreenViewController *parent) { m_parent = parent; }

    void rateChanged(bool isPlaying, float) override
    {
        m_parent.playing = isPlaying;
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
            interface->playbackSessionModel()->removeClient(*this);
        m_interface = interface;
        if (m_interface && m_interface->playbackSessionModel())
            interface->playbackSessionModel()->addClient(*this);
    }

private:
    WKFullScreenViewController *m_parent { nullptr };
    RefPtr<PlaybackSessionInterfaceAVKit> m_interface;
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
@property (readonly, nonatomic) CGFloat _effectiveFullscreenInsetTop;
@end

@implementation WKFullScreenViewController {
    RetainPtr<UILongPressGestureRecognizer> _touchGestureRecognizer;
    RetainPtr<WKFullscreenStackView> _stackView;
    RetainPtr<_WKExtrinsicButton> _cancelButton;
    RetainPtr<_WKExtrinsicButton> _pipButton;
    RetainPtr<UIButton> _locationButton;
    RetainPtr<UILayoutGuide> _topGuide;
    RetainPtr<NSLayoutConstraint> _topConstraint;
    WebKit::FullscreenTouchSecheuristic _secheuristic;
    WKFullScreenViewControllerPlaybackSessionModelClient _playbackClient;
    CGFloat _nonZeroStatusBarHeight;
}

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

    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _playbackClient.setInterface(nullptr);

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
    }];
}

- (void)hideUI
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [self.view removeConstraints:@[_topConstraint.get()]];
        _topConstraint = [[_topGuide topAnchor] constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:self.view.safeAreaInsets.top];
        [_topConstraint setActive:YES];
        [_stackView setAlpha:0];
        self.prefersStatusBarHidden = YES;
    } completion:^(BOOL finished) {
        if (!finished)
            return;

        [self.view removeConstraints:@[_topConstraint.get()]];
        _topConstraint = [[_topGuide topAnchor] constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor];
        [_topConstraint setActive:YES];
        [_stackView setHidden:YES];
    }];
}

- (void)videoControlsManagerDidChange
{
    WebPageProxy* page = [self._webView _page];
    PlaybackSessionManagerProxy* playbackSessionManager = page ? page->playbackSessionManager() : nullptr;
    PlatformPlaybackSessionInterface* playbackSessionInterface = playbackSessionManager ? playbackSessionManager->controlsManagerInterface() : nullptr;
    _playbackClient.setInterface(playbackSessionInterface);

    PlaybackSessionModel* playbackSessionModel = playbackSessionInterface ? playbackSessionInterface->playbackSessionModel() : nullptr;
    self.playing = playbackSessionModel ? playbackSessionModel->isPlaying() : NO;
    [_pipButton setHidden:!playbackSessionModel];
}

@synthesize prefersStatusBarHidden=_prefersStatusBarHidden;

- (void)setPrefersStatusBarHidden:(BOOL)value
{
    _prefersStatusBarHidden = value;
    [self setNeedsStatusBarAppearanceUpdate];
    [self _updateWebViewFullscreenInsets];
}

- (void)setPlaying:(BOOL)isPlaying
{
    if (_playing == isPlaying)
        return;

    _playing = isPlaying;
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

#pragma mark - UIViewController Overrides

- (void)loadView
{
    [self setView:adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]).get()];
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    _cancelButton = [_WKExtrinsicButton buttonWithType:UIButtonTypeSystem];
    [_cancelButton setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_cancelButton setAdjustsImageWhenHighlighted:NO];
    [_cancelButton setExtrinsicContentSize:CGSizeMake(60.0, 47.0)];
    [WKFullscreenStackView applyPrimaryGlyphTintToView:_cancelButton.get()];
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
    [WKFullscreenStackView applyPrimaryGlyphTintToView:_pipButton.get()];
    UIImage *startPiPImage = [UIImage imageNamed:@"StartPictureInPictureButton" inBundle:bundle compatibleWithTraitCollection:nil];
    UIImage *stopPiPImage = [UIImage imageNamed:@"StopPictureInPictureButton" inBundle:bundle compatibleWithTraitCollection:nil];
    [_pipButton setImage:[startPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    [_pipButton setImage:[stopPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateSelected];
    [_pipButton setTintColor:[UIColor whiteColor]];
    [_pipButton sizeToFit];
    [_pipButton addTarget:self action:@selector(_togglePiPAction:) forControlEvents:UIControlEventTouchUpInside];

    _stackView = adoptNS([[WKFullscreenStackView alloc] initWithArrangedSubviews:@[_cancelButton.get(), _pipButton.get()] axis:UILayoutConstraintAxisHorizontal]);
    [_stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_stackView setTargetViewForSecondaryMaterialOverlay:_cancelButton.get()];
    [[self view] addSubview:_stackView.get()];

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
    [self.view insertSubview:self._webView atIndex:0];

    if (auto* manager = self._manager)
        manager->setFullscreenAutoHideDelay(autoHideDelay);
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
    return _prefersStatusBarHidden;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
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

@dynamic _effectiveFullscreenInsetTop;
- (CGFloat)_effectiveFullscreenInsetTop
{
    if (!self.prefersStatusBarHidden)
        return 0;

    CGRect cancelFrame = _cancelButton.get().frame;
    CGPoint maxXY = CGPointMake(CGRectGetMaxX(cancelFrame), CGRectGetMaxY(cancelFrame));
    return [_cancelButton convertPoint:maxXY toView:self.view].y;
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
    if ([_touchGestureRecognizer state] != UIGestureRecognizerStateBegan || [_touchGestureRecognizer state] == UIGestureRecognizerStateEnded) {
        double score = _secheuristic.scoreOfNextTouch([_touchGestureRecognizer locationInView:self.view]);
        if (score > requiredScore)
            [self _showPhishingAlert];
    }
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
        manager->setFullscreenInsetTop(self._effectiveFullscreenInsetTop);
}

- (void)_showPhishingAlert
{
    NSString *alertTitle = WEB_UI_STRING("Deceptive Website Warning", "Fullscreen Deceptive Website Warning Sheet Title");
    NSString *alertMessage = [NSString stringWithFormat:WEB_UI_STRING("The website \"%@\" may be a deceptive website. Would you like to exit fullscreen?", "Fullscreen Deceptive Website Warning Sheet Content Text") , (NSString *)self.location];
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:alertTitle message:alertMessage preferredStyle:UIAlertControllerStyleAlert];

    UIAlertAction* exitAction = [UIAlertAction actionWithTitle:WEB_UI_STRING("Exit Fullscreen", "Fullscreen Deceptive Website Exit Action") style:UIAlertActionStyleCancel handler:^(UIAlertAction * action) {
        [self _cancelAction:action];
    }];

    UIAlertAction* stayAction = [UIAlertAction actionWithTitle:WEB_UI_STRING("Stay in Fullscreen", "Fullscreen Deceptive Website Stay Action") style:UIAlertActionStyleDefault handler:^(UIAlertAction * action) {
        _secheuristic.reset();
    }];

    [alert addAction:exitAction];
    [alert addAction:stayAction];
    [self presentViewController:alert animated:YES completion:nil];
}

@end

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS)
