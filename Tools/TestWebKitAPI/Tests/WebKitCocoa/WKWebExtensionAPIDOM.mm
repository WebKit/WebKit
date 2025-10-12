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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "HTTPServer.h"
#import "WebExtensionUtilities.h"

namespace TestWebKitAPI {

static auto *domManifest = @{
    @"manifest_version": @3,

    @"name": @"DOM Test",
    @"description": @"DOM Test",
    @"version": @"1",

    @"content_scripts": @[ @{
        @"js": @[ @"content.js" ],
        @"matches": @[ @"*://localhost/*" ],
        @"all_frames": @YES,
    } ],
};

TEST(WKWebExtensionAPIDOM, OpenOrClosedShadowRoot)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *contentScript = Util::constructScript(@[
        @"const hostOpen = document.createElement('div')",
        @"hostOpen.id = 'host-open'",
        @"document.body.appendChild(hostOpen)",

        @"const shadowRootOpen = hostOpen.attachShadow({ mode: 'open' })",
        @"shadowRootOpen.innerHTML = `<p id='open-child'>Open Child</p>`",

        @"const hostClosed = document.createElement('div')",
        @"hostClosed.id = 'host-closed'",
        @"document.body.appendChild(hostClosed)",

        @"const shadowRootClosed = hostClosed.attachShadow({ mode: 'closed' })",
        @"shadowRootClosed.innerHTML = `<p id='closed-child'>Closed Child</p>`",

        @"const resultOpen = browser.dom.openOrClosedShadowRoot(hostOpen)",
        @"browser.test.assertEq(typeof resultOpen, 'object', 'Should return shadow root for open host')",
        @"browser.test.assertEq(resultOpen?.mode, 'open', 'Returned open shadow root should have mode open')",

        @"const resultClosed = browser.dom.openOrClosedShadowRoot(hostClosed)",
        @"browser.test.assertEq(typeof resultClosed, 'object', 'Should return shadow root for closed host')",
        @"browser.test.assertEq(resultClosed?.mode, 'closed', 'Returned closed shadow root should have mode closed')",

        @"const noShadowHost = document.createElement('div')",
        @"const resultNone = browser.dom.openOrClosedShadowRoot(noShadowHost)",
        @"browser.test.assertEq(resultNone, null, 'Should return null for host without shadow root')",

        @"browser.test.notifyPass()"
    ]);

    auto manager = Util::loadExtension(domManifest, @{ @"content.js": contentScript });

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPIDOM, OpenOrClosedShadowRootViaElement)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *contentScript = Util::constructScript(@[
        @"const hostOpen = document.createElement('div')",
        @"hostOpen.id = 'host-open'",
        @"document.body.appendChild(hostOpen)",

        @"const shadowRootOpen = hostOpen.attachShadow({ mode: 'open' })",
        @"shadowRootOpen.innerHTML = `<p id='open-child'>Open Child</p>`",

        @"const hostClosed = document.createElement('div')",
        @"hostClosed.id = 'host-closed'",
        @"document.body.appendChild(hostClosed)",

        @"const shadowRootClosed = hostClosed.attachShadow({ mode: 'closed' })",
        @"shadowRootClosed.innerHTML = `<p id='closed-child'>Closed Child</p>`",

        @"const resultOpen = hostOpen.openOrClosedShadowRoot",
        @"browser.test.assertEq(typeof resultOpen, 'object', 'Should return shadow root for open host')",
        @"browser.test.assertEq(resultOpen?.mode, 'open', 'Returned open shadow root should have mode open')",

        @"const resultClosed = hostClosed.openOrClosedShadowRoot",
        @"browser.test.assertEq(typeof resultClosed, 'object', 'Should return shadow root for closed host')",
        @"browser.test.assertEq(resultClosed?.mode, 'closed', 'Returned closed shadow root should have mode closed')",

        @"const noShadowHost = document.createElement('div')",
        @"const resultNone = noShadowHost.openOrClosedShadowRoot",
        @"browser.test.assertEq(resultNone, null, 'Should return null for host without shadow root')",

        @"browser.test.notifyPass()"
    ]);

    auto manager = Util::loadExtension(domManifest, @{ @"content.js": contentScript });

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
