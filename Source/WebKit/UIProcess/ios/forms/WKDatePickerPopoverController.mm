/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WKDatePickerPopoverController.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitUtilities.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

@interface WKDatePickerPopoverView : UIView

@property (readonly, nonatomic) UIDatePicker *datePicker;
@property (readonly, nonatomic) UIToolbar *accessoryView;

@end

@implementation WKDatePickerPopoverView {
    RetainPtr<UIVisualEffectView> _backgroundView;
    __weak UIDatePicker *_datePicker;
    RetainPtr<UIToolbar> _accessoryView;
    CGSize _contentSize;
}

- (instancetype)initWithDatePicker:(UIDatePicker *)datePicker
{
    if (!(self = [super initWithFrame:CGRectZero]))
        return nil;

    UIBlurEffect *blurEffect = nil;
#if !PLATFORM(APPLETV)
    blurEffect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
#endif
    _backgroundView = adoptNS([[UIVisualEffectView alloc] initWithEffect:blurEffect]);
    _datePicker = datePicker;
    _accessoryView = adoptNS([UIToolbar new]);

    [_backgroundView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:_backgroundView.get()];

    static constexpr auto marginSize = 16;
    _datePicker.translatesAutoresizingMaskIntoConstraints = NO;
    _datePicker.layoutMargins = UIEdgeInsetsMake(marginSize, marginSize, marginSize, marginSize);
    [_datePicker sizeToFit];
    [[_backgroundView contentView] addSubview:_datePicker];

    [_accessoryView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_accessoryView setStandardAppearance:^{
        auto appearance = adoptNS([UIToolbarAppearance new]);
        [appearance setBackgroundEffect:nil];
        return appearance.autorelease();
    }()];
    [_accessoryView sizeToFit];
    [[_backgroundView contentView] addSubview:_accessoryView.get()];

    CGFloat toolbarBottomMargin = _datePicker.datePickerMode == UIDatePickerModeDateAndTime ? 8 : 2;
    auto datePickerSize = [_datePicker bounds].size;
    auto accessoryViewSize = [_accessoryView bounds].size;

    _contentSize.width = 2 * marginSize + std::max<CGFloat>(datePickerSize.width, accessoryViewSize.width);
    _contentSize.height = toolbarBottomMargin + 2 * marginSize + datePickerSize.height + accessoryViewSize.height;

    [NSLayoutConstraint activateConstraints:@[
        [self.widthAnchor constraintEqualToConstant:_contentSize.width],
        [self.heightAnchor constraintEqualToConstant:_contentSize.height],
        [[_backgroundView leadingAnchor] constraintEqualToAnchor:self.leadingAnchor],
        [[_backgroundView trailingAnchor] constraintEqualToAnchor:self.trailingAnchor],
        [[_backgroundView topAnchor] constraintEqualToAnchor:self.topAnchor],
        [[_backgroundView bottomAnchor] constraintEqualToAnchor:self.bottomAnchor constant:-toolbarBottomMargin],
        [[_datePicker heightAnchor] constraintEqualToConstant:datePickerSize.height],
        [[_datePicker leadingAnchor] constraintEqualToAnchor:self.leadingAnchor],
        [[_datePicker trailingAnchor] constraintEqualToAnchor:self.trailingAnchor],
        [[_datePicker topAnchor] constraintEqualToAnchor:self.topAnchor],
        [[_datePicker bottomAnchor] constraintEqualToSystemSpacingBelowAnchor:[_accessoryView topAnchor] multiplier:1],
        [[_accessoryView leadingAnchor] constraintEqualToAnchor:self.leadingAnchor],
        [[_accessoryView trailingAnchor] constraintEqualToAnchor:self.trailingAnchor],
        [[_accessoryView heightAnchor] constraintEqualToConstant:accessoryViewSize.height],
        [[_accessoryView bottomAnchor] constraintEqualToAnchor:[_backgroundView bottomAnchor]],
    ]];

    return self;
}

- (UIDatePicker *)datePicker
{
    return _datePicker;
}

- (UIToolbar *)accessoryView
{
    return _accessoryView.get();
}

- (CGSize)estimatedMaximumPopoverSize
{
    constexpr auto additionalHeightToAvoidClippingToolbar = 60;
    return CGSize {
        _contentSize.width,
        _contentSize.height + additionalHeightToAvoidClippingToolbar
    };
}

@end

@interface WKDatePickerPopoverController () <UIPopoverPresentationControllerDelegate>

@end

@implementation WKDatePickerPopoverController {
    RetainPtr<WKDatePickerPopoverView> _contentView;
    __weak id<WKDatePickerPopoverControllerDelegate> _delegate;
    BOOL _canSendPopoverControllerDidDismiss;
}

- (instancetype)initWithDatePicker:(UIDatePicker *)datePicker delegate:(id<WKDatePickerPopoverControllerDelegate>)delegate
{
    if (!(self = [super init]))
        return nil;

    _contentView = adoptNS([[WKDatePickerPopoverView alloc] initWithDatePicker:datePicker]);
    _delegate = delegate;
    self.modalPresentationStyle = UIModalPresentationPopover;
    self.popoverPresentationController.delegate = self;
    return self;
}

- (void)resetDatePicker
{
    [_delegate datePickerPopoverControllerDidReset:self];
}

- (void)dismissDatePicker
{
    [self.presentingViewController dismissViewControllerAnimated:YES completion:[strongSelf = retainPtr(self)] {
        [strongSelf _dispatchPopoverControllerDidDismissIfNeeded];
    }];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    auto resetButton = adoptNS([[UIBarButtonItem alloc] initWithTitle:WEB_UI_STRING_KEY("Reset", "Reset Button Date/Time Context Menu", "Reset button in date input context menu") style:UIBarButtonItemStylePlain target:self action:@selector(resetDatePicker)]);
    auto doneButton = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(dismissDatePicker)]);

    [_contentView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_contentView accessoryView].items = @[ resetButton.get(), UIBarButtonItem.flexibleSpaceItem, doneButton.get() ];

    [self.view addSubview:_contentView.get()];

    [NSLayoutConstraint activateConstraints:@[
        [[_contentView leadingAnchor] constraintEqualToAnchor:self.view.leadingAnchor],
        [[_contentView trailingAnchor] constraintEqualToAnchor:self.view.trailingAnchor],
        [[_contentView topAnchor] constraintEqualToAnchor:self.view.topAnchor],
        [[_contentView bottomAnchor] constraintGreaterThanOrEqualToAnchor:self.view.layoutMarginsGuide.bottomAnchor]
    ]];

    self.preferredContentSize = [_contentView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
}

- (void)presentInView:(UIView *)view sourceRect:(CGRect)rect completion:(void(^)())completion
{
    RetainPtr controller = [view _wk_viewControllerForFullScreenPresentation];

    while ([controller isBeingDismissed]) {
        // When tabbing between date pickers, it's possible for the view controller for fullscreen presentation
        // to be the previously focused input's popover. To ensure that we don't try to present our new
        // `WKDatePickerPopoverController` out of the previous `WKDatePickerPopoverController`, march up the
        // -presentingViewController chain until we find a view controller that isn't being dismissed.
        controller = [controller presentingViewController];
    }

    if (!controller)
        return completion();

    _canSendPopoverControllerDidDismiss = YES;
    // Specifying arrow directions is necessary here because UIKit will prefer to show the popover below the
    // source rect and cover the source rect with the arrow, even if `-canOverlapSourceViewRect` is `NO`.
    // To prevent the popover arrow from covering the element, we force UIKit to choose the direction that
    // has the most available space, so won't cover the element with the popover arrow unless we really have
    // no other choice.
    auto rectInWindow = [view convertRect:rect toCoordinateSpace:view.window];
    auto windowBounds = view.window.bounds;
    auto distanceFromTop = CGRectGetMinY(rectInWindow) - CGRectGetMinY(windowBounds);
    auto distanceFromLeft = CGRectGetMinX(rectInWindow) - CGRectGetMinX(windowBounds);
    auto distanceFromRight = CGRectGetMaxX(windowBounds) - CGRectGetMaxX(rectInWindow);
    auto distanceFromBottom = CGRectGetMaxY(windowBounds) - CGRectGetMaxY(rectInWindow);
    auto estimatedMaximumPopoverSize = [_contentView estimatedMaximumPopoverSize];

    auto canContainPopover = [&](CGFloat width, CGFloat height) {
        return estimatedMaximumPopoverSize.width < width && estimatedMaximumPopoverSize.height < height;
    };

    auto presentationController = self.popoverPresentationController;
    UIPopoverArrowDirection permittedDirections = 0;
    if (canContainPopover(CGRectGetWidth(windowBounds), distanceFromTop))
        permittedDirections |= UIPopoverArrowDirectionDown;
    if (canContainPopover(distanceFromLeft, CGRectGetHeight(windowBounds)))
        permittedDirections |= UIPopoverArrowDirectionRight;
    if (canContainPopover(distanceFromRight, CGRectGetHeight(windowBounds)))
        permittedDirections |= UIPopoverArrowDirectionLeft;
    if (canContainPopover(CGRectGetWidth(windowBounds), distanceFromBottom))
        permittedDirections |= UIPopoverArrowDirectionUp;

    presentationController.permittedArrowDirections = permittedDirections;
    presentationController.sourceView = view;
    if (permittedDirections)
        presentationController.sourceRect = rect;
    else
        presentationController.sourceRect = [view convertRect:view.window.bounds fromCoordinateSpace:view.window];
    [controller presentViewController:self animated:YES completion:completion];
}

- (void)_dispatchPopoverControllerDidDismissIfNeeded
{
    if (std::exchange(_canSendPopoverControllerDidDismiss, NO))
        [_delegate datePickerPopoverControllerDidDismiss:self];
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (UIModalPresentationStyle)adaptivePresentationStyleForPresentationController:(UIPresentationController *)controller traitCollection:(UITraitCollection *)traitCollection
{
    return UIModalPresentationNone;
}

- (void)presentationControllerDidDismiss:(UIPresentationController *)presentationController
{
    [self _dispatchPopoverControllerDidDismissIfNeeded];
}

@end

#endif // PLATFORM(IOS_FAMILY)
