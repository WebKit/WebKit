/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) || PLATFORM(VISION)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>

#if HAVE(STYLUS_DEVICE_OBSERVATION)

@interface WKStylusDeviceObserver
+ (WKStylusDeviceObserver *)sharedInstance;
@property (nonatomic) BOOL hasStylusDevice;
- (void)start;
- (void)startChangeTimer:(NSTimeInterval)timeInterval;
@end

static WKStylusDeviceObserver *getStylusDeviceObserver()
{
    return [NSClassFromString(@"WKStylusDeviceObserver") sharedInstance];
}

enum class HasStylusDevice : bool { No, Yes };
static RetainPtr<TestWKWebView> createWebView(HasStylusDevice hasStylusDevice)
{
    WKStylusDeviceObserver *stylusDeviceObserver = getStylusDeviceObserver();
    [stylusDeviceObserver start];
    stylusDeviceObserver.hasStylusDevice = hasStylusDevice == HasStylusDevice::Yes;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@""];
    return webView;
}

TEST(iOSStylusSupport, StylusInitiallyDisconnected)
{
    auto webView = createWebView(HasStylusDevice::No);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSStylusSupport, StylusInitiallyConnected)
{
    auto webView = createWebView(HasStylusDevice::Yes);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSStylusSupport, StylusLaterDisconnected)
{
    auto webView = createWebView(HasStylusDevice::Yes);

    getStylusDeviceObserver().hasStylusDevice = NO;

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSStylusSupport, StylusLaterConnected)
{
    auto webView = createWebView(HasStylusDevice::No);

    getStylusDeviceObserver().hasStylusDevice = YES;

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSStylusSupport, StylusDisconnectedTimeoutFire)
{
    auto webView = createWebView(HasStylusDevice::Yes);

    [getStylusDeviceObserver() startChangeTimer:0.01];

    __block BOOL done = false;
    [NSTimer scheduledTimerWithTimeInterval:0.05 repeats:NO block:^(NSTimer *timer) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

TEST(iOSStylusSupport, StylusDisconnectedTimeoutCancel)
{
    auto webView = createWebView(HasStylusDevice::Yes);

    [getStylusDeviceObserver() startChangeTimer:0.01];

    getStylusDeviceObserver().hasStylusDevice = YES;

    __block BOOL done = false;
    [NSTimer scheduledTimerWithTimeInterval:0.05 repeats:NO block:^(NSTimer *timer) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"pointer: coarse"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"pointer: fine"]);

    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer"]);
    EXPECT_FALSE([webView evaluateMediaQuery:@"any-pointer: none"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: coarse"]);
    EXPECT_TRUE([webView evaluateMediaQuery:@"any-pointer: fine"]);
}

#endif // HAVE(STYLUS_DEVICE_OBSERVATION)

#endif // PLATFORM(IOS) || PLATFORM(VISION)
