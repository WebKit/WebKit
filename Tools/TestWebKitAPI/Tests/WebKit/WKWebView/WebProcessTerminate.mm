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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"

#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, WebProcessTerminate)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto pid = [webView _webProcessIdentifier];
    [webView _killWebContentProcessAndResetState];

    [webView loadHTMLString:@"test" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    auto pid2 = [webView _webProcessIdentifier];
    EXPECT_TRUE(pid != pid2);
}

@interface WebProcessTerminationSchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation WebProcessTerminationSchemeHandler
- (void)webView:(WKWebView *)webView startURLSchemeTask:(id<WKURLSchemeTask>)task
{
    RetainPtr data = [@"<html><body>hello</body></html>" dataUsingEncoding:NSUTF8StringEncoding];
    RetainPtr response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:[data length] textEncodingName:nil]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:data.get()];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id<WKURLSchemeTask>)task
{
}
@end

// The pool keeps its web content processes in creation order, so a process created later is never
// first in the list. -[WKProcessPool _requestWebProcessTermination:] must still terminate it.
TEST(WebKit, RequestWebProcessTerminationForNonFirstProcess)
{
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    processPoolConfiguration.get().processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol = YES;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    RetainPtr handler = adoptNS([[WebProcessTerminationSchemeHandler alloc] init]);
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setProcessPool:processPool.get()];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"termtest"];

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    __block bool didFinishNavigation = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        didFinishNavigation = true;
    }];

    // First web view -> first web content process in the pool's list.
    RetainPtr firstWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [firstWebView setNavigationDelegate:navigationDelegate.get()];
    [firstWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"termtest://first.example/"]]];
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;
    auto firstPID = [firstWebView _webProcessIdentifier];

    // Second web view (different site) -> a second, non-first web content process.
    RetainPtr secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [secondWebView setNavigationDelegate:navigationDelegate.get()];
    [secondWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"termtest://second.example/"]]];
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;
    auto secondPID = [secondWebView _webProcessIdentifier];

    // Sanity: two distinct web content processes exist, and the second was created after the first.
    EXPECT_NE(firstPID, secondPID);
    EXPECT_EQ(2U, [processPool _webProcessCountIgnoringPrewarmed]);

    WKWebView *secondWebViewToMatch = secondWebView.get();
    __block bool secondWebViewProcessTerminated = false;
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *webView, _WKProcessTerminationReason) {
        if (webView == secondWebViewToMatch)
            secondWebViewProcessTerminated = true;
    }];

    EXPECT_TRUE([processPool _requestWebProcessTermination:secondPID]);

    // The (non-first) second process must actually be terminated within a bounded time. With the bug,
    // only the first process is examined, so this never happens and the wait times out.
    TestWebKitAPI::Util::runFor(&secondWebViewProcessTerminated, 5_s);
    EXPECT_TRUE(secondWebViewProcessTerminated);
    EXPECT_EQ(0, [secondWebView _webProcessIdentifier]);

    // The first process must be left running.
    EXPECT_EQ(firstPID, [firstWebView _webProcessIdentifier]);
}

TEST(WebKit, TerminateAllProcessesDuringLaunch)
{
    RetainPtr webView = adoptNS([WKWebView new]);

    // Initiate a load to make sure the process actually launches.
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    // Call terminateAllProcesses while the process is still launching.
    [webView.get().configuration.processPool _terminateAllWebContentProcesses];

    TestWebKitAPI::Util::runFor(0.5_s);

    // The WKWebView should be able to recover from the WebProcess termination and navigation should succeed.
    [webView loadHTMLString:@"test" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];
}
