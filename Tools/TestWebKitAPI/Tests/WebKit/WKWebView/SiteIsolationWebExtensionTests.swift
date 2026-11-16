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

#if ENABLE_WK_WEB_EXTENSIONS

import Foundation
private import TestWebKitAPILibrary
import Testing
import WebKit
private import WebKit_Private.WKWebExtensionPrivate

import struct Foundation.URL
import struct Swift.String

@MainActor
struct SiteIsolationWebExtensionTests {
    @Test
    func webAccessibleResourceLoadsInCrossSiteIframeWhenExtensionLoadsAfterPage() async throws {
        var server = try HTTPServer(protocol: .httpsProxy) {
            Route("/mainframe") {
                "<iframe src='https://webkit.org/iframe'></iframe>"
            }

            Route("/iframe") {
                "<body>subframe text</body>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "web_accessible_resources": [
                [
                    "resources": ["resource.txt"],
                    "matches": ["<all_urls>"],
                ]
            ],
        ]

        try await server.run { serverConfiguration in
            let controller = WKWebExtensionController(configuration: .nonPersistent())
            let configuration = WKWebViewConfiguration(httpsProxyFor: serverConfiguration)
            configuration.webExtensionController = controller

            let (webView, navigationDelegate) = siteIsolatedWebViewAndDelegate(configuration: configuration)
            webView.load(URLRequest(url: try #require(URL(string: "https://example.com/mainframe"))))
            try await navigationDelegate.waitForDidFinishNavigation()

            let webExtension = try #require(WKWebExtension(manifestDictionary: manifest, resources: ["resource.txt": "extension resource"]))
            let context = WKWebExtensionContext(for: webExtension)
            try controller.load(context)

            let childFrame = try #require(await webView.firstChildFrame)
            let resourceText = try await webView.callJavaScript(
                returning: String.self,
                in: childFrame,
                arguments: ["url": context.baseURL.appending(path: "resource.txt").absoluteString]
            ) {
                "return await (await fetch(url)).text()"
            }
            #expect(resourceText == "extension resource")
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
