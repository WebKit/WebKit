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

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKPreferences.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, OrthogonalFlowAvailableSize)
{
    CGPoint origin = CGPointMake(0, 0);
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(origin.x, origin.y, 500, 500)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"orthogonal-flow-available-size" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    __block bool done = false;
    [webView evaluateJavaScript:@"runTest()" completionHandler:^(NSNumber *result, NSError *error) {
        EXPECT_EQ(500, result.intValue);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView setFrame:CGRectMake(origin.x, origin.y, 500, 400)];
    [webView evaluateJavaScript:@"runTest()" completionHandler:^(NSNumber *result, NSError *error) {
        EXPECT_EQ(400, result.intValue);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView setFrame:CGRectMake(origin.x, origin.y, 500, 600)];
    [webView evaluateJavaScript:@"runTest()" completionHandler:^(NSNumber *result, NSError *error) {
        EXPECT_EQ(600, result.intValue);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}
