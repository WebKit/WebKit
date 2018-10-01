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

#if WK_API_ENABLED && PLATFORM(IOS)

#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKitLegacy/WebEvent.h>
#import <cmath>

@implementation TestWKWebView (KeyboardInputTests)

static CGRect rounded(CGRect rect)
{
    return CGRectMake(std::round(rect.origin.x), std::round(rect.origin.y), std::round(rect.size.width), std::round(rect.size.height));
}

- (void)waitForCaretViewFrameToBecome:(CGRect)frame
{
    BOOL hasEmittedWarning = NO;
    NSTimeInterval secondsToWaitUntilWarning = 2;
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        CGRect currentFrame = rounded(self.caretViewRectInContentCoordinates);
        if (CGRectEqualToRect(currentFrame, frame))
            break;

        if (hasEmittedWarning || startTime + secondsToWaitUntilWarning >= [NSDate timeIntervalSinceReferenceDate])
            continue;

        NSLog(@"Expected a caret rect of %@, but still observed %@", NSStringFromCGRect(frame), NSStringFromCGRect(currentFrame));
        hasEmittedWarning = YES;
    }
}

- (void)waitForSelectionViewRectsToBecome:(NSArray<NSValue *> *)selectionRects
{
    BOOL hasEmittedWarning = NO;
    NSTimeInterval secondsToWaitUntilWarning = 2;
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        NSArray<NSValue *> *currentRects = self.selectionViewRectsInContentCoordinates;
        BOOL selectionRectsMatch = YES;
        if (currentRects.count == selectionRects.count) {
            for (NSUInteger index = 0; index < selectionRects.count; ++index)
                selectionRectsMatch |= CGRectEqualToRect(selectionRects[index].CGRectValue, rounded(currentRects[index].CGRectValue));
        } else
            selectionRectsMatch = NO;

        if (selectionRectsMatch)
            break;

        if (hasEmittedWarning || startTime + secondsToWaitUntilWarning >= [NSDate timeIntervalSinceReferenceDate])
            continue;

        NSLog(@"Expected a selection rects of %@, but still observed %@", selectionRects, currentRects);
        hasEmittedWarning = YES;
    }
}

@end

static RetainPtr<TestWKWebView> webViewWithAutofocusedInput(const RetainPtr<TestInputDelegate>& inputDelegate)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    bool doneWaiting = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        doneWaiting = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><input autofocus>"];

    TestWebKitAPI::Util::run(&doneWaiting);
    doneWaiting = false;
    return webView;
}

static RetainPtr<TestWKWebView> webViewWithAutofocusedInput()
{
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    return webViewWithAutofocusedInput(inputDelegate);
}

namespace TestWebKitAPI {

TEST(KeyboardInputTests, CustomInputViewAndInputAccessoryView)
{
    auto inputView = adoptNS([[UIView alloc] init]);
    auto inputAccessoryView = adoptNS([[UIView alloc] init]);
    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setWillStartInputSessionHandler:[inputView, inputAccessoryView] (WKWebView *, id<_WKFormInputSession> session) {
        session.customInputView = inputView.get();
        session.customInputAccessoryView = inputAccessoryView.get();
    }];

    auto webView = webViewWithAutofocusedInput(inputDelegate);
    EXPECT_EQ(inputView.get(), [webView firstResponder].inputView);
    EXPECT_EQ(inputAccessoryView.get(), [webView firstResponder].inputAccessoryView);
}

TEST(KeyboardInputTests, CanHandleKeyEventInCompletionHandler)
{
    auto webView = webViewWithAutofocusedInput();
    bool doneWaiting = false;

    id <UITextInputPrivate> contentView = (id <UITextInputPrivate>)[webView firstResponder];
    auto firstWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyDown timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);
    auto secondWebEvent = adoptNS([[WebEvent alloc] initWithKeyEventType:WebEventKeyUp timeStamp:CFAbsoluteTimeGetCurrent() characters:@"a" charactersIgnoringModifiers:@"a" modifiers:0 isRepeating:NO withFlags:0 withInputManagerHint:nil keyCode:0 isTabKey:NO]);
    [contentView handleKeyWebEvent:firstWebEvent.get() withCompletionHandler:[&] (WebEvent *event, BOOL) {
        EXPECT_TRUE([event isEqual:firstWebEvent.get()]);
        [contentView handleKeyWebEvent:secondWebEvent.get() withCompletionHandler:[&] (WebEvent *event, BOOL) {
            EXPECT_TRUE([event isEqual:secondWebEvent.get()]);
            [contentView insertText:@"a"];
            doneWaiting = true;
        }];
    }];

    TestWebKitAPI::Util::run(&doneWaiting);
    EXPECT_WK_STREQ("a", [webView stringByEvaluatingJavaScript:@"document.querySelector('input').value"]);
}

TEST(KeyboardInputTests, CaretSelectionRectAfterRestoringFirstResponder)
{
    auto expectedCaretRect = CGRectMake(16, 13, 2, 15);
    auto webView = webViewWithAutofocusedInput();
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForCaretViewFrameToBecome:expectedCaretRect];

    dispatch_block_t restoreActiveFocusState = [webView _retainActiveFocusedState];
    [webView resignFirstResponder];
    restoreActiveFocusState();
    [webView waitForCaretViewFrameToBecome:CGRectZero];

    [webView becomeFirstResponder];
    [webView waitForCaretViewFrameToBecome:expectedCaretRect];
}

TEST(KeyboardInputTests, RangedSelectionRectAfterRestoringFirstResponder)
{
    NSArray *expectedSelectionRects = @[ [NSValue valueWithCGRect:CGRectMake(16, 13, 24, 15)] ];

    auto webView = webViewWithAutofocusedInput();
    [[webView textInputContentView] insertText:@"hello"];
    [webView selectAll:nil];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName"]);
    [webView waitForSelectionViewRectsToBecome:expectedSelectionRects];

    dispatch_block_t restoreActiveFocusState = [webView _retainActiveFocusedState];
    [webView resignFirstResponder];
    restoreActiveFocusState();
    [webView waitForSelectionViewRectsToBecome:@[ ]];

    [webView becomeFirstResponder];
    [webView waitForSelectionViewRectsToBecome:expectedSelectionRects];
}

TEST(KeyboardInputTests, KeyboardTypeForInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id <_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input id='input'>"];

    auto runTest = ^(NSString *inputType, NSString *inputMode, NSString *pattern, UIKeyboardType expectedKeyboardType) {
        [webView stringByEvaluatingJavaScript:@"input.blur()"];
        [webView stringByEvaluatingJavaScript:[NSString stringWithFormat:@"input.type = '%@'; input.inputMode = '%@'; input.pattern = '%@'", inputType, inputMode, pattern]];
        [webView stringByEvaluatingJavaScript:@"input.focus()"];

        UIView<UITextInputPrivate> *textInput = (UIView<UITextInputPrivate> *)[webView textInputContentView];
        UIKeyboardType keyboardType = [textInput textInputTraits].keyboardType;

        bool success = keyboardType == expectedKeyboardType;
        if (!success)
            NSLog(@"Displayed %li for <input type='%@' inputmode='%@' pattern='%@'>. Expected %li.", (long)keyboardType, inputType, inputMode, pattern, (long)expectedKeyboardType);

        return success;
    };

    NSDictionary *expectedKeyboardTypeForInputType = @{
        @"text": @(UIKeyboardTypeDefault),
        @"password": @(UIKeyboardTypeDefault),
        @"search": @(UIKeyboardTypeDefault),
        @"email": @(UIKeyboardTypeEmailAddress),
        @"tel": @(UIKeyboardTypePhonePad),
        @"number": @(UIKeyboardTypeNumbersAndPunctuation),
        @"url": @(UIKeyboardTypeURL)
    };

    NSDictionary *expectedKeyboardTypeForInputMode = @{
        @"": @(-1),
        @"text": @(UIKeyboardTypeDefault),
        @"tel": @(UIKeyboardTypePhonePad),
        @"url": @(UIKeyboardTypeURL),
        @"email": @(UIKeyboardTypeEmailAddress),
        @"numeric": @(UIKeyboardTypeNumbersAndPunctuation),
        @"decimal": @(UIKeyboardTypeDecimalPad),
        @"search": @(UIKeyboardTypeWebSearch)
    };

    NSDictionary *expectedKeyboardTypeForPattern = @{
        @"": @(-1),
        @"\\\\d*": @(UIKeyboardTypeNumberPad),
        @"[0-9]*": @(UIKeyboardTypeNumberPad)
    };

    for (NSString *inputType in [expectedKeyboardTypeForInputType allKeys]) {
        for (NSString *inputMode in [expectedKeyboardTypeForInputMode allKeys]) {
            for (NSString *pattern in [expectedKeyboardTypeForPattern allKeys]) {
                NSNumber *keyboardType;
                if (inputMode.length) {
                    // inputmode has the highest priority.
                    keyboardType = expectedKeyboardTypeForInputMode[inputMode];
                } else {
                    if (pattern.length && ([inputType isEqual: @"text"] || [inputType isEqual: @"number"])) {
                        // Special case for text and number inputs that have a numeric pattern.
                        keyboardType = expectedKeyboardTypeForPattern[pattern];
                    } else {
                        // Otherwise, the input type determines the keyboard type.
                        keyboardType = expectedKeyboardTypeForInputType[inputType];
                    }
                }

                EXPECT_TRUE(runTest(inputType, inputMode, pattern, (UIKeyboardType)keyboardType.intValue));
            }
        }
    }
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS)
