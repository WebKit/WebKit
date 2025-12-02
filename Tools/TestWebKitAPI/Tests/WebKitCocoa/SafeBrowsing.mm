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

#if HAVE(SAFE_BROWSING)

#import "ClassMethodSwizzler.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestResourceLoadDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <Foundation/NSURLError.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKResourceLoadDelegate.h>
#import <WebKit/_WKResourceLoadInfo.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/SoftLinking.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>

SOFT_LINK_PRIVATE_FRAMEWORK(SafariSafeBrowsing);
SOFT_LINK_CLASS(SafariSafeBrowsing, SSBLookupContext);
SOFT_LINK_CLASS(SafariSafeBrowsing, SSBLookupResult);
SOFT_LINK_CLASS(SafariSafeBrowsing, SSBServiceLookupResult);

static bool committedNavigation;
static bool warningShown;
static bool didCloseCalled;

@interface SafeBrowsingNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegatePrivate>
@end

@implementation SafeBrowsingNavigationDelegate

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    committedNavigation = true;
}

- (void)_webViewDidShowSafeBrowsingWarning:(WKWebView *)webView
{
    warningShown = true;
}

- (void)webViewDidClose:(WKWebView *)webView
{
    didCloseCalled = true;
}

@end

@interface TestServiceLookupResult : NSObject {
    RetainPtr<NSString> _provider;
    BOOL _isPhishing;
    BOOL _isMalware;
    BOOL _isUnwantedSoftware;
}
@end

@implementation TestServiceLookupResult

+ (instancetype)resultWithProvider:(RetainPtr<NSString>&&)provider phishing:(BOOL)phishing malware:(BOOL)malware unwantedSoftware:(BOOL)unwantedSoftware
{
    auto result = adoptNS([[TestServiceLookupResult alloc] init]);
    if (!result)
        return nil;

    result->_provider = WTFMove(provider);
    result->_isPhishing = phishing;
    result->_isMalware = malware;
    result->_isUnwantedSoftware = unwantedSoftware;

    return result.autorelease();
}

- (NSString *)provider
{
    return _provider.get();
}

- (BOOL)isPhishing
{
    return _isPhishing;
}

- (BOOL)isMalware
{
    return _isMalware;
}

- (BOOL)isUnwantedSoftware
{
    return _isUnwantedSoftware;
}

- (NSString *)malwareDetailsBaseURLString
{
    return @"test://";
}

- (NSURL *)learnMoreURL
{
    return [NSURL URLWithString:@"test://"];
}

- (NSString *)reportAnErrorBaseURLString
{
    return @"test://";
}

- (NSString *)localizedProviderDisplayName
{
    return @"test display name";
}

@end

@interface TestLookupResult : NSObject {
    RetainPtr<NSArray> _results;
}
@end

@implementation TestLookupResult

+ (instancetype)resultWithResults:(RetainPtr<NSArray<TestServiceLookupResult *>>&&)results
{
    auto result = adoptNS([[TestLookupResult alloc] init]);
    if (!result)
        return nil;
    
    result->_results = WTFMove(results);
    
    return result.autorelease();
}

- (NSArray<TestServiceLookupResult *> *)serviceLookupResults
{
    return _results.get();
}

@end

@interface TestLookupContext : NSObject
@end

@implementation TestLookupContext

+ (TestLookupContext *)sharedLookupContext
{
    static TestLookupContext *context = [[TestLookupContext alloc] init];
    return context;
}

- (void)lookUpURL:(NSURL *)URL completionHandler:(void (^)(TestLookupResult *, NSError *))completionHandler
{
    completionHandler([TestLookupResult resultWithResults:@[[TestServiceLookupResult resultWithProvider:@"SSBProviderApple" phishing:YES malware:NO unwantedSoftware:NO]]], nil);
}

@end

static NSURL *resourceURL(NSString *resource)
{
    return [NSBundle.test_resourcesBundle URLForResource:resource withExtension:@"html"];
}

TEST(SafeBrowsing, Preference)
{
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    __block bool done = false;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().didStartProvisionalNavigation = ^(WKWebView *, WKNavigation *) {
        done = true;
    };

    auto webView = adoptNS([WKWebView new]);
    EXPECT_TRUE([webView configuration].preferences.fraudulentWebsiteWarningEnabled);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    [webView setNavigationDelegate:delegate.get()];
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple")]];
    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = NO;
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple2")]];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE([webView configuration].preferences.fraudulentWebsiteWarningEnabled);
    EXPECT_FALSE([webView _safeBrowsingWarning]);
}

static RetainPtr<WKWebView> safeBrowsingView()
{
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    static auto delegate = adoptNS([SafeBrowsingNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple")]];
    EXPECT_FALSE(warningShown);
    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_TRUE(warningShown);
#if !PLATFORM(MAC)
    [[webView _safeBrowsingWarning] didMoveToWindow];
#endif
    return webView;
}

#if PLATFORM(MAC)
static void checkTitleAndClick(NSButton *button, const char* expectedTitle)
{
    EXPECT_STREQ(button.title.UTF8String, expectedTitle);
    [button performClick:nil];
}
#else
static void checkTitleAndClick(UIButton *button, const char* expectedTitle)
{
    EXPECT_STREQ([button attributedTitleForState:UIControlStateNormal].string.UTF8String, expectedTitle);
    UIView *target = button.superview.superview;
    SEL selector = NSSelectorFromString(strcmp(expectedTitle, "Go Back") ? @"showDetailsClicked" : @"goBackClicked");
    [target performSelector:selector];
}
#endif

template<typename ViewType> void goBack(ViewType *view, bool mainFrame = true)
{
    WKWebView *webView = (WKWebView *)view.superview;
    auto box = view.subviews.firstObject;
    checkTitleAndClick(box.subviews[3], "Go Back");
    if (mainFrame)
        EXPECT_EQ([webView _safeBrowsingWarning], nil);
    else
        EXPECT_NE([webView _safeBrowsingWarning], nil);
}

TEST(SafeBrowsing, GoBack)
{
    auto webView = safeBrowsingView();
    EXPECT_FALSE(didCloseCalled);
    goBack([webView _safeBrowsingWarning]);
    EXPECT_TRUE(didCloseCalled);
}

TEST(SafeBrowsing, GoBackAfterRestoreFromSessionState)
{
    auto webView1 = adoptNS([WKWebView new]);
    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"]]];
    [webView1 _test_waitForDidFinishNavigation];
    _WKSessionState *state = [webView1 _sessionState];

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto delegate = adoptNS([SafeBrowsingNavigationDelegate new]);
    auto webView2 = adoptNS([WKWebView new]);
    [webView2 configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    [webView2 setNavigationDelegate:delegate.get()];
    [webView2 setUIDelegate:delegate.get()];
    [webView2 _restoreSessionState:state andNavigate:YES];
    EXPECT_FALSE(warningShown);
    while (![webView2 _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_TRUE(warningShown);
#if !PLATFORM(MAC)
    [[webView2 _safeBrowsingWarning] didMoveToWindow];
#endif
    EXPECT_FALSE(didCloseCalled);
    goBack([webView2 _safeBrowsingWarning]);
    EXPECT_TRUE(didCloseCalled);
    WKBackForwardList *list = [webView2 backForwardList];
    EXPECT_FALSE(!!list.backItem);
    EXPECT_FALSE(!!list.forwardItem);
    EXPECT_TRUE([list.currentItem.URL.path hasSuffix:@"/simple.html"]);
}

template<typename ViewType> void visitUnsafeSite(ViewType *view)
{
    [view performSelector:NSSelectorFromString(@"clickedOnLink:") withObject:[NSURL URLWithString:@"WKVisitUnsafeWebsiteSentinel"]];
}

TEST(SafeBrowsing, VisitUnsafeWebsite)
{
    auto webView = safeBrowsingView();
    auto warning = [webView _safeBrowsingWarning];
    EXPECT_EQ(warning.subviews.count, 1ull);
#if PLATFORM(MAC)
    EXPECT_GT(warning.subviews.firstObject.subviews[2].frame.size.height, 0);
#endif
    EXPECT_WK_STREQ([webView title], "Deceptive Website Warning");
    checkTitleAndClick(warning.subviews.firstObject.subviews[4], "Show Details");
    EXPECT_EQ(warning.subviews.count, 2ull);
    EXPECT_FALSE(committedNavigation);
    visitUnsafeSite(warning);
    EXPECT_WK_STREQ([webView title], "");
    TestWebKitAPI::Util::run(&committedNavigation);
}

TEST(SafeBrowsing, NavigationClearsWarning)
{
    auto webView = safeBrowsingView();
    EXPECT_NE([webView _safeBrowsingWarning], nil);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"]]];
    while ([webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, ShowWarningSPI)
{
    __block bool completionHandlerCalled = false;
    __block BOOL shouldContinueValue = NO;
    auto webView = adoptNS([WKWebView new]);
    auto showWarning = ^{
        completionHandlerCalled = false;
        [webView _showSafeBrowsingWarningWithURL:nil title:@"test title" warning:@"test warning" details:adoptNS([[NSAttributedString alloc] initWithString:@"test details"]).get() completionHandler:^(BOOL shouldContinue) {
            shouldContinueValue = shouldContinue;
            completionHandlerCalled = true;
        }];
#if !PLATFORM(MAC)
        [[webView _safeBrowsingWarning] didMoveToWindow];
#endif
    };

    showWarning();
    checkTitleAndClick([webView _safeBrowsingWarning].subviews.firstObject.subviews[3], "Go Back");
    TestWebKitAPI::Util::run(&completionHandlerCalled);
    EXPECT_FALSE(shouldContinueValue);

    showWarning();
    [[webView _safeBrowsingWarning] performSelector:NSSelectorFromString(@"clickedOnLink:") withObject:[WKWebView _visitUnsafeWebsiteSentinel]];
    TestWebKitAPI::Util::run(&completionHandlerCalled);
    EXPECT_TRUE(shouldContinueValue);
}

static Vector<URL> urls;

@interface SafeBrowsingObserver : NSObject
@end

@implementation SafeBrowsingObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    urls.append((NSURL *)[change objectForKey:NSKeyValueChangeNewKey]);
}

@end

TEST(SafeBrowsing, URLObservation)
{
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    RetainPtr<NSURL> simpleURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];
    RetainPtr<NSURL> simple2URL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    auto observer = adoptNS([SafeBrowsingObserver new]);

    auto webViewWithWarning = [&] () -> RetainPtr<WKWebView> {
        auto webView = adoptNS([WKWebView new]);
        auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
        navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
            TestWebKitAPI::Util::runFor(0.01_s);
            decisionHandler(WKNavigationActionPolicyAllow);
        };
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
        [webView addObserver:observer.get() forKeyPath:@"URL" options:NSKeyValueObservingOptionNew context:nil];

        [webView loadHTMLString:@"meaningful content to be drawn" baseURL:simpleURL.get()];
        while (![webView _safeBrowsingWarning])
            TestWebKitAPI::Util::spinRunLoop();
#if !PLATFORM(MAC)
        [[webView _safeBrowsingWarning] didMoveToWindow];
#endif
        visitUnsafeSite([webView _safeBrowsingWarning]);
        EXPECT_TRUE(!![webView _safeBrowsingWarning]);
        while ([webView _safeBrowsingWarning])
            TestWebKitAPI::Util::spinRunLoop();
        EXPECT_FALSE(!![webView _safeBrowsingWarning]);

        [webView evaluateJavaScript:[NSString stringWithFormat:@"window.location='%@'", simple2URL.get()] completionHandler:nil];
        while (![webView _safeBrowsingWarning])
            TestWebKitAPI::Util::spinRunLoop();
#if !PLATFORM(MAC)
        [[webView _safeBrowsingWarning] didMoveToWindow];
#endif
        return webView;
    };
    
    auto checkURLs = [&] (Vector<RetainPtr<NSURL>>&& expected) {
        EXPECT_EQ(urls.size(), expected.size());
        if (urls.size() != expected.size())
            return;
        for (size_t i = 0; i < expected.size(); ++i)
            EXPECT_STREQ(urls[i].string().utf8().data(), [expected[i] absoluteString].UTF8String);
    };

    {
        auto webView = webViewWithWarning();
        checkURLs({ simpleURL, simple2URL });
        goBack([webView _safeBrowsingWarning]);
        checkURLs({ simpleURL, simple2URL, simpleURL });
        [webView removeObserver:observer.get() forKeyPath:@"URL"];
    }
    
    urls.clear();

    {
        auto webView = webViewWithWarning();
        checkURLs({ simpleURL, simple2URL });
        visitUnsafeSite([webView _safeBrowsingWarning]);
        TestWebKitAPI::Util::spinRunLoop(5);
        checkURLs({ simpleURL, simple2URL });
        [webView removeObserver:observer.get() forKeyPath:@"URL"];
    }
}

static RetainPtr<NSString> phishingResourceName;

@interface SimpleLookupContext : NSObject
@end

@implementation SimpleLookupContext

+ (SimpleLookupContext *)sharedLookupContext
{
    static SimpleLookupContext *context = [[SimpleLookupContext alloc] init];
    return context;
}

- (void)lookUpURL:(NSURL *)URL completionHandler:(void (^)(TestLookupResult *, NSError *))completionHandler
{
    BOOL phishing = NO;
    if ([URL isEqual:resourceURL(phishingResourceName.get())] || [[URL path] hasSuffix:phishingResourceName.get()])
        phishing = YES;
    completionHandler([TestLookupResult resultWithResults:@[[TestServiceLookupResult resultWithProvider:@"SSBProviderApple" phishing:phishing malware:NO unwantedSoftware:NO]]], nil);
}

@end

static bool navigationFinished;

@interface WKWebViewGoBackNavigationDelegate : NSObject <WKNavigationDelegate>
@end

@implementation WKWebViewGoBackNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    navigationFinished = true;
}

@end

TEST(SafeBrowsing, WKWebViewGoBack)
{
    phishingResourceName = @"simple3";
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [SimpleLookupContext methodForSelector:@selector(sharedLookupContext)]);
    
    auto delegate = adoptNS([WKWebViewGoBackNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple")]];
    TestWebKitAPI::Util::run(&navigationFinished);

    navigationFinished = false;
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple2")]];
    TestWebKitAPI::Util::run(&navigationFinished);

    navigationFinished = false;
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple3")]];
    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
    [webView goBack];
    TestWebKitAPI::Util::run(&navigationFinished);
    EXPECT_TRUE([[webView URL] isEqual:resourceURL(@"simple2")]);
}

TEST(SafeBrowsing, WKWebViewGoBackIFrame)
{
    phishingResourceName = @"simple";
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [SimpleLookupContext methodForSelector:@selector(sharedLookupContext)]);
    
    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView configuration].preferences._safeBrowsingEnabled = YES;

    __block bool navigationFailed = false;
    __block bool navigationFinished = false;
    delegate.get().didFailProvisionalLoadInSubframeWithError = ^(WKWebView *, WKFrameInfo *frame, NSError *error) {
        EXPECT_NOT_NULL(error);
        auto failingURL = (NSURL *)[error.userInfo valueForKey:NSURLErrorFailingURLErrorKey];
        EXPECT_TRUE([failingURL.lastPathComponent isEqualToString:@"simple.html"]);
#if USE(NSURL_ERROR_FAILING_URL_STRING_KEY)
        auto failingURLString = (NSString *)[error.userInfo valueForKey:NSURLErrorFailingURLStringErrorKey];
        EXPECT_TRUE([failingURLString hasSuffix:@"/simple.html"]);
#endif
        navigationFailed = true;
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        navigationFinished = true;
    };

    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple2")]];
    TestWebKitAPI::Util::run(&navigationFinished);
    EXPECT_FALSE(navigationFailed);
    navigationFinished = false;
    navigationFailed = false;

    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple-iframe")]];
    TestWebKitAPI::Util::run(&navigationFinished);
    TestWebKitAPI::Util::run(&navigationFailed);
}

@interface NullLookupContext : NSObject
@end
@implementation NullLookupContext
+ (NullLookupContext *)sharedLookupContext
{
    return nil;
}
@end

TEST(SafeBrowsing, MissingFramework)
{
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [NullLookupContext methodForSelector:@selector(sharedLookupContext)]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
}

static Seconds delayDuration;

@interface DelayedLookupContext : NSObject
@end

@implementation DelayedLookupContext

+ (DelayedLookupContext *)sharedLookupContext
{
    static DelayedLookupContext *context = [[DelayedLookupContext alloc] init];
    return context;
}

- (void)lookUpURL:(NSURL *)URL completionHandler:(void (^)(TestLookupResult *, NSError *))completionHandler
{
    BOOL phishing = ![URL isEqual:resourceURL(@"simple2")] && ![[URL path] isEqual:@"/safe"];
    RunLoop::mainSingleton().dispatchAfter(delayDuration, [completionHandler = makeBlockPtr(completionHandler), phishing] {
        completionHandler.get()([TestLookupResult resultWithResults:@[[TestServiceLookupResult resultWithProvider:@"SSBProviderApple" phishing:phishing malware:NO unwantedSoftware:NO]]], nil);
    });
}

@end

TEST(SafeBrowsing, HangTimeout)
{
    delayDuration = 1000_s;
    TestWebKitAPI::HTTPServer server({
        { "/test"_s, { "test"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [DelayedLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    navigationFinished = false;
    [webView setNavigationDelegate:delegate.get()];
    [webView evaluateJavaScript:@"window.location = 'https://example2.com/test'" completionHandler:nil];

    [delegate waitForDidFinishNavigation];
}

TEST(SafeBrowsing, PostResponse)
{
    delayDuration = 25_ms;
    TestWebKitAPI::HTTPServer server({
        { "/test"_s, { "test"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [DelayedLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    navigationFinished = false;
    [webView setNavigationDelegate:delegate.get()];
    [webView evaluateJavaScript:@"window.location = 'https://example2.com/test'" completionHandler:nil];

    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, PostResponseIframe)
{
    delayDuration = 25_ms;
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [DelayedLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    [webView configuration].preferences._safeBrowsingEnabled = YES;

    __block bool navigationFailed = false;
    __block bool navigationFinished = false;
    delegate.get().didFailProvisionalLoadInSubframeWithError = ^(WKWebView *, WKFrameInfo *frame, NSError *error) {
        EXPECT_NOT_NULL(error);
        auto failingURL = (NSURL *)[error.userInfo valueForKey:NSURLErrorFailingURLErrorKey];
        EXPECT_TRUE([failingURL.lastPathComponent isEqualToString:@"simple.html"]);
#if USE(NSURL_ERROR_FAILING_URL_STRING_KEY)
        auto failingURLString = (NSString *)[error.userInfo valueForKey:NSURLErrorFailingURLStringErrorKey];
        EXPECT_TRUE([failingURLString hasSuffix:@"/simple.html"]);
#endif
        navigationFailed = true;
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        navigationFinished = true;
    };

    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple2")]];
    TestWebKitAPI::Util::run(&navigationFinished);
    EXPECT_FALSE(navigationFailed);
}

static const char* mainResource = "";
TEST(SafeBrowsing, PreresponseSafeBrowsingWarning)
{
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [TestLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    auto handler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"sb"];
    configuration.get().preferences.fraudulentWebsiteWarningEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];

    [handler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        RunLoop::mainSingleton().dispatchAfter(1000_s, [task = retainPtr(task)] {
            auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.get().request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
            [task didReceiveResponse:response.get()];
            [task didReceiveData:[NSData dataWithBytes:mainResource length:strlen(mainResource)]];
            [task didFinish];
        });
    }];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"sb://host1/main.html"]]];

    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, PostResponseServerSideRedirect)
{
    delayDuration = 2_ms;
    TestWebKitAPI::HTTPServer server({
        { "/safe"_s, { 301, { { "Location"_s, "/redirectTarget"_s } } } },
        { "/redirectTarget"_s, { "hi"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [DelayedLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    navigationFinished = false;
    [webView setNavigationDelegate:delegate.get()];
    [webView evaluateJavaScript:@"window.location = 'https://example2.com/safe'" completionHandler:nil];

    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, MultipleRedirectsFirstPhishing)
{
    phishingResourceName = @"safe";
    TestWebKitAPI::HTTPServer server({
        { "/safe"_s, { 301, { { "Location"_s, "/redirectTarget1"_s } } } },
        { "/redirectTarget1"_s, { 301, { { "Location"_s, "/redirectTarget2"_s } } } },
        { "/redirectTarget2"_s, { "hi"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [SimpleLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    navigationFinished = false;
    [webView setNavigationDelegate:delegate.get()];
    [webView evaluateJavaScript:@"window.location = 'https://example2.com/safe'" completionHandler:nil];

    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, MultipleRedirectsMiddlePhishing)
{
    phishingResourceName = @"redirectTarget1";
    TestWebKitAPI::HTTPServer server({
        { "/safe"_s, { 301, { { "Location"_s, "/redirectTarget1"_s } } } },
        { "/redirectTarget1"_s, { 301, { { "Location"_s, "/redirectTarget2"_s } } } },
        { "/redirectTarget2"_s, { "hi"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [SimpleLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    navigationFinished = false;
    [webView setNavigationDelegate:delegate.get()];
    [webView evaluateJavaScript:@"window.location = 'https://example2.com/safe'" completionHandler:nil];

    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, MultipleRedirectsLastPhishing)
{
    phishingResourceName = @"redirectTarget2";
    TestWebKitAPI::HTTPServer server({
        { "/safe"_s, { 301, { { "Location"_s, "/redirectTarget1"_s } } } },
        { "/redirectTarget1"_s, { 301, { { "Location"_s, "/redirectTarget2"_s } } } },
        { "/redirectTarget2"_s, { "hi"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [SimpleLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    navigationFinished = false;
    [webView setNavigationDelegate:delegate.get()];
    [webView evaluateJavaScript:@"window.location = 'https://example2.com/safe'" completionHandler:nil];

    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, PostResponseInjectedBundleSkipsDecidePolicyForResponse)
{
    delayDuration = 25_ms;
    TestWebKitAPI::HTTPServer server({
        { "/test"_s, { "test"_s } },
    });
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"SkipDecidePolicyForResponsePlugIn"];

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [DelayedLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    navigationFinished = false;
    [webView setNavigationDelegate:delegate.get()];
    [webView evaluateJavaScript:@"window.location = 'https://example2.com/test'" completionHandler:nil];

    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, PostTimeout)
{
    delayDuration = 100_ms;
    TestWebKitAPI::HTTPServer server({
        { "/test"_s, { "test"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();

    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [DelayedLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    [webView configuration].preferences.fraudulentWebsiteWarningEnabled = YES;
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [delegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:delegate.get()];

    [webView evaluateJavaScript:@"window.location = 'https://example2.com/test'" completionHandler:nil];

    EXPECT_WK_STREQ([webView title], "");
    while (![webView _safeBrowsingWarning])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(SafeBrowsing, PhishingInFrame)
{
    phishingResourceName = @"simple";
    ClassMethodSwizzler swizzler(getSSBLookupContextClassSingleton(), @selector(sharedLookupContext), [SimpleLookupContext methodForSelector:@selector(sharedLookupContext)]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    auto webView = adoptNS([WKWebView new]);
    auto configuration = webView.get().configuration;
    auto preferences = configuration.preferences;
    preferences._safeBrowsingEnabled = YES;

    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"SiteIsolationEnabled"]) {
            [preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }

    __block bool navigationFailed = false;
    __block bool navigationFinished = false;
    delegate.get().didFailProvisionalLoadInSubframeWithError = ^(WKWebView *, WKFrameInfo *frame, NSError *error) {
        EXPECT_NOT_NULL(error);
        auto failingURL = (NSURL *)[error.userInfo valueForKey:NSURLErrorFailingURLErrorKey];
        EXPECT_TRUE([failingURL.lastPathComponent isEqualToString:@"simple.html"]);
#if USE(NSURL_ERROR_FAILING_URL_STRING_KEY)
        auto failingURLString = (NSString *)[error.userInfo valueForKey:NSURLErrorFailingURLStringErrorKey];
        EXPECT_TRUE([failingURLString hasSuffix:@"/simple.html"]);
#endif
        navigationFailed = true;
    };
    delegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        navigationFinished = true;
    };

    [webView setNavigationDelegate:delegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:resourceURL(@"simple-iframe")]];
    TestWebKitAPI::Util::run(&navigationFinished);
    TestWebKitAPI::Util::run(&navigationFailed);
    EXPECT_TRUE(navigationFailed);
    EXPECT_TRUE(navigationFinished);
}


#endif // HAVE(SAFE_BROWSING)
