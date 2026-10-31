/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKCapturePreviewViewController.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(MEDIA_STREAM) && !PLATFORM(APPLETV)

#import "CapturePreviewDeviceSelection.h"
#import "CapturePreviewLevelMeterState.h"
#import "CapturePreviewPromptModel.h"
#import "LayerHostingContext.h"
#import "Logging.h"
#import <WebCore/HostingContext.h>
#import <WebCore/LocalizedStrings.h>
#import <algorithm>
#import <cmath>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/MachSendRightAnnotated.h>
#import <wtf/MonotonicTime.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if USE(EXTENSIONKIT)
#import <BrowserEngineKit/BELayerHierarchyHostingView.h>
#endif

using PreviewDeviceType = WebKit::CapturePreviewDeviceType;

constexpr CGFloat previewAspectRatio = 4.0 / 3.0;
constexpr CGFloat contentSpacing = 16;
constexpr CGFloat contentInset = 20;
constexpr CGFloat informativeTextSpacing = 4;
constexpr CGFloat decisionButtonSpacing = 28;
constexpr CGFloat decisionButtonStackSpacing = 8;
constexpr CGFloat promptCornerRadius = 16;
constexpr CGFloat previewCornerRadius = 8;
constexpr CGFloat promptTopMargin = 24;
constexpr CGFloat promptSideMargin = 8;
constexpr CGFloat sectionCornerRadius = 12;
constexpr CGFloat sectionInset = 12;
constexpr CGFloat audioLevelIconSpacing = 10;
constexpr CGFloat previewBadgeInset = 8;
constexpr CGFloat previewBadgeHorizontalPadding = 8;
constexpr CGFloat previewBadgeVerticalPadding = 3;

constexpr CGFloat preferredContentWidth = 380;

static CGFloat promptWidthForAvailableWidth(CGFloat availableWidth)
{
    CGFloat width = availableWidth - 2 * promptSideMargin;
    return width > 0 ? std::min(preferredContentWidth, width) : preferredContentWidth;
}

@interface WKCapturePreviewLayerHostView : UIView
@end

@implementation WKCapturePreviewLayerHostView

+ (Class)layerClass
{
    return [CALayerHost class];
}

@end

@interface WKCapturePreviewAudioLevelView : UIView {
    WebKit::CapturePreviewLevelMeterState _state;
}
- (void)setLevel:(float)level;
- (void)reset;
@end

@implementation WKCapturePreviewAudioLevelView

- (instancetype)init
{
    if (!(self = [super initWithFrame:CGRectZero]))
        return nil;

    [self setOpaque:NO];
    [self setBackgroundColor:[UIColor clearColor]];
    [self setContentMode:UIViewContentModeRedraw];

    [self setIsAccessibilityElement:YES];
    [self setAccessibilityLabel:WEB_UI_STRING_KEY(@"Microphone level", "Microphone level (usermedia)", @"Accessibility label for the microphone level meter in the user media prompt").createNSString().get()];
    [self setAccessibilityTraits:UIAccessibilityTraitUpdatesFrequently];

    return self;
}

- (NSString *)accessibilityValue
{
    return [NSNumberFormatter localizedStringFromNumber:@(_state.levelFraction()) numberStyle:NSNumberFormatterPercentStyle];
}

- (CGSize)intrinsicContentSize
{
    return CGSizeMake(UIViewNoIntrinsicMetric, WebKit::CapturePreviewLevelMeterState::height);
}

- (void)setLevel:(float)level
{
    if (_state.update(level, MonotonicTime::now()))
        [self setNeedsDisplay];
}

- (void)reset
{
    if (_state.reset())
        [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect
{
    using State = WebKit::CapturePreviewLevelMeterState;
    CGRect bounds = [self bounds];

    for (unsigned index = 0; index < State::segmentCount; ++index) {
        switch (_state.segment(index)) {
        case State::Segment::Peak:
            [[UIColor systemBlueColor] setFill];
            break;
        case State::Segment::Lit:
            [[UIColor secondaryLabelColor] setFill];
            break;
        case State::Segment::Unlit:
            [[UIColor secondarySystemFillColor] setFill];
            break;
        }

        CGRect segment = CGRectMake(CGRectGetMinX(bounds) + State::segmentX(index, CGRectGetWidth(bounds)), CGRectGetMinY(bounds), State::segmentWidth, CGRectGetHeight(bounds));
        [[UIBezierPath bezierPathWithRoundedRect:segment cornerRadius:State::segmentWidth / 2] fill];
    }
}

@end

@interface WKCapturePreviewPresentationController : UIPresentationController {
    RetainPtr<UIView> _dimmingView;
}
@end

@implementation WKCapturePreviewPresentationController

- (CGRect)frameOfPresentedViewInContainerView
{
    RetainPtr containerView = [self containerView];
    if (!containerView)
        return CGRectZero;

    CGRect safeBounds = UIEdgeInsetsInsetRect([containerView bounds], [containerView safeAreaInsets]);
    CGSize contentSize = [[self presentedViewController] preferredContentSize];
    CGFloat width = std::min(contentSize.width, CGRectGetWidth(safeBounds));
    CGFloat height = std::max<CGFloat>(0, std::min(contentSize.height, CGRectGetHeight(safeBounds) - 2 * promptTopMargin));
    CGFloat originX = CGRectGetMinX(safeBounds) + std::round((CGRectGetWidth(safeBounds) - width) / 2);
    CGFloat originY = CGRectGetMinY(safeBounds) + promptTopMargin;
    return CGRectMake(originX, originY, width, height);
}

- (void)presentationTransitionWillBegin
{
    [super presentationTransitionWillBegin];

    RetainPtr containerView = [self containerView];
    _dimmingView = adoptNS([[UIView alloc] initWithFrame:[containerView bounds]]);
    [_dimmingView setBackgroundColor:[UIColor colorWithWhite:0 alpha:0.4]];
    [_dimmingView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
    [_dimmingView setAlpha:0];
    [containerView insertSubview:_dimmingView.get() atIndex:0];

    [[[self presentedViewController] transitionCoordinator] animateAlongsideTransition:[dimmingView = _dimmingView](id<UIViewControllerTransitionCoordinatorContext>) {
        [dimmingView setAlpha:1];
    } completion:nil];
}

- (void)dismissalTransitionWillBegin
{
    [super dismissalTransitionWillBegin];

    [[[self presentedViewController] transitionCoordinator] animateAlongsideTransition:[dimmingView = _dimmingView](id<UIViewControllerTransitionCoordinatorContext>) {
        [dimmingView setAlpha:0];
    } completion:nil];
}

- (void)containerViewWillLayoutSubviews
{
    [super containerViewWillLayoutSubviews];
    [[self presentedView] setFrame:[self frameOfPresentedViewInContainerView]];
}

@end

@interface WKCapturePreviewViewController () <UIViewControllerTransitioningDelegate>
@end

@implementation WKCapturePreviewViewController {
    RetainPtr<NSString> _alertTitle;
    RetainPtr<NSString> _informativeText;
    RetainPtr<NSString> _allowButtonTitle;
    RetainPtr<NSString> _denyButtonTitle;

    RetainPtr<UIStackView> _contentStack;
    RetainPtr<UIView> _previewContainer;
    RetainPtr<UIView> _previewBadge;
    RetainPtr<WKCapturePreviewLayerHostView> _previewLayerHost;
    WebKit::LayerHostingContextID _previewHostingContextID;
#if USE(EXTENSIONKIT)
    RetainPtr<BELayerHierarchyHostingView> _previewHostingView;
#endif
    RetainPtr<NSMutableArray<UIButton *>> _allowButtons;
    RetainPtr<UIButton> _videoButton;
    RetainPtr<UIButton> _audioButton;
    RetainPtr<WKCapturePreviewAudioLevelView> _audioLevelMeter;

    WebKit::CapturePreviewPromptModel _model;
    BOOL _shouldDismissOnceVisible;
    CGFloat _promptWidth;
    RetainPtr<UIScrollView> _scrollView;
    RetainPtr<UILabel> _titleLabel;
    RetainPtr<UILabel> _informativeLabel;

    Vector<WebKit::MediaPermissionPromptAdditionalAction> _additionalActions;

    CompletionHandler<void(WebKit::MediaPermissionPromptResult&&)> _decisionHandler;
}

- (instancetype)initWithAvailableSize:(CGSize)availableSize title:(NSString *)alertTitle informativeText:(NSString *)informativeText allowButtonTitle:(NSString *)allowButtonTitle denyButtonTitle:(NSString *)denyButtonTitle additionalActions:(Vector<WebKit::MediaPermissionPromptAdditionalAction>&&)additionalActions videoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices
{
    if (!(self = [super initWithNibName:nil bundle:nil]))
        return nil;

    _alertTitle = alertTitle;
    _informativeText = informativeText;
    _allowButtonTitle = allowButtonTitle;
    _denyButtonTitle = denyButtonTitle;
    _additionalActions = WTF::move(additionalActions);
    _model = { WTF::move(videoDevices), WTF::move(audioDevices) };

    _promptWidth = promptWidthForAvailableWidth(availableSize.width);

    self.modalPresentationStyle = UIModalPresentationCustom;
    self.transitioningDelegate = self;

    return self;
}

- (CGFloat)_contentWidth
{
    return _promptWidth - 2 * contentInset;
}

- (CGFloat)_previewWidth
{
    return [self _contentWidth] - 2 * sectionInset;
}

- (CGSize)previewSize
{
    CGFloat width = [self _previewWidth];
    return CGSizeMake(width, std::round(width / previewAspectRatio));
}

- (void)dealloc
{
    if (_decisionHandler) {
        RELEASE_LOG_ERROR(WebRTC, "WKCapturePreviewViewController was never answered, and probably never shown");
        _decisionHandler({ });
    }
    [super dealloc];
}

- (NSString *)selectedVideoDeviceID
{
    return nsStringNilIfEmpty(_model.selectedDeviceID(PreviewDeviceType::Camera)).autorelease();
}

- (NSString *)selectedAudioDeviceID
{
    return nsStringNilIfEmpty(_model.selectedDeviceID(PreviewDeviceType::Microphone)).autorelease();
}

- (void)setSelectionChangedHandler:(Function<void(std::optional<WebCore::CaptureDevice>&&, std::optional<WebCore::CaptureDevice>&&)>&&)handler
{
    _model.setSelectionChangedHandler(WTF::move(handler));
}

- (BOOL)isStopped
{
    return _model.isStopped();
}

- (void)setDecisionHandler:(CompletionHandler<void(WebKit::MediaPermissionPromptResult&&)>&&)handler
{
    ASSERT(!_decisionHandler);
    _decisionHandler = WTF::move(handler);
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    [[self view] setBackgroundColor:[UIColor systemGroupedBackgroundColor]];
    [[self view] setAccessibilityViewIsModal:YES];
    [[self view] setClipsToBounds:YES];
    [[[self view] layer] setCornerRadius:promptCornerRadius];

    RetainPtr titleLabel = adoptNS([[UILabel alloc] init]);
    _titleLabel = titleLabel;
    [titleLabel setText:_alertTitle.get()];
    [titleLabel setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
    [titleLabel setAdjustsFontForContentSizeCategory:YES];
    [titleLabel setNumberOfLines:0];
    [titleLabel setLineBreakMode:NSLineBreakByWordWrapping];

    // Without this a multi-line label reports a single line as its intrinsic size, so the prompt is
    // too short and the title is truncated rather than wrapped.
    [titleLabel setPreferredMaxLayoutWidth:[self _contentWidth]];
    [titleLabel setTextAlignment:NSTextAlignmentCenter];

    _previewContainer = adoptNS([[UIView alloc] init]);
    [_previewContainer setBackgroundColor:[UIColor blackColor]];
    [_previewContainer setClipsToBounds:YES];
    [[_previewContainer layer] setCornerRadius:previewCornerRadius];

    RetainPtr denyConfiguration = [UIButtonConfiguration grayButtonConfiguration];
    [denyConfiguration setTitle:_denyButtonTitle.get()];
    [denyConfiguration setCornerStyle:UIButtonConfigurationCornerStyleMedium];
    RetainPtr denyButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [denyButton setConfiguration:denyConfiguration.get()];
    [denyButton addTarget:self action:@selector(_deny) forControlEvents:UIControlEventTouchUpInside];

    RetainPtr allowConfiguration = [UIButtonConfiguration filledButtonConfiguration];
    [allowConfiguration setTitle:_allowButtonTitle.get()];
    [allowConfiguration setCornerStyle:UIButtonConfigurationCornerStyleMedium];
    RetainPtr allowButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [allowButton setConfiguration:allowConfiguration.get()];
    [allowButton addTarget:self action:@selector(_allow) forControlEvents:UIControlEventTouchUpInside];

    _allowButtons = adoptNS([[NSMutableArray alloc] initWithObjects:allowButton.get(), nil]);

    // Use a stack view so buttons the application adds line up with `Allow` and `Don't Allow` instead
    // of reading as a separate group.
    RetainPtr decisionButtons = adoptNS([[UIStackView alloc] init]);
    [decisionButtons setAxis:UILayoutConstraintAxisVertical];
    [decisionButtons setSpacing:decisionButtonStackSpacing];
    [decisionButtons addArrangedSubview:allowButton.get()];

    for (auto& action : _additionalActions) {
        RetainPtr configuration = [UIButtonConfiguration grayButtonConfiguration];
        [configuration setTitle:action.title.createNSString().get()];
        [configuration setCornerStyle:UIButtonConfigurationCornerStyleMedium];
        [configuration setTitleLineBreakMode:NSLineBreakByWordWrapping];

        RetainPtr additionalAction = [UIAction actionWithTitle:@"" image:nil identifier:nil handler:[weakSelf = WeakObjCPtr<WKCapturePreviewViewController>(self), promptAction = action](UIAction *) {
            [weakSelf.get() _activateAdditionalAction:promptAction];
        }];
        RetainPtr additionalButton = [UIButton buttonWithConfiguration:configuration.get() primaryAction:additionalAction.get()];
        [decisionButtons addArrangedSubview:additionalButton.get()];

        if (action.decision == WebKit::MediaPermissionPromptAdditionalAction::Decision::Allow)
            [_allowButtons addObject:additionalButton.get()];
    }

    [decisionButtons addArrangedSubview:denyButton.get()];

    _contentStack = adoptNS([[UIStackView alloc] init]);
    [_contentStack setAxis:UILayoutConstraintAxisVertical];
    [_contentStack setSpacing:contentSpacing];
    [_contentStack setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_contentStack addArrangedSubview:titleLabel.get()];

    RetainPtr<UIView> lastViewBeforeDecisionButtons = titleLabel;

    if (_informativeText) {
        RetainPtr informativeLabel = adoptNS([[UILabel alloc] init]);
        _informativeLabel = informativeLabel;
        [informativeLabel setText:_informativeText.get()];
        [informativeLabel setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]];
        [informativeLabel setAdjustsFontForContentSizeCategory:YES];
        [informativeLabel setNumberOfLines:0];
        [informativeLabel setLineBreakMode:NSLineBreakByWordWrapping];
        [informativeLabel setPreferredMaxLayoutWidth:[self _contentWidth]];
        [informativeLabel setTextAlignment:NSTextAlignmentCenter];
        [_contentStack addArrangedSubview:informativeLabel.get()];
        [_contentStack setCustomSpacing:informativeTextSpacing afterView:titleLabel.get()];
        lastViewBeforeDecisionButtons = informativeLabel;
    }

    if (_model.requestNeeds(PreviewDeviceType::Camera)) {
        _videoButton = [self _createDeviceButtonForDeviceType:PreviewDeviceType::Camera];

        _previewBadge = adoptNS([[UIView alloc] init]);
        [_previewBadge setBackgroundColor:[UIColor systemBlueColor]];
        [_previewBadge setUserInteractionEnabled:NO];
        [_previewBadge setTranslatesAutoresizingMaskIntoConstraints:NO];

        RetainPtr badgeLabel = adoptNS([[UILabel alloc] init]);
        [badgeLabel setText:WEB_UI_STRING_KEY(@"Preview", "Preview (usermedia)", @"Badge label identifying the live camera preview in the user media prompt").createNSString().get()];
        [badgeLabel setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleCaption1]];
        [badgeLabel setAdjustsFontForContentSizeCategory:YES];
        [badgeLabel setTextColor:[UIColor whiteColor]];
        [badgeLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
        [_previewBadge addSubview:badgeLabel.get()];
        [_previewContainer addSubview:_previewBadge.get()];
        [NSLayoutConstraint activateConstraints:@[
            [[badgeLabel topAnchor] constraintEqualToAnchor:[_previewBadge topAnchor] constant:previewBadgeVerticalPadding],
            [[badgeLabel bottomAnchor] constraintEqualToAnchor:[_previewBadge bottomAnchor] constant:-previewBadgeVerticalPadding],
            [[badgeLabel leadingAnchor] constraintEqualToAnchor:[_previewBadge leadingAnchor] constant:previewBadgeHorizontalPadding],
            [[badgeLabel trailingAnchor] constraintEqualToAnchor:[_previewBadge trailingAnchor] constant:-previewBadgeHorizontalPadding],
            [[_previewBadge topAnchor] constraintEqualToAnchor:[_previewContainer topAnchor] constant:previewBadgeInset],
            [[_previewBadge leadingAnchor] constraintEqualToAnchor:[_previewContainer leadingAnchor] constant:previewBadgeInset],
        ]];

        RetainPtr cameraStack = adoptNS([[UIStackView alloc] init]);
        [cameraStack setAxis:UILayoutConstraintAxisVertical];
        [cameraStack setSpacing:sectionInset];
        [cameraStack addArrangedSubview:_previewContainer.get()];
        [cameraStack addArrangedSubview:_videoButton.get()];

        RetainPtr cameraSection = [self _createSectionWithContentView:cameraStack.get()];
        [_contentStack addArrangedSubview:cameraSection.get()];
        lastViewBeforeDecisionButtons = cameraSection;
    }

    if (_model.requestNeeds(PreviewDeviceType::Microphone)) {
        _audioButton = [self _createDeviceButtonForDeviceType:PreviewDeviceType::Microphone];

        RetainPtr microphoneIcon = adoptNS([[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"mic" withConfiguration:[UIImageSymbolConfiguration configurationWithTextStyle:UIFontTextStyleBody]]]);
        [microphoneIcon setTintColor:[UIColor secondaryLabelColor]];
        [microphoneIcon setIsAccessibilityElement:NO];
        [microphoneIcon setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];

        _audioLevelMeter = adoptNS([[WKCapturePreviewAudioLevelView alloc] init]);

        RetainPtr meterRow = adoptNS([[UIStackView alloc] init]);
        [meterRow setAxis:UILayoutConstraintAxisHorizontal];
        [meterRow setAlignment:UIStackViewAlignmentCenter];
        [meterRow setSpacing:audioLevelIconSpacing];
        [meterRow addArrangedSubview:microphoneIcon.get()];
        [meterRow addArrangedSubview:_audioLevelMeter.get()];

        RetainPtr microphoneStack = adoptNS([[UIStackView alloc] init]);
        [microphoneStack setAxis:UILayoutConstraintAxisVertical];
        [microphoneStack setSpacing:sectionInset];
        [microphoneStack addArrangedSubview:meterRow.get()];
        [microphoneStack addArrangedSubview:_audioButton.get()];

        RetainPtr microphoneSection = [self _createSectionWithContentView:microphoneStack.get()];
        [_contentStack addArrangedSubview:microphoneSection.get()];
        lastViewBeforeDecisionButtons = microphoneSection;
    }

    [_contentStack addArrangedSubview:decisionButtons.get()];
    [_contentStack setCustomSpacing:decisionButtonSpacing afterView:lastViewBeforeDecisionButtons.get()];

    _scrollView = adoptNS([[UIScrollView alloc] init]);
    [_scrollView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_scrollView addSubview:_contentStack.get()];
    [[self view] addSubview:_scrollView.get()];

    RetainPtr view = [self view];
    RetainPtr contentGuide = [_scrollView contentLayoutGuide];
    RetainPtr frameGuide = [_scrollView frameLayoutGuide];
    NSMutableArray *constraints = [NSMutableArray array];
    [constraints addObjectsFromArray:@[
        [[_scrollView topAnchor] constraintEqualToAnchor:[view topAnchor]],
        [[_scrollView bottomAnchor] constraintEqualToAnchor:[view bottomAnchor]],
        [[_scrollView leadingAnchor] constraintEqualToAnchor:[view leadingAnchor]],
        [[_scrollView trailingAnchor] constraintEqualToAnchor:[view trailingAnchor]],
        [[_contentStack topAnchor] constraintEqualToAnchor:[contentGuide topAnchor] constant:contentInset],
        [[_contentStack bottomAnchor] constraintEqualToAnchor:[contentGuide bottomAnchor] constant:-contentInset],
        [[_contentStack leadingAnchor] constraintEqualToAnchor:[frameGuide leadingAnchor] constant:contentInset],
        [[_contentStack trailingAnchor] constraintEqualToAnchor:[frameGuide trailingAnchor] constant:-contentInset],
    ]];
    if (_model.requestNeeds(PreviewDeviceType::Camera))
        [constraints addObject:[[_previewContainer widthAnchor] constraintEqualToAnchor:[_previewContainer heightAnchor] multiplier:previewAspectRatio]];
    [NSLayoutConstraint activateConstraints:constraints];

    [self _updateAllowButtonEnablement];
    [self _updatePreferredContentSize];

    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ] withTarget:self action:@selector(_contentSizeCategoryDidChange)];
}

- (void)_contentSizeCategoryDidChange
{
    [self _updatePreferredContentSize];
    [[[self presentationController] containerView] setNeedsLayout];
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];

    if (_shouldDismissOnceVisible) {
        _shouldDismissOnceVisible = NO;
        [[self presentingViewController] dismissViewControllerAnimated:YES completion:nil];
        return;
    }

    [self becomeFirstResponder];

    if ([_scrollView contentSize].height > CGRectGetHeight([_scrollView bounds]))
        [_scrollView flashScrollIndicators];

    _model.beginPreview();
}

- (void)dismissWhenPossible
{
    if ([self isBeingPresented]) {
        // UIKit ignores a dismissal while the presentation is still animating, which would leave a prompt
        // on screen whose buttons no longer do anything.
        _shouldDismissOnceVisible = YES;
        return;
    }

    [[self presentingViewController] dismissViewControllerAnimated:YES completion:nil];
}

- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

    [coordinator animateAlongsideTransition:nil completion:[weakSelf = WeakObjCPtr<WKCapturePreviewViewController>(self)](id<UIViewControllerTransitionCoordinatorContext>) {
        [weakSelf.get() _availableSizeDidChange];
    }];
}

- (void)_availableSizeDidChange
{
    CGFloat promptWidth = promptWidthForAvailableWidth([[self presentingViewController] view].safeAreaLayoutGuide.layoutFrame.size.width);
    if (promptWidth == _promptWidth)
        return;

    _promptWidth = promptWidth;
    [_titleLabel setPreferredMaxLayoutWidth:[self _contentWidth]];
    [_informativeLabel setPreferredMaxLayoutWidth:[self _contentWidth]];
    [self _updatePreferredContentSize];
    [[[self presentationController] containerView] setNeedsLayout];

    _model.notifySelectionChanged();
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];

    [[_previewBadge layer] setCornerRadius:CGRectGetHeight([_previewBadge bounds]) / 2];
}

- (void)_updatePreferredContentSize
{
    CGSize fittingSize = [_contentStack systemLayoutSizeFittingSize:CGSizeMake([self _contentWidth], UILayoutFittingCompressedSize.height) withHorizontalFittingPriority:UILayoutPriorityRequired verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
    self.preferredContentSize = CGSizeMake(_promptWidth, fittingSize.height + 2 * contentInset);
}

- (UIPresentationController *)presentationControllerForPresentedViewController:(UIViewController *)presented presentingViewController:(UIViewController *)presenting sourceViewController:(UIViewController *)source
{
    return adoptNS([[WKCapturePreviewPresentationController alloc] initWithPresentedViewController:presented presentingViewController:presenting]).autorelease();
}

- (RetainPtr<UIView>)_createSectionWithContentView:(UIView *)contentView
{
    RetainPtr section = adoptNS([[UIView alloc] init]);
    [section setBackgroundColor:[UIColor secondarySystemGroupedBackgroundColor]];
    [[section layer] setCornerRadius:sectionCornerRadius];

    [contentView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [section addSubview:contentView];
    [NSLayoutConstraint activateConstraints:@[
        [[contentView topAnchor] constraintEqualToAnchor:[section topAnchor] constant:sectionInset],
        [[contentView bottomAnchor] constraintEqualToAnchor:[section bottomAnchor] constant:-sectionInset],
        [[contentView leadingAnchor] constraintEqualToAnchor:[section leadingAnchor] constant:sectionInset],
        [[contentView trailingAnchor] constraintEqualToAnchor:[section trailingAnchor] constant:-sectionInset],
    ]];

    return section;
}

- (RetainPtr<UIButton>)_createDeviceButtonForDeviceType:(PreviewDeviceType)deviceType
{
    RetainPtr button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setShowsMenuAsPrimaryAction:YES];

    RetainPtr configuration = [UIButtonConfiguration grayButtonConfiguration];
    [configuration setImage:[UIImage systemImageNamed:@"chevron.up.chevron.down"]];
    [configuration setImagePlacement:NSDirectionalRectEdgeTrailing];
    [configuration setImagePadding:contentSpacing / 2];
    [configuration setCornerStyle:UIButtonConfigurationCornerStyleMedium];
    [button setConfiguration:configuration.get()];

    if (deviceType == PreviewDeviceType::Camera)
        [button setAccessibilityLabel:WEB_UI_STRING_KEY(@"Camera", "Camera (usermedia)", @"Accessibility label for the camera menu in the user media prompt").createNSString().get()];
    else
        [button setAccessibilityLabel:WEB_UI_STRING_KEY(@"Microphone", "Microphone (usermedia)", @"Accessibility label for the microphone menu in the user media prompt").createNSString().get()];

    [self _updateMenuForButton:button.get() deviceType:deviceType];
    return button;
}

- (void)_updateMenuForButton:(UIButton *)button deviceType:(PreviewDeviceType)deviceType
{
    auto& devices = _model.devices(deviceType);
    size_t selectedIndex = _model.selectedIndex(deviceType);

    // Actions are built per device rather than from labels so that two devices with the same label still
    // get separate entries, and so the index survives a device list update.
    NSMutableArray<UIAction *> *actions = [NSMutableArray arrayWithCapacity:devices.size()];
    for (size_t index = 0; index < devices.size(); ++index) {
        RetainPtr title = devices[index].label().createNSString();
        RetainPtr action = [UIAction actionWithTitle:title.get() image:nil identifier:nil handler:[weakSelf = WeakObjCPtr<WKCapturePreviewViewController>(self), deviceType, index](UIAction *) {
            RetainPtr strongSelf = weakSelf.get();
            if (!strongSelf)
                return;
            [strongSelf _didSelectDeviceAtIndex:index deviceType:deviceType];
        }];

        [action setState:index == selectedIndex ? UIMenuElementStateOn : UIMenuElementStateOff];
        [actions addObject:action.get()];
    }

    [button setMenu:[UIMenu menuWithTitle:@"" children:actions]];

    // Hide the button if a device is no longer available
    [button setHidden:devices.isEmpty()];
    [button setEnabled:devices.size() > 1];
    if (selectedIndex < devices.size()) {
        RetainPtr title = devices[selectedIndex].label().createNSString();
        RetainPtr configuration = [button configuration];
        [configuration setTitle:title.get()];
        [button setConfiguration:configuration.get()];
        [button setAccessibilityValue:title.get()];
    }
}

- (void)_didSelectDeviceAtIndex:(size_t)index deviceType:(PreviewDeviceType)deviceType
{
    if (index >= _model.devices(deviceType).size())
        return;

    String previousDeviceID = _model.selectedDeviceID(deviceType);
    _model.select(deviceType, index);
    if (deviceType == PreviewDeviceType::Microphone && _model.selectedDeviceID(deviceType) != previousDeviceID)
        [_audioLevelMeter reset];
    [self _updateMenuForButton:deviceType == PreviewDeviceType::Camera ? _videoButton.get() : _audioButton.get() deviceType:deviceType];
    _model.notifySelectionChanged();
}

- (void)setAudioLevel:(float)level
{
    [_audioLevelMeter setLevel:level];
}

- (void)setPreviewHostingContext:(const WebCore::HostingContext&)hostingContext gpuProcessIdentifier:(ProcessID)gpuProcessIdentifier
{
    if (_model.shouldIgnoreHostingContext(hostingContext))
        return;

    if (_previewLayerHost && hostingContext.contextID == _previewHostingContextID)
        return;

    _previewHostingContextID = 0;

    [_previewLayerHost removeFromSuperview];
    _previewLayerHost = nil;
#if USE(EXTENSIONKIT)
    _previewHostingView = nil;
#endif

    if (!hostingContext.contextID)
        return;

    _previewLayerHost = adoptNS([[WKCapturePreviewLayerHostView alloc] init]);
    [_previewLayerHost setUserInteractionEnabled:NO];
    [_previewLayerHost setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_previewContainer insertSubview:_previewLayerHost.get() atIndex:0];
    [NSLayoutConstraint activateConstraints:@[
        [[_previewLayerHost topAnchor] constraintEqualToAnchor:[_previewContainer topAnchor]],
        [[_previewLayerHost bottomAnchor] constraintEqualToAnchor:[_previewContainer bottomAnchor]],
        [[_previewLayerHost leadingAnchor] constraintEqualToAnchor:[_previewContainer leadingAnchor]],
        [[_previewLayerHost trailingAnchor] constraintEqualToAnchor:[_previewContainer trailingAnchor]],
    ]];

#if USE(EXTENSIONKIT)
    _previewHostingView = adoptNS([[BELayerHierarchyHostingView alloc] init]);
    [_previewHostingView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_previewLayerHost addSubview:_previewHostingView.get()];
    [NSLayoutConstraint activateConstraints:@[
        [[_previewHostingView topAnchor] constraintEqualToAnchor:[_previewLayerHost topAnchor]],
        [[_previewHostingView bottomAnchor] constraintEqualToAnchor:[_previewLayerHost bottomAnchor]],
        [[_previewHostingView leadingAnchor] constraintEqualToAnchor:[_previewLayerHost leadingAnchor]],
        [[_previewHostingView trailingAnchor] constraintEqualToAnchor:[_previewLayerHost trailingAnchor]],
    ]];

    RetainPtr<BELayerHierarchyHandle> layerHandle;
#if ENABLE(MACH_PORT_LAYER_HOSTING)
    UNUSED_PARAM(gpuProcessIdentifier);
    layerHandle = WebKit::LayerHostingContext::createHostingHandle(WTF::MachSendRightAnnotated { hostingContext.sendRightAnnotated });
#else
    layerHandle = WebKit::LayerHostingContext::createHostingHandle(gpuProcessIdentifier, hostingContext.contextID);
#endif
    if (!layerHandle) {
        RELEASE_LOG_ERROR(WebRTC, "CapturePreview unable to create a layer hosting handle for contextID=%u", hostingContext.contextID);
        return;
    }
    [_previewHostingView setHandle:layerHandle.get()];
#else
    UNUSED_PARAM(gpuProcessIdentifier);
    [checked_objc_cast<CALayerHost>([_previewLayerHost layer]) setContextId:hostingContext.contextID];
#endif
    _previewHostingContextID = hostingContext.contextID;
}

- (void)updateWithVideoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices
{
    if (_model.hasSameDevices(videoDevices, audioDevices))
        return;

    String previousMicrophoneID = _model.selectedDeviceID(PreviewDeviceType::Microphone);
    bool previewNeedsRestart = _model.update(WTF::move(videoDevices), WTF::move(audioDevices));

    if (_videoButton)
        [self _updateMenuForButton:_videoButton.get() deviceType:PreviewDeviceType::Camera];
    if (_audioButton)
        [self _updateMenuForButton:_audioButton.get() deviceType:PreviewDeviceType::Microphone];

    if (_model.selectedDeviceID(PreviewDeviceType::Microphone) != previousMicrophoneID)
        [_audioLevelMeter reset];

    if ([self isViewLoaded]) {
        [self _updateAllowButtonEnablement];
        [self _updatePreferredContentSize];
    }

    if (previewNeedsRestart)
        _model.notifySelectionChanged();
}

- (void)_updateAllowButtonEnablement
{
    bool canStillSatisfyRequest = _model.canStillSatisfyRequest();
    [_allowButtons enumerateObjectsUsingBlock:^(UIButton *allowButton, NSUInteger, BOOL *) {
        [allowButton setEnabled:canStillSatisfyRequest];
    }];
}

- (void)_allow
{
    if (auto handler = std::exchange(_decisionHandler, { }))
        handler({ .granted = true, .wasAnswered = true });
}

- (void)_deny
{
    if (auto handler = std::exchange(_decisionHandler, { }))
        handler({ .granted = false, .wasAnswered = true });
}

- (BOOL)accessibilityPerformEscape
{
    // Escape is Don't Allow, as on macOS.
    [self _deny];
    return YES;
}

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (NSArray<UIKeyCommand *> *)keyCommands
{
    return @[ [UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:0 action:@selector(_deny)] ];
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];

    if ([self isBeingDismissed] || ![self presentingViewController])
        [self dismissWithoutAnswer];
}

- (void)dismissWithoutAnswer
{
    if (auto handler = std::exchange(_decisionHandler, { }))
        handler({ });
}

- (void)_activateAdditionalAction:(const WebKit::MediaPermissionPromptAdditionalAction&)action
{
    bool granted = action.decision == WebKit::MediaPermissionPromptAdditionalAction::Decision::Allow;
    if (auto handler = std::exchange(_decisionHandler, { }))
        handler({ .granted = granted, .wasAnswered = true, .chosenAction = action.source });
}

- (void)stop
{
    _model.stop();
    [_audioLevelMeter reset];
    [self setPreviewHostingContext:WebCore::HostingContext { } gpuProcessIdentifier:0];
}

@end

#endif // PLATFORM(IOS_FAMILY) && ENABLE(MEDIA_STREAM) && !PLATFORM(APPLETV)
