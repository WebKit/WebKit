/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <wtf/RetainPtr.h>

@interface NavigationActionTestDelegate : NSObject <WKNavigationDelegate>

@property (nonatomic, readonly) WKNavigationAction *navigationAction;
@property (nonatomic) WKNavigationActionPolicy navigationPolicy;

@end

@implementation NavigationActionTestDelegate {
    RetainPtr<WKNavigationAction> _navigationAction;
    bool _hasReceivedNavigationCallback;
    bool _hasFinishedNavigation;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _navigationPolicy = WKNavigationActionPolicyAllow;

    return self;
}

- (WKNavigationAction *)navigationAction
{
    return _navigationAction.get();
}

- (void)waitForNavigationActionCallback
{
    _navigationAction = nullptr;
    _hasReceivedNavigationCallback = false;
    TestWebKitAPI::Util::run(&_hasReceivedNavigationCallback);
}

- (void)waitForDidFinishNavigation
{
    _hasFinishedNavigation = false;
    TestWebKitAPI::Util::run(&_hasFinishedNavigation);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(_navigationPolicy);
    _navigationAction = navigationAction;
    _hasReceivedNavigationCallback = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _hasFinishedNavigation = true;
}

@end

TEST(WKNavigationAction, ShouldPerformDownload_NoDownloadAttribute)
{
    auto navigationDelegate = adoptNS([[NavigationActionTestDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto baseURL = retainPtr([NSURL URLWithString:@"https://example.com/index.html"]);
    [webView loadHTMLString:@"<a id='link' href='file.txt'>Click Me!</a>" baseURL:baseURL.get()];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
    navigationDelegate.get().navigationPolicy = WKNavigationActionPolicyCancel;
    [navigationDelegate waitForNavigationActionCallback];

    EXPECT_NOT_NULL(navigationDelegate.get().navigationAction);
    EXPECT_FALSE(navigationDelegate.get().navigationAction._shouldPerformDownload);
}

TEST(WKNavigationAction, ShouldPerformDownload_BlankDownloadAttribute_SameOrigin)
{
    auto navigationDelegate = adoptNS([[NavigationActionTestDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto baseURL = retainPtr([NSURL URLWithString:@"https://example.com/index.html"]);
    [webView loadHTMLString:@"<a id='link' href='file.txt' download>Click Me!</a>" baseURL:baseURL.get()];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
    navigationDelegate.get().navigationPolicy = WKNavigationActionPolicyCancel;
    [navigationDelegate waitForNavigationActionCallback];

    EXPECT_NOT_NULL(navigationDelegate.get().navigationAction);
    EXPECT_TRUE(navigationDelegate.get().navigationAction._shouldPerformDownload);
}

TEST(WKNavigationAction, ShouldPerformDownload_BlankDownloadAttribute_CrossOrigin)
{
    auto navigationDelegate = adoptNS([[NavigationActionTestDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto baseURL = retainPtr([NSURL URLWithString:@"https://example.com/index.html"]);
    [webView loadHTMLString:@"<a id='link' href='https://not-example-dot-com.com/file.txt' download>Click Me!</a>" baseURL:baseURL.get()];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
    navigationDelegate.get().navigationPolicy = WKNavigationActionPolicyCancel;
    [navigationDelegate waitForNavigationActionCallback];

    EXPECT_NOT_NULL(navigationDelegate.get().navigationAction);
    EXPECT_FALSE(navigationDelegate.get().navigationAction._shouldPerformDownload);
}

TEST(WKNavigationAction, ShouldPerformDownload_DownloadAttribute_SameOrigin)
{
    auto navigationDelegate = adoptNS([[NavigationActionTestDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto baseURL = retainPtr([NSURL URLWithString:@"https://example.com/index.html"]);
    [webView loadHTMLString:@"<a id='link' href='file.txt' download='file2.txt'>Click Me!</a>" baseURL:baseURL.get()];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
    navigationDelegate.get().navigationPolicy = WKNavigationActionPolicyCancel;
    [navigationDelegate waitForNavigationActionCallback];

    EXPECT_NOT_NULL(navigationDelegate.get().navigationAction);
    EXPECT_TRUE(navigationDelegate.get().navigationAction._shouldPerformDownload);
}

TEST(WKNavigationAction, ShouldPerformDownload_DownloadAttribute_CrossOrigin)
{
    auto navigationDelegate = adoptNS([[NavigationActionTestDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    auto baseURL = retainPtr([NSURL URLWithString:@"https://example.com/index.html"]);
    [webView loadHTMLString:@"<a id='link' href='https://not-example-dot-com.com/file.txt' download='file2.txt'>Click Me!</a>" baseURL:baseURL.get()];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"document.getElementById('link').click();" completionHandler:nil];
    navigationDelegate.get().navigationPolicy = WKNavigationActionPolicyCancel;
    [navigationDelegate waitForNavigationActionCallback];

    EXPECT_NOT_NULL(navigationDelegate.get().navigationAction);
    EXPECT_FALSE(navigationDelegate.get().navigationAction._shouldPerformDownload);
}

TEST(WKNavigationAction, BlobRequestBody)
{
    NSString *html = @""
        "<script>"
            "function bodyLoaded() {"
                "const form = Object.assign(document.createElement('form'), {"
                    "action: '/formAction', method: 'POST', enctype: 'multipart/form-data',"
                "});"
                "document.body.append(form);"
                "const fileInput = Object.assign(document.createElement('input'), {"
                    "type: 'file', name: 'file',"
                "});"
                "form.append(fileInput);"
                "const dataTransfer = new DataTransfer;"
                "dataTransfer.items.add(new File(['a'], 'filename'));"
                "fileInput.files = dataTransfer.files;"
                "form.submit();"
            "}"
        "</script>"
        "<body onload='bodyLoaded()'>";
    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    __block bool done = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        EXPECT_NULL(action.request.HTTPBody);
        if ([action.request.URL.absoluteString isEqualToString:@"about:blank"])
            completionHandler(WKNavigationActionPolicyAllow);
        else {
            EXPECT_WK_STREQ(action.request.URL.absoluteString, "/formAction");
            completionHandler(WKNavigationActionPolicyCancel);
            done = true;
        }
    };
    [webView loadHTMLString:html baseURL:nil];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKNavigationAction, NonMainThread)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { "hi"_s } },
    });

    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    __block bool done = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            completionHandler(WKNavigationActionPolicyAllow);
        });
    };
    delegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *action, void (^completionHandler)(WKNavigationResponsePolicy)) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            completionHandler(WKNavigationResponsePolicyAllow);
        });
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        done = true;
    };

    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
}
