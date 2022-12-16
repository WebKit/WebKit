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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKNavigationResponsePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

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

@interface NavigationResponseTestDelegate : NSObject <WKNavigationDelegate>

@property (nonatomic, readonly) WKNavigationResponse *navigationResponse;
@property (nonatomic) WKNavigationResponsePolicy navigationPolicy;

@end

@implementation NavigationResponseTestDelegate {
    RetainPtr<WKNavigationResponse> _navigationResponse;
    bool _hasReceivedResponseCallback;
    bool _hasReceivedNavigationFinishedCallback;
}

- (WKNavigationResponse *)navigationResponse
{
    return _navigationResponse.get();
}

- (void)waitForNavigationResponseCallback
{
    _hasReceivedResponseCallback = false;
    TestWebKitAPI::Util::run(&_hasReceivedResponseCallback);
}

- (void)waitForNavigationFinishedCallback
{
    _hasReceivedNavigationFinishedCallback = false;
    TestWebKitAPI::Util::run(&_hasReceivedNavigationFinishedCallback);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _hasReceivedNavigationFinishedCallback = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(WKNavigationResponsePolicyAllow);
    _navigationResponse = navigationResponse;
    _hasReceivedResponseCallback = true;
}

@end

TEST(WebKit, WKNavigationResponseDownloadAttribute)
{
    auto getDownloadResponse = [] (RetainPtr<NSString> body) -> RetainPtr<WKNavigationResponse> {
        using namespace TestWebKitAPI;
        HTTPServer server([body](Connection connection) {
            connection.receiveHTTPRequest([=](Vector<char>&&) {
                unsigned bodyLength = [body length];
                NSString *firstResponse = [NSString stringWithFormat:
                    @"HTTP/1.1 200 OK\r\n"
                    "Content-Length: %d\r\n\r\n"
                    "%@",
                    bodyLength,
                    body.get()
                ];
                connection.send(firstResponse, [=] {
                    connection.receiveHTTPRequest([=](Vector<char>&&) {
                        NSString *secondResponse = @"HTTP/1.1 200 OK\r\n"
                            "Content-Length: 6\r\n"
                            "Content-Disposition: attachment; filename=fromHeader.txt;\r\n\r\n"
                            "Hello!";
                        connection.send(secondResponse);
                    });
                });
            });
        });
        auto delegate = adoptNS([NavigationResponseTestDelegate new]);
        auto webView = adoptNS([WKWebView new]);
        [webView setNavigationDelegate:delegate.get()];

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForNavigationFinishedCallback];

        [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
        [delegate waitForNavigationResponseCallback];
        EXPECT_STREQ([delegate navigationResponse].response.suggestedFilename.UTF8String, "fromHeader.txt");
        return [delegate navigationResponse];
    };
    auto shouldBeFromDownloadAttribute = getDownloadResponse(@"<a id='link' href='/fromHref.txt' download='fromDownloadAttribute.txt'>Click Me!</a>");
    auto shouldBeNull = getDownloadResponse(@"<a id='link' href='/fromHref.txt'>Click Me!</a>");
    auto shouldBeEmpty = getDownloadResponse(@"<a id='link' href='/fromHref.txt' download=''>Click Me!</a>");
    
    EXPECT_STREQ([shouldBeFromDownloadAttribute _downloadAttribute].UTF8String, "fromDownloadAttribute.txt");
    EXPECT_STREQ([shouldBeEmpty _downloadAttribute].UTF8String, "");
    EXPECT_NOT_NULL([shouldBeEmpty _downloadAttribute]);
    EXPECT_NULL([shouldBeNull _downloadAttribute]);
}

#if WK_HAVE_C_SPI
TEST(WebKit, SkipDecidePolicyForResponse)
{
    TestWebKitAPI::HTTPServer server({
        { "/1"_s, { { { "Content-Type"_s, "text/HTML"_s } }, "hi"_s } },
        { "/2"_s, { { { "Content-Type"_s, "text/plain"_s } }, "hi"_s } },
        { "/3"_s, { "hi"_s } },
        { "/4"_s, { 204, { { "Content-Type"_s, "text/HTML"_s } }, "hi"_s } },
        { "/5"_s, { 404, { { "Content-Type"_s, "text/HTML"_s } }, "hi"_s } },
        { "/6"_s, { 503, { { "Content-Type"_s, "text/HTML"_s } }, "hi"_s } },
        { "/7"_s, { { { "Content-Type"_s, "text/html"_s }, { "Content-Disposition"_s, "attachment ; other stuff"_s } }, "hi"_s } },
    });

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"SkipDecidePolicyForResponsePlugIn"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    __block bool responseDelegateCalled { false };
    delegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        responseDelegateCalled = true;
        completionHandler(WKNavigationResponsePolicyAllow);
    };
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:server.request("/1"_s)];
    [delegate waitForDidFinishNavigation];
    EXPECT_FALSE(responseDelegateCalled);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSBundle.mainBundle URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    [delegate waitForDidFinishNavigation];
    EXPECT_TRUE(std::exchange(responseDelegateCalled, false));

    for (auto& path : Vector { "/2"_s, "/3"_s, "/4"_s, "/5"_s, "/6"_s, "/7"_s }) {
        [webView loadRequest:server.request(path)];
        [delegate waitForDidFinishNavigation];
        EXPECT_TRUE(std::exchange(responseDelegateCalled, false));
    }
}
#endif
