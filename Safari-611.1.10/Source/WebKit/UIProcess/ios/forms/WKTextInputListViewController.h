/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(WATCHOS)

#import "WKQuickboardListViewController.h"

typedef NS_ENUM(NSInteger, WKNumberPadInputMode) {
    WKNumberPadInputModeNone,
    WKNumberPadInputModeNumbersAndSymbols,
    WKNumberPadInputModeTelephone,
    WKNumberPadInputModeNumbersOnly
};

@class WKTextInputListViewController;

@protocol WKTextInputListViewControllerDelegate <WKQuickboardViewControllerDelegate>

- (WKNumberPadInputMode)numericInputModeForListViewController:(WKTextInputListViewController *)controller;
- (NSString *)textContentTypeForListViewController:(WKTextInputListViewController *)controller;
- (NSArray<UITextSuggestion *> *)textSuggestionsForListViewController:(WKTextInputListViewController *)controller;
- (void)listViewController:(WKTextInputListViewController *)controller didSelectTextSuggestion:(UITextSuggestion *)suggestion;
- (BOOL)allowsDictationInputForListViewController:(PUICQuickboardViewController *)controller;

@end

@interface WKTextInputListViewController : WKQuickboardListViewController

- (instancetype)initWithDelegate:(id <WKTextInputListViewControllerDelegate>)delegate NS_DESIGNATED_INITIALIZER;
- (void)reloadTextSuggestions;

@property (nonatomic, weak) id <WKTextInputListViewControllerDelegate> delegate;

@end

@interface WKTextInputListViewController (Testing)

- (void)enterText:(NSString *)text;

@end

#endif // PLATFORM(WATCHOS)
