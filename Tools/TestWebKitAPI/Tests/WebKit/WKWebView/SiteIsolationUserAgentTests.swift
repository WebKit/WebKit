// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebpagePreferencesPrivate

import struct Foundation.URL
import struct Swift.String

// The user agent of a page applies to all of its frames, so every web content process that hosts one of them needs the
// user agent and whether it's custom. With site isolation, that includes processes created after the user agent is set,
// such as the processes of cross-site iframes, which get both from the page's creation parameters.

@MainActor
struct SiteIsolationUserAgentTests {
    @Test
    func customUserAgentAppliesInCrossOriginIframe() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "<body>subframe text</body>"
            }
        }

        try await server.run { serverConfiguration in
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)

            // A frame prefers the main frame's site-specific user agent to the page's user agent, unless its process
            // knows that the page's user agent is custom. Set one so that a process that doesn't know reports "QuirkUA".
            navigationDelegate.decidePolicyForNavigationActionWithPreferences = { navigationAction, preferences, decisionHandler in
                if navigationAction.targetFrame?.isMainFrame ?? false {
                    preferences._customUserAgentAsSiteSpecificQuirks = "QuirkUA"
                }
                decisionHandler(.allow, preferences)
            }
            let customUserAgent = "CustomUA"
            webView.customUserAgent = customUserAgent

            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let mainFrameUserAgent = try await webView.callJavaScript(returning: String.self) { "return navigator.userAgent" }
            #expect(mainFrameUserAgent == customUserAgent)

            let childFrame = try #require(await webView.firstChildFrame)
            let childFrameUserAgent = try await webView.callJavaScript(returning: String.self, in: childFrame) {
                "return navigator.userAgent"
            }
            #expect(childFrameUserAgent == customUserAgent)
        }
    }
}
