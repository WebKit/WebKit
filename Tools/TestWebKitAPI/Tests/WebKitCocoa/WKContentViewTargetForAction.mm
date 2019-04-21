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

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

@interface TestWKContentViewTargetForActionView : TestWKWebView
@end

@implementation TestWKContentViewTargetForActionView

- (void)testAction:(id)sender
{
}

@end

TEST(WebKit, WKContentViewTargetForAction)
{
    auto webView = adoptNS([[TestWKContentViewTargetForActionView alloc] init]);
    [webView synchronouslyLoadTestPageNamed:@"rich-and-plain-text"];
    [webView becomeFirstResponder];
    [webView stringByEvaluatingJavaScript:@"selectPlainText()"];
    [webView waitForNextPresentationUpdate];

    // FIXME: Once this test is running in an application, it should instead use
    // -[UIApplication sendAction:to:from:forEvent:] and verify that the action is dispatched to the
    // right responder.
    UIView *contentView = [webView valueForKey:@"_currentContentView"];
    EXPECT_EQ([webView targetForAction:@selector(copy:) withSender:nil], contentView);
    EXPECT_EQ([contentView targetForAction:@selector(copy:) withSender:nil], contentView);
    EXPECT_EQ([webView targetForAction:@selector(testAction:) withSender:nil], webView.get());
}

#endif
