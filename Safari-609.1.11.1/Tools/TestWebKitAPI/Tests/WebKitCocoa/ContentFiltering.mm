/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#import "ContentFiltering.h"
#import "MockContentFilterSettings.h"
#import "PlatformUtilities.h"
#import "TestProtocol.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKErrorRef.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKDownloadDelegate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>

using Decision = WebCore::MockContentFilterSettings::Decision;
using DecisionPoint = WebCore::MockContentFilterSettings::DecisionPoint;

static bool isDone;

@interface MockContentFilterEnabler : NSObject <NSCopying, NSSecureCoding>
- (instancetype)initWithDecision:(Decision)decision decisionPoint:(DecisionPoint)decisionPoint;
@end

@implementation MockContentFilterEnabler {
    Decision _decision;
    DecisionPoint _decisionPoint;
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    return [super init];
}

- (instancetype)initWithDecision:(Decision)decision decisionPoint:(DecisionPoint)decisionPoint
{
    if (!(self = [super init]))
        return nil;

    _decision = decision;
    _decisionPoint = decisionPoint;
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeInt:static_cast<int>(_decision) forKey:@"Decision"];
    [coder encodeInt:static_cast<int>(_decisionPoint) forKey:@"DecisionPoint"];
}

@end

static RetainPtr<WKWebViewConfiguration> configurationWithContentFilterSettings(Decision decision, DecisionPoint decisionPoint)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ContentFilteringPlugIn"]);
    auto contentFilterEnabler = adoptNS([[MockContentFilterEnabler alloc] initWithDecision:decision decisionPoint:decisionPoint]);
    [[configuration processPool] _setObject:contentFilterEnabler.get() forBundleParameter:NSStringFromClass([MockContentFilterEnabler class])];
    return configuration;
}

@interface ServerRedirectNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation ServerRedirectNavigationDelegate

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    EXPECT_WK_STREQ(webView.URL.absoluteString, @"http://redirect/?pass");
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    EXPECT_WK_STREQ(webView.URL.absoluteString, @"http://pass/");
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

TEST(ContentFiltering, URLAfterServerRedirect)
{
    @autoreleasepool {
        [TestProtocol registerWithScheme:@"http"];

        auto configuration = configurationWithContentFilterSettings(Decision::Allow, DecisionPoint::AfterAddData);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[ServerRedirectNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://redirect?pass"]]];
        TestWebKitAPI::Util::run(&isDone);

        [TestProtocol unregister];
    }
}

@interface BecomeDownloadDelegate : NSObject <WKNavigationDelegate>
@end

@implementation BecomeDownloadDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(_WKNavigationResponsePolicyBecomeDownload);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    isDone = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

static bool downloadDidStart;

@interface ContentFilteringDownloadDelegate : NSObject <_WKDownloadDelegate>
@end

@implementation ContentFilteringDownloadDelegate

- (void)_downloadDidStart:(_WKDownload *)download
{
    downloadDidStart = true;
}

@end

static void downloadTest(Decision decision, DecisionPoint decisionPoint)
{
    @autoreleasepool {
        [TestProtocol registerWithScheme:@"http"];

        auto configuration = configurationWithContentFilterSettings(decision, decisionPoint);
        auto downloadDelegate = adoptNS([[ContentFilteringDownloadDelegate alloc] init]);
        [[configuration processPool] _setDownloadDelegate:downloadDelegate.get()];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[BecomeDownloadDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://redirect/?download"]]];

        isDone = false;
        downloadDidStart = false;
        const bool downloadShouldStart = decision == Decision::Allow || decisionPoint > DecisionPoint::AfterResponse;
        if (downloadShouldStart)
            TestWebKitAPI::Util::run(&downloadDidStart);
        else
            TestWebKitAPI::Util::run(&isDone);

        EXPECT_EQ(downloadShouldStart, downloadDidStart);

        [TestProtocol unregister];
    }
}

TEST(ContentFiltering, AllowDownloadAfterWillSendRequest)
{
    downloadTest(Decision::Allow, DecisionPoint::AfterWillSendRequest);
}

TEST(ContentFiltering, BlockDownloadAfterWillSendRequest)
{
    downloadTest(Decision::Block, DecisionPoint::AfterWillSendRequest);
}

TEST(ContentFiltering, AllowDownloadAfterRedirect)
{
    downloadTest(Decision::Allow, DecisionPoint::AfterRedirect);
}

TEST(ContentFiltering, BlockDownloadAfterRedirect)
{
    downloadTest(Decision::Block, DecisionPoint::AfterRedirect);
}

TEST(ContentFiltering, AllowDownloadAfterResponse)
{
    downloadTest(Decision::Allow, DecisionPoint::AfterResponse);
}

TEST(ContentFiltering, BlockDownloadAfterResponse)
{
    downloadTest(Decision::Block, DecisionPoint::AfterResponse);
}

TEST(ContentFiltering, AllowDownloadAfterAddData)
{
    downloadTest(Decision::Allow, DecisionPoint::AfterAddData);
}

TEST(ContentFiltering, BlockDownloadAfterAddData)
{
    downloadTest(Decision::Block, DecisionPoint::AfterAddData);
}

TEST(ContentFiltering, AllowDownloadAfterFinishedAddingData)
{
    downloadTest(Decision::Allow, DecisionPoint::AfterFinishedAddingData);
}

TEST(ContentFiltering, BlockDownloadAfterFinishedAddingData)
{
    downloadTest(Decision::Block, DecisionPoint::AfterFinishedAddingData);
}

TEST(ContentFiltering, AllowDownloadNever)
{
    downloadTest(Decision::Allow, DecisionPoint::Never);
}

TEST(ContentFiltering, BlockDownloadNever)
{
    downloadTest(Decision::Block, DecisionPoint::Never);
}

@interface LoadAlternateNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation LoadAlternateNavigationDelegate

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
    EXPECT_EQ(kWKErrorCodeFrameLoadBlockedByContentFilter, error.code);
    [webView _loadAlternateHTMLString:@"FAIL" baseURL:nil forUnreachableURL:[error.userInfo objectForKey:NSURLErrorFailingURLErrorKey]];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    [webView evaluateJavaScript:@"document.body.innerText" completionHandler:^ (id result, NSError *error) {
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"blocked", result);
        isDone = true;
    }];
}

@end

static void loadAlternateTest(Decision decision, DecisionPoint decisionPoint)
{
    @autoreleasepool {
        [TestProtocol registerWithScheme:@"http"];

        auto configuration = configurationWithContentFilterSettings(decision, decisionPoint);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[LoadAlternateNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://redirect/?result"]]];

        isDone = false;
        TestWebKitAPI::Util::run(&isDone);

        [TestProtocol unregister];
    }
}

TEST(ContentFiltering, LoadAlternateAfterWillSendRequestWK2)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterWillSendRequest);
}

TEST(ContentFiltering, LoadAlternateAfterRedirectWK2)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterRedirect);
}

TEST(ContentFiltering, LoadAlternateAfterResponseWK2)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterResponse);
}

TEST(ContentFiltering, LoadAlternateAfterAddDataWK2)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterAddData);
}

TEST(ContentFiltering, LoadAlternateAfterFinishedAddingDataWK2)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterFinishedAddingData);
}


@interface LazilyLoadPlatformFrameworksController : NSObject <WKNavigationDelegate>
@property (nonatomic, readonly) WKWebView *webView;
- (void)expectParentalControlsLoaded:(BOOL)parentalControlsShouldBeLoaded networkExtensionLoaded:(BOOL)networkExtensionShouldBeLoaded;
@end

@implementation LazilyLoadPlatformFrameworksController {
    RetainPtr<WKWebView> _webView;
    RetainPtr<id <ContentFilteringProtocol>> _remoteObjectProxy;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"ContentFilteringPlugIn"];
    _webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [_webView setNavigationDelegate:self];

    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(ContentFilteringProtocol)];
    _remoteObjectProxy = [[_webView _remoteObjectRegistry] remoteObjectProxyWithInterface:interface];

    return self;
}

- (WKWebView *)webView
{
    return _webView.get();
}

- (void)expectParentalControlsLoaded:(BOOL)parentalControlsShouldBeLoaded networkExtensionLoaded:(BOOL)networkExtensionShouldBeLoaded
{
    isDone = false;
    [_remoteObjectProxy checkIfPlatformFrameworksAreLoaded:^(BOOL parentalControlsLoaded, BOOL networkExtensionLoaded) {
#if HAVE(PARENTAL_CONTROLS)
        EXPECT_EQ(static_cast<bool>(parentalControlsShouldBeLoaded), static_cast<bool>(parentalControlsLoaded));
#endif
#if HAVE(NETWORK_EXTENSION)
        EXPECT_EQ(static_cast<bool>(networkExtensionShouldBeLoaded), static_cast<bool>(networkExtensionLoaded));
#endif // HAVE(NETWORK_EXTENSION)
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

TEST(ContentFiltering, LazilyLoadPlatformFrameworks)
{
    @autoreleasepool {
        auto controller = adoptNS([[LazilyLoadPlatformFrameworksController alloc] init]);
        [controller expectParentalControlsLoaded:NO networkExtensionLoaded:NO];

        isDone = false;
        [[controller webView] loadHTMLString:@"PASS" baseURL:[NSURL URLWithString:@"about:blank"]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO networkExtensionLoaded:NO];

        isDone = false;
        [[controller webView] loadData:[NSData dataWithBytes:"PASS" length:4] MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:[NSURL URLWithString:@"about:blank"]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO networkExtensionLoaded:NO];

        isDone = false;
        NSURL *fileURL = [[NSBundle mainBundle] URLForResource:@"ContentFiltering" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
        [[controller webView] loadFileURL:fileURL allowingReadAccessToURL:fileURL];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO networkExtensionLoaded:NO];

        isDone = false;
        [TestProtocol registerWithScheme:@"custom"];
        [[controller webView] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"custom://test"]]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO networkExtensionLoaded:NO];
        [TestProtocol unregister];

        isDone = false;
        [TestProtocol registerWithScheme:@"http"];
        [[controller webView] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://test"]]];
        TestWebKitAPI::Util::run(&isDone);
#if PLATFORM(MAC)
        [controller expectParentalControlsLoaded:NO networkExtensionLoaded:YES];
#else
        [controller expectParentalControlsLoaded:YES networkExtensionLoaded:YES];
#endif
        [TestProtocol unregister];

#if PLATFORM(MAC)
        isDone = false;
        [TestProtocol registerWithScheme:@"https"];
        [[controller webView] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://test"]]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:YES networkExtensionLoaded:YES];
        [TestProtocol unregister];
#endif
    }
}
