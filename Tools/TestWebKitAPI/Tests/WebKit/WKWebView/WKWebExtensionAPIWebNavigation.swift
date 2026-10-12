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
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private._WKWebExtensionWebNavigationURLFilter

import struct Foundation.URL
import struct Swift.String

@MainActor
struct WKWebExtensionAPIWebNavigationTests {
    private let webNavigationManifest: [String: Any] = [
        "manifest_version": 3,
        "permissions": ["webNavigation"],
        "background": [
            "scripts": ["background.js"],
            "type": "module",
            "persistent": false,
        ],
    ]

    @Test
    func eventListenerRegistration() async throws {
        let backgroundScript = """
            function listener() { browser.test.notifyFail('This listener should not have been called') }
            browser.test.assertFalse(browser.webNavigation.onBeforeNavigate.hasListener(listener), 'Should not have listener')

            browser.webNavigation.onBeforeNavigate.addListener(listener)
            browser.test.assertTrue(browser.webNavigation.onBeforeNavigate.hasListener(listener), 'Should have listener')

            browser.webNavigation.onBeforeNavigate.removeListener(listener)
            browser.test.assertFalse(browser.webNavigation.onBeforeNavigate.hasListener(listener), 'Should not have listener')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func beforeNavigateEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onBeforeNavigate.addListener((details) => {
                browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
                browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

                browser.test.assertEq(typeof details?.url, 'string', 'details.url should be')
                browser.test.assertTrue(details?.url?.includes('localhost'), 'details.url should include localhost')

                browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
                browser.test.assertEq(typeof details?.timeStamp, 'number', 'details.timeStamp should be')

                browser.test.assertEq(details?.documentId, undefined, 'details.documentId should be')

                browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func committedEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onCommitted.addListener((details) => {
                browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
                browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

                browser.test.assertEq(typeof details?.url, 'string', 'details.url should be')
                browser.test.assertTrue(details?.url?.includes('localhost'), 'details.url should include localhost')

                browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
                browser.test.assertEq(typeof details?.timeStamp, 'number', 'details.timeStamp should be')

                browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
                browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

                browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func domContentLoadedEvent() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let backgroundScript = """
            browser.webNavigation.onDOMContentLoaded.addListener((details) => {
                browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
                browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

                browser.test.assertEq(typeof details?.url, 'string', 'details.url should be')
                browser.test.assertTrue(details?.url?.includes('localhost'), 'details.url should include localhost')

                browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
                browser.test.assertEq(typeof details?.timeStamp, 'number', 'details.timeStamp should be')

                browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
                browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

                browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
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
            browser.webNavigation.onCompleted.addListener((details) => {
                browser.test.assertEq(details?.frameId, 0, 'details.frameId should be')
                browser.test.assertEq(details?.parentFrameId, -1, 'details.parentFrameId should be')

                browser.test.assertEq(typeof details?.url, 'string', 'details.url should be')
                browser.test.assertTrue(details?.url?.includes('localhost'), 'details.url should include localhost')

                browser.test.assertEq(typeof details?.tabId, 'number', 'details.tabId should be')
                browser.test.assertEq(typeof details?.timeStamp, 'number', 'details.timeStamp should be')

                browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
                browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

                browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
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

            browser.webNavigation.onCommitted.addListener(passListener, { 'url': [ {'hostContains': 'localhost'} ] })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
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

            browser.webNavigation.onCommitted.addListener(failListener, { 'url': [ {'hostContains': 'example'} ] })
            browser.webNavigation.onCommitted.addListener(passListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
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
            let beforeNavigateEventFired = false
            let onCommittedEventFired = false
            let onDOMContentLoadedEventFired = false

            function beforeNavigateHandler() { beforeNavigateEventFired = true }
            function onCommittedHandler() { onCommittedEventFired = true }
            function onDOMContentLoadedHandler() { onDOMContentLoadedEventFired = true }

            function onCompletedHandler() {
              browser.test.assertTrue(beforeNavigateEventFired)
              browser.test.assertTrue(onCommittedEventFired)
              browser.test.assertTrue(onDOMContentLoadedEventFired)

              browser.test.notifyPass()
            }

            browser.webNavigation.onBeforeNavigate.addListener(beforeNavigateHandler)
            browser.webNavigation.onCommitted.addListener(onCommittedHandler)
            browser.webNavigation.onDOMContentLoaded.addListener(onDOMContentLoadedHandler)
            browser.webNavigation.onCompleted.addListener(onCompletedHandler)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
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

            browser.webNavigation.onBeforeNavigate.addListener((details) => {
              browser.test.assertEq(details?.documentId, undefined, 'details.documentId should be')
            })

            browser.webNavigation.onCommitted.addListener((details) => {
              browser.test.assertEq(documentId, null, 'documentId should be')

              browser.test.assertEq(typeof details?.documentId, 'string', 'details.documentId should be')
              browser.test.assertEq(details?.documentId?.length, 36, 'details.documentId.length should be')

              documentId = details?.documentId
            })

            browser.webNavigation.onDOMContentLoaded.addListener((details) => {
              browser.test.assertEq(documentId, details?.documentId, 'details.documentId should stay consistent in onDOMContentLoaded')
            })

            browser.webNavigation.onCompleted.addListener((details) => {
              browser.test.assertEq(documentId, details?.documentId, 'details.documentId should stay consistent in onCompleted')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
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
            function navigationListener() {
              browser.webNavigation.onCommitted.removeListener(navigationListener)
              browser.test.assertFalse(browser.webNavigation.onCommitted.hasListener(navigationListener), 'Listener should be removed')
            }

            browser.webNavigation.onCommitted.addListener(navigationListener)
            browser.webNavigation.onCommitted.addListener(() => browser.test.notifyPass())

            browser.test.assertTrue(browser.webNavigation.onCommitted.hasListener(navigationListener), 'Listener should be registered')

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)
            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func errorOccurredEventDuringProvisionalLoad() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }

            Route("/frame.html")
                .responseBehavior(.terminateConnectionAfterReceivingRequest)
        }

        let backgroundScript = """
            function errorListener(details) {
              browser.test.assertTrue(details?.frameId != 0)

              browser.test.assertTrue(details?.url.includes('localhost'))
              browser.test.assertTrue(details?.url.includes('frame'))

              browser.test.notifyPass()
            }

            browser.webNavigation.onErrorOccurred.addListener(errorListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            let requestURL = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: requestURL))

            try await manager.run()
        }
    }

    @Test
    func errorOccurredEventDuringLoad() async throws {
        var unexpectedPaths: [String] = []

        var server = try HTTPServer(protocol: .http) { connection in
            while let path = try await connection.receiveRequestPath() {
                switch path {
                case "/":
                    let body = "<iframe src='/frame.html'></iframe>"
                    let reply = """
                        HTTP/1.1 200 OK\r\n\
                        Content-Type: text/html\r\n\
                        Content-Length: \(body.utf8.count)\r\n\
                        Connection: close\r\n\
                        \r\n\
                        \(body)
                        """
                    try await connection.send(reply)

                case "/frame.html":
                    let response = """
                        HTTP/1.1 200 OK\r\n\
                        Content-Length: 1000000\r\n\
                        \r\n\
                        \(String(repeating: " ", count: 500_000))
                        """

                    try await connection.send(response)
                    await connection.terminate()
                    return

                default:
                    unexpectedPaths.append(path)
                }
            }
        }

        let backgroundScript = """
            async function errorListener(details) {
              // This should be a subframe
              browser.test.assertTrue(details.frameId != 0)

              // The URL of the frame that failed loading should include localhost and frame.
              browser.test.assertTrue(details.url.includes('localhost'))
              browser.test.assertTrue(details.url.includes('frame'))

              const frame = await browser.webNavigation.getFrame({ tabId: details.tabId, frameId: details.frameId })
              browser.test.assertEq(frame.parentFrameId, 0)
              browser.test.assertTrue(frame.errorOccurred)

              // And since the failure happened after the load had been committed, the frame object has the URL set as well.
              browser.test.assertTrue(frame.url.includes('localhost'))
              browser.test.assertTrue(frame.url.includes('frame'))

              browser.test.notifyPass()
            }

            // The passListener firing will consider the test passed.
            browser.webNavigation.onErrorOccurred.addListener(errorListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            let requestURL = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: requestURL))

            try await manager.run()
        }

        #expect(unexpectedPaths.isEmpty)
    }

    @Test
    func getFrameWithMainFrame() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }

            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            function completedListener(details) {
              if (details?.frameId !== 0)
                return

              browser.webNavigation.getFrame({ tabId: details?.tabId, frameId: 0 }, (frame) => {
                browser.test.assertEq(frame?.parentFrameId, -1)
                browser.test.assertTrue(frame?.url?.includes('localhost'))
                browser.test.assertFalse(frame?.url?.includes('frame'))
                browser.test.assertEq(typeof frame?.documentId, 'string', 'frame.documentId should be')
                browser.test.assertEq(frame?.documentId?.length, 36, 'frame.documentId.length should be')

                browser.test.notifyPass()
              })
            }

            browser.webNavigation.onCompleted.addListener(completedListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            let requestURL = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: requestURL))

            try await manager.run()
        }
    }

    @Test
    func getFrameWithSubframe() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }

            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            function completedListener(details) {
              if (details?.frameId === 0)
                return

              browser.webNavigation.getFrame({ tabId: details?.tabId, frameId: details?.frameId }, (frame) => {
                browser.test.assertEq(frame.parentFrameId, 0)
                browser.test.assertTrue(frame.url.includes('localhost'))
                browser.test.assertTrue(frame.url.includes('frame'))
                browser.test.assertEq(typeof frame?.documentId, 'string', 'frame.documentId should be')
                browser.test.assertEq(frame?.documentId?.length, 36, 'frame.documentId.length should be')

                browser.test.notifyPass()
              })
            }

            browser.webNavigation.onCompleted.addListener(completedListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            let requestURL = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: requestURL))

            try await manager.run()
        }
    }

    @Test
    func getAllFrames() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }

            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            function completedListener(details) {
              if (details.frameId !== 0)
                return

              browser.webNavigation.getAllFrames({ tabId: details?.tabId }, (frames) => {
                browser.test.assertEq(frames?.length, 2)

                for (let frame of frames) {
                  if (frame?.frameId === 0) {
                    browser.test.assertEq(frame?.parentFrameId, -1)
                    browser.test.assertTrue(frame?.url.includes('localhost'))
                    browser.test.assertFalse(frame?.url.includes('frame'))
                  } else {
                    browser.test.assertEq(frame?.parentFrameId, 0)
                    browser.test.assertTrue(frame?.url.includes('localhost'))
                    browser.test.assertTrue(frame?.url.includes('frame'))
                  }

                  browser.test.assertEq(typeof frame?.documentId, 'string', 'frame.documentId should be')
                  browser.test.assertEq(frame?.documentId?.length, 36, 'frame.documentId.length should be')
                }

                browser.test.notifyPass()
              })
            }

            browser.webNavigation.onCompleted.addListener(completedListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            let requestURL = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: requestURL))

            try await manager.run()
        }
    }

    @Test
    func errorOccurred() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }

            Route("/frame.html")
                .responseBehavior(.terminateConnectionAfterReceivingRequest)
        }

        let backgroundScript = """
            function errorListener(details) {
              // A subframe should have been the one to have the error.
              browser.test.assertFalse(details.frameId == 0)
              browser.test.assertEq(details.parentFrameId, 0)

              // Get more information about the frame to verify the errorOccurred bit was set.
              browser.webNavigation.getFrame({ tabId: details.tabId, frameId: details.frameId }, function(frame) {
                browser.test.assertEq(frame.parentFrameId, 0)
                browser.test.assertTrue(frame.errorOccurred)

                // One thing to note here is that if the provisional load fails, there won't be a URL in the details.
                browser.test.assertEq(frame.url, '')

                browser.test.notifyPass()
              })
            }

            // The passListener firing will consider the test passed.
            browser.webNavigation.onErrorOccurred.addListener(errorListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            let requestURL = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: requestURL))

            try await manager.run()
        }
    }

    @Test
    func errors() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<iframe src='/frame.html'></iframe>"
            }

            Route("/frame.html", headerFields: ["Content-Type": "text/html"]) {
                "<body style='background-color: blue'></body>"
            }
        }

        let backgroundScript = """
            async function completedListener(details) {
              // Only listen for when the main frame loads so we don't call this method more than once.
              if (details.frameId !== 0)
                return
              const activeTab = await browser.tabs.query({ active: true })
              // Make sure invalid tab/frame IDs vend an error message - use arbitrary frame and tabIds.
              await browser.test.assertRejects(browser.webNavigation.getFrame({tabId: (details.tabId + 1), frameId: 0}), /tab not found/i)
              await browser.test.assertRejects(browser.webNavigation.getFrame({tabId: details.tabId, frameId: 42}), /frame not found/i)
              browser.test.notifyPass()
            }

            // The passListener firing will consider the test passed.
            browser.webNavigation.onCompleted.addListener(completedListener)

            browser.test.sendMessage('Load Tab')
            """

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: webNavigationManifest, resources: ["background.js": backgroundScript])
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: WKWebExtension.Permission.webNavigation)

            let requestURL = configuration.localhostAddress
            context.setPermissionStatus(.grantedExplicitly, for: requestURL)
            context.setPermissionStatus(.grantedExplicitly, for: requestURL.appending(path: "frame.html"))

            try await manager.waitForTestMessage("Load Tab")

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: requestURL))

            try await manager.run()
        }
    }

    @Test
    func urlFilterTestMatchAllPredicates() throws {
        var errorString: NSString?
        let filterDictionary: [String: Any] = [
            "url": [
                [
                    "schemes": ["https"],
                    "hostEquals": "apple.com",
                ]
            ]
        ]

        let filter = try #require(
            unsafe _WKWebExtensionWebNavigationURLFilter(dictionary: filterDictionary, outErrorMessage: &errorString)
        )
        #expect(errorString == nil)

        let httpsAppleURL = try #require(URL(string: "https://apple.com"))
        let httpAppleURL = try #require(URL(string: "http://apple.com"))
        let httpsExampleURL = try #require(URL(string: "https://example.com"))

        #expect(filter.matchesURL(httpsAppleURL))
        #expect(filter.matchesURL(httpAppleURL) == false)
        #expect(filter.matchesURL(httpsExampleURL) == false)
    }

    @Test
    func urlFilterMatchesOnePredicate() throws {
        var errorString: NSString?
        let filterDictionary: [String: Any] = [
            "url": [
                ["hostEquals": "apple.com"],
                ["hostEquals": "example.com"],
            ]
        ]

        let filter = try #require(
            unsafe _WKWebExtensionWebNavigationURLFilter(dictionary: filterDictionary, outErrorMessage: &errorString)
        )
        #expect(errorString == nil)

        let httpAppleURL = try #require(URL(string: "http://apple.com"))
        let httpExampleURL = try #require(URL(string: "http://example.com"))
        let aboutBlankURL = try #require(URL(string: "about:blank"))
        let devNullURL = try #require(URL(string: "file:///dev/null"))

        #expect(filter.matchesURL(httpAppleURL))
        #expect(filter.matchesURL(httpExampleURL))
        #expect(filter.matchesURL(aboutBlankURL) == false)
        #expect(filter.matchesURL(devNullURL) == false)
    }

    @Test
    func emptyFilterMatchesEverything() throws {
        var errorString: NSString?
        let filterDictionary: [String: Any] = [
            "url": []
        ]

        let filter = try #require(
            unsafe _WKWebExtensionWebNavigationURLFilter(dictionary: filterDictionary, outErrorMessage: &errorString)
        )
        #expect(errorString == nil)

        let aboutBlankURL = try #require(URL(string: "about:blank"))
        let httpExampleURL = try #require(URL(string: "http://example.com"))
        let devNullURL = try #require(URL(string: "file:///dev/null"))

        #expect(filter.matchesURL(aboutBlankURL))
        #expect(filter.matchesURL(httpExampleURL))
        #expect(filter.matchesURL(devNullURL))
    }

    @Test
    func urlKeyTypeChecking() {
        func test(_ inputDictionary: [String: Any], _ expectedError: String?, sourceLocation: SourceLocation = #_sourceLocation) {
            var error: NSString?
            let filter = unsafe _WKWebExtensionWebNavigationURLFilter(dictionary: inputDictionary, outErrorMessage: &error)
            if let expectedError {
                #expect(error as String? == expectedError, sourceLocation: sourceLocation)
                #expect(filter == nil, sourceLocation: sourceLocation)
            } else {
                #expect(error == nil, sourceLocation: sourceLocation)
                #expect(filter != nil, sourceLocation: sourceLocation)
            }
        }

        test([:], "The 'filters' value is invalid, because it is missing required keys: 'url'.")
        test(["a": "b"], "The 'filters' value is invalid, because it is missing required keys: 'url'.")
        test(["a": "b", "url": []], nil)
        test(
            ["url": NSNull()],
            "The 'filters' value is invalid, because 'url' is expected to be an array of objects, but null was provided."
        )
        test(["url": []], nil)
        test(
            ["url": ["A"]],
            "The 'filters' value is invalid, because 'url' is expected to be an array of objects, but a string was provided in the array."
        )
        test(
            ["url": [[]]],
            "The 'filters' value is invalid, because 'url' is expected to be an array of objects, but an array was provided in the array."
        )
        test(["url": [[:]]], nil)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
