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

#if PLATFORM(MAC)

#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <wtf/RetainPtr.h>

static _WKWebGLLoadPolicy firstPolicy;
static _WKWebGLLoadPolicy secondPolicy;

static RetainPtr<NSURL> firstURL;
static RetainPtr<NSURL> secondURL;
static RetainPtr<NSString> alert;

static bool testComplete { false };
static RetainPtr<NSURL> htmlURL;

static NSString *data = @"<script>"
    "var canvas = document.createElement('canvas');"
    "var context = canvas.getContext('webgl');"
    "if (context) {"
        "var framebuffer = context.createFramebuffer();"
        "var status = context.checkFramebufferStatus(context.FRAMEBUFFER);"
        "if (status == context.FRAMEBUFFER_UNSUPPORTED)"
            "alert('doing stuff with webgl context failed');"
        "else if (status == context.FRAMEBUFFER_COMPLETE)"
            "alert('doing stuff with webgl context succeeded');"
        "else alert('unexpected status');"
    "} else alert('webgl context creation failed');"
"</script>";

@interface WebGLTestDelegate : NSObject <WKNavigationDelegatePrivate, WKUIDelegate, WKURLSchemeHandler>
@end
    
@implementation WebGLTestDelegate

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    [urlSchemeTask didReceiveResponse:[[[NSURLResponse alloc] initWithURL:urlSchemeTask.request.URL MIMEType:@"text/html" expectedContentLength:data.length textEncodingName:nil] autorelease]];
    [urlSchemeTask didReceiveData:[data dataUsingEncoding:NSUTF8StringEncoding]];
    [urlSchemeTask didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
}

- (void)_webView:(WKWebView *)webView webGLLoadPolicyForURL:(NSURL *)url decisionHandler:(void (^)(_WKWebGLLoadPolicy))decisionHandler
{
    ASSERT(!firstURL);
    firstURL = url;
    decisionHandler(firstPolicy);
}

- (void)_webView:(WKWebView *)webView resolveWebGLLoadPolicyForURL:(NSURL *)url decisionHandler:(void (^)(_WKWebGLLoadPolicy))decisionHandler
{
    ASSERT(!secondURL);
    secondURL = url;
    decisionHandler(secondPolicy);
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    alert = message;
    testComplete = true;
    completionHandler();
}

@end

static void runTest(_WKWebGLLoadPolicy first, _WKWebGLLoadPolicy second = _WKWebGLLoadPolicyBlockCreation)
{
    testComplete = false;
    alert = nil;
    firstURL = nil;
    secondURL = nil;
    firstPolicy = first;
    secondPolicy = second;
    
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto delegate = adoptNS([[WebGLTestDelegate alloc] init]);
    [configuration setURLSchemeHandler:delegate.get() forURLScheme:@"test"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:htmlURL.get()]];
    TestWebKitAPI::Util::run(&testComplete);
}

TEST(WebKit, WebGLPolicy)
{
    htmlURL = [NSURL URLWithString:@"test:///html"];
    
    runTest(_WKWebGLLoadPolicyBlockCreation);
    EXPECT_STREQ([alert UTF8String], "webgl context creation failed");
    EXPECT_TRUE([htmlURL isEqual:firstURL.get()]);
    EXPECT_TRUE(nullptr == secondURL);

    runTest(_WKWebGLLoadPolicyAllowCreation);
    EXPECT_STREQ([alert UTF8String], "doing stuff with webgl context succeeded");
    EXPECT_TRUE([htmlURL isEqual:firstURL.get()]);
    EXPECT_TRUE(nullptr == secondURL);

    runTest(_WKWebGLLoadPolicyPendingCreation, _WKWebGLLoadPolicyBlockCreation);
    EXPECT_STREQ([alert UTF8String], "doing stuff with webgl context failed");
    EXPECT_TRUE([htmlURL isEqual:firstURL.get()]);
    EXPECT_TRUE([htmlURL isEqual:secondURL.get()]);

    runTest(_WKWebGLLoadPolicyPendingCreation, _WKWebGLLoadPolicyAllowCreation);
    // FIXME: This ought to succeed. https://bugs.webkit.org/show_bug.cgi?id=129122
    EXPECT_STREQ([alert UTF8String], "doing stuff with webgl context failed");
    EXPECT_TRUE([htmlURL isEqual:firstURL.get()]);
    EXPECT_TRUE([htmlURL isEqual:secondURL.get()]);
}

@interface DelegateWithoutWebGL : NSObject <WKUIDelegate>
@end

@implementation DelegateWithoutWebGL

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    alert = message;
    testComplete = true;
    completionHandler();
}

@end

TEST(WebKit, WebGLPolicyNoDelegate)
{
    auto delegate = adoptNS([[DelegateWithoutWebGL alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:data baseURL:[NSURL URLWithString:@"http://example.com/"]];
    TestWebKitAPI::Util::run(&testComplete);
    EXPECT_STREQ([alert UTF8String], "doing stuff with webgl context succeeded");
}

#endif // PLATFORM(MAC)
