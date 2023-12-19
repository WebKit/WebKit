/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import <WebKit/WKBackForwardListPrivate.h>
#import <WebKit/WKErrorPrivate.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

static RetainPtr<WKNavigation> currentNavigation;
static RetainPtr<NSURL> redirectURL;
static NSTimeInterval redirectDelay;
static bool didCancelRedirect;

@interface NavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation NavigationDelegate

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    EXPECT_EQ(currentNavigation, navigation);
    EXPECT_NOT_NULL(navigation._request);
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    EXPECT_EQ(currentNavigation, navigation);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    EXPECT_EQ(currentNavigation, navigation);

    isDone = true;
}

@end

TEST(WKNavigation, NavigationDelegate)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto delegate = adoptNS([[NavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    @autoreleasepool {
        EXPECT_EQ(delegate, [webView navigationDelegate]);
    }

    delegate = nil;
    EXPECT_NULL([webView navigationDelegate]);
}

TEST(WKNavigation, LoadRequest)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    RetainPtr<NavigationDelegate> delegate = adoptNS([[NavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];

    currentNavigation = [webView loadRequest:request];
    ASSERT_NOT_NULL(currentNavigation);
    ASSERT_TRUE([[currentNavigation _request] isEqual:request]);

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKNavigation, HTTPBody)
{
    __block bool done = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    NSData *testData = [@"testhttpbody" dataUsingEncoding:NSUTF8StringEncoding];
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        EXPECT_TRUE([action.request.HTTPBody isEqualToData:testData]);
        decisionHandler(WKNavigationActionPolicyCancel);
        done = true;
    };
    auto webView = adoptNS([WKWebView new]);
    [webView setNavigationDelegate:delegate.get()];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"test:///willNotActuallyLoad"]];
    [request setHTTPBody:testData];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKNavigation, UserAgentAndAccept)
{
    using namespace TestWebKitAPI;
    HTTPServer server([](Connection) { });
    __block bool done = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        EXPECT_WK_STREQ(action.request.allHTTPHeaderFields[@"User-Agent"], "testUserAgent");
        EXPECT_WK_STREQ(action.request.allHTTPHeaderFields[@"Accept"], "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
        decisionHandler(WKNavigationActionPolicyCancel);
        done = true;
    };
    auto webView = adoptNS([WKWebView new]);
    webView.get().customUserAgent = @"testUserAgent";
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:server.request()];
    TestWebKitAPI::Util::run(&done);
}

@interface FrameNavigationDelegate : NSObject <WKNavigationDelegate>
- (void)waitForNavigations:(size_t)count;
- (void)clearState;
@property (nonatomic, readonly) NSArray<NSURLRequest *> *requests;
@property (nonatomic, readonly) NSArray<WKFrameInfo *> *frames;
@property (nonatomic, readonly) NSArray<NSString *> *callbacks;
@end

@implementation FrameNavigationDelegate {
    RetainPtr<NSMutableArray<NSURLRequest *>> _requests;
    RetainPtr<NSMutableArray<WKFrameInfo *>> _frames;
    RetainPtr<NSMutableArray<NSString *>> _callbacks;
    size_t _navigationCount;
}

- (void)waitForNavigations:(size_t)expectedNavigationCount
{
    while (_navigationCount < expectedNavigationCount)
        TestWebKitAPI::Util::spinRunLoop();
}

- (NSArray<NSURLRequest *> *)requests
{
    return _requests.get();
}

- (NSArray<WKFrameInfo *> *)frames
{
    return _frames.get();
}

- (NSArray<NSString *> *)callbacks
{
    return _callbacks.get();
}

- (void)clearState
{
    _requests = nullptr;
    _frames = nullptr;
    _callbacks = nullptr;
    _navigationCount = 0;
}

- (void)_webView:(WKWebView *)webView didStartProvisionalLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame
{
    if (!_requests)
        _requests = [NSMutableArray array];
    [_requests addObject:request];

    if (!_frames)
        _frames = [NSMutableArray array];
    [_frames addObject:frame];

    if (!_callbacks)
        _callbacks = [NSMutableArray array];
    [_callbacks addObject:@"start provisional"];
}

- (void)_webView:(WKWebView *)webView didFailProvisionalLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame withError:(NSError *)error
{
    EXPECT_TRUE(frame._errorOccurred);

    [_requests addObject:request];
    [_frames addObject:frame];
    [_callbacks addObject:@"fail provisional"];
    _navigationCount++;
}

- (void)_webView:(WKWebView *)webView didCommitLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame
{
    [_requests addObject:request];
    [_frames addObject:frame];
    [_callbacks addObject:@"commit"];
}

- (void)_webView:(WKWebView *)webView didFailLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame withError:(NSError *)error
{
    EXPECT_TRUE(frame._errorOccurred);

    [_requests addObject:request];
    [_frames addObject:frame];
    [_callbacks addObject:@"fail"];
    _navigationCount++;
}

- (void)_webView:(WKWebView *)webView didFinishLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame
{
    EXPECT_FALSE(frame._errorOccurred);

    [_requests addObject:request];
    [_frames addObject:frame];
    [_callbacks addObject:@"finish"];
    _navigationCount++;
}

@end

TEST(WKNavigation, Frames)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([TestURLSchemeHandler new]);
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *responseString = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"frame://host1/"])
            responseString = @"<iframe src='frame://host2/'></iframe>";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host2/"])
            responseString = @"<script>function navigate() { window.location='frame://host3/' }</script><body onload='navigate()'></body>";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host3/"]) {
            [task didFailWithError:[NSError errorWithDomain:@"testErrorDomain" code:42 userInfo:nil]];
            return;
        } else if ([task.request.URL.absoluteString isEqualToString:@"frame://host4/"])
            responseString = @"<p>Hello World</p>";

        ASSERT(responseString);
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:responseString.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[responseString dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"frame"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([FrameNavigationDelegate new]);
    webView.get().navigationDelegate = delegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"frame://host1/"]]];
    [delegate waitForNavigations:3];

    struct ExpectedStrings {
        const char* callback;
        const char* frameRequest;
        const char* frameSecurityOriginHost;
        const char* request;
    };

    auto checkCallbacks = [delegate] (Vector<ExpectedStrings> expectedVector) {
        NSArray<NSURLRequest *> *requests = delegate.get().requests;
        NSArray<WKFrameInfo *> *frames = delegate.get().frames;
        NSArray<NSString *> *callbacks = delegate.get().callbacks;
        EXPECT_EQ(requests.count, expectedVector.size());
        EXPECT_EQ(frames.count, expectedVector.size());
        EXPECT_EQ(callbacks.count, expectedVector.size());

        auto checkCallback = [] (NSString *callback, WKFrameInfo *frame, NSURLRequest *request, const ExpectedStrings& expected) {
            EXPECT_WK_STREQ(callback, expected.callback);
            EXPECT_WK_STREQ(frame.request.URL.absoluteString, expected.frameRequest);
            EXPECT_WK_STREQ(frame.securityOrigin.host, expected.frameSecurityOriginHost);
            EXPECT_WK_STREQ(request.URL.absoluteString, expected.request);
        };

        for (size_t i = 0; i < expectedVector.size(); ++i)
            checkCallback(callbacks[i], frames[i], requests[i], expectedVector[i]);
    };

    checkCallbacks({
        {
            "start provisional",
            "",
            "",
            "frame://host1/"
        }, {
            "commit",
            "frame://host1/",
            "host1",
            "frame://host1/"
        }, {
            "start provisional",
            "",
            "host1",
            "frame://host2/"
        }, {
            "commit",
            "frame://host2/",
            "host2",
            "frame://host2/"
        }, {
            "finish",
            "frame://host2/",
            "host2",
            "frame://host2/"
        }, {
            "finish",
            "frame://host1/",
            "host1",
            "frame://host1/"
        }, {
            "start provisional",
            "frame://host2/",
            "host2",
            "frame://host3/"
        }, {
            "fail provisional",
            "frame://host2/",
            "host2",
            "frame://host3/"
        }
    });

    // After the failed navigation, perform another successful navigation that will clear the errorOccurred state.
    [delegate clearState];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"frame://host4/"]]];
    [delegate waitForNavigations:1];

    checkCallbacks({
        {
            "start provisional",
            "frame://host1/",
            "host1",
            "frame://host4/"
        }, {
            "commit",
            "frame://host4/",
            "host4",
            "frame://host4/"
        }, {
            "finish",
            "frame://host4/",
            "host4",
            "frame://host4/"
        }
    });
}

@interface DidFailProvisionalNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DidFailProvisionalNavigationDelegate

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    EXPECT_EQ(currentNavigation, navigation);
    EXPECT_NOT_NULL(navigation._request);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    EXPECT_EQ(currentNavigation, navigation);
    EXPECT_NOT_NULL(navigation._request);

    EXPECT_TRUE([error.domain isEqualToString:NSURLErrorDomain]);
    EXPECT_EQ(NSURLErrorUnsupportedURL, error.code);

    isDone = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(WKNavigationActionPolicyAllow);
}

@end

TEST(WKNavigation, DidFailProvisionalNavigation)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    RetainPtr<DidFailProvisionalNavigationDelegate> delegate = adoptNS([[DidFailProvisionalNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"non-existant-scheme://"]];

    currentNavigation = [webView loadRequest:request];
    ASSERT_NOT_NULL(currentNavigation);
    ASSERT_TRUE([[currentNavigation _request] isEqual:request]);

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);
}

@interface CrashReasonDelegate : NSObject <WKNavigationDelegate>
@end

@implementation CrashReasonDelegate

- (void)_webView:(WKWebView *)webView webContentProcessDidTerminateWithReason:(_WKProcessTerminationReason)reason
{
    EXPECT_EQ(reason, _WKProcessTerminationReasonRequestedByClient);
    isDone = true;
}

@end

TEST(WKNavigation, CrashReason)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    
    auto delegate = adoptNS([[CrashReasonDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    
    [webView loadHTMLString:@"<html>start the web process</html>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView _killWebContentProcessAndResetState];
    
    TestWebKitAPI::Util::run(&isDone);
}

@interface DecidePolicyForPageCacheNavigationDelegate : NSObject <WKNavigationDelegate>
@property (nonatomic) BOOL decidedPolicyForBackForwardNavigation;
@end

@implementation DecidePolicyForPageCacheNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (navigationAction.navigationType == WKNavigationTypeBackForward)
        _decidedPolicyForBackForwardNavigation = YES;

    decisionHandler(WKNavigationActionPolicyAllow);
}

@end

TEST(WKNavigation, DecidePolicyForPageCacheNavigation)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    RetainPtr<DecidePolicyForPageCacheNavigationDelegate> delegate = adoptNS([[DecidePolicyForPageCacheNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,1"]];

    isDone = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,2"]];

    isDone = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView goBack];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_TRUE([delegate decidedPolicyForBackForwardNavigation]);
}

@interface NavigationActionHasNavigationDelegate : NSObject <WKNavigationDelegate> {
@public
    WKNavigation *navigation;
}
@end

@implementation NavigationActionHasNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    EXPECT_TRUE(!!navigationAction._mainFrameNavigation);
    EXPECT_EQ(navigationAction._mainFrameNavigation, navigation);
    decisionHandler(WKNavigationActionPolicyAllow);
}

@end

TEST(WKNavigation, NavigationActionHasNavigation)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    RetainPtr<NavigationActionHasNavigationDelegate> delegate = adoptNS([[NavigationActionHasNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,1"]];

    isDone = false;
    delegate->navigation = [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);
}

@interface ClientRedirectNavigationDelegate : NSObject<WKNavigationDelegatePrivate>
@end

@implementation ClientRedirectNavigationDelegate
- (void)_webView:(WKWebView *)webView willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)delay
{
    redirectURL = URL;
    redirectDelay = delay;
}
- (void)_webViewDidCancelClientRedirect:(WKWebView *)webView
{
    didCancelRedirect = true;
}
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}
@end

TEST(WKNavigation, WebViewWillPerformClientRedirect)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._allowTopNavigationToDataURLs = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[ClientRedirectNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,%3Cmeta%20http-equiv=%22refresh%22%20content=%22123;URL=data:text/html,Page1%22%3E"]];

    isDone = false;
    redirectURL = nil;
    redirectDelay = 0;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_DOUBLE_EQ(redirectDelay, 123);
    ASSERT_STREQ(redirectURL.get().absoluteString.UTF8String, "data:text/html,Page1");

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,%3Cscript%3Ewindow.location=%22data:text/html,Page2%22;%3C/script%3E"]];
    isDone = false;
    redirectURL = nil;
    redirectDelay = NSTimeIntervalSince1970; // Use any non-zero value, we will test that the delegate receives a delay of 0.
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_DOUBLE_EQ(redirectDelay, 0);
    ASSERT_STREQ(redirectURL.get().absoluteString.UTF8String, "data:text/html,Page2");
}

TEST(WKNavigation, WebViewDidCancelClientRedirect)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._allowTopNavigationToDataURLs = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[ClientRedirectNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    // Test 1: During a navigation that is not a client redirect, -_webViewDidCancelClientRedirect: should not be called.

    auto request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,Page1"]];

    isDone = false;
    didCancelRedirect = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_FALSE(didCancelRedirect);

    // Test 2: When a client redirect does happen, -_webViewDidCancelClientRedirect: should still be called. It essentially
    // is called whenever the web view transitions from "expecting a redirect" to "not expecting a redirect".

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,%3Cscript%3Ewindow.location=%22data:text/html,Page2%22;%3C/script%3E"]];
    isDone = false;
    didCancelRedirect = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_FALSE(didCancelRedirect);

    isDone = false;
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_TRUE(didCancelRedirect);

    // Test 3: When another navigation begins while a client redirect is scheduled, -_webViewDidCancelClientRedirect:
    // should be called.

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,%3Cmeta%20http-equiv=%22refresh%22%20content=%2210000;URL=data:text/html,Page3%22%3E"]];

    isDone = false;
    didCancelRedirect = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_FALSE(didCancelRedirect);

    request = [NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,Page4"]];
    isDone = false;
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&isDone);

    ASSERT_TRUE(didCancelRedirect);
}

@interface NavigationActionSPIDelegate : NSObject <WKNavigationDelegate> {
@public
    BOOL _spiCalled;
}
- (BOOL)spiCalled;
@end

@implementation NavigationActionSPIDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

-(void)_webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences userInfo:(id <NSSecureCoding>)userInfo decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler {
    _spiCalled = TRUE;
    decisionHandler(WKNavigationActionPolicyAllow, preferences);
}

- (BOOL)spiCalled
{
    return _spiCalled;
}

@end

TEST(WKNavigation, NavigationActionSPI)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto delegate = adoptNS([[NavigationActionSPIDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"data:text/html,1"]]];
    TestWebKitAPI::Util::run(&isDone);
    EXPECT_TRUE([delegate spiCalled]);
}

static bool navigationComplete;

@interface BackForwardDelegate : NSObject<WKNavigationDelegatePrivate>
@end
@implementation BackForwardDelegate
- (void)_webView:(WKWebView *)webView willGoToBackForwardListItem:(WKBackForwardListItem *)item inPageCache:(BOOL)inPageCache
{
    const char* expectedURL = [[[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"] absoluteString] UTF8String];
    EXPECT_STREQ(item.URL.absoluteString.UTF8String, expectedURL);
    EXPECT_TRUE(item.title == nil);
    EXPECT_STREQ(item.initialURL.absoluteString.UTF8String, expectedURL);
    EXPECT_TRUE(inPageCache);
    isDone = true;
}
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    navigationComplete = true;
}
@end

TEST(WKNavigation, WillGoToBackForwardListItem)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[BackForwardDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&navigationComplete);
    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&navigationComplete);
    [webView goBack];
    TestWebKitAPI::Util::run(&isDone);
}

#if PLATFORM(MAC)

RetainPtr<WKBackForwardListItem> firstItem;
RetainPtr<WKBackForwardListItem> secondItem;

@interface ListItemDelegate : NSObject<WKNavigationDelegatePrivate>
@end
@implementation ListItemDelegate
- (void)_webView:(WKWebView *)webView backForwardListItemAdded:(WKBackForwardListItem *)itemAdded removed:(NSArray<WKBackForwardListItem *> *)itemsRemoved
{
    NSString *firstURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"].absoluteString;
    NSString *secondURL = [[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"].absoluteString;

    if (!firstItem) {
        EXPECT_NULL(firstItem);
        EXPECT_NULL(secondItem);
        EXPECT_NULL(itemsRemoved);
        EXPECT_NOT_NULL(itemAdded);
        EXPECT_STREQ(firstURL.UTF8String, itemAdded.URL.absoluteString.UTF8String);
        firstItem = itemAdded;
    } else if (!secondItem) {
        EXPECT_NOT_NULL(firstItem);
        EXPECT_NULL(secondItem);
        EXPECT_NULL(itemsRemoved);
        EXPECT_NOT_NULL(itemAdded);
        EXPECT_STREQ(secondURL.UTF8String, itemAdded.URL.absoluteString.UTF8String);
        secondItem = itemAdded;
    } else {
        EXPECT_NOT_NULL(firstItem);
        EXPECT_NOT_NULL(secondItem);
        EXPECT_NOT_NULL(itemsRemoved);
        EXPECT_NULL(itemAdded);
        EXPECT_EQ([itemsRemoved count], 2u);
        EXPECT_STREQ(firstURL.UTF8String, [itemsRemoved objectAtIndex:0].URL.absoluteString.UTF8String);
        EXPECT_STREQ(secondURL.UTF8String, [itemsRemoved objectAtIndex:1].URL.absoluteString.UTF8String);
        isDone = true;
    }
}
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    navigationComplete = true;
}
@end

TEST(WKNavigation, ListItemAddedRemoved)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[ListItemDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&navigationComplete);
    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];
    TestWebKitAPI::Util::run(&navigationComplete);
    [[webView backForwardList] _removeAllItems];
    TestWebKitAPI::Util::run(&isDone);
}

#endif // PLATFORM(MAC)

@interface LoadingObserver : NSObject
@property (nonatomic, readonly) size_t changesObserved;
@end

@implementation LoadingObserver {
    size_t _changesObserved;
}

- (size_t)changesObserved
{
    return _changesObserved;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    EXPECT_WK_STREQ(keyPath, "loading");
    _changesObserved++;
}

@end

TEST(WKNavigation, FrameBackLoading)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { "<iframe src='frame1.html'></iframe>"_s } },
        { "/frame1.html"_s, { "<a href='frame2.html'>link</a>"_s } },
        { "/frame2.html"_s, { "<script>alert('frame2 loaded')</script>"_s } },
    });
    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestUIDelegate new]);
    auto observer = adoptNS([LoadingObserver new]);
    [webView setUIDelegate:delegate.get()];
    [webView addObserver:observer.get() forKeyPath:@"loading" options:NSKeyValueObservingOptionNew context:nil];
    EXPECT_FALSE([webView isLoading]);
    EXPECT_EQ([observer changesObserved], 0u);
    [webView loadRequest:server.request()];
    EXPECT_TRUE([webView isLoading]);
    EXPECT_EQ([observer changesObserved], 1u);
    while ([observer changesObserved] < 2u)
        Util::spinRunLoop();
    EXPECT_FALSE([webView isLoading]);
    EXPECT_EQ([observer changesObserved], 2u);
    EXPECT_FALSE([webView canGoBack]);
    [webView evaluateJavaScript:@"document.querySelector('iframe').contentWindow.document.querySelector('a').click()" completionHandler:nil];
    EXPECT_WK_STREQ([delegate waitForAlert], "frame2 loaded");
    EXPECT_EQ([observer changesObserved], 2u);
    EXPECT_TRUE([webView canGoBack]);
    [webView goBack];
    while ([observer changesObserved] < 3)
        Util::spinRunLoop();
    EXPECT_TRUE([webView isLoading]);
    while ([observer changesObserved] < 4)
        Util::spinRunLoop();
    EXPECT_FALSE([webView isLoading]);
    [webView removeObserver:observer.get() forKeyPath:@"loading"];

}

TEST(WKNavigation, SimultaneousNavigationWithFontsFinishes)
{
    constexpr auto mainHTML =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<style>"
    "@font-face {"
    "    font-family: 'WebFont';"
    "    src: url('Ahem.svg') format('svg');"
    "}"
    "</style>"
    "<script src='scriptsrc.js'></script>"
    "</head>"
    "<body>"
    "<span style=\"font: 100px 'WebFont';\">text</span>"
    "<iframe src='iframesrc.html'></iframe>"
    "<script>window.location='refresh-nav:///'</script>"
    "</body>"
    "</html>"_s;

    NSString *svg = [NSString stringWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"AllAhem" withExtension:@"svg" subdirectory:@"TestWebKitAPI.resources"] encoding:NSUTF8StringEncoding error:nil];

    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { mainHTML } },
        { "/Ahem.svg"_s, { svg } },
        { "/scriptsrc.js"_s, { "/* js content */"_s } },
        { "/iframesrc.html"_s, { "frame content"_s } },
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.scheme isEqualToString:@"refresh-nav"])
            completionHandler(WKNavigationActionPolicyCancel);
        else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView loadRequest:server.request()];
    Util::run(&finishedNavigation);
}

TEST(WKNavigation, LoadRadarURLFromSandboxedFrameAllowPopups)
{
    constexpr auto mainHTML = "<iframe src='frame.html' sandbox='allow-scripts allow-popups'></iframe>"_s;
    constexpr auto frameHTML = "<a id='testLink' href='rdar://84498192'>Link</a><script>setTimeout(() => { document.getElementById('testLink').click() }, 0);</script>"_s;

    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { mainHTML } },
        { "/frame.html"_s, { frameHTML } },
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool didTryToLoadRadarURL = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.scheme isEqualToString:@"rdar"]) {
            didTryToLoadRadarURL = true;
            completionHandler(WKNavigationActionPolicyCancel);
        } else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView loadRequest:server.request()];
    Util::run(&finishedNavigation);

    Util::run(&didTryToLoadRadarURL);
}

TEST(WKNavigation, LoadRadarURLFromSandboxedFrameAllowTopNavigation)
{
    constexpr auto mainHTML = "<iframe src='frame.html' sandbox='allow-scripts allow-top-navigation'></iframe>"_s;
    constexpr auto frameHTML = "<a id='testLink' href='rdar://84498192'>Link</a><script>setTimeout(() => { document.getElementById('testLink').click() }, 0);</script>"_s;

    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { mainHTML } },
        { "/frame.html"_s, { frameHTML } },
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool didTryToLoadRadarURL = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.scheme isEqualToString:@"rdar"]) {
            didTryToLoadRadarURL = true;
            completionHandler(WKNavigationActionPolicyCancel);
        } else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView loadRequest:server.request()];
    Util::run(&finishedNavigation);

    Util::run(&didTryToLoadRadarURL);
}

TEST(WKNavigation, LoadRadarURLFromSandboxedFrameAllowCustomProtocolsNavigation)
{
    constexpr auto mainHTML = "<iframe src='frame.html' sandbox='allow-scripts allow-top-navigation-to-custom-protocols'></iframe>"_s;
    constexpr auto frameHTML = "<a id='testLink' href='rdar://84498192'>Link</a><script>setTimeout(() => { document.getElementById('testLink').click() }, 0);</script>"_s;

    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { mainHTML } },
        { "/frame.html"_s, { frameHTML } },
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool didTryToLoadRadarURL = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.scheme isEqualToString:@"rdar"]) {
            didTryToLoadRadarURL = true;
            completionHandler(WKNavigationActionPolicyCancel);
        } else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView loadRequest:server.request()];
    Util::run(&finishedNavigation);

    Util::run(&didTryToLoadRadarURL);
}

TEST(WKNavigation, LoadRadarURLFromSandboxedFrameWithUserGesture)
{
    constexpr auto mainHTML = "<iframe src='frame.html' sandbox='allow-scripts allow-top-navigation-by-user-activation'></iframe>"_s;
    constexpr auto frameHTML = "<a id='testLink' href='rdar://84498192'>Link</a></script>"_s;

    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { mainHTML } },
        { "/frame.html"_s, { frameHTML } },
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block RetainPtr<WKFrameInfo> iframe;
    __block bool didTryToLoadRadarURL = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if (!action.targetFrame.isMainFrame)
            iframe = action.targetFrame;
        if ([action.request.URL.scheme isEqualToString:@"rdar"]) {
            didTryToLoadRadarURL = true;
            completionHandler(WKNavigationActionPolicyCancel);
        } else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView loadRequest:server.request()];
    Util::run(&finishedNavigation);

    ASSERT_TRUE(!!iframe);

    // Running Javascript simulates a user gesture so the navigation should be permitted due to 'allow-top-navigation-by-user-activation'.
    [webView evaluateJavaScript:@"document.getElementById('testLink').click()" inFrame:iframe.get() inContentWorld:WKContentWorld.pageWorld completionHandler:nil];

    Util::run(&didTryToLoadRadarURL);
}

TEST(WKNavigation, LoadRadarURLFromSandboxedFrame)
{
    constexpr auto mainHTML = "<iframe src='frame.html' sandbox='allow-scripts'></iframe>"_s;
    constexpr auto frameHTML = "<a id='testLink' href='rdar://84498192'>Link</a><script>setTimeout(() => { document.getElementById('testLink').click() }, 0);</script>"_s;

    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { mainHTML } },
        { "/frame.html"_s, { frameHTML } },
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool didTryToLoadRadarURL = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.scheme isEqualToString:@"rdar"]) {
            didTryToLoadRadarURL = true;
            completionHandler(WKNavigationActionPolicyCancel);
        } else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView loadRequest:server.request()];
    Util::run(&finishedNavigation);

    Util::runFor(0.5_s);

    EXPECT_FALSE(didTryToLoadRadarURL);
}

TEST(WKNavigation, LoadRadarURLFromSandboxedFrameMissingUserGesture)
{
    constexpr auto mainHTML = "<iframe src='frame.html' sandbox='allow-scripts allow-top-navigation-by-user-activation'></iframe>"_s;
    constexpr auto frameHTML = "<a id='testLink' href='rdar://84498192'>Link</a><script>setTimeout(() => { document.getElementById('testLink').click() }, 0);</script>"_s;

    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/"_s, { mainHTML } },
        { "/frame.html"_s, { frameHTML } },
    });

    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool didTryToLoadRadarURL = false;
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.scheme isEqualToString:@"rdar"]) {
            didTryToLoadRadarURL = true;
            completionHandler(WKNavigationActionPolicyCancel);
        } else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    __block bool finishedNavigation = false;
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedNavigation = true;
    };

    [webView loadRequest:server.request()];
    Util::run(&finishedNavigation);

    Util::runFor(0.5_s);

    EXPECT_FALSE(didTryToLoadRadarURL);
}

TEST(WKNavigation, CrossOriginCOOPCancelResponseFailProvisionalNavigationCallback)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/path1"_s, { "hi"_s } },
        { "/path2"_s, { "hi"_s } },
        { "/path3"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block Vector<bool> finishedSuccessfullyCallbacks;
    auto loadWithResponsePolicy = ^(WKWebView *webView, NSString *url, WKNavigationResponsePolicy responsePolicy) {
        auto callbacksSizeBefore = finishedSuccessfullyCallbacks.size();

        auto delegate = adoptNS([TestNavigationDelegate new]);
        delegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *response, void (^decisionHandler)(WKNavigationResponsePolicy)) {
            decisionHandler(responsePolicy);
        };

        delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        };
        delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *) {
            finishedSuccessfullyCallbacks.append(false);
        };
        delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
            finishedSuccessfullyCallbacks.append(true);
        };
        [webView setNavigationDelegate:delegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
        while (finishedSuccessfullyCallbacks.size() == callbacksSizeBefore)
            TestWebKitAPI::Util::spinRunLoop(10);
    };

    loadWithResponsePolicy(webView.get(), @"https://webkit.org/path1", WKNavigationResponsePolicyAllow);
    loadWithResponsePolicy(webView.get(), @"https://webkit.org/path2", WKNavigationResponsePolicyCancel);
    loadWithResponsePolicy(webView.get(), @"https://example.com/path3", WKNavigationResponsePolicyCancel);

    Vector<bool> expectedCallbacks { true, false, false };
    EXPECT_EQ(finishedSuccessfullyCallbacks, expectedCallbacks);
}

TEST(WKNavigation, HTTPSFirstHTTPDowngrade)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/secure"_s, { { }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    HTTPServer httpServer({
        { "http://site.example/secure"_s, { "Welcome"_s } }
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSFirst;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];
    while (!finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 2);

    [webView waitForNextPresentationUpdate];

    __block bool doneEvaluatingJavaScript { false };
    [webView evaluateJavaScript:@"window.location.protocol" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http:", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location.toString()" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        auto url = [NSURL URLWithString:(NSString *)value];
        EXPECT_WK_STREQ(@"http", url.scheme);
        EXPECT_WK_STREQ(@"http://site.example/secure", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.hasOwnProperty('crypto')" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value boolValue]);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.crypto.hasOwnProperty('subtle')" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_FALSE([value boolValue]);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://site.example/secure", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ(@"http://site.example/secure", [webView _mainFrameURL].absoluteString);
}

TEST(WKNavigation, HTTPSFirstHTTPDowngradeRedirect)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/secure"_s, { { }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    HTTPServer httpServer({
        { "http://site.example/secure"_s, { 302, {{ "Location"_s, "https://site.example/secure"_s }}, "redirecting..."_s } }
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSFirst;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];
    while (!errorCode)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, NSURLErrorServerCertificateUntrusted);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 3);
}

TEST(WKNavigation, HTTPSFirstRedirectNoHTTPDowngradeRedirect)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/redirect"_s, { 302, {{ "Location"_s, "https://site2.example/page1"_s }}, "redirecting..."_s } },
        { "/page1"_s, { { }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    HTTPServer httpServer({
        { "http://site.example/redirect"_s, { 302, {{ "Location"_s, "https://site.example"_s }}, "redirecting..."_s } }
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSFirst;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    auto delegate = adoptNS([TestNavigationDelegate new]);

    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        if (!loadCount)
            completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        else
            completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
    };

    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/redirect"]]];
    while (!errorCode)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, NSURLErrorServerCertificateUntrusted);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 2);
}

TEST(WKNavigation, HTTPSOnlyInitialLoad)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/secure"_s, { { }, "secure page"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    HTTPServer httpServer({
        { "http://site.example/notsecure"_s, { { }, "not secure page"_s } },
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    __block bool didReceiveAuthenticationChallenge { false };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        didReceiveAuthenticationChallenge = true;
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NOT_NULL(error);
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site.example/notsecure"]]];

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, _WKErrorCodeHTTPNavigationWithHTTPSOnly);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_FALSE(didReceiveAuthenticationChallenge);
    EXPECT_EQ(loadCount, 1);

    [webView waitForNextPresentationUpdate];
    EXPECT_WK_STREQ(@"", [webView _mainFrameURL].absoluteString);
}

TEST(WKNavigation, HTTPSOnlyHTTPFallback)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/secure"_s, { { }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NOT_NULL(error);
        EXPECT_NOT_NULL(error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_WK_STREQ(@"HTTPSOnlyHTTPFallback", error.userInfo[@"errorRecoveryMethod"]);
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];

    while (!errorCode)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, NSURLErrorServerCertificateUntrusted);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 1);

    [webView waitForNextPresentationUpdate];

    __block bool doneEvaluatingJavaScript { false };
    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"about:blank", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ(@"", [webView _mainFrameURL].absoluteString);
}

TEST(WKNavigation, HTTPSOnlyHTTPFallbackBypassEnabledCertificateError)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/secure"_s, { { }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly | _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnlyExplicitlyBypassedForDomain;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NOT_NULL(error);
        EXPECT_NULL(error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_WK_STREQ(@"", error.userInfo[@"errorRecoveryMethod"]);
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];

    while (!errorCode)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, NSURLErrorServerCertificateUntrusted);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 1);

    [webView waitForNextPresentationUpdate];

    __block bool doneEvaluatingJavaScript { false };
    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"about:blank", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ(@"", [webView _mainFrameURL].absoluteString);
}

TEST(WKNavigation, HTTPSOnlyWithSameSiteBypass)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/secure"_s, { { }, "hi"_s } },
        { "/secure2"_s, { { }, "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    HTTPServer httpServer({
        { "http://site.example/secure"_s, { { }, "hi: not secure"_s } },
        { "http://www2.site.example/secure"_s, { { }, "hi: not secure"_s } },
        { "http://site2.example/secure"_s, { { }, "hi: not secure"_s } }
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    // Step 1: Attempt https load without implementing didReceiveAuthenticationChallenge
    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NOT_NULL(error);
        EXPECT_NOT_NULL(error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_WK_STREQ(@"HTTPSOnlyHTTPFallback", error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_NOT_NULL(error.userInfo[@"NSErrorFailingURLKey"]);
        EXPECT_WK_STREQ(@"https://site.example/secure", error.userInfo[@"NSErrorFailingURLStringKey"]);
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];

    while (!errorCode)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, NSURLErrorServerCertificateUntrusted);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 1);

    // Step 2: Attempt http load with HTTPS-bypass enabled
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy |= _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnlyExplicitlyBypassedForDomain;

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NULL(error);
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };

    errorCode  = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://site.example/secure"]]];

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 1);

    __block bool doneEvaluatingJavaScript { false };
    [webView evaluateJavaScript:@"window.location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://site.example/secure", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ(@"http://site.example/secure", [webView _mainFrameURL].absoluteString);

    // Step 3: Attempt http load with same-site HTTPS-bypass enabled
    errorCode  = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location = \"http://www2.site.example/secure\"" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://www2.site.example/secure", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 1);

    [webView waitForNextPresentationUpdate];

    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://www2.site.example/secure", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ(@"http://www2.site.example/secure", [webView _mainFrameURL].absoluteString);

    // Step 4: Attempt cross-site http load with HTTPS-bypass enabled
    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NOT_NULL(error);
        EXPECT_NOT_NULL(error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_WK_STREQ(@"HTTPSOnlyHTTPFallback", error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_NOT_NULL(error.userInfo[@"NSErrorFailingURLKey"]);
        EXPECT_WK_STREQ(@"https://site2.example/secure", error.userInfo[@"NSErrorFailingURLStringKey"]);
        errorCode = error.code;
    };

    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy &= ~_WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnlyExplicitlyBypassedForDomain;

    errorCode  = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location = \"http://site2.example/secure\"" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://site2.example/secure", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, NSURLErrorServerCertificateUntrusted);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 1);

    [webView waitForNextPresentationUpdate];

    doneEvaluatingJavaScript = false;
    [webView evaluateJavaScript:@"window.location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://www2.site.example/secure", (NSString *)value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    EXPECT_WK_STREQ(@"http://www2.site.example/secure", [webView _mainFrameURL].absoluteString);
}

TEST(WKNavigation, HTTPSOnlyWithHTTPRedirect)
{
    using namespace TestWebKitAPI;
    HTTPServer httpsServer({
        { "/secure"_s, { 302, {{ "Location"_s, "http://site.example/secure"_s }}, "redirecting..."_s } },
        { "/secure2"_s, { 302, {{ "Location"_s, "http://site2.example/secure3"_s }}, "redirecting..."_s } },
        { "/secure3"_s, { 302, {{ "Location"_s, "http://site2.example/secure2"_s }}, "redirecting..."_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    HTTPServer httpServer({
        { "http://site.example/secure"_s, { { }, "hi: not secure"_s } },
        { "http://site2.example/secure2"_s, { { }, "hi: not secure"_s } },
        { "http://site2.example/secure3"_s, { { }, "hi: not secure"_s } },
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port()),
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(httpsServer.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block int errorCode { 0 };
    __block bool finishedSuccessfully { false };
    __block int loadCount { 0 };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        ++loadCount;
        completionHandler(WKNavigationActionPolicyAllow);
    };
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NOT_NULL(error);
        EXPECT_NOT_NULL(error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_WK_STREQ(@"HTTPSOnlyHTTPFallback", error.userInfo[@"errorRecoveryMethod"]);
        EXPECT_NOT_NULL(error.userInfo[@"NSErrorFailingURLKey"]);
        EXPECT_WK_STREQ(@"https://site.example/secure", error.userInfo[@"NSErrorFailingURLStringKey"]);
        errorCode = error.code;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, _WKErrorCodeHTTPSUpgradeRedirectLoop);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 2);

    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NOT_NULL(error);
        EXPECT_NOT_NULL(error.userInfo[@"NSErrorFailingURLKey"]);
        EXPECT_WK_STREQ(@"https://site2.example/secure2", error.userInfo[@"NSErrorFailingURLStringKey"]);
        errorCode = error.code;
    };

    errorCode = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure2"]]];

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(errorCode, kCFURLErrorHTTPTooManyRedirects);
    EXPECT_FALSE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 21);

    configuration.get().defaultWebpagePreferences._networkConnectionIntegrityPolicy = _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnly | _WKWebsiteNetworkConnectionIntegrityPolicyHTTPSOnlyExplicitlyBypassedForDomain;
    errorCode = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        EXPECT_NULL(error);
        if (error)
            errorCode = error.code;
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure"]]];

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 2);
    EXPECT_WK_STREQ(@"http://site.example/secure", [webView _mainFrameURL].absoluteString);

    errorCode = 0;
    finishedSuccessfully = false;
    loadCount = 0;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://site.example/secure2"]]];

    while (!errorCode && !finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_EQ(errorCode, 0);
    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_EQ(loadCount, 2);
    EXPECT_WK_STREQ(@"http://site2.example/secure3", [webView _mainFrameURL].absoluteString);
}

TEST(WKNavigation, LeakCheck)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "/example"_s, { "hi"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [storeConfiguration setAllowsHSTSWithUntrustedRootCertificate:YES];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    __block __weak WKNavigation *gLastNavigation = nil;

    __block bool done = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    delegate.get().didStartProvisionalNavigation = ^(WKWebView *, WKNavigation *navigation) {
        gLastNavigation = navigation;
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        EXPECT_EQ(gLastNavigation, navigation);
        done = true;
    };
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    __weak WKNavigation *navigationToExample;
    @autoreleasepool {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        Util::run(&done);
        navigationToExample = gLastNavigation;

        auto examplePID = [webView _webProcessIdentifier];

        done = false;
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
        Util::run(&done);

        EXPECT_NE(examplePID, [webView _webProcessIdentifier]);
        EXPECT_NE(gLastNavigation, navigationToExample);

        [webView _clearBackForwardCache];
    }

    while (navigationToExample)
        Util::spinRunLoop();
}

TEST(WKNavigation, Multiple303Redirects)
{
    using namespace TestWebKitAPI;
    HTTPServer httpServer({
        { "http://site.example/page1"_s, { 303, {{ "Location"_s, "http://site.example/page2"_s }}, "see other..."_s } },
        { "http://site.example/page2"_s, { 303, {{ "Location"_s, "http://site.example/page3"_s }}, "see other..."_s } },
        { "http://site.example/page3"_s, { "Done."_s  } },
    });

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(httpServer.port())
    }];

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    __block bool finishedSuccessfully { false };
    __block bool reachedPage3 { false };
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.path isEqual:@"/page1"]) {
            EXPECT_WK_STREQ(action.request.URL.path, @"/page1");
            EXPECT_WK_STREQ(action.request.HTTPMethod, @"POST");
            EXPECT_WK_STREQ(action.request.allHTTPHeaderFields[@"Content-Type"], @"text/plain");
        } else if ([action.request.URL.path isEqual:@"/page2"]) {
            EXPECT_WK_STREQ(action.request.HTTPMethod, @"GET");
            EXPECT_NULL(action.request.allHTTPHeaderFields[@"Content-Type"]);
        } else if ([action.request.URL.path isEqual:@"/page3"]) {
            EXPECT_WK_STREQ(action.request.HTTPMethod, @"GET");
            EXPECT_NULL(action.request.allHTTPHeaderFields[@"Content-Type"]);
            reachedPage3 = true;
        } else
            EXPECT_TRUE(0);
        completionHandler(WKNavigationActionPolicyAllow);
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finishedSuccessfully = true;
    };

    [webView setNavigationDelegate:delegate.get()];
    auto request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"http://site.example/page1"]];
    request.HTTPMethod = @"POST";
    [request setValue:@"text/plain" forHTTPHeaderField:@"Content-Type"];
    [request setValue:@"0" forHTTPHeaderField:@"Content-Length"];
    [webView loadRequest:request];

    while (!finishedSuccessfully)
        TestWebKitAPI::Util::spinRunLoop(5);

    EXPECT_TRUE(finishedSuccessfully);
    EXPECT_TRUE(reachedPage3);
}

TEST(WKNavigation, NavigationToUnknownBlankURL)
{
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];

    __block bool navigationFailed = false;
    delegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *navigation, NSError *error) {
        navigationFailed = true;
        done = true;
    };

    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        done = true;
    };

    done = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:srcdoc"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank#foo"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank?foo=bar"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:srcdoc#foo"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:srcdoc?foo=bar"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:foo"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:google.com"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(navigationFailed);

    done = false;
    navigationFailed = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about://newtab/"]]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(navigationFailed);
}
