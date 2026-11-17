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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit

import struct Swift.String

@MainActor
struct WKWebExtensionAPIExtensionTypesTests {
    private let extensionTypesManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "ExtensionTypes Test",
        "description": "ExtensionTypes Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    @Test
    func imageFormat() async throws {
        let backgroundScript = """
            browser.test.assertEq(typeof browser.extensionTypes, 'object', 'extensionTypes should be an object')
            browser.test.assertEq(typeof browser.extensionTypes.ImageFormat, 'object', 'ImageFormat should be an object')

            browser.test.assertEq(browser.extensionTypes.ImageFormat.JPEG, 'jpeg', 'JPEG should be defined')
            browser.test.assertEq(browser.extensionTypes.ImageFormat.PNG, 'png', 'PNG should be defined')

            browser.test.assertDeepEq(Object.keys(browser.extensionTypes.ImageFormat).sort(), [ 'JPEG', 'PNG' ], 'ImageFormat should only define the supported values')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: extensionTypesManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func runAt() async throws {
        let backgroundScript = """
            browser.test.assertEq(typeof browser.extensionTypes.RunAt, 'object', 'RunAt should be an object')

            browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_START, 'document_start', 'DOCUMENT_START should be defined')
            browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_END, 'document_end', 'DOCUMENT_END should be defined')
            browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_IDLE, 'document_idle', 'DOCUMENT_IDLE should be defined')

            browser.test.assertDeepEq(Object.keys(browser.extensionTypes.RunAt).sort(), [ 'DOCUMENT_END', 'DOCUMENT_IDLE', 'DOCUMENT_START' ], 'RunAt should only define the supported values')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: extensionTypesManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func cssOrigin() async throws {
        let backgroundScript = """
            browser.test.assertEq(typeof browser.extensionTypes.CSSOrigin, 'object', 'CSSOrigin should be an object')

            browser.test.assertEq(browser.extensionTypes.CSSOrigin.AUTHOR, 'author', 'AUTHOR should be defined')
            browser.test.assertEq(browser.extensionTypes.CSSOrigin.USER, 'user', 'USER should be defined')

            browser.test.assertDeepEq(Object.keys(browser.extensionTypes.CSSOrigin).sort(), [ 'AUTHOR', 'USER' ], 'CSSOrigin should only define the supported values')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: extensionTypesManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func executionWorld() async throws {
        let backgroundScript = """
            browser.test.assertEq(typeof browser.extensionTypes.ExecutionWorld, 'object', 'ExecutionWorld should be an object')

            browser.test.assertEq(browser.extensionTypes.ExecutionWorld.ISOLATED, 'ISOLATED', 'ISOLATED should be defined')
            browser.test.assertEq(browser.extensionTypes.ExecutionWorld.MAIN, 'MAIN', 'MAIN should be defined')

            browser.test.assertDeepEq(Object.keys(browser.extensionTypes.ExecutionWorld).sort(), [ 'ISOLATED', 'MAIN' ], 'ExecutionWorld should only define the supported values')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: extensionTypesManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func availableInContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "ExtensionTypes Content Script Test",
            "description": "ExtensionTypes Content Script Test",
            "version": "1",

            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "js": ["content.js"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        let backgroundScript = """
            browser.test.sendMessage('Load Tab')
            """

        let contentScript = """
            browser.test.assertEq(typeof browser.extensionTypes, 'object', 'extensionTypes should be available in content scripts')
            browser.test.assertEq(browser.extensionTypes.ImageFormat.PNG, 'png', 'ImageFormat.PNG should be defined')
            browser.test.assertEq(browser.extensionTypes.RunAt.DOCUMENT_IDLE, 'document_idle', 'RunAt.DOCUMENT_IDLE should be defined')

            browser.test.notifyPass()
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(
                manifest: manifest,
                resources: ["background.js": backgroundScript, "content.js": contentScript]
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
