/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

@interface SlowTextInputContext : NSTextInputContext
@property (nonatomic) BlockPtr<void()> handledInputMethodEventBlock;
@end

@implementation SlowTextInputContext

- (void)handleEventByInputMethod:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler
{
    [super handleEventByInputMethod:event completionHandler:^(BOOL handled) {
        dispatch_async(dispatch_get_main_queue(), ^() {
            completionHandler(handled);
            if (_handledInputMethodEventBlock)
                _handledInputMethodEventBlock();
        });
    }];
}

- (void)handleEvent:(NSEvent *)event completionHandler:(void(^)(BOOL handled))completionHandler
{
    [super handleEvent:event completionHandler:^(BOOL handled) {
        dispatch_async(dispatch_get_main_queue(), ^() {
            completionHandler(handled);
        });
    }];
}

@end

@interface SlowInputWebView : TestWKWebView {
    RetainPtr<SlowTextInputContext> _slowInputContext;
}
@end

@implementation SlowInputWebView

- (NSTextInputContext *)inputContext
{
    return self._web_superInputContext;
}

- (SlowTextInputContext *)_web_superInputContext
{
    if (!_slowInputContext)
        _slowInputContext = adoptNS([[SlowTextInputContext alloc] initWithClient:(id<NSTextInputClient>)self]);
    return _slowInputContext.get();
}

@end

TEST(WKWebViewMacEditingTests, DoubleClickDoesNotSelectTrailingSpace)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"double-click-does-not-select-trailing-space"];

    __block bool finishedSelectingText = false;
    [webView performAfterReceivingMessage:@"selected" action:^() {
        finishedSelectingText = true;
    }];
    [webView sendClicksAtPoint:NSMakePoint(200, 200) numberOfClicks:2];
    TestWebKitAPI::Util::run(&finishedSelectingText);

    NSString *selectedText = [webView stringByEvaluatingJavaScript:@"getSelection().getRangeAt(0).toString()"];
    EXPECT_STREQ("Hello", selectedText.UTF8String);
}

TEST(WKWebViewMacEditingTests, DoNotCrashWhenCallingTextInputClientMethodsWhileDeallocatingView)
{
    NSString *textContent = @"This test should not cause us to dereference null.";

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<p>%@</p>", textContent]];
    [webView removeFromSuperview];

    __unsafe_unretained id <NSTextInputClient_Async> inputClient = (id <NSTextInputClient_Async>)webView.get();
    [inputClient hasMarkedTextWithCompletionHandler:^(BOOL) {
        [inputClient selectedRangeWithCompletionHandler:^(NSRange) {
            [inputClient markedRangeWithCompletionHandler:^(NSRange) { }];
        }];
    }];

    EXPECT_WK_STREQ(textContent, [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(WKWebViewMacEditingTests, DoNotCrashWhenInterpretingKeyEventWhileDeallocatingView)
{
    __block bool isDone = false;

    @autoreleasepool {
        SlowInputWebView *webView = [[[SlowInputWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)] autorelease];
        [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<body contenteditable>Hello world</body>"]];
        [webView stringByEvaluatingJavaScript:@"document.body.focus()"];
        [webView waitForNextPresentationUpdate];
        [webView removeFromSuperview];
        [webView typeCharacter:'a'];

        webView._web_superInputContext.handledInputMethodEventBlock = ^() {
            isDone = true;
        };
    }

    TestWebKitAPI::Util::run(&isDone);
}

#endif // PLATFORM(MAC)
