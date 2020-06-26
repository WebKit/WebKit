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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import <WebKit/WKBackForwardListPrivate.h>
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

static bool isDone;
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

    NavigationDelegate *delegate = [[NavigationDelegate alloc] init];
    [webView setNavigationDelegate:delegate];

    @autoreleasepool {
        EXPECT_EQ(delegate, [webView navigationDelegate]);
    }

    [delegate release];
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

#if HAVE(NETWORK_FRAMEWORK)
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
#endif

@interface FrameNavigationDelegate : NSObject <WKNavigationDelegate>
- (void)waitForNavigations:(size_t)count;
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
    [_requests addObject:request];
    [_frames addObject:frame];
    [_callbacks addObject:@"fail"];
    _navigationCount++;
}

- (void)_webView:(WKWebView *)webView didFinishLoadWithRequest:(NSURLRequest *)request inFrame:(WKFrameInfo *)frame
{
    [_requests addObject:request];
    [_frames addObject:frame];
    [_callbacks addObject:@"finish"];
    _navigationCount++;
}

@end

TEST(WKNavigation, Frames)
{
    WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
    TestURLSchemeHandler *handler = [[TestURLSchemeHandler new] autorelease];
    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSString *responseString = nil;
        if ([task.request.URL.absoluteString isEqualToString:@"frame://host1/"])
            responseString = @"<iframe src='frame://host2/'></iframe>";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host2/"])
            responseString = @"<script>function navigate() { window.location='frame://host3/' }</script><body onload='navigate()'></body>";
        else if ([task.request.URL.absoluteString isEqualToString:@"frame://host3/"]) {
            [task didFailWithError:[NSError errorWithDomain:@"testErrorDomain" code:42 userInfo:nil]];
            return;
        }

        ASSERT(responseString);
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:responseString.length textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[responseString dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:handler forURLScheme:@"frame"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
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

#if HAVE(NETWORK_FRAMEWORK)

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
        { "/", { "<iframe src='frame1.html'></iframe>" } },
        { "/frame1.html", { "<a href='frame2.html'>link</a>" } },
        { "/frame2.html", { "<script>alert('frame2 loaded')</script>" } },
    });
    auto webView = [[WKWebView new] autorelease];
    auto delegate = [[TestUIDelegate new] autorelease];
    auto observer = [[LoadingObserver new] autorelease];
    webView.UIDelegate = delegate;
    [webView addObserver:observer forKeyPath:@"loading" options:NSKeyValueObservingOptionNew context:nil];
    EXPECT_FALSE(webView.loading);
    EXPECT_EQ(observer.changesObserved, 0u);
    [webView loadRequest:server.request()];
    EXPECT_TRUE(webView.loading);
    EXPECT_EQ(observer.changesObserved, 1u);
    while (observer.changesObserved < 2u)
        Util::spinRunLoop();
    EXPECT_FALSE(webView.loading);
    EXPECT_EQ(observer.changesObserved, 2u);
    EXPECT_FALSE(webView.canGoBack);
    [webView evaluateJavaScript:@"document.querySelector('iframe').contentWindow.document.querySelector('a').click()" completionHandler:nil];
    EXPECT_WK_STREQ([delegate waitForAlert], "frame2 loaded");
    EXPECT_EQ(observer.changesObserved, 2u);
    EXPECT_TRUE(webView.canGoBack);
    [webView goBack];
    while (observer.changesObserved < 3)
        Util::spinRunLoop();
    EXPECT_TRUE(webView.loading);
    while (observer.changesObserved < 4)
        Util::spinRunLoop();
    EXPECT_FALSE(webView.loading);
    [webView removeObserver:observer forKeyPath:@"loading"];

}

#endif // HAVE(NETWORK_FRAMEWORK)
