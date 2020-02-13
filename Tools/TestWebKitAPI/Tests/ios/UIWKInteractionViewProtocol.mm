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

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <wtf/RetainPtr.h>

@interface TestWKWebView (UIWKInteractionViewTesting)
- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point;
- (void)updateSelectionWithExtentPoint:(CGPoint)point;
- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity;
@end

@implementation TestWKWebView (UIWKInteractionViewTesting)

- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point
{
    __block bool done = false;
    [self.textInputContentView selectTextWithGranularity:granularity atPoint:point completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point
{
    __block bool done = false;
    [self.textInputContentView updateSelectionWithExtentPoint:point completionHandler:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity
{
    __block bool done = false;
    [self.textInputContentView updateSelectionWithExtentPoint:point withBoundary:granularity completionHandler:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

@end

TEST(UIWKInteractionViewProtocol, SelectTextWithCharacterGranularity)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body style='font-size: 20px;'>Hello world</body>"];
    [webView selectTextWithGranularity:UITextGranularityCharacter atPoint:CGPointMake(10, 20)];
    [webView updateSelectionWithExtentPoint:CGPointMake(300, 20) withBoundary:UITextGranularityCharacter];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

TEST(UIWKInteractionViewProtocol, UpdateSelectionWithExtentPoint)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable style='font-size: 20px;'>Hello world</body>"];

    [webView evaluateJavaScript:@"getSelection().setPosition(document.body, 1)" completionHandler:nil];
    [webView updateSelectionWithExtentPoint:CGPointMake(5, 20)];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);

    [webView evaluateJavaScript:@"getSelection().setPosition(document.body, 0)" completionHandler:nil];
    [webView updateSelectionWithExtentPoint:CGPointMake(300, 20)];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

#endif
