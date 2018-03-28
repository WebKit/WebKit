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

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKURLSchemeHandler.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <WebKit/_WKWebsitePolicies.h>
#import <wtf/Deque.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

#if WK_API_ENABLED

static bool done;
static bool didCreateWebView;
static int numberOfDecidePolicyCalls;

static RetainPtr<NSMutableArray> receivedMessages = adoptNS([@[] mutableCopy]);
static bool receivedMessage;
@interface PSONMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation PSONMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    [receivedMessages addObject:[message body]];
    receivedMessage = true;
}
@end

@interface PSONNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation PSONNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    done = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    ++numberOfDecidePolicyCalls;
    decisionHandler(WKNavigationActionPolicyAllow);
}

@end

static RetainPtr<WKWebView> createdWebView;

@interface PSONUIDelegate : NSObject <WKUIDelegatePrivate>
- (instancetype)initWithNavigationDelegate:(PSONNavigationDelegate *)navigationDelegate;
@end

@implementation PSONUIDelegate {
    RetainPtr<PSONNavigationDelegate> _navigationDelegate;
}

- (instancetype)initWithNavigationDelegate:(PSONNavigationDelegate *)navigationDelegate
{
    if (!(self = [super init]))
        return nil;

    _navigationDelegate = navigationDelegate;
    return self;
}

- (nullable WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    createdWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    [createdWebView setNavigationDelegate:_navigationDelegate.get()];
    didCreateWebView = true;
    return createdWebView.get();
}

@end

@interface PSONScheme : NSObject <WKURLSchemeHandler> {
    const char* _bytes;
}
- (instancetype)initWithBytes:(const char*)bytes;
@end

@implementation PSONScheme

- (instancetype)initWithBytes:(const char*)bytes
{
    self = [super init];
    _bytes = bytes;
    return self;
}

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)task
{
    RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:1 textEncodingName:nil]);
    [task didReceiveResponse:response.get()];

    if (_bytes) {
        RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytesNoCopy:(void *)_bytes length:strlen(_bytes) freeWhenDone:NO]);
        [task didReceiveData:data.get()];
    } else
        [task didReceiveData:[@"Hello" dataUsingEncoding:NSUTF8StringEncoding]];

    [task didFinish];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)task
{
}

@end

static const char* testBytes = R"PSONRESOURCE(
<head>
<script>

function log(msg)
{
    window.webkit.messageHandlers.pson.postMessage(msg);
}

window.onload = function(evt) {
    if (window.history.state != "onloadCalled")
        setTimeout('window.history.replaceState("onloadCalled", "");', 0);
}

window.onpageshow = function(evt) {
    log("PageShow called. Persisted: " + evt.persisted + ", and window.history.state is: " + window.history.state);
}

</script>
</head>
)PSONRESOURCE";

static const char* windowOpenCrossOriginNoOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    window.open("pson2://host/main2.html", "_blank", "noopener");
}
</script>
)PSONRESOURCE";

static const char* windowOpenCrossOriginWithOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    window.open("pson2://host/main2.html");
}
</script>
)PSONRESOURCE";

static const char* windowOpenSameOriginNoOpenerTestBytes = R"PSONRESOURCE(
<script>
window.onload = function() {
    if (!opener)
        window.open("pson1://host/main2.html", "_blank", "noopener");
}
</script>
)PSONRESOURCE";

static const char* dummyBytes = R"PSONRESOURCE(
<body>TEST</body>
)PSONRESOURCE";

TEST(ProcessSwap, Basic)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    RetainPtr<PSONScheme> handler = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON1"];
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON2"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson1://host/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson1://host/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson2://host/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];

    EXPECT_EQ(pid1, pid2);
    EXPECT_FALSE(pid2 == pid3);

    // 3 loads, 3 decidePolicy calls (e.g. the load that did perform a process swap should not have generated an additional decidePolicy call)
    EXPECT_EQ(numberOfDecidePolicyCalls, 3);
}

TEST(ProcessSwap, Back)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    RetainPtr<PSONScheme> handler1 = adoptNS([[PSONScheme alloc] initWithBytes:testBytes]);
    RetainPtr<PSONScheme> handler2 = adoptNS([[PSONScheme alloc] init]);
    [webViewConfiguration setURLSchemeHandler:handler1.get() forURLScheme:@"PSON1"];
    [webViewConfiguration setURLSchemeHandler:handler2.get() forURLScheme:@"PSON2"];

    RetainPtr<PSONMessageHandler> messageHandler = adoptNS([[PSONMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"pson"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto delegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson1://host/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid1 = [webView _webProcessIdentifier];

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson2://host/main2.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid2 = [webView _webProcessIdentifier];

    [webView goBack];

    TestWebKitAPI::Util::run(&receivedMessage);
    receivedMessage = false;
    TestWebKitAPI::Util::run(&done);
    done = false;

    auto pid3 = [webView _webProcessIdentifier];

    // 3 loads, 3 decidePolicy calls (e.g. any load that performs a process swap should not have generated an
    // additional decidePolicy call as a result of the process swap)
    EXPECT_EQ(numberOfDecidePolicyCalls, 3);

    EXPECT_EQ([receivedMessages count], 2u);
    EXPECT_TRUE([receivedMessages.get()[0] isEqualToString:@"PageShow called. Persisted: false, and window.history.state is: null"]);

    // FIXME: We'd like to get the page restoring from the page cache like before process swapping, which will make Persisted be "true"
    // For now it's expected to be false"
    EXPECT_TRUE([receivedMessages.get()[1] isEqualToString:@"PageShow called. Persisted: false, and window.history.state is: onloadCalled"]);

    EXPECT_FALSE(pid1 == pid2);
    EXPECT_FALSE(pid2 == pid3);

    // FIXME: Ideally we'd like to get process caching happening such that pid1 and pid3 are equal.
    // But for now they should not be.
    EXPECT_FALSE(pid1 == pid3);
}

#if PLATFORM(MAC)

TEST(ProcessSwap, CrossOriginWindowOpenNoOpener)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    RetainPtr<PSONScheme> handler1 = adoptNS([[PSONScheme alloc] initWithBytes:windowOpenCrossOriginNoOpenerTestBytes]);
    RetainPtr<PSONScheme> handler2 = adoptNS([[PSONScheme alloc] initWithBytes:dummyBytes]);
    [webViewConfiguration setURLSchemeHandler:handler1.get() forURLScheme:@"PSON1"];
    [webViewConfiguration setURLSchemeHandler:handler2.get() forURLScheme:@"PSON2"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson1://host/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_NE(pid1, pid2);
}

TEST(ProcessSwap, CrossOriginWindowOpenWithOpener)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    RetainPtr<PSONScheme> handler1 = adoptNS([[PSONScheme alloc] initWithBytes:windowOpenCrossOriginWithOpenerTestBytes]);
    RetainPtr<PSONScheme> handler2 = adoptNS([[PSONScheme alloc] initWithBytes:dummyBytes]);
    [webViewConfiguration setURLSchemeHandler:handler1.get() forURLScheme:@"PSON1"];
    [webViewConfiguration setURLSchemeHandler:handler2.get() forURLScheme:@"PSON2"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson1://host/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    // FIXME: This should eventually be false once we support process swapping when there is an opener.
    EXPECT_EQ(pid1, pid2);
}

TEST(ProcessSwap, SameOriginWindowOpenNoOpener)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = YES;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];
    RetainPtr<PSONScheme> handler = adoptNS([[PSONScheme alloc] initWithBytes:windowOpenSameOriginNoOpenerTestBytes]);
    [webViewConfiguration setURLSchemeHandler:handler.get() forURLScheme:@"PSON1"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto navigationDelegate = adoptNS([[PSONNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    auto uiDelegate = adoptNS([[PSONUIDelegate alloc] initWithNavigationDelegate:navigationDelegate.get()]);
    [webView setUIDelegate:uiDelegate.get()];

    numberOfDecidePolicyCalls = 0;
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"pson1://host/main1.html"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&done);
    done = false;

    TestWebKitAPI::Util::run(&didCreateWebView);
    didCreateWebView = false;

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(2, numberOfDecidePolicyCalls);

    auto pid1 = [webView _webProcessIdentifier];
    EXPECT_TRUE(!!pid1);
    auto pid2 = [createdWebView _webProcessIdentifier];
    EXPECT_TRUE(!!pid2);

    EXPECT_EQ(pid1, pid2);
}

#endif // PLATFORM(MAC)

#endif // WK_API_ENABLED
