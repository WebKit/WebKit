/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static RetainPtr<NSMutableArray> consoleMessages;

static void didReceivePageMessageFromInjectedBundle(WKPageRef, WKStringRef messageName, WKTypeRef message, const void*)
{
    if (WKStringIsEqualToUTF8CString(messageName, "ConsoleMessage"))
        [consoleMessages addObject:Util::toNS((WKStringRef)message)];
}

static void setInjectedBundleClient(WKWebView *webView)
{
    WKPageInjectedBundleClientV0 injectedBundleClient = {
        { 0, nullptr },
        didReceivePageMessageFromInjectedBundle,
        nullptr,
    };
    WKPageSetPageInjectedBundleClient(webView._pageRefForTransitionToWKWebView, &injectedBundleClient.base);
}

TEST(WebKit, ConsoleMessageWithDetails)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundlePageConsoleMessageWithDetails"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    setInjectedBundleClient(webView.get());

    consoleMessages = [NSMutableArray array];
    [webView synchronouslyLoadTestPageNamed:@"console-message-with-details"];

    EXPECT_EQ([consoleMessages count], 2u);
    EXPECT_TRUE([consoleMessages.get()[0] isEqualToString:@"Hello world!"]);
    EXPECT_TRUE([consoleMessages.get()[1] isEqualToString:@"Hello world!, arguments: argument1, argument2"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
