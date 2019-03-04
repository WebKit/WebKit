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

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKContextPrivate.h>
#import <WebKit/WKProcessGroupPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

static bool loadedOrCrashed = false;
static bool loaded = false;
static bool webProcessCrashed = false;
static bool networkProcessCrashed = false;
static pid_t pid;
static WKWebView* testView;

@interface MonitorWebContentCrashNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation MonitorWebContentCrashNavigationDelegate

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    webProcessCrashed = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    loaded = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKit, NetworkProcessCrashWithPendingConnection)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKProcessPool> processPool = adoptNS([[WKProcessPool alloc] init]);
    [configuration setProcessPool:processPool.get()];

    // FIXME: Adopt the new API once it's added in webkit.org/b/174061.
    WKContextClientV0 client;
    memset(&client, 0, sizeof(client));
    client.networkProcessDidCrash = [](WKContextRef context, const void* clientInfo) {
        networkProcessCrashed = true;
    };
    WKContextSetClient(static_cast<WKContextRef>(processPool.get()), &client.base);

    RetainPtr<WKWebView> webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView1.get() loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [webView1.get() _test_waitForDidFinishNavigation];

    [webView2.get() loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [webView2.get() _test_waitForDidFinishNavigation];

    pid_t webView1PID = [webView1.get() _webProcessIdentifier];
    pid_t webView2PID = [webView2.get() _webProcessIdentifier];
    EXPECT_NE(webView1PID, webView2PID);

    pid_t initialNetworkProcessIdentififer = [processPool.get() _networkProcessIdentifier];
    EXPECT_NE(initialNetworkProcessIdentififer, 0);
    kill(initialNetworkProcessIdentififer, SIGKILL);
    Util::run(&networkProcessCrashed);
    networkProcessCrashed = false;

    [webView1.get() loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    [webView1.get() _test_waitForDidFinishNavigation];

    pid_t relaunchedNetworkProcessIdentifier = [processPool.get() _networkProcessIdentifier];
    EXPECT_NE(initialNetworkProcessIdentififer, relaunchedNetworkProcessIdentifier);
    EXPECT_FALSE(networkProcessCrashed);

    kill(relaunchedNetworkProcessIdentifier, SIGSTOP);

    [webView2.get() loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    Util::sleep(0.5); // Wait for the WebContent process to send CreateNetworkConnectionToWebProcess
    kill(relaunchedNetworkProcessIdentifier, SIGKILL);
    Util::run(&networkProcessCrashed);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    loadedOrCrashed = false;
    [navigationDelegate setDidFinishNavigation:^(WKWebView *, WKNavigation *) {
        loadedOrCrashed = true;
        loaded = true;
    }];
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *) {
        loadedOrCrashed = true;
        webProcessCrashed = true;
    }];
    [webView2 setNavigationDelegate:navigationDelegate.get()];
    TestWebKitAPI::Util::run(&loadedOrCrashed);
    EXPECT_TRUE(loaded);
    EXPECT_FALSE(webProcessCrashed);
}

TEST(WebKit, NetworkProcessRelaunchOnLaunchFailure)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto processPool = adoptNS([[WKProcessPool alloc] init]);

    [configuration setProcessPool:processPool.get()];

    webProcessCrashed = false;
    loaded = false;
    networkProcessCrashed = false;

    WKContextClientV0 client;
    memset(&client, 0, sizeof(client));
    client.networkProcessDidCrash = [](WKContextRef context, const void* clientInfo) {
        pid = [testView _webProcessIdentifier];
        EXPECT_GT(pid, 0); // The WebProcess is already launched when the network process crashes.
        networkProcessCrashed = true;
    };
    WKContextSetClient(static_cast<WKContextRef>(processPool.get()), &client.base);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    testView = webView.get();

    // Constucting a WebView starts a network process so terminate this one. The page load below will then request a network process and we
    // make this new network process launch crash on startup.
    [processPool _terminateNetworkProcess];
    [processPool _makeNextNetworkProcessLaunchFailForTesting];

    auto delegate = adoptNS([[MonitorWebContentCrashNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&loaded);

    EXPECT_TRUE(networkProcessCrashed);
    EXPECT_FALSE(webProcessCrashed);
    EXPECT_GT([webView _webProcessIdentifier], 0);
    EXPECT_EQ(pid, [webView _webProcessIdentifier]);
    EXPECT_GT([processPool.get() _networkProcessIdentifier], 0);
}

} // namespace TestWebKitAPI
