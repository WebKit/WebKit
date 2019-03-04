/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKErrorPrivate.h>
#import <wtf/RetainPtr.h>

static bool isDone;

TEST(WKWebView, EvaluateJavaScriptBlockCrash)
{
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];

        [webView evaluateJavaScript:@"" completionHandler:^(id result, NSError *error) {
            // NOTE: By referencing the request here, we convert the block into a stack block rather than a global block and thus allow the copying of the block
            // in evaluateJavaScript to be meaningful.
            (void)request;
            
            EXPECT_NULL(result);
            EXPECT_NOT_NULL(error);

            isDone = true;
        }];

        // Force the WKWebView to be destroyed to allow evaluateJavaScript's completion handler to be called with an error.
        webView = nullptr;
    }

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, EvaluateJavaScriptErrorCases)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.body" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_WK_STREQ(@"WKErrorDomain", [error domain]);
        EXPECT_EQ(WKErrorJavaScriptResultTypeIsUnsupported, [error code]);

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    [webView evaluateJavaScript:@"document.body.insertBefore(document, document)" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_WK_STREQ(@"WKErrorDomain", [error domain]);
        EXPECT_EQ(WKErrorJavaScriptExceptionOccurred, [error code]);
        EXPECT_NOT_NULL([error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey]);
        EXPECT_GT([[error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey] length], (unsigned long)0);
        EXPECT_EQ(1, [[error.userInfo objectForKey:_WKJavaScriptExceptionLineNumberErrorKey] intValue]);
        EXPECT_EQ(27, [[error.userInfo objectForKey:_WKJavaScriptExceptionColumnNumberErrorKey] intValue]);

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    [webView evaluateJavaScript:@"\n\nthrow 'something bad'" completionHandler:^(id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_WK_STREQ(@"WKErrorDomain", [error domain]);
        EXPECT_EQ(WKErrorJavaScriptExceptionOccurred, [error code]);
        EXPECT_WK_STREQ(@"something bad", [error.userInfo objectForKey:_WKJavaScriptExceptionMessageErrorKey]);
        EXPECT_EQ(3, [[error.userInfo objectForKey:_WKJavaScriptExceptionLineNumberErrorKey] intValue]);
        EXPECT_EQ(22, [[error.userInfo objectForKey:_WKJavaScriptExceptionColumnNumberErrorKey] intValue]);
        EXPECT_NOT_NULL([error.userInfo objectForKey:_WKJavaScriptExceptionSourceURLErrorKey]);

        isDone = true;
    }];

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}
