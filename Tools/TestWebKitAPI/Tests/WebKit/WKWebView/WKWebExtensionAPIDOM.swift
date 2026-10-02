// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIDOMTests {
    private let domManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "DOM Test",
        "description": "DOM Test",
        "version": "1",

        "content_scripts": [
            [
                "js": ["content.js"],
                "matches": ["*://localhost/*"],
                "all_frames": true,
            ]
        ],
    ]

    @Test
    func openOrClosedShadowRoot() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let contentScript = """
            const hostOpen = document.createElement('div')
            hostOpen.id = 'host-open'
            document.body.appendChild(hostOpen)

            const shadowRootOpen = hostOpen.attachShadow({ mode: 'open' })
            shadowRootOpen.innerHTML = `<p id='open-child'>Open Child</p>`

            const hostClosed = document.createElement('div')
            hostClosed.id = 'host-closed'
            document.body.appendChild(hostClosed)

            const shadowRootClosed = hostClosed.attachShadow({ mode: 'closed' })
            shadowRootClosed.innerHTML = `<p id='closed-child'>Closed Child</p>`

            const resultOpen = browser.dom.openOrClosedShadowRoot(hostOpen)
            browser.test.assertEq(typeof resultOpen, 'object', 'Should return shadow root for open host')
            browser.test.assertEq(resultOpen?.mode, 'open', 'Returned open shadow root should have mode open')

            const resultClosed = browser.dom.openOrClosedShadowRoot(hostClosed)
            browser.test.assertEq(typeof resultClosed, 'object', 'Should return shadow root for closed host')
            browser.test.assertEq(resultClosed?.mode, 'closed', 'Returned closed shadow root should have mode closed')

            const noShadowHost = document.createElement('div')
            const resultNone = browser.dom.openOrClosedShadowRoot(noShadowHost)
            browser.test.assertEq(resultNone, null, 'Should return null for host without shadow root')

            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: domManifest, resources: ["content.js": contentScript])
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func openOrClosedShadowRootViaElement() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let contentScript = """
            const hostOpen = document.createElement('div')
            hostOpen.id = 'host-open'
            document.body.appendChild(hostOpen)

            const shadowRootOpen = hostOpen.attachShadow({ mode: 'open' })
            shadowRootOpen.innerHTML = `<p id='open-child'>Open Child</p>`

            const hostClosed = document.createElement('div')
            hostClosed.id = 'host-closed'
            document.body.appendChild(hostClosed)

            const shadowRootClosed = hostClosed.attachShadow({ mode: 'closed' })
            shadowRootClosed.innerHTML = `<p id='closed-child'>Closed Child</p>`

            const resultOpen = hostOpen.openOrClosedShadowRoot
            browser.test.assertEq(typeof resultOpen, 'object', 'Should return shadow root for open host')
            browser.test.assertEq(resultOpen?.mode, 'open', 'Returned open shadow root should have mode open')

            const resultClosed = hostClosed.openOrClosedShadowRoot
            browser.test.assertEq(typeof resultClosed, 'object', 'Should return shadow root for closed host')
            browser.test.assertEq(resultClosed?.mode, 'closed', 'Returned closed shadow root should have mode closed')

            const noShadowHost = document.createElement('div')
            const resultNone = noShadowHost.openOrClosedShadowRoot
            browser.test.assertEq(resultNone, null, 'Should return null for host without shadow root')

            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: domManifest, resources: ["content.js": contentScript])
            let context = try #require(manager.context)
            let webView = try #require(manager.defaultTab?.webView)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
