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

#import "config.h"

#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, AttrStyle)
{
    auto poolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    [poolConfiguration setAttrStyleEnabled:YES];
    auto pool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:poolConfiguration.get()]);
    auto viewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [viewConfiguration setProcessPool:pool.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:viewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"AttrStyle" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool isDone = false;
    [webView evaluateJavaScript:@"'style' in Attr.prototype" completionHandler:^(NSNumber *result, NSError *error) {
        EXPECT_TRUE(result.boolValue);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView evaluateJavaScript:@"document.body.getAttributeNode('background').style.cssText" completionHandler:^(NSString *result, NSError *error) {
        EXPECT_STREQ("background-image: url(\"about://example.com/body.png\");", result.UTF8String);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView evaluateJavaScript:@"document.body.getAttributeNode('dir').style.cssText" completionHandler:^(NSString *result, NSError *error) {
        EXPECT_STREQ("direction: rtl; unicode-bidi: embed;", result.UTF8String);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView evaluateJavaScript:@"document.body.getAttributeNode('marginheight').style.cssText" completionHandler:^(NSString *result, NSError *error) {
        EXPECT_STREQ("margin-bottom: 20px; margin-top: 20px;", result.UTF8String);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView evaluateJavaScript:@"document.getElementById('target').getAttributeNode('align').style.cssText" completionHandler:^(NSString *result, NSError *error) {
        EXPECT_STREQ("text-align: -webkit-center;", result.UTF8String);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}
