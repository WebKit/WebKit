/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKWebView.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/NSURLExtras.h>

static bool didFinishTest;
static bool didFailProvisionalLoad;
static const char literal[] = "https://www.example.com<>/";

static NSURL *literalURL(const char* literal)
{
    return WTF::URLWithData([NSData dataWithBytes:literal length:strlen(literal)], nil);
}

@interface LoadInvalidURLNavigationActionDelegate : NSObject <WKNavigationDelegate>
@end

@implementation LoadInvalidURLNavigationActionDelegate

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    didFinishTest = true;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    EXPECT_WK_STREQ(error.domain, @"WebKitErrorDomain");
    EXPECT_EQ(error.code, 101);
    EXPECT_TRUE([error.userInfo[@"NSErrorFailingURLKey"] isEqual:literalURL(literal)]);

    didFailProvisionalLoad = true;
    didFinishTest = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKit, LoadInvalidURLRequest)
{
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        RetainPtr<LoadInvalidURLNavigationActionDelegate> delegate = adoptNS([[LoadInvalidURLNavigationActionDelegate alloc] init]);
        [webView setNavigationDelegate:delegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:literalURL(literal)]];

        didFinishTest = false;
        didFailProvisionalLoad = false;
        Util::run(&didFinishTest);

        EXPECT_TRUE(didFailProvisionalLoad);
    }
}

TEST(WebKit, LoadInvalidURLRequestNonASCII)
{
    __block bool done = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().webContentProcessDidTerminate = ^(WKWebView *) {
        ASSERT_NOT_REACHED();
    };
    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_WK_STREQ(error.domain, @"WebKitErrorDomain");
        EXPECT_EQ(error.code, WebKitErrorCannotShowURL);
        EXPECT_WK_STREQ([error.userInfo[@"NSErrorFailingURLKey"] absoluteString], "http://%E2%80%80");
        done = true;
    };
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    const UInt8 bytes[10] = { 'h', 't', 't', 'p', ':', '/', '/', 0xE2, 0x80, 0x80 };
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:(NSURL *)adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, bytes, 10, kCFStringEncodingUTF8, nullptr, true)).get()];
    [request _setProperty:request.URL forKey:@"_kCFHTTPCookiePolicyPropertySiteForCookies"];
    [webView loadRequest:request];
    Util::run(&done);
}

} // namespace TestWebKitAPI

