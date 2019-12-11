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
#include "WKQuickboardListViewController.h"

#if PLATFORM(WATCHOS)

#import <PepperUICore/PUICActionController.h>
#import <PepperUICore/PUICActionGroup.h>
#import <PepperUICore/PUICApplication_Private.h>
#import <PepperUICore/PUICQuickboardLanguageController.h>
#import <PepperUICore/PUICQuickboardViewController_Private.h>
#import <PepperUICore/PUICStatusBarAppContextView.h>
#import <wtf/RetainPtr.h>

static const CGFloat itemCellTopToLabelBaseline = 26;
static const CGFloat itemCellBaselineToBottom = 8;

@implementation WKQuickboardListItemCell

- (CGFloat)topToLabelBaselineSpecValue
{
    return itemCellTopToLabelBaseline;
}

- (CGFloat)baselineToBottomSpecValue
{
    return itemCellBaselineToBottom;
}

@end

@interface WKQuickboardListViewController () <PUICQuickboardLanguageControllerDelegate>
@end

@implementation WKQuickboardListViewController {
    BOOL _contextViewNeedsUpdate;
    RetainPtr<UIView> _contextView;
    RetainPtr<PUICStatusBarGlobalContextViewAssertion> _statusBarAssertion;
}

@dynamic delegate;

- (instancetype)initWithDelegate:(id <WKQuickboardViewControllerDelegate>)delegate
{
    if (self = [super initWithDelegate:delegate dictationMode:PUICDictationModeText])
        _contextViewNeedsUpdate = YES;

    return self;
}

- (void)updateContextViewIfNeeded
{
    if (!_contextViewNeedsUpdate)
        return;

    auto previousContextView = _contextView;
    if ([self.delegate shouldDisplayInputContextViewForListViewController:self])
        _contextView = [self.delegate inputContextViewForViewController:self];
    else
        _contextView = nil;

    _contextViewNeedsUpdate = NO;
}

- (BOOL)prefersStatusBarHidden
{
    return NO;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.headerView.hidden = YES;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    _statusBarAssertion = [[PUICApplication sharedPUICApplication] _takeStatusBarGlobalContextAssertionAnimated:NO];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(_handleStatusBarNavigation) name:PUICStatusBarNavigationBackButtonPressedNotification object:nil];
    [center addObserver:self selector:@selector(_handleStatusBarNavigation) name:PUICStatusBarTitleTappedNotification object:nil];

    configureStatusBarForController(self, self.delegate);
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];

    _statusBarAssertion = nil;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:PUICStatusBarNavigationBackButtonPressedNotification object:nil];
    [center removeObserver:self name:PUICStatusBarTitleTappedNotification object:nil];
}

- (void)_handleStatusBarNavigation
{
    [self.delegate quickboardInputCancelled:self];
}

- (void)reloadContextView
{
    _contextViewNeedsUpdate = YES;
    [self reloadHeaderContentView];
}

- (PUICActionController *)actionController
{
    if (![self.delegate allowsLanguageSelectionMenuForListViewController:self])
        return nil;

    PUICActionItem *languageSelectionActionItem = [self.languageController languageSelectionActionItemForViewController:self];
    auto actionGroup = adoptNS([[PUICActionGroup alloc] initWithActionItems:@[ languageSelectionActionItem ] actionStyle:PUICActionStyleAutomatic]);
    return [[[PUICActionController alloc] initWithActionGroup:actionGroup.get()] autorelease];
}

#pragma mark - PUICQuickboardLanguageControllerDelegate

- (void)languageControllerDidChangePrimaryLanguage:(PUICQuickboardLanguageController *)languageController
{
    if ([self.delegate respondsToSelector:@selector(quickboard:languageDidChange:)])
        [self.delegate quickboard:self languageDidChange:languageController.primaryLanguage];
}

#pragma mark - Quickboard subclassing

- (CGFloat)headerContentViewHeight
{
    [self updateContextViewIfNeeded];

    return [_contextView sizeThatFits:self.contentView.bounds.size].height;
}

- (UIView *)headerContentView
{
    [self updateContextViewIfNeeded];

    CGFloat viewWidth = CGRectGetWidth(self.contentView.bounds);
    CGSize sizeThatFits = [_contextView sizeThatFits:self.contentView.bounds.size];
    [_contextView setFrame:CGRectMake((viewWidth - sizeThatFits.width) / 2, 0, sizeThatFits.width, sizeThatFits.height)];
    [_contextView layoutSubviews];
    return _contextView.get();
}

@end

void configureStatusBarForController(PUICQuickboardViewController *controller, id <WKQuickboardViewControllerDelegate> delegate)
{
    // An internal client (e.g. Safari's modal sheet) may have moved the status bar offscreen.
    // Before updating the status bar, make sure that we bring the status bar back to its original position.
    PUICStatusBar *statusBar = [PUICApplication sharedPUICApplication]._puicStatusBar;
    statusBar.frame = CGRect { CGPointZero, statusBar.frame.size };

    PUICApplicationStatusBarItem *item = controller.puic_applicationStatusBarItem;
    item.title = [delegate inputLabelTextForViewController:controller];
    item.titleColor = [UIColor systemBlueColor];
    item.navUIBackButtonDisabled = NO;
    item.showNavigationUI = YES;
    item.titleInteractive = YES;
    [item commitChangesAnimated:NO];
}

#endif // PLATFORM(WATCHOS)
