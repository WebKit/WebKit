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
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
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

    [webView loadSimulatedRequest:loadRequest responseHTMLString:htmlString];
    [delegate waitForDidFinishNavigation];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME(223658): Remove this once adopters have moved to the final API.
    [webView loadSimulatedRequest:loadRequest withResponseHTMLString:htmlString];
ALLOW_DEPRECATED_DECLARATIONS_END
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

    [webView loadSimulatedRequest:loadRequest response:response.get() responseData:data];
    [delegate waitForDidFinishNavigation];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME(223658): Remove this once adopters have moved to the final API.
    [webView loadSimulatedRequest:loadRequest withResponse:response.get() responseData:data];
ALLOW_DEPRECATED_DECLARATIONS_END
    [delegate waitForDidFinishNavigation];
}

TEST(WKWebView, LoadSimulatedRequestDelegateCallbacks)
{
    enum class Callback : uint8_t {
        NavigationAction,
        NavigationResponse,
        StartProvisional,
        Commit,
        Finish
    };

    auto checkCallbacks = [] (const Vector<Callback> vector) {
        ASSERT_EQ(vector.size(), 4u);
        EXPECT_EQ(vector[0], Callback::NavigationAction);
        EXPECT_EQ(vector[1], Callback::StartProvisional);
        EXPECT_EQ(vector[2], Callback::Commit);
        EXPECT_EQ(vector[3], Callback::Finish);
    };

    __block Vector<Callback> callbacks;
    __block bool finished = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^completionHandler)(WKNavigationActionPolicy)) {
        callbacks.append(Callback::NavigationAction);
        completionHandler(WKNavigationActionPolicyAllow);
    };
    delegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        callbacks.append(Callback::NavigationResponse);
        completionHandler(WKNavigationResponsePolicyAllow);
    };
    delegate.get().didStartProvisionalNavigation = ^(WKWebView *, WKNavigation *) {
        callbacks.append(Callback::StartProvisional);
    };
    delegate.get().didCommitNavigation = ^(WKWebView *, WKNavigation *) {
        callbacks.append(Callback::Commit);
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        callbacks.append(Callback::Finish);
        finished = true;
    };

    auto webView = adoptNS([WKWebView new]);
    webView.get().navigationDelegate = delegate.get();
    NSURL *url = [NSURL URLWithString:@"https://webkit.org/"];
    [webView loadHTMLString:@"hi!" baseURL:url];
    TestWebKitAPI::Util::run(&finished);

    checkCallbacks(callbacks);
    callbacks = { };
    finished = false;

    [webView loadSimulatedRequest:[NSURLRequest requestWithURL:url] response:adoptNS([[NSURLResponse alloc] initWithURL:url MIMEType:@"text/html" expectedContentLength:3 textEncodingName:nil]).get() responseData:[NSData dataWithBytes:"hi!" length:3]];
    TestWebKitAPI::Util::run(&finished);
    checkCallbacks(callbacks);
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
    [webView loadSimulatedRequest:loadRequest responseHTMLString:htmlString];
    [delegate waitForDidFinishNavigation];

    NSURLRequest *loadRequest2 = [NSURLRequest requestWithURL:exampleURL2];
    [webView loadSimulatedRequest:loadRequest2 responseHTMLString:htmlString2];
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
    
    // loadHTMLString is peculiarly different than loadSimulatedRequest, but we need to leave it
    // like it is until we decide to change it, probably with a linked-on-or-after check.
    auto webView2 = adoptNS([WKWebView new]);
    [webView2 setNavigationDelegate:delegate.get()];
    [webView2 loadHTMLString:htmlString baseURL:loadRequest.URL];
    [delegate waitForDidFinishNavigation];
    [webView2 loadHTMLString:htmlString2 baseURL:loadRequest2.URL];
    [delegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView2 canGoBack]);
    WKBackForwardList *list2 = [webView2 backForwardList];
    EXPECT_WK_STREQ("", [list2.currentItem.URL absoluteString]);
    EXPECT_EQ(0u, list2.backList.count);
    EXPECT_EQ(0u, list2.forwardList.count);
}
