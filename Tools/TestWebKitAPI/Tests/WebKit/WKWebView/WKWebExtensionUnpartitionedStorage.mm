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
#import "Helpers/cocoa/WebExtensionUtilities.h"
#import <WebKit/WKWebViewPrivate.h>
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

TEST(WKWebExtensionUnpartitionedStorage, HostPermissionWithPathAppliesToWholeOrigin)
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

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
