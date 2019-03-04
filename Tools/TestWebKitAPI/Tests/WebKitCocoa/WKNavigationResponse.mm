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

#import "Utilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

static bool isDone;

@interface WKNavigationResponseTestNavigationDelegate : NSObject <WKNavigationDelegate>
@property (nonatomic) BOOL expectation;
@end

@implementation WKNavigationResponseTestNavigationDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    EXPECT_EQ(navigationResponse.canShowMIMEType, self.expectation);
    decisionHandler(WKNavigationResponsePolicyAllow);
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

@interface WKNavigationResponseTestSchemeHandler : NSObject <WKURLSchemeHandler>
@property (nonatomic, copy) NSString *mimeType;
@end

@implementation WKNavigationResponseTestSchemeHandler

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@"test:///json-response"] MIMEType:self.mimeType expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];
    [task didReceiveData:[@"{\"data\":1234}" dataUsingEncoding:NSUTF8StringEncoding]];
    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask { }

@end

TEST(WebKit, WKNavigationResponseJSONMIMEType)
{
    isDone = false;
    auto schemeHandler = adoptNS([[WKNavigationResponseTestSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[WKNavigationResponseTestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();

    schemeHandler.get().mimeType = @"application/json";
    navigationDelegate.get().expectation = YES;

    NSURL *testURL = [NSURL URLWithString:@"test:///json-response"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL]];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WebKit, WKNavigationResponseJSONMIMEType2)
{
    isDone = false;
    auto schemeHandler = adoptNS([[WKNavigationResponseTestSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[WKNavigationResponseTestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();

    schemeHandler.get().mimeType = @"application/vnd.api+json";
    navigationDelegate.get().expectation = YES;

    NSURL *testURL = [NSURL URLWithString:@"test:///json-response"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL]];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WebKit, WKNavigationResponseUnknownMIMEType)
{
    isDone = false;
    auto schemeHandler = adoptNS([[WKNavigationResponseTestSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[WKNavigationResponseTestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();

    schemeHandler.get().mimeType = @"garbage/json";
    navigationDelegate.get().expectation = NO;

    NSURL *testURL = [NSURL URLWithString:@"test:///json-response"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL]];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WebKit, WKNavigationResponsePDFType)
{
    isDone = false;
    auto schemeHandler = adoptNS([[WKNavigationResponseTestSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[WKNavigationResponseTestNavigationDelegate alloc] init]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [[[webView configuration] processPool] _addSupportedPlugin: @"" named: @"com.apple.webkit.builtinpdfplugin" withMimeTypes: [NSSet setWithArray: @[ @"application/pdf" ]] withExtensions: [NSSet setWithArray: @[ ]]];

    schemeHandler.get().mimeType = @"application/pdf";
    navigationDelegate.get().expectation = YES;

    NSURL *testURL = [NSURL URLWithString:@"test:///pdf-response"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL]];
    TestWebKitAPI::Util::run(&isDone);
}
