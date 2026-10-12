// Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private._WKWebExtensionWebRequestFilter

#if ENABLE_WK_WEB_EXTENSIONS_SIDEBAR || ENABLE_WK_WEB_EXTENSIONS_OFFSCREEN
private import WebKit_Private._WKFeature
private import WebKit_Private.WKPreferencesPrivate
#endif

#if ENABLE_WK_WEB_EXTENSIONS_SIDEBAR
private import WebKit_Private._WKWebExtensionSidebar
#endif

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIWebRequestTests {
    private let webRequestManifest: [String: Any] = [
        "manifest_version": 3,
        "permissions": ["webRequest"],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
        ],
    ]

    #if ENABLE_WK_WEB_EXTENSIONS_SIDEBAR || ENABLE_WK_WEB_EXTENSIONS_OFFSCREEN
    private func configurationEnablingFeature(_ featureKey: String) -> WKWebExtensionController.Configuration {
        let configuration = WKWebExtensionController.Configuration.nonPersistent()

        for feature in WKPreferences._features() where feature.key == featureKey {
            configuration.webViewConfiguration.preferences._setEnabled(true, for: feature)
        }

        return configuration
    }
    #endif

    private func makeFilter(
        with dictionary: [String: Any],
        sourceLocation: SourceLocation = #_sourceLocation
    ) throws -> _WKWebExtensionWebRequestFilter {
        var error: NSString?
        let filter = unsafe _WKWebExtensionWebRequestFilter(dictionary: dictionary, outErrorMessage: &error)
        #expect(error == nil, sourceLocation: sourceLocation)

        return try #require(filter, sourceLocation: sourceLocation)
    }

    #if WTF_PLATFORM_MAC
    @Test
    func manifestV2Persistent() async throws {
        let backgroundScript = """
            browser.test.assertEq(typeof browser.webRequest, 'object', 'webRequest should be defined')

            browser.test.notifyPass()
            """

        let manifest: [String: Any] = [
            "manifest_version": 2,
            "permissions": ["webRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": true,
            ],
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }
    #endif

    @Test
    func manifestV2NonPersistent() async throws {
        let backgroundScript = """
            browser.test.assertEq(typeof browser.webRequest, 'object', 'webRequest should be defined')

            browser.test.notifyPass()
            """

        let manifest: [String: Any] = [
            "manifest_version": 2,
            "permissions": ["webRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func eventListenerRegistration() async throws {
        let backgroundScript = """
            function listener() { browser.test.notifyFail('This listener should not have been called') }
            browser.test.assertFalse(browser.webRequest.onCompleted.hasListener(listener), 'Should not have listener')

            browser.webRequest.onCompleted.addListener(listener)
            browser.test.assertTrue(browser.webRequest.onCompleted.hasListener(listener), 'Should have listener')

            browser.webRequest.onCompleted.removeListener(listener)
            browser.test.assertFalse(browser.webRequest.onCompleted.hasListener(listener), 'Should not have listener')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func beforeRequestEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.requestBody, undefined, 'details.requestBody should be undefined')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventForSubresource() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='/image.png'>"
            }

            Route("/image.png", headerFields: ["Content-Type": "image/png"]) {
                "..."
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (details?.type !== 'image')
                return

              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/image.png'), 'details.url should include /image.png')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')

              browser.test.assertEq(details?.requestBody, undefined, 'details.requestBody should be undefined')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventForSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/subframe.html'></iframe>"
            }

            Route("/subframe.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details?.url?.includes('/subframe.html'))
                return

              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(typeof details?.frameId, 'number', 'details.frameId should be')
              browser.test.assertTrue(details?.frameId !== 0, 'details.frameId should not be the main frame')
              browser.test.assertEq(typeof details?.parentFrameId, 'number', 'details.parentFrameId should be')
              browser.test.assertTrue(details?.parentFrameId !== -1, 'details.parentFrameId should not be -1')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/subframe.html'), 'details.url should include /subframe.html')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'sub_frame', 'details.type should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventSubframeTypeFilter() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/subframe.html'></iframe>"
            }

            Route("/subframe.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details?.url?.includes('/subframe.html'))
                return
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ], types: [ 'sub_frame' ] })

            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details?.url?.includes('/subframe.html'))
                return
              browser.test.notifyFail('the main_frame type filter should not match a subframe load')
            }, { urls: [ '<all_urls>' ], types: [ 'main_frame' ] })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventSubframeParentFrameIsMainFrame() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/subframe.html'></iframe>"
            }

            Route("/subframe.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details?.url?.includes('/subframe.html'))
                return
              browser.test.assertTrue(details?.frameId !== 0, 'the subframe should have a non-zero frameId')
              browser.test.assertEq(details?.parentFrameId, 0, 'the subframe parentFrameId should be the main frame')
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventNestedSubframeParentFrameId() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/middle.html'></iframe>"
            }

            Route("/middle.html", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/inner.html'></iframe>"
            }

            Route("/inner.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            let middleFrameId
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (details?.url?.includes('/middle.html')) {
                middleFrameId = details.frameId
                browser.test.assertTrue(details.frameId !== 0, 'the middle subframe should have a non-zero frameId')
                browser.test.assertEq(details.parentFrameId, 0, 'the middle subframe parent should be the main frame')
                return
              }
              if (!details?.url?.includes('/inner.html'))
                return
              browser.test.assertTrue(details.parentFrameId !== 0, 'the inner subframe parent is not the main frame')
              browser.test.assertEq(details.parentFrameId, middleFrameId, 'the inner subframe parent should be the middle subframe')
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventForActionPopupSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subframe.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details?.url?.includes('/subframe.html'))
                return
              browser.test.assertEq(details?.type, 'sub_frame', 'the popup iframe request should be a sub_frame')
              browser.test.assertEq(details?.tabId, -1, 'a request from an action popup should not be associated with a tab')
              browser.test.assertTrue(details?.frameId !== 0, 'the popup iframe should have a non-zero frameId')
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] })

            browser.test.sendMessage('Ready')
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["webRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
            ],
            "action": [
                "default_popup": "popup.html"
            ],
        ]

        try await server.run { configuration in
            let subframeURL = configuration.localhostAddress.appending(path: "subframe.html")
            let popupHTML = "<body><iframe src='\(subframeURL.absoluteString)'></iframe></body>"

            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "popup.html": popupHTML])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: subframeURL)

            manager.internalDelegate.presentPopupForAction = { _ in
                // Do nothing so the popup web view will stay loaded.
            }

            try await manager.waitForTestMessage("Ready")

            context.performAction(for: manager.defaultTab)

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventForBackgroundPageRequest() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subresource.txt", headerFields: ["Content-Type": "text/plain"]) {
                "resource"
            }
        }

        try await server.run { configuration in
            let subresourceURL = configuration.localhostAddress.appending(path: "subresource.txt")

            let backgroundScript = """
                browser.webRequest.onBeforeRequest.addListener((details) => {
                  if (!details?.url?.includes('/subresource.txt'))
                    return
                  browser.test.assertEq(details?.type, 'xmlhttprequest', 'the background page fetch should be an xmlhttprequest')
                  browser.test.assertEq(details?.tabId, -1, 'a request from the background page should not be associated with a tab')
                  browser.test.notifyPass()
                }, { urls: [ '<all_urls>' ] })

                fetch('\(subresourceURL.absoluteString)')
                """

            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: subresourceURL)

            try await manager.run()
        }
    }

    #if ENABLE_WK_WEB_EXTENSIONS_SIDEBAR
    @Test
    func beforeRequestEventForSidebarSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subframe.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details?.url?.includes('/subframe.html'))
                return
              browser.test.assertEq(details?.type, 'sub_frame', 'the sidebar iframe request should be a sub_frame')
              browser.test.assertEq(details?.tabId, -1, 'a request from a sidebar should not be associated with a tab')
              browser.test.assertTrue(details?.frameId !== 0, 'the sidebar iframe should have a non-zero frameId')
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] })

            browser.test.runWithUserGesture(() => browser.sidebarAction.open())
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["webRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
            ],
            "sidebar_action": [
                "default_panel": "sidebar.html"
            ],
        ]

        try await server.run { configuration in
            let subframeURL = configuration.localhostAddress.appending(path: "subframe.html")
            let sidebarHTML = "<body><iframe src='\(subframeURL.absoluteString)'></iframe></body>"

            let manager = try loadWebExtension(
                manifest: manifest,
                resources: ["background.js": backgroundScript, "sidebar.html": sidebarHTML],
                configuration: configurationEnablingFeature("WebExtensionSidebarEnabled")
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: subframeURL)

            manager.internalDelegate.presentSidebar = { _ in
                // Do nothing so the sidebar web view will stay loaded.
            }

            try await manager.run()
        }
    }
    #endif

    #if ENABLE_WK_WEB_EXTENSIONS_OFFSCREEN
    @Test
    func beforeRequestEventForOffscreenDocumentSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/subframe.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details?.url?.includes('/subframe.html'))
                return
              browser.test.assertEq(details?.type, 'sub_frame', 'the offscreen iframe request should be a sub_frame')
              browser.test.assertEq(details?.tabId, -1, 'a request from an offscreen document should not be associated with a tab')
              browser.test.assertTrue(details?.frameId !== 0, 'the offscreen iframe should have a non-zero frameId')
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] })

            browser.offscreen.createDocument({ url: 'offscreen.html', reasons: [ 'TESTING' ], justification: 'test' })
            """

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["webRequest", "offscreen"],
            "background": [
                "service_worker": "background.js",
                "type": "module",
            ],
        ]

        try await server.run { configuration in
            let subframeURL = configuration.localhostAddress.appending(path: "subframe.html")
            let offscreenHTML = "<body><iframe src='\(subframeURL.absoluteString)'></iframe></body>"

            let manager = try loadWebExtension(
                manifest: manifest,
                resources: ["background.js": backgroundScript, "offscreen.html": offscreenHTML],
                configuration: configurationEnablingFeature("WebExtensionOffscreenEnabled")
            )
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: subframeURL)

            try await manager.run()
        }
    }
    #endif

    @Test
    func beforeRequestEventWithRequestBodyAndFormData() async throws {
        let pageScript = """
            const formData = new FormData()
            formData.append('username', 'user1')
            formData.append('username', 'user2')
            formData.append('age', '42')

            const response = await fetch('/test', {
              method: 'POST',
              body: formData
            })

            const text = await response.text()
            browser.test.assertEq(text, 'OK', 'Response body should be OK')
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module' src='/form.js'></script>"
            }

            Route("/form.js", headerFields: ["Content-Type": "application/javascript"]) {
                pageScript
            }

            Route("/test", headerFields: ["Content-Type": "text/plain"]) {
                "OK"
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details.url.includes('/test'))
                return

              browser.test.assertEq(details.method, 'POST', 'details.method should be POST')
              browser.test.assertEq(details.requestBody?.raw?.length, 1, 'There should be one raw item')

              const decoder = new TextDecoder()
              const rawData = details.requestBody?.raw?.[0]?.bytes
              const bodyText = decoder.decode(rawData ?? new Uint8Array())

              browser.test.assertTrue(bodyText.includes('Content-Disposition: form-data; name="username"'), 'Username field should exist in multipart data')
              browser.test.assertTrue(bodyText.includes('user1'), 'Multipart data should include user1')
              browser.test.assertTrue(bodyText.includes('user2'), 'Multipart data should include user2')
              browser.test.assertTrue(bodyText.includes('Content-Disposition: form-data; name="age"'), 'Age field should exist in multipart data')
              browser.test.assertTrue(bodyText.includes('42'), 'Multipart data should include age value 42')

              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] }, [ 'requestBody' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventWithRequestBodyAndBlob() async throws {
        let pageScript = """
            const blob = new Blob(['This is some text blob content'], { type: 'text/plain' })

            const response = await fetch('/test', {
              method: 'POST',
              body: blob
            })

            const text = await response.text()
            browser.test.assertEq(text, 'OK', 'Response body should be')
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module' src='/blob.js'></script>"
            }

            Route("/blob.js", headerFields: ["Content-Type": "application/javascript"]) {
                pageScript
            }

            Route("/test", headerFields: ["Content-Type": "text/plain"]) {
                "OK"
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details.url.includes('/test'))
                return

              browser.test.assertEq(details.method, 'POST', 'details.method should be')
              browser.test.assertEq(details.requestBody?.raw?.length, 1, 'There should be one raw item')

              const decoder = new TextDecoder()
              const bodyText = decoder.decode(details.requestBody?.raw?.[0]?.bytes ?? new Uint8Array())
              browser.test.assertTrue(bodyText.includes('This is some text blob content'), 'Blob content should match')

              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] }, [ 'requestBody' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventWithRequestBodyAndJSON() async throws {
        let pageScript = """
            const response = await fetch('/test', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ key: 'value', count: 10 })
            })

            const text = await response.text()
            browser.test.assertEq(text, 'OK', 'Response body should be')
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module' src='/fetch.js'></script>"
            }

            Route("/fetch.js", headerFields: ["Content-Type": "application/javascript"]) {
                pageScript
            }

            Route("/test", headerFields: ["Content-Type": "text/plain"]) {
                "OK"
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details.url.includes('/test'))
                return

              browser.test.assertEq(details.method, 'POST', 'details.method should be')
              browser.test.assertEq(typeof details.requestBody, 'object', 'details.requestBody type should be')
              browser.test.assertTrue(details.requestBody?.raw?.[0]?.bytes?.byteLength > 0, 'details.requestBody.raw should contain data')

              const decoder = new TextDecoder()
              const bodyText = decoder.decode(details.requestBody?.raw?.[0]?.bytes ?? new Uint8Array())
              const jsonData = JSON.parse(bodyText ?? '{}')

              browser.test.assertEq(jsonData?.key, 'value', 'Request body key should be')
              browser.test.assertEq(jsonData?.count, 10, 'Request body count should be')

              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] }, [ 'requestBody' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeRequestEventWithRequestBodyAndForm() async throws {
        let pageScript = """
            const form = document.createElement('form')
            form.action = '/test'
            form.method = 'POST'

            const inputText1 = document.createElement('input')
            inputText1.type = 'text'
            inputText1.name = 'username'
            inputText1.value = 'user1'
            form.appendChild(inputText1)

            const inputText2 = document.createElement('input')
            inputText2.type = 'text'
            inputText2.name = 'username'
            inputText2.value = 'user2'
            form.appendChild(inputText2)

            const inputNumber = document.createElement('input')
            inputNumber.type = 'number'
            inputNumber.name = 'age'
            inputNumber.value = '42'
            form.appendChild(inputNumber)

            document.body.appendChild(form)
            form.submit()
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module' src='/form.js'></script>"
            }

            Route("/form.js", headerFields: ["Content-Type": "application/javascript"]) {
                pageScript
            }

            Route("/test", headerFields: ["Content-Type": "text/plain"]) {
                "OK"
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details.url.includes('/test'))
                return

              browser.test.assertEq(details.method, 'POST', 'details.method should be')
              browser.test.assertEq(typeof details.requestBody, 'object', 'details.requestBody type should be')
              browser.test.assertEq(typeof details.requestBody?.formData, 'object', 'details.requestBody.formData type should be')

              const formData = details.requestBody?.formData
              browser.test.assertEq(formData?.username?.length, 2, 'username array length should be')
              browser.test.assertEq(formData?.username?.[0], 'user1', 'First username should be')
              browser.test.assertEq(formData?.username?.[1], 'user2', 'Second username should be')
              browser.test.assertEq(formData?.age?.length, 1, 'age array length should be')
              browser.test.assertEq(formData?.age?.[0], '42', 'age should be')

              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] }, [ 'requestBody' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeSendHeadersEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeSendHeaders.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.requestHeaders, undefined, 'details.requestHeaders should be undefined')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeSendHeadersEventWithRequestHeaders() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeSendHeaders.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertTrue(Array.isArray(details?.requestHeaders), 'details.requestHeaders should be an array')
              browser.test.assertTrue(details?.requestHeaders?.some((header) => header.name === 'User-Agent'), 'details.requestHeaders should include User-Agent')

              browser.test.notifyPass()
            }, null, [ 'requestHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func beforeSendHeadersEventForSubresource() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='/image.png'>"
            }

            Route("/image.png", headerFields: ["Content-Type": "image/png"]) {
                "..."
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeSendHeaders.addListener((details) => {
              if (details?.type !== 'image')
                return

              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/image.png'), 'details.url should include /image.png')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')

              browser.test.assertTrue(Array.isArray(details?.requestHeaders), 'details.requestHeaders should be an array')
              browser.test.assertTrue(details?.requestHeaders?.some((header) => header.name === 'User-Agent'), 'details.requestHeaders should include User-Agent')

              browser.test.notifyPass()
            }, [ 'requestHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func sendHeadersEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onSendHeaders.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.requestHeaders, undefined, 'details.requestHeaders should be undefined')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func sendHeadersEventWithRequestHeaders() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onSendHeaders.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertTrue(Array.isArray(details?.requestHeaders), 'details.requestHeaders should be an array')
              browser.test.assertTrue(details?.requestHeaders?.some((header) => header.name === 'User-Agent'), 'details.requestHeaders should include User-Agent')

              browser.test.notifyPass()
            }, null, [ 'requestHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func sendHeadersEventForSubresource() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='/image.png'>"
            }

            Route("/image.png", headerFields: ["Content-Type": "image/png"]) {
                "..."
            }
        }

        let backgroundScript = """
            browser.webRequest.onSendHeaders.addListener((details) => {
              if (details?.type !== 'image')
                return

              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/image.png'), 'details.url should include /image.png')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')

              browser.test.assertTrue(Array.isArray(details?.requestHeaders), 'details.requestHeaders should be an array')
              browser.test.assertTrue(details?.requestHeaders?.some((header) => header.name === 'User-Agent'), 'details.requestHeaders should include User-Agent')

              browser.test.notifyPass()
            }, [ 'requestHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func headersReceivedEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onHeadersReceived.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertEq(details?.responseHeaders, undefined, 'details.responseHeaders should be undefined')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func headersReceivedEventWithResponseHeaders() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onHeadersReceived.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertTrue(Array.isArray(details?.responseHeaders), 'details.responseHeaders should be an array')
              browser.test.assertTrue(details?.responseHeaders?.some((header) => header.name === 'Content-Type' && header.value === 'text/html'), 'details.responseHeaders should include Content-Type: text/html')

              browser.test.notifyPass()
            }, null, [ 'responseHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func headersReceivedEventForSubresource() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='/image.png'>"
            }

            Route("/image.png", headerFields: ["Content-Type": "image/png", "Cache-Control": "no-cache"]) {
                "..."
            }
        }

        let backgroundScript = """
            browser.webRequest.onHeadersReceived.addListener((details) => {
              if (details?.type !== 'image')
                return

              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/image.png'), 'details.url should include /image.png')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertTrue(Array.isArray(details?.responseHeaders), 'details.responseHeaders should be an array')
              browser.test.assertTrue(details?.responseHeaders?.some((header) => header.name === 'Content-Type' && header.value === 'image/png'), 'details.responseHeaders should include Content-Type: image/png')
              browser.test.assertTrue(details?.responseHeaders?.some((header) => header.name === 'Cache-Control' && header.value === 'no-cache'), 'details.responseHeaders should include Cache-Control: no-cache')

              browser.test.notifyPass()
            }, [ 'responseHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func errorOccurredEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='nonexistent.png' />"
            }

            Route("/nonexistent.png")
                .responseBehavior(.terminateConnectionAfterReceivingRequest)
        }

        let backgroundScript = """
            browser.webRequest.onErrorOccurred.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'image', 'details.type should be')

              browser.test.assertEq(typeof details?.error, 'string', 'details.error should be a string')
              browser.test.assertTrue(details?.error?.length > 0, 'details.error should not be empty')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func webRequestFiresForDeclarativeNetRequestBlockedLoad() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/blocked", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["webRequest", "declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "block",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\": 1, \"priority\": 1, \"action\": { \"type\": \"block\" }, \"condition\": { \"urlFilter\": \"blocked\", \"resourceTypes\": [\"main_frame\"] } } ]"

        let backgroundScript = """
            let beforeRequestFired = false
            browser.webRequest.onBeforeRequest.addListener((details) => {
              if (!details.url.includes('/blocked')) return
              beforeRequestFired = true
              browser.test.assertEq(details.type, 'main_frame', 'onBeforeRequest type')
              browser.test.assertEq(details.method, 'GET', 'onBeforeRequest method')
            }, { urls: [ '<all_urls>' ] })
            browser.webRequest.onErrorOccurred.addListener((details) => {
              if (!details.url.includes('/blocked')) return
              browser.test.assertTrue(beforeRequestFired, 'onBeforeRequest fired before onErrorOccurred')
              browser.test.assertEq(details.type, 'main_frame', 'onErrorOccurred type')
              browser.test.assertEq(typeof details.error, 'string', 'error is a string')
              browser.test.assertTrue(details.error.includes('content blocker'), 'error mentions content blocker')
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] })
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            let blockedURL = configuration.localhostAddress.appending(path: "blocked")
            context.setPermissionStatus(.grantedExplicitly, for: blockedURL)

            try await manager.waitForTestMessage("Load Tab")
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: blockedURL))
            try await manager.run()
        }
    }

    @Test
    func webRequestParentFrameIdForDeclarativeNetRequestBlockedSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/blocked'></iframe>"
            }

            Route("/blocked", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        let manifest: [String: Any] = [
            "manifest_version": 3,
            "permissions": ["webRequest", "declarativeNetRequest"],
            "background": [
                "scripts": ["background.js"],
                "type": "module",
                "persistent": false,
            ],
            "declarative_net_request": [
                "rule_resources": [
                    [
                        "id": "block",
                        "enabled": true,
                        "path": "rules.json",
                    ]
                ]
            ],
        ]

        let rules =
            "[ { \"id\": 1, \"priority\": 1, \"action\": { \"type\": \"block\" }, \"condition\": { \"urlFilter\": \"blocked\", \"resourceTypes\": [\"sub_frame\"] } } ]"

        let backgroundScript = """
            browser.webRequest.onErrorOccurred.addListener((details) => {
              if (!details.url.includes('/blocked')) return
              browser.test.assertTrue(details.frameId !== 0, 'the blocked subframe should have a non-zero frameId')
              browser.test.assertEq(details.parentFrameId, 0, 'the blocked subframe parentFrameId should be the main frame')
              browser.test.notifyPass()
            }, { urls: [ '<all_urls>' ] })
            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript, "rules.json": rules])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.declarativeNetRequest)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")
            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))
            try await manager.run()
        }
    }

    @Test
    func webRequestFiresForTabsCreate() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<html><body>hi</body></html>"
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let backgroundScript = """
                const expectedEvents = ['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders', 'onHeadersReceived', 'onResponseStarted', 'onCompleted']
                const firedEvents = []
                const filter = { urls: [ '<all_urls>' ] }

                function recordEvent(name, details) {
                  if (details?.type !== 'main_frame')
                    return
                  if (!details?.url?.includes('http://localhost'))
                    return

                  firedEvents.push(name)

                  if (name !== 'onCompleted')
                    return

                  browser.test.assertEq(JSON.stringify(firedEvents), JSON.stringify(expectedEvents), 'all main_frame webRequest events should fire in order')
                  browser.test.notifyPass()
                }

                browser.webRequest.onBeforeRequest.addListener((details) => recordEvent('onBeforeRequest', details), filter)
                browser.webRequest.onBeforeSendHeaders.addListener((details) => recordEvent('onBeforeSendHeaders', details), filter)
                browser.webRequest.onSendHeaders.addListener((details) => recordEvent('onSendHeaders', details), filter)
                browser.webRequest.onHeadersReceived.addListener((details) => recordEvent('onHeadersReceived', details), filter)
                browser.webRequest.onResponseStarted.addListener((details) => recordEvent('onResponseStarted', details), filter)
                browser.webRequest.onCompleted.addListener((details) => recordEvent('onCompleted', details), filter)
                browser.webRequest.onErrorOccurred.addListener((details) => { if (details?.type === 'main_frame') browser.test.notifyFail('unexpected onErrorOccurred') }, filter)

                browser.tabs.create({ url: '\(url.absoluteString)' })
                """

            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: url)

            let tabDocumentURLWithoutAccess = try #require(URL(string: "https://webkit.org/"))

            manager.internalDelegate.openNewTab = { [weak manager] tabConfiguration, extensionContext, completionHandler in
                guard let newTab = manager?.defaultWindow?.openNewTab(at: UInt(tabConfiguration.index)) else {
                    completionHandler(nil, nil)
                    return
                }

                newTab.overrideURL = tabDocumentURLWithoutAccess

                if let url = tabConfiguration.url {
                    newTab.changeWebViewIfNeeded(for: url, for: extensionContext)
                    newTab.webView?.load(URLRequest(url: url))
                }

                completionHandler(newTab, nil)
            }

            try await manager.run()
        }
    }

    #if WTF_PLATFORM_MAC
    @Test
    func webRequestFiresForWindowsCreate() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<html><body>hi</body></html>"
            }
        }

        try await server.run { configuration in
            let url = configuration.localhostAddress

            let backgroundScript = """
                const expectedEvents = ['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders', 'onHeadersReceived', 'onResponseStarted', 'onCompleted']
                const firedEvents = []
                const filter = { urls: [ '<all_urls>' ] }

                function recordEvent(name, details) {
                  if (details?.type !== 'main_frame')
                    return
                  if (!details?.url?.includes('http://localhost'))
                    return

                  firedEvents.push(name)

                  if (name !== 'onCompleted')
                    return

                  browser.test.assertEq(JSON.stringify(firedEvents), JSON.stringify(expectedEvents), 'all main_frame webRequest events should fire in order')
                  browser.test.notifyPass()
                }

                browser.webRequest.onBeforeRequest.addListener((details) => recordEvent('onBeforeRequest', details), filter)
                browser.webRequest.onBeforeSendHeaders.addListener((details) => recordEvent('onBeforeSendHeaders', details), filter)
                browser.webRequest.onSendHeaders.addListener((details) => recordEvent('onSendHeaders', details), filter)
                browser.webRequest.onHeadersReceived.addListener((details) => recordEvent('onHeadersReceived', details), filter)
                browser.webRequest.onResponseStarted.addListener((details) => recordEvent('onResponseStarted', details), filter)
                browser.webRequest.onCompleted.addListener((details) => recordEvent('onCompleted', details), filter)
                browser.webRequest.onErrorOccurred.addListener((details) => { if (details?.type === 'main_frame') browser.test.notifyFail('unexpected onErrorOccurred') }, filter)

                browser.windows.create({ url: '\(url.absoluteString)' })
                """

            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: url)

            let tabDocumentURLWithoutAccess = try #require(URL(string: "https://webkit.org/"))

            manager.internalDelegate.openNewWindow = { [weak manager] windowConfiguration, extensionContext, completionHandler in
                guard let newWindow = manager?.openNewWindow(usingPrivateBrowsing: windowConfiguration.shouldBePrivate) else {
                    completionHandler(nil, nil)
                    return
                }

                let newTab = newWindow.tabs.first
                newTab?.overrideURL = tabDocumentURLWithoutAccess

                if let url = windowConfiguration.tabURLs.first {
                    newTab?.changeWebViewIfNeeded(for: url, for: extensionContext)
                    newTab?.webView?.load(URLRequest(url: url))
                }

                completionHandler(newWindow, nil)
            }

            try await manager.run()
        }
    }
    #endif

    @Test
    func redirectOccurredEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", statusCode: 301, headerFields: ["Location": "/target.html"]) {
                "redirecting..."
            }

            Route("/target.html", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRedirect.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertTrue(details?.redirectUrl?.includes('/target.html'), 'details.redirectUrl should include /target.html')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func redirectOccurredEventForSubresource() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='/redirecting.png' />"
            }

            Route("/redirecting.png", statusCode: 301, headerFields: ["Location": "/final.png"]) {
                "redirecting..."
            }

            Route("/final.png", headerFields: ["Content-Type": "image/png"]) {
                "..."
            }
        }

        let backgroundScript = """
            browser.webRequest.onBeforeRedirect.addListener((details) => {
              if (details?.type !== 'image')
                return

              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/redirecting.png'), 'details.url should include /redirecting.png')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')

              browser.test.assertTrue(details?.redirectUrl?.includes('/final.png'), 'details.redirectUrl should include /final.png')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func responseStartedEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onResponseStarted.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertEq(details?.responseHeaders, undefined, 'details.responseHeaders should be undefined')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func responseStartedEventWithResponseHeaders() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onResponseStarted.addListener((details) => {
              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertTrue(Array.isArray(details?.responseHeaders), 'details.responseHeaders should be an array')
              browser.test.assertTrue(details?.responseHeaders?.some((header) => header.name === 'Content-Type' && header.value === 'text/html'), 'details.responseHeaders should include Content-Type: text/html')

              browser.test.notifyPass()
            }, null, [ 'responseHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func responseStartedEventForSubresource() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='/image.png'>"
            }

            Route("/image.png", headerFields: ["Content-Type": "image/png"]) {
                "..."
            }
        }

        let backgroundScript = """
            browser.webRequest.onResponseStarted.addListener((details) => {
              if (details?.type !== 'image')
                return

              browser.test.assertEq(typeof details?.requestId, 'string', 'details.requestId should be')
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/image.png'), 'details.url should include /image.png')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'image', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertTrue(Array.isArray(details?.responseHeaders), 'details.responseHeaders should be an array')
              browser.test.assertTrue(details?.responseHeaders?.some((header) => header.name === 'Content-Type' && header.value === 'image/png'), 'details.responseHeaders should include Content-Type: image/png')

              browser.test.notifyPass()
            }, [ 'responseHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func completedEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onCompleted.addListener((details) => {
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertEq(details?.responseHeaders, undefined, 'details.responseHeaders should be undefined')

              browser.test.notifyPass()
            }, [ 'bogus' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func completedEventWithResponseHeaders() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webRequest.onCompleted.addListener((details) => {
              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('http://localhost'), 'details.url should include http://localhost')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'main_frame', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertTrue(Array.isArray(details?.responseHeaders), 'details.responseHeaders should be an array')
              browser.test.assertTrue(details?.responseHeaders?.some((header) => header.name === 'Content-Type' && header.value === 'text/html'), 'details.responseHeaders should include Content-Type: text/html')

              browser.test.notifyPass()
            }, [ 'responseHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func completedEventForSubresource() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<img src='/image.png'>"
            }

            Route("/image.png", headerFields: ["Content-Type": "image/png"]) {
                "..."
            }
        }

        let backgroundScript = """
            browser.webRequest.onCompleted.addListener((details) => {
              if (details?.type !== 'image')
                return

              browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
              browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
              browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              browser.test.assertTrue(details?.url?.includes('/image.png'), 'details.url should include /image.png')
              browser.test.assertEq(details?.method, 'GET', 'details.method should be')
              browser.test.assertEq(details?.type, 'image', 'details.type should be')

              browser.test.assertEq(details?.statusCode, 200, 'details.statusCode should be')
              browser.test.assertEq(details?.statusLine, 'OK', 'details.statusLine should be')

              browser.test.assertTrue(Array.isArray(details?.responseHeaders), 'details.responseHeaders should be an array')
              browser.test.assertTrue(details?.responseHeaders?.some((header) => header.name === 'Content-Type' && header.value === 'image/png'), 'details.responseHeaders should include Content-Type: image/png')

              browser.test.notifyPass()
            }, null, [ 'responseHeaders' ])

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func allowedFilter() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            function passListener() { browser.test.notifyPass() }

            browser.webRequest.onCompleted.addListener(passListener, { 'urls': [ '*://*.localhost/*' ] })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            // Grant the webRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func deniedFilter() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            function passListener() { browser.test.notifyPass() }
            function failListener() { browser.test.notifyFail('This listener should not have been called') }

            browser.webRequest.onCompleted.addListener(failListener, { 'urls': [ '*://*.example.com/*' ] })
            browser.webRequest.onCompleted.addListener(passListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            // Grant the webRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func allEventsFired() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            let beforeRequestFired = false
            let beforeSendHeadersFired = false
            let sendHeadersFired = false
            let headersReceivedFired = false
            let responseStartedFired = false

            browser.webRequest.onBeforeRequest.addListener(() => { beforeRequestFired = true })
            browser.webRequest.onBeforeSendHeaders.addListener(() => { beforeSendHeadersFired = true })
            browser.webRequest.onSendHeaders.addListener(() => { sendHeadersFired = true })
            browser.webRequest.onHeadersReceived.addListener(() => { headersReceivedFired = true })
            browser.webRequest.onResponseStarted.addListener(() => { responseStartedFired = true })

            function completedHandler() {
              browser.test.assertTrue(beforeRequestFired)
              browser.test.assertTrue(beforeSendHeadersFired)
              browser.test.assertTrue(sendHeadersFired)
              browser.test.assertTrue(headersReceivedFired)
              browser.test.assertTrue(responseStartedFired)

              browser.test.notifyPass()
            }

            browser.webRequest.onCompleted.addListener(completedHandler)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            // Grant the webRequest permission.
            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func documentIdAcrossEvents() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            let documentId = null

            browser.webRequest.onBeforeRequest.addListener((details) => {
              browser.test.assertEq(documentId, null, 'documentId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              documentId = details?.documentId
            })

            browser.webRequest.onBeforeSendHeaders.addListener((details) => {
              browser.test.assertEq(documentId, details?.documentId, 'details.documentId should stay consistent in onBeforeSendHeaders')
            })

            browser.webRequest.onSendHeaders.addListener((details) => {
              browser.test.assertEq(documentId, details?.documentId, 'details.documentId should stay consistent in onSendHeaders')
            })

            browser.webRequest.onHeadersReceived.addListener((details) => {
              browser.test.assertEq(documentId, details?.documentId, 'details.documentId should stay consistent in onHeadersReceived')
            })

            browser.webRequest.onResponseStarted.addListener((details) => {
              browser.test.assertEq(documentId, details?.documentId, 'details.documentId should stay consistent in onResponseStarted')
            })

            browser.webRequest.onCompleted.addListener((details) => {
              browser.test.assertEq(documentId, details?.documentId, 'details.documentId should stay consistent in onCompleted')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func removeListenerDuringEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            function requestListener() {
              browser.webRequest.onCompleted.removeListener(requestListener)
              browser.test.assertFalse(browser.webRequest.onCompleted.hasListener(requestListener), 'Listener should be removed')
            }

            browser.webRequest.onCompleted.addListener(requestListener)
            browser.webRequest.onCompleted.addListener(() => browser.test.notifyPass())

            browser.test.assertTrue(browser.webRequest.onCompleted.hasListener(requestListener), 'Listener should be registered')

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webRequestManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webRequest)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func initialization() {
        var error: NSString?
        var filter: _WKWebExtensionWebRequestFilter?

        filter = unsafe _WKWebExtensionWebRequestFilter(
            dictionary: [
                "urls": ["http://foo.com/*", "http://bar.org/*"]
            ],
            outErrorMessage: &error
        )
        #expect(filter != nil)
        #expect(error == nil)

        filter = unsafe _WKWebExtensionWebRequestFilter(
            dictionary: [
                "urls": ["http://foo.com/*", "http://bar.org/*"],
                "types": ["main_frame", "sub_frame"],
                "tabId": 123,
            ],
            outErrorMessage: &error
        )
        #expect(filter != nil)
        #expect(error == nil)

        filter = unsafe _WKWebExtensionWebRequestFilter(
            dictionary: [
                "urls": ["http://foo.com/*", "http://bar.org/*"],
                "types": ["main_frame", "sub_frame"],
                "windowId": 123,
            ],
            outErrorMessage: &error
        )
        #expect(filter != nil)
        #expect(error == nil)

        filter = unsafe _WKWebExtensionWebRequestFilter(
            dictionary: [
                "urls": ["http://has.both.a.tab.id.and.window.id.com/*"],
                "types": ["main_frame", "sub_frame"],
                "windowId": 123,
                "tabId": 9001,
            ],
            outErrorMessage: &error
        )
        #expect(filter != nil)
        #expect(error == nil)

        filter = unsafe _WKWebExtensionWebRequestFilter(
            dictionary: [
                "urls": []
            ],
            outErrorMessage: &error
        )
        #expect(filter != nil)
        #expect(error == nil)

        filter = unsafe _WKWebExtensionWebRequestFilter(
            dictionary: [
                "urls": ["http://foo.com/*", 3]
            ],
            outErrorMessage: &error
        )
        #expect(filter == nil)
        #expect(error == "'urls' is expected to be an array of strings, but a number was provided in the array")

        filter = unsafe _WKWebExtensionWebRequestFilter(
            dictionary: [
                "urls": ["$"]
            ],
            outErrorMessage: &error
        )
        #expect(filter == nil)

        #expect(
            error
                == "The 'urls' value is invalid, because '$' is an invalid match pattern. \"$\" cannot be parsed because it doesn't have a scheme."
        )
    }

    @Test
    func tabIDMatch() throws {
        let url = try #require(URL(string: "http://example.com/a"))
        let type = _WKWebExtensionWebRequestResourceType.other

        var filter = try makeFilter(with: [
            "urls": ["http://example.com/*"]
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 100, windowID: 1))

        filter = try makeFilter(with: [
            "urls": ["http://example.com/*"],
            "tabId": -1,
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 100, windowID: 1))

        filter = try makeFilter(with: [
            "urls": ["http://example.com/*"],
            "tabId": 100,
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 100, windowID: 1))
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 200, windowID: 1) == false)
    }

    @Test
    func windowIDMatch() throws {
        let url = try #require(URL(string: "http://example.com/a"))
        let type = _WKWebExtensionWebRequestResourceType.other

        var filter = try makeFilter(with: [
            "urls": ["http://example.com/*"]
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: -1, windowID: -1))

        filter = try makeFilter(with: [
            "urls": ["http://example.com/*"],
            "windowId": -1,
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 1))

        filter = try makeFilter(with: [
            "urls": ["http://example.com/*"],
            "windowId": 100,
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 100))
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 200) == false)
    }

    @Test
    func urlMatch() throws {
        let url = try #require(URL(string: "http://example.com/a/b"))
        let otherURL = try #require(URL(string: "http://some-other-website.biz/a/b"))
        let type = _WKWebExtensionWebRequestResourceType.other

        var filter = try makeFilter(with: [
            "urls": []
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: type, url: otherURL, tabID: 1, windowID: 1))

        filter = try makeFilter(with: [
            "urls": ["http://example.com/*"]
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: type, url: otherURL, tabID: 1, windowID: 1) == false)

        filter = try makeFilter(with: [
            "urls": ["http://not-example.com/*", "http://not-some-other-website.biz/*"]
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 1) == false)

        filter = try makeFilter(with: [
            "urls": ["http://example.com/*", "http://not-some-other-website.biz/*"]
        ])
        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: type, url: otherURL, tabID: 1, windowID: 1) == false)

        filter = try makeFilter(with: [
            "urls": ["http://example.com/*/b", "http://example.com/a/*"]
        ])

        #expect(filter.matchesRequestForResource(of: type, url: url, tabID: 1, windowID: 1))
    }

    @Test
    func resourceTypesMatch() throws {
        let url = try #require(URL(string: "http://example.com/a/b"))

        var filter = try makeFilter(with: [
            "urls": []
        ])
        #expect(filter.matchesRequestForResource(of: .image, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: .script, url: url, tabID: 1, windowID: 1))

        filter = try makeFilter(with: [
            "urls": [],
            "types": [],
        ])
        #expect(filter.matchesRequestForResource(of: .image, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: .script, url: url, tabID: 1, windowID: 1))

        filter = try makeFilter(with: [
            "urls": [],
            "types": ["image"],
        ])
        #expect(filter.matchesRequestForResource(of: .image, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: .script, url: url, tabID: 1, windowID: 1) == false)

        filter = try makeFilter(with: [
            "urls": [],
            "types": ["image", "script"],
        ])
        #expect(filter.matchesRequestForResource(of: .image, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: .script, url: url, tabID: 1, windowID: 1))
        #expect(filter.matchesRequestForResource(of: .websocket, url: url, tabID: 1, windowID: 1) == false)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
