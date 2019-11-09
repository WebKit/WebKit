/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFindConfiguration.h>
#import <WebKit/WKFindResult.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>


TEST(WKWebView, FindAPIForwardsNoMatch)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);

    EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

    static bool done;
    [webView findString:@"foobar" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIForwardsWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIForwardsNoWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().wraps = NO;

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIBackwardsWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().backwards = YES;

    EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIBackwardsNoWrap)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().backwards = YES;
    configuration.get().wraps = NO;

    EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_FALSE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:0]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WKWebView, FindAPIForwardsCaseSensitive)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"word Word word" baseURL:nil];

    // The default find configuration is "forwards, case insensitive, wrapping"
    auto configuration = adoptNS([[WKFindConfiguration alloc] init]);
    configuration.get().caseSensitive = YES;

    static bool done;
    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:10 endOffset:14]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:0 endOffset:4]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    [webView findString:@"Word" withConfiguration:configuration.get() completionHandler:^(WKFindResult *result) {
        EXPECT_TRUE(result.matchFound);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:5 endOffset:9]);

        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}
