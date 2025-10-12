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

#if ENABLE(CONTENT_FILTERING) && HAVE(WEBCONTENTANALYSIS_FRAMEWORK)

#import "DeprecatedGlobalValues.h"
#import "ContentFiltering.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestProtocol.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebCore/MockContentFilterSettings.h>
#import <WebKit/WKErrorRef.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKDownloadDelegate.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <pal/spi/cocoa/NEFilterSourceSPI.h>
#import <pal/spi/cocoa/WebFilterEvaluatorSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(NetworkExtension);
SOFT_LINK_CLASS_OPTIONAL(NetworkExtension, NEFilterSource);

SOFT_LINK_PRIVATE_FRAMEWORK(WebContentAnalysis);
SOFT_LINK_CLASS(WebContentAnalysis, WebFilterEvaluator);

using Decision = WebCore::MockContentFilterSettings::Decision;
using DecisionPoint = WebCore::MockContentFilterSettings::DecisionPoint;

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
    self = [super init];
    return self;
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
    EXPECT_WK_STREQ(webView.URL.absoluteString, @"https://redirect/?pass");
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    EXPECT_WK_STREQ(webView.URL.absoluteString, @"https://pass/");
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

TEST(ContentFiltering, URLAfterServerRedirect)
{
    @autoreleasepool {
        [TestProtocol registerWithScheme:@"https"];

        auto configuration = configurationWithContentFilterSettings(Decision::Allow, DecisionPoint::AfterAddData);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[ServerRedirectNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://redirect?pass"]]];
        TestWebKitAPI::Util::run(&isDone);

        [TestProtocol unregister];
    }
}

@interface BecomeDownloadDelegate : NSObject <WKNavigationDelegate>
@end

@implementation BecomeDownloadDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    decisionHandler(WKNavigationResponsePolicyDownload);
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
        [TestProtocol registerWithScheme:@"https"];

        auto configuration = configurationWithContentFilterSettings(decision, decisionPoint);
        auto downloadDelegate = adoptNS([[ContentFilteringDownloadDelegate alloc] init]);
        [[configuration processPool] _setDownloadDelegate:downloadDelegate.get()];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[BecomeDownloadDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://redirect/?download"]]];

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

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    EXPECT_FALSE(true);
}

@end

static void loadAlternateTest(Decision decision, DecisionPoint decisionPoint)
{
    @autoreleasepool {
        [TestProtocol registerWithScheme:@"https"];

        auto configuration = configurationWithContentFilterSettings(decision, decisionPoint);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[LoadAlternateNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://redirect/?result"]]];

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

TEST(ContentFiltering, CookieAccessFromReplacementData)
{
    auto networkProcessStarter = adoptNS([WKWebView new]);
    [networkProcessStarter synchronouslyLoadHTMLString:@"hi"];
    auto pidBefore = networkProcessStarter.get().configuration.websiteDataStore._networkProcessIdentifier;
    loadAlternateTest(Decision::Block, DecisionPoint::AfterWillSendRequest);
    auto pidAfter = networkProcessStarter.get().configuration.websiteDataStore._networkProcessIdentifier;
    EXPECT_EQ(pidBefore, pidAfter);
    TestWebKitAPI::Util::runFor(Seconds(0.1));
}

@interface LazilyLoadPlatformFrameworksController : NSObject <WKNavigationDelegate>
@property (nonatomic, readonly) WKWebView *webView;
- (void)expectParentalControlsLoaded:(BOOL)parentalControlsShouldBeLoaded;
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

- (void)expectParentalControlsLoaded:(BOOL)parentalControlsShouldBeLoaded
{
    isDone = false;
    [_remoteObjectProxy checkIfPlatformFrameworksAreLoaded:^(BOOL parentalControlsLoaded) {
#if HAVE(PARENTAL_CONTROLS) && HAVE(WEBCONTENTANALYSIS_FRAMEWORK)
        EXPECT_EQ(static_cast<bool>(parentalControlsShouldBeLoaded), static_cast<bool>(parentalControlsLoaded));
#endif
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

@end

static BOOL filterRequired(id self, SEL _cmd)
{
    return YES;
}

static BOOL isManagedSession(id self, SEL _cmd)
{
    return YES;
}

TEST(ContentFiltering, LazilyLoadPlatformFrameworks)
{
    // Swizzle [NEFilterSource filterRequired] to return YES in the UI process since NetworkExtension will not be loaded otherwise.
    Method method = class_getClassMethod(getNEFilterSourceClassSingleton(), @selector(filterRequired));
    method_setImplementation(method, reinterpret_cast<IMP>(filterRequired));

    // Swizzle [WebFilterEvaluator isManagedSession] to return YES in the UI process since WebContentAnalysis will not be loaded otherwise.
    method = class_getClassMethod(getWebFilterEvaluatorClassSingleton(), @selector(isManagedSession));
    method_setImplementation(method, reinterpret_cast<IMP>(isManagedSession));

    @autoreleasepool {
        auto controller = adoptNS([[LazilyLoadPlatformFrameworksController alloc] init]);
        [controller expectParentalControlsLoaded:NO];

        isDone = false;
        [[controller webView] loadHTMLString:@"PASS" baseURL:[NSURL URLWithString:@"about:blank"]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO];

        isDone = false;
        [[controller webView] loadData:[NSData dataWithBytes:"PASS" length:4] MIMEType:@"text/html" characterEncodingName:@"UTF-8" baseURL:[NSURL URLWithString:@"about:blank"]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO];

        isDone = false;
        NSURL *fileURL = [NSBundle.test_resourcesBundle URLForResource:@"ContentFiltering" withExtension:@"html"];
        [[controller webView] loadFileURL:fileURL allowingReadAccessToURL:fileURL];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO];

        isDone = false;
        [TestProtocol registerWithScheme:@"custom"];
        [[controller webView] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"custom://test"]]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO];
        [TestProtocol unregister];

        isDone = false;
        [TestProtocol registerWithScheme:@"http"];
        [[controller webView] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://test"]]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO];
        [TestProtocol unregister];

#if PLATFORM(MAC)
        isDone = false;
        [TestProtocol registerWithScheme:@"https"];
        [[controller webView] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://test"]]];
        TestWebKitAPI::Util::run(&isDone);
        [controller expectParentalControlsLoaded:NO];
        [TestProtocol unregister];
#endif
    }
}

TEST(ContentFiltering, URLAfterServerRedirectBlocked)
{
    auto mainForFetchTestBytes = "<html>"
    "<body>"
    "<script>"
    "function log(msg)"
    "{"
    "    window.webkit.messageHandlers.sw.postMessage(msg);"
    "}"
    ""
    "try {"
    ""
    "function addFrame()"
    "{"
    "    frame = document.createElement('iframe');"
    "    frame.src = \"/test.html\";"
    "    frame.onload = function() { window.webkit.messageHandlers.sw.postMessage(frame.contentDocument.body.innerHTML); }"
    "    document.body.appendChild(frame);"
    "}"
    ""
    "navigator.serviceWorker.register('/sw.js').then(function(reg) {"
    "    if (reg.active) {"
    "        addFrame();"
    "        return;"
    "    }"
    "    worker = reg.installing;"
    "    worker.addEventListener('statechange', function() {"
    "        if (worker.state == 'activated')"
    "            addFrame();"
    "    });"
    "}).catch(function(error) {"
    "    log(\"Registration failed with: \" + error);"
    "});"
    "} catch(e) {"
    "    log(\"Exception: \" + e);"
    "}"
    "</script>"
    "</body>"
    "</html>"_s;

    auto serviceWorkerJS = "<script>"
    "self.addEventListener(\"fetch\", (event) => {"
    "});"
    "</script>"_s;

    [WKWebsiteDataStore _allowWebsiteDataRecordsForAllOrigins];

    // Start with a clean slate data store
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    @autoreleasepool {
        [TestProtocol registerWithScheme:@"https"];

        TestWebKitAPI::HTTPServer server({
            { "/"_s, { mainForFetchTestBytes } },
            { "/sw.js"_s, { { { "Content-Type"_s, "application/javascript"_s } }, serviceWorkerJS } },
        });

        auto configuration = configurationWithContentFilterSettings(Decision::Block, DecisionPoint::AfterAddData);
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto navigationDelegate = adoptNS([[LoadAlternateNavigationDelegate alloc] init]);
        [webView setNavigationDelegate:navigationDelegate.get()];
        [webView loadRequest:server.request()];

        // LoadAlternateNavigationDelegate checks expectations here
        TestWebKitAPI::Util::run(&isDone);

        [TestProtocol unregister];
    }
}



#endif // ENABLE(CONTENT_FILTERING)
