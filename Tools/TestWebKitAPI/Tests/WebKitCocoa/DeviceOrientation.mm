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

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <wtf/Function.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

static RetainPtr<NSMutableArray> receivedMessages = adoptNS([@[] mutableCopy]);
static bool didReceiveMessage;

@interface DeviceOrientationMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation DeviceOrientationMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message body])
        [receivedMessages addObject:[message body]];
    else
        [receivedMessages addObject:@""];

    didReceiveMessage = true;
}
@end

@interface DeviceOrientationPermissionUIDelegate : NSObject <WKUIDelegatePrivate> {
}
- (instancetype)initWithHandler:(Function<bool()>&&)decisionHandler;
@end

@implementation DeviceOrientationPermissionUIDelegate {
Function<bool()> _decisionHandler;
}

- (instancetype)initWithHandler:(Function<bool()>&&)decisionHandler
{
    self = [super init];
    _decisionHandler = WTFMove(decisionHandler);
    return self;
}

- (void)_webView:(WKWebView *)webView shouldAllowDeviceOrientationAndMotionAccessRequestedByFrame:(WKFrameInfo *)requestingFrame decisionHandler:(void (^)(BOOL))decisionHandler
{
    decisionHandler(_decisionHandler());
}

@end

enum class DeviceOrientationPermission { Granted, Denied, Default };
static void runDeviceOrientationTest(DeviceOrientationPermission deviceOrientationPermission)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

    auto messageHandler = adoptNS([[DeviceOrientationMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<DeviceOrientationPermissionUIDelegate> uiDelegate;
    switch (deviceOrientationPermission) {
    case DeviceOrientationPermission::Granted:
        uiDelegate = adoptNS([[DeviceOrientationPermissionUIDelegate alloc] initWithHandler:[] { return true; }]);
        break;
    case DeviceOrientationPermission::Denied:
        uiDelegate = adoptNS([[DeviceOrientationPermissionUIDelegate alloc] initWithHandler:[] { return false; }]);
        break;
    case DeviceOrientationPermission::Default:
        break;
    }
    [webView setUIDelegate:uiDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    [webView _test_waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"DeviceOrientationEvent.requestPermission().then((granted) => { webkit.messageHandlers.testHandler.postMessage(granted) });" completionHandler: [&] (id result, NSError *error) { }];

    TestWebKitAPI::Util::run(&didReceiveMessage);
    didReceiveMessage = false;

    switch (deviceOrientationPermission) {
    case DeviceOrientationPermission::Granted:
        EXPECT_WK_STREQ(@"granted", receivedMessages.get()[0]);
        break;
    case DeviceOrientationPermission::Denied:
    case DeviceOrientationPermission::Default:
        EXPECT_WK_STREQ(@"denied", receivedMessages.get()[0]);
        break;
    }

    bool addedEventListener = false;
    [webView evaluateJavaScript:@"addEventListener('deviceorientation', (e) => { webkit.messageHandlers.testHandler.postMessage('received-event') });" completionHandler: [&] (id result, NSError *error) {
        addedEventListener = true;
    }];

    TestWebKitAPI::Util::run(&addedEventListener);
    addedEventListener = false;

    [webView _simulateDeviceOrientationChangeWithAlpha:1.0 beta:2.0 gamma:3.0];

    if (deviceOrientationPermission == DeviceOrientationPermission::Granted) {
        TestWebKitAPI::Util::run(&didReceiveMessage);
        EXPECT_WK_STREQ(@"received-event", receivedMessages.get()[1]);
    } else {
        TestWebKitAPI::Util::sleep(0.1);
        EXPECT_FALSE(didReceiveMessage);
    }
    didReceiveMessage = false;
}

TEST(DeviceOrientation, PermissionDeniedByDefault)
{
    runDeviceOrientationTest(DeviceOrientationPermission::Default);
}

TEST(DeviceOrientation, PermissionGranted)
{
    runDeviceOrientationTest(DeviceOrientationPermission::Granted);
}

TEST(DeviceOrientation, PermissionDenied)
{
    runDeviceOrientationTest(DeviceOrientationPermission::Denied);
}

#endif // PLATFORM(IOS_FAMILY)
