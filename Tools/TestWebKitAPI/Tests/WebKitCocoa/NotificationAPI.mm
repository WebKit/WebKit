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

#if ENABLE(NOTIFICATIONS) && !(PLATFORM(IOS) || PLATFORM(VISION))
#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <wtf/Function.h>
#import <wtf/RetainPtr.h>

static unsigned clientPermissionRequestCount;
static bool didReceiveMessage;

@interface NotificationPermissionMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation NotificationPermissionMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message body])
        [receivedMessages addObject:[message body]];
    else
        [receivedMessages addObject:@""];
    didReceiveMessage = true;
}
@end

@interface NotificationPermissionUIDelegate : NSObject <WKUIDelegatePrivate> {
}
- (instancetype)initWithHandler:(Function<bool()>&&)decisionHandler;
@end

@implementation NotificationPermissionUIDelegate {
Function<bool()> _decisionHandler;
}

- (instancetype)initWithHandler:(Function<bool()>&&)decisionHandler
{
    self = [super init];
    _decisionHandler = WTFMove(decisionHandler);
    return self;
}

- (void)_webView:(WKWebView *)webView requestNotificationPermissionForSecurityOrigin:(WKSecurityOrigin *)securityOrigin decisionHandler:(void (^)(BOOL))decisionHandler
{
    decisionHandler(_decisionHandler());
    ++clientPermissionRequestCount;
}

@end

namespace TestWebKitAPI {

enum class ShouldGrantPermission : bool { No, Yes };
static void runRequestPermissionTest(ShouldGrantPermission shouldGrantPermission)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[NotificationPermissionMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto uiDelegate = adoptNS([[NotificationPermissionUIDelegate alloc] initWithHandler:[shouldGrantPermission] { return shouldGrantPermission == ShouldGrantPermission::Yes; }]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    didReceiveMessage = false;
    clientPermissionRequestCount = 0;
    [receivedMessages removeAllObjects];

    [webView evaluateJavaScript:@"Notification.requestPermission((permission) => { webkit.messageHandlers.testHandler.postMessage(permission) });" completionHandler:nil];
    TestWebKitAPI::Util::run(&didReceiveMessage);

    EXPECT_EQ(clientPermissionRequestCount, 1U);
    if (shouldGrantPermission == ShouldGrantPermission::Yes)
        EXPECT_WK_STREQ(@"granted", receivedMessages.get()[0]);
    else
        EXPECT_WK_STREQ(@"denied", receivedMessages.get()[0]);


    didReceiveMessage = false;
    [webView evaluateJavaScript:@"Notification.requestPermission((permission) => { webkit.messageHandlers.testHandler.postMessage(permission) });" completionHandler:nil];
    TestWebKitAPI::Util::run(&didReceiveMessage);

    // All calls to Notification.requestPermission result in a call to the client to request permission.
    EXPECT_EQ(clientPermissionRequestCount, 2U);
    if (shouldGrantPermission == ShouldGrantPermission::Yes)
        EXPECT_WK_STREQ(@"granted", receivedMessages.get()[1]);
    else
        EXPECT_WK_STREQ(@"denied", receivedMessages.get()[1]);
}

TEST(Notification, RequestPermissionDenied)
{
    runRequestPermissionTest(ShouldGrantPermission::No);
}

TEST(Notification, RequestPermissionGranted)
{
    runRequestPermissionTest(ShouldGrantPermission::Yes);
}

static void runParallelPermissionRequestsTest(ShouldGrantPermission shouldGrantPermission)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[NotificationPermissionMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto uiDelegate = adoptNS([[NotificationPermissionUIDelegate alloc] initWithHandler:[shouldGrantPermission] { return shouldGrantPermission == ShouldGrantPermission::Yes; }]);
    [webView setUIDelegate:uiDelegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    clientPermissionRequestCount = 0;
    [receivedMessages removeAllObjects];

    constexpr unsigned permissionRequestsCount = 5;
    for (unsigned i = 0; i < permissionRequestsCount; ++i)
        [webView evaluateJavaScript:@"Notification.requestPermission((permission) => { webkit.messageHandlers.testHandler.postMessage(permission) });" completionHandler:nil];

    while ([receivedMessages count] != permissionRequestsCount)
        TestWebKitAPI::Util::spinRunLoop(10);

    // We should have called out to the client only once.
    EXPECT_EQ(clientPermissionRequestCount, 1U);

    // All requests should have the same permission.
    for (id message in receivedMessages.get()) {
        if (shouldGrantPermission == ShouldGrantPermission::Yes)
            EXPECT_WK_STREQ(@"granted", message);
        else
            EXPECT_WK_STREQ(@"denied", message);
    }
}

TEST(Notification, ParallelPermissionRequestsDenied)
{
    runParallelPermissionRequestsTest(ShouldGrantPermission::No);
}

TEST(Notification, ParallelPermissionRequestsGranted)
{
    runParallelPermissionRequestsTest(ShouldGrantPermission::Yes);
}

} // namespace TestWebKitAPI

#endif // ENABLE(NOTIFICATIONS) && !(PLATFORM(IOS) || PLATFORM(VISION))
