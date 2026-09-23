/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "PlatformUtilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKSecurityOrigin.h>
#import <WebKit/WKString.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStoreConfigurationRef.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

// LocalNetworkAccessEnabled has no dedicated Obj-C setter, so flip it via the generic _WKFeature list.
static void setFeatureEnabledForConfiguration(WKWebViewConfiguration *configuration, NSString *key, BOOL enabled)
{
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:key])
            [configuration.preferences _setEnabled:enabled forFeature:feature];
    }
}

static void setLocalNetworkAccessEnabledForConfiguration(WKWebViewConfiguration *configuration, BOOL enabled)
{
    setFeatureEnabledForConfiguration(configuration, @"LocalNetworkAccessEnabled", enabled);
}

static RetainPtr<TestWKWebView> createWebViewForLocalNetworkAccessTesting(BOOL localNetworkAccessEnabled)
{
    // WebProcessPlugInWithInternals is what makes `window.internals` available to page script here.
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    setLocalNetworkAccessEnabledForConfiguration(configuration.get(), localNetworkAccessEnabled);
    // Without _allowTestOnlyIPC, NetworkProcess treats SetLocalNetworkAccessPermissionForTesting as
    // an invalid message and terminates the WebContent process instead of servicing it.
    configuration.get()._allowTestOnlyIPC = YES;
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
}

// Address-space overrides are fixed when the session is created, so they ride on the data store
// configuration. Keyed on resolved address and port, so a page staged through one has to be loaded via
// 127.0.0.1: HTTPServer also listens on IPv6, and localhost resolves to ::1.
static RetainPtr<WKWebsiteDataStore> dataStoreWithAddressSpaceOverrides(const String& overrides)
{
    auto configuration = adoptWK(WKWebsiteDataStoreConfigurationCreate());
    auto overridesString = adoptWK(WKStringCreateWithUTF8CString(overrides.utf8().legacyCStringPointer()));
    WKWebsiteDataStoreConfigurationSetIPAddressSpaceOverridesForTesting(configuration.get(), overridesString.get());
    return adoptNS((WKWebsiteDataStore *)WKWebsiteDataStoreCreateWithConfiguration(configuration.get()));
}

static RetainPtr<TestWKWebView> createWebViewWithAddressSpaceOverrides(const String& overrides)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    setLocalNetworkAccessEnabledForConfiguration(configuration.get(), YES);
    configuration.get()._allowTestOnlyIPC = YES;
    configuration.get().websiteDataStore = dataStoreWithAddressSpaceOverrides(overrides).get();
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
}

// Writes straight into the session's store, which is what lets a test skip the prompt entirely.
static void setLoopbackPermission(WKWebsiteDataStore *store, NSString *topOrigin, NSString *requestingOrigin, bool granted)
{
    bool done = false;
    auto top = adoptWK(WKStringCreateWithUTF8CString(String(topOrigin).utf8().legacyCStringPointer()));
    auto requesting = adoptWK(WKStringCreateWithUTF8CString(String(requestingOrigin).utf8().legacyCStringPointer()));
    WKWebsiteDataStoreSetLocalNetworkAccessPermissionForTesting((WKWebsiteDataStoreRef)store, top.get(), requesting.get(), true, granted, &done, [](void* context) {
        *static_cast<bool*>(context) = true;
    });
    Util::run(&done);
}



static constexpr auto crossOriginLoopbackFetchPageBytes = R"HTMLRESOURCE(
<script>
async function doFetch(targetOrigin)
{
    await fetch(targetOrigin + "/target.txt", { mode: "no-cors" })
        .then(() => window.fetchResult = "success")
        .catch(e => window.fetchResult = "error: " + e.message);
}
</script>
)HTMLRESOURCE"_s;

static String waitForFetchResult(TestWKWebView *webView)
{
    Util::waitForConditionWithLogging([&] -> bool {
        return [[webView stringByEvaluatingJavaScript:@"window.fetchResult || ''"] length] > 0;
    }, 10, @"Timed out waiting for fetch result.");
    return String { [webView stringByEvaluatingJavaScript:@"window.fetchResult"] };
}

TEST(LocalNetworkAccessTests, DisabledFeatureAllowsLoopbackFetchUnconditionally)
{
    auto server = HTTPServer({
        { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(NO);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost("/index.html"_s)];

    RetainPtr targetOrigin = server.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

// A loopback-hosted page reaching another loopback address is not reaching a *less* public address
// space, so no permission is required. The WPT navigation tests assert the same ("loopback to
// loopback: no permission required"). Before the document's own address space was seeded from its
// response it was hardcoded to Public, which made this case look less-public and block.
TEST(LocalNetworkAccessTests, CrossOriginLoopbackFetchAllowedWithoutGrant)
{
    // Loaded via requestWithLocalhost so the page origin (http://localhost:<port>) differs from the
    // fetch target (http://127.0.0.1:<port>), keeping the same-origin exemption out of it.
    auto server = HTTPServer({
        { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(YES);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost("/index.html"_s)];

    RetainPtr targetOrigin = server.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

// Granting permission must not change the outcome for a case that never needed one.
TEST(LocalNetworkAccessTests, CrossOriginLoopbackFetchSucceedsWithGrant)
{
    auto server = HTTPServer({
        { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(YES);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost("/index.html"_s)];

    NSString *pageOrigin = [webView stringByEvaluatingJavaScript:@"location.origin"];
    setLoopbackPermission([webView configuration].websiteDataStore, pageOrigin, pageOrigin, true);

    RetainPtr targetOrigin = server.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

TEST(LocalNetworkAccessTests, SameOriginTargetAddressSpaceFetchSucceedsWithoutGrant)
{
    // This exemption is unconditional and doesn't consult the permission stub, unlike the
    // cross-origin permission-gated path covered by the two tests above.
    auto server = HTTPServer({
        { "/index.html"_s, { "<script>window.pageLoaded = true;</script>"_s } },
        { "/target.txt"_s, { "hello"_s } },
    });

    auto webView = createWebViewForLocalNetworkAccessTesting(YES);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    NSString *fetchScript =
        @"(async () => {"
        "  try {"
        "    await fetch(location.origin + '/target.txt', { targetAddressSpace: 'loopback' });"
        "    window.fetchResult = 'success';"
        "  } catch (e) {"
        "    window.fetchResult = 'error: ' + e.message;"
        "  }"
        "})(); undefined";
    [webView objectByEvaluatingJavaScript:fetchScript];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

// Both the page and the fetch target below are *public* hostnames; the only loopback address in play
// is the proxy itself. remoteAddress reports the near peer, so classifying from it would make both
// look like loopback and block the fetch. A proxied connection is therefore treated as public: what
// is local to the proxy is not local to us.
static RetainPtr<TestWKWebView> createProxiedWebViewForLocalNetworkAccessTesting(BOOL localNetworkAccessEnabled, uint16_t proxyPort)
{
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(proxyPort),
    }];
    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    setLocalNetworkAccessEnabledForConfiguration(configuration.get(), localNetworkAccessEnabled);
    configuration.get()._allowTestOnlyIPC = YES;
    [configuration setWebsiteDataStore:dataStore.get()];
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
}

static HTTPServer makeLocalNetworkAccessProxy()
{
    return HTTPServer(HTTPServer::UseCoroutines::Yes, [](Connection connection) -> ConnectionTask {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "http://example.com/index.html"_s)
                co_await connection.awaitableSend(HTTPResponse(String { crossOriginLoopbackFetchPageBytes }).serialize());
            else if (path == "http://other.example/target.txt"_s)
                co_await connection.awaitableSend(HTTPResponse("hello"_s).serialize());
            else
                co_await connection.awaitableSend(HTTPResponse(404, { }, { }).serialize());
        }
    });
}

// Without the proxy check this fetch is blocked: the proxy answers from 127.0.0.1, so the page and
// the target both classify as loopback, and a page that believes it is public reaching loopback is a
// less-public request that needs a permission no one granted.
TEST(LocalNetworkAccessTests, ProxiedPublicFetchIsNotClassifiedFromProxyAddress)
{
    auto proxy = makeLocalNetworkAccessProxy();
    auto webView = createProxiedWebViewForLocalNetworkAccessTesting(YES, proxy.port());
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/index.html"]]];

    [webView objectByEvaluatingJavaScript:@"doFetch('http://other.example'); undefined"];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

// Control: with the feature off the check never runs, so this must succeed either way. If it fails,
// the proxy harness itself is broken rather than the classification.
TEST(LocalNetworkAccessTests, ProxiedPublicFetchControlWithFeatureDisabled)
{
    auto proxy = makeLocalNetworkAccessProxy();
    auto webView = createProxiedWebViewForLocalNetworkAccessTesting(NO, proxy.port());
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/index.html"]]];

    [webView objectByEvaluatingJavaScript:@"doFetch('http://other.example'); undefined"];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

// A public client is staged with two servers: the page server's port is mapped
// public by the override, and the fetch target sits on a second, unmapped port that keeps the loopback
// space its resolved address gives it. One server cannot do both jobs -- the override is keyed on
// address and port, so it would classify the target public too.
static HTTPServer makePageServer()
{
    return HTTPServer({
        { "/index.html"_s, { String { crossOriginLoopbackFetchPageBytes } } },
    });
}

static HTTPServer makeLoopbackTargetServer()
{
    return HTTPServer({
        { "/target.txt"_s, { "hello"_s } },
    });
}

static RetainPtr<TestWKWebView> loadPublicPageOverLoopback(HTTPServer& pageServer, BOOL grantPermission)
{
    RetainPtr webView = createWebViewWithAddressSpaceOverrides(makeString("127.0.0.1:"_s, pageServer.port(), "=public"_s));
    [webView synchronouslyLoadRequest:pageServer.request("/index.html"_s)];

    if (grantPermission) {
        NSString *pageOrigin = [webView stringByEvaluatingJavaScript:@"location.origin"];
        setLoopbackPermission([webView configuration].websiteDataStore, pageOrigin, pageOrigin, true);
    }
    return webView;
}

TEST(LocalNetworkAccessTests, PublicPageFetchingLoopbackIsBlockedWithoutGrant)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    auto webView = loadPublicPageOverLoopback(server, NO);

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));
}

TEST(LocalNetworkAccessTests, PublicPageFetchingLoopbackIsAllowedWithGrant)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    auto webView = loadPublicPageOverLoopback(server, YES);

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

// A local-space document reaching loopback needs the loopback-network permission: loopback is less
// public than local. The spec requires this ("lhs is loopback and rhs is either local or public"), but
// Chromium does not enforce it for cross-origin local requests, so no WPT row covers it -- every check
// in fetch/local-network-access/iframe.tentative.https.window.html runs with a public or loopback
// client, never a local one. These two tests are the only end-to-end coverage of the rule.
static RetainPtr<TestWKWebView> loadLocalSpacePageReachingLoopback(HTTPServer& pageServer, BOOL grantPermission)
{
    // The page is served over loopback like everything else here, so it is classified local only
    // because of the override. It stays a secure context either way, since 127.0.0.1 is potentially
    // trustworthy -- without that the check would fail on the secure-context gate instead.
    RetainPtr webView = createWebViewWithAddressSpaceOverrides(makeString("127.0.0.1:"_s, pageServer.port(), "=local"_s));
    [webView synchronouslyLoadRequest:pageServer.request("/index.html"_s)];

    if (grantPermission) {
        NSString *pageOrigin = [webView stringByEvaluatingJavaScript:@"location.origin"];
        setLoopbackPermission([webView configuration].websiteDataStore, pageOrigin, pageOrigin, true);
    }
    return webView;
}

TEST(LocalNetworkAccessTests, LocalToLoopbackFetchBlockedWithoutGrant)
{
    auto pageServer = HTTPServer({ { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } } });
    auto targetServer = HTTPServer({ { "/target.txt"_s, { "hello"_s } } });

    auto webView = loadLocalSpacePageReachingLoopback(pageServer, NO);

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));
}

TEST(LocalNetworkAccessTests, LocalToLoopbackFetchAllowedWithGrant)
{
    auto pageServer = HTTPServer({ { "/index.html"_s, { crossOriginLoopbackFetchPageBytes } } });
    auto targetServer = HTTPServer({ { "/target.txt"_s, { "hello"_s } } });

    auto webView = loadLocalSpacePageReachingLoopback(pageServer, YES);

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
}

} // namespace TestWebKitAPI

// The prompt is the only part of the feature an embedder implements, and it is reached through this
// delegate. Nothing else in the suite exercises it: every other test writes the decision straight into
// the network session's store, which skips the request entirely.
@interface TestLocalNetworkAccessUIDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic) _WKLocalNetworkAccessDecision decision;
@property (nonatomic) BOOL deferDecisions;
@property (nonatomic, readonly) NSUInteger promptCount;
@property (nonatomic, readonly) BOOL lastWasLoopback;
- (NSString *)lastRequestingOrigin;
- (NSString *)lastTopLevelOrigin;
- (void)answerDeferredDecisions;
@end

@implementation TestLocalNetworkAccessUIDelegate {
    Vector<BlockPtr<void(_WKLocalNetworkAccessDecision)>> _deferred;
    RetainPtr<NSString> _lastRequestingOrigin;
    RetainPtr<NSString> _lastTopLevelOrigin;
}

static RetainPtr<NSString> originDescription(WKSecurityOrigin *origin)
{
    return adoptNS([[NSString alloc] initWithFormat:@"%@://%@:%ld", origin.protocol, origin.host, (long)origin.port]);
}

- (void)_webView:(WKWebView *)webView requestLocalNetworkAccessPermissionForSecurityOrigin:(WKSecurityOrigin *)securityOrigin topLevelOrigin:(WKSecurityOrigin *)topLevelOrigin isLoopback:(BOOL)isLoopback decisionHandler:(void (^)(_WKLocalNetworkAccessDecision))decisionHandler
{
    ++_promptCount;
    _lastRequestingOrigin = originDescription(securityOrigin);
    _lastTopLevelOrigin = originDescription(topLevelOrigin);
    _lastWasLoopback = isLoopback;

    if (_deferDecisions) {
        _deferred.append(makeBlockPtr(decisionHandler));
        return;
    }
    decisionHandler(_decision);
}

- (NSString *)lastRequestingOrigin
{
    return _lastRequestingOrigin.get();
}

- (NSString *)lastTopLevelOrigin
{
    return _lastTopLevelOrigin.get();
}

- (void)answerDeferredDecisions
{
    for (auto& handler : std::exchange(_deferred, { }))
        handler(_decision);
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebViewForPromptTesting(TestLocalNetworkAccessUIDelegate *delegate, HTTPServer& pageServer)
{
    RetainPtr webView = createWebViewWithAddressSpaceOverrides(makeString("127.0.0.1:"_s, pageServer.port(), "=public"_s));
    [webView setUIDelegate:delegate];
    return webView;
}

static constexpr auto twoLoopbackFetchesPageBytes = R"HTMLRESOURCE(
<script>
async function doTwoFetches(targetOrigin)
{
    const settle = (promise) => promise.then(() => "success", () => "error");
    const results = await Promise.all([
        settle(fetch(targetOrigin + "/target.txt", { mode: "no-cors" })),
        settle(fetch(targetOrigin + "/other.txt", { mode: "no-cors" })),
    ]);
    window.fetchResult = results.join(",");
}
</script>
)HTMLRESOURCE"_s;

TEST(LocalNetworkAccessTests, PromptAllowingTheRequestLetsTheFetchThrough)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionGrant;

    auto webView = createWebViewForPromptTesting(delegate.get(), server);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
    EXPECT_EQ(1u, delegate.get().promptCount);
    EXPECT_TRUE(delegate.get().lastWasLoopback);
}

TEST(LocalNetworkAccessTests, PromptDenyingTheRequestBlocksTheFetch)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionDeny;

    auto webView = createWebViewForPromptTesting(delegate.get(), server);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));
    EXPECT_EQ(1u, delegate.get().promptCount);
}

// The prompt names the requesting origin and the top-level origin separately, because the grant is
// keyed on the pair. Asserting both catches the two being swapped, which no outcome-only test would.
TEST(LocalNetworkAccessTests, PromptNamesBothOriginsOfThePair)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionGrant;

    auto webView = createWebViewForPromptTesting(delegate.get(), server);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    NSString *pageOrigin = [webView stringByEvaluatingJavaScript:@"location.origin"];
    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");

    EXPECT_WK_STREQ(delegate.get().lastRequestingOrigin, pageOrigin);
    EXPECT_WK_STREQ(delegate.get().lastTopLevelOrigin, pageOrigin);
}

// Answering records the decision, so a later request for the same origin pair and address space must
// use it rather than asking again.
TEST(LocalNetworkAccessTests, AnsweredPromptIsNotRaisedASecondTime)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionGrant;

    auto webView = createWebViewForPromptTesting(delegate.get(), server);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
    EXPECT_EQ(1u, delegate.get().promptCount);

    [webView objectByEvaluatingJavaScript:@"window.fetchResult = ''; undefined"];
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");
    EXPECT_EQ(1u, delegate.get().promptCount);
}

// Two requests in flight for the same address space raise one prompt, not two. The decision is held
// until both are outstanding, so a single answer satisfying both is what proves they coalesced --
// answering immediately would let the second request find a recorded decision instead and pass either way.
TEST(LocalNetworkAccessTests, ConcurrentRequestsRaiseOnePrompt)
{
    HTTPServer targetServer({
        { "/target.txt"_s, { "hello"_s } },
        { "/other.txt"_s, { "hello"_s } },
    });
    HTTPServer server({
        { "/index.html"_s, { String { twoLoopbackFetchesPageBytes } } },
    });

    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionGrant;
    delegate.get().deferDecisions = YES;

    auto webView = createWebViewForPromptTesting(delegate.get(), server);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doTwoFetches('%@'); undefined", targetOrigin.get()]];

    Util::waitForConditionWithLogging([&] -> bool {
        return delegate.get().promptCount >= 1;
    }, 10, @"Timed out waiting for the first prompt.");
    Util::runFor(0.5_s);
    EXPECT_EQ(1u, delegate.get().promptCount);

    // Stop deferring first: if a regression raised a second prompt, deferring it would leave it
    // unanswered and the wait below would never finish, since waitForConditionWithLogging has no timeout.
    delegate.get().deferDecisions = NO;
    [delegate.get() answerDeferredDecisions];
    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success,success");
    EXPECT_EQ(1u, delegate.get().promptCount);
}

// NotNow records nothing, so unlike Grant or Deny it must not answer from a session-cached decision:
// each request has to reach the delegate again.
TEST(LocalNetworkAccessTests, NotNowRecordsNothingAndTheNextRequestAsksAgain)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionNotNow;

    auto webView = createWebViewForPromptTesting(delegate.get(), server);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));
    EXPECT_EQ(1u, delegate.get().promptCount);

    [webView objectByEvaluatingJavaScript:@"window.fetchResult = ''; undefined"];
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));
    EXPECT_EQ(2u, delegate.get().promptCount);
}

// A NotNow answer must not retarget to another page's waiter the way NotHosted does. The defer seam
// makes this deterministic: both pages' requests coalesce into the same pending entry while the first
// prompt is still outstanding, so a single NotNow answer has to satisfy both. If it wrongly retargeted,
// the second page's request would raise its own prompt and promptCount would be 2 instead of 1.
TEST(LocalNetworkAccessTests, NotNowDoesNotRetargetToAnotherPage)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionNotNow;
    delegate.get().deferDecisions = YES;

    auto webViewOne = createWebViewForPromptTesting(delegate.get(), server);
    // Same configuration, so both pages share one data store and therefore one network session.
    RetainPtr webViewTwo = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:[webViewOne configuration]]);
    [webViewTwo setUIDelegate:delegate.get()];
    [webViewOne synchronouslyLoadRequest:server.request("/index.html"_s)];
    [webViewTwo synchronouslyLoadRequest:server.request("/index.html"_s)];

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webViewOne objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    [webViewTwo objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];

    Util::waitForConditionWithLogging([&] -> bool {
        return delegate.get().promptCount >= 1;
    }, 10, @"Timed out waiting for the first prompt.");
    Util::runFor(0.5_s);
    EXPECT_EQ(1u, delegate.get().promptCount);
    EXPECT_EQ(0u, [[webViewOne stringByEvaluatingJavaScript:@"window.fetchResult || ''"] length]);
    EXPECT_EQ(0u, [[webViewTwo stringByEvaluatingJavaScript:@"window.fetchResult || ''"] length]);

    // Stop deferring first: if a regression retargeted and raised a second prompt, deferring it would
    // leave it unanswered and the waits below would never finish, since waitForConditionWithLogging has
    // no timeout.
    delegate.get().deferDecisions = NO;
    [delegate.get() answerDeferredDecisions];
    EXPECT_TRUE(waitForFetchResult(webViewOne.get()).startsWith("error:"_s));
    EXPECT_TRUE(waitForFetchResult(webViewTwo.get()).startsWith("error:"_s));
    EXPECT_EQ(1u, delegate.get().promptCount);
}

static void revokeLocalNetworkAccessPermissions(WKWebsiteDataStore *store, NSString *topOrigin)
{
    bool done = false;
    auto origin = adoptWK(WKStringCreateWithUTF8CString(String(topOrigin).utf8().legacyCStringPointer()));
    WKWebsiteDataStoreRevokeLocalNetworkAccessPermissionsForTesting((WKWebsiteDataStoreRef)store, origin.get(), &done, [](void* context) {
        *static_cast<bool*>(context) = true;
    });
    Util::run(&done);
}

static String recordedLoopbackPermission(TestWKWebView *webView)
{
    [webView objectByEvaluatingJavaScript:@"window.permissionState = ''; navigator.permissions.query({ name: 'loopback-network' }).then(status => window.permissionState = status.state); undefined"];
    Util::waitForConditionWithLogging([&] -> bool {
        return [[webView stringByEvaluatingJavaScript:@"window.permissionState"] length] > 0;
    }, 10, @"Timed out waiting for the permission query.");
    return String { [webView stringByEvaluatingJavaScript:@"window.permissionState"] };
}

// A revocation that lands while a prompt is still on screen must win: the requests waiting on it are
// refused, and the answer that arrives afterwards records nothing.
TEST(LocalNetworkAccessTests, RevokingDuringAPromptDiscardsTheLateAnswer)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    RetainPtr delegate = adoptNS([[TestLocalNetworkAccessUIDelegate alloc] init]);
    delegate.get().decision = _WKLocalNetworkAccessDecisionGrant;
    delegate.get().deferDecisions = YES;

    auto webView = createWebViewForPromptTesting(delegate.get(), server);
    [webView synchronouslyLoadRequest:server.request("/index.html"_s)];

    NSString *pageOrigin = [webView stringByEvaluatingJavaScript:@"location.origin"];
    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    Util::waitForConditionWithLogging([&] -> bool {
        return delegate.get().promptCount >= 1;
    }, 10, @"Timed out waiting for the prompt.");

    revokeLocalNetworkAccessPermissions([webView configuration].websiteDataStore, pageOrigin);
    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));

    delegate.get().deferDecisions = NO;
    [delegate.get() answerDeferredDecisions];
    Util::runFor(0.2_s);
    EXPECT_WK_STREQ(recordedLoopbackPermission(webView.get()), "prompt");
}

// With no delegate method there is nobody to ask. The request is refused, but no decision is recorded,
// since the user never made one.
TEST(LocalNetworkAccessTests, NoDelegateRefusesWithoutRecordingADecision)
{
    auto targetServer = makeLoopbackTargetServer();
    auto server = makePageServer();
    auto webView = loadPublicPageOverLoopback(server, NO);

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));
    EXPECT_WK_STREQ(recordedLoopbackPermission(webView.get()), "prompt");
}

static void setPermissionForTesting(WKWebsiteDataStore *store, NSString *origin, bool granted)
{
    setLoopbackPermission(store, origin, origin, granted);
}

// A response served from the cache is checked too. The address space is not part of the request, so a
// cache hit has to carry the space its response was originally resolved to; if it did not, revoking a
// grant would leave every already-cached local network response reachable for as long as it stayed fresh.
TEST(LocalNetworkAccessTests, CachedResponseIsStillCheckedAfterTheGrantIsRevoked)
{
    HTTPServer targetServer({
        { "/target.txt"_s, HTTPResponse({ { "Cache-Control"_s, "max-age=3600"_s } }, "hello"_s) },
    });
    auto server = makePageServer();

    auto webView = loadPublicPageOverLoopback(server, NO);

    NSString *pageOrigin = [webView stringByEvaluatingJavaScript:@"location.origin"];
    setPermissionForTesting([webView configuration].websiteDataStore, pageOrigin, true);

    RetainPtr targetOrigin = targetServer.origin().createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_WK_STREQ(waitForFetchResult(webView.get()), "success");

    setPermissionForTesting([webView configuration].websiteDataStore, pageOrigin, false);

    auto requestsBeforeSecondFetch = targetServer.totalRequests();

    [webView objectByEvaluatingJavaScript:@"window.fetchResult = ''; undefined"];
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetch('%@'); undefined", targetOrigin.get()]];
    EXPECT_TRUE(waitForFetchResult(webView.get()).startsWith("error:"_s));

    // Without this the test would pass just as well if the second fetch went to the network and was
    // refused there, which would say nothing about whether a cache hit is checked.
    EXPECT_EQ(requestsBeforeSecondFetch, targetServer.totalRequests());
}

static constexpr auto fetchURLPageBytes = R"HTMLRESOURCE(
<script>
async function doFetchURL(url)
{
    await fetch(url, { mode: "no-cors" })
        .then(() => window.fetchResult = "success")
        .catch(e => window.fetchResult = "error: " + e.message);
}
</script>
)HTMLRESOURCE"_s;

// A redirect is the only hop that didReceiveResponse never sees, so the check on the redirect is the
// only thing standing between a public page and a local network server that answers with a 302. The
// redirect is served BY the loopback server here for that reason: it is the 302's own connection that
// has to be refused, not the request that follows it.
static String runRedirectFromLoopback(bool grantPermission)
{
    HTTPServer loopbackServer({
        { "/redirect"_s, HTTPResponse(302, { { "Location"_s, "/target.txt"_s } }) },
        { "/target.txt"_s, { "hello"_s } },
    });

    HTTPServer publicServer({ { "/index.html"_s, HTTPResponse { String { fetchURLPageBytes } } } });

    // Maps the page server public, so the loopback redirect server is less public than it.
    RetainPtr webView = createWebViewWithAddressSpaceOverrides(makeString("127.0.0.1:"_s, publicServer.port(), "=public"_s));
    [webView synchronouslyLoadRequest:publicServer.request("/index.html"_s)];

    if (grantPermission)
        setPermissionForTesting([webView configuration].websiteDataStore, [webView stringByEvaluatingJavaScript:@"location.origin"], true);

    RetainPtr redirectURL = makeString(loopbackServer.origin(), "/redirect"_s).createNSString();
    [webView objectByEvaluatingJavaScript:[NSString stringWithFormat:@"doFetchURL('%@'); undefined", redirectURL.get()]];
    return waitForFetchResult(webView.get());
}

TEST(LocalNetworkAccessTests, RedirectServedFromLoopbackIsBlockedWithoutGrant)
{
    EXPECT_TRUE(runRedirectFromLoopback(false).startsWith("error:"_s));
}

TEST(LocalNetworkAccessTests, RedirectServedFromLoopbackIsAllowedWithGrant)
{
    EXPECT_WK_STREQ(runRedirectFromLoopback(true), "success");
}

} // namespace TestWebKitAPI
