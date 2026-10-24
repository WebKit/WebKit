/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WebExtensionUtilities.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SetForScope.h>
#import <wtf/text/MakeString.h>

namespace TestWebKitAPI {

static constexpr auto reportToParentScript = "<script>"
    "parent.postMessage({ host: location.hostname, cookie: document.cookie }, '*')"
    "</script>"_s;

static NSDictionary *manifestWithLocalhostHostPermission()
{
    return @{
        @"manifest_version": @3,

        @"name": @"Unpartitioned Storage Test",
        @"description": @"Unpartitioned Storage Test",
        @"version": @"1",

        @"host_permissions": @[ @"*://localhost/*" ],

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },
    };
}

static NSDictionary *extensionResources(TestWebKitAPI::HTTPServer& server)
{
    auto *permittedURL = [NSString stringWithFormat:@"http://localhost:%d/read", server.port()];
    auto *otherURL = [NSString stringWithFormat:@"http://127.0.0.1:%d/read", server.port()];

    auto *extensionPageScript = Util::constructScript(@[
        @"const results = { }",
        @"let remaining = 2",

        @"window.addEventListener('message', (event) => {",
        @"  results[event.data.host] = event.data",
        @"  if (!--remaining)",
        @"    browser.test.sendMessage('Frames Reported', results)",
        @"})",

        [NSString stringWithFormat:@"for (const url of ['%@', '%@']) {", permittedURL, otherURL],
        @"  const iframe = document.createElement('iframe')",
        @"  iframe.src = url",
        @"  document.body.appendChild(iframe)",
        @"}",
    ]);

    return @{
        @"background.js": Util::constructScript(@[ @"browser.test.sendMessage('Ready')" ]),
        @"extension-page.js": extensionPageScript,
        @"extension-page.html": @"<body><script type='module' src='extension-page.js'></script></body>",
    };
}

static void seedFirstPartyState(TestWebExtensionManager *manager, TestWebKitAPI::HTTPServer& server, NSURLRequest *request)
{
    auto expectedRequests = server.totalRequests() + 2;

    [manager.defaultTab changeWebViewIfNeededForURL:request.URL forExtensionContext:manager.context];
    [manager.defaultTab.webView loadRequest:request];

    EXPECT_TRUE(Util::waitFor([&] {
        Util::runFor(0.05_s);
        return server.totalRequests() >= expectedRequests;
    }));
}

static NSDictionary *loadExtensionPageAndCollectFrameReports(TestWebExtensionManager *manager)
{
    auto *extensionPageURL = [NSURL URLWithString:@"extension-page.html" relativeToURL:manager.context.baseURL];

    [manager.defaultTab changeWebViewIfNeededForURL:extensionPageURL forExtensionContext:manager.context];
    [manager.defaultTab.webView loadRequest:[NSURLRequest requestWithURL:extensionPageURL]];

    return [manager runUntilTestMessage:@"Frames Reported"];
}

TEST(WKWebExtensionUnpartitionedStorage, SameSiteCookiesAreSentToHostPermittedFrame)
{
    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=lax-cookie; SameSite=Lax; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, reportToParentScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), extensionResources(server));

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/read"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.requestWithLocalhost("/seed"_s));
    seedFirstPartyState(manager.get(), server, server.request("/seed"_s));

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_NS_EQUAL(results[@"localhost"][@"cookie"], @"key=lax-cookie");

    EXPECT_NS_EQUAL(results[@"127.0.0.1"][@"cookie"], @"");
}

TEST(WKWebExtensionUnpartitionedStorage, SameSiteCookiesAreSentToHostPermittedFrameWithSiteIsolation)
{
    SetForScope siteIsolation { Util::shouldEnableSiteIsolationForWebExtensionsTest, true };

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=lax-cookie; SameSite=Lax; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, reportToParentScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), extensionResources(server));

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/read"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.requestWithLocalhost("/seed"_s));
    seedFirstPartyState(manager.get(), server, server.request("/seed"_s));

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_NS_EQUAL(results[@"localhost"][@"cookie"], @"key=lax-cookie");

    EXPECT_NS_EQUAL(results[@"127.0.0.1"][@"cookie"], @"");
}

TEST(WKWebExtensionUnpartitionedStorage, HostPermissionWithPathAppliesToWholeSite)
{
    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=lax-cookie; SameSite=Lax; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, reportToParentScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    NSMutableDictionary *manifest = [manifestWithLocalhostHostPermission() mutableCopy];
    manifest[@"host_permissions"] = @[ @"*://localhost/read*" ];

    auto manager = Util::loadExtension(manifest, extensionResources(server));

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:[WKWebExtensionMatchPattern matchPatternWithString:@"*://localhost/read*"]];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.requestWithLocalhost("/seed"_s));
    seedFirstPartyState(manager.get(), server, server.request("/seed"_s));

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_NS_EQUAL(results[@"localhost"][@"cookie"], @"key=lax-cookie");

    EXPECT_NS_EQUAL(results[@"127.0.0.1"][@"cookie"], @"");
}

static constexpr auto reportNamedFrameToTopScript = "<script>"
    "top.postMessage({ name: window.name }, '*')"
    "</script>"_s;

static NSDictionary *extensionResourcesEmbeddingNamedFrames(NSDictionary<NSString *, NSString *> *namesToURLs)
{
    auto *frames = [NSMutableArray array];
    for (NSString *name in namesToURLs)
        [frames addObject:[NSString stringWithFormat:@"['%@', '%@']", name, namesToURLs[name]]];

    auto *extensionPageScript = Util::constructScript(@[
        @"const results = { }",
        [NSString stringWithFormat:@"let remaining = %lu", (unsigned long)namesToURLs.count],

        @"window.addEventListener('message', (event) => {",
        @"  results[event.data.name] = event.data",
        @"  if (!--remaining)",
        @"    browser.test.sendMessage('Frames Reported', results)",
        @"})",

        [NSString stringWithFormat:@"for (const [name, url] of [%@]) {", [frames componentsJoinedByString:@", "]],
        @"  const iframe = document.createElement('iframe')",
        @"  iframe.name = name",
        @"  iframe.src = url",
        @"  document.body.appendChild(iframe)",
        @"}",
    ]);

    return @{
        @"background.js": Util::constructScript(@[ @"browser.test.sendMessage('Ready')" ]),
        @"extension-page.js": extensionPageScript,
        @"extension-page.html": @"<body><script type='module' src='extension-page.js'></script></body>",
    };
}

TEST(WKWebExtensionUnpartitionedStorage, RedirectOutOfHostPermittedFrameIsPartitioned)
{
    static constexpr auto navigateToRedirectScript = "<script>location.href = '/redirect-to-other'</script>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=lax-cookie; SameSite=Lax; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/start"_s, { { { "Content-Type"_s, "text/html"_s } }, navigateToRedirectScript } },
        { "/redirected"_s, { { { "Content-Type"_s, "text/html"_s } }, reportNamedFrameToTopScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);
    server.addResponse("/redirect-to-other"_s, { 302, { { "Location"_s, makeString("http://127.0.0.1:"_s, server.port(), "/redirected"_s) } }, "redirecting..."_s });

    auto *resources = extensionResourcesEmbeddingNamedFrames(@{
        @"redirected": [NSString stringWithFormat:@"http://localhost:%d/start", server.port()],
    });

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), resources);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/start"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.request("/seed"_s));

    loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_WK_STREQ(server.lastRequestCookies(), "");
}

TEST(WKWebExtensionUnpartitionedStorage, RedirectOutOfHostPermittedFrameIsPartitionedWithSiteIsolation)
{
    SetForScope siteIsolation { Util::shouldEnableSiteIsolationForWebExtensionsTest, true };

    static constexpr auto navigateToRedirectScript = "<script>location.href = '/redirect-to-other'</script>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=lax-cookie; SameSite=Lax; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/start"_s, { { { "Content-Type"_s, "text/html"_s } }, navigateToRedirectScript } },
        { "/redirected"_s, { { { "Content-Type"_s, "text/html"_s } }, reportNamedFrameToTopScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);
    server.addResponse("/redirect-to-other"_s, { 302, { { "Location"_s, makeString("http://127.0.0.1:"_s, server.port(), "/redirected"_s) } }, "redirecting..."_s });

    auto *resources = extensionResourcesEmbeddingNamedFrames(@{
        @"redirected": [NSString stringWithFormat:@"http://localhost:%d/start", server.port()],
    });

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), resources);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/start"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.request("/seed"_s));

    loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_WK_STREQ(server.lastRequestCookies(), "");
}

static constexpr auto reportNamedFrameCookieToTopScript = "<script>"
    "top.postMessage({ name: window.name, cookie: document.cookie }, '*')"
    "</script>"_s;

static void runRedirectIntoHostPermittedFrameTest(bool siteIsolationEnabled)
{
    SetForScope siteIsolation { Util::shouldEnableSiteIsolationForWebExtensionsTest, siteIsolationEnabled };

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=lax-cookie; SameSite=Lax; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, reportNamedFrameCookieToTopScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);
    server.addResponse("/redirect-to-permitted"_s, { 302, { { "Location"_s, makeString("http://localhost:"_s, server.port(), "/read"_s) } }, "redirecting..."_s });

    auto *resources = extensionResourcesEmbeddingNamedFrames(@{
        @"redirected": [NSString stringWithFormat:@"http://127.0.0.1:%d/redirect-to-permitted", server.port()],
    });

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), resources);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/read"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.requestWithLocalhost("/seed"_s));

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_WK_STREQ(server.lastRequestCookies(), "key=lax-cookie");
    EXPECT_NS_EQUAL(results[@"redirected"][@"cookie"], @"key=lax-cookie");
}

TEST(WKWebExtensionUnpartitionedStorage, RedirectIntoHostPermittedFrameIsUnpartitioned)
{
    runRedirectIntoHostPermittedFrameTest(false);
}

TEST(WKWebExtensionUnpartitionedStorage, RedirectIntoHostPermittedFrameIsUnpartitionedWithSiteIsolation)
{
    runRedirectIntoHostPermittedFrameTest(true);
}

TEST(WKWebExtensionUnpartitionedStorage, AboutBlankChildOfHostPermittedFrameSeesItsCookies)
{
    static constexpr auto reportAboutBlankChildToParentScript = "<body><script>"
        "const child = document.body.appendChild(document.createElement('iframe'));"
        "parent.postMessage({ host: location.hostname, cookie: child.contentDocument.cookie }, '*')"
        "</script></body>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=cookie; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, reportAboutBlankChildToParentScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), extensionResources(server));

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/read"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.requestWithLocalhost("/seed"_s));
    seedFirstPartyState(manager.get(), server, server.request("/seed"_s));

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_NS_EQUAL(results[@"localhost"][@"cookie"], @"key=cookie");
    EXPECT_NS_EQUAL(results[@"127.0.0.1"][@"cookie"], @"");
}

static NSDictionary *extensionResourcesEmbeddingFrame(NSString *frameURL)
{
    auto *extensionPageScript = Util::constructScript(@[
        @"window.addEventListener('message', (event) => {",
        @"  if (event.data === 'Checked')",
        @"    browser.test.sendMessage('Checked')",
        @"})",

        @"const frame = document.createElement('iframe')",
        [NSString stringWithFormat:@"frame.src = '%@'", frameURL],
        @"document.body.appendChild(frame)",
    ]);

    return @{
        @"background.js": Util::constructScript(@[ @"browser.test.sendMessage('Ready')" ]),
        @"extension-page.js": extensionPageScript,
        @"extension-page.html": @"<body><script type='module' src='extension-page.js'></script></body>",
    };
}

static void loadExtensionPageAndWaitForCheck(TestWebExtensionManager *manager)
{
    auto *extensionPageURL = [NSURL URLWithString:@"extension-page.html" relativeToURL:manager.context.baseURL];

    [manager.defaultTab changeWebViewIfNeededForURL:extensionPageURL forExtensionContext:manager.context];
    [manager.defaultTab.webView loadRequest:[NSURLRequest requestWithURL:extensionPageURL]];

    [manager runUntilTestMessage:@"Checked"];
}

TEST(WKWebExtensionUnpartitionedStorage, CrossSiteNavigationOfHostPermittedFrameDoesNotSendStrictCookies)
{
    static constexpr auto checkedScript = "<script>parent.postMessage('Checked', '*')</script>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=strict-cookie; SameSite=Strict; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/idle"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
        { "/check"_s, { { { "Content-Type"_s, "text/html"_s } }, checkedScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionPageScript = Util::constructScript(@[
        @"window.addEventListener('message', (event) => {",
        @"  if (event.data === 'Checked')",
        @"    browser.test.sendMessage('Checked')",
        @"})",

        @"const frame = document.createElement('iframe')",
        [NSString stringWithFormat:@"frame.onload = () => { frame.onload = null; frame.src = 'http://localhost:%d/check' }", server.port()],
        [NSString stringWithFormat:@"frame.src = 'http://localhost:%d/idle'", server.port()],
        @"document.body.appendChild(frame)",
    ]);

    auto *resources = @{
        @"background.js": Util::constructScript(@[ @"browser.test.sendMessage('Ready')" ]),
        @"extension-page.js": extensionPageScript,
        @"extension-page.html": @"<body><script type='module' src='extension-page.js'></script></body>",
    };

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), resources);

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/check"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.requestWithLocalhost("/seed"_s));

    loadExtensionPageAndWaitForCheck(manager.get());

    EXPECT_WK_STREQ(server.lastRequestCookies(), "");
}

TEST(WKWebExtensionUnpartitionedStorage, SameSiteNavigationOfHostPermittedFrameSendsStrictCookies)
{
    static constexpr auto checkedScript = "<script>parent.postMessage('Checked', '*')</script>"_s;

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=strict-cookie; SameSite=Strict; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/navigate"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>location.href = '/check'</script>"_s } },
        { "/check"_s, { { { "Content-Type"_s, "text/html"_s } }, checkedScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *permittedURL = [NSString stringWithFormat:@"http://localhost:%d/navigate", server.port()];

    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), extensionResourcesEmbeddingFrame(permittedURL));

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/check"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, server.requestWithLocalhost("/seed"_s));

    loadExtensionPageAndWaitForCheck(manager.get());

    EXPECT_WK_STREQ(server.lastRequestCookies(), "key=strict-cookie");
}

TEST(WKWebExtensionUnpartitionedStorage, NonExtensionPageDoesNotGiveSubframesUnpartitionedAccess)
{
    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=lax-cookie; SameSite=Lax; Path=/"_s } }, ""_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>top.postMessage(document.cookie, '*')</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);
    server.addResponse("/main"_s, { { { "Content-Type"_s, "text/html"_s } }, makeString("<script>onmessage = (event) => alert(event.data)</script><iframe src='http://localhost:"_s, server.port(), "/read'></iframe>"_s) });

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:WKWebsiteDataStore.nonPersistentDataStore];
    [configuration _setShouldRelaxThirdPartyCookieBlocking:YES];
    [configuration _setCORSDisablingPatterns:@[ @"*://localhost/*" ]];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadRequest:server.requestWithLocalhost("/seed"_s)];
    [webView loadRequest:server.request("/main"_s)];

    EXPECT_WK_STREQ([webView _test_waitForAlert], "");
}

static WKWebExtensionControllerConfiguration *controllerConfigurationWithHTTPSProxy(TestWebKitAPI::HTTPServer& server)
{
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];

    RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    [dataStore _setResourceLoadStatisticsEnabled:YES];

    auto *configuration = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
    configuration.defaultWebsiteDataStore = dataStore.get();
    return configuration;
}

static NSDictionary *manifestWithHostPermission(NSString *hostPermission)
{
    NSMutableDictionary *manifest = [[manifestWithLocalhostHostPermission() mutableCopy] autorelease];
    manifest[@"host_permissions"] = @[ hostPermission ];
    return manifest;
}

static void runSameSiteNavigationToOtherHostOfHostPermittedFrameTest(bool siteIsolationEnabled)
{
    SetForScope siteIsolation { Util::shouldEnableSiteIsolationForWebExtensionsTest, siteIsolationEnabled };

    TestWebKitAPI::HTTPServer server({
        { "/seed"_s, { { { "Content-Type"_s, "text/html"_s }, { "Set-Cookie"_s, "key=strict-cookie; SameSite=Strict; Secure; Domain=a.com; Path=/"_s } }, "<script>fetch('/seeded')</script>"_s } },
        { "/seeded"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s } },
        { "/navigate"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>location.href = 'https://nested.a.com/read'</script>"_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, reportNamedFrameCookieToTopScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    auto *resources = extensionResourcesEmbeddingNamedFrames(@{
        @"navigated": @"https://a.com/navigate",
    });

    auto manager = Util::loadExtension(manifestWithHostPermission(@"*://a.com/*"), resources, controllerConfigurationWithHTTPSProxy(server));

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:[WKWebExtensionMatchPattern matchPatternWithString:@"*://a.com/*"]];

    [manager runUntilTestMessage:@"Ready"];

    seedFirstPartyState(manager.get(), server, [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://a.com/seed"]]);

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_WK_STREQ(server.lastRequestCookies(), "key=strict-cookie");
    EXPECT_NS_EQUAL(results[@"navigated"][@"cookie"], @"key=strict-cookie");
}

TEST(WKWebExtensionUnpartitionedStorage, SameSiteNavigationToOtherHostOfHostPermittedFrameIsUnpartitioned)
{
    runSameSiteNavigationToOtherHostOfHostPermittedFrameTest(false);
}

TEST(WKWebExtensionUnpartitionedStorage, SameSiteNavigationToOtherHostOfHostPermittedFrameIsUnpartitionedWithSiteIsolation)
{
    runSameSiteNavigationToOtherHostOfHostPermittedFrameTest(true);
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION

TEST(WKWebExtensionUnpartitionedStorage, RedirectOutOfHostPermittedFrameDoesNotUseItsCookiePartition)
{
    TestWebKitAPI::HTTPServer server({
        { "/redirect-to-tracker"_s, { 302, { { "Location"_s, "https://tracker.com/redirected"_s } }, "redirecting..."_s } },
        { "/redirected"_s, { { { "Content-Type"_s, "text/html"_s } }, reportNamedFrameToTopScript } },
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    auto *resources = extensionResourcesEmbeddingNamedFrames(@{
        @"redirected": @"https://a.com/redirect-to-tracker",
    });

    auto manager = Util::loadExtension(manifestWithHostPermission(@"*://a.com/*"), resources, controllerConfigurationWithHTTPSProxy(server));

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:[WKWebExtensionMatchPattern matchPatternWithString:@"*://a.com/*"]];

    [manager runUntilTestMessage:@"Ready"];

    auto *partitionedCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"key",
        NSHTTPCookieValue: @"partitioned-cookie",
        NSHTTPCookieDomain: @"tracker.com",
        NSHTTPCookiePath: @"/",
        NSHTTPCookieSecure: @"TRUE",
        @"StoragePartition": @"https://a.com",
    }];

    __block bool didSetCookie = false;
    [manager.get().controller.configuration.defaultWebsiteDataStore.httpCookieStore setCookie:partitionedCookie completionHandler:^{
        didSetCookie = true;
    }];
    Util::run(&didSetCookie);

    loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_WK_STREQ(server.lastRequestCookies(), "");
}

#endif // ENABLE(OPT_IN_PARTITIONED_COOKIES) && defined(CFN_COOKIE_ACCEPTS_POLICY_PARTITION) && CFN_COOKIE_ACCEPTS_POLICY_PARTITION

static NSString * const writeLocalStorageScript = @"localStorage.setItem('key', 'local-storage-value')";
static NSString * const readLocalStorageScript = @"return localStorage.getItem('key') ?? ''";

static NSString * const writeIndexedDBScript = @""
    "const database = await new Promise((resolve, reject) => {"
    "  const request = indexedDB.open('database');"
    "  request.onupgradeneeded = () => request.result.createObjectStore('store');"
    "  request.onsuccess = () => resolve(request.result);"
    "  request.onerror = () => reject(request.error);"
    "});"
    "const transaction = database.transaction('store', 'readwrite');"
    "transaction.objectStore('store').put('indexeddb-value', 'key');"
    "await new Promise((resolve, reject) => {"
    "  transaction.oncomplete = resolve;"
    "  transaction.onerror = () => reject(transaction.error);"
    "});"
    "database.close();";
static NSString * const readIndexedDBScript = @""
    "if (!(await indexedDB.databases()).some(database => database.name === 'database'))"
    "  return '';"
    "const database = await new Promise((resolve, reject) => {"
    "  const request = indexedDB.open('database');"
    "  request.onsuccess = () => resolve(request.result);"
    "  request.onerror = () => reject(request.error);"
    "});"
    "const request = database.transaction('store').objectStore('store').get('key');"
    "const value = await new Promise((resolve, reject) => {"
    "  request.onsuccess = () => resolve(request.result);"
    "  request.onerror = () => reject(request.error);"
    "});"
    "database.close();"
    "return value ?? '';";

static NSString * const writeCacheStorageScript = @"await (await caches.open('cache')).put('/cached', new Response('cache-storage-value'))";
static NSString * const readCacheStorageScript = @""
    "const response = await caches.match('/cached');"
    "return response ? await response.text() : '';";

static NSString * const writeFileSystemScript = @""
    "const root = await navigator.storage.getDirectory();"
    "const writable = await (await root.getFileHandle('file', { create: true })).createWritable();"
    "await writable.write('file-system-value');"
    "await writable.close();";
static NSString * const readFileSystemScript = @""
    "const root = await navigator.storage.getDirectory();"
    "try {"
    "  return await (await (await root.getFileHandle('file')).getFile()).text();"
    "} catch (error) {"
    "  if (error.name === 'NotFoundError')"
    "    return '';"
    "  throw error;"
    "}";

static NSString * const holdWebLockScript = @"await new Promise(resolve => navigator.locks.request('lock', () => { resolve(); return new Promise(() => { }) }))";
static NSString * const readHeldWebLocksScript = @"return (await navigator.locks.query()).held.map(lock => lock.name).join(', ')";

static String scriptReportingResult(NSString *script, ASCIILiteral report)
{
    return makeString("let value;"_s
        "try {"_s
        "  value = await (async () => { "_s, String(script), " })();"_s
        "} catch (error) {"_s
        "  value = String(error);"_s
        "}"_s, report);
}

static TestWebKitAPI::HTTPServer storageTestServer(NSString *frameScript, NSString *workerScript = nil)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
        { "/read"_s, { { { "Content-Type"_s, "text/html"_s } }, makeString("<script type='module'>"_s, scriptReportingResult(frameScript, "parent.postMessage({ host: location.hostname, value }, '*')"_s), "</script>"_s) } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    if (workerScript) {
        server.addResponse("/worker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, scriptReportingResult(workerScript, "postMessage(value)"_s) });
        server.addResponse("/shared-worker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, makeString("const result = (async () => { "_s, scriptReportingResult(workerScript, "return value;"_s), " })();"_s
            "onconnect = async event => event.ports[0].postMessage(await result);"_s) });
    }

    return server;
}

static WKWebsiteDataStore *persistentDataStore()
{
    static NeverDestroyed<RetainPtr<WKWebsiteDataStore>> dataStore = [] {
        RetainPtr directory = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString] isDirectory:YES];
        RetainPtr configuration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initWithDirectory:directory.get()]);
        return adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:configuration.get()]);
    }();
    return dataStore.get().get();
}

static WKWebExtensionControllerConfiguration *controllerConfigurationWithPersistentDataStore()
{
    auto *dataStore = persistentDataStore();

    __block bool removedData = false;
    [dataStore removeDataOfTypes:WKWebsiteDataStore.allWebsiteDataTypes modifiedSince:NSDate.distantPast completionHandler:^{
        removedData = true;
    }];
    Util::run(&removedData);

    auto *configuration = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
    configuration.defaultWebsiteDataStore = dataStore;
    return configuration;
}

static RetainPtr<TestWebExtensionManager> loadStorageTestExtension(TestWebKitAPI::HTTPServer& server)
{
    auto manager = Util::loadExtension(manifestWithLocalhostHostPermission(), extensionResources(server), controllerConfigurationWithPersistentDataStore());

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:server.requestWithLocalhost("/read"_s).URL];

    [manager runUntilTestMessage:@"Ready"];

    return manager;
}

static RetainPtr<TestWKWebView> loadFirstPartyPage(TestWebExtensionManager *manager, NSURLRequest *request, NSString *script)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:manager.controller.configuration.defaultWebsiteDataStore];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadRequest:request];

    NSError *error = nil;
    [webView objectByCallingAsyncFunction:script withArguments:nil error:&error];
    EXPECT_NULL(error);

    return webView;
}

static void runUnpartitionedStorageTest(NSString *firstPartyScript, NSString *frameScript, NSString *expectedValue, NSString *workerScript = nil)
{
    auto server = storageTestServer(frameScript, workerScript);
    auto manager = loadStorageTestExtension(server);

    auto permittedFirstPartyWebView = loadFirstPartyPage(manager.get(), server.requestWithLocalhost(), firstPartyScript);
    auto otherFirstPartyWebView = loadFirstPartyPage(manager.get(), server.request(), firstPartyScript);

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_NS_EQUAL(results[@"localhost"][@"value"], expectedValue);
    EXPECT_NS_EQUAL(results[@"127.0.0.1"][@"value"], @"");
}

TEST(WKWebExtensionUnpartitionedStorage, LocalStorageIsUnpartitionedInHostPermittedFrame)
{
    runUnpartitionedStorageTest(writeLocalStorageScript, readLocalStorageScript, @"local-storage-value");
}

TEST(WKWebExtensionUnpartitionedStorage, IndexedDBIsUnpartitionedInHostPermittedFrame)
{
    runUnpartitionedStorageTest(writeIndexedDBScript, readIndexedDBScript, @"indexeddb-value");
}

TEST(WKWebExtensionUnpartitionedStorage, CacheStorageIsUnpartitionedInHostPermittedFrame)
{
    runUnpartitionedStorageTest(writeCacheStorageScript, readCacheStorageScript, @"cache-storage-value");
}

TEST(WKWebExtensionUnpartitionedStorage, FileSystemIsUnpartitionedInHostPermittedFrame)
{
    runUnpartitionedStorageTest(writeFileSystemScript, readFileSystemScript, @"file-system-value");
}

TEST(WKWebExtensionUnpartitionedStorage, WebLocksAreUnpartitionedInHostPermittedFrame)
{
    runUnpartitionedStorageTest(holdWebLockScript, readHeldWebLocksScript, @"lock");
}

TEST(WKWebExtensionUnpartitionedStorage, DedicatedWorkerOfHostPermittedFrameIsUnpartitioned)
{
    auto *workerScript = [NSString stringWithFormat:@"return [await (async () => { %@ })(), await (async () => { %@ })()].filter(Boolean).join(', ')", readIndexedDBScript, readFileSystemScript];
    auto *frameScript = @"return await new Promise(resolve => new Worker('/worker.js', { type: 'module' }).onmessage = event => resolve(event.data))";

    runUnpartitionedStorageTest([writeIndexedDBScript stringByAppendingString:writeFileSystemScript], frameScript, @"indexeddb-value, file-system-value", workerScript);
}

TEST(WKWebExtensionUnpartitionedStorage, SharedWorkerOfHostPermittedFrameIsUnpartitioned)
{
    auto *workerScript = [NSString stringWithFormat:@"return [await (async () => { %@ })(), await (async () => { %@ })()].filter(Boolean).join(', ')", readIndexedDBScript, readCacheStorageScript];
    auto *frameScript = @""
        "return await new Promise(resolve => {"
        "  const worker = new SharedWorker('/shared-worker.js');"
        "  worker.onerror = () => resolve('error');"
        "  worker.port.onmessage = event => resolve(event.data);"
        "})";

    runUnpartitionedStorageTest([writeIndexedDBScript stringByAppendingString:writeCacheStorageScript], frameScript, @"indexeddb-value, cache-storage-value", workerScript);
}

TEST(WKWebExtensionUnpartitionedStorage, ServiceWorkerControlsHostPermittedFrame)
{
    NSString *fetchFromServiceWorkerScript = @"return await (await fetch('/from-service-worker')).text()";
    NSString *registerServiceWorkerScript = @""
        "await navigator.serviceWorker.register('/service-worker.js');"
        "await navigator.serviceWorker.ready;";

    auto *frameScript = [NSString stringWithFormat:@""
        "const frameValue = await (async () => { %@ })();"
        "const workerValue = await new Promise(resolve => new Worker('/worker.js', { type: 'module' }).onmessage = event => resolve(event.data));"
        "return [frameValue, workerValue].filter(Boolean).join(', ')", fetchFromServiceWorkerScript];

    auto server = storageTestServer(frameScript, fetchFromServiceWorkerScript);
    server.addResponse("/from-service-worker"_s, { { { "Content-Type"_s, "text/plain"_s } }, ""_s });
    server.addResponse("/service-worker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, ""
        "self.addEventListener('install', () => self.skipWaiting());"
        "self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));"
        "self.addEventListener('fetch', event => {"
        "  if (new URL(event.request.url).pathname === '/from-service-worker')"
        "    event.respondWith(new Response('service-worker-value'));"
        "});"_s });

    auto manager = loadStorageTestExtension(server);

    auto permittedFirstPartyWebView = loadFirstPartyPage(manager.get(), server.requestWithLocalhost(), registerServiceWorkerScript);
    auto otherFirstPartyWebView = loadFirstPartyPage(manager.get(), server.request(), registerServiceWorkerScript);

    auto *results = loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_NS_EQUAL(results[@"localhost"][@"value"], @"service-worker-value, service-worker-value");
    EXPECT_NS_EQUAL(results[@"127.0.0.1"][@"value"], @"");
}

static void copySessionStorage(WKWebView *fromWebView, WKWebView *toWebView)
{
    __block RetainPtr<NSData> sessionStorageData;
    __block bool done = false;
    [fromWebView fetchDataOfTypes:WKWebViewDataTypeSessionStorage completionHandler:^(NSData *data, NSError *error) {
        EXPECT_NULL(error);
        sessionStorageData = data;
        done = true;
    }];
    Util::run(&done);

    done = false;
    [toWebView restoreData:sessionStorageData.get() completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    Util::run(&done);
}

TEST(WKWebExtensionUnpartitionedStorage, SessionStorageIsUnpartitionedInHostPermittedFrame)
{
    auto server = storageTestServer(@"return sessionStorage.getItem('key') ?? ''");
    auto manager = loadStorageTestExtension(server);

    auto *extensionPageURL = [NSURL URLWithString:@"extension-page.html" relativeToURL:manager.get().context.baseURL];
    [manager.get().defaultTab changeWebViewIfNeededForURL:extensionPageURL forExtensionContext:manager.get().context];

    for (NSURLRequest *request in @[ server.requestWithLocalhost(), server.request() ]) {
        auto firstPartyWebView = loadFirstPartyPage(manager.get(), request, @"sessionStorage.setItem('key', 'session-storage-value'); await navigator.storage.estimate()");
        copySessionStorage(firstPartyWebView.get(), manager.get().defaultTab.webView);
    }

    [manager.get().defaultTab.webView loadRequest:[NSURLRequest requestWithURL:extensionPageURL]];

    NSDictionary *results = [manager runUntilTestMessage:@"Frames Reported"];

    EXPECT_NS_EQUAL(results[@"localhost"][@"value"], @"session-storage-value");
    EXPECT_NS_EQUAL(results[@"127.0.0.1"][@"value"], @"");
}

TEST(WKWebExtensionUnpartitionedStorage, BroadcastChannelIsUnpartitionedInHostPermittedFrame)
{
    NSString *listenScript = @""
        "window.receivedMessages = [];"
        "window.firstMessage = new Promise(resolve => {"
        "  window.channel = new BroadcastChannel('channel');"
        "  channel.onmessage = event => {"
        "    receivedMessages.push(event.data);"
        "    resolve(event.data);"
        "  };"
        "});";

    auto server = storageTestServer(@"window.channel = new BroadcastChannel('channel'); channel.postMessage(location.hostname); return ''");
    auto manager = loadStorageTestExtension(server);

    auto permittedFirstPartyWebView = loadFirstPartyPage(manager.get(), server.requestWithLocalhost(), listenScript);
    auto otherFirstPartyWebView = loadFirstPartyPage(manager.get(), server.request(), listenScript);

    loadExtensionPageAndCollectFrameReports(manager.get());

    EXPECT_NS_EQUAL([permittedFirstPartyWebView objectByCallingAsyncFunction:@"return await firstMessage" withArguments:nil], @"localhost");
    EXPECT_NS_EQUAL([otherFirstPartyWebView objectByEvaluatingJavaScript:@"receivedMessages.join(', ')"], @"");
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
