/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#import "WKTextInputListViewController.h"

#if HAVE(PEPPER_UI_CORE)

#import "WKNumberPadViewController.h"
#import <wtf/RetainPtr.h>

@implementation WKTextInputListViewController {
    BOOL _contextViewNeedsUpdate;
    RetainPtr<UIView> _contextView;
    RetainPtr<WKNumberPadViewController> _numberPadViewController;
}

@dynamic delegate;

- (instancetype)initWithDelegate:(id <WKTextInputListViewControllerDelegate>)delegate
{
    if (!(self = [super initWithDelegate:delegate dictationMode:PUICDictationModeText]))
        return nil;

    _contextViewNeedsUpdate = YES;
    self.textInputContext = [self.delegate textInputContextForListViewController:self];
    return self;
}

- (void)reloadContextView
{
    _contextViewNeedsUpdate = YES;
    [self reloadHeaderContentView];
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

- (BOOL)requiresNumericInput
{
    return [self.delegate numericInputModeForListViewController:self] != WKNumberPadInputModeNone;
}

- (NSArray *)additionalTrayButtons
{
    if (!self.requiresNumericInput)
        return @[ ];

#if HAVE(PUIC_BUTTON_TYPE_PILL)
    auto numberPadButton = retainPtr([PUICQuickboardListTrayButton buttonWithType:PUICButtonTypePill]);
#else
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto numberPadButton = adoptNS([[PUICQuickboardListTrayButton alloc] initWithFrame:CGRectZero tintColor:nil defaultHeight:self.specs.defaultButtonHeight]);
ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    [numberPadButton setAction:PUICQuickboardActionAddNumber];
    [numberPadButton addTarget:self action:@selector(presentNumberPadViewController) forControlEvents:UIControlEventTouchUpInside];
    return @[ numberPadButton.get() ];
}

- (void)presentNumberPadViewController
{
    if (_numberPadViewController)
        return;

    WKNumberPadInputMode mode = [self.delegate numericInputModeForListViewController:self];
    if (mode == WKNumberPadInputModeNone) {
        ASSERT_NOT_REACHED();
        return;
    }

    NSString *initialText = [self.delegate initialValueForViewController:self];
    _numberPadViewController = adoptNS([[WKNumberPadViewController alloc] initWithDelegate:self.delegate initialText:initialText inputMode:mode]);
    [self presentViewController:_numberPadViewController.get() animated:YES completion:nil];
}

- (void)updateTextSuggestions:(NSArray<UITextSuggestion *> *)suggestions
{
    auto messages = adoptNS([[NSMutableArray<NSAttributedString *> alloc] initWithCapacity:suggestions.count]);
    for (UITextSuggestion *suggestion in suggestions) {
        auto attributedString = adoptNS([[NSAttributedString alloc] initWithString:suggestion.displayText]);
        [messages addObject:attributedString.get()];
    }
    self.messages = messages.get();
}

- (void)enterText:(NSString *)text
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [self.delegate quickboard:static_cast<id<PUICQuickboardController>>(self) textEntered:adoptNS([[NSAttributedString alloc] initWithString:text]).get()];
    ALLOW_DEPRECATED_DECLARATIONS_END
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
    return _contextView.get();
}

- (BOOL)shouldShowLanguageButton
{
    return [self.delegate allowsLanguageSelectionForListViewController:self];
}

- (BOOL)supportsDictationInput
{
    return [self.delegate allowsDictationInputForListViewController:self];
}

- (BOOL)shouldShowTrayView
{
    return self.requiresNumericInput;
}

- (BOOL)shouldShowTextField
{
    return !self.requiresNumericInput;
}

- (BOOL)supportsArouetInput
{
    return !self.requiresNumericInput;
}

@end

#endif // HAVE(PEPPER_UI_CORE)
