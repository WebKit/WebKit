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
#import <WebKit/WKFoundation.h>

#import "BundleCSSStyleDeclarationHandleProtocol.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

static bool didVerifyStyle;

@interface BundleCSSStyleDeclarationHandleRemoteObject : NSObject <BundleCSSStyleDeclarationHandleProtocol>
@end

@implementation BundleCSSStyleDeclarationHandleRemoteObject

- (void)verifyStyle:(BOOL)valid
{
    EXPECT_TRUE(valid);

    didVerifyStyle = true;
}

@end

TEST(WebKit, WKWebProcessPlugInCSSStyleDeclarationHandle)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleCSSStyleDeclarationHandlePlugIn"]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto object = adoptNS([[BundleCSSStyleDeclarationHandleRemoteObject alloc] init]);
    auto interface = retainPtr([_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(BundleCSSStyleDeclarationHandleProtocol)]);
    [[webView _remoteObjectRegistry] registerExportedObject:object.get() interface:interface.get()];

    NSString *html = @(
        "<script>"
        "function verifyStyle(style) {"
        "    return style === document.body.style;"
        "}"
        "</script>"
    );
    [webView loadHTMLString:html baseURL:nil];

    TestWebKitAPI::Util::run(&didVerifyStyle);
}
