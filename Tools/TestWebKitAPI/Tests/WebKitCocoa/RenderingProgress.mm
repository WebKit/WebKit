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
#import <WebKit/WKFoundation.h>

#import "PlatformUtilities.h"
#import "RenderingProgressProtocol.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKUserScriptPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static bool didObserveFirstMeaningfulPaint;

@interface DidFirstMeaningfulPaintRemoteObject : NSObject <DidFirstMeaningfulPaintProtocol>
@end

@implementation DidFirstMeaningfulPaintRemoteObject
- (void)didFirstMeaningfulPaint
{
    didObserveFirstMeaningfulPaint = true;
}
@end

TEST(RenderingProgress, FirstMeaningfulPaint)
{
    NSString * const testPlugInClassName = @"RenderingProgressPlugIn";

    RetainPtr<WKWebViewConfiguration> configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:testPlugInClassName]);
    
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    RetainPtr<DidFirstMeaningfulPaintRemoteObject> object = adoptNS([[DidFirstMeaningfulPaintRemoteObject alloc] init]);
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(DidFirstMeaningfulPaintProtocol)];
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface];

    didObserveFirstMeaningfulPaint = false;

    [webView loadHTMLString:@"<body style='background-color: red;'></body>" baseURL:nil];

    TestWebKitAPI::Util::run(&didObserveFirstMeaningfulPaint);
}

TEST(WKWebView, RenderTree)
{
    __block bool done { false };
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView loadHTMLString:@"<div>hi</div>" baseURL:nil];
    [webView _getRenderTreeAsStringWithCompletionHandler:^(NSString *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_WK_STREQ(result,
#if PLATFORM(MAC)
            "layer at (0,0) size 100x100\n"
            "  RenderView at (0,0) size 100x100\n"
            "layer at (0,0) size 100x100\n"
            "  RenderBlock {HTML} at (0,0) size 100x100\n"
            "    RenderBody {BODY} at (8,8) size 84x84\n"
#else
            "layer at (0,0) size 980x980\n"
            "  RenderView at (0,0) size 980x980\n"
            "layer at (0,0) size 980x980\n"
            "  RenderBlock {HTML} at (0,0) size 980x980\n"
            "    RenderBody {BODY} at (8,8) size 964x964\n"
#endif
        );
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}
