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

#include "config.h"
#include "WKNumberPadViewController.h"

#if PLATFORM(WATCHOS)

#import "UIKitSPI.h"
#import "WKNumberPadView.h"
#import <PepperUICore/PUICQuickboardViewController_Private.h>
#import <PepperUICore/PUICResources.h>
#import <PepperUICore/UIDevice+PUICAdditions.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

static const CGFloat numberPadViewTopMargin = 30;
static const CGFloat headerButtonWidth = 24;
static const CGFloat inputLabelMinimumScale = 0.7;
static const CGFloat numberPadViewDismissAnimationDuration = 0.3;
static const NSTimeInterval numberPadDeleteKeyRepeatDelay = 0.35;
static const NSTimeInterval numberPadDeleteKeyRepeatInterval = 0.1;
static CGFloat inputLabelFontSize()
{
    if ([[UIDevice currentDevice] puic_deviceVariant] == PUICDeviceVariantCompact)
        return 16;
    return 18;
}

@implementation WKNumberPadViewController {
    RetainPtr<NSMutableString> _inputText;
    RetainPtr<WKNumberPadView> _numberPadView;
    WKNumberPadInputMode _inputMode;
    RetainPtr<UILabel> _inputLabel;
    RetainPtr<UIButton> _deleteButton;
    RetainPtr<UIButton> _backChevronButton;
    BOOL _shouldDismissWithFadeAnimation;
}

- (instancetype)initWithDelegate:(id <PUICQuickboardViewControllerDelegate>)delegate initialText:(NSString *)initialText inputMode:(WKNumberPadInputMode)inputMode
{
    if (!(self = [super initWithDelegate:delegate]))
        return nil;

    _inputText = adoptNS(initialText.mutableCopy);
    _inputMode = inputMode;
    _shouldDismissWithFadeAnimation = NO;
    return self;
}

- (void)dealloc
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [super dealloc];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _numberPadView = adoptNS([[WKNumberPadView alloc] initWithFrame:UIRectInset(self.contentView.bounds, numberPadViewTopMargin, 0, 0, 0) controller:self]);
    [self.contentView addSubview:_numberPadView.get()];

    _inputLabel = adoptNS([[UILabel alloc] init]);
    [_inputLabel setFont:[UIFont systemFontOfSize:inputLabelFontSize() weight:UIFontWeightSemibold]];
    [_inputLabel setTextColor:[UIColor whiteColor]];
    [_inputLabel setLineBreakMode:NSLineBreakByTruncatingHead];
    [_inputLabel setTextAlignment:NSTextAlignmentCenter];
    [_inputLabel setMinimumScaleFactor:inputLabelMinimumScale];
    [_inputLabel setAdjustsFontSizeToFitWidth:YES];
    [self.headerView addSubview:_inputLabel.get()];

    _deleteButton = [UIButton buttonWithType:UIButtonTypeCustom];
    UIImage *deleteButtonIcon = [[PUICResources imageNamed:@"keypad-delete-glyph" inBundle:[NSBundle bundleWithIdentifier:@"com.apple.PepperUICore"] shouldCache:YES] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [_deleteButton setImage:deleteButtonIcon forState:UIControlStateNormal];
    [_deleteButton setTintColor:[UIColor systemRedColor]];
    [_deleteButton addTarget:self action:@selector(_startDeletionTimer) forControlEvents:UIControlEventTouchDown];
    [_deleteButton addTarget:self action:@selector(_deleteButtonPressed) forControlEvents:UIControlEventTouchUpInside];
    [_deleteButton addTarget:self action:@selector(_cancelDeletionTimers) forControlEvents:UIControlEventTouchUpOutside];
    [self.headerView addSubview:_deleteButton.get()];

    _backChevronButton = [UIButton buttonWithType:UIButtonTypeCustom];
    UIImage *backChevronButtonIcon = [[PUICResources imageNamed:@"status-bar-chevron" inBundle:[NSBundle bundleWithIdentifier:@"com.apple.PepperUICore"] shouldCache:YES] imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [_backChevronButton setImage:backChevronButtonIcon forState:UIControlStateNormal];
    [_backChevronButton setTintColor:[UIColor systemGrayColor]];
    [_backChevronButton addTarget:self action:@selector(_cancelInput) forControlEvents:UIControlEventTouchUpInside];
    [self.headerView addSubview:_backChevronButton.get()];

    [self _reloadHeaderViewFromInputText];
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    [self _cancelDeletionTimers];
}

- (void)viewWillLayoutSubviews
{
    [super viewWillLayoutSubviews];

    [_inputLabel setFrame:UIRectInsetEdges(self.headerView.bounds, UIRectEdgeLeft | UIRectEdgeRight, headerButtonWidth)];
    [_deleteButton setFrame:CGRectMake(CGRectGetWidth(self.headerView.bounds) - headerButtonWidth, 0, headerButtonWidth, CGRectGetHeight(self.headerView.bounds))];
    [_backChevronButton setFrame:CGRectMake(0, 0, headerButtonWidth, CGRectGetHeight(self.headerView.bounds))];
}

- (void)_reloadHeaderViewFromInputText
{
    BOOL hasInputText = [_inputText length];
    self.cancelButton.hidden = hasInputText;
    [_deleteButton setHidden:!hasInputText];
    [_backChevronButton setHidden:!hasInputText];
    [_inputLabel setText:_inputText.get()];
}

- (void)didSelectKey:(WKNumberPadKey)key
{
    [self _handleKeyPress:key];
}

- (void)_handleKeyPress:(WKNumberPadKey)key
{
    switch (key) {
    case WKNumberPadKeyDash:
        [_inputText appendString:@"-"];
        break;
    case WKNumberPadKeyAsterisk:
        [_inputText appendString:@"*"];
        break;
    case WKNumberPadKeyOctothorpe:
        [_inputText appendString:@"#"];
        break;
    case WKNumberPadKeyClosingParenthesis:
        [_inputText appendString:@")"];
        break;
    case WKNumberPadKeyOpeningParenthesis:
        [_inputText appendString:@"("];
        break;
    case WKNumberPadKeyPlus:
        [_inputText appendString:@"+"];
        break;
    case WKNumberPadKeyAccept:
        [self.delegate quickboard:self textEntered:[[[NSAttributedString alloc] initWithString:_inputText.get()] autorelease]];
        return;
    case WKNumberPadKey0:
        [_inputText appendString:@"0"];
        break;
    case WKNumberPadKey1:
        [_inputText appendString:@"1"];
        break;
    case WKNumberPadKey2:
        [_inputText appendString:@"2"];
        break;
    case WKNumberPadKey3:
        [_inputText appendString:@"3"];
        break;
    case WKNumberPadKey4:
        [_inputText appendString:@"4"];
        break;
    case WKNumberPadKey5:
        [_inputText appendString:@"5"];
        break;
    case WKNumberPadKey6:
        [_inputText appendString:@"6"];
        break;
    case WKNumberPadKey7:
        [_inputText appendString:@"7"];
        break;
    case WKNumberPadKey8:
        [_inputText appendString:@"8"];
        break;
    case WKNumberPadKey9:
        [_inputText appendString:@"9"];
        break;
    default:
        break;
    }

    [self _cancelDeletionTimers];
    [self _reloadHeaderViewFromInputText];
}

- (void)_cancelInput
{
    _shouldDismissWithFadeAnimation = YES;
    [self.delegate quickboardInputCancelled:self];
}

- (void)_deleteLastInputCharacter
{
    if (![_inputText length])
        return;

    [_inputText deleteCharactersInRange:NSMakeRange([_inputText length] - 1, 1)];
    [self _reloadHeaderViewFromInputText];
}

- (void)_deleteButtonPressed
{
    [self _deleteLastInputCharacter];
    [self _cancelDeletionTimers];
}

- (void)_cancelDeletionTimers
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_startDeletionTimer) object:nil];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(_deletionTimerFired) object:nil];
}

- (void)_startDeletionTimer
{
    [self _cancelDeletionTimers];
    [self performSelector:@selector(_deletionTimerFired) withObject:nil afterDelay:numberPadDeleteKeyRepeatDelay];
}

- (void)_deletionTimerFired
{
    [self _cancelDeletionTimers];
    [self _deleteLastInputCharacter];
    if ([_inputText length])
        [self performSelector:@selector(_deletionTimerFired) withObject:nil afterDelay:numberPadDeleteKeyRepeatInterval];
}

#pragma mark - PUICQuickboardViewController overrides

- (void)addContentViewAnimations:(BOOL)isPresenting
{
    if (!_shouldDismissWithFadeAnimation) {
        [super addContentViewAnimations:isPresenting];
        return;
    }

    CABasicAnimation *fadeOutAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    fadeOutAnimation.fromValue = @1;
    fadeOutAnimation.toValue = @0;
    fadeOutAnimation.duration = numberPadViewDismissAnimationDuration;
    fadeOutAnimation.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut];
    [self.contentView addAnimation:fadeOutAnimation forKey:@"WebKitNumberPadFadeOutAnimationKey"];
    self.contentView.alpha = 0;
}

@end

#endif // PLATFORM(WATCHOS)
