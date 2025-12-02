/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#import "DragAndDropSimulator.h"
#import "FindInPageUtilities.h"
#import "FrameTreeChecks.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestScriptMessageHandler.h"
#import "TestUIDelegate.h"
#import "TestURLSchemeHandler.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import "WKWebViewFindStringFindDelegate.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/SQLiteDatabase.h>
#import <WebCore/SQLiteStatement.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKNavigationDelegatePrivate.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKURLSchemeTaskPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKJSHandle.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKSessionState.h>
#import <WebKit/_WKTextManipulationConfiguration.h>
#import <WebKit/_WKTextManipulationDelegate.h>
#import <WebKit/_WKTextManipulationItem.h>
#import <WebKit/_WKTextManipulationToken.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/MakeString.h>

#if ENABLE(IMAGE_ANALYSIS)
#import "ImageAnalysisTestingUtilities.h"
#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

@interface WKWebView ()
- (void)copy:(id)sender;
- (void)paste:(id)sender;
@end

@interface SiteIsolationTextManipulationDelegate : NSObject <_WKTextManipulationDelegate>
- (void)_webView:(WKWebView *)webView didFindTextManipulationItems:(NSArray<_WKTextManipulationItem *> *)items;
@property (nonatomic, readonly, copy) NSArray<_WKTextManipulationItem *> *items;
@end

@implementation SiteIsolationTextManipulationDelegate {
    RetainPtr<NSMutableArray> _items;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _items = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)_webView:(WKWebView *)webView didFindTextManipulationItems:(NSArray<_WKTextManipulationItem *> *)items
{
    [_items addObjectsFromArray:items];
}

- (NSArray<_WKTextManipulationItem *> *)items
{
    return _items.get();
}
@end

@interface NavigationDelegateAllowingAllTLS : NSObject<WKNavigationDelegate>
- (void)waitForDidFinishNavigation;
@end

@implementation NavigationDelegateAllowingAllTLS {
    bool _finishedNavigation;
}
- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}
- (void)waitForDidFinishNavigation
{
    _finishedNavigation = false;
    TestWebKitAPI::Util::run(&_finishedNavigation);
}
- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _finishedNavigation = true;
}
@end

@interface TestObserver : NSObject

@property (nonatomic, copy) void (^observeValueForKeyPath)(NSString *, id);

@end

@implementation TestObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    _observeValueForKeyPath(keyPath, object);
}

@end

#if ENABLE(IMAGE_ANALYSIS)

static unsigned gDidProcessRequestCount = 0;

static void processRequestWithResults(id, SEL, VKImageAnalyzerRequest *, void (^)(double progress), void (^completion)(VKImageAnalysis *, NSError *))
{
    gDidProcessRequestCount++;
    completion(TestWebKitAPI::createImageAnalysisWithSimpleFixedResults().get(), nil);
}

static VKImageAnalyzerRequest *makeFakeRequest(id, SEL, CGImageRef image, VKImageOrientation orientation, VKAnalysisTypes requestTypes)
{
    return TestWebKitAPI::createRequest(image, orientation, requestTypes).leakRef();
}

template <typename FunctionType>
std::pair<std::unique_ptr<InstanceMethodSwizzler>, std::unique_ptr<InstanceMethodSwizzler>> makeImageAnalysisRequestSwizzler(FunctionType function)
{
    return std::pair {
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerClassSingleton(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(function)),
        makeUnique<InstanceMethodSwizzler>(PAL::getVKImageAnalyzerRequestClassSingleton(), @selector(initWithCGImage:orientation:requestType:), reinterpret_cast<IMP>(makeFakeRequest))
    };
}

@interface TestWKWebViewImageAnalysisTests : TestWKWebView
- (void)waitForImageAnalysisRequests:(unsigned)numberOfRequests;
@end

@implementation TestWKWebViewImageAnalysisTests

- (void)waitForImageAnalysisRequests:(unsigned)numberOfRequests
{
    TestWebKitAPI::Util::waitForConditionWithLogging([&] {
        return gDidProcessRequestCount == numberOfRequests;
    }, 3, @"Timed out waiting for %u image analysis requests to complete, got %u", numberOfRequests, gDidProcessRequestCount);

    [self waitForNextPresentationUpdate];
    EXPECT_EQ(gDidProcessRequestCount, numberOfRequests);
}

@end

#endif // ENABLE(IMAGE_ANALYSIS)

namespace TestWebKitAPI {

static void enableFeature(WKWebViewConfiguration *configuration, NSString *featureName)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:featureName]) {
            [preferences _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

static void enableSiteIsolation(WKWebViewConfiguration *configuration)
{
    enableFeature(configuration, @"SiteIsolationEnabled");
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect, bool enable)
{
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    if (enable)
        enableSiteIsolation(configuration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:rect configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTFMove(webView), WTFMove(navigationDelegate) };
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> viewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect = CGRectZero)
{
    return siteIsolatedViewAndDelegate(configuration, rect, false);
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(RetainPtr<WKWebViewConfiguration> configuration, CGRect rect = CGRectZero)
{
    return siteIsolatedViewAndDelegate(configuration, rect, true);
}

enum class EnableProcessCache : bool { No, Yes };
static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewWithSharedProcess(const HTTPServer& server,
    EnableProcessCache enableProcessCache = EnableProcessCache::No, NSURL *dataStoreDirectory = nil, NSURL *itpRoot = nil, NSString *domainsWithUserInteraction = nil)
{
    RetainPtr<_WKWebsiteDataStoreConfiguration> dataStoreConfiguration;
    if (!dataStoreDirectory || !itpRoot)
        dataStoreConfiguration = [[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration];
    else {
        dataStoreConfiguration = [[_WKWebsiteDataStoreConfiguration alloc] initWithDirectory:dataStoreDirectory];
        dataStoreConfiguration.get()._resourceLoadStatisticsDirectory = itpRoot;
    }

    [dataStoreConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [dataStoreConfiguration setAdditionalDomainsWithUserInteractionForTesting:domainsWithUserInteraction];

    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];
    if (enableProcessCache == EnableProcessCache::Yes) {
        auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
        processPoolConfiguration.get().usesWebProcessCache = YES;
        processPoolConfiguration.get().prewarmsProcessesAutomatically = YES;
        auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
        [configuration setProcessPool:processPool.get()];
    }

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    enableSiteIsolation(configuration.get());
    enableFeature(configuration.get(), @"SiteIsolationSharedProcessEnabled");
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();
    return { WTFMove(webView), WTFMove(navigationDelegate) };
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> siteIsolatedViewAndDelegate(const HTTPServer& server, CGRect rect = CGRectZero)
{
    return siteIsolatedViewAndDelegate(server.httpsProxyConfiguration(), rect);
}

static std::pair<RetainPtr<TestWKWebView>, RetainPtr<TestNavigationDelegate>> viewAndDelegate(const HTTPServer& server, CGRect rect = CGRectZero)
{
    return viewAndDelegate(server.httpsProxyConfiguration(), rect);
}

static bool processStillRunning(pid_t pid)
{
    return !kill(pid, 0);
}

static bool frameTreesMatch(_WKFrameTreeNode *actualRoot, ExpectedFrameTree&& expectedRoot)
{
    WKFrameInfo *info = actualRoot.info;
    if (info._isLocalFrame != std::holds_alternative<String>(expectedRoot.remoteOrOrigin))
        return false;

    if (auto* expectedOrigin = std::get_if<String>(&expectedRoot.remoteOrOrigin)) {
        WKSecurityOrigin *origin = info.securityOrigin;
        auto actualOrigin = makeString(String(origin.protocol), "://"_s, String(origin.host), origin.port ? makeString(':', origin.port) : String());
        if (actualOrigin != *expectedOrigin)
            return false;
    }

    if (actualRoot.childFrames.count != expectedRoot.children.size())
        return false;
    for (_WKFrameTreeNode *actualChild in actualRoot.childFrames) {
        auto index = expectedRoot.children.findIf([&] (auto& expectedFrameTree) {
            return frameTreesMatch(actualChild, ExpectedFrameTree { expectedFrameTree });
        });
        if (index == WTF::notFound)
            return false;
        expectedRoot.children.removeAt(index);
    }
    return expectedRoot.children.isEmpty();
}

static bool frameTreesMatch(NSSet<_WKFrameTreeNode *> *actualFrameTrees, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    if (actualFrameTrees.count != expectedFrameTrees.size())
        return false;

    for (_WKFrameTreeNode *root in actualFrameTrees) {
        auto index = expectedFrameTrees.findIf([&] (auto& expectedFrameTree) {
            return frameTreesMatch(root, ExpectedFrameTree { expectedFrameTree });
        });
        if (index == WTF::notFound)
            return false;
        expectedFrameTrees.removeAt(index);
    }
    return expectedFrameTrees.isEmpty();
}

static RetainPtr<NSSet> frameTrees(WKWebView *webView)
{
    __block RetainPtr<NSSet> result;
    [webView _frameTrees:^(NSSet<_WKFrameTreeNode *> *frameTrees) {
        result = frameTrees;
    }];
    while (!result)
        Util::spinRunLoop();
    return result;
}

static Vector<char> indentation(size_t count)
{
    Vector<char> result;
    for (size_t i = 0; i < count; i++)
        result.append(' ');
    result.append(0);
    return result;
}

static void printTree(_WKFrameTreeNode *n, size_t indent = 0)
{
    if (n.info._isLocalFrame)
        WTFLogAlways("%s%@://%@ (pid %d)", indentation(indent).span().data(), n.info.securityOrigin.protocol, n.info.securityOrigin.host, n.info._processIdentifier);
    else
        WTFLogAlways("%s(remote) (pid %d)", indentation(indent).span().data(), n.info._processIdentifier);
    for (_WKFrameTreeNode *c in n.childFrames)
        printTree(c, indent + 1);
}

static void printTree(const ExpectedFrameTree& n, size_t indent = 0)
{
    if (auto* s = std::get_if<String>(&n.remoteOrOrigin))
        WTFLogAlways("%s%s", indentation(indent).span().data(), s->utf8().data());
    else
        WTFLogAlways("%s(remote)", indentation(indent).span().data());
    for (const auto& c : n.children)
        printTree(c, indent + 1);
}

static void checkFrameTreesInProcesses(NSSet<_WKFrameTreeNode *> *actualTrees, const Vector<ExpectedFrameTree>& expectedFrameTrees)
{
    bool result = frameTreesMatch(actualTrees, Vector<ExpectedFrameTree> { expectedFrameTrees });
    if (!result) {
        WTFLogAlways("ACTUAL");
        for (_WKFrameTreeNode *n in actualTrees)
            printTree(n);
        WTFLogAlways("EXPECTED");
        for (const auto& e : expectedFrameTrees)
            printTree(e);
        WTFLogAlways("END");
    }
    EXPECT_TRUE(result);
}

void checkFrameTreesInProcesses(WKWebView *webView, Vector<ExpectedFrameTree>&& expectedFrameTrees)
{
    checkFrameTreesInProcesses(frameTrees(webView).get(), WTFMove(expectedFrameTrees));
}

static unsigned countWebPages(const RetainPtr<WKWebView>& webView)
{
    __block bool done { false };
    __block unsigned result { 0 };
    [webView.get().configuration.processPool _countWebPagesInAllProcessesForTesting:^(unsigned count) {
        result = count;
        done = true;
    }];
    Util::run(&done);
    return result;
}

enum class FrameType : bool { Local, Remote };
static pid_t findFramePID(NSSet<_WKFrameTreeNode *> *set, FrameType local)
{
    for (_WKFrameTreeNode *node in set) {
        if (node.info._isLocalFrame == (local == FrameType::Local))
            return node.info._processIdentifier;
    }
    EXPECT_FALSE(true);
    return 0;
}

TEST(SiteIsolation, LoadingCallbacksAndPostMessage)
{
    auto exampleHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('parent frame received ' + event.data)"
    "    }, false);"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*');"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        parent.window.postMessage(event.data + 'pong', { 'targetOrigin' : '*' });"
    "    }, false)"
    "</script>"_s;

    bool finishedLoading { false };
    size_t framesCommitted { 0 };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse(exampleHTML).serialize());
                continue;
            }
            if (path == "/webkit"_s) {
                size_t contentLength = 2000000 + webkitHTML.length();
                co_await connection.awaitableSend(makeString("HTTP/1.1 200 OK\r\nContent-Length: "_s, contentLength, "\r\n\r\n"_s));

                co_await connection.awaitableSend(webkitHTML);
                co_await connection.awaitableSend(Vector<uint8_t>(1000000, ' '));

                while (framesCommitted < 2)
                    Util::spinRunLoop();
                Util::runFor(Seconds(0.5));
                EXPECT_EQ(framesCommitted, 2u);

                EXPECT_FALSE(finishedLoading);
                co_await connection.awaitableSend(Vector<uint8_t>(1000000, ' '));
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didCommitLoadWithRequestInFrame = makeBlockPtr([&](WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo) {
        NSString *url = frameInfo.request.URL.absoluteString;
        switch (++framesCommitted) {
        case 1:
            EXPECT_WK_STREQ(url, "https://example.com/example");
            EXPECT_TRUE(frameInfo.isMainFrame);
            break;
        case 2:
            EXPECT_WK_STREQ(url, "https://webkit.org/webkit");
            EXPECT_FALSE(frameInfo.isMainFrame);
            break;
        default:
            EXPECT_FALSE(true);
            break;
        }
    }).get();
    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "parent frame received pingpong");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, CancelNavigationResponseCleansUpProvisionalFrame)
{
    HTTPServer server({
        { "/main"_s, { "hi"_s } },
        { "/iframe1"_s, { "<script>alert('loaded iframe1')</script>"_s } },
        { "/iframe2"_s, { "shouldn't actually load"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:
        @"var iframe1 = document.createElement('iframe');"
        "document.body.appendChild(iframe1);"
        "iframe1.src = 'https://apple.com/iframe1';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded iframe1");

    __block uint32_t failureCount = 0;
    navigationDelegate.get().didFailProvisionalLoadInSubframeWithError = ^(WKWebView* webView, WKFrameInfo* frame, NSError* error) {
        EXPECT_WK_STREQ(error.domain, WebKitErrorDomain);
        EXPECT_EQ(error.code, WebKitErrorFrameLoadInterruptedByPolicyChange);
        failureCount++;
    };

    navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse* navigationResponse, void (^completionHandler)(WKNavigationResponsePolicy)) {
        completionHandler(WKNavigationResponsePolicyCancel);
    };

    [webView evaluateJavaScript:
        @"var iframe2 = document.createElement('iframe');"
        "document.body.appendChild(iframe2);"
        "iframe2.src = 'https://apple.com/iframe2';"
    completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 1; }
    ));

    // Make sure second navigation doesn't assert in WebFrame::createProvisionalFrame()
    [webView evaluateJavaScript:@"iframe2.src = 'https://apple.com/iframe2';" completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 2; }
    ));
}

TEST(SiteIsolation, CancelNavigationActionCleansUpProvisionalFrame)
{
    HTTPServer server({
        { "/main"_s, { "hi"_s } },
        { "/iframe1"_s, { "<script>alert('loaded iframe1')</script>"_s } },
        { "/iframe2"_s, { 302, { { "Location"_s, "https://example.org/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { "this should not be loaded"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:
        @"var iframe1 = document.createElement('iframe');"
        "document.body.appendChild(iframe1);"
        "iframe1.src = 'https://apple.com/iframe1';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded iframe1");

    __block uint32_t failureCount = 0;
    navigationDelegate.get().didFailProvisionalLoadInSubframeWithError = ^(WKWebView* webView, WKFrameInfo* frame, NSError* error) {
        EXPECT_WK_STREQ(error.domain, WebKitErrorDomain);
        EXPECT_EQ(error.code, WebKitErrorFrameLoadInterruptedByPolicyChange);
        failureCount++;
    };

    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction* action, void (^completionHandler)(WKNavigationActionPolicy)) {
        if ([action.request.URL.absoluteString containsString:@"/redirected"])
            completionHandler(WKNavigationActionPolicyCancel);
        else
            completionHandler(WKNavigationActionPolicyAllow);
    };

    [webView evaluateJavaScript:
        @"var iframe2 = document.createElement('iframe');"
        "document.body.appendChild(iframe2);"
        "iframe2.src = 'https://apple.com/iframe2';"
    completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 1; }
    ));

    // Make sure second navigation doesn't assert in WebFrame::createProvisionalFrame()
    [webView evaluateJavaScript:@"iframe2.src = 'https://apple.com/iframe2';" completionHandler:nil];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor(
        ^{ return failureCount == 2; }
    ));
}

TEST(SiteIsolation, BasicPostMessageWindowOpen)
{
    auto exampleHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        w.postMessage('pong', '*');"
    "    }, false);"
    "</script>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('opened page received ' + event.data);"
    "    }, false);"
    "</script>"_s;

    __block bool openerFinishedLoading { false };
    __block bool openedFinishedLoading { false };
    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block RetainPtr<WKWebView> openerWebView;
    __block RetainPtr<WKWebView> openedWebView;

    auto openerNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openerNavigationDelegate allowAnyTLSCertificate];
    openerNavigationDelegate.get().didFinishNavigation = ^(WKWebView *opener, WKNavigation *navigation) {
        EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
        checkFrameTreesInProcesses(opener, { { "https://example.com"_s } });
        openerFinishedLoading = true;
    };

    __block auto openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
    [openedNavigationDelegate allowAnyTLSCertificate];
    openedNavigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *navigation) {
        EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://webkit.org/webkit");
        checkFrameTreesInProcesses(openerWebView.get(), { { "https://example.com"_s }, { RemoteFrame } });
        checkFrameTreesInProcesses(openedWebView.get(), { { "https://webkit.org"_s }, { RemoteFrame } });
        auto openerFrames = frameTrees(openerWebView.get());
        auto openedFrames = frameTrees(openedWebView.get());
        EXPECT_NE([openerWebView _webProcessIdentifier], [openedWebView _webProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Remote), [openedWebView _webProcessIdentifier]);
        EXPECT_EQ(findFramePID(openedFrames.get(), FrameType::Remote), [openerWebView _webProcessIdentifier]);
        openedFinishedLoading = true;
    };
    openedNavigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        auto openerFrames = frameTrees(openerWebView.get());
        checkFrameTreesInProcesses(openerFrames.get(), { { "https://example.com"_s }, { RemoteFrame } });
        checkFrameTreesInProcesses(openedWebView.get(), { { "https://example.com"_s } });
        EXPECT_EQ([openerWebView _webProcessIdentifier], [openedWebView _webProcessIdentifier]);
        EXPECT_NE([openedWebView _webProcessIdentifier], [openedWebView _provisionalWebProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Remote), [openedWebView _provisionalWebProcessIdentifier]);
        EXPECT_EQ(findFramePID(openerFrames.get(), FrameType::Local), [openerWebView _webProcessIdentifier]);
        completionHandler(WKNavigationResponsePolicyAllow);
    };

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().UIDelegate = uiDelegate.get();
        openedWebView.get().navigationDelegate = openedNavigationDelegate.get();
        return openedWebView.get();
    };

    openerWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    openerWebView.get().navigationDelegate = openerNavigationDelegate.get();
    openerWebView.get().UIDelegate = uiDelegate.get();
    openerWebView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [openerWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&openerFinishedLoading);

    [openerWebView evaluateJavaScript:@"w = window.open('https://webkit.org/webkit')" completionHandler:nil];

    Util::run(&openedFinishedLoading);

    [openedWebView evaluateJavaScript:@"try { window.opener.postMessage('ping', '*'); } catch(e) { alert('error ' + e) }" completionHandler:nil];

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "opened page received pong");
}

struct WebViewAndDelegates {
    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestMessageHandler> messageHandler;
    RetainPtr<TestNavigationDelegate> navigationDelegate;
    RetainPtr<TestUIDelegate> uiDelegate;
};

static std::pair<WebViewAndDelegates, WebViewAndDelegates> openerAndOpenedViews(const HTTPServer& server, NSString *url = @"https://example.com/example", bool waitForOpenedNavigation = true)
{
    __block WebViewAndDelegates opener;
    __block WebViewAndDelegates opened;
    opener.navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [opener.navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    opener.webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    opener.webView.get().navigationDelegate = opener.navigationDelegate.get();
    opener.uiDelegate = adoptNS([TestUIDelegate new]);
    opener.uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        enableSiteIsolation(configuration);
        opened.webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.navigationDelegate = adoptNS([TestNavigationDelegate new]);
        [opened.navigationDelegate allowAnyTLSCertificate];
        opened.uiDelegate = adoptNS([TestUIDelegate new]);
        opened.webView.get().navigationDelegate = opened.navigationDelegate.get();
        opened.webView.get().UIDelegate = opened.uiDelegate.get();
        return opened.webView.get();
    };
    [opener.webView setUIDelegate:opener.uiDelegate.get()];
    opener.webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [opener.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    while (!opened.webView)
        Util::spinRunLoop();
    if (waitForOpenedNavigation)
        [opened.navigationDelegate waitForDidFinishNavigation];
    return { WTFMove(opener), WTFMove(opened) };
}

TEST(SiteIsolation, NavigationAfterWindowOpen)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/example_opened_after_navigation"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });
    pid_t webKitPid = findFramePID(frameTrees(opener.webView.get()).get(), FrameType::Remote);

    [opened.webView evaluateJavaScript:@"window.location = 'https://example.com/example_opened_after_navigation'" completionHandler:nil];
    [opened.navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://example.com"_s } });

    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
}

// FIXME: Re-enable this test once https://bugs.webkit.org/show_bug.cgi?id=300844 is resolved
#if !defined(NDEBUG)
TEST(SiteIsolation, DISABLED_OpenBeforeInitialLoad)
#else
TEST(SiteIsolation, OpenBeforeInitialLoad)
#endif
{
    HTTPServer server({
        { "/webkit"_s, { "<script>alert('loaded')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    RetainPtr<WKWebView> opened;
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opened = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        opened.get().navigationDelegate = navigationDelegate.get();
        opened.get().UIDelegate = uiDelegate.get();
        return opened.get();
    };
    [webView setUIDelegate:uiDelegate.get()];
    [webView evaluateJavaScript:@"window.open('https://webkit.org/webkit')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "loaded");
}

TEST(SiteIsolation, OpenWithNoopener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit', '_blank', 'noopener')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    __block RetainPtr openerView = opener.webView;
    __block RetainPtr openedView = opened.webView;
    opened.navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^completionHandler)(WKNavigationActionPolicy)) {
        checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
        checkFrameTreesInProcesses(openedView.get(), { { "://"_s } }); // FIXME: This should be https://webkit.org
        EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
        completionHandler(WKNavigationActionPolicyAllow);
    };
    opened.navigationDelegate.get().decidePolicyForNavigationResponse = ^(WKNavigationResponse *, void (^completionHandler)(WKNavigationResponsePolicy)) {
        checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
        checkFrameTreesInProcesses(openedView.get(), { { "://"_s } }); // FIXME: This should be https://webkit.org
        EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
        completionHandler(WKNavigationResponsePolicyAllow);
    };
    [opened.navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(openerView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(openedView.get(), { { "https://webkit.org"_s } });
    EXPECT_NE([openerView _webProcessIdentifier], [openedView _webProcessIdentifier]);
}

TEST(SiteIsolation, PreferencesUpdatesToAllProcesses)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } },
        { "/opened"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = NO;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    __block bool opened { false };
    uiDelegate.get().createWebViewWithConfiguration = ^WKWebView *(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures)
    {
        opened = true;
        return nil;
    };
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"window.open('https://example.com/opened')" inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&opened);
}

TEST(SiteIsolation, ParentOpener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);

    [opened.webView evaluateJavaScript:@"try { opener.postMessage('test1', '*'); alert('posted message 1') } catch(e) { alert(e) }" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "posted message 1");

    [opened.webView evaluateJavaScript:@"try { top.opener.postMessage('test2', '*'); alert('posted message 2') } catch(e) { alert(e) }" inFrame:[opened.webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "posted message 2");
}

TEST(SiteIsolation, LoadStringAfterOpen)
{
    NSString *alertOpener = @"<script>alert(!!window.opener)</script>";
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/apple"_s, { alertOpener } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);

    [opened.webView evaluateJavaScript:@"window.location = 'https://apple.com/apple'" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    [opener.webView evaluateJavaScript:@"w.location = 'https://other.com/apple'" completionHandler:nil];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    [opened.webView loadHTMLString:alertOpener baseURL:[NSURL URLWithString:@"https://example.org/"]];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "true");

    [opened.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.net/apple"]]];
    EXPECT_WK_STREQ([opened.uiDelegate waitForAlert], "false");
}

TEST(SiteIsolation, LoadDuringOpen)
{
    HTTPServer server({
        { "/example"_s, { "window.open('https://webkit.org/webkit')"_s } },
        { "/webkit"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) -> WKWebView * {
        RetainPtr auxiliary = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:[webView configuration]]);
        auxiliary.get().navigationDelegate = navigationDelegate.get();
        [auxiliary loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [navigationDelegate waitForDidFinishNavigation];
        return nil;
    };
    [webView setUIDelegate:uiDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"window.open('https://webkit.org/webkit');alert('done')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "done");
}

TEST(SiteIsolation, WindowOpenRedirect)
{
    HTTPServer server({
        { "/example1"_s, { "<script>w = window.open('https://webkit.org/webkit1')</script>"_s } },
        { "/webkit1"_s, { 302, { { "Location"_s, "/webkit2"_s } }, "redirecting..."_s } },
        { "/webkit2"_s, { "loaded!"_s } },
        { "/example2"_s, { "<script>w = window.open('https://webkit.org/webkit3')</script>"_s } },
        { "/webkit3"_s, { 302, { { "Location"_s, "https://example.com/example3"_s } }, "redirecting..."_s } },
        { "/example3"_s, { "loaded!"_s } },
        { "/example4"_s, { "<script>w = window.open('https://webkit.org/webkit4')</script>"_s } },
        { "/webkit4"_s, { 302, { { "Location"_s, "https://apple.com/apple"_s } }, "redirecting..."_s } },
        { "/apple"_s, { "loaded!"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example1");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://webkit.org/webkit2");
    }
    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example2");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://example.com/example3");
    }
    {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example4");
        EXPECT_WK_STREQ(opened.webView.get().URL.absoluteString, "https://apple.com/apple");
    }
}

void pollUntilOpenedWindowIsClosed(RetainPtr<WKWebView> webView, bool& finished)
{
    [webView evaluateJavaScript:@"openedWindow.closed" completionHandler:makeBlockPtr([webView, &finished](id result, NSError *error) {
        if ([result boolValue])
            finished = true;
        else
            pollUntilOpenedWindowIsClosed(webView, finished);
    }).get()];
}

TEST(SiteIsolation, ClosedStatePropagation)
{
    HTTPServer server({
        { "/example"_s, { "<script>let openedWindow = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    {
        bool openerSawClosedState = false;
        auto [opener, opened] = openerAndOpenedViews(server);
        [opened.webView evaluateJavaScript:@"window.close()" completionHandler:nil];
        pollUntilOpenedWindowIsClosed(opener.webView, openerSawClosedState);
        Util::run(&openerSawClosedState);
    }

    {
        bool openerSawClosedState = false;
        auto [opener, opened] = openerAndOpenedViews(server);
        [opened.webView _close];
        pollUntilOpenedWindowIsClosed(opener.webView, openerSawClosedState);
        Util::run(&openerSawClosedState);
    }
}

TEST(SiteIsolation, CloseAfterWindowOpen)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    pid_t webKitPid = findFramePID(frameTrees(opener.webView.get()).get(), FrameType::Remote);

    EXPECT_FALSE([[opener.webView objectByEvaluatingJavaScript:@"w.closed"] boolValue]);
    [opener.webView evaluateJavaScript:@"w.close()" completionHandler:nil];
    [opened.uiDelegate waitForDidClose];
    [opened.webView _close];
    while (processStillRunning(webKitPid))
        Util::spinRunLoop();
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    EXPECT_TRUE([[opener.webView objectByEvaluatingJavaScript:@"w.closed"] boolValue]);
}

// FIXME: <rdar://117383420> Add a test that deallocates the opened WKWebView without being asked to by JS.
// Check state using native *and* JS APIs. Make sure processes are torn down as expected.
// Same with the opener WKWebView. We would probably need to set remotePageProxyInOpenerProcess
// to null manually to make the process terminate.
//
// Also test when the opener frame (if it's an iframe) is removed from the tree and garbage collected.
// That should probably do some teardown that should be visible from the API.

TEST(SiteIsolation, PostMessageWithMessagePorts)
{
    auto exampleHTML = "<script>"
    "    const channel = new MessageChannel();"
    "    channel.port1.onmessage = function() {"
    "        alert('parent frame received ' + event.data)"
    "    };"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*', [channel.port2]);"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        event.ports[0].postMessage('got port and message ' + event.data);"
    "    }, false)"
    "</script>"_s;

    auto example2HTML = "<script>"
    "    onload = () => {"
    "        const channel = new MessageChannel();"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*', [channel.port2]);"
    "        channel.port1.postMessage('sent message after sending port');"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit2'></iframe>"_s;

    auto webkit2HTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        event.ports[0].onmessage = (e)=>{ alert('port received message ' + event.data); }"
    "    }, false)"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } },
        { "/example2"_s, { example2HTML } },
        { "/webkit2"_s, { webkit2HTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "parent frame received got port and message ping");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example2"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "port received message ping");
}

TEST(SiteIsolation, PostMessageWithNotAllowedTargetOrigin)
{
    auto exampleHTML = "<script>"
    "    onload = () => {"
    "        document.getElementById('webkit_frame').contentWindow.postMessage('ping', 'https://foo.org');"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "    setTimeout(() => { alert('child did not receive message'); }, 1000);"
    "</script>"_s;

    bool finishedLoading { false };
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse(exampleHTML).serialize());
                continue;
            }
            if (path == "/webkit"_s) {
                co_await connection.awaitableSend(HTTPResponse(webkitHTML).serialize());
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "child did not receive message");

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
}

TEST(SiteIsolation, PostMessageToIFrameWithOpaqueOrigin)
{
    auto exampleHTML = "<script>"
    "    onload = () => {"
    "        try {"
    "           document.getElementById('webkit_frame').contentWindow.postMessage('ping', 'data:');"
    "        } catch (error) {"
    "           alert(error);"
    "        }"
    "    }"
    "</script>"
    "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s;

    auto webkitHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "</script>"_s;

    bool finishedLoading { false };
    
    HTTPServer server({
        { "/example"_s, { exampleHTML } },
        { "/webkit"_s, { webkitHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().didFinishNavigation = makeBlockPtr([&](WKWebView *, WKNavigation *navigation) {
        if (navigation._request) {
            EXPECT_WK_STREQ(navigation._request.URL.absoluteString, "https://example.com/example");
            finishedLoading = true;
        }
    }).get();

    __block RetainPtr<NSString> alert;
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alert = message;
        completionHandler();
    };

    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&finishedLoading);

    while (!alert)
        Util::spinRunLoop();
    EXPECT_WK_STREQ(alert.get(), "SyntaxError: The string did not match the expected pattern.");

    auto mainFrame = [webView mainFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = mainFrame.childFrames.firstObject.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
}

TEST(SiteIsolation, QueryFramesStateAfterNavigating)
{
    HTTPServer server({
        { "/page1.html"_s, { "<iframe src='subframe1.html'></iframe><iframe src='subframe2.html'></iframe><iframe src='subframe3.html'></iframe>"_s } },
        { "/page2.html"_s, { "<iframe src='subframe4.html'></iframe>"_s } },
        { "/subframe1.html"_s, { "SubFrame1"_s } },
        { "/subframe2.html"_s, { "SubFrame2"_s } },
        { "/subframe3.html"_s, { "SubFrame3"_s } },
        { "/subframe4.html"_s, { "SubFrame4"_s } }
    }, HTTPServer::Protocol::Http);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    [webView synchronouslyLoadRequest:server.request("/page1.html"_s)];
    EXPECT_EQ(3u, [webView mainFrame].childFrames.count);

    [webView synchronouslyLoadRequest:server.request("/page2.html"_s)];
    EXPECT_EQ(1u, [webView mainFrame].childFrames.count);
}

TEST(SiteIsolation, NavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://example.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
}

TEST(SiteIsolation, ParentNavigatingCrossOriginIframeToSameOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://example.com/example_subframe' }</script>"_s } },
        { "/example_subframe"_s, { "<script>onload = ()=>{ alert('done') }</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });
}

TEST(SiteIsolation, IframeNavigatesSelfWithoutChangingOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>window.location='/webkit_second'</script>"_s } },
        { "/webkit_second"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, IframeWithConfirm)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>confirm('confirm message')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForConfirm], "confirm message");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, IframeWithPrompt)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>prompt('prompt message', 'default input')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForPromptWithReply:@"default input"], "prompt message");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, GrandchildIframe)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<iframe onload='alert(\"grandchild loaded successfully\")' srcdoc=\"<script>window.location='https://apple.com/apple'</script>\">"_s } },
        { "/apple"_s, { ""_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "grandchild loaded successfully");
}

TEST(SiteIsolation, GrandchildIframeSameOriginAsGrandparent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<iframe src='https://example.com/example_grandchild'></iframe>\">"_s } },
        { "/example_grandchild"_s, { "<script>alert('grandchild loaded successfully')</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "grandchild loaded successfully");
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame, { { "https://example.com"_s } } } } },
        { RemoteFrame, { { "https://webkit.org"_s, { { RemoteFrame } } } } }
    });
}

TEST(SiteIsolation, ChildNavigatingToNewDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://foo.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "foo.com");
}

TEST(SiteIsolation, ChildNavigatingToMainFrameDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://example.com/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
}

TEST(SiteIsolation, ChildNavigatingToSameDomain)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<script>window.location='https://webkit.org/example_subframe'</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, ChildNavigatingToDomainLoadedOnADifferentPage)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('done')</script>"_s } },
        { "/foo"_s, { "<iframe id='foo'><html><body><p>Hello world.</p></body></html></iframe>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [firstWebView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [firstWebView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/foo"]]];
    
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:firstWebView.get().configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto firstWebViewMainFrame = [firstWebView mainFrame];
    EXPECT_NE(firstWebViewMainFrame.info._processIdentifier, 0);
    pid_t firstFramePID = firstWebViewMainFrame.info._processIdentifier;
    EXPECT_WK_STREQ(firstWebViewMainFrame.info.securityOrigin.host, "webkit.org");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_NE(firstFramePID, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, MainFrameWithTwoIFramesInTheSameProcess)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame_1' src='https://webkit.org/a'></iframe><iframe id='webkit_frame_2' src='https://webkit.org/b'></iframe>"_s } },
        { "/a"_s, { "<script>alert('donea')</script>"_s } },
        { "/b"_s, { "<script>alert('doneb')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    NSString* alert1 = [webView _test_waitForAlert];
    NSString* alert2 = [webView _test_waitForAlert];
    if ([alert1 isEqualToString:@"donea"])
        EXPECT_WK_STREQ(alert2, "doneb");
    else if ([alert1 isEqualToString:@"doneb"])
        EXPECT_WK_STREQ(alert2, "donea");
    else
        EXPECT_TRUE(false);

    auto mainFrame = [webView mainFrame];
    EXPECT_EQ(mainFrame.childFrames.count, 2u);
    _WKFrameTreeNode *childFrame = mainFrame.childFrames.firstObject;
    _WKFrameTreeNode *otherChildFrame = mainFrame.childFrames[1];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    pid_t otherChildFramePid = otherChildFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(otherChildFramePid, 0);
    EXPECT_EQ(childFramePid, otherChildFramePid);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "webkit.org");
    EXPECT_WK_STREQ(otherChildFrame.info.securityOrigin.host, "webkit.org");
}

TEST(SiteIsolation, ChildBeingNavigatedToMainFrameDomainByParent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto mainFrame = [webView mainFrame];
    auto childFrame = [webView firstChildFrame];
    EXPECT_NE(mainFrame.info._processIdentifier, childFrame._processIdentifier);

    [webView evaluateJavaScript:@"document.getElementById('webkit_frame').src = 'https://example.com/example_subframe'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });

    while (processStillRunning(childFrame._processIdentifier))
        Util::spinRunLoop();
}

TEST(SiteIsolation, ChildBeingNavigatedToSameDomainByParent)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://webkit.org/example_subframe' }</script>"_s } },
        { "/example_subframe"_s, { "<script>alert('done')</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");

    auto mainFrame = [webView mainFrame];
    auto childFrame = [webView firstChildFrame];
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.securityOrigin.host, "webkit.org");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, ChildBeingNavigatedToNewDomainByParent)
{
    auto appleHTML = "<script>"
        "window.addEventListener('message', (event) => {"
        "    parent.window.postMessage(event.data + 'pong', { 'targetOrigin' : '*' });"
        "}, false);"
        "alert('apple iframe loaded')"
        "</script>"_s;

    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe><script>onload = () => { document.getElementById('webkit_frame').src = 'https://apple.com/apple' }</script>"_s } },
        { "/webkit"_s, { "<html></html>"_s } },
        { "/apple"_s, { appleHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "apple iframe loaded");

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://apple.com"_s } }
        }
    });

    NSString *jsCheckingPostMessageRoundTripAfterIframeProcessChange = @""
    "window.addEventListener('message', (event) => {"
    "    alert('parent frame received ' + event.data)"
    "}, false);"
    "document.getElementById('webkit_frame').contentWindow.postMessage('ping', '*');";
    [webView evaluateJavaScript:jsCheckingPostMessageRoundTripAfterIframeProcessChange completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "parent frame received pingpong");
}

TEST(SiteIsolation, IframeRedirectSameSite)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { 302, { { "Location"_s, "https://www.webkit.org/www_webkit"_s } }, "redirecting..."_s } },
        { "/www_webkit"_s, { "arrived!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://www.webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, IframeRedirectCrossSite)
{
    HTTPServer server({
        { "/example1"_s, { "<iframe src='https://webkit.org/webkit1'></iframe>"_s } },
        { "/webkit1"_s, { 302, { { "Location"_s, "https://apple.com/apple1"_s } }, "redirecting..."_s } },
        { "/apple1"_s, { "arrived!"_s } },
        { "/example2"_s, { "<iframe src='https://webkit.org/webkit2'></iframe>"_s } },
        { "/webkit2"_s, { 302, { { "Location"_s, "https://webkit.org/webkit3"_s } }, "redirecting..."_s } },
        { "/webkit3"_s, { 302, { { "Location"_s, "https://example.com/example3"_s } }, "redirecting..."_s } },
        { "/example3"_s, { "arrived!"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://apple.com"_s } }
        }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example2"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });
}

TEST(SiteIsolation, CrossOriginOpenerPolicy)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { { { "Content-Type"_s, "text/html"_s }, { "Cross-Origin-Opener-Policy"_s, "same-origin"_s } }, "iframe content"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
    [webView waitForNextPresentationUpdate];
}

static void testCrossOriginOpenerPolicyMainFrame(bool useSharedProcess)
{
    HTTPServer server({
        { "/example"_s, { { { "cross-origin-opener-policy"_s, "same-origin-allow-popups"_s } }, "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "iframe content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<TestWKWebView> webView;
    RetainPtr<TestNavigationDelegate> navigationDelegate;

    if (useSharedProcess)
        std::tie(webView, navigationDelegate) = siteIsolatedViewWithSharedProcess(server);
    else
        std::tie(webView, navigationDelegate) = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CrossOriginOpenerPolicyMainFrame)
{
    testCrossOriginOpenerPolicyMainFrame(false);
}

TEST(SiteIsolation, CrossOriginOpenerPolicyMainFrameWithSharedProcess)
{
    testCrossOriginOpenerPolicyMainFrame(true);
}

TEST(SiteIsolation, NavigationWithIFrames)
{
    HTTPServer server({
        { "/1"_s, { "<iframe src='https://domain2.com/2'></iframe>"_s } },
        { "/2"_s, { "hi!"_s } },
        { "/3"_s, { "<iframe src='https://domain4.com/4'></iframe>"_s } },
        { "/4"_s, { "<iframe src='https://domain5.com/5'></iframe>"_s } },
        { "/5"_s, { "hi!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/1"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/3"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain3.com"_s, { { RemoteFrame, { { RemoteFrame } } } } },
        { RemoteFrame, { { "https://domain4.com"_s, { { RemoteFrame } } } } },
        { RemoteFrame, { { RemoteFrame, { { "https://domain5.com"_s } } } } }
    });

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://domain1.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://domain2.com"_s } } }
    });
}

TEST(SiteIsolation, RemoveFrames)
{
    HTTPServer server({
        { "/webkit_main"_s, { "<iframe src='https://webkit.org/webkit_iframe' id='wk'></iframe><iframe src='https://example.com/example_iframe' id='ex'></iframe>"_s } },
        { "/webkit_iframe"_s, { "hi!"_s } },
        { "/example_iframe"_s, { "<iframe src='example_grandchild_frame'></iframe>"_s } },
        { "/example_grandchild_frame"_s, { "hi!"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit_main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { "https://webkit.org"_s }, { RemoteFrame, { { RemoteFrame } } } }
        }, { RemoteFrame,
            { { RemoteFrame }, { "https://example.com"_s, { { "https://example.com"_s } } } }
        }
    });

    __block bool removedLocalFrame { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('wk');frame.parentNode.removeChild(frame);" completionHandler:^(id, NSError *error) {
        removedLocalFrame = true;
    }];
    Util::run(&removedLocalFrame);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame, { { RemoteFrame } } } }
        }, { RemoteFrame,
            { { "https://example.com"_s, { { "https://example.com"_s } } } }
        }
    });

    __block bool removedRemoteFrame { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('ex');frame.parentNode.removeChild(frame);" completionHandler:^(id, NSError *error) {
        removedRemoteFrame = true;
    }];
    Util::run(&removedRemoteFrame);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s }
    });
}

TEST(SiteIsolation, RemoveFrameFromRemoteFrame)
{
    HTTPServer server({
        { "/main"_s, { "<iframe src='https://webkit.org/child'></iframe>"_s } },
        { "/child"_s, { "<iframe src='https://example.com/grandchild' id=grandchildframe></iframe>"_s } },
        { "/grandchild"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame, { { "https://example.com"_s } } } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s, { { RemoteFrame } } } }
        }
    });

    [webView objectByEvaluatingJavaScript:@"grandchildframe.parentNode.removeChild(grandchildframe);1" inFrame:[webView firstChildFrame]];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        }
    });
}

TEST(SiteIsolation, ProvisionalLoadFailure)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } },
        { "/apple"_s,  { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { "https://example.com"_s } } }
    });

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');document.body.appendChild(iframe);iframe.src = 'https://apple.com/apple'" completionHandler:nil];
    Vector<ExpectedFrameTree> expectedFrameTreesAfterAddingApple { {
        "https://example.com"_s, { { "https://example.com"_s }, { RemoteFrame } }
    }, {
        RemoteFrame, { { RemoteFrame }, { "https://apple.com"_s } }
    } };
    while (!frameTreesMatch(frameTrees(webView.get()).get(), Vector<ExpectedFrameTree> { expectedFrameTreesAfterAddingApple }))
        Util::spinRunLoop();

    [webView evaluateJavaScript:@"iframe.onload = alert('done');iframe.src = 'https://webkit.org/webkit'" completionHandler:nil];

    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");
    checkFrameTreesInProcesses(webView.get(), WTFMove(expectedFrameTreesAfterAddingApple));
}

TEST(SiteIsolation, MultipleReloads)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s,  { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });

    [webView reload];
    Util::runFor(0.1_s);
    [webView reload];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });
}

#if PLATFORM(MAC)
TEST(SiteIsolation, GeneratesPageLoadTimingAfterOnUnloadFetch)
{
    // FIXME: this is a bit more convoluted than it needs to be due to didGeneratePageLoadTiming not working unless the page has at least one subresource load.
    HTTPServer server({
        { "/1"_s, { "<iframe src='https://example.com/iframe'></iframe><script src='/script'></script> 1"_s } },
        { "/2"_s, { "<iframe src='https://example.com/iframe'></iframe><script src='/script'></script> 2"_s } },
        { "/iframe"_s, { "<script>window.addEventListener('unload', () => fetch('/foobar', { keepalive: true }))</script> <script src='/script'></script>"_s } },
        { "/foobar"_s, { "foobar"_s } },
        { "/script"_s, { "console.log('hello from script')"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool didGeneratePageLoadTiming = false;

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 400));
    navigationDelegate.get().didGeneratePageLoadTiming = ^(_WKPageLoadTiming *timing) {
        if (timing)
            didGeneratePageLoadTiming = true;
    };

    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400) styleMask:0 backing:NSBackingStoreBuffered defer:NO]);
    [window.get().contentView addSubview:webView.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/1"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://example.com"_s } } }
    });

    for (int i = 0; i < 20 && !didGeneratePageLoadTiming; i++)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE(didGeneratePageLoadTiming);
    didGeneratePageLoadTiming = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/2"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://example.com"_s } } }
    });

    for (int i = 0; i < 20 && !didGeneratePageLoadTiming; i++)
        TestWebKitAPI::Util::runFor(0.1_s);

    EXPECT_TRUE(didGeneratePageLoadTiming);
}

TEST(SiteIsolation, PropagateMouseEventsToSubframe)
{
    auto mainframeHTML = "<script>"
    "    window.eventTypes = [];"
    "    window.addEventListener('message', function(event) {"
    "        window.eventTypes.push(event.data);"
    "    });"
    "</script>"
    "<iframe src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<script>"
    "    addEventListener('mousemove', (event) => { window.parent.postMessage('mousemove', '*') });"
    "    addEventListener('mousedown', (event) => { window.parent.postMessage('mousedown,' + event.pageX + ',' + event.pageY, '*') });"
    "    addEventListener('mouseup', (event) => { window.parent.postMessage('mouseup,' + event.pageX + ',' + event.pageY, '*') });"
    "    alert('iframe loaded');"
    "</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    EXPECT_WK_STREQ("iframe loaded", [webView _test_waitForAlert]);

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(50, 50) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];

    NSArray<NSString *> *eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    while (eventTypes.count != 3u)
        eventTypes = [webView objectByEvaluatingJavaScript:@"window.eventTypes"];
    EXPECT_WK_STREQ("mousemove", eventTypes[0]);
    EXPECT_WK_STREQ("mousedown,40,40", eventTypes[1]);
    EXPECT_WK_STREQ("mouseup,40,40", eventTypes[2]);
}

TEST(SiteIsolation, RunOpenPanel)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html><input style='width: 100vw; height: 100vh;' type='file'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    __block bool fileSelected = false;
    [uiDelegate setRunOpenPanelWithParameters:^(WKWebView *, WKOpenPanelParameters *, WKFrameInfo *, void (^completionHandler)(NSArray<NSURL *> *)) {
        fileSelected = true;
        completionHandler(@[ [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"test"]] ]);
    }];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(100, 100) toView:nil];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];
    Util::run(&fileSelected);

    NSString *js = @"function f() { try { return document.getElementsByTagName('input')[0].files[0].name } catch (e) { return 'exception: ' + e; } }; f()";
    while (![[webView objectByEvaluatingJavaScript:js inFrame:[webView firstChildFrame]] isEqualToString:@"test"])
        Util::spinRunLoop();
}

// FIXME when rdar://163227871 is resolved.
#if PLATFORM(MAC)
TEST(SiteIsolation, DISABLED_CancelOpenPanel)
#else
TEST(SiteIsolation, CancelOpenPanel)
#endif
{
    auto subframeHTML = "<!DOCTYPE html><input style='width: 100vw; height: 100vh;' id='file' type='file'>"
        "<script>"
        "document.getElementById('file').addEventListener('cancel', () => { alert('cancel'); });"
        "</script>"_s;
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://b.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    [uiDelegate setRunOpenPanelWithParameters:^(WKWebView *, WKOpenPanelParameters *, WKFrameInfo *, void (^completionHandler)(NSArray<NSURL *> *)) {
        completionHandler(nil);
    }];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(100, 100) toView:nil];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "cancel");
}

TEST(SiteIsolation, DragEvents)
{
    auto mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue;'></div>"
    "<script>"
    "    draggable.addEventListener('dragstart', (event) => { window.parent.postMessage('dragstart', '*') });"
    "    draggable.addEventListener('dragend', (event) => { window.parent.postMessage('dragend', '*') });"
    "    draggable.addEventListener('dragenter', (event) => { window.parent.postMessage('dragenter:' + event.clientX + ',' + event.clientY, '*') });"
    "    draggable.addEventListener('dragleave', (event) => { window.parent.postMessage('dragleave', '*') });"
    "    addEventListener('dragover', (event) => { window.parent.postMessage('dragover:' + event.clientX + ',' + event.clientY, '*') });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration]);
    RetainPtr webView = [simulator webView];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 150)];

    NSArray<NSString *> *events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_GT(events.count, 4U);

    bool foundDragLeave = false;
    for (NSString *event in events) {
        if ([event hasPrefix:@"dragleave"]) {
            foundDragLeave = true;
            break;
        }
    }
    EXPECT_WK_STREQ("dragstart", events[0]);
    EXPECT_TRUE(foundDragLeave);
    EXPECT_WK_STREQ("dragend", events[events.count - 1]);

    NSString *dragenterEvent = events[1];
    EXPECT_TRUE([dragenterEvent hasPrefix:@"dragenter:"]);
    NSString *dragenterCoords = [dragenterEvent substringFromIndex:[@"dragenter:" length]];
    NSArray *dragenterComponents = [dragenterCoords componentsSeparatedByString:@","];
    if (dragenterComponents.count == 2) {
        int x = [dragenterComponents[0] intValue];
        int y = [dragenterComponents[1] intValue];
        EXPECT_TRUE(x >= 65 && x <= 75) << "Expected dragenter x coordinate around 71, got " << x;
        EXPECT_TRUE(y >= 65 && y <= 75) << "Expected dragenter y coordinate around 71, got " << y;
    }

    NSString *lastDragOverEvent = nil;
    for (NSString *event in events) {
        if ([event hasPrefix:@"dragover:"])
            lastDragOverEvent = event;
    }
    EXPECT_NOT_NULL(lastDragOverEvent);
    if (lastDragOverEvent) {
        NSString *dragoverCoords = [lastDragOverEvent substringFromIndex:[@"dragover:" length]];
        NSArray *dragoverComponents = [dragoverCoords componentsSeparatedByString:@","];
        if (dragoverComponents.count == 2) {
            int x = [dragoverComponents[0] intValue];
            int y = [dragoverComponents[1] intValue];
            EXPECT_TRUE(x >= 135 && x <= 145) << "Expected final dragover x coordinate around 140, got " << x;
            EXPECT_TRUE(y >= 135 && y <= 145) << "Expected final dragover y coordinate around 140, got " << y;
        }
    }
}

TEST(SiteIsolation, FrameMetrics)
{
    auto mainframeHTML = "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div>"
    "Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc.Lots and lots of content in this div. Let's just keep going and going and going. Lazy brown foxes, etc etc etc."
    "</div>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr frame = [webView mainFrame];
    WKFrameInfo *info = frame.get().info;

    EXPECT_EQ(info._isScrollable, NO);
    EXPECT_EQ(info._contentSize.width, 800);
    EXPECT_EQ(info._contentSize.height, 600);
    EXPECT_TRUE(CGSizeEqualToSize(info._contentSize, info._visibleContentSize));
    EXPECT_TRUE(CGSizeEqualToSize(info._contentSize, info._visibleContentSizeExcludingScrollbars));

    frame = frame.get().childFrames.firstObject;
    info = frame.get().info;

    EXPECT_EQ(info._isScrollable, YES);
    EXPECT_EQ(info._visibleContentSize.width, 300);
    EXPECT_EQ(info._visibleContentSize.height, 300);
    EXPECT_EQ(info._visibleContentSize.height, info._visibleContentSizeExcludingScrollbars.height);
    EXPECT_EQ(info._visibleContentSizeExcludingScrollbars.width, info._contentSize.width);
    EXPECT_TRUE(info._visibleContentSizeExcludingScrollbars.width < info._visibleContentSize.width);
}

void writeImageDataToPasteboard(NSString *type, NSData *data)
{
    [NSPasteboard.generalPasteboard declareTypes:@[type] owner:nil];
    [NSPasteboard.generalPasteboard setData:data forType:type];
}

TEST(SiteIsolation, PasteGIF)
{
    auto mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' src='https://domain2.com/subframe'></iframe>"_s;

    auto subframeHTML = "<body>"
    "<div id='editor' contenteditable style=\"height:100%; width: 100%;\"></div>"
    "<script>"
    "const editor = document.getElementById('editor');"
    "editor.focus();"
    "editor.addEventListener('paste', (event) => { window.parent.postMessage(event.clipboardData.files[0].name, '*'); });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    CGPoint eventLocationInWindow = [webView convertPoint:CGPointMake(20, 20) toView:nil];
    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    auto *data = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"sunset-in-cupertino-400px" ofType:@"gif"]];
    writeImageDataToPasteboard(UTTypeGIF.identifier, data);
    [webView paste:nil];

    [webView mouseEnterAtPoint:eventLocationInWindow];
    [webView mouseMoveToPoint:eventLocationInWindow withFlags:0];
    [webView mouseDownAtPoint:eventLocationInWindow simulatePressure:NO];
    [webView mouseUpAtPoint:eventLocationInWindow];
    [webView waitForPendingMouseEvents];

    NSArray<NSString *> *events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_EQ(1U, events.count);
    EXPECT_WK_STREQ("image.gif", events[0]);
}

#endif

TEST(SiteIsolation, ShutDownFrameProcessesAfterNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hello"_s } },
        { "/apple"_s, { "hello"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t iframePID = findFramePID(frameTrees(webView.get()).get(), FrameType::Remote);
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s, { { RemoteFrame } } },
        { RemoteFrame, { { "https://webkit.org"_s } } }
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), { { "https://apple.com"_s } });

    while (processStillRunning(iframePID))
        Util::spinRunLoop();
}

TEST(SiteIsolation, OpenerProcessSharing)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/opener_iframe'></iframe>"_s } },
        { "/opened"_s, { "<iframe src='https://webkit.org/opened_iframe'></iframe>"_s } },
        { "/opener_iframe"_s, { "hello"_s } },
        { "/opened_iframe"_s, { "<script>alert('done')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server);

    __block RetainPtr<TestWKWebView> openedWebView;
    __block auto uiDelegate = adoptNS([TestUIDelegate new]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        static auto openedNavigationDelegate = adoptNS([TestNavigationDelegate new]);
        [openedNavigationDelegate allowAnyTLSCertificate];
        openedWebView.get().navigationDelegate = openedNavigationDelegate.get();
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"w = window.open('/opened')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "done");

    auto openerMainFrame = [webView mainFrame];
    auto openedMainFrame = [openedWebView mainFrame];
    pid_t openerMainFramePid = openerMainFrame.info._processIdentifier;
    pid_t openedMainFramePid = openedMainFrame.info._processIdentifier;
    pid_t openerIframePid = openerMainFrame.childFrames.firstObject.info._processIdentifier;
    pid_t openedIframePid = openedMainFrame.childFrames.firstObject.info._processIdentifier;

    EXPECT_EQ(openerMainFramePid, openedMainFramePid);
    EXPECT_NE(openerMainFramePid, 0);
    EXPECT_EQ(openerIframePid, openedIframePid);
    EXPECT_NE(openerIframePid, 0);
}

#if PLATFORM(MAC)
TEST(SiteIsolation, AppKitText)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<html><body><input id='input' value='test'></input></body></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrameInfo = [webView firstChildFrame];
    auto textLocation = NSMakePoint(23, 564);
    while ("TEST"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView uppercaseWord:nil];
        Util::runFor(10_ms);
    }
    while ("test"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView lowercaseWord:nil];
        Util::runFor(10_ms);
    }
    while ("Test"_s != String([webView stringByEvaluatingJavaScript:@"input.value" inFrame:childFrameInfo.get()])) {
        [webView sendClickAtPoint:textLocation];
        Util::runFor(10_ms);
        [webView capitalizeWord:nil];
        Util::runFor(10_ms);
    }
}
#endif

TEST(SiteIsolation, SetFocusedFrame)
{
    auto mainframeHTML = "<iframe id='iframe' src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView mainFrame].info._isFocused);
    EXPECT_FALSE([webView firstChildFrame]._isFocused);

    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while ([webView mainFrame].info._isFocused || ![webView firstChildFrame]._isFocused)
        Util::spinRunLoop();

    [webView evaluateJavaScript:@"window.focus()" completionHandler:nil];
    while (![webView mainFrame].info._isFocused || [webView firstChildFrame]._isFocused)
        Util::spinRunLoop();
}

TEST(SiteIsolation, EvaluateJavaScriptInFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<script>test = 'abc';</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_WK_STREQ("abc", [webView objectByEvaluatingJavaScript:@"window.test" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, MainFrameURLAfterFragmentNavigation)
{
    NSString *json = @"["
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"blocked_when_fragment_in_top_url\", \"if-top-url\":[\"fragment\"]}},"
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"always_blocked\", \"if-top-url\":[\"http\"]}}"
    "]";
    __block bool doneRemoving { false };
    [WKContentRuleListStore.defaultStore removeContentRuleListForIdentifier:@"Identifier" completionHandler:^(NSError *error) {
        doneRemoving = true;
    }];
    Util::run(&doneRemoving);
    __block RetainPtr<WKContentRuleList> list;
    [WKContentRuleListStore.defaultStore compileContentRuleListForIdentifier:@"Identifier" encodedContentRuleList:json completionHandler:^(WKContentRuleList *ruleList, NSError *error) {
        list = ruleList;
    }];
    while (!list)
        Util::spinRunLoop();

    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/blocked_when_fragment_in_top_url"_s, { "loaded successfully"_s } },
        { "/always_blocked"_s, { "loaded successfully"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView.get().configuration.userContentController addContentRuleList:list.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto canLoadURLInIFrame = [childFrame = RetainPtr { [webView firstChildFrame] }, webView = RetainPtr { webView }] (NSString *path) -> bool {
        __block std::optional<bool> loadedSuccessfully;
        [webView callAsyncJavaScript:[NSString stringWithFormat:@"try { let response = await fetch('%@'); return await response.text() } catch (e) { return 'load failed' }", path] arguments:nil inFrame:childFrame.get() inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *error) {
            if ([result isEqualToString:@"loaded successfully"])
                loadedSuccessfully = true;
            else if ([result isEqualToString:@"load failed"])
                loadedSuccessfully = false;
            else
                EXPECT_FALSE(true);
        }];
        while (!loadedSuccessfully)
            Util::spinRunLoop();
        return *loadedSuccessfully;
    };
    EXPECT_TRUE(canLoadURLInIFrame(@"/blocked_when_fragment_in_top_url"));
    EXPECT_FALSE(canLoadURLInIFrame(@"/always_blocked"));

    [webView evaluateJavaScript:@"window.location = '#fragment'" completionHandler:nil];
    while (![webView.get().URL.fragment isEqualToString:@"fragment"])
        Util::spinRunLoop();

    EXPECT_FALSE(canLoadURLInIFrame(@"/blocked_when_fragment_in_top_url"));
    EXPECT_FALSE(canLoadURLInIFrame(@"/always_blocked"));
}

TEST(SiteIsolation, LoadRequestOnOpenerWebView)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    [opener.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/webkit"]]];
    [opener.navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://apple.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://webkit.org"_s } });
}

TEST(SiteIsolation, LoadRequestOnOpenedWebView)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    [opened.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/webkit"]]];
    [opened.navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://apple.com"_s } });
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
}

TEST(SiteIsolation, FocusOpenedWindow)
{
    auto openerHTML = "<script>"
    "    let w = window.open('https://domain2.com/opened');"
    "</script>"_s;
    HTTPServer server({
        { "/example"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    EXPECT_FALSE([[[opener.webView mainFrame] info] _isFocused]);
    EXPECT_FALSE([[[opened.webView mainFrame] info] _isFocused]);

    [opener.webView.get() evaluateJavaScript:@"w.focus()" completionHandler:nil];
    while (![[[opened.webView mainFrame] info] _isFocused])
        Util::spinRunLoop();
    EXPECT_FALSE([[[opener.webView mainFrame] info] _isFocused]);
}

#if PLATFORM(MAC)
TEST(SiteIsolation, OpenedWindowFocusDelegates)
{
    auto openerHTML = "<script>"
        "    let w = window.open('https://domain2.com/opened');"
        "</script>"_s;
    HTTPServer server({
        { "/example"_s, { openerHTML } },
        { "/opened"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);

    __block bool calledFocusWebView = false;
    [opened.uiDelegate setFocusWebView:^(WKWebView *viewToFocus) {
        calledFocusWebView = true;
    }];

    __block bool calledUnfocusWebView = false;
    [opened.uiDelegate setUnfocusWebView:^(WKWebView *viewToFocus) {
        calledUnfocusWebView = true;
    }];

    [opener.webView.get() evaluateJavaScript:@"w.focus()" completionHandler:nil];
    Util::run(&calledFocusWebView);

    [opener.webView.get() evaluateJavaScript:@"w.blur()" completionHandler:nil];
    Util::run(&calledUnfocusWebView);
}
#endif

TEST(SiteIsolation, FindStringInFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    EXPECT_FALSE([[webView findStringAndWait:@"Missing string" withConfiguration:findConfiguration.get()] matchFound]);
}

TEST(SiteIsolation, FindStringInNestedFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nested_subframe'></iframe>"_s } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    EXPECT_FALSE([[webView findStringAndWait:@"Missing string" withConfiguration:findConfiguration.get()] matchFound]);
}

TEST(SiteIsolation, FindStringSelection)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionWithEmptyFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/empty_subframe'></iframe>"
        "<iframe src='https://domain4.com/subframe'></iframe>"
        "<iframe src='https://domain5.com/empty_subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } },
        { "/empty_subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[2].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionNoWrap)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    findConfiguration.get().wraps = NO;
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        [[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound];
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
    };

    std::array<SelectionOffsets, 3> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionBackwards)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    findConfiguration.get().backwards = YES;
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
}

TEST(SiteIsolation, FindStringSelectionSameOriginFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionNestedFrames)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    auto subframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain4.com/nested_subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 5>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[3].first endOffset:offsets[3].second inFrame:mainFrame.childFrames[0].childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[4].first endOffset:offsets[4].second inFrame:mainFrame.childFrames[1].childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 11 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionMultipleMatchesInMainFrame)
{
    auto mainframeHTML = "<p>Hello world Hello world Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 12, 23 }, { 0, 0 } } },
        { { { 24, 35 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionMultipleMatchesInChildFrame)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world Hello world Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 2>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
    };

    std::array<SelectionOffsets, 5> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 } } },
        { { { 0, 0 }, { 12, 23 } } },
        { { { 0, 0 }, { 24, 35 } } },
        { { { 0, 11 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringSelectionSameOriginFrameBeforeWrap)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // FIXME(267907): If the iframe is not added like this the UI process and web processes may have mismatched frame trees.
    auto addFrameToBody = @"let frame = document.createElement('iframe');"
    "frame.setAttribute('src', 'https://domain1.com/subframe');"
    "document.body.appendChild(frame);";
    __block bool done = false;
    [webView evaluateJavaScript:addFrameToBody completionHandler:^(id _Nullable, NSError * _Nullable error) {
        done = true;
    }];
    Util::run(&done);

    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    using SelectionOffsets = std::array<std::pair<int, int>, 3>;
    auto findStringAndValidateResults = [&findConfiguration](TestWKWebView *webView, const SelectionOffsets& offsets) {
        EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
        auto mainFrame = [webView mainFrame];
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[0].first endOffset:offsets[0].second inFrame:mainFrame.info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[1].first endOffset:offsets[1].second inFrame:mainFrame.childFrames[0].info]);
        EXPECT_TRUE([webView selectionRangeHasStartOffset:offsets[2].first endOffset:offsets[2].second inFrame:mainFrame.childFrames[1].info]);
    };

    std::array<SelectionOffsets, 4> selectionOffsetsForFrames = { {
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 11 }, { 0, 0 } } },
        { { { 0, 0 }, { 0, 0 }, { 0, 11 } } },
        { { { 0, 11 }, { 0, 0 }, { 0, 0 } } }
    } };
    for (auto& offsets : selectionOffsetsForFrames)
        findStringAndValidateResults(webView.get(), offsets);
    findConfiguration.get().backwards = YES;
    for (auto it = selectionOffsetsForFrames.rbegin() + 1; it != selectionOffsetsForFrames.rend(); ++it)
        findStringAndValidateResults(webView.get(), *it);
}

TEST(SiteIsolation, FindStringMatchCount)
{
    auto mainframeHTML = "<p>Hello world</p>"
        "<iframe src='https://domain2.com/subframe'></iframe>"
        "<iframe src='https://domain3.com/subframe'></iframe>"_s;
    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    auto findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE([[webView findStringAndWait:@"Hello World" withConfiguration:findConfiguration.get()] matchFound]);
    EXPECT_EQ(3ul, [findDelegate matchesCount]);
}

TEST(SiteIsolation, CountStringMatches)
{
    HTTPServer server({
        { "/mainframe"_s, { "<p>Hello world</p><iframe src='https://webkit.org/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    auto findConfiguration = adoptNS([[WKFindConfiguration alloc] init]);
    auto findDelegate = adoptNS([[WKWebViewFindStringFindDelegate alloc] init]);
    [webView _setFindDelegate:findDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView _countStringMatches:@"Hello world" options:0 maxCount:100];
    while ([findDelegate matchesCount] != 2)
        Util::spinRunLoop();
}

#if PLATFORM(MAC)
TEST(SiteIsolation, ProcessDisplayNames)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "<script></script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    __block bool done { false };
    [webView.get().configuration.websiteDataStore removeDataOfTypes:WKWebsiteDataStore.allWebsiteDataTypes modifiedSince:NSDate.distantPast completionHandler:^{
        done = true;
    }];
    Util::run(&done);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    pid_t mainFramePID { 0 };
    pid_t iframePID { 0 };
    auto trees = frameTrees(webView.get());
    EXPECT_EQ([trees count], 2u);
    for (_WKFrameTreeNode *tree in trees.get()) {
        if (tree.info._isLocalFrame)
            mainFramePID = tree.info._processIdentifier;
        else if (tree.childFrames.count)
            iframePID = tree.childFrames[0].info._processIdentifier;
    }
    EXPECT_NE(mainFramePID, iframePID);
    EXPECT_NE(mainFramePID, 0);
    EXPECT_NE(iframePID, 0);

    done = false;
    WKProcessPool *pool = webView.get().configuration.processPool;
    [pool _getActivePagesOriginsInWebProcessForTesting:mainFramePID completionHandler:^(NSArray<NSString *> *result) {
        EXPECT_EQ(result.count, 1u);
        EXPECT_WK_STREQ(result[0], "https://example.com");
        done = true;
    }];
    Util::run(&done);

    done = false;
    [pool _getActivePagesOriginsInWebProcessForTesting:iframePID completionHandler:^(NSArray<NSString *> *result) {
        EXPECT_EQ(result.count, 1u);
        EXPECT_WK_STREQ(result[0], "https://apple.com");
        done = true;
    }];
    Util::run(&done);
}
#endif

TEST(SiteIsolation, NavigateOpener)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/webkit2"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    [opened.webView evaluateJavaScript:@"const originalOpener = window.opener;" completionHandler:nil];
    [opened.webView evaluateJavaScript:@"opener.location = '/webkit2'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://webkit.org"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://webkit.org"_s } });

    __block bool done { false };
    [opened.webView evaluateJavaScript:@"originalOpener === window.opener" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    Util::run(&done);

    [opened.webView evaluateJavaScript:@"opener.location = '/webkit'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://webkit.org"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://webkit.org"_s } });

    done = false;
    [opened.webView evaluateJavaScript:@"originalOpener === window.opener" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([result boolValue]);
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, NavigateOpenerToProvisionalNavigationFailure)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://webkit.org/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://example.com/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });

    [opened.webView evaluateJavaScript:@"opener.location = 'https://apple.com/terminate'" completionHandler:nil];
    [opener.navigationDelegate waitForDidFailProvisionalNavigation];
    EXPECT_NE(opened.webView.get()._webProcessIdentifier, opener.webView.get()._webProcessIdentifier);
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s }, { RemoteFrame } });
    checkFrameTreesInProcesses(opened.webView.get(), { { RemoteFrame }, { "https://webkit.org"_s } });
}

TEST(SiteIsolation, OpenProvisionalFailure)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    [opened.navigationDelegate waitForDidFailProvisionalNavigation];
    checkFrameTreesInProcesses(opener.webView.get(), { { "https://example.com"_s } });
    checkFrameTreesInProcesses(opened.webView.get(), { { "https://example.com"_s } });
}

TEST(SiteIsolation, NavigateIframeToProvisionalNavigationFailure)
{
    HTTPServer server({
        { "/webkit"_s, { "<iframe id='testiframe' src='https://example.com/example'></iframe>"_s } },
        { "/example"_s, { "hi"_s } },
        { "/redirect_to_example_terminate"_s, { 302, { { "Location"_s, "https://example.com/terminate"_s } }, "redirecting..."_s } },
        { "/redirect_to_webkit_terminate"_s, { 302, { { "Location"_s, "https://webkit.org/terminate"_s } }, "redirecting..."_s } },
        { "/redirect_to_apple_terminate"_s, { 302, { { "Location"_s, "https://apple.com/terminate"_s } }, "redirecting..."_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s } }
        },
    });

    __block bool provisionalLoadFailed { false };
    navigationDelegate.get().didFailProvisionalLoadWithRequestInFrameWithError = ^(WKWebView *, NSURLRequest *, WKFrameInfo *frameInfo, NSError *error) {
        EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
        EXPECT_EQ(error.code, NSURLErrorNetworkConnectionLost);
        EXPECT_FALSE(frameInfo.isMainFrame);
        provisionalLoadFailed = true;
    };

    __block RetainPtr blockScopeWebView { webView };
    auto checkProvisionalLoadFailure = ^(NSString *url) {
        provisionalLoadFailed = false;
        [blockScopeWebView evaluateJavaScript:[NSString stringWithFormat:@"document.getElementById('testiframe').src = '%@'", url] completionHandler:nil];
        while (!provisionalLoadFailed)
            Util::spinRunLoop();
        checkFrameTreesInProcesses(blockScopeWebView.get(), {
            { "https://webkit.org"_s,
                { { RemoteFrame } }
            }, { RemoteFrame,
                { { "https://example.com"_s } }
            },
        });
    };
    checkProvisionalLoadFailure(@"https://example.com/terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/terminate");
    checkProvisionalLoadFailure(@"https://apple.com/terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_example_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_example_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_example_terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_webkit_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_webkit_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_webkit_terminate");

    checkProvisionalLoadFailure(@"https://example.com/redirect_to_apple_terminate");
    checkProvisionalLoadFailure(@"https://webkit.org/redirect_to_apple_terminate");
    checkProvisionalLoadFailure(@"https://apple.com/redirect_to_apple_terminate");
}

TEST(SiteIsolation, DrawAfterNavigateToDomainAgain)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://b.com/b'></iframe>"_s } },
        { "/b"_s, { "hi"_s } },
        { "/c"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://b.com"_s } }
        }
    });

    [webView evaluateJavaScript:@"window.location = 'https://c.com/c'" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://c.com"_s }
    });

    [webView evaluateJavaScript:@"window.location = 'https://a.com/a'" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://b.com"_s } }
        }
    });

    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CancelProvisionalLoad)
{
    HTTPServer server({
        { "/main"_s, {
            "<iframe id='testiframe' src='https://example.com/respond_quickly'></iframe>"
            "<iframe src='https://example.com/respond_quickly'></iframe>"_s
        } },
        { "/respond_quickly"_s, { "hi"_s } },
        { "/never_respond"_s, { HTTPResponse::Behavior::NeverSendResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/main"]]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    auto checkStateAfterSequentialFrameLoads = [webView = RetainPtr { webView }, navigationDelegate = RetainPtr { navigationDelegate }] (NSString *first, NSString *second, Vector<ExpectedFrameTree>&& expectedTrees) {
        [webView evaluateJavaScript:[NSString stringWithFormat:@"i = document.getElementById('testiframe'); i.addEventListener('load', () => { alert('iframe loaded') }); i.src = '%@'; setTimeout(()=>{ i.src = '%@' }, Math.random() * 100)", first, second] completionHandler:nil];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "iframe loaded");
        checkFrameTreesInProcesses(webView.get(), WTFMove(expectedTrees));
    };

    checkStateAfterSequentialFrameLoads(@"https://webkit.org/never_respond", @"https://example.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://example.com/never_respond", @"https://webkit.org/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { "https://webkit.org"_s } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://webkit.org/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { "https://webkit.org"_s } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://example.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { "https://example.com"_s } }
        },
    });

    checkStateAfterSequentialFrameLoads(@"https://apple.com/never_respond", @"https://apple.com/respond_quickly", {
        { "https://webkit.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://example.com"_s }, { RemoteFrame } }
        }, { RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s } }
        }
    });
}

// FIXME: If a provisional load happens in a RemoteFrame with frame children, does anything clear out those
// child frames when the load commits? Probably not. Needs a test.

// FIXME: Add a test that verifies that provisional frames are not accessible via DOMWindow.frames.

// FIXME: Make a test that tries to access its parent that used to be remote during a provisional navigation of
// the parent to that domain to verify that even the main frame uses provisional frames.

TEST(SiteIsolation, OpenThenClose)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr<WKWebView> retainOpener;
    @autoreleasepool {
        auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
        retainOpener = opener.webView;
    }
}

TEST(SiteIsolation, CustomUserAgent)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView setCustomUserAgent:@"Custom UserAgent"];
    EXPECT_WK_STREQ(@"Custom UserAgent", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, ApplicationNameForUserAgent)
{
    auto mainframeHTML = "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain3.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex].endsWith(" Custom UserAgent"_s));
                receivedRequestFromSubframe = true;
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _setApplicationNameForUserAgent:@"Custom UserAgent"];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]] hasSuffix:@" Custom UserAgent"]);
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgent)
{
    auto mainframeHTML = "<iframe src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain3.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    bool firstRequest = true;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                if (firstRequest)
                    EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                else
                    EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent2"_s);
                receivedRequestFromSubframe = true;
                firstRequest = false;
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    EXPECT_WK_STREQ("Custom UserAgent", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    Util::run(&receivedRequestFromSubframe);
    EXPECT_WK_STREQ("Custom UserAgent2", [webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgentDuringCrossSiteProvisionalNavigation)
{
    auto mainframeHTML = "<iframe id='frame' src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain2.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                receivedRequestFromSubframe = true;
                continue;
            }
            if (path == "/missing"_s)
                continue;
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *webView, WKNavigation *) {
        [webView evaluateJavaScript:@"document.getElementById('frame').src = 'https://domain4.com/subframe';" completionHandler:nil];
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain3.com/missing"]]];
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, WebsitePoliciesCustomNavigatorPlatform)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomNavigatorPlatform:@"Custom Navigator Platform"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_WK_STREQ("Custom Navigator Platform", [webView objectByEvaluatingJavaScript:@"navigator.platform" inFrame:[webView firstChildFrame]]);
}

TEST(SiteIsolation, LoadHTMLString)
{
    HTTPServer server({
        { "/webkit"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    NSString *html = @"<iframe src='https://webkit.org/webkit'></iframe>";
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://example.com"]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });

    [webView loadHTMLString:html baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://webkit.org"_s,
            { { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, WebsitePoliciesCustomUserAgentDuringSameSiteProvisionalNavigation)
{
    auto mainframeHTML = "<iframe id='frame' src='https://domain2.com/subframe'></iframe>"_s;
    auto subframeHTML = "<script src='https://domain2.com/request_from_subframe'></script>"_s;
    bool receivedRequestFromSubframe = false;
    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/mainframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(mainframeHTML).serialize());
                continue;
            }
            if (path == "/subframe"_s) {
                co_await connection.awaitableSend(HTTPResponse(subframeHTML).serialize());
                continue;
            }
            if (path == "/request_from_subframe"_s) {
                auto headers = String::fromUTF8(request.span()).split("\r\n"_s);
                auto userAgentIndex = headers.findIf([](auto& header) {
                    return header.startsWith("User-Agent:"_s);
                });
                co_await connection.awaitableSend(HTTPResponse(""_s).serialize());
                EXPECT_TRUE(headers[userAgentIndex] == "User-Agent: Custom UserAgent"_s);
                receivedRequestFromSubframe = true;
                continue;
            }
            if (path == "/missing"_s)
                continue;
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedRequestFromSubframe);
    receivedRequestFromSubframe = false;

    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if (navigationAction.targetFrame.mainFrame)
            [preferences _setCustomUserAgent:@"Custom UserAgent2"];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };

    navigationDelegate.get().didStartProvisionalNavigation = ^(WKWebView *webView, WKNavigation *) {
        [webView evaluateJavaScript:@"document.getElementById('frame').src = 'https://domain3.com/subframe';" completionHandler:nil];
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/missing"]]];
    Util::run(&receivedRequestFromSubframe);
}

TEST(SiteIsolation, ProvisionalLoadFailureOnCrossSiteRedirect)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='webkit_frame' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { ""_s } },
        { "/redirect"_s, { 302, { { "Location"_s, "https://example.com/terminate"_s } }, "redirecting..."_s } },
        { "/terminate"_s, { HTTPResponse::Behavior::TerminateConnectionAfterReceivingRequest } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    navigationDelegate.get().didFailProvisionalLoadWithRequestInFrameWithError = ^(WKWebView *, NSURLRequest *request, WKFrameInfo *, NSError *) {
        EXPECT_WK_STREQ(request.URL.absoluteString, "https://example.com/terminate");
        done = true;
    };
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/redirect'" inFrame:[webView firstChildFrame] inContentWorld:WKContentWorld.pageWorld completionHandler:nil];
    Util::run(&done);
}

TEST(SiteIsolation, SynchronouslyExecuteEditCommandSelectAll)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe' src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "test"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
        Util::spinRunLoop();
}

#if PLATFORM(MAC)
TEST(SiteIsolation, SelectAll)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe' src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "test"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    while (![childFrame _isFocused])
        childFrame = [webView firstChildFrame];

    [webView selectAll:nil];
    while (![webView selectionRangeHasStartOffset:0 endOffset:4 inFrame:childFrame.get()])
        Util::spinRunLoop();
}

TEST(SiteIsolation, TopContentInsetAfterCrossSiteNavigation)
{
    HTTPServer server({
        { "/source"_s, { "<script> location.href = 'https://webkit.org/destination'; </script>"_s } },
        { "/destination"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView _setTopContentInset:10];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/source"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(-10, [[webView objectByEvaluatingJavaScript:@"window.innerHeight"] intValue]);
}
#endif

TEST(SiteIsolation, PresentationUpdateAfterCrossSiteNavigation)
{
    HTTPServer server({
        { "/source"_s, { "<script> location.href = 'https://webkit.org/destination'; </script>"_s } },
        { "/destination"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/source"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
}

TEST(SiteIsolation, CanGoBackAfterLoadingAndNavigatingFrame)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='frame' src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { ""_s } },
        { "/destination"_s, { "<script> alert('done'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_FALSE([webView canGoBack]);

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/destination'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "done");
    EXPECT_TRUE([webView canGoBack]);
}

TEST(SiteIsolation, CanGoBackAfterNavigatingFrameCrossOrigin)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='frame' src='https://domain1.com/source'></iframe>"_s } },
        { "/source"_s, { ""_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"location.href = 'https://domain2.com/destination'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");
    EXPECT_TRUE([webView canGoBack]);
}

TEST(SiteIsolation, RestoreSessionFromAnotherWebView)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/frame'></iframe>"_s } },
        { "/frame"_s, { "<script> alert('done'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView1, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView1 _test_waitForAlert], "done");

    auto [webView2, navigationDelegate2] = siteIsolatedViewAndDelegate(server);
    [webView2 _restoreSessionState:[webView1 _sessionState] andNavigate:YES];
    EXPECT_WK_STREQ([webView2 _test_waitForAlert], "done");
}

static void testNavigateIframeBackForward(NSString *navigationURL, bool restoreSessionState)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "source");

    RetainPtr childFrame = [webView firstChildFrame];
    [webView evaluateJavaScript:[NSString stringWithFormat:@"location.href = '%@'", navigationURL] inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "destination");

    if (restoreSessionState)
        [webView _restoreSessionState:[webView _sessionState] andNavigate:NO];

    [webView goBack];
    EXPECT_WK_STREQ("source", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/source", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);

    [webView goForward];
    EXPECT_WK_STREQ("destination", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ(navigationURL, [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);

    [webView goBack];
    EXPECT_WK_STREQ("source", [webView _test_waitForAlert]);
    EXPECT_WK_STREQ("https://webkit.org/source", [webView objectByEvaluatingJavaScript:@"location.href" inFrame:childFrame.get()]);
}

TEST(SiteIsolation, NavigateIframeSameOriginBackForward)
{
    testNavigateIframeBackForward(@"https://webkit.org/destination", false);
}

TEST(SiteIsolation, DISABLED_NavigateIframeSameOriginBackForwardAfterSessionRestore)
{
    testNavigateIframeBackForward(@"https://webkit.org/destination", true);
}

TEST(SiteIsolation, NavigateIframeCrossOriginBackForward)
{
    testNavigateIframeBackForward(@"https://apple.com/destination", false);
}

TEST(SiteIsolation, DISABLED_NavigateIframeCrossOriginBackForwardAfterSessionRestore)
{
    testNavigateIframeBackForward(@"https://apple.com/destination", true);
}

TEST(SiteIsolation, ValidateSessionRestoreWithoutNavigating)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/source'></iframe>"_s } },
        { "/source"_s, { "<script> alert('source'); </script>"_s } },
        { "/destination"_s, { "<script> alert('destination'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [normalView, normalNavigationDelegate] = viewAndDelegate(server);
    auto [isolatedView, isolatedNavigationDelegate] = siteIsolatedViewAndDelegate(server);

    [normalView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([normalView _test_waitForAlert], "source");

    [isolatedView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([isolatedView _test_waitForAlert], "source");

    RetainPtr childFrame = [normalView firstChildFrame];
    NSString *navigationURL = @"https://apple.com/destination";
    [normalView evaluateJavaScript:[NSString stringWithFormat:@"location.href = '%@'", navigationURL] inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([normalView _test_waitForAlert], "destination");

    childFrame = [isolatedView firstChildFrame];
    [isolatedView evaluateJavaScript:[NSString stringWithFormat:@"location.href = '%@'", navigationURL] inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([isolatedView _test_waitForAlert], "destination");

    RetainPtr normalSessionState = [normalView _sessionState];
    RetainPtr isolatedSessionState = [isolatedView _sessionState];

    // FIXME: These two session states should be equal, but are not.
    // This is because the fidelity of the back/forward list in the UI process is wrong with site isolation on.
    // You should also be able to do a deep comparison of the WKBackForwardListItems here for equality, but cannot for the same reason.
    // Covered by https://bugs.webkit.org/show_bug.cgi?id=300832
    //
    // EXPECT_TRUE([isolatedSessionState isEqualForTesting:normalSessionState.get()]);

    // Meanwhile, we can verify that the the session state generated by a normal web view successfully restores into an isolated view,
    // and that the isolated view will succesfully recreate that same session state.
    [isolatedView _restoreSessionState:normalSessionState.get() andNavigate:NO];
    RetainPtr newIsolatedSessionState = [isolatedView _sessionState];

    EXPECT_TRUE([newIsolatedSessionState isEqualForTesting:normalSessionState.get()]);
}

TEST(SiteIsolation, DiscardUncachedBackItemForNavigatedOverIframe)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().pageCacheEnabled = NO;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ("a", [webView _test_waitForAlert]);

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/b'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ("b", [webView _test_waitForAlert]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/c"]]];
    EXPECT_WK_STREQ("c", [webView _test_waitForAlert]);

    [webView goBack];
    EXPECT_WK_STREQ("a", [webView _test_waitForAlert]);
}

TEST(SiteIsolation, ProtocolProcessSeparation)
{
    HTTPServer secureServer({
        { "/subdomain"_s, { "hi"_s } },
        { "/no_subdomain"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    HTTPServer plaintextServer({
        { "http://a.com/"_s, {
            "<iframe src='https://a.com/no_subdomain'></iframe>"
            "<iframe src='https://subdomain.a.com/subdomain'></iframe>"_s
        } }
    });

    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", secureServer.port()]]];
    [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", plaintextServer.port()]]];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://a.com/"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        { "http://a.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://subdomain.a.com"_s }, { "https://a.com"_s } }
        },
    });
}

TEST(SiteIsolation, GoBackToPageWithIframe)
{
    HTTPServer server({
        { "/a"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/b"_s, { ""_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/a"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://b.com/b"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView goBack];
    [navigationDelegate waitForDidFinishNavigation];
    checkFrameTreesInProcesses(webView.get(), {
        { "https://a.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "https://frame.com"_s } }
        },
    });
}

TEST(SiteIsolation, NavigateNestedIframeSameOriginBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://a.com/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    RetainPtr<WKFrameInfo> childFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject.info;
    [webView evaluateJavaScript:@"location.href = 'https://a.com/b'" inFrame:childFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
}

TEST(SiteIsolation, GoBackToNestedIframeCreatedAfterNavigatingSibling)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    auto createIframe = @"var iframe = document.createElement('iframe');"
        "iframe.src = 'https://apple.com/c';"
        "document.body.appendChild(iframe);";
    [webView evaluateJavaScript:createIframe completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");

    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/b'" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");

    [webView evaluateJavaScript:createIframe inFrame:[webView secondChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");

    RetainPtr<WKFrameInfo> nestedChildFrame = [webView mainFrame].childFrames[1].childFrames.firstObject.info;
    [webView evaluateJavaScript:@"location.href = 'https://apple.com/a'" inFrame:nestedChildFrame.get() completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
}

TEST(SiteIsolation, AdvancedPrivacyProtectionsHideScreenMetricsFromBindings)
{
    auto frameHTML = [NSString stringWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"simple" ofType:@"html"] encoding:NSUTF8StringEncoding error:NULL];
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://frame.com/frame'></iframe>"_s } },
        { "/frame"_s, { frameHTML } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setNetworkConnectionIntegrityPolicy:_WKWebsiteNetworkConnectionIntegrityPolicyEnhancedTelemetry | _WKWebsiteNetworkConnectionIntegrityPolicyEnabled];
    [configuration setDefaultWebpagePreferences:preferences.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    webView.get().navigationDelegate = navigationDelegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenX" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screenY" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availLeft" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"screen.availTop" inFrame:childFrame.get()] intValue]);
}

TEST(SiteIsolation, UpdateWebpagePreferences)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://b.com/frame'></iframe>"_s } },
        { "/frame"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto preferences = adoptNS([WKWebpagePreferences new]);
    [preferences _setCustomUserAgent:@"Custom UserAgent"];
    [webView _updateWebpagePreferences:preferences.get()];
    while (![[webView objectByEvaluatingJavaScript:@"navigator.userAgent" inFrame:[webView firstChildFrame]] isEqualToString:@"Custom UserAgent"])
        Util::spinRunLoop();
}

TEST(SiteIsolation, MainFrameRedirectBetweenExistingProcesses)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "hi"_s } },
        { "/webkit_redirect"_s, { 302, { { "Location"_s, "https://example.com/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.length"] intValue], 1);
    auto pidBefore = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://webkit.org/webkit_redirect"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.length"] intValue], 0);
    EXPECT_EQ([webView _webProcessIdentifier], pidBefore);
}

TEST(SiteIsolation, URLSchemeTask)
{
    HTTPServer server({
        { "/example"_s, { ""_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto handler = adoptNS([TestURLSchemeHandler new]);
    handler.get().startURLSchemeTaskHandler = ^(WKWebView *, id<WKURLSchemeTask> task) {
        if ([task.request.URL.path isEqualToString:@"/example"])
            respond(task, "<iframe src='customscheme://webkit.org/webkit'></iframe>");
        else if ([task.request.URL.path isEqualToString:@"/webkit"]) {
            respond(task, "<script>"
                "var xhr = new XMLHttpRequest();"
                "xhr.open('GET', '/fetched');"
                "xhr.onreadystatechange = function () {"
                    "if (xhr.readyState == xhr.DONE) { alert(xhr.responseURL + ' ' + xhr.responseText) }"
                "};"
                "xhr.send();"
            "</script>");
        } else if ([task.request.URL.path isEqualToString:@"/fetched"]) {
            auto newRequest = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL URLWithString:@"customscheme://webkit.org/redirected"]]);
            [(id<WKURLSchemeTaskPrivate>)task _willPerformRedirection:adoptNS([NSURLResponse new]).get() newRequest:newRequest.get() completionHandler:^(NSURLRequest *request) {
                respond(task, "hi");
            }];
        } else
            EXPECT_TRUE(false);
    };
    [configuration setURLSchemeHandler:handler.get() forURLScheme:@"customscheme"];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"customscheme://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "customscheme://webkit.org/redirected hi");
    checkFrameTreesInProcesses(webView.get(), {
        { "customscheme://example.com"_s,
            { { RemoteFrame } }
        }, { RemoteFrame,
            { { "customscheme://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, ThemeColor)
{
    HTTPServer server({
        { "/example"_s, {
            "<style> html { background-color: blue } </style>"
            "<meta name='theme-color' content='red'><iframe src='https://webkit.org/webkit'></iframe>"_s
        } },
        { "/webkit"_s, {
            "<style> html { background-color: red } </style>"
            "<meta name='theme-color' content='blue'>"_s
        } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, delegate] = siteIsolatedViewAndDelegate(server);
    EXPECT_FALSE([webView themeColor]);
    EXPECT_TRUE([webView underPageBackgroundColor]);

    __block bool observedThemeColor { false };
    __block bool observedUnderPageBackgroundColor { false };
    auto observer = adoptNS([TestObserver new]);
    observer.get().observeValueForKeyPath = ^(NSString *path, id view) {
        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        if ([path isEqualToString:@"themeColor"]) {
            auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
            EXPECT_TRUE(CGColorEqualToColor([[view themeColor] CGColor], redColor.get()));
            observedThemeColor = true;
        } else {
            EXPECT_WK_STREQ(path, "underPageBackgroundColor");
            auto blueColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), blueColorComponents));
            EXPECT_TRUE(CGColorEqualToColor([[view underPageBackgroundColor] CGColor], blueColor.get()));
            observedUnderPageBackgroundColor = true;
        }
    };
    [webView.get() addObserver:observer.get() forKeyPath:@"themeColor" options:NSKeyValueObservingOptionNew context:nil];
    [webView.get() addObserver:observer.get() forKeyPath:@"underPageBackgroundColor" options:NSKeyValueObservingOptionNew context:nil];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [delegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    Util::run(&observedThemeColor);
    Util::run(&observedUnderPageBackgroundColor);
    Util::runFor(0.1_s);

    [webView.get() removeObserver:observer.get() forKeyPath:@"themeColor"];
    [webView.get() removeObserver:observer.get() forKeyPath:@"underPageBackgroundColor"];
}

static WebViewAndDelegates makeWebViewAndDelegates(HTTPServer& server, bool enable = true)
{
    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    RetainPtr configuration = server.httpsProxyConfiguration();
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get(), CGRectZero, enable);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setUIDelegate:uiDelegate.get()];
    return {
        WTFMove(webView),
        WTFMove(messageHandler),
        WTFMove(navigationDelegate),
        WTFMove(uiDelegate)
    };
};

TEST(SiteIsolation, SandboxFlags)
{
    NSString *checkAlertJS = @"alert('alerted');window.open('https://example.com/opened');window.webkit.messageHandlers.testHandler.postMessage('testHandler')";

    HTTPServer server({
        { "/example"_s, { "<iframe sandbox='allow-scripts' id='testiframe' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/check-when-loaded"_s, { [NSString stringWithFormat:@"<script>onload = ()=>{ %@ }</script>", checkAlertJS] } },
        { "/csp-forbids-alert"_s, { { { "Content-Security-Policy"_s, "sandbox allow-scripts"_s } }, "<script>alert('alerted');window.webkit.messageHandlers.testHandler.postMessage('testHandler')</script>"_s } },
        { "/opened"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    bool receivedMessage { false };
    bool receivedAlert { false };
    bool receivedOpen { false };
    auto reset = [&] {
        receivedMessage = false;
        receivedAlert = false;
        receivedOpen = false;
    };

    WebViewAndDelegates openedWebViewAndDelegates;
    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;
    [webViewAndDelegates.messageHandler addMessage:@"testHandler" withHandler:[&] {
        receivedMessage = true;
    }];
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = [&](WKWebView *, NSString *alert, WKFrameInfo *, void (^completionHandler)()) {
        receivedAlert = true;
        completionHandler();
    };
    auto returnNilOpenedView = [&] (WKWebViewConfiguration *, WKNavigationAction *, WKWindowFeatures *) -> WKWebView * {
        receivedOpen = true;
        return nil;
    };
    auto returnNonNilOpenedView = [&] (WKWebViewConfiguration *configuration, WKNavigationAction *, WKWindowFeatures *) -> WKWebView * {
        EXPECT_FALSE(openedWebViewAndDelegates.webView);
        openedWebViewAndDelegates = WebViewAndDelegates {
            adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]),
            nil,
            adoptNS([TestNavigationDelegate new]),
            uiDelegate
        };
        openedWebViewAndDelegates.webView.get().UIDelegate = uiDelegate.get();
        openedWebViewAndDelegates.webView.get().navigationDelegate = openedWebViewAndDelegates.navigationDelegate.get();
        [openedWebViewAndDelegates.navigationDelegate allowAnyTLSCertificate];
        receivedOpen = true;
        return openedWebViewAndDelegates.webView.get();
    };
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [webViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"let i = document.getElementById('testiframe'); i.sandbox = 'allow-scripts allow-modals'" completionHandler:^(id, NSError *) {
        [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    }];
    Util::run(&receivedMessage);
    // The second warning of https://html.spec.whatwg.org/multipage/iframe-embed-object.html#attr-iframe-sandbox
    // says we shouldn't change the effective sandbox until an iframe navigates.
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://example.org/csp-forbids-alert'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.src = 'https://example.org/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_FALSE(receivedOpen);

    reset();
    [webView evaluateJavaScript:@"i.removeAttribute('sandbox'); i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNonNilOpenedView;
    [webView evaluateJavaScript:@"i.sandbox = 'allow-scripts allow-popups'; i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    while (!openedWebViewAndDelegates.webView)
        Util::spinRunLoop();
    [openedWebViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;
    [openedWebViewAndDelegates.webView evaluateJavaScript:checkAlertJS completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNonNilOpenedView;
    openedWebViewAndDelegates.webView = nil;
    [webView evaluateJavaScript:@"i.sandbox = 'allow-scripts allow-popups allow-popups-to-escape-sandbox'; i.src = 'https://apple.com/check-when-loaded'" completionHandler:nil];
    while (!openedWebViewAndDelegates.webView)
        Util::spinRunLoop();
    [openedWebViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
    EXPECT_TRUE(receivedOpen);

    reset();
    uiDelegate.get().createWebViewWithConfiguration = returnNilOpenedView;
    [openedWebViewAndDelegates.webView evaluateJavaScript:checkAlertJS completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_TRUE(receivedAlert);
    EXPECT_TRUE(receivedOpen);
}

TEST(SiteIsolation, SandboxFlagsDuringNavigation)
{
    bool receivedIframe2Request { false };
    HTTPServer server { HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        while (true) {
            auto path = HTTPServer::parsePath(co_await connection.awaitableReceiveHTTPRequest());
            if (path == "/example"_s) {
                co_await connection.awaitableSend(HTTPResponse("<iframe sandbox='allow-scripts' id='testiframe' src='https://webkit.org/iframe1'></iframe>"_s).serialize());
                continue;
            }
            if (path == "/iframe1"_s) {
                co_await connection.awaitableSend(HTTPResponse("hi"_s).serialize());
                continue;
            }
            if (path == "/iframe2"_s) {
                receivedIframe2Request = true;
                // Never respond.
                continue;
            }
            EXPECT_FALSE(true);
        }
    }, HTTPServer::Protocol::HttpsProxy };

    NSString *checkAlertJS = @"alert('alerted');window.webkit.messageHandlers.testHandler.postMessage('testHandler')";

    bool receivedMessage { false };
    bool receivedAlert { false };
    auto reset = [&] {
        receivedMessage = false;
        receivedAlert = false;
        receivedIframe2Request = false;
    };

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    webViewAndDelegates.uiDelegate.get().runJavaScriptAlertPanelWithMessage = [&](WKWebView *, NSString *alert, WKFrameInfo *, void (^completionHandler)()) {
        receivedAlert = true;
        completionHandler();
    };
    [webViewAndDelegates.messageHandler addMessage:@"testHandler" withHandler:[&] {
        receivedMessage = true;
    }];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [webViewAndDelegates.navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);

    reset();
    [webView evaluateJavaScript:@"let i = document.getElementById('testiframe'); i.sandbox = 'allow-scripts allow-modals'; i.src='https://webkit.org/iframe2'" completionHandler:nil];
    Util::run(&receivedIframe2Request);
    [webView evaluateJavaScript:checkAlertJS inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedMessage);
    EXPECT_FALSE(receivedAlert);
}

TEST(SiteIsolation, NavigateNestedRootFramesBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/nest'></iframe>"_s } },
        { "/nest"_s, { "<iframe src='https://a.com/a'></iframe>"_s } },
        { "/a"_s, { "<script> alert('a'); </script>"_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");

    RetainPtr<_WKFrameTreeNode> nestedChildFrame = [webView mainFrame].childFrames.firstObject.childFrames.firstObject;
    [webView evaluateJavaScript:@"location.href = 'https://a.com/b'" inFrame:[nestedChildFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "a");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
}

TEST(SiteIsolation, NavigateFrameWithSiblingsBackForward)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/a'></iframe> <iframe src='https://webkit.org/b'></iframe>"_s } },
        { "/a"_s, { ""_s } },
        { "/b"_s, { "<script> alert('b'); </script>"_s } },
        { "/c"_s, { "<script> alert('c'); </script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");

    RetainPtr<_WKFrameTreeNode> secondRootFrame = [webView mainFrame].childFrames[1];
    [webView evaluateJavaScript:@"location.href = 'https://webkit.org/c'" inFrame:[secondRootFrame info] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
    [webView goBack];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "b");
    [webView goForward];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "c");
}

TEST(SiteIsolation, RedirectToCSP)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/initial'></iframe>"_s } },
        { "/initial"_s, { 302, { { "Location"_s, "https://example.org/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { { { "Content-Type"_s, "text/html"_s }, { "Content-Security-Policy"_s, "frame-ancestors 'none'"_s } }, "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
}

TEST(SiteIsolation, IframeWithCSPHeaderForFrameAncestors)
{
    auto html = "<script>"
    "let origins = location.ancestorOrigins;"
    "let array = [];"
    "for (var i = 0; i < origins.length; i = i + 1) { array.push(origins.item(i)); };"
    "alert(array)"
    "</script>"_s;

    HTTPServer server({
        { "/"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { { { "Content-Type"_s, "text/html"_s }, { "Content-Security-Policy"_s, "frame-ancestors https://example.com"_s } }, html } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "https://example.com");
}

TEST(SiteIsolation, MultipleWebViewsWithSameOpenedConfiguration)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='/iframe'></iframe>"_s } },
        { "/iframe"_s, {
            "<script>onload = () => { document.getElementById('mylink').click() }</script>"
            "<a href='/popup' target='_blank' id='mylink'>link</a>"_s
        } },
        { "/popup"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/example", false);
    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:opened.webView.get().configuration]);
    [opened.navigationDelegate waitForDidFinishNavigation];
    // FIXME: load something with webView2 without asserting, like https://example.com/popup
}

TEST(SiteIsolation, RecoverFromCrash)
{
    HTTPServer server({
        { "/crash"_s, { "<script>window.internals.terminateWebContentProcess()</script>"_s } },
        { "/dontcrash"_s, { "hi"_s } },
        { "/iframecrash"_s, { "<iframe src='https://webkit.org/crash'></iframe>"_s } },
        { "/iframedontcrash"_s, { "<iframe src='https://webkit.org/dontcrash'></iframe>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration);

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/crash"]]];
    [navigationDelegate waitForWebContentProcessDidTerminate];
    [webView reload];
    [navigationDelegate waitForWebContentProcessDidTerminate];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/iframecrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/iframedontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/dontcrash"]]];
    [navigationDelegate waitForDidFinishNavigation];
}

TEST(SiteIsolation, IframeOpener)
{
    auto mainFrameHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('main frame received ' + event.data)"
    "    }, false);"
    "    onload = () => { window.open('https://example.com/iframe', 'myframename') }"
    "</script>"
    "<iframe name='myframename'></iframe>"_s;

    auto iframeHTML = "<script>"
    "    window.addEventListener('message', (event) => {"
    "        alert('child frame received ' + event.data)"
    "    }, false);"
    "    try { window.opener.postMessage('hello', '*') } catch (e) { alert('error ' + e) }"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { mainFrameHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    auto verifyThatOpenerIsParent = [webView = RetainPtr { webView }] (bool openerShouldBeParent) {
        auto value = openerShouldBeParent ? "1" : "0";
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.frames[0].opener == self"], value);
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"window.opener == window.parent" inFrame:[webView firstChildFrame]], value);
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" inFrame:[webView firstChildFrame] completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "child frame received hello");
    verifyThatOpenerIsParent(false);

    [webView evaluateJavaScript:@"window.open('https://webkit.org/iframe', 'myframename')" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "main frame received hello");
    verifyThatOpenerIsParent(true);
}

TEST(SiteIsolation, CrossProtocolNavigationWithAboutURL)
{
    HTTPServer secureServer({
        { "/example"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    HTTPServer plaintextServer({
        { "http://example.com/example"_s, { "hi"_s } },
    });

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", secureServer.port()]]];
    [storeConfiguration setHTTPProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", plaintextServer.port()]]];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(viewConfiguration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:viewConfiguration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto processIdentifier1 = [webView _webProcessIdentifier];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto processIdentifier2 = [webView _webProcessIdentifier];
    EXPECT_EQ(processIdentifier1, processIdentifier2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    auto processIdentifier3 = [webView _webProcessIdentifier];
    // Process should not be reused as protocols are different.
    EXPECT_NE(processIdentifier2, processIdentifier3);
}

TEST(SiteIsolation, ProcessReuse)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe' id='onlyiframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/iframe_with_alert"_s, { "<script>alert('loaded')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().usesWebProcessCache = YES;
    RetainPtr processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);
    RetainPtr webViewConfiguration = server.httpsProxyConfiguration();
    [webViewConfiguration setProcessPool:processPool.get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(webViewConfiguration.get());
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView objectByEvaluatingJavaScript:@"var frame = document.getElementById('onlyiframe'); frame.parentNode.removeChild(frame);1"];
    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');iframe.src = 'https://webkit.org/iframe_with_alert';document.body.appendChild(iframe)" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded");

    EXPECT_EQ(countWebPages(webView), 2u);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(countWebPages(webView), 2u);
}

TEST(SiteIsolation, ProcessTerminationReason)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='onlyiframe' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/iframe2"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    RetainPtr navigationDelegate = adoptNS([NavigationDelegateAllowingAllTLS new]);
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(server.totalRequests(), 2u);

    kill([webView firstChildFrame]._processIdentifier, 9);
    Util::runFor(0.1_s);
    EXPECT_EQ(server.totalRequests(), 2u);

    [webView evaluateJavaScript:@"onlyiframe.src='https://webkit.org/iframe2'" completionHandler:nil];
    while (server.totalRequests() < 3u)
        Util::spinRunLoop();

    kill([webView mainFrame].info._processIdentifier, 9);
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(server.totalRequests(), 5u);
}

TEST(SiteIsolation, FormSubmit)
{
    auto mainHTML = "<script>onload=()=>{onlyform.submit()}</script>"
    "<iframe name='onlyiframe' src='https://webkit.org/iframe'></iframe>"
    "<form action='alert_when_loaded' method='get' target='onlyiframe' id='onlyform'><input type='hidden' name='textname' value='textvalue'>"_s;

    HTTPServer server({
        { "/example"_s, { mainHTML } },
        { "/iframe"_s, { "hi"_s } },
        { "/alert_when_loaded?textname=textvalue"_s, { "<script>alert(window.location.search)</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "?textname=textvalue");
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        }
    });
}

TEST(SiteIsolation, ContentRuleListFrameURL)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<script>fetch('/example')</script>"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded second iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    __block WKWebViewConfiguration *configuration = [webView configuration];

    __block bool doneClearing { false };
    [[WKContentRuleListStore defaultStore] removeContentRuleListForIdentifier:@"Test" completionHandler:^(NSError *error) {
        doneClearing = true;
    }];
    TestWebKitAPI::Util::run(&doneClearing);

    __block bool doneCompiling = false;
    static NSString *filterSource = @"["
        "{\"action\":{\"type\":\"block\"},\"trigger\":{\"url-filter\":\"should_not_match\", \"if-frame-url\":[\"should_not_match\"]}}"
    "]";
    [[WKContentRuleListStore defaultStore] compileContentRuleListForIdentifier:@"Test" encodedContentRuleList:filterSource completionHandler:^(WKContentRuleList *ruleList, NSError *error) {
        [configuration.userContentController addContentRuleList:ruleList];
        doneCompiling = true;
    }];
    TestWebKitAPI::Util::run(&doneCompiling);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');document.body.appendChild(iframe);iframe.src = 'https://webkit.org/alert_when_loaded'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded second iframe");
}

TEST(SiteIsolation, ReuseConfiguration)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    RetainPtr configuration = server.httpsProxyConfiguration();

    auto [webView1, navigationDelegate1] = siteIsolatedViewAndDelegate(configuration);
    [webView1 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate1 waitForDidFinishNavigation];

    auto [webView2, navigationDelegate2] = siteIsolatedViewAndDelegate(configuration);
    [webView2 loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate2 waitForDidFinishNavigation];
}

TEST(SiteIsolation, ReuseConfigurationLoadHTMLString)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    enableSiteIsolation(configuration.get());
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView1 loadHTMLString:@"hi!" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView1 _test_waitForDidFinishNavigation];

    auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView2 loadHTMLString:@"hi!" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    [webView2 _test_waitForDidFinishNavigation];

    EXPECT_NE([webView1 _webProcessIdentifier], [webView2 _webProcessIdentifier]);
}

static void callMethodOnFirstVideoElementInFrame(WKWebView *webView, NSString *methodName, WKFrameInfo *frame)
{
    __block RetainPtr<NSError> error;
    __block bool done = false;

    NSString *source = [NSString stringWithFormat:@"document.getElementsByTagName('video')[0].%@()", methodName];
    [webView callAsyncJavaScript:source arguments:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
        error = callError;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_FALSE(!!error) << [error description].UTF8String;
}

static void expectPlayingAudio(WKWebView *webView, bool expected, ASCIILiteral reason)
{
    bool success = TestWebKitAPI::Util::waitFor([webView, expected]() {
        return [webView _isPlayingAudio] == expected;
    });
    EXPECT_TRUE(success) << reason.characters();
}

TEST(SiteIsolation, PlayAudioInMultipleFrames)
{
    auto mainFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"
    "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"_s;

    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.headerFields.set("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", nil);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in main frame"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in remote frame"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"pause", nil);
    expectPlayingAudio(webView.get(), true, "Should still be playing audio after pausing one of the two frames"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"pause", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), false, "Should not be playing audio after pausing in both frames"_s);
}

TEST(SiteIsolation, PlayAudioInRemoteFrameThenRemove)
{
    auto mainFrameHTML = "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"_s;

    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.headerFields.set("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in main frame"_s);

    __block bool done = false;
    __block RetainPtr<NSError> error;
    [webView evaluateJavaScript:@"document.querySelectorAll('iframe').forEach(iframe => iframe.remove())" completionHandler:^(id result, NSError *scriptError) {
        error = scriptError;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(!!error) << [error description].UTF8String;
    done = false;

    expectPlayingAudio(webView.get(), false, "Should not be playing audio after removing iframe"_s);
}

TEST(SiteIsolation, MutesAndSetsAudioInMultipleFrames)
{
    auto mainFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"
        "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video src='/video-with-audio.mp4' webkit-playsinline loop></video>"_s;

    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"video-with-audio" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.headerFields.set("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
        { "/video-with-audio.mp4"_s, { videoData.get() } },
    }, HTTPServer::Protocol::HttpsProxy);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    enableSiteIsolation(configuration);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", nil);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in main frame"_s);

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView firstChildFrame]);
    expectPlayingAudio(webView.get(), true, "Should be playing audio in remote frame"_s);

    auto expectMuted = [&](bool expectedMuted, WKFrameInfo *frame, ASCIILiteral reason) {
        bool success = TestWebKitAPI::Util::waitFor([&]() {
            id actuallyMuted = [webView objectByEvaluatingJavaScript:@"window.internals.isEffectivelyMuted(document.getElementsByTagName('video')[0])" inFrame:frame];
            return [actuallyMuted boolValue] == expectedMuted;
        });
        EXPECT_TRUE(success) << reason.characters();
    };

    auto expectMediaVolume = [&](float expectedMediaVolume, WKFrameInfo *frame, ASCIILiteral reason) {
        bool success = TestWebKitAPI::Util::waitFor([&]() {
            id actualMediaVolume = [webView objectByEvaluatingJavaScript:@"window.internals.pageMediaVolume()" inFrame:frame];
            return [actualMediaVolume floatValue] == expectedMediaVolume;
        });
        EXPECT_TRUE(success) << reason.characters();
    };

    expectMuted(false, nil, "Should not be muted in main frame"_s);
    expectMuted(false, [webView firstChildFrame], "Should not be muted in remote frame"_s);

    [webView _setPageMuted:_WKMediaAudioMuted];
    [webView _setMediaVolumeForTesting:0.125f];

    expectMuted(true, nil, "Should be muted in main frame"_s);
    expectMediaVolume(0.125f, nil, "Should set volume in main frame"_s);
    expectMuted(true, [webView firstChildFrame], "Should be muted in remote frame"_s);
    expectMediaVolume(0.125f, nil, "Should set volume in remote frame"_s);

    auto addFrameToBody = @""
        "return new Promise((resolve, reject) => {"
        "    let frame = document.createElement('iframe');"
        "    frame.onload = () => resolve(true);"
        "    frame.setAttribute('src', 'https://webkit.org/subframe');"
        "    document.body.appendChild(frame);"
        "})";
    __block RetainPtr<NSError> error;
    __block bool done = false;
    [webView callAsyncJavaScript:addFrameToBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
        error = callError;
        done = true;
    }];
    Util::run(&done);
    EXPECT_FALSE(!!error) << "Failed to add iframe: " << [error description].UTF8String;

    callMethodOnFirstVideoElementInFrame(webView.get(), @"play", [webView secondChildFrame]);
    expectMuted(true, [webView secondChildFrame], "Should be muted in newly created remote frame"_s);
    expectMediaVolume(0.125f, [webView secondChildFrame], "Should initialize newly created remote frame with previously set media volume"_s);
}

#if ENABLE(MEDIA_STREAM)

TEST(SiteIsolation, StopsMediaCaptureInRemoteFrame)
{
    auto mainFrameHTML = "<video id='video' controlsplaysinline autoplay></video>"
        "<script>var didStartStream = new Promise(resolve => { video.onplay = resolve; })</script>"
        "<script>var didEndStream = new Promise(resolve => { video.onended = resolve; })</script>"
        "<iframe allow='camera *' src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<video id='video' controlsplaysinline autoplay></video>"
        "<script>var didStartStream = new Promise(resolve => { video.onplay = resolve; })</script>"
        "<script>var didEndStream = new Promise(resolve => { video.onended = resolve; })</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { { { "Content-Type"_s, "text/html"_s } }, mainFrameHTML } },
        { "/subframe"_s, { { { "Content-Type"_s, "text/html"_s } }, subFrameHTML } },
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration _setMediaCaptureEnabled:YES];

    RetainPtr preferences = [configuration preferences];
    [preferences _setMediaCaptureRequiresSecureConnection:NO];
    [preferences _setMockCaptureDevicesEnabled:YES];
    [preferences _setGetUserMediaRequiresFocus:NO];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectZero, false);
    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto assertStartCaptureSucceedsInFrameWithPrompt = [&](WKFrameInfo *frame) {
        __block RetainPtr<NSError> error;
        __block bool done = false;

        NSString *source = @"return navigator.mediaDevices.getUserMedia({ audio: false, video: true }).then(stream => { video.srcObject = stream; })";
        [webView callAsyncJavaScript:source arguments:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
            error = callError;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        ASSERT_FALSE(!!error) << "Failed to start capture: " << [error description].UTF8String;

        [delegate waitUntilPrompted];
    };

    // FIXME: the mock media stream doesn't seem to start or stop producing frames for the video
    // element in the sim. This doesn't seem to be related to site isolation.
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    auto assertVideoStreamStartedOrEndedInFrame = [&](bool, WKFrameInfo*) { };
#else
    auto assertVideoStreamStartedOrEndedInFrame = [&](bool started, WKFrameInfo* frame) {
        __block RetainPtr<NSError> error;
        __block bool done = false;

        NSString *source = started ? @"return didStartStream.then(() => true)" : @"return didEndStream.then(() => true)";
        [webView callAsyncJavaScript:source arguments:nil inFrame:frame inContentWorld:WKContentWorld.pageWorld completionHandler:^(id result, NSError *callError) {
            error = callError;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        ASSERT_FALSE(!!error) << "Capture failed to " << (started ? "start" : "end") << " frames for video element: " << [error description].UTF8String;
    };
#endif

    auto assertVideoStreamStartedInFrame = [&](WKFrameInfo *frame) {
        assertVideoStreamStartedOrEndedInFrame(true, frame);
    };
    auto assertVideoStreamEndedInFrame = [&](WKFrameInfo *frame) {
        assertVideoStreamStartedOrEndedInFrame(false, frame);
    };

    auto assertCaptureState = [&](_WKMediaCaptureStateDeprecated expected) {
        _WKMediaCaptureStateDeprecated actual;
        TestWebKitAPI::Util::waitFor([webView, expected, &actual]() {
            actual = [webView _mediaCaptureState];
            return actual == expected;
        });
        ASSERT_EQ(actual, expected);
    };

    assertStartCaptureSucceedsInFrameWithPrompt(nil);
    assertStartCaptureSucceedsInFrameWithPrompt([webView firstChildFrame]);
    assertVideoStreamStartedInFrame(nil);
    assertVideoStreamStartedInFrame([webView firstChildFrame]);
    assertCaptureState(_WKMediaCaptureStateDeprecatedActiveCamera);

    [webView _stopMediaCapture];

    assertVideoStreamEndedInFrame(nil);
    assertVideoStreamEndedInFrame([webView firstChildFrame]);
    assertCaptureState(_WKMediaCaptureStateDeprecatedNone);

    assertStartCaptureSucceedsInFrameWithPrompt([webView firstChildFrame]);
    assertStartCaptureSucceedsInFrameWithPrompt(nil);
    assertVideoStreamStartedInFrame(nil);
    assertVideoStreamStartedInFrame([webView firstChildFrame]);
    assertCaptureState(_WKMediaCaptureStateDeprecatedActiveCamera);
}

#endif // ENABLE(MEDIA_STREAM)

TEST(SiteIsolation, FrameServerTrust)
{
    HTTPServer plaintextServer({
        { "/"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
    });
    HTTPServer secureServer({
        { "/iframe"_s, { "<script>alert('iframe loaded')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool receivedAlert { false };
    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frameInfo, void (^completionHandler)(void)) {
        EXPECT_WK_STREQ(message, "iframe loaded");
        EXPECT_NULL(frameInfo._serverTrust);
        completionHandler();
        receivedAlert = true;
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(secureServer);
    webView.get().UIDelegate = uiDelegate.get();
    [webView loadRequest:plaintextServer.request()];
    Util::run(&receivedAlert);
    EXPECT_NULL([webView mainFrame].info._serverTrust);
    verifyCertificateAndPublicKey([webView firstChildFrame]._serverTrust);
}

TEST(SiteIsolation, CoordinateTransformation)
{
    HTTPServer server({
        { "/example"_s, { "<br><iframe id='wk' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    auto convertPoint = [] (TestWKWebView *webView, CGPoint point) {
        __block CGPoint result;
        __block bool done { false };
        [webView _convertPoint:point fromFrame:[webView firstChildFrame] toMainFrameCoordinates:^(CGPoint transformedPoint, NSError *error) {
            EXPECT_NULL(error);
            result = transformedPoint;
            done = true;
        }];
        Util::run(&done);
        return result;
    };
    auto convertRect = [] (TestWKWebView *webView, CGRect rect) {
        __block CGRect result;
        __block bool done { false };
        [webView _convertRect:rect fromFrame:[webView firstChildFrame] toMainFrameCoordinates:^(CGRect transformedRect, NSError *error) {
            EXPECT_NULL(error);
            result = transformedRect;
            done = true;
        }];
        Util::run(&done);
        return result;
    };

#if PLATFORM(MAC)
    constexpr auto expectedTransformedY = 38;
#else
    constexpr auto expectedTransformedY = 40;
#endif
    {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [navigationDelegate waitForDidFinishNavigation];
        auto transformedPoint = convertPoint(webView.get(), { 11, 10 });
        EXPECT_EQ(transformedPoint.x, 21);
        EXPECT_EQ(transformedPoint.y, expectedTransformedY);
        auto transformedRect = convertRect(webView.get(), { { 11, 10 }, { 9, 8 } });
        EXPECT_EQ(transformedRect.origin.x, 21);
        EXPECT_EQ(transformedRect.origin.y, expectedTransformedY);
        EXPECT_EQ(transformedRect.size.height, 8);
        EXPECT_EQ(transformedRect.size.width, 9);
    }

    {
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/example"]]];
        [navigationDelegate waitForDidFinishNavigation];
        auto transformedPoint = convertPoint(webView.get(), { 11, 10 });
        EXPECT_EQ(transformedPoint.x, 21);
        EXPECT_EQ(transformedPoint.y, expectedTransformedY);
        auto transformedRect = convertRect(webView.get(), { { 11, 10 }, { 9, 8 } });
        EXPECT_EQ(transformedRect.origin.x, 21);
        EXPECT_EQ(transformedRect.origin.y, expectedTransformedY);
        EXPECT_EQ(transformedRect.size.height, 8);
        EXPECT_EQ(transformedRect.size.width, 9);
    }

    RetainPtr frameInfoOfRemovedFrame = [webView firstChildFrame];
    __block bool removedIframe { false };
    [webView evaluateJavaScript:@"var frame = document.getElementById('wk');frame.parentNode.removeChild(frame)" completionHandler:^(id, NSError *error) {
        removedIframe = true;
    }];
    Util::run(&removedIframe);
    __block bool done { false };
    [webView _convertPoint:CGPoint { 11, 10 } fromFrame:frameInfoOfRemovedFrame.get() toMainFrameCoordinates:^(CGPoint, NSError *error) {
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    Util::run(&done);
    done = false;
    [webView _convertRect:CGRect { { 11, 10 }, { 9, 8 } } fromFrame:frameInfoOfRemovedFrame.get() toMainFrameCoordinates:^(CGRect, NSError *error) {
        EXPECT_NOT_NULL(error);
        done = true;
    }];
    Util::run(&done);
}

RetainPtr<_WKTextManipulationToken> createToken(NSString *identifier, NSString *content)
{
    RetainPtr<_WKTextManipulationToken> token = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token setIdentifier: identifier];
    [token setContent: content];
    return token;
}

static RetainPtr<_WKTextManipulationItem> createItem(NSString *itemIdentifier, const Vector<RetainPtr<_WKTextManipulationToken>>& tokens)
{
    RetainPtr<NSMutableArray> wkTokens = adoptNS([[NSMutableArray alloc] init]);
    for (auto& token : tokens)
        [wkTokens addObject:token.get()];

    return adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:itemIdentifier tokens:wkTokens.get()]);
}

TEST(SiteIsolation, CompleteTextManipulation)
{
    static constexpr auto mainFrameBytes = R"TESTRESOURCE(
    <div id='text'>mainframe content</div>
    <script>
        function getTextContent() {
            window.webkit.messageHandlers.testHandler.postMessage(document.getElementById('text').innerHTML);
        }
        function getIframeTextContent() {
            document.getElementById('iframe').contentWindow.postMessage('print', '*');
        }
        function postResult(event) {
            window.webkit.messageHandlers.testHandler.postMessage(event.data);
        }
        addEventListener('message', postResult, false);
    </script>
    <iframe id='iframe' src='https://apple.com/apple'></iframe>
    )TESTRESOURCE"_s;

    static constexpr auto iframeBytes = R"TESTRESOURCE(
    <div id='text'>iframe content</div>
    <script>
        addEventListener('message', () => {
            let textElement = document.getElementById('text');
            parent.postMessage(textElement.innerHTML, '*');
        }, false);
        parent.postMessage('loaded', '*');
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/example"_s, { mainFrameBytes } },
        { "/apple"_s, { iframeBytes } },
    }, HTTPServer::Protocol::HttpsProxy);

    bool didLoad = false;
    bool didReceiveMainFrameContent = false;
    bool didReceiveIframeContent = false;
    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    [webViewAndDelegates.messageHandler addMessage:@"loaded" withHandler:[&]() {
        didLoad = true;
    }];
    [webViewAndDelegates.messageHandler addMessage:@"MAINFRAME CONTENT" withHandler:[&]() {
        didReceiveMainFrameContent = true;
    }];
    [webViewAndDelegates.messageHandler addMessage:@"IFRAME CONTENT" withHandler:[&]() {
        didReceiveIframeContent = true;
    }];
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr textManipulationDelegate = adoptNS([[SiteIsolationTextManipulationDelegate alloc] init]);
    [webView _setTextManipulationDelegate:textManipulationDelegate.get()];
    RetainPtr manipulationConfiguration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    manipulationConfiguration.get().includeSubframes = YES;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    Util::run(&didLoad);

    [webView _startTextManipulationsWithConfiguration:manipulationConfiguration.get() completion:^{ }];
    while ([textManipulationDelegate items].count < 2)
        Util::spinRunLoop();

    RetainPtr items = [textManipulationDelegate items];
    auto sortFunction = ^(_WKTextManipulationItem *item1, _WKTextManipulationItem *item2) {
        auto value1 = [NSNumber numberWithBool:item1.isSubframe];
        auto value2 = [NSNumber numberWithBool:item2.isSubframe];
        return [value1 compare:value2];
    };
    RetainPtr sortedItems = [items.get() sortedArrayUsingComparator:sortFunction];
    EXPECT_EQ(items.get().count, 2UL);
    auto firstItem = [sortedItems objectAtIndex:0];
    auto secondItem = [sortedItems objectAtIndex:1];
    EXPECT_EQ(firstItem.isSubframe, NO);
    EXPECT_EQ(firstItem.isCrossSiteSubframe, NO);
    EXPECT_EQ(firstItem.tokens.count, 1UL);
    EXPECT_STREQ("mainframe content", firstItem.tokens[0].content.UTF8String);
    EXPECT_EQ(secondItem.isSubframe, YES);
    EXPECT_EQ(secondItem.isCrossSiteSubframe, YES);
    EXPECT_EQ(secondItem.tokens.count, 1UL);
    EXPECT_STREQ("iframe content", secondItem.tokens[0].content.UTF8String);

    __block bool done = false;
    [webView _completeTextManipulationForItems:@[
        (_WKTextManipulationItem *)createItem(firstItem.identifier, { createToken(firstItem.tokens[0].identifier, @"MAINFRAME CONTENT") }),
        (_WKTextManipulationItem *)createItem(secondItem.identifier, { createToken(secondItem.tokens[0].identifier, @"IFRAME CONTENT") })
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    Util::run(&done);

    [webView evaluateJavaScript:@"getTextContent()" completionHandler:nil];
    Util::run(&didReceiveMainFrameContent);

    [webView evaluateJavaScript:@"getIframeTextContent()" completionHandler:nil];
    Util::run(&didReceiveIframeContent);
}

TEST(SiteIsolation, CompleteTextManipulationFailsInSomeFrame)
{
    static constexpr auto mainFrameBytes = R"TESTRESOURCE(
    <div>mainframe content</div>
    <script>
        function removeIframe() {
            let element = document.getElementById('iframe');
            element.parentNode.removeChild(element);
        }
        let messageCount = 0;
        addEventListener('message', () => {
            if (++messageCount == 2)
                window.webkit.messageHandlers.testHandler.postMessage('loaded');
        }, false);
    </script>
    <iframe id='iframe' src='https://apple.com/iframe'></iframe>
    <iframe src='https://webkit.org/iframe'></iframe>
    )TESTRESOURCE"_s;

    static constexpr auto iframeBytes = R"TESTRESOURCE(
    <div>iframe content</div>
    <script>
        parent.postMessage('loaded', '*');
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameBytes } },
        { "/iframe"_s, { iframeBytes } }
    }, HTTPServer::Protocol::HttpsProxy);

    bool receivedMessage = false;
    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    [webViewAndDelegates.messageHandler addMessage:@"loaded" withHandler:[&]() {
        receivedMessage = true;
    }];
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr textManipulationDelegate = adoptNS([[SiteIsolationTextManipulationDelegate alloc] init]);
    [webView _setTextManipulationDelegate:textManipulationDelegate.get()];
    RetainPtr manipulationConfiguration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    manipulationConfiguration.get().includeSubframes = YES;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    Util::run(&receivedMessage);

    [webView _startTextManipulationsWithConfiguration:manipulationConfiguration.get() completion:^{ }];
    while ([textManipulationDelegate items].count < 3)
        Util::spinRunLoop();

    RetainPtr items = [textManipulationDelegate items];
    auto sortFunction = ^(_WKTextManipulationItem *item1, _WKTextManipulationItem *item2) {
        auto value1 = [NSNumber numberWithBool:item1.isSubframe];
        auto value2 = [NSNumber numberWithBool:item2.isSubframe];
        return [value1 compare:value2];
    };

    RetainPtr sortedItems = [items.get() sortedArrayUsingComparator:sortFunction];
    EXPECT_EQ(items.get().count, 3UL);
    auto item1 = [sortedItems objectAtIndex:0];
    auto item2 = [sortedItems objectAtIndex:1];
    auto item3 = [sortedItems objectAtIndex:2];
    EXPECT_EQ(item1.isSubframe, NO);
    EXPECT_EQ(item1.isCrossSiteSubframe, NO);
    EXPECT_EQ(item1.tokens.count, 1UL);
    EXPECT_STREQ("mainframe content", item1.tokens[0].content.UTF8String);
    EXPECT_EQ(item2.isSubframe, YES);
    EXPECT_EQ(item2.isCrossSiteSubframe, YES);
    EXPECT_EQ(item2.tokens.count, 1UL);
    EXPECT_STREQ("iframe content", item2.tokens[0].content.UTF8String);
    EXPECT_EQ(item3.isSubframe, YES);
    EXPECT_EQ(item3.isCrossSiteSubframe, YES);
    EXPECT_EQ(item3.tokens.count, 1UL);
    EXPECT_STREQ("iframe content", item3.tokens[0].content.UTF8String);

    __block bool done = false;
    [webView evaluateJavaScript:@"removeIframe()" completionHandler:^(id, NSError *) {
        done = true;
    }];
    Util::run(&done);

    __block RetainPtr newItem1 = createItem(item1.identifier, { createToken(item1.tokens[0].identifier, @"MAINFRAME CONTENT") });
    __block RetainPtr newItem2 = createItem(item2.identifier, { createToken(item2.tokens[0].identifier, @"IFRAME CONTENT") });
    __block RetainPtr newItem3 = createItem(item3.identifier, { createToken(item3.tokens[0].identifier, @"IFRAME CONTENT") });
    [webView _completeTextManipulationForItems:@[ newItem1.get(), newItem2.get(), newItem3.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_NOT_NULL(errors);
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorContentChanged);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], newItem2.get());
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(SiteIsolation, TextManipulationInIframeIfIframeIsAddedAfterTranslationCall)
{
    HTTPServer server({
        { "/example"_s, {
        "<script>"
        "    function addCrossDomainIframe() {"
        "        const iframe = document.createElement('iframe');"
        "        iframe.src = 'https://apple.com/iframe.html';"
        "        document.body.appendChild(iframe);"
        "    }"
        "</script>"_s } },

        { "/iframe.html"_s, { "<html><body>hello<br>world<div>WebKit</div></body></html>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;

    RetainPtr textManipulationDelegate = adoptNS([[SiteIsolationTextManipulationDelegate alloc] init]);
    [webView _setTextManipulationDelegate:textManipulationDelegate.get()];
    RetainPtr manipulationConfiguration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    manipulationConfiguration.get().includeSubframes = YES;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [webView _startTextManipulationsWithConfiguration:manipulationConfiguration.get() completion:^{
        done = true;
    }];
    Util::run(&done);

    [webView evaluateJavaScript:@"addCrossDomainIframe();" completionHandler:nil];

    while ([textManipulationDelegate items].count < 3)
        Util::spinRunLoop();

    RetainPtr items = [textManipulationDelegate items];
    EXPECT_EQ(items.get().count, 3UL);
}

TEST(SiteIsolation, CreateWebArchive)
{
    HTTPServer server({
        { "/mainframe"_s, { "<div>mainframe content</div><iframe src='https://apple.com/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div>iframe content</div>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><div>mainframe content</div><iframe src=\"https://apple.com/iframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://example.com/mainframe"
            },
            @"WebSubframeArchives" : @[ @{
                @"WebMainResource" : @{
                    @"WebResourceData" : [@"<html><head></head><body><div>iframe content</div></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                    @"WebResourceFrameName" : @"<!--frame1-->",
                    @"WebResourceMIMEType" : @"text/html",
                    @"WebResourceTextEncodingName" : @"UTF-8",
                    @"WebResourceURL" : @"https://apple.com/iframe"
                }
            } ],
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

TEST(SiteIsolation, CreateWebArchiveNestedFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nestedframe'></iframe>"_s } },
        { "/nestedframe"_s, { "<p>hello</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    [webView createWebArchiveDataWithCompletionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><iframe src=\"https://domain2.com/subframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://domain1.com/mainframe"
            },
            @"WebSubframeArchives" : @[ @{
                @"WebMainResource" : @{
                    @"WebResourceData" : [@"<html><head></head><body><iframe src=\"https://domain3.com/nestedframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                    @"WebResourceFrameName" : @"<!--frame1-->",
                    @"WebResourceMIMEType" : @"text/html",
                    @"WebResourceTextEncodingName" : @"UTF-8",
                    @"WebResourceURL" : @"https://domain2.com/subframe"
                },
                @"WebSubframeArchives" : @[ @{
                    @"WebMainResource" : @{
                        @"WebResourceData" : [@"<html><head></head><body><p>hello</p></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                        @"WebResourceFrameName" : @"<!--frame2-->",
                        @"WebResourceMIMEType" : @"text/html",
                        @"WebResourceTextEncodingName" : @"UTF-8",
                        @"WebResourceURL" : @"https://domain3.com/nestedframe"
                    }
                } ]
            } ],
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

TEST(SiteIsolation, CreateWebArchiveForFrame)
{
    HTTPServer server({
        { "/mainframe"_s, { "<div>mainframe content</div><iframe src='https://apple.com/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div>iframe content</div>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    __block RetainPtr<NSArray<_WKFrameTreeNode *>> childFrames;
    [webView _frames:^(_WKFrameTreeNode *result) {
        EXPECT_NOT_NULL(result);
        EXPECT_NOT_NULL(result.childFrames);
        childFrames = result.childFrames;
        done = true;
    }];
    Util::run(&done);
    done = false;

    EXPECT_EQ([childFrames count], 1u);
    [webView _createWebArchiveForFrame:childFrames.get().firstObject.info completionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><div>iframe content</div></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"<!--frame1-->",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://apple.com/iframe"
            },
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

TEST(SiteIsolation, CreateWebArchiveForFrames)
{
    HTTPServer server({
        { "/mainframe"_s, { "<div>mainframe content</div><iframe src='https://apple.com/iframe'></iframe><iframe src='https://example.com/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<div>iframe content</div>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    static bool done = false;
    __block RetainPtr<NSMutableArray<WKFrameInfo *>> frames;
    [webView _frames:^(_WKFrameTreeNode *rootFrame) {
        frames = adoptNS([[NSMutableArray alloc] init]);
        EXPECT_NOT_NULL(rootFrame);
        // Include main frame.
        [frames addObject:rootFrame.info];

        EXPECT_EQ([rootFrame.childFrames count], 2u);
        for (_WKFrameTreeNode *frame in rootFrame.childFrames) {
            if (!frame || !frame.info || !frame.info.securityOrigin || !frame.info.securityOrigin.host)
                continue;
            // Include frames that load example.com.
            if ([frame.info.securityOrigin.host containsString:@"example.com"])
                [frames addObject:frame.info];
        }
        done = true;
    }];
    Util::run(&done);
    done = false;

    EXPECT_EQ([frames count], 2u);
    [webView _createWebArchiveForFrames:[frames copy] rootFrame:frames.get().firstObject completionHandler:^(NSData *result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        NSDictionary* actualDictionary = [NSPropertyListSerialization propertyListWithData:result options:0 format:nil error:nil];
        EXPECT_NOT_NULL(actualDictionary);
        NSDictionary *expectedDictionary = @{
            @"WebMainResource" : @{
                @"WebResourceData" : [@"<html><head></head><body><div>mainframe content</div><iframe src=\"https://apple.com/iframe\"></iframe><iframe src=\"https://example.com/iframe\"></iframe></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                @"WebResourceFrameName" : @"",
                @"WebResourceMIMEType" : @"text/html",
                @"WebResourceTextEncodingName" : @"UTF-8",
                @"WebResourceURL" : @"https://example.com/mainframe"
            },
            @"WebSubframeArchives" : @[ @{
                @"WebMainResource" : @{
                    @"WebResourceData" : [@"<html><head></head><body><div>iframe content</div></body></html>" dataUsingEncoding:NSUTF8StringEncoding],
                    @"WebResourceFrameName" : @"<!--frame2-->",
                    @"WebResourceMIMEType" : @"text/html",
                    @"WebResourceTextEncodingName" : @"UTF-8",
                    @"WebResourceURL" : @"https://example.com/iframe"
                }
            } ],
        };
        EXPECT_TRUE([expectedDictionary isEqualToDictionary:actualDictionary]);
        done = true;
    }];
    Util::run(&done);
    done = false;
}

static void validateWebArchiveMainResource(NSDictionary *actualResource, NSDictionary *expectedResource)
{
    if (!actualResource)
        return;

    for (id key in expectedResource) {
        NSString *actualValue = [actualResource objectForKey:key];
        if (!actualValue)
            actualValue = @"NULL";
        EXPECT_WK_STREQ([actualValue UTF8String], [[expectedResource objectForKey:key] UTF8String]);
    }
}

TEST(SiteIsolation, CreateWebArchiveForCopy)
{
    static constexpr auto mainframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    mainframecontent
    <iframe id='subframe' src='https://example2.com/subframe'></iframe>
    <script>
        function alertSubframe() { document.getElementById('subframe').contentWindow.postMessage('alert', '*'); }
    </script>
    )TESTRESOURCE"_s;

    static constexpr auto subframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    subframecontent
    <script>
        window.addEventListener('message', function(event) { 
            alert('hi');
        });
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeBytes } },
        { "/subframe"_s, { subframeBytes } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    static bool alerted = false;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alerted = true;
        completionHandler();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView waitForNextPresentationUpdate];

    [webView copy:nil];

    // Ensure there is enough time to collect webarchive from subframe process.
    [webView evaluateJavaScript:@"alertSubframe()" completionHandler:nil];
    Util::run(&alerted);

#if PLATFORM(IOS_FAMILY)
    RetainPtr archiveData = [UIPasteboard.generalPasteboard dataForPasteboardType:UTTypeWebArchive.identifier];
#else
    RetainPtr archiveData = [NSPasteboard.generalPasteboard dataForType:UTTypeWebArchive.identifier];
#endif
    NSDictionary* expectedMainFrameResource = @{
        @"WebResourceFrameName" : @"",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example.com/mainframe"
    };
    NSDictionary* expectedSubframeResource = @{
        @"WebResourceFrameName" : @"<!--frame1-->",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example2.com/subframe"
    };
    NSDictionary* actualMainframeArchive = [NSPropertyListSerialization propertyListWithData:archiveData.get() options:0 format:nil error:nil];
    EXPECT_NOT_NULL(actualMainframeArchive);
    validateWebArchiveMainResource([actualMainframeArchive objectForKey:@"WebMainResource"], expectedMainFrameResource);
    NSArray *subframeArchives = [actualMainframeArchive objectForKey:@"WebSubframeArchives"];
    EXPECT_NOT_NULL(subframeArchives);
    EXPECT_EQ(subframeArchives.count, 1u);
    validateWebArchiveMainResource([subframeArchives.firstObject objectForKey:@"WebMainResource"], expectedSubframeResource);
}

TEST(SiteIsolation, CreateWebArchiveNestedFrameForCopy)
{
    static constexpr auto mainframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    mainframecontent
    <iframe id='subframe' src='https://example2.com/subframe'></iframe>
    <script>
        function alertSubframe() { document.getElementById('subframe').contentWindow.postMessage('alert', '*'); }
    </script>
    )TESTRESOURCE"_s;

    static constexpr auto subframeBytes = R"TESTRESOURCE(
    <!DOCTYPE html>
    subframecontent
    <iframe src='https://example.com/nestedframe'></iframe>
    <script>
        window.addEventListener('message', function(event) { 
            alert('hi');
        });
    </script>
    )TESTRESOURCE"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeBytes } },
        { "/subframe"_s, { subframeBytes } },
        { "/nestedframe"_s, { "nestedframecontent"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    static bool alerted = false;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        alerted = true;
        completionHandler();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView stringByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)"];
    [webView waitForNextPresentationUpdate];

    [webView copy:nil];

    // Ensure there is enough time to collect webarchive from subframe process.
    [webView evaluateJavaScript:@"alertSubframe()" completionHandler:nil];
    Util::run(&alerted);

#if PLATFORM(IOS_FAMILY)
    RetainPtr archiveData = [UIPasteboard.generalPasteboard dataForPasteboardType:UTTypeWebArchive.identifier];
#else
    RetainPtr archiveData = [NSPasteboard.generalPasteboard dataForType:UTTypeWebArchive.identifier];
#endif
    NSDictionary* expectedMainFrameResource = @{
        @"WebResourceFrameName" : @"",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example.com/mainframe"
    };
    NSDictionary* expectedSubframeResource = @{
        @"WebResourceFrameName" : @"<!--frame1-->",
        @"WebResourceMIMEType" : @"text/html",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example2.com/subframe"
    };
    NSDictionary* expectedNestedFrameResource = @{
        @"WebResourceFrameName" : @"<!--frame2-->",
        @"WebResourceMIMEType" : @"text/plain",
        @"WebResourceTextEncodingName" : @"UTF-8",
        @"WebResourceURL" : @"https://example.com/nestedframe"
    };
    NSDictionary* actualMainframeArchive = [NSPropertyListSerialization propertyListWithData:archiveData.get() options:0 format:nil error:nil];
    EXPECT_NOT_NULL(actualMainframeArchive);
    validateWebArchiveMainResource([actualMainframeArchive objectForKey:@"WebMainResource"], expectedMainFrameResource);
    NSArray *actualSubframeArchives = [actualMainframeArchive objectForKey:@"WebSubframeArchives"];
    EXPECT_NOT_NULL(actualSubframeArchives);
    EXPECT_EQ(actualSubframeArchives.count, 1u);
    NSDictionary* actualSubframeArchive = actualSubframeArchives.firstObject;
    validateWebArchiveMainResource([actualSubframeArchive objectForKey:@"WebMainResource"], expectedSubframeResource);
    NSArray *actualNestedFrameArchives = [actualSubframeArchive objectForKey:@"WebSubframeArchives"];
    EXPECT_NOT_NULL(actualNestedFrameArchives);
    EXPECT_EQ(actualNestedFrameArchives.count, 1u);
    validateWebArchiveMainResource([actualNestedFrameArchives.firstObject objectForKey:@"WebMainResource"], expectedNestedFrameResource);
}

TEST(SiteIsolation, LoadWebArchive)
{
    RetainPtr<NSURL> archiveURL = [NSBundle.test_resourcesBundle URLForResource:@"SiteIsolationLoadWebArchive" withExtension:@"webarchive"];
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get(), CGRectZero, true);
    [webView loadRequest:[NSURLRequest requestWithURL:archiveURL.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { "https://apple.com"_s } }
        },
    });
}

TEST(SiteIsolation, LoadWebArchiveNestedFrame)
{
    RetainPtr<NSURL> archiveURL = [NSBundle.test_resourcesBundle URLForResource:@"SiteIsolationLoadWebArchiveNestedFrame" withExtension:@"webarchive"];
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration.get(), CGRectZero, true);
    [webView loadRequest:[NSURLRequest requestWithURL:archiveURL.get()]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://domain1.com"_s,
            { { "https://domain2.com"_s, { { "https://domain3.com"_s } } } }
        },
    });
}

// FIXME: Re-enable this once the extra resize events are gone.
// https://bugs.webkit.org/show_bug.cgi?id=292311 might do it.
TEST(SiteIsolation, DISABLED_Events)
{
    auto eventListeners = "<script>"
    "addEventListener('resize', ()=>{ alert('resize') });"
    "addEventListener('load', ()=>{ alert('load') });"
    "addEventListener('beforeunload', ()=>{ alert('beforeunload') });"
    "addEventListener('unload', ()=>{ alert('unload') });"
    "addEventListener('pageswap', ()=>{ alert('pageswap') });"
    "addEventListener('pageshow', ()=>{ alert('pageshow') });"
    "addEventListener('pagehide', ()=>{ alert('pagehide') });"
    "addEventListener('pagereveal', ()=>{ alert('pagereveal') });"
    "addEventListener('focus', ()=>{ alert('focus') });"
    "addEventListener('blur', ()=>{ alert('blur') });"
    "</script>"_s;

    HTTPServer server({
        { "/example"_s, { makeString(eventListeners, "<br><iframe id='wk' src='https://webkit.org/iframe'></iframe>"_s) } },
        { "/iframe"_s, { eventListeners } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool receivedLastExpectedMessage = false;
    __block RetainPtr<NSMutableArray<NSString *>> webkitMessages = adoptNS([NSMutableArray new]);
    __block RetainPtr<NSMutableArray<NSString *>> exampleMessages = adoptNS([NSMutableArray new]);
    __block RetainPtr<NSMutableArray<NSString *>> appleMessages = adoptNS([NSMutableArray new]);
    RetainPtr delegate = adoptNS([TestUIDelegate new]);
    delegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *frame, void (^completionHandler)(void)) {
        NSString *host = frame.securityOrigin.host;
        if ([host isEqualToString:@"apple.com"])
            [appleMessages addObject:message];
        else if ([host isEqualToString:@"webkit.org"])
            [webkitMessages addObject:message];
        else if ([host isEqualToString:@"example.com"])
            [exampleMessages addObject:message];
        else
            EXPECT_FALSE(true);
        completionHandler();
        if ([message isEqualToString:@"pageshow"] && [frame.securityOrigin.host isEqualToString:@"apple.com"])
            receivedLastExpectedMessage = true;
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    webView.get().UIDelegate = delegate.get();
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView evaluateJavaScript:@"wk.height = 75" completionHandler:nil];
    [webView evaluateJavaScript:@"window.location = 'https://apple.com/iframe'" inFrame:[webView firstChildFrame] completionHandler:nil];
    Util::run(&receivedLastExpectedMessage);
    Util::runFor(Seconds(0.1));

    NSArray *expectedExampleMessages = @[
#if PLATFORM(IOS_FAMILY)
        @"pagereveal",
#endif
        @"load",
        @"pageshow",
    ];
    if (![exampleMessages isEqualToArray:expectedExampleMessages]) {
        WTFLogAlways("Actual example messages: %@", exampleMessages.get());
        EXPECT_TRUE(false);
    }

    NSArray *expectedWebKitMessages = @[
        @"load",
        @"pageshow",
#if PLATFORM(IOS_FAMILY)
        @"pagereveal",
#endif
        @"resize",
        // FIXME: <rdar://150216569> There should be a pageswap from webkit.org here.
    ];
    if (![webkitMessages isEqualToArray:expectedWebKitMessages]) {
        WTFLogAlways("Actual webkit messages: %@", webkitMessages.get());
        EXPECT_TRUE(false);
    }

    NSArray *expectedAppleMessages = @[
        @"load",
        @"pageshow",
#if PLATFORM(IOS_FAMILY)
        @"pagereveal",
#endif
    ];
    if (![appleMessages isEqualToArray:expectedAppleMessages]) {
        WTFLogAlways("Actual apple messages: %@", appleMessages.get());
        EXPECT_TRUE(false);
    }
}

#if ENABLE(DRAG_SUPPORT) && PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
TEST(SiteIsolation, DragAndDrop)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"link-and-target-div" withExtension:@"html"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSArray *registeredTypes = [[simulator sourceItemProviders].firstObject registeredTypeIdentifiers];
    EXPECT_WK_STREQ(UTTypeURL.identifier, [registeredTypes firstObject]);
}
#endif

TEST(SiteIsolation, FramesDuringProvisionalNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } },
        { "/second_iframe"_s, { TestWebKitAPI::HTTPResponse::Behavior::NeverSendResponse } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];
    EXPECT_EQ(server.totalRequests(), 2u);

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe');document.body.appendChild(iframe);iframe.src = 'https://webkit.org/second_iframe'" completionHandler:nil];
    while (server.totalRequests() < 3)
        Util::spinRunLoop();
    EXPECT_EQ([[webView objectByEvaluatingJavaScript:@"window.parent.length" inFrame:[webView firstChildFrame]] intValue], 2);

    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame }, { "https://example.com"_s } }
        }, { RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame } }
        },
    });
}

TEST(SiteIsolation, DoAfterNextPresentationUpdate)
{
    HTTPServer server({
        { "/main"_s, { "<iframe src='https://webkit2.org/text'></iframe><iframe src='https://webkit3.org/text'></iframe>"_s } },
        { "/navigatefrom"_s, { "<script>window.location='https://webkit2.org/navigateto'</script>"_s } },
        { "/navigateto"_s, { "<iframe src='https://webkit1.org/alert'></iframe>"_s } },
        { "/alert"_s, { "<script>alert('loaded')</script>"_s } },
        { "/text"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    RetainPtr webView = webViewAndDelegates.webView;
    RetainPtr navigationDelegate = webViewAndDelegates.navigationDelegate;
    RetainPtr uiDelegate = webViewAndDelegates.uiDelegate;
    RetainPtr<WKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = [&](WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().navigationDelegate = navigationDelegate.get();
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit1.org/main"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"window.open('https://webkit1.org/navigatefrom')" completionHandler:nil];
    EXPECT_WK_STREQ([uiDelegate waitForAlert], "loaded");

    __block bool done = false;
    [openedWebView _doAfterNextPresentationUpdate:^{
        done = true;
    }];
    Util::run(&done);
}

TEST(SiteIsolation, UserScript)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSString *js = @"if (window.parent != window.self) { alert('script ran in iframe') }";
    RetainPtr script = adoptNS([[WKUserScript alloc] initWithSource:js injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:NO]);
    [[webView configuration].userContentController _addUserScriptImmediately:script.get()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "script ran in iframe");
}

TEST(SiteIsolation, SharedProcessMostBasic)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame, { { RemoteFrame } } } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s, { { "https://apple.com"_s } } } }
        },
    });
}

TEST(SiteIsolation, SharedProcessSameOrigin)
{
    HTTPServer server({
        { "/top"_s, { "<!DOCTYPE html><iframe src='./subframe'></iframe>"_s } },
        { "/subframe"_s, { "<!DOCTYPE html><p>hi</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/top"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { "https://example.com"_s } },
        },
    });
}

TEST(SiteIsolation, SharedProcessLoadIsolatedSiteInSubframeOfNewWindow)
{
    HTTPServer server({
        { "/opener"_s, { "<!DOCTYPE html><a href='javascript:window.open(`https://b.com/top`, `newWindow`);'>opener</a>"_s } },
        { "/top"_s, { "<!DOCTYPE html><iframe src='https://a.com/content'></iframe>"_s } },
        { "/content"_s, { "<!DOCTYPE html><p>hi</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block auto *sharedNavigationDelegate = navigationDelegate.get();
    __block RetainPtr<TestWKWebView> opendWebView;
    auto uiDelegate = adoptNS([[TestUIDelegate new] init]);
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opendWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 200) configuration:configuration]);
        opendWebView.get().navigationDelegate = sharedNavigationDelegate;
        return opendWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/opener"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), { { "https://a.com"_s, } });

    [webView evaluateJavaScript:@"document.querySelector('a').click()" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(opendWebView.get(), {
        {
            "https://b.com"_s,
            { { RemoteFrame } },
        },
        {
            RemoteFrame,
            { { "https://a.com"_s } },
        },
    });

    auto *mainFrameInfo = [webView mainFrame].info;
    auto *subFrameInfo = [opendWebView mainFrame].childFrames[0].info;
    EXPECT_STREQ("a.com", mainFrameInfo.request.URL.host.UTF8String);
    EXPECT_STREQ("b.com", [opendWebView mainFrame].info.request.URL.host.UTF8String);
    EXPECT_STREQ("a.com", subFrameInfo.request.URL.host.UTF8String);
    EXPECT_EQ(mainFrameInfo._processIdentifier, subFrameInfo._processIdentifier);
}

TEST(SiteIsolation, SharedProcessBasicNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<script>fetch('/example')</script>"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded second iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView evaluateJavaScript:@"var iframe = document.createElement('iframe'); document.body.appendChild(iframe); iframe.src = 'https://webkit.org/alert_when_loaded'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded second iframe");

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://webkit.org"_s } }
        },
    });
}

TEST(SiteIsolation, SharedProcessWithWebsitePolicies)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded alert iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        if ([navigationAction.request.URL.host isEqual:@"apple.com"] || [navigationAction.request.URL.path isEqual:@"alert_when_loaded"])
            preferences._allowSharedProcess = NO;
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame } }
        },
    });

    [webView evaluateJavaScript:@"document.body.appendChild(document.createElement('iframe')).src = 'https://w3.org/alert_when_loaded'" completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded alert iframe");

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame }, { RemoteFrame } }
        },
    });
}

TEST(SiteIsolation, SharedProcessBasicWebProcessCache)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/w3c"_s, { "w3c"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://w3.org/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://w3.org"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1B = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2B = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(mainFrameProcessB, mainFrameProcess);
    EXPECT_NE(childFrameProcess1B, childFrameProcess1);
    EXPECT_EQ(childFrameProcess1B, childFrameProcess2B);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcessC = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1C = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2C = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_EQ(mainFrameProcessC, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1C, childFrameProcess1);
    EXPECT_EQ(childFrameProcess2C, childFrameProcess2);
}

TEST(SiteIsolation, WebProcessCacheCrashWithZeroSharedProcess)
{
    HTTPServer server({
        { "/page"_s, { "<!DOCTYPE html><p>page"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    pid_t previousPID = 0;
    unsigned cacheSize = webView.get().configuration.processPool._processCacheCapacity;
    for (unsigned i = 0; i <= cacheSize + 1; ++i) {
        auto url = makeString("https://domain-"_s, i, ".com/page"_s);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url.createNSString().get()]]];
        [navigationDelegate waitForDidFinishNavigation];
        pid_t currentPID = [webView mainFrame].info._processIdentifier;
        EXPECT_NE(currentPID, previousPID);
        previousPID = currentPID;
    }
}

TEST(SiteIsolation, SharedProcessWebProcessCacheCanEvict)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);

    RetainPtr<WKProcessPool> processPool = webView.get().configuration.processPool;
    [processPool _setCachedProcessLifetimeForTesting:0];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://w3.org/empty"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), { { "https://w3.org"_s, } });
    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    EXPECT_NE(mainFrameProcessB, mainFrameProcess);

    // No processes should be in the cache due to eviction timeout of 0.
    bool cacheIsEmpty = Util::waitFor([&]() {
        return ![processPool _processCacheSize];
    });
    EXPECT_TRUE(cacheIsEmpty);
}

TEST(SiteIsolation, SharedProcessBasicWebProcessCacheCrash)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/first"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/second"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/w3c"_s, { "w3c"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/first"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://w3.org"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/second"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://w3.org"_s } }
        },
    });

    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1B = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2B = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_EQ(mainFrameProcessB, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1B, childFrameProcess1);
    EXPECT_EQ(childFrameProcess2B, childFrameProcess2);
}

TEST(SiteIsolation, SharedProcessWithResourceLoadStatistics)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
        { "/alert_when_loaded"_s, { "<script>alert('loaded alert iframe')</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    NSURL *dataStoreRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessWithResourceLoadStatisticsTestDataStore"] isDirectory:YES];
    NSURL *itpRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessWithResourceLoadStatisticsTestITP"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    NSURL *itpDatabaseFile = [itpRoot URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpDatabaseFile.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    [defaultFileManager createDirectoryAtURL:itpRoot withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *sourceFile = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:sourceFile.path]);
    [defaultFileManager copyItemAtPath:sourceFile.path toPath:itpDatabaseFile.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    auto database = makeUniqueRef<WebCore::SQLiteDatabase>();
    EXPECT_TRUE(database->open(itpDatabaseFile.path));
    EXPECT_TRUE(database->executeCommand("UPDATE ObservedDomains SET hadUserInteraction = 1 WHERE registrableDomain = 'webkit.org'"_s));
    database->close();

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, dataStoreRoot, itpRoot);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { RemoteFrame } }
        },
    });
}

#if PLATFORM(MAC)

TEST(SiteIsolation, SharedProcessAfterClick)
{
    HTTPServer server({
        { "/warmup"_s, { "<iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    NSURL *dataStoreRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterClickDataStore"] isDirectory:YES];
    NSURL *itpRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterClickTestITP"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    NSURL *itpDatabaseFile = [itpRoot URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpDatabaseFile.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    [defaultFileManager createDirectoryAtURL:itpRoot withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *sourceFile = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:sourceFile.path]);
    [defaultFileManager copyItemAtPath:sourceFile.path toPath:itpDatabaseFile.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, dataStoreRoot, itpRoot);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/warmup"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://w3.org"_s } }
        },
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];
    [webView waitForPendingMouseEvents];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame } }
        },
    });
}

TEST(SiteIsolation, SharedProcessAfterKeyDown)
{
    HTTPServer server({
        { "/warmup"_s, { "<iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    NSURL *dataStoreRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterKeyDownDataStore"] isDirectory:YES];
    NSURL *itpRoot = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"SharedProcessAfterKeyDownTestITP"] isDirectory:YES];
    auto defaultFileManager = [NSFileManager defaultManager];
    NSURL *itpDatabaseFile = [itpRoot URLByAppendingPathComponent:@"observations.db"];
    [defaultFileManager removeItemAtPath:itpDatabaseFile.path error:nil];
    EXPECT_FALSE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    [defaultFileManager createDirectoryAtURL:itpRoot withIntermediateDirectories:YES attributes:nil error:nil];
    NSURL *sourceFile = [NSBundle.test_resourcesBundle URLForResource:@"basicITPDatabase" withExtension:@"db"];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:sourceFile.path]);
    [defaultFileManager copyItemAtPath:sourceFile.path toPath:itpDatabaseFile.path error:nil];
    EXPECT_TRUE([defaultFileManager fileExistsAtPath:itpDatabaseFile.path]);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, dataStoreRoot, itpRoot);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/warmup"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://apple.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://w3.org"_s } }
        },
    });

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView typeCharacter:'n'];
    [webView waitForNextPresentationUpdate];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { "https://w3.org"_s } }
        },
    });
}

TEST(SiteIsolation, SharedProcessAfterUserInteractionInSharedProcesss)
{
    HTTPServer server({
        { "/warmup"_s, { "<iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/payload"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    __block RetainPtr<TestWKWebView> opendWebView;
    auto uiDelegate = adoptNS([[TestUIDelegate new] init]);
    auto *sharedNavigationDelegate = navigationDelegate.get();
    webView.get().UIDelegate = uiDelegate.get();
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        opendWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 200) configuration:configuration]);
        opendWebView.get().navigationDelegate = sharedNavigationDelegate;
        return opendWebView.get();
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/warmup"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://w3.org"_s } }
        },
    });

    [webView evaluateJavaScript:@"w = window.open('https://w3.org/w3c', 'newWindow')" completionHandler:nil];
    [navigationDelegate waitForDidFinishNavigation];

    EXPECT_EQ([webView mainFrame].childFrames.firstObject.info._processIdentifier, [opendWebView mainFrame].info._processIdentifier);

    [opendWebView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];
    [opendWebView waitForPendingMouseEvents];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/payload"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s }, { "https://w3.org"_s } }
        },
    });
}


TEST(SiteIsolation, SharedProcessWebProcessCacheSharedProcessForSiteWithUserInteraction)
{
    HTTPServer server({
        { "/empty"_s, { ""_s } },
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><!DOCTYPE html><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "webkit"_s } },
        { "/w3c"_s, { "w3c"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::Yes);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { "https://apple.com"_s } }
        },
    });
    auto mainFrameProcess = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1 = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2 = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_NE(childFrameProcess1, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1, childFrameProcess2);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/apple"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://apple.com"_s,
        },
    });
    auto mainFrameProcessB = [webView mainFrame].info._processIdentifier;
    EXPECT_NE(mainFrameProcessB, mainFrameProcess);

    [webView sendClicksAtPoint:NSMakePoint(50, 50) numberOfClicks:1];
    [webView waitForPendingMouseEvents];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame } },
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s } },
        },
    });
    auto mainFrameProcessC = [webView mainFrame].info._processIdentifier;
    auto childFrameProcess1C = [webView mainFrame].childFrames[0].info._processIdentifier;
    auto childFrameProcess2C = [webView mainFrame].childFrames[1].info._processIdentifier;
    EXPECT_EQ(mainFrameProcessC, mainFrameProcess);
    EXPECT_EQ(childFrameProcess1C, childFrameProcess1);
    EXPECT_NE(childFrameProcess2C, childFrameProcess2);
}

TEST(SiteIsolation, SharedProcessWithUserInteractionOverride)
{
    HTTPServer server({
        { "/example"_s, { "<iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe><iframe src='https://w3.org/w3c'></iframe>"_s } },
        { "/apple"_s, { "apple content"_s } },
        { "/webkit"_s, { "webkit content"_s } },
        { "/w3c"_s, { "w3c content"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server, EnableProcessCache::No, nil, nil, @"apple.com");
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    checkFrameTreesInProcesses(webView.get(), {
        {
            "https://example.com"_s,
            { { RemoteFrame }, { RemoteFrame }, { RemoteFrame } }
        },
        {
            RemoteFrame,
            { { "https://webkit.org"_s }, { RemoteFrame }, { "https://w3.org"_s } }
        },
        {
            RemoteFrame,
            { { RemoteFrame }, { "https://apple.com"_s }, { RemoteFrame } }
        }
    });
}

#endif

static auto advanceFocusAcrossFramesMainFrame = R"FOCUSRESOURCE(
<script>

function sendResult(msg) {
    window.webkit.messageHandlers.testHandler.postMessage(msg);
}

function postResult(event) {
    sendResult(event.data)
}

addEventListener('message', postResult, false);

</script>
<div id="div1" tabindex="1">Main 1</div><br>
<div id="div2" tabindex="2">Main 2</div><br>
<iframe id="iframe1" src="https://webkit.org/iframe"></iframe><br>
<script>
document.body.addEventListener("focus", (event) => {
    sendResult('main - focus body', '*');
});
document.getElementById("div1").addEventListener("focus", (event) => {
    sendResult('main - focus div1', '*');
});
document.getElementById("div2").addEventListener("focus", (event) => {
    sendResult('main - focus div2', '*');
});
document.getElementById("iframe1").addEventListener("focus", (event) => {
    sendResult('main - focus iframe element', '*');
});
</script>
)FOCUSRESOURCE"_s;

static auto advanceFocusAcrossFramesChildFrame = R"FOCUSRESOURCE(
<div id="div1" tabindex="1">Child 1</div><br>
<div id="div2" tabindex="2">Child 2</div><br>
<div id="log">Initial logging</div>
<script>
document.body.addEventListener("focus", (event) => {
    parent.postMessage('iframe - focus body', '*');
});
document.getElementById("div1").addEventListener("focus", (event) => {
    parent.postMessage('iframe - focus div1', '*');
});
document.getElementById("div2").addEventListener("focus", (event) => {
    parent.postMessage('iframe - focus div2', '*');
});
</script>
)FOCUSRESOURCE"_s;

// FIXME: To enable, need `typeCharacter:` support for TestWKWebView on iOS
#if PLATFORM(MAC)
TEST(SiteIsolation, AdvanceFocusAcrossFrames)
{
    HTTPServer server({
        { "/example"_s, { advanceFocusAcrossFramesMainFrame } },
        { "/iframe"_s, { advanceFocusAcrossFramesChildFrame } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto webViewAndDelegates = makeWebViewAndDelegates(server);
    auto webView = WTFMove(webViewAndDelegates.webView);
    auto messageHandler = WTFMove(webViewAndDelegates.messageHandler);
    auto navigationDelegate = WTFMove(webViewAndDelegates.navigationDelegate);
    auto uiDelegate = WTFMove(webViewAndDelegates.uiDelegate);

    __block RetainPtr<NSString> mostRecentMessage;
    __block bool messageReceived = false;
    [messageHandler setDidReceiveScriptMessage:^(NSString *message) {
        mostRecentMessage = message;
        messageReceived = true;
    }];

    uiDelegate.get().takeFocus = ^(WKWebView *, _WKFocusDirection) {
        mostRecentMessage = @"Chrome focus taken";
        messageReceived = true;
    };

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    NSArray *expectedMessages = @[
        @"main - focus div1",
        @"main - focus div1",
        @"main - focus div2",
        @"iframe - focus div1",
        @"iframe - focus div2",
        @"Chrome focus taken"
    ];
    size_t currentExpected = 0;

    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);
    messageReceived = false;
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);

    messageReceived = false;
    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);

    messageReceived = false;
    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);

    messageReceived = false;
    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);

    messageReceived = false;
    [webView typeCharacter:'\t'];
    Util::run(&messageReceived);
    EXPECT_TRUE([mostRecentMessage isEqualToString:expectedMessages[currentExpected++]]);
}
#endif // PLATFORM(MAC)

TEST(SiteIsolation, HitTesting)
{
    auto text = "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum "_s;

    HTTPServer server({
        { "/example"_s, { makeString(
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<iframe id=iframeid1 src='https://webkit.org/webkitframe'></iframe>"
            "<iframe id=iframeid2 src='/exampleframe'></iframe>"
            "<div id=mainframediv>"_s, text, text, "</div>"_s
        ) } },
        { "/webkitframe"_s, { makeString("<div id=webkitiframediv>"_s, text, text, "</div>"_s) } },
        { "/exampleframe"_s, { makeString("<div id=exampleiframediv>"_s, text, text, "</div>"_s) } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto hitTestResult = [] (RetainPtr<WKWebView> webView, CGPoint point, WKFrameInfo *coordinateFrame = nil) -> RetainPtr<_WKJSHandle> {
        __block bool done { false };
        __block RetainPtr<_WKJSHandle> result;
        [webView _hitTestAtPoint:point inFrameCoordinateSpace:coordinateFrame completionHandler:^(_WKJSHandle *node, NSError *error) {
            done = true;
            EXPECT_NE(!node, !error);
            result = node;
        }];
        Util::run(&done);
        return result;
    };

    auto hitNodePrototypeAndParentElement = [&] (RetainPtr<TestWKWebView> webView, CGPoint point, WKFrameInfo *coordinateFrame = nil) -> NSString * {
        auto node = hitTestResult(webView, point, coordinateFrame);
        EXPECT_EQ([node world], WKContentWorld.pageWorld);
        if (!node)
            return @"(error)";
        return [webView objectByCallingAsyncFunction:@"return Object.getPrototypeOf(n).toString() + ' ' + n.id + ', child of ' + n.parentElement?.id" withArguments:@{ @"n" : node.get() } inFrame:node.get().frame inContentWorld:WKContentWorld.pageWorld];
    };

    auto runTest = [&] (bool withSiteIsolation) {
        RetainPtr configuration = server.httpsProxyConfiguration();
        if (withSiteIsolation)
            enableSiteIsolation(configuration.get());

        constexpr size_t widthWiderThanTwoIframes { 650 };
        constexpr size_t heightShorterThanHitTestCoordinates { 100 };
        RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, widthWiderThanTwoIframes, heightShorterThanHitTestCoordinates) configuration:configuration.get()]);
#if PLATFORM(MAC)
        // on iOS this is a race condition because iOS proactively launches the web content process,
        // which sometimes makes a main frame before the hit test request and sometimes does not.
        EXPECT_FALSE(hitTestResult(webView, CGPointMake(100, 100)));
#endif

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
        [webView _test_waitForDidFinishNavigationWhileIgnoringSSLErrors];

        auto hitTestPointInMainFrame = [=] (size_t x, size_t y, const char* expected) {
            EXPECT_WK_STREQ(hitNodePrototypeAndParentElement(webView, CGPointMake(x, y)), expected);
        };
        hitTestPointInMainFrame(40, 40, "[object Text] undefined, child of webkitiframediv");
        hitTestPointInMainFrame(340, 40, "[object Text] undefined, child of exampleiframediv");
        hitTestPointInMainFrame(40, 240, "[object Text] undefined, child of mainframediv");
        hitTestPointInMainFrame(300, 240, "[object Text] undefined, child of mainframediv");
        hitTestPointInMainFrame(340, 300, "[object Text] undefined, child of mainframediv");

        RetainPtr iframe = [webView firstChildFrame];
        auto hitTestPointInIFrame = [=] (size_t x, size_t y, const char* expected) {
            EXPECT_WK_STREQ(hitNodePrototypeAndParentElement(webView, CGPointMake(x, y), iframe.get()), expected);
        };
        hitTestPointInIFrame(10, 10, "[object Text] undefined, child of webkitiframediv");
        hitTestPointInIFrame(260, 160, "[object HTMLDivElement] webkitiframediv, child of ");
        hitTestPointInIFrame(300, 220, "[object HTMLHtmlElement] , child of undefined");
    };
    runTest(true);
    runTest(false);
}

TEST(SiteIsolation, WKFrameInfo_isSameFrame)
{
    HTTPServer server({
        { "/example"_s, { "<!DOCTYPE html><iframe src='https://webkit.org/webkit'></iframe><iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/webkit"_s, { "hello"_s } },
        { "/apple"_s, { "world"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewWithSharedProcess(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block RetainPtr<WKFrameInfo> mainFrameInfo;
    [webView _frames:^(_WKFrameTreeNode *result) {
        mainFrameInfo = result.info;
    }];
    while (!mainFrameInfo)
        Util::spinRunLoop();

    __block RetainPtr<_WKFrameTreeNode> frames;
    [webView _frames:^(_WKFrameTreeNode *result) {
        frames = result;
    }];
    while (!frames)
        Util::spinRunLoop();

    EXPECT_TRUE([mainFrameInfo _isSameFrame:[frames info]]);
    EXPECT_FALSE([mainFrameInfo _isSameFrame:[[[frames childFrames]objectAtIndex:0] info]]);
    EXPECT_FALSE([mainFrameInfo _isSameFrame:[[[frames childFrames]objectAtIndex:1] info]]);
}

#if ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)
TEST(SiteIsolation, DragSourceEndedAtCoordinateTransformation)
{
    static constexpr ASCIILiteral mainframeHTML = "<script>"
    "    window.events = [];"
    "    addEventListener('message', function(event) {"
    "        window.events.push(event.data);"
    "    });"
    "</script>"
    "<iframe width='300' height='300' style='position: absolute; top: 200px; left: 200px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; padding: 0; width: 100%; height: 100vh; background-color: lightblue;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue; position: absolute; top: 50px; left: 50px;'>Drag me</div>"
    "<script>"
    "    const draggable = document.getElementById('draggable');"
    "    draggable.addEventListener('dragstart', (event) => {"
    "        parent.postMessage('dragstart:' + event.clientX + ',' + event.clientY, '*');"
    "    });"
    "    draggable.addEventListener('dragend', (event) => {"
    "        parent.postMessage('dragend:' + event.clientX + ',' + event.clientY, '*');"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration]);
    RetainPtr webView = [simulator webView];
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(300, 300) to:CGPointMake(350, 350)];

    NSArray<NSString *> *events = [webView objectByEvaluatingJavaScript:@"window.events"];
    EXPECT_GT(events.count, 0U);

    bool foundDragStart = false;
    bool foundDragEnd = false;
    NSString *dragEndEvent = nil;

    for (NSString *event in events) {
        if ([event hasPrefix:@"dragstart:"]) {
            foundDragStart = true;
        } else if ([event hasPrefix:@"dragend:"]) {
            foundDragEnd = true;
            dragEndEvent = event;
        }
    }

    EXPECT_TRUE(foundDragStart) << "Should have received dragstart event in remote frame";
    EXPECT_TRUE(foundDragEnd) << "Should have received dragend event in remote frame";

    if (dragEndEvent) {
        NSString *coords = [dragEndEvent substringFromIndex:[@"dragend:" length]];
        NSArray *components = [coords componentsSeparatedByString:@","];
        if (components.count == 2) {
            int x = [components[0] intValue];
            int y = [components[1] intValue];
            EXPECT_TRUE(x >= 190 && x <= 200) << "Expected dragend x coordinate around 196, got " << x;
            EXPECT_TRUE(y >= 95 && y <= 105) << "Expected dragend y coordinate around 100, got " << y;
        }
    }
}

TEST(SiteIsolation, DragImageLocation)
{
    static constexpr ASCIILiteral mainframeHTML = "<iframe width='300' height='300' style='position: absolute; top: 200px; left: 200px; border: 2px solid red;' src='https://domain2.com/subframe'></iframe>"_s;

    static constexpr ASCIILiteral subframeHTML = "<body style='margin: 0; padding: 0; width: 100%; height: 100vh; background-color: lightblue;'>"
    "<div id='draggable' draggable='true' style='width: 100px; height: 100px; background-color: blue; position: absolute; top: 50px; left: 50px;'>Drag me</div>"
    "<script>"
    "    const draggable = document.getElementById('draggable');"
    "    draggable.addEventListener('dragstart', (event) => {"
    "        e.dataTransfer.setData('text/plain', this.textContent);"
    "    });"
    "</script>"
    "</body>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainframeHTML } },
        { "/subframe"_s, { subframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    RetainPtr webView = [simulator webView];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [simulator runFrom:CGPointMake(300, 300) to:CGPointMake(350, 350)];

    EXPECT_EQ([simulator initialDragImageLocationInView], NSMakePoint(252, 352));
}

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(MAC)

TEST(SiteIsolation, AlternateRequest)
{
    auto alertLocation = "<script>alert(window.location)</script>"_s;
    HTTPServer server({
        { "/example1"_s, { "<script>window.location = 'https://example.com/example2'</script>"_s } },
        { "/example2"_s, { alertLocation } },
        { "/example3"_s, { alertLocation } },
        { "/webkit1"_s, { "<script>window.location = 'https://example.com/example4'</script>"_s } },
        { "/webkit2"_s, { alertLocation } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    navigationDelegate.get().decidePolicyForNavigationActionWithPreferences = ^(WKNavigationAction *action, WKWebpagePreferences *preferences, void (^completionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        NSString *url = action.request.URL.absoluteString;
        if ([url isEqualToString:@"https://example.com/example2"])
            preferences._alternateRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example3"]];
        if ([url isEqualToString:@"https://example.com/example3"])
            EXPECT_FALSE(true);
        if ([url isEqualToString:@"https://example.com/example4"])
            preferences._alternateRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit2"]];
        completionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example1"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "https://example.com/example3");
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://webkit.org/webkit1"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "https://webkit.org/webkit2");
}

TEST(SiteIsolation, StatusBarVisibility)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit', '_blank', 'popup=no,menubar=no,status=no,toolbar=no,resizable=no,location=no,scrollbars=no,fullscreen=no')</script>"_s } },
        { "/webkit"_s, { "<iframe src='https://apple.com/apple'></iframe>"_s } },
        { "/apple"_s, { "hi"_s } },
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server);
    NSString *statusBarVisible = @"window.statusbar.visible";
    EXPECT_TRUE([[opener.webView objectByEvaluatingJavaScript:statusBarVisible] boolValue]);
    EXPECT_FALSE([[opened.webView objectByEvaluatingJavaScript:statusBarVisible] boolValue]);
    EXPECT_FALSE([[opened.webView objectByEvaluatingJavaScript:statusBarVisible inFrame:[opened.webView firstChildFrame]] boolValue]);
    EXPECT_TRUE([opener.webView _statusBarIsVisible]);
    EXPECT_FALSE([opened.webView _statusBarIsVisible]);
    [opened.webView _setStatusBarIsVisible:YES];
    EXPECT_TRUE([[opened.webView objectByEvaluatingJavaScript:statusBarVisible] boolValue]);
    EXPECT_TRUE([[opened.webView objectByEvaluatingJavaScript:statusBarVisible inFrame:[opened.webView firstChildFrame]] boolValue]);
}

TEST(SiteIsolation, LocalIframeOpensBlobURLFromFileMainFrame)
{
    auto iframeHTML = "<script>"
    "const htmlContent = ` <!DOCTYPE html> <html> <h1>Blob URL Loaded</h1> <script>window.webkit.messageHandlers.testHandler.postMessage('blob url loaded');<\\/script> </html> `;"
    "const blob = new Blob([htmlContent], { type: 'text/html' });"
    "const blobUrl = URL.createObjectURL(blob);"
    "const newWindow = window.open(blobUrl, '_blank', 'width=800,height=600,scrollbars=yes,resizable=yes');"
    "window.parent.postMessage('ping', '*');"
    "</script>"
    "<h1>blob-popup-local-iframe</h1>"_s;

    HTTPServer server({
    { "/blob-popup-local-iframe.html"_s, { iframeHTML } },
    }, HTTPServer::Protocol::Http, nullptr, nullptr, 8001);

    auto configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration);

    RetainPtr messageHandler = adoptNS([TestMessageHandler new]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);

    __block RetainPtr<WKWebView> blobWindow;
    __block bool blobWindowOpened = false;
    __block bool blobContentChecked = false;

    auto uiDelegate = adoptNS([TestUIDelegate new]);
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        EXPECT_WK_STREQ([action.request.URL scheme], @"blob");
        blobWindowOpened = true;
        blobWindow = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        return blobWindow.get();
    };

    [webView setUIDelegate:uiDelegate.get()];
    webView.get().configuration.preferences.javaScriptCanOpenWindowsAutomatically = YES;

    [messageHandler addMessage:@"blob url loaded" withHandler:^{
        blobContentChecked = true;
    }];

    NSURL *file = [NSBundle.test_resourcesBundle URLForResource:@"blob-popup-file-mainframe" withExtension:@"html"];
    [webView loadFileURL:file allowingReadAccessToURL:file.URLByDeletingLastPathComponent];

    TestWebKitAPI::Util::run(&blobContentChecked);

    EXPECT_TRUE(blobWindowOpened);
    EXPECT_NOT_NULL(blobWindow.get());
}

#if PLATFORM(MAC)

TEST(SiteIsolation, ColorInputPickerLocation)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe style='margin: 100px; width: 400px; height: 300px;' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><input style='margin: 50px; appearance: none; width: 50px; height: 50px;' type='color'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    __block bool done = false;
    __block NSRect popoverPositioningRect = NSZeroRect;
    __block RetainPtr<NSView> popoverPositioningView;

    InstanceMethodSwizzler swizzler {
        NSPopover.class,
        @selector(showRelativeToRect:ofView:preferredEdge:),
        imp_implementationWithBlock(^(id, NSRect positioningRect, NSView *positioningView, NSRectEdge) {
            popoverPositioningRect = positioningRect;
            popoverPositioningView = positioningView;
            done = true;
        })
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    [webView sendClickAtPoint:NSMakePoint(200, 400)];

    Util::run(&done);

    EXPECT_EQ(popoverPositioningRect, NSMakeRect(0, 0, 50, 50));

    NSRect popoverPositioningViewBoundsInWebViewCoordinates = [popoverPositioningView convertRect:[popoverPositioningView bounds] toView:webView.get()];
    EXPECT_EQ(popoverPositioningViewBoundsInWebViewCoordinates, NSMakeRect(168, 168, 50, 50));
}

#endif

#if ENABLE(IMAGE_ANALYSIS)

static RetainPtr<WKWebViewConfiguration> createWebViewConfigurationWithTextRecognitionEnhancements()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"TextRecognitionInVideosEnabled"] || [key isEqualToString:@"VisualTranslationEnabled"] || [key isEqualToString:@"RemoveBackgroundEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }
    [configuration _setAttachmentElementEnabled:YES];
#if ENABLE(SERVICE_CONTROLS)
    [configuration _setImageControlsEnabled:YES];
#endif
    return configuration;
}

TEST(SiteIsolation, IframeImageTranslation)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    HTTPServer server({
        { "/example"_s, { "<iframe src='https://apple.com/multiple-images.html'></iframe>"_s } },
        { "/multiple-images.html"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-images" withExtension:@"html"]] } },
        { "/large-red-square.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-200px.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-200px" withExtension:@"png"]] } },
        { "/test.jpg"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"jpg"]] } },
        { "/400x400-green.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"400x400-green" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-100px.tiff"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-100px" withExtension:@"tiff"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = createWebViewConfigurationWithTextRecognitionEnhancements();

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    enableSiteIsolation(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebViewImageAnalysisTests alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView _startImageAnalysis:nil target:nil];
    [webView waitForImageAnalysisRequests:5];
    gDidProcessRequestCount = 0;
}

TEST(SiteIsolation, IframeImageTranslationIfIframeIsAddedAfterTranslationCall)
{
    auto requestSwizzler = makeImageAnalysisRequestSwizzler(processRequestWithResults);

    HTTPServer server({
        { "/example"_s, {
        "<script>"
        "    function addCrossDomainIframe() {"
        "        const iframe = document.createElement('iframe');"
        "        iframe.src = 'https://apple.com/multiple-images.html';"
        "        iframe.width = '100%';"
        "        iframe.height = '700';"
        "        iframe.style='border:none';"
        "        document.body.appendChild(iframe);"
        "    }"
        "</script>"_s } },

        { "/multiple-images.html"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-images" withExtension:@"html"]] } },
        { "/large-red-square.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"large-red-square" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-200px.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-200px" withExtension:@"png"]] } },
        { "/test.jpg"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"jpg"]] } },
        { "/400x400-green.png"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"400x400-green" withExtension:@"png"]] } },
        { "/sunset-in-cupertino-100px.tiff"_s, { [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-100px" withExtension:@"tiff"]] } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = createWebViewConfigurationWithTextRecognitionEnhancements();

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    enableSiteIsolation(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebViewImageAnalysisTests alloc] initWithFrame:CGRectMake(0, 0, 600, 600) configuration:configuration.get()]);
    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];
    [navigationDelegate waitForDidFinishNavigation];

    [webView _startImageAnalysis:nil target:nil];

    [webView evaluateJavaScript: @"addCrossDomainIframe();" completionHandler:nil];
    [webView waitForImageAnalysisRequests:5];

    NSArray<NSString *> *overlaysAsText = [webView objectByEvaluatingJavaScript:@"imageOverlaysAsText();" inFrame:[webView firstChildFrame]];
    EXPECT_EQ(overlaysAsText.count, 5U);
    for (NSString *overlayText in overlaysAsText)
        EXPECT_WK_STREQ(overlayText, @"Foo bar");

    gDidProcessRequestCount = 0;
}

#endif // ENABLE(IMAGE_ANALYSIS)

#if PLATFORM(MAC)
TEST(SiteIsolation, AccessibilityTokenAfterPageNavigation)
{
    HTTPServer server({
        { "/example"_s, { "<script>w = window.open('https://webkit.org/webkit')</script>"_s } },
        { "/webkit"_s, { ""_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [opener, opened] = openerAndOpenedViews(server);
    [opened.webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://apple.com/webkit"]]];
    [opened.navigationDelegate waitForDidFinishNavigation];

    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return [opened.webView hasRemoteAccessibilityChild];
    }));
}
#endif // PLATFORM(MAC)

#if HAVE(UIFINDINTERACTION)

TEST(SiteIsolation, FindStringInFrameIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Hello world", searchOptions.get(), 1UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Missing string", searchOptions.get(), 0UL);
}

TEST(SiteIsolation, FindStringInNestedFrameIOS)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://domain2.com/subframe'></iframe>"_s } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nested_subframe'></iframe>"_s } },
        { "/nested_subframe"_s, { "<p>Hello world</p>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Hello world", searchOptions.get(), 1UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Missing string", searchOptions.get(), 0UL);
}

TEST(SiteIsolation, FindStringAcrossMultipleFramesIOS)
{
    const auto mainFrameSrc = "<iframe src='https://domain2.com/subframe'></iframe>"
    "<iframe src='https://domain3.com/subframe2'></iframe>"
    "<p>foobar</p>"
    "<iframe src='https://domain4.com/subframe3'></iframe>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameSrc } },
        { "/subframe"_s, { "<iframe src='https://domain3.com/nested_subframe'></iframe>"_s } },
        { "/nested_subframe"_s, { "<p>I am going to write a bunch of words and the word foobar will be somewhere in the middle.</p>"_s } },
        { "/subframe2"_s, { "<iframe src='https://domain5.com/nested_subframe2'></iframe>"_s } },
        { "/nested_subframe2"_s, { "<p>nested foobarfoobar</p>"_s } },
        { "/subframe3"_s, { "<p>foobar</p>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://domain1.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"foobar", searchOptions.get(), 5UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"nothing", searchOptions.get(), 0UL);
}

#endif

TEST(SiteIsolation, MainPageNavigatesCrossOriginIframeToAboutBlank)
{
    HTTPServer server({
        { "/example"_s, { "<iframe id='iframe1' src='https://webkit.org/webkit'></iframe>"_s } },
        { "/webkit"_s, { "<script>alert('loaded iframe1');</script>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    // Load main page
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/example"]]];

    // Wait until cross-origin iframe is loaded
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded iframe1");

    // Before the main page navigates the iframe, check that
    // the iframe is in a separate process.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { RemoteFrame } }
        },
        { RemoteFrame,
            { { "https://webkit.org"_s } }
        },
    });

    // Have the main frame navigate a cross-origin child iframe
    // to about:blank.
    // The about:blank iframe should inherit the origin of it's parent,
    // or it's opener if the parent doesn't exist.
    // https://dev.w3.org/html5/spec-LC/origin-0.html
    // https://dev.w3.org/html5/spec-LC/browsers.html#about-blank-origin
    //
    // iframe goes from "https://example.com/example" -> "about:blank"
    // and inherits the origin of the origin which initiated navigation.
    [webView evaluateJavaScript:
        @"let iframe1 = document.getElementById('iframe1');"
        "iframe1.onload = () => { alert('loaded about:blank'); };"
        "iframe1.src = 'about:blank';"
    completionHandler:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded about:blank");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    pid_t mainFramePid = mainFrame.info._processIdentifier;
    pid_t childFramePid = childFrame.info._processIdentifier;
    EXPECT_NE(mainFramePid, 0);
    EXPECT_NE(childFramePid, 0);
    EXPECT_EQ(mainFramePid, childFramePid);
    EXPECT_WK_STREQ(mainFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");

    // After navigation, the cross-origin iframe has now become about:blank
    // and should have the same origin as the main page.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s } }
        },
    });
}

TEST(SiteIsolation, ChildIframeNavigatesCrossOriginGrandchildIframeToAboutBlank)
{
    HTTPServer server({
        { "/main"_s, { "<iframe id='child' src='https://example.com/child_iframe'></iframe>"_s } },
        { "/child_iframe"_s, { "<iframe id='grandchild' src='https://webkit.org/grandchild_iframe'></iframe>"_s } },
        { "/grandchild_iframe"_s, { "<script>alert('loaded webkit.org');</script>"_s } },
    }, HTTPServer::Protocol::HttpsProxy);
    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/main"]]];

    // wait for cross-origin grandchild iframe to be loaded
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded webkit.org");

    // ensure that the cross-origin grandchild iframe is in a separate process
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s, { { RemoteFrame } } } }
        },
        { RemoteFrame,
            { { RemoteFrame, { { "https://webkit.org"_s } } } }
        },
    });

    // note that this JavaScript gets executed in the context of the
    // child iframe (which is at example.com).
    //
    // The example.com child iframe navigates the webkit.org grandchild iframe.
    // to about:blank
    [webView evaluateJavaScript:
        @"let grandchild = document.getElementById('grandchild');"
        "grandchild.onload = () => { alert('loaded about:blank'); };"
        "grandchild.src = 'about:blank';"
    inFrame:[webView firstChildFrame]
    completionHandler:nil];

    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded about:blank");

    auto mainFrame = [webView mainFrame];
    auto childFrame = mainFrame.childFrames.firstObject;
    auto grandChildFrame = childFrame.childFrames.firstObject;
    pid_t childFramePid = childFrame.info._processIdentifier;
    pid_t grandChildFramePid = grandChildFrame.info._processIdentifier;
    EXPECT_NE(childFramePid, 0);
    EXPECT_NE(grandChildFramePid, 0);
    EXPECT_EQ(childFramePid, grandChildFramePid);
    EXPECT_WK_STREQ(childFrame.info.securityOrigin.host, "example.com");
    EXPECT_WK_STREQ(grandChildFrame.info.securityOrigin.host, "example.com");

    // After navigation, the cross-origin iframe has now become about:blank
    // and should have the same origin as the main page.
    checkFrameTreesInProcesses(webView.get(), {
        { "https://example.com"_s,
            { { "https://example.com"_s,  { { "https://example.com"_s } } } }
        },
    });
}

TEST(SiteIsolation, BrowsingContextGroupSwitchForIncompatibleCrossOriginOpenerPolicy)
{
    HTTPServer server({
        { "/coop-unsafe-none"_s, { { { "Cross-Origin-Opener-Policy"_s, "unsafe-none"_s } }, "<script>w = window.open('https://webkit.org/coop-same-origin-allow-popups')</script>"_s } },
        { "/coop-same-origin-allow-popups"_s, { { { "Cross-Origin-Opener-Policy"_s, "same-origin-allow-popups"_s } }, "child"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [opener, opened] = openerAndOpenedViews(server, @"https://example.com/coop-unsafe-none");

    EXPECT_WK_STREQ(opened.webView.get().URL.host, "webkit.org");
    checkFrameTreesInProcesses(opener.webView.get(), {
        { "https://example.com"_s },
    });
    checkFrameTreesInProcesses(opened.webView.get(), {
        { "https://webkit.org"_s },
    });
}

}
