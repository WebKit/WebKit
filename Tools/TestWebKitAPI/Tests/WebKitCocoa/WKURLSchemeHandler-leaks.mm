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

#ifndef NDEBUG

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKTestingSupport.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

@interface WKWebView (TestingSecrets)
+ (size_t)_webURLSchemeTaskInstanceCount;
+ (size_t)_apiURLSchemeTaskInstanceCount;
@end

@interface LeakSchemeHandler : NSObject <WKURLSchemeHandler>
@end

@implementation LeakSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:[NSData dataWithBytes:"1" length:1]];

    // Don't finish these tasks, as we're exploring their leaks
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

static void runUntilTasksInFlight(size_t count)
{
    while (true) {
        EXPECT_EQ(WKGetAPIURLSchemeTaskInstanceCount(), WKGetWebURLSchemeTaskInstanceCount());

        if (WKGetAPIURLSchemeTaskInstanceCount() == count)
            return;

        TestWebKitAPI::Util::spinRunLoop(10);
    }
}

auto e = EPERM;

#if PLATFORM(IOS_FAMILY)
// This test is sometimes timing out on iOS (rdar://problem/33665676).
TEST(URLSchemeHandler, DISABLED_Leaks1)
#else
TEST(URLSchemeHandler, Leaks1)
#endif
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<LeakSchemeHandler> handler = adoptNS([[LeakSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    @autoreleasepool {
    RetainPtr<WKWebView> webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        @autoreleasepool {
        RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
            @autoreleasepool {
                RetainPtr<WKWebView> webView3 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

                handler = nil;
                configuration = nil;

                [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main1"]]];
                [webView2 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main2"]]];
                [webView3 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main3"]]];

                runUntilTasksInFlight(3);
            }
            runUntilTasksInFlight(2);
        }
        runUntilTasksInFlight(1);
    }
    runUntilTasksInFlight(0);
}

TEST(URLSchemeHandler, Leaks2)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<LeakSchemeHandler> handler = adoptNS([[LeakSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    handler = nil;
    configuration = nil;

    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main1"]]];
    [webView2 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main2"]]];

    runUntilTasksInFlight(2);

    kill([webView1 _webProcessIdentifier], SIGKILL);
    kill([webView2 _webProcessIdentifier], SIGKILL);

    runUntilTasksInFlight(0);
}

TEST(URLSchemeHandler, Leaks3)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<LeakSchemeHandler> handler = adoptNS([[LeakSchemeHandler alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"testing"];

    RetainPtr<WKWebView> webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    handler = nil;
    configuration = nil;

    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main1"]]];
    [webView2 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"testing:main2"]]];

    runUntilTasksInFlight(2);

    [webView1.get().configuration.processPool _terminateNetworkProcess];

    runUntilTasksInFlight(0);
}

#endif // NDEBUG
