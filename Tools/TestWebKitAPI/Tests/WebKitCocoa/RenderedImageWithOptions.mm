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

#if WK_API_ENABLED

#import "RenderedImageWithOptionsProtocol.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

using namespace TestWebKitAPI;

static void runTestWithWidth(NSNumber *width, CGSize expectedSize)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"RenderedImageWithOptionsPlugIn"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(RenderedImageWithOptionsProtocol)];
    auto remoteObject = retainPtr([[webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface]);

    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"rendered-image-excluding-overflow" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    [webView _test_waitForDidFinishNavigation];

    __block bool testFinished = false;
    [remoteObject renderImageWithWidth:width completionHandler:^(CGSize imageSize) {
#if PLATFORM(IOS_FAMILY)
        CGFloat scale = [UIScreen mainScreen].scale;
#elif PLATFORM(MAC)
        CGFloat scale = [NSScreen mainScreen].backingScaleFactor;
#endif
        EXPECT_EQ(expectedSize.width, imageSize.width / scale);
        EXPECT_EQ(expectedSize.height, imageSize.height / scale);
        testFinished = true;
    }];

    Util::run(&testFinished);
}

TEST(WebKit, NodeHandleRenderedImageExcludingOverflow)
{
    runTestWithWidth(nil, { 720, 540 });
}

TEST(WebKit, NodeHandleRenderedImageWithWidth)
{
    runTestWithWidth(@(-1), { 0, 0 });
    runTestWithWidth(@0, { 0, 0 });
    runTestWithWidth(@1, { 1, 1 });
    runTestWithWidth(@360, { 360, 270 });
    runTestWithWidth(@719, { 719, 539 });
    runTestWithWidth(@720, { 720, 540 });
    runTestWithWidth(@721, { 721, 541 });
}

#endif // WK_API_ENABLED
