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

@implementation TestWKWebView (KeyboardInputTests)

- (void)waitForCaretViewFrameToBecome:(CGRect)frame
{
    BOOL hasEmittedWarning = NO;
    NSTimeInterval secondsToWaitUntilWarning = 2;
    NSTimeInterval startTime = [NSDate timeIntervalSinceReferenceDate];
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        CGRect currentFrame = self.caretViewRectInContentCoordinates;
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
                selectionRectsMatch |= [selectionRects[index] isEqualToValue:currentRects[index]];
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

static RetainPtr<TestWKWebView> webViewWithAutofocusedInput()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto inputDelegate = adoptNS([[TestInputDelegate alloc] init]);

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

namespace TestWebKitAPI {

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
    auto expectedCaretRect = CGRectMake(16, 13, 3, 15);
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

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(IOS)
