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

#if WK_API_ENABLED

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import <WebKit/WKBrowsingContextController.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_HAVE_C_SPI
#import "PlatformWebView.h"
#import <WebKit/WKBrowsingContextLoadDelegate.h>
#import <WebKit/WKContextPrivate.h>
#import <WebKit/WKPageUIClient.h>
#endif

static NSString * const echoScheme = @"echo";
static RetainPtr<NSURL> lastEchoedURL;

@interface EchoURLProtocol : NSURLProtocol
@end

@implementation EchoURLProtocol

+ (BOOL)canInitWithRequest:(NSURLRequest *)request
{
    return [request.URL.scheme isEqualToString:echoScheme];
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request
{
    return request;
}

+ (BOOL)requestIsCacheEquivalent:(NSURLRequest *)a toRequest:(NSURLRequest *)b
{
    return NO;
}

- (void)startLoading
{
    NSURL *requestURL = self.request.URL;
    EXPECT_TRUE([requestURL.scheme isEqualToString:echoScheme]);

    lastEchoedURL = requestURL;

    NSData *data = [requestURL.absoluteString dataUsingEncoding:NSASCIIStringEncoding];
    auto response = adoptNS([[NSURLResponse alloc] initWithURL:self.request.URL MIMEType:@"text/html" expectedContentLength:[data length] textEncodingName:nil]);
    [self.client URLProtocol:self didReceiveResponse:response.get() cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:data];
    [self.client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading
{
}

@end

#if WK_HAVE_C_SPI

static bool didFinishLoad = false;

@interface WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequestLoadDelegate : NSObject <WKBrowsingContextLoadDelegate>
@end

@implementation WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequestLoadDelegate

- (void)browsingContextController:(WKBrowsingContextController *)sender didFailProvisionalLoadWithError:(NSError *)error
{
    didFinishLoad = true;
}

- (void)browsingContextControllerDidFinishLoad:(WKBrowsingContextController *)sender
{
    didFinishLoad = true;
}

@end

#endif

namespace TestWebKitAPI {

TEST(WebKit, RegisterAsCanDisplayOnlyIfCanRequest_SameOriginLoad)
{
    @autoreleasepool {
        [NSURLProtocol registerClass:[EchoURLProtocol class]];
        [WKBrowsingContextController registerSchemeForCustomProtocol:echoScheme];

        auto processPool = adoptNS([[WKProcessPool alloc] init]);
        [processPool _registerURLSchemeAsCanDisplayOnlyIfCanRequest:echoScheme];

        auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [webViewConfiguration setProcessPool:processPool.get()];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration.get()]);

        NSString *htmlString = @"<!DOCTYPE html><body><script src='echo://A/A_1'></script>";
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"echo://A"]];
        [webView _test_waitForDidFinishNavigation];
        EXPECT_WK_STREQ(@"echo://A/A_1", [lastEchoedURL absoluteString]);
        lastEchoedURL = nullptr;

        [WKBrowsingContextController unregisterSchemeForCustomProtocol:echoScheme];
        [NSURLProtocol unregisterClass:[EchoURLProtocol class]];
    }
}

TEST(WebKit, RegisterAsCanDisplayOnlyIfCanRequest_CrossOriginLoad)
{
    @autoreleasepool {
        [NSURLProtocol registerClass:[EchoURLProtocol class]];
        [WKBrowsingContextController registerSchemeForCustomProtocol:echoScheme];

        auto processPool = adoptNS([[WKProcessPool alloc] init]);
        [processPool _registerURLSchemeAsCanDisplayOnlyIfCanRequest:echoScheme];

        auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [webViewConfiguration setProcessPool:processPool.get()];

        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration.get()]);

        NSString *htmlString = @"<!DOCTYPE html><body><script src='echo://A/A_1'></script>";
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"echo://B"]];
        [webView _test_waitForDidFinishNavigation];
        EXPECT_NULL(lastEchoedURL); // Script blocked
        lastEchoedURL = nullptr;

        [WKBrowsingContextController unregisterSchemeForCustomProtocol:echoScheme];
        [NSURLProtocol unregisterClass:[EchoURLProtocol class]];
    }
}

#if WK_HAVE_C_SPI

TEST(WebKit, WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest_SameOriginLoad)
{
    @autoreleasepool {
        [NSURLProtocol registerClass:[EchoURLProtocol class]];
        [WKBrowsingContextController registerSchemeForCustomProtocol:echoScheme];

        WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
        WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(context.get(), Util::toWK(echoScheme.UTF8String).get());

        PlatformWebView webView { context.get() };
        webView.platformView().browsingContextController.loadDelegate = [[WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequestLoadDelegate alloc] init];
        [webView.platformView().browsingContextController loadHTMLString:@"<!DOCTYPE html><body><script src='echo://A/A_1'></script>" baseURL:[NSURL URLWithString:@"echo://A"]];
        Util::run(&didFinishLoad);
        didFinishLoad = false;

        EXPECT_WK_STREQ(@"echo://A/A_1", [lastEchoedURL absoluteString]);
        lastEchoedURL = nullptr;

        [WKBrowsingContextController unregisterSchemeForCustomProtocol:echoScheme];
        [NSURLProtocol unregisterClass:[EchoURLProtocol class]];
    }
}

TEST(WebKit, WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest_CrossOriginLoad)
{
    @autoreleasepool {
        [NSURLProtocol registerClass:[EchoURLProtocol class]];
        [WKBrowsingContextController registerSchemeForCustomProtocol:echoScheme];

        WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
        WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(context.get(), Util::toWK(echoScheme.UTF8String).get());

        PlatformWebView webView { context.get() };
        webView.platformView().browsingContextController.loadDelegate = [[WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequestLoadDelegate alloc] init];
        [webView.platformView().browsingContextController loadHTMLString:@"<!DOCTYPE html><body><script src='echo://A/A_1'></script>" baseURL:[NSURL URLWithString:@"echo://B"]];
        Util::run(&didFinishLoad);
        didFinishLoad = false;

        EXPECT_NULL(lastEchoedURL); // Script blocked
        lastEchoedURL = nullptr;

        [WKBrowsingContextController unregisterSchemeForCustomProtocol:echoScheme];
        [NSURLProtocol unregisterClass:[EchoURLProtocol class]];
    }
}

#endif // WK_HAVE_C_SPI

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED
