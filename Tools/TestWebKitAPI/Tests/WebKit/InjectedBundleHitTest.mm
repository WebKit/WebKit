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

#import "InjectedBundleHitTestProtocol.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

@interface WKWebViewWithHitTester : TestWKWebView
@property (nonatomic, weak) id <InjectedBundleHitTestProtocol> hitTester;
@end

@implementation WKWebViewWithHitTester

- (BOOL)hasSelectableTextAt:(CGPoint)point
{
    __block bool done = false;
    __block BOOL hasSelectableText = NO;
    [_hitTester hasSelectableTextAt:[NSValue valueWithPoint:point] completionHandler:^(BOOL result) {
        hasSelectableText = result;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    return hasSelectableText;
}

@end

namespace TestWebKitAPI {

static RetainPtr<WKWebViewWithHitTester> createWebViewWithHitTester()
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"InjectedBundleHitTestPlugIn" configureJSCForTesting:YES];
    auto webView = adoptNS([[WKWebViewWithHitTester alloc] initWithFrame:CGRectMake(0, 0, 500, 500) configuration:configuration]);

    auto interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(InjectedBundleHitTestProtocol)];
    [webView setHitTester:[[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface]];
    return webView;
}

TEST(InjectedBundleHitTest, TextParagraph)
{
    auto webView = createWebViewWithHitTester();
    [webView synchronouslyLoadTestPageNamed:@"simple-responsive-page"];

    EXPECT_TRUE([webView hasSelectableTextAt:CGPointMake(40, 40)]);
    EXPECT_FALSE([webView hasSelectableTextAt:CGPointMake(300, 300)]);
}

#if ENABLE(IMAGE_ANALYSIS)

TEST(InjectedBundleHitTest, ImageOverlay)
{
    auto webView = createWebViewWithHitTester();
    [webView synchronouslyLoadTestPageNamed:@"simple-image-overlay"];

    EXPECT_TRUE([webView hasSelectableTextAt:CGPointMake(150, 40)]);
    EXPECT_FALSE([webView hasSelectableTextAt:CGPointMake(150, 200)]);
    EXPECT_FALSE([webView hasSelectableTextAt:CGPointMake(400, 300)]);
}

#endif // ENABLE(IMAGE_ANALYSIS)

} // namespace TestWebKitAPI
