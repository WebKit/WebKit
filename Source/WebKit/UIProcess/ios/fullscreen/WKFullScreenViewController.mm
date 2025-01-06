/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#import "WKFullScreenViewController.h"

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)

#import "FullscreenTouchSecheuristic.h"
#import "LinearMediaKitExtras.h"
#import "PlaybackSessionManagerProxy.h"
#import "UIKitUtilities.h"
#import "VideoPresentationManagerProxy.h"
#import "WKExtrinsicButton.h"
#import "WKFullscreenStackView.h"
#import "WKWebViewIOS.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/PlaybackSessionInterfaceAVKitLegacy.h>
#import <WebCore/PlaybackSessionInterfaceTVOS.h>
#import <WebCore/VideoPresentationInterfaceAVKitLegacy.h>
#import <WebCore/VideoPresentationInterfaceTVOS.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/CheckedRef.h>
#import <wtf/FastMalloc.h>
#import <wtf/MonotonicTime.h>
#import <wtf/OptionSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakObjCPtr.h>

#if ENABLE(LINEAR_MEDIA_PLAYER)
#import "WKSLinearMediaPlayer.h"
#endif

#if PLATFORM(VISION)
#import "MRUIKitSPI.h"
#endif

static const NSTimeInterval showHideAnimationDuration = 0.1;
static const NSTimeInterval pipHideAnimationDuration = 0.2;
static const NSTimeInterval autoHideDelay = 4.0;

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
static const Seconds bannerMinimumHideDelay = 1_s;
#endif

@class WKFullscreenStackView;

class WKFullScreenViewControllerPlaybackSessionModelClient final : WebCore::PlaybackSessionModelClient, public CanMakeCheckedPtr<WKFullScreenViewControllerPlaybackSessionModelClient> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WKFullScreenViewControllerPlaybackSessionModelClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WKFullScreenViewControllerPlaybackSessionModelClient);
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

    void setInterface(WebCore::PlaybackSessionInterfaceIOS* interface)
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
    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

    WeakObjCPtr<WKFullScreenViewController> m_parent;
    RefPtr<WebCore::PlaybackSessionInterfaceIOS> m_interface;
};

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

@interface WKFullScreenViewController () <UIGestureRecognizerDelegate, UIToolbarDelegate, WKExtrinsicButtonDelegate>
@property (weak, nonatomic) WKWebView *_webView; // Cannot be retained, see <rdar://problem/14884666>.
@property (readonly, nonatomic) WebKit::WebFullScreenManagerProxy* _manager;
@property (readonly, nonatomic) WebCore::FloatBoxExtent _effectiveFullscreenInsets;
@end

@implementation WKFullScreenViewController {
    BOOL _valid;
    WeakObjCPtr<id<WKFullScreenViewControllerDelegate>> _delegate;
    RetainPtr<UILongPressGestureRecognizer> _touchGestureRecognizer;
    RetainPtr<UIView> _animatingView;
    RetainPtr<UIStackView> _stackView;
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    RetainPtr<UIStackView> _banner;
    RetainPtr<_WKInsetLabel> _bannerLabel;
    RetainPtr<UITapGestureRecognizer> _bannerTapToDismissRecognizer;
    MonotonicTime _bannerMinimumHideDelayTime;
#endif
    RetainPtr<WKExtrinsicButton> _cancelButton;
    RetainPtr<WKExtrinsicButton> _pipButton;
    RetainPtr<UIButton> _locationButton;
    RetainPtr<UILayoutGuide> _topGuide;
    RetainPtr<NSLayoutConstraint> _topConstraint;
    String _location;
    WebKit::FullscreenTouchSecheuristic _secheuristic;
    WKFullScreenViewControllerPlaybackSessionModelClient _playbackClient;
    CGFloat _nonZeroStatusBarHeight;
    std::optional<UIInterfaceOrientationMask> _supportedOrientations;
    BOOL _isShowingMenu;
#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    BOOL _shouldHideCustomControls;
#endif
#if PLATFORM(VISION)
    RetainPtr<WKExtrinsicButton> _moreActionsButton;
    BOOL _isInteractingWithSystemChrome;
#endif
#if ENABLE(LINEAR_MEDIA_PLAYER)
    RetainPtr<UIViewController> _environmentPickerButtonViewController;
    RetainPtr<UIStackView> _centeredStackView;
    RetainPtr<UIButton> _enterVideoFullscreenButton;
    enum ButtonState {
        EnvironmentPicker = 1 << 0,
        FullscreenVideo = 1 << 1
    };
    OptionSet<ButtonState> _buttonState;
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

#if PLATFORM(VISION)
    UIWindowScene *windowScene = webView.window.windowScene;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didBeginInteractionWithSystemChrome:) name:_UIWindowSceneDidBeginLiveResizeNotification object:windowScene];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didEndInteractionWithSystemChrome:) name:_UIWindowSceneDidEndLiveResizeNotification object:windowScene];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didBeginInteractionWithSystemChrome:) name:_MRUIWindowSceneDidBeginRepositioningNotification object:windowScene];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_didEndInteractionWithSystemChrome:) name:_MRUIWindowSceneDidEndRepositioningNotification object:windowScene];
#endif

    _secheuristic.setParameters(WebKit::FullscreenTouchSecheuristicParameters::iosParameters());
    self._webView = webView;

    _playbackClient.setParent(self);
    _valid = YES;
    _isShowingMenu = NO;
#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    _shouldHideCustomControls = NO;
#endif
#if PLATFORM(VISION)
    _isInteractingWithSystemChrome = NO;
#endif

    return self;
}

- (void)invalidate
{
    if (!_valid)
        return;

    _valid = NO;

    [self _pauseIfNeeded];

    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideBanner) object:nil];
#endif
#if ENABLE(LINEAR_MEDIA_PLAYER)
    [self _didCleanupFullscreen];
#endif

    [[NSNotificationCenter defaultCenter] removeObserver:self];
    _playbackClient.setParent(nullptr);
    _playbackClient.setInterface(nullptr);
}

- (void)dealloc
{
    [self invalidate];
    [super dealloc];
}

- (id<WKFullScreenViewControllerDelegate>)delegate
{
    return _delegate.get().get();
}

- (void)setDelegate:(id<WKFullScreenViewControllerDelegate>)delegate
{
    _delegate = delegate;
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
        [[self delegate] showUI];
        [_stackView setHidden:NO];
        [_stackView setAlpha:1];
        self.prefersStatusBarHidden = NO;
        self.prefersHomeIndicatorAutoHidden = NO;
        if (_topConstraint)
            [NSLayoutConstraint deactivateConstraints:@[_topConstraint.get()]];
        _topConstraint = [[_topGuide topAnchor] constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor];
        [_topConstraint setActive:YES];
#if ENABLE(LINEAR_MEDIA_PLAYER)
        [_centeredStackView setHidden:NO];
        [_centeredStackView setAlpha:1];
#endif
    }];
}

- (void)hideUI
{
    ASSERT(_valid);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];

    if (_isShowingMenu)
        return;

#if PLATFORM(VISION)
    if (_isInteractingWithSystemChrome)
        return;
#endif

    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [[self delegate] hideUI];
        if (_topConstraint)
            [NSLayoutConstraint deactivateConstraints:@[_topConstraint.get()]];
        _topConstraint = [[_topGuide topAnchor] constraintEqualToAnchor:self.view.topAnchor constant:self.view.safeAreaInsets.top];
        [_topConstraint setActive:YES];
        [_stackView setAlpha:0];
        self.prefersStatusBarHidden = YES;
        self.prefersHomeIndicatorAutoHidden = YES;
#if ENABLE(LINEAR_MEDIA_PLAYER)
        [_centeredStackView setAlpha:0];
#endif
    } completion:^(BOOL finished) {
        if (!finished)
            return;

        [_stackView setHidden:YES];
#if ENABLE(LINEAR_MEDIA_PLAYER)
        [_centeredStackView setHidden:YES];
#endif
    }];
}

- (void)showBanner
{
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    ASSERT(_valid);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideBanner) object:nil];

    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [_banner setHidden:NO];
        [_banner setAlpha:1];
    }];

    _bannerMinimumHideDelayTime = MonotonicTime::now() + bannerMinimumHideDelay;

    [self performSelector:@selector(hideBanner) withObject:nil afterDelay:autoHideDelay];
#endif
}

- (void)hideBanner
{
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    ASSERT(_valid);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideBanner) object:nil];
    [UIView animateWithDuration:showHideAnimationDuration animations:^{
        [_banner setAlpha:0];
    } completion:^(BOOL finished) {
        if (!finished)
            return;

        [_banner setHidden:YES];
    }];
#endif
}

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
- (void)_bannerDismissalRecognized:(NSNotification*)notification
{
    auto remainingDelay = _bannerMinimumHideDelayTime - MonotonicTime::now();
    if (remainingDelay <= 0_s) {
        [self hideBanner];
        return;
    }

    [self performSelector:@selector(hideBanner) withObject:nil afterDelay:remainingDelay.seconds()];
}
#endif

- (void)videoControlsManagerDidChange
{
    ASSERT(_valid);
    RefPtr playbackSessionInterface = [self _playbackSessionInterface];

    _playbackClient.setInterface(playbackSessionInterface.get());

    WebCore::PlaybackSessionModel* playbackSessionModel = playbackSessionInterface ? playbackSessionInterface->playbackSessionModel() : nullptr;
    self.playing = playbackSessionModel ? playbackSessionModel->isPlaying() : NO;
    bool isPiPEnabled = false;
    if (auto page = [self._webView _page])
        isPiPEnabled = page->preferences().pictureInPictureAPIEnabled() && page->preferences().allowsPictureInPictureMediaPlayback();
    bool isPiPSupported = playbackSessionModel && playbackSessionModel->isPictureInPictureSupported();
#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    [_cancelButton setHidden:_shouldHideCustomControls];
#endif

#if PLATFORM(VISION)
    bool isDimmingEnabled = false;
    if (auto page = [self._webView _page])
        isDimmingEnabled = page->preferences().fullscreenSceneDimmingEnabled();
    [_moreActionsButton setHidden:_shouldHideCustomControls || !isDimmingEnabled];

    isPiPEnabled = !_shouldHideCustomControls && isPiPEnabled;
#endif
    [_pipButton setHidden:!isPiPEnabled || !isPiPSupported];

#if ENABLE(LINEAR_MEDIA_PLAYER)
    [self configureEnvironmentPickerOrFullscreenVideoButtonView];
#endif
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
- (void)configureEnvironmentPickerOrFullscreenVideoButtonView
{
    ASSERT(_valid);

    RefPtr page = self._webView._page.get();
    if (!page || !page->preferences().linearMediaPlayerEnabled()) {
        [self _removeEnvironmentPickerButtonView];
        [self _removeEnvironmentFullscreenVideoButtonView];
        return;
    }

    RefPtr playbackSessionInterface = [self _playbackSessionInterface];
    auto* playbackSessionModel = playbackSessionInterface ? playbackSessionInterface->playbackSessionModel() : nullptr;
    if (!playbackSessionModel || !playbackSessionModel->supportsLinearMediaPlayer()) {
        [self _removeEnvironmentPickerButtonView];
        [self _removeEnvironmentFullscreenVideoButtonView];
        return;
    }

    if (RetainPtr mediaPlayer = playbackSessionInterface->linearMediaPlayer(); [mediaPlayer spatialVideoMetadata]) {
        if (!_buttonState.contains(FullscreenVideo)) {
            [_centeredStackView addArrangedSubview:_enterVideoFullscreenButton.get()];
            _buttonState.add(FullscreenVideo);
        }
    } else
        [self _removeEnvironmentFullscreenVideoButtonView];

    RefPtr videoPresentationManager = page->videoPresentationManager();
    RefPtr videoPresentationInterface = videoPresentationManager ? videoPresentationManager->controlsManagerInterface() : nullptr;

    LMPlayableViewController *playableViewController = videoPresentationInterface ? videoPresentationInterface->playableViewController() : nil;
    UIViewController *environmentPickerButtonViewController = playableViewController.wks_environmentPickerButtonViewController;

    if (environmentPickerButtonViewController) {
        playableViewController.wks_automaticallyDockOnFullScreenPresentation = YES;
        playableViewController.wks_dismissFullScreenOnExitingDocking = YES;
    }

    if (_environmentPickerButtonViewController == environmentPickerButtonViewController) {
        ASSERT(!environmentPickerButtonViewController || [[_stackView arrangedSubviews] containsObject:environmentPickerButtonViewController.view]);
        return;
    }

    [self _removeEnvironmentPickerButtonView];
    if (!environmentPickerButtonViewController)
        return;

    [self addChildViewController:environmentPickerButtonViewController];
    [_stackView insertArrangedSubview:environmentPickerButtonViewController.view atIndex:1];
    [environmentPickerButtonViewController didMoveToParentViewController:self];
    _environmentPickerButtonViewController = environmentPickerButtonViewController;
    _buttonState.add(EnvironmentPicker);
}

- (void)_removeEnvironmentPickerButtonView
{
    if (!_buttonState.contains(EnvironmentPicker))
        return;

    UIView *environmentPickerButtonView = [_environmentPickerButtonViewController view];

    [_environmentPickerButtonViewController willMoveToParentViewController:nil];
    [_stackView removeArrangedSubview:environmentPickerButtonView];
    [environmentPickerButtonView removeFromSuperview];
    [self removeChildViewController:_environmentPickerButtonViewController.get()];

    _environmentPickerButtonViewController = nil;
    _buttonState.remove(EnvironmentPicker);
}

- (void)_removeEnvironmentFullscreenVideoButtonView
{
    if (!_buttonState.contains(FullscreenVideo))
        return;

    [_centeredStackView removeArrangedSubview:_enterVideoFullscreenButton.get()];
    [_enterVideoFullscreenButton removeFromSuperview];
    _buttonState.remove(FullscreenVideo);
}

- (void)_didCleanupFullscreen
{
    // Place videoPresentationInterface cleanup code here.
}
#endif // ENABLE(LINEAR_MEDIA_PLAYER)

- (void)setAnimatingViewAlpha:(CGFloat)alpha
{
    ASSERT(_valid);
    [UIView animateWithDuration:pipHideAnimationDuration animations:^{
        _animatingView.get().alpha = alpha;
    }];
}

#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)

- (void)hideCustomControls:(BOOL)hidden
{
    if (_shouldHideCustomControls == hidden)
        return;

    _shouldHideCustomControls = hidden;
    [self videoControlsManagerDidChange];
}

#endif

#if PLATFORM(VISION)

- (void)_didBeginInteractionWithSystemChrome:(NSNotificationCenter *)notification
{
    _isInteractingWithSystemChrome = YES;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
}

- (void)_didEndInteractionWithSystemChrome:(NSNotificationCenter *)notification
{
    _isInteractingWithSystemChrome = NO;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
    if (_playing)
        [self performSelector:@selector(hideUI) withObject:nil afterDelay:autoHideDelay];
}

#endif // PLATFORM(VISION)

+ (NSSet<NSString *> *)keyPathsForValuesAffectingAdditionalSafeAreaInsets
{
    return [NSSet setWithObjects:@"prefersStatusBarHidden", @"view.window.windowScene.statusBarManager.statusBarHidden", @"view.window.safeAreaInsets", nil];
}

- (UIEdgeInsets)_additionalBottomInsets
{
    // If the window's safeAreaInsets.bottom is 0 then no additional bottom inset is necessary
    // (e.g., because the device has a home button).
    if (!self.view.window.safeAreaInsets.bottom)
        return UIEdgeInsetsZero;

    // This value was determined experimentally by finding the smallest value that
    // allows for interacting with a slider aligned to the bottom of the web view without
    // accidentally triggering system pan gestures.
    static const UIEdgeInsets additionalBottomInsets = UIEdgeInsetsMake(0, 0, 8, 0);

    return additionalBottomInsets;
}

- (UIEdgeInsets)additionalSafeAreaInsets
{
    // When the status bar hides, the resulting changes to safeAreaInsets cause
    // fullscreen content to jump up and down in response.

    // Do not add additional insets if the status bar is not hidden.
    if (!self.view.window.windowScene.statusBarManager.statusBarHidden)
        return [self _additionalBottomInsets];

    // If the status bar is hidden while would would prefer it not be,
    // we should not reserve space as it likely won't re-appear.
    if (!self.prefersStatusBarHidden)
        return [self _additionalBottomInsets];

    // Additionally, hiding the status bar does not reduce safeAreaInsets when
    // the status bar resides within a larger safe area inset (e.g., due to the
    // camera area).
    if (self.view.window.safeAreaInsets.top > 0)
        return [self _additionalBottomInsets];

    // Otherwise, provide what is effectively a constant safeAreaInset.top by adding
    // an additional safeAreaInset at the top equal to the status bar height.
    return UIEdgeInsetsAdd([self _additionalBottomInsets], UIEdgeInsetsMake(_nonZeroStatusBarHeight, 0, 0, 0), UIRectEdgeAll);
}

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
#if !PLATFORM(APPLETV)
    [self setNeedsUpdateOfHomeIndicatorAutoHidden];
#endif
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

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    [_bannerLabel setText:[NSString stringWithFormat:WEB_UI_NSSTRING(@"”%@” is in full screen.\nSwipe down to exit.", "Full Screen Warning Banner Content Text"), (NSString *)self.location]];
    [_bannerLabel sizeToFit];
#endif
}

#pragma mark - UIViewController Overrides

- (void)loadView
{
    CGSize buttonSize;
    UIImage *doneImage;
    UIImage *startPiPImage;
    UIImage *stopPiPImage;

    // FIXME: Rename `alternateFullScreenControlDesignEnabled` to something that explains it is for visionOS.
    auto alternateFullScreenControlDesignEnabled = self._webView._page->preferences().alternateFullScreenControlDesignEnabled();
    
    if (alternateFullScreenControlDesignEnabled) {
        buttonSize = CGSizeMake(44.0, 44.0);
        doneImage = [UIImage systemImageNamed:@"arrow.down.right.and.arrow.up.left"];
        startPiPImage = nil;
        stopPiPImage = nil;
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

    _cancelButton = [self _createButtonWithExtrinsicContentSize:buttonSize];
    [_cancelButton setImage:[doneImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
    [_cancelButton sizeToFit];
    [_cancelButton addTarget:self action:@selector(_cancelAction:) forControlEvents:UIControlEventTouchUpInside];
#if PLATFORM(APPLETV)
    [_cancelButton setConfiguration:UIButtonConfiguration.filledButtonConfiguration];
#endif

    if (alternateFullScreenControlDesignEnabled) {
        UIButtonConfiguration *cancelButtonConfiguration = [UIButtonConfiguration filledButtonConfiguration];
        [_cancelButton setConfiguration:cancelButtonConfiguration];

#if PLATFORM(VISION)
        // FIXME: I think PLATFORM(VISION) is always true when `alternateFullScreenControlDesignEnabled` is true.
        _moreActionsButton = [self _createButtonWithExtrinsicContentSize:buttonSize];
        [_moreActionsButton setConfiguration:cancelButtonConfiguration];
        [_moreActionsButton setMenu:self._webView.fullScreenWindowSceneDimmingAction];
        [_moreActionsButton setShowsMenuAsPrimaryAction:YES];
        [_moreActionsButton setImage:[[UIImage systemImageNamed:@"ellipsis"] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
        _enterVideoFullscreenButton = [UIButton buttonWithType:UIButtonTypeSystem];
        [self _setupButton:_enterVideoFullscreenButton.get()];

        UIButtonConfiguration *fullscreenButtonConfiguration = [UIButtonConfiguration filledButtonConfiguration];
        fullscreenButtonConfiguration.imagePadding = 9;
        fullscreenButtonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(12, 18, 12, 22);
        fullscreenButtonConfiguration.titleLineBreakMode = NSLineBreakByClipping;

        RetainPtr imageConfiguration = [[UIImageSymbolConfiguration configurationWithTextStyle:UIFontTextStyleBody] configurationByApplyingConfiguration:[UIImageSymbolConfiguration configurationWithWeight:UIImageSymbolWeightMedium]];

        fullscreenButtonConfiguration.image = [[UIImage systemImageNamed:@"cube" withConfiguration:imageConfiguration.get()] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

        RetainPtr descriptor = [UIFontDescriptor preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
        descriptor = [descriptor fontDescriptorByAddingAttributes:@{
            UIFontWeightTrait : [NSNumber numberWithDouble:UIFontWeightMedium]
        }];
        RetainPtr buttonTitle = adoptNS([[NSMutableAttributedString alloc] initWithString:WebCore::fullscreenControllerViewSpatial() attributes:@{
            NSFontAttributeName : [UIFont fontWithDescriptor:descriptor.get() size:0]
        }]);
        fullscreenButtonConfiguration.attributedTitle = buttonTitle.get();

        [_enterVideoFullscreenButton setConfiguration:fullscreenButtonConfiguration];
        [_enterVideoFullscreenButton setContentHorizontalAlignment:UIControlContentHorizontalAlignmentLeft];

        [_enterVideoFullscreenButton sizeToFit];

        [_enterVideoFullscreenButton addTarget:self action:@selector(_enterVideoFullscreenAction:) forControlEvents:UIControlEventTouchUpInside];

        _centeredStackView = adoptNS([[UIStackView alloc] init]);
        [_centeredStackView setSpacing:24.0];
#endif

        _stackView = adoptNS([[UIStackView alloc] init]);
        [_stackView addArrangedSubview:_cancelButton.get()];
        [_stackView addArrangedSubview:_pipButton.get()];
#if PLATFORM(VISION)
        [_stackView addArrangedSubview:_moreActionsButton.get()];
#endif
        [_stackView setSpacing:24.0];
    } else {
        _pipButton = [self _createButtonWithExtrinsicContentSize:buttonSize];
        [_pipButton setImage:[startPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateNormal];
        [_pipButton setImage:[stopPiPImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate] forState:UIControlStateSelected];
        [_pipButton sizeToFit];
        [_pipButton addTarget:self action:@selector(_togglePiPAction:) forControlEvents:UIControlEventTouchUpInside];

        RetainPtr<WKFullscreenStackView> stackView = adoptNS([[WKFullscreenStackView alloc] init]);
#if PLATFORM(APPLETV)
        [stackView addArrangedSubview:_cancelButton.get()];
#else
        [stackView addArrangedSubview:_cancelButton.get() applyingMaterialStyle:AVBackgroundViewMaterialStyleSecondary tintEffectStyle:AVBackgroundViewTintEffectStyleSecondary];
        [stackView addArrangedSubview:_pipButton.get() applyingMaterialStyle:AVBackgroundViewMaterialStylePrimary tintEffectStyle:AVBackgroundViewTintEffectStyleSecondary];
#endif
        _stackView = WTFMove(stackView);
    }

    [_stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_animatingView addSubview:_stackView.get()];

#if ENABLE(LINEAR_MEDIA_PLAYER)
    [_centeredStackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_animatingView addSubview:_centeredStackView.get()];
#endif

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
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

    _bannerTapToDismissRecognizer = adoptNS([[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(_bannerDismissalRecognized:)]);
    [_bannerTapToDismissRecognizer setDelegate:self];
    [_banner addGestureRecognizer:_bannerTapToDismissRecognizer.get()];

    [_banner setTranslatesAutoresizingMaskIntoConstraints:NO];

    [_animatingView addSubview:_banner.get()];
#endif

    UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
    UILayoutGuide *margins = self.view.layoutMarginsGuide;

    _topGuide = adoptNS([[UILayoutGuide alloc] init]);
    [self.view addLayoutGuide:_topGuide.get()];
    NSLayoutYAxisAnchor *topAnchor = [_topGuide topAnchor];
    NSLayoutConstraint *stackViewToTopGuideConstraint;
    if (alternateFullScreenControlDesignEnabled)
        stackViewToTopGuideConstraint = [[_stackView topAnchor] constraintEqualToSystemSpacingBelowAnchor:topAnchor multiplier:3];
    else
        stackViewToTopGuideConstraint = [[_stackView topAnchor] constraintEqualToAnchor:topAnchor];
    _topConstraint = [topAnchor constraintEqualToAnchor:safeArea.topAnchor];
    [NSLayoutConstraint activateConstraints:@[
        _topConstraint.get(),
        stackViewToTopGuideConstraint,
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
        [[_banner topAnchor] constraintEqualToSystemSpacingBelowAnchor:[_stackView bottomAnchor] multiplier:3],
        [[_banner centerXAnchor] constraintEqualToAnchor:self.view.centerXAnchor],
#endif
        [[_stackView leadingAnchor] constraintEqualToAnchor:margins.leadingAnchor],
#if ENABLE(LINEAR_MEDIA_PLAYER)
        // Align stack view's top anchor to the other stack view and to the middle of its superview.
        [[_centeredStackView topAnchor] constraintEqualToAnchor:[_stackView topAnchor]],
        [[_centeredStackView centerXAnchor] constraintEqualToAnchor:[_animatingView centerXAnchor]],
#endif
    ]];

    [_stackView setAlpha:0];
    [_stackView setHidden:YES];
#if ENABLE(LINEAR_MEDIA_PLAYER)
    [_centeredStackView setAlpha:0];
    [_centeredStackView setHidden:YES];
#endif
    [self videoControlsManagerDidChange];

#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    [_bannerLabel setPreferredMaxLayoutWidth:self.view.bounds.size.width];
    [_banner setAlpha:0];
    [_banner setHidden:YES];
#endif

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

#if PLATFORM(VISION)
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
    [self._webView setFrame:[_animatingView bounds]];
#if ENABLE(FULLSCREEN_DISMISSAL_GESTURES)
    [_bannerLabel setPreferredMaxLayoutWidth:self.view.bounds.size.width];
#endif
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [self._webView _setInterfaceOrientationOverride:UIApplication.sharedApplication.statusBarOrientation];
        ALLOW_DEPRECATED_DECLARATIONS_END
    } completion:nil];
}

#if !PLATFORM(VISION)
- (UIStatusBarStyle)preferredStatusBarStyle
{
    return UIStatusBarStyleLightContent;
}
#endif

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

- (RefPtr<WebCore::PlatformPlaybackSessionInterface>) _playbackSessionInterface
{
    auto page = [self._webView _page];
    if (!page)
        return nullptr;

    WebKit::PlaybackSessionManagerProxy* playbackSessionManager = page->playbackSessionManager();
    if (!playbackSessionManager)
        return nullptr;

    return playbackSessionManager->controlsManagerInterface();
}

- (void)_pauseIfNeeded
{
    RefPtr page = self._webView._page.get();
    if (!page)
        return;

    if (page->preferences().fullScreenEnabled())
        return;

    // When only VideoFullscreenRequiresElementFullscreen is enabled,
    // mimic AVPlayerViewController's behavior by pausing playback when exiting fullscreen.
    ASSERT(page->preferences().videoFullscreenRequiresElementFullscreen());

    RefPtr interface = [self _playbackSessionInterface];
    if (!interface)
        return;

    auto model = interface->playbackSessionModel();
    if (!model)
        return;

    model->pause();
}

- (void)_cancelAction:(id)sender
{
    ASSERT(_valid);
    [[self delegate] requestExitFullScreen];
}

- (void)_togglePiPAction:(id)sender
{
    ASSERT(_valid);

    RefPtr playbackSessionInterface = [self _playbackSessionInterface];
    if (!playbackSessionInterface)
        return;

    if (auto* playbackSessionModel = playbackSessionInterface->playbackSessionModel())
        playbackSessionModel->togglePictureInPicture();
}

- (void)_enterVideoFullscreenAction:(id)sender
{
    ASSERT(_valid);
    RefPtr page = [self._webView _page].get();
    if (!page)
        return;

    [self hideUI];

    page->enterFullscreen();
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

    RefPtr page = self._webView._page.get();
    if (page && !page->preferences().fullScreenEnabled()) {
        ASSERT(page->preferences().videoFullscreenRequiresElementFullscreen());
        _secheuristic.reset();
        return;
    }

    NSString *alertTitle = WEB_UI_STRING("It looks like you are typing while in full screen", "Full Screen Deceptive Website Warning Sheet Title");
    NSString *alertMessage = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Typing is not allowed in full screen websites. “%@” may be showing a fake keyboard to trick you into disclosing personal or financial information.", "Full Screen Deceptive Website Warning Sheet Content Text"), (NSString *)self.location];
    auto alert = WebKit::createUIAlertController(alertTitle, alertMessage);

    if (page) {
        page->suspendAllMediaPlayback([] { });
        page->suspendActiveDOMObjectsAndAnimations();
    }

    UIAlertAction* exitAction = [UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("Exit Full Screen", "Exit Full Screen (Element Full Screen)", "Full Screen Deceptive Website Exit Action") style:UIAlertActionStyleCancel handler:^(UIAlertAction * action) {
        [self _cancelAction:action];
        if (RefPtr page = self._webView._page.get()) {
            page->resumeActiveDOMObjectsAndAnimations();
            page->resumeAllMediaPlayback([] { });
        }
    }];

    UIAlertAction* stayAction = [UIAlertAction actionWithTitle:WEB_UI_STRING_KEY("Stay in Full Screen", "Stay in Full Screen (Element Full Screen)", "Full Screen Deceptive Website Stay Action") style:UIAlertActionStyleDefault handler:^(UIAlertAction * action) {
        if (RefPtr page = self._webView._page.get()) {
            page->resumeActiveDOMObjectsAndAnimations();
            page->resumeAllMediaPlayback([] { });
        }
        _secheuristic.reset();
    }];

    [alert addAction:exitAction];
    [alert addAction:stayAction];
    [self presentViewController:alert.get() animated:YES completion:nil];
}

- (WKExtrinsicButton *)_createButtonWithExtrinsicContentSize:(CGSize)size
{
    WKExtrinsicButton *button = [WKExtrinsicButton buttonWithType:UIButtonTypeSystem];
    [self _setupButton:button];
    [button setDelegate:self];
    [button setExtrinsicContentSize:size];
    return button;
}

- (void)_setupButton:(UIButton *)button
{
    [button setTranslatesAutoresizingMaskIntoConstraints:NO];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [button setAdjustsImageWhenHighlighted:NO];
ALLOW_DEPRECATED_DECLARATIONS_END
    [button setTintColor:[UIColor whiteColor]];
}

- (void)wkExtrinsicButtonWillDisplayMenu:(WKExtrinsicButton *)button
{
    _isShowingMenu = YES;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
}

- (void)wkExtrinsicButtonWillDismissMenu:(WKExtrinsicButton *)button
{
    _isShowingMenu = NO;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(hideUI) object:nil];
    if (_playing)
        [self performSelector:@selector(hideUI) withObject:nil afterDelay:autoHideDelay];
}

@end

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
