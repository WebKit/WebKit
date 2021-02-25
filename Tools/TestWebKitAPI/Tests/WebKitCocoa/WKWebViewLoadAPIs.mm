/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/NSURLExtras.h>

static NSString *exampleURLString = @"https://example.com/";
static NSURL *exampleURL = [NSURL URLWithString:exampleURLString];
static NSString *htmlString = @"<html><body><h1>Hello, world!</h1></body></html>";
static NSString *exampleURLString2 = @"https://example.org/";
static NSURL *exampleURL2 = [NSURL URLWithString:exampleURLString2];
static NSString *htmlString2 = @"<html><body><h1>Hello, new world!</h1></body></html>";

TEST(WKWebView, LoadSimulatedRequestUsingResponseHTMLString)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *loadRequest = [NSURLRequest requestWithURL:exampleURL];

    [webView loadSimulatedRequest:loadRequest withResponseHTMLString:htmlString];
    [delegate waitForDidFinishNavigation];
}

TEST(WKWebView, LoadSimulatedRequestUsingResponseData)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *loadRequest = [NSURLRequest requestWithURL:exampleURL];

    NSData *data = [htmlString dataUsingEncoding:NSUTF8StringEncoding];
    auto response = adoptNS([[NSURLResponse alloc] initWithURL:exampleURL MIMEType:@"text/HTML" expectedContentLength:[data length] textEncodingName:@"UTF-8"]);

    [webView loadSimulatedRequest:loadRequest withResponse:response.get() responseData:data];
    [delegate waitForDidFinishNavigation];
}

TEST(WKWebView, LoadFileRequest)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURL *file = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSURLRequest *request = [NSURLRequest requestWithURL:file];
    [webView loadFileRequest:request allowingReadAccessToURL:file.URLByDeletingLastPathComponent];
    [delegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ(webView.get()._resourceDirectoryURL.path, file.URLByDeletingLastPathComponent.path);
}

TEST(WKWebView, LoadSimulatedRequestUpdatesBackForwardList)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *loadRequest = [NSURLRequest requestWithURL:exampleURL];
    [webView loadSimulatedRequest:loadRequest withResponseHTMLString:htmlString];
    [delegate waitForDidFinishNavigation];

    NSURLRequest *loadRequest2 = [NSURLRequest requestWithURL:exampleURL2];
    [webView loadSimulatedRequest:loadRequest2 withResponseHTMLString:htmlString2];
    [delegate waitForDidFinishNavigation];
    
    WKBackForwardList *list = [webView backForwardList];
    EXPECT_WK_STREQ(exampleURLString2, [list.currentItem.URL absoluteString]);
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_TRUE(!list.forwardItem);
    EXPECT_WK_STREQ(exampleURLString, [list.backItem.URL absoluteString]);
    
    EXPECT_TRUE([webView canGoBack]);
    if (![webView canGoBack])
        return;

    [webView goBack];
    [delegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(exampleURLString, [list.currentItem.URL absoluteString]);
    EXPECT_EQ((NSUInteger)0, list.backList.count);
    EXPECT_EQ((NSUInteger)1, list.forwardList.count);
    EXPECT_TRUE(!list.backItem);
    EXPECT_WK_STREQ(exampleURLString2, [list.forwardItem.URL absoluteString]);

    EXPECT_TRUE([webView canGoForward]);
    if (![webView canGoForward])
        return;

    [webView goForward];
    [delegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ(exampleURLString2, [list.currentItem.URL absoluteString]);
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_TRUE(!list.forwardItem);
    EXPECT_WK_STREQ(exampleURLString, [list.backItem.URL absoluteString]);
}
