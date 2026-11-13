// Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
struct WKWebExtensionAPITestTests {
    private let manifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Test Extension",
        "description": "Test Extension",
        "version": "1.0",

        "background": [
            "scripts": ["background.js"],
            "persistent": false,
        ],

        "content_scripts": [
            [
                "matches": ["*://*/*"],
                "js": ["content.js"],
            ]
        ],
    ]

    @Test
    func testStartedEvent() async throws {
        let backgroundScript = """
            browser.test.onTestStarted.addListener((data) => {
              browser.test.assertEq(data?.testName, 'test', 'data.testName should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Send Test Message')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Send Test Message")

        manager.sendTestStarted(withArgument: ["testName": "test"])

        try await manager.run()
    }

    @Test
    func testFinishedEvent() async throws {
        let backgroundScript = """
            browser.test.onTestFinished.addListener((data) => {
              browser.test.assertEq(data?.testName, 'test', 'data.testName should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Send Test Message')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Send Test Message")

        manager.sendTestFinished(withArgument: ["testName": "test"])

        try await manager.run()
    }

    @Test
    func messageEvent() async throws {
        let backgroundScript = """
            browser.test.onMessage.addListener((message, data) => {
              browser.test.assertEq(message, 'Test', 'message should be')
              browser.test.assertEq(data?.key, 'value', 'data.key should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Send Test Message')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Send Test Message")

        manager.sendTestMessage("Test", withArgument: ["key": "value"])

        try await manager.run()
    }

    @Test
    func messageEventInWebPage() async throws {
        let pageScript = """
            browser.test.onMessage.addListener((message, data) => {
              browser.test.assertEq(message, 'Test', 'message should be')
              browser.test.assertEq(data?.key, 'value', 'data.key should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Ready for Message')
            """

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script type='module' src='script.js'></script>"
            }

            Route("/script.js", headerFields: ["Content-Type": "application/javascript"]) {
                pageScript
            }
        }

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank."
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: manifest, resources: resources)
            let webView = try requireNonNil(manager.defaultTab?.webView)

            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Ready for Message")

            manager.sendTestMessage("Test", withArgument: ["key": "value"])

            try await manager.run()
        }
    }

    @Test
    func messageEventInContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let contentScript = """
            browser.test.onMessage.addListener((message, data) => {
              browser.test.assertEq(message, 'Test', 'message should be')
              browser.test.assertEq(data?.key, 'value', 'data.key should be')

              browser.test.notifyPass()
            })

            browser.test.sendMessage('Ready for Message')
            """

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank.",
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            manager.load()

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Ready for Message")

            manager.sendTestMessage("Test", withArgument: ["key": "value"])

            try await manager.run()
        }
    }

    @Test
    func jsHandleCreationInBackgroundPage() async throws {
        let backgroundScript = """
            const canCreateJSHandle = typeof window.webkit?.createJSHandle === 'function'
            browser.test.assertTrue(canCreateJSHandle, 'createJSHandle should be available in an extension page')

            const handle = canCreateJSHandle ? window.webkit.createJSHandle(document.documentElement) : null
            browser.test.assertTrue(!!handle, 'createJSHandle should return a handle for an element')

            browser.test.notifyPass()
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        try await manager.run()
    }

    @Test
    func jsHandleCreationInContentScript() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        let contentScript = """
            const canCreateJSHandle = typeof window.webkit?.createJSHandle === 'function'
            browser.test.assertTrue(canCreateJSHandle, 'createJSHandle should be available in a content script')

            const handle = canCreateJSHandle ? window.webkit.createJSHandle(document.body) : null
            browser.test.assertTrue(!!handle, 'createJSHandle should return a handle for an element')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank.",
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            manager.load()

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func jsHandleCreationInExtensionFrame() async throws {
        let frameManifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Test Extension",
            "description": "Test Extension",
            "version": "1.0",

            "background": [
                "scripts": ["background.js"],
                "persistent": false,
            ],

            "content_scripts": [
                [
                    "matches": ["*://localhost/*"],
                    "js": ["content.js"],
                ]
            ],

            "web_accessible_resources": [
                [
                    "resources": ["*.html"],
                    "matches": ["*://localhost/*"],
                ]
            ],
        ]

        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body></body>"
            }
        }

        let frameScript = """
            const canCreateJSHandle = typeof window.webkit?.createJSHandle === 'function'
            browser.test.assertTrue(canCreateJSHandle, 'createJSHandle should be available in an extension frame')

            const handle = canCreateJSHandle ? window.webkit.createJSHandle(document.documentElement) : null
            browser.test.assertTrue(!!handle, 'createJSHandle should return a handle for an element')

            browser.test.notifyPass()
            """

        let contentScript = """
            const iframe = document.createElement('iframe')
            iframe.src = browser.runtime.getURL('extension-frame.html')
            document.body.appendChild(iframe)
            """

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank.",
            "content.js": contentScript,
            "extension-frame.js": frameScript,
            "extension-frame.html": "<script src='extension-frame.js'></script>",
        ]

        try await server.run { configuration in
            let manager = try loadWebExtension(manifest: frameManifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func jsHandleCreationUnavailableInWebPage() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<body><script>document.body.dataset.canCreateJSHandle = String(typeof window.webkit?.createJSHandle === 'function')</script></body>"
            }
        }

        let contentScript = """
            browser.test.assertEq(document.body.dataset.canCreateJSHandle, 'false', 'createJSHandle should not be available to the web page')

            browser.test.notifyPass()
            """

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank.",
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            manager.load()

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func jsHandleCreationUnavailableInWebPageFrameInExtensionPage() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>parent.postMessage(typeof window.webkit?.createJSHandle, '*')</script>"
            }
        }

        try await server.run { configuration in
            let backgroundScript = """
                const iframe = document.createElement('iframe')
                iframe.src = '\(configuration.localhostAddress.absoluteString)'

                window.addEventListener('message', (event) => {
                  browser.test.assertEq(event.data, 'undefined', 'createJSHandle should not be available to a web page frame')

                  browser.test.notifyPass()
                })

                document.body.appendChild(iframe)
                """

            let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
            try await manager.run()
        }
    }

    @Test
    func messageEventWithSendMessageReply() async throws {
        let backgroundScript = """
            browser.test.onMessage.addListener((message, data) => {
              browser.test.assertEq(message, 'Test', 'message should be')
              browser.test.assertEq(data, undefined, 'data should be')

              browser.test.sendMessage('Received')
            })

            browser.test.sendMessage('Ready')
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.waitForTestMessage("Ready")
        manager.sendTestMessage("Test")
        try await manager.waitForTestMessage("Received")
    }

    @Test
    func sendMessage() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Test', { key: 'value' });
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let receivedMessage = try await manager.waitForTestMessage("Test")
        expectEqual(receivedMessage as? [String: String], ["key": "value"])
    }

    @Test
    func sendMessageMultipleTimes() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Test', { key: 'One' });
            browser.test.sendMessage('Test', { key: 'Two' });
            browser.test.sendMessage('Test', { key: 'Three' });
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let firstMessage = try await manager.waitForTestMessage("Test")
        expectEqual(firstMessage as? [String: String], ["key": "One"])

        let secondMessage = try await manager.waitForTestMessage("Test")
        expectEqual(secondMessage as? [String: String], ["key": "Two"])

        let thirdMessage = try await manager.waitForTestMessage("Test")
        expectEqual(thirdMessage as? [String: String], ["key": "Three"])
    }

    @Test
    func sendMessageOutOfOrder() async throws {
        let backgroundScript = """
            browser.test.sendMessage('Message 1', { key: 'One' });
            browser.test.sendMessage('Message 2', { key: 'Two' });
            browser.test.sendMessage('Message 3', { key: 'Three' });
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let secondMessage = try await manager.waitForTestMessage("Message 2")
        expectEqual(secondMessage as? [String: String], ["key": "Two"])

        let thirdMessage = try await manager.waitForTestMessage("Message 3")
        expectEqual(thirdMessage as? [String: String], ["key": "Three"])

        let firstMessage = try await manager.waitForTestMessage("Message 1")
        expectEqual(firstMessage as? [String: String], ["key": "One"])
    }

    @Test
    func sendMessageBeforeListenerAdded() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                ""
            }
        }

        let testMessage = "Queued test message"

        let contentScript = """
            browser.test.onMessage.addListener((message, data) => {
              browser.test.assertEq(message, '\(testMessage)')
              browser.test.notifyPass()
            })
            """

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank.",
            "content.js": contentScript,
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let context = try requireNonNil(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            manager.load()

            manager.sendTestMessage(testMessage)

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
        }
    }

    @Test
    func testFinishedEventListenerAddedToPageBeforeExtensionLoads() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>browser.test.onTestFinished.addListener((data) => { document.title = data.testName }); document.title = 'Listener Added'</script>"
            }
        }

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank.",
            "content.js": "// This script is intentionally left blank.",
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)
            let webView = try requireNonNil(manager.defaultTab?.webView)

            webView.load(URLRequest(url: configuration.localhostAddress))

            try await waitForCondition("the page to add its listener", timeout: .seconds(10)) {
                webView.title == "Listener Added"
            }

            manager.load()

            manager.sendTestFinished(withArgument: ["testName": "Test Finished"])

            try await waitForCondition("the page's listener to be called", timeout: .seconds(10)) {
                webView.title == "Test Finished"
            }

            try manager.checkCollectedFailures()
        }
    }

    @Test
    func testStartedEventListenerAddedToPageAfterExtensionLoads() async throws {
        var server = try HTTPServer(protocol: .http) {
            Route("/", headerFields: ["Content-Type": "text/html"]) {
                "<script>browser.test.onTestStarted.addListener((data) => { document.title = data.testName }); document.title = 'Listener Added'</script>"
            }
        }

        let resources: [String: Any] = [
            "background.js": "// This script is intentionally left blank.",
            "content.js": "// This script is intentionally left blank.",
        ]

        try await server.run { configuration in
            let manager = parseWebExtension(manifest: manifest, resources: resources)

            manager.load()

            let webView = try requireNonNil(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await waitForCondition("the page to add its listener", timeout: .seconds(10)) {
                webView.title == "Listener Added"
            }

            manager.sendTestStarted(withArgument: ["testName": "Test Started"])

            try await waitForCondition("the page's listener to be called", timeout: .seconds(10)) {
                webView.title == "Test Started"
            }

            try manager.checkCollectedFailures()
        }
    }

    @Test
    func addAnonymousAsyncTest() async throws {
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(async () => {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('Passing an anonymous function into addTest resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectTrue(manager.testsAdded.isEmpty)
        expectTrue(manager.testsStarted.isEmpty)
        expectTrue(manager.testResults.isEmpty)
    }

    @Test
    func addAsyncTestThatPasses() async throws {
        let testName = "passingTest"
        let backgroundScript = """
            browser.test.assertResolves(browser.test.addTest(async function passingTest() {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, [testName])
        expectEqual(manager.testsStarted, [testName])
        expectEqual(manager.testResults as? [String: Bool], [testName: true])
    }

    @Test
    func addAsyncTestThatFails() async throws {
        let testName = "failingTest"
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(async function failingTest() {
              browser.test.assertTrue(false)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, [testName])
        expectEqual(manager.testsStarted, [testName])
        expectEqual(manager.testResults as? [String: Bool], [testName: false])
    }

    @Test
    func addAsyncTestThatThrows() async throws {
        let testName = "failingTest"
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(async function failingTest() {
              throw new Error('fail the test')
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('Throwing an error in the addTest method resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, [testName])
        expectEqual(manager.testsStarted, [testName])
        expectEqual(manager.testResults as? [String: Bool], [testName: false])
    }

    @Test
    func addMultipleAsyncTestsThatPass() async throws {
        let testNames = ["testA", "testB"]
        let backgroundScript = """
            browser.test.assertResolves(browser.test.addTest(async function testA() {
              browser.test.assertTrue(true)
            }))
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))

            browser.test.assertResolves(browser.test.addTest(async function testB() {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testsStarted, testNames)
        expectEqual(manager.testResults as? [String: Bool], [testNames[0]: true, testNames[1]: true])
    }

    @Test
    func addMultipleAsyncTestsWithFailure() async throws {
        let testNames = ["testA", "testB"]
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(async function testA() {
              browser.test.assertTrue(false)
            }))
              .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))

            browser.test.assertResolves(browser.test.addTest(async function testB() {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testsStarted, testNames)
        expectEqual(manager.testResults as? [String: Bool], [testNames[0]: false, testNames[1]: true])
    }

    @Test
    func addAnonymousTest() async throws {
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(() => {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('Passing an anonymous function into addTest resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectTrue(manager.testsAdded.isEmpty)
        expectTrue(manager.testsStarted.isEmpty)
        expectTrue(manager.testResults.isEmpty)
    }

    @Test
    func addTestThatPasses() async throws {
        let testName = "passingTest"
        let backgroundScript = """
            browser.test.assertResolves(browser.test.addTest(function passingTest() {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, [testName])
        expectEqual(manager.testsStarted, [testName])
        expectEqual(manager.testResults as? [String: Bool], [testName: true])
    }

    @Test
    func addTestThatFails() async throws {
        let testName = "failingTest"
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(function failingTest() {
              browser.test.assertTrue(false)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, [testName])
        expectEqual(manager.testsStarted, [testName])
        expectEqual(manager.testResults as? [String: Bool], [testName: false])
    }

    @Test
    func addTestThatThrows() async throws {
        let testName = "failingTest"
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(function failingTest() {
              throw new Error('fail the test')
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('Throwing an error in the addTest method resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, [testName])
        expectEqual(manager.testsStarted, [testName])
        expectEqual(manager.testResults as? [String: Bool], [testName: false])
    }

    @Test
    func addMultipleTestsThatPass() async throws {
        let testNames = ["testA", "testB"]
        let backgroundScript = """
            browser.test.assertResolves(browser.test.addTest(function testA() {
              browser.test.assertTrue(true)
            }))
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))

            browser.test.assertResolves(browser.test.addTest(function testB() {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testsStarted, testNames)
        expectEqual(manager.testResults as? [String: Bool], [testNames[0]: true, testNames[1]: true])
    }

    @Test
    func addMultipleTestsWithFailure() async throws {
        let testNames = ["testA", "testB"]
        let backgroundScript = """
            browser.test.assertRejects(browser.test.addTest(function testA() {
              browser.test.assertTrue(false)
            }))
              .catch(() => browser.test.notifyFail('A failing assertion in the addTest method resolved the promise.'))

            browser.test.assertResolves(browser.test.addTest(function testB() {
              browser.test.assertTrue(true)
            }))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A passing assertion in the addTest method rejected the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testsStarted, testNames)
        expectEqual(manager.testResults as? [String: Bool], [testNames[0]: false, testNames[1]: true])
    }

    @Test
    func runAnonymousTests() async throws {
        let backgroundScript = """
            browser.test.assertRejects(browser.test.runTests([
              async () => {
                browser.test.assertTrue(true)
              },
              () => {
                browser.test.assertTrue(true)
              }
            ]))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('Passing an anonymous function into runTests resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectTrue(manager.testsAdded.isEmpty)
        expectTrue(manager.testsStarted.isEmpty)
        expectTrue(manager.testResults.isEmpty)
    }

    @Test
    func runTestsThatPass() async throws {
        let testNames = ["testA", "testB"]
        let backgroundScript = """
            browser.test.assertResolves(browser.test.runTests([
              function testA() {
                browser.test.assertTrue(true)
              },
              async function testB() {
                browser.test.assertTrue(true)
              }
            ]))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('All passing tests passed into runTests rejected the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testsStarted, testNames)
        expectEqual(manager.testResults as? [String: Bool], [testNames[0]: true, testNames[1]: true])
    }

    @Test
    func runTestsWithTestThatFails() async throws {
        let testNames = ["testA", "testB"]
        let backgroundScript = """
            browser.test.assertRejects(browser.test.runTests([
              function testA() {
                browser.test.assertTrue(false)
              },
              async function testB() {
                browser.test.assertTrue(true)
              }
            ]))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A failing test passed into runTests resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testsStarted, testNames)
        expectEqual(manager.testResults as? [String: Bool], [testNames[0]: false, testNames[1]: true])
    }

    @Test
    func runTestsWithAsyncTestThatFails() async throws {
        let testNames = ["testA", "testB"]
        let backgroundScript = """
            browser.test.assertRejects(browser.test.runTests([
              function testA() {
                browser.test.assertTrue(true)
              },
              async function testB() {
                browser.test.assertTrue(false)
              }
            ]))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('A failing async test passed into runTests resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testsStarted, testNames)
        expectEqual(manager.testResults as? [String: Bool], [testNames[0]: true, testNames[1]: false])
    }

    @Test
    func runTestsVerifyFailedTestAborts() async throws {
        let testNames = ["testAssertTrue", "testAssertFalse", "testAssertEq", "testAssertDeepEq", "testAssertThrows"]
        let backgroundScript = """
            function testAssertTrue() {
              browser.test.assertTrue(false)
              browser.test.notifyFail()
            }

            function testAssertFalse() {
              browser.test.assertFalse(true)
              browser.test.notifyFail()
            }

            function testAssertEq() {
              browser.test.assertEq(false, 4)
              browser.test.notifyFail()
            }

            function testAssertDeepEq() {
              browser.test.assertDeepEq({ 'key': 'value' }, { 'key2': 'value2' })
              browser.test.notifyFail()
            }

            function testAssertThrows() {
              browser.test.assertThrows(() => browser.permissions.getAll())
              browser.test.notifyFail()
            }

            browser.test.assertRejects(browser.test.runTests([
              testAssertTrue, testAssertFalse, testAssertEq, testAssertDeepEq, testAssertThrows
            ]))
              .then(() => browser.test.notifyPass())
              .catch(() => browser.test.notifyFail('Test(s) with failing assertions resolved the promise.'))
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testResults.count, testNames.count)
    }

    @Test
    func assertionsThatPass() async throws {
        let testNames = [
            "assertSafe",
            "assertSafeResolveWithResolvedPromise",
            "assertSafeResolveWithNonPromiseResult",
            "assertResolves",
            "assertRejects",
            "assertRejectsWithMatchingError",
            "assertThrows",
        ]

        let backgroundScript = """
            const tests = [
              browser.test.addTest(async function assertSafe() {
                const result = browser.test.assertSafe(() => 'Plain String')
                browser.test.assertEq(result, 'Plain String')
              }),

              browser.test.addTest(async function assertSafeResolveWithResolvedPromise() {
                const result = await browser.test.assertSafeResolve(() => Promise.resolve({ key: 'Value' }))
                browser.test.assertDeepEq(result, { key: 'Value' })
              }),

              browser.test.addTest(async function assertSafeResolveWithNonPromiseResult() {
                const result = await browser.test.assertSafeResolve(() => ({ key: 'Value' }))
                browser.test.assertDeepEq(result, { key: 'Value' })
              }),

              browser.test.addTest(async function assertResolves() {
                const result = await browser.test.assertResolves(Promise.resolve({ key: 'Value' }))
                browser.test.assertDeepEq(result, { key: 'Value' })
              }),

              browser.test.addTest(async function assertRejects() {
                await browser.test.assertRejects(Promise.reject(new Error('Expected Error')))
              }),

              browser.test.addTest(async function assertRejectsWithMatchingError() {
                await browser.test.assertRejects(Promise.reject(new Error('Expected Error')), /Expected/)
              }),

              browser.test.addTest(async function assertThrows() {
                browser.test.assertThrows(() => { throw new Error('Expected Error') })
              })
            ]

            Promise.allSettled(tests).then(() => browser.test.notifyPass())
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testResults as? [String: Bool], Dictionary(uniqueKeysWithValues: testNames.map { ($0, true) }))
    }

    @Test
    func assertionsThatFail() async throws {
        let testNames = [
            "assertSafeWithThrowingFunction",
            "assertSafeResolveWithRejectedPromise",
            "assertSafeResolveWithRejectedAsyncFunction",
            "assertSafeResolveWithThrowingFunction",
            "assertResolvesWithRejectedPromise",
            "assertRejectsWithResolvedPromise",
            "assertRejectsWithMismatchedError",
            "assertThrowsWithoutThrowing",
        ]

        // The catch blocks hide the rejection that await rethrows, so these only fail if the assertion reports it itself.
        let backgroundScript = """
            const tests = [
              browser.test.addTest(async function assertSafeWithThrowingFunction() {
                browser.test.assertSafe(() => { throw new Error('Expected Error') })
              }),

              browser.test.addTest(async function assertSafeResolveWithRejectedPromise() {
                try {
                  await browser.test.assertSafeResolve(() => Promise.reject(new Error('Expected Error')))
                } catch (error) { }
              }),

              browser.test.addTest(async function assertSafeResolveWithRejectedAsyncFunction() {
                try {
                  await browser.test.assertSafeResolve(async () => { throw new Error('Expected Error') })
                } catch (error) { }
              }),

              browser.test.addTest(async function assertSafeResolveWithThrowingFunction() {
                browser.test.assertSafeResolve(() => { throw new Error('Expected Error') })
              }),

              browser.test.addTest(async function assertResolvesWithRejectedPromise() {
                try {
                  await browser.test.assertResolves(Promise.reject(new Error('Expected Error')))
                } catch (error) { }
              }),

              browser.test.addTest(async function assertRejectsWithResolvedPromise() {
                try {
                  await browser.test.assertRejects(Promise.resolve({ key: 'Value' }))
                } catch (error) { }
              }),

              browser.test.addTest(async function assertRejectsWithMismatchedError() {
                try {
                  await browser.test.assertRejects(Promise.reject(new Error('Other Error')), /Expected/)
                } catch (error) { }
              }),

              browser.test.addTest(async function assertThrowsWithoutThrowing() {
                browser.test.assertThrows(() => 'Plain String')
              })
            ]

            Promise.allSettled(tests).then(() => browser.test.notifyPass())
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testsAdded, testNames)
        expectEqual(manager.testResults as? [String: Bool], Dictionary(uniqueKeysWithValues: testNames.map { ($0, false) }))
    }

    @Test
    func assertRejectsRejectsReturnedPromiseOnFailure() async throws {
        let backgroundScript = """
            let resolvedPromiseOutcome
            let mismatchedErrorOutcome
            let reachedAfterFailure = false

            const tests = [
              browser.test.addTest(async function assertRejectsWithResolvedPromise() {
                resolvedPromiseOutcome = await browser.test.assertRejects(Promise.resolve({ key: 'Value' })).then(() => 'resolved', () => 'rejected')
              }),

              browser.test.addTest(async function assertRejectsWithMismatchedError() {
                mismatchedErrorOutcome = await browser.test.assertRejects(Promise.reject(new Error('Other Error')), /Expected/).then(() => 'resolved', () => 'rejected')
              }),

              browser.test.addTest(async function assertRejectsStopsTest() {
                await browser.test.assertRejects(Promise.resolve({ key: 'Value' }))
                reachedAfterFailure = true
              })
            ]

            Promise.allSettled(tests).then(() => {
              browser.test.sendMessage('Outcomes', { resolvedPromiseOutcome, mismatchedErrorOutcome, reachedAfterFailure })
              browser.test.notifyPass()
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let outcomes = try await requireNonNil(manager.waitForTestMessage("Outcomes") as? [String: Any])
        expectEqual(outcomes["resolvedPromiseOutcome"] as? String, "rejected")
        expectEqual(outcomes["mismatchedErrorOutcome"] as? String, "rejected")
        expectEqual(outcomes["reachedAfterFailure"] as? Bool, false)

        try await manager.run()

        expectEqual(manager.testResults["assertRejectsWithResolvedPromise"] as? Bool, false)
        expectEqual(manager.testResults["assertRejectsWithMismatchedError"] as? Bool, false)
        expectEqual(manager.testResults["assertRejectsStopsTest"] as? Bool, false)
    }

    @Test
    func assertResolvesRejectsReturnedPromiseOnFailure() async throws {
        let backgroundScript = """
            let rejectedPromiseOutcome
            let reachedAfterFailure = false

            const tests = [
              browser.test.addTest(async function assertResolvesWithRejectedPromise() {
                rejectedPromiseOutcome = await browser.test.assertResolves(Promise.reject(new Error('Expected Error'))).then(() => 'resolved', () => 'rejected')
              }),

              browser.test.addTest(async function assertResolvesStopsTest() {
                await browser.test.assertResolves(Promise.reject(new Error('Expected Error')))
                reachedAfterFailure = true
              }),

              browser.test.addTest(async function assertSafeResolveStopsTest() {
                await browser.test.assertSafeResolve(() => Promise.reject(new Error('Expected Error')))
                reachedAfterFailure = true
              })
            ]

            Promise.allSettled(tests).then(() => {
              browser.test.sendMessage('Outcomes', { rejectedPromiseOutcome, reachedAfterFailure })
              browser.test.notifyPass()
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let outcomes = try await requireNonNil(manager.waitForTestMessage("Outcomes") as? [String: Any])
        expectEqual(outcomes["rejectedPromiseOutcome"] as? String, "rejected")
        expectEqual(outcomes["reachedAfterFailure"] as? Bool, false)

        try await manager.run()

        expectEqual(manager.testResults["assertResolvesWithRejectedPromise"] as? Bool, false)
        expectEqual(manager.testResults["assertResolvesStopsTest"] as? Bool, false)
        expectEqual(manager.testResults["assertSafeResolveStopsTest"] as? Bool, false)
    }

    @Test
    func assertSafeStopsTestOnFailure() async throws {
        let backgroundScript = """
            let reachedAfterAssertSafe = false
            let reachedAfterAssertSafeResolve = false

            const tests = [
              browser.test.addTest(async function assertSafeStopsTest() {
                browser.test.assertSafe(() => { throw new Error('Expected Error') })
                reachedAfterAssertSafe = true
              }),

              browser.test.addTest(async function assertSafeResolveStopsTest() {
                await browser.test.assertSafeResolve(() => { throw new Error('Expected Error') })
                reachedAfterAssertSafeResolve = true
              })
            ]

            Promise.allSettled(tests).then(() => {
              browser.test.sendMessage('Outcomes', { reachedAfterAssertSafe, reachedAfterAssertSafeResolve })
              browser.test.notifyPass()
            })
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        let outcomes = try await requireNonNil(manager.waitForTestMessage("Outcomes") as? [String: Any])
        expectEqual(outcomes["reachedAfterAssertSafe"] as? Bool, false)
        expectEqual(outcomes["reachedAfterAssertSafeResolve"] as? Bool, false)

        try await manager.run()

        expectEqual(manager.testResults["assertSafeStopsTest"] as? Bool, false)
        expectEqual(manager.testResults["assertSafeResolveStopsTest"] as? Bool, false)
    }

    @Test
    func unawaitedAssertionFailsEvenWhenItPasses() async throws {
        let backgroundScript = """
            const tests = [
              browser.test.addTest(async function unawaitedPassingAssertRejects() {
                browser.test.assertRejects(Promise.reject(new Error('Expected Error')))
              }),

              browser.test.addTest(async function unawaitedPassingAssertResolves() {
                browser.test.assertResolves(Promise.resolve('Value'))
              }),

              browser.test.addTest(async function unawaitedPassingAssertSafeResolve() {
                browser.test.assertSafeResolve(() => Promise.resolve('Value'))
              }),

              browser.test.addTest(async function unawaitedFailingAssertSafeResolve() {
                browser.test.assertSafeResolve(() => new Promise((resolve, reject) => setTimeout(() => reject(new Error('Expected Error')), 50)))
              }),

              browser.test.addTest(async function awaitedPassingAssertions() {
                await browser.test.assertRejects(Promise.reject(new Error('Expected Error')))
                await browser.test.assertResolves(Promise.resolve('Value'))
                await browser.test.assertSafeResolve(() => Promise.resolve('Value'))
              }),

              browser.test.addTest(async function trailingTest() {
                await new Promise(resolve => setTimeout(resolve, 200))
              })
            ]

            Promise.allSettled(tests).then(() => browser.test.notifyPass())
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        let unawaitedTestNames = [
            "unawaitedPassingAssertRejects",
            "unawaitedPassingAssertResolves",
            "unawaitedPassingAssertSafeResolve",
            "unawaitedFailingAssertSafeResolve",
        ]

        for testName in unawaitedTestNames {
            expectEqual(manager.testResults[testName] as? Bool, false, "\(testName) should have failed")
        }

        expectEqual(manager.testResults["awaitedPassingAssertions"] as? Bool, true)
        expectEqual(manager.testResults["trailingTest"] as? Bool, true)
    }

    @Test
    func awaitedAssertionFailureOutsideTestFinishesRun() async throws {
        let backgroundScript = """
            (async () => {
              await browser.test.assertRejects(Promise.resolve({ key: 'Value' }), /Expected/, 'Promise should reject')

              browser.test.notifyPass()
            })()
            """

        let manager = parseWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        manager.load()

        // The failed await throws and skips notifyPass(), so this only returns because the failure finishes the run.
        let error = await #expect(throws: (any Error).self) {
            try await manager.run()
        }

        expectEqual(error?.localizedDescription.contains("Promise should reject"), true)
    }

    @Test
    func assertionFailureOutsideTestFinishesRun() async throws {
        let backgroundScript = """
            browser.test.assertEq(1, 2, 'Values should be equal')
            """

        let manager = parseWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])
        manager.load()

        let error = await #expect(throws: (any Error).self) {
            try await manager.run()
        }

        expectEqual(error?.localizedDescription.contains("Values should be equal"), true)
    }

    @Test
    func unawaitedAssertionFailsTheTestThatStartedIt() async throws {
        let backgroundScript = """
            let settleAssertRejects
            let settleAssertResolves

            const tests = [
              browser.test.addTest(async function unawaitedAssertRejects() {
                browser.test.assertRejects(new Promise(resolve => settleAssertRejects = resolve))
              }),

              browser.test.addTest(async function unrelatedTest() {
                settleAssertRejects()
                await new Promise(resolve => setTimeout(resolve, 50))
              }),

              browser.test.addTest(async function unawaitedAssertResolves() {
                browser.test.assertResolves(new Promise((resolve, reject) => settleAssertResolves = () => reject(new Error('Expected Error'))))
              }),

              browser.test.addTest(async function trailingTest() {
                settleAssertResolves()
                await new Promise(resolve => setTimeout(resolve, 50))
              })
            ]

            Promise.allSettled(tests).then(() => browser.test.notifyPass())
            """

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": backgroundScript])

        try await manager.run()

        expectEqual(manager.testResults["unawaitedAssertRejects"] as? Bool, false)
        expectEqual(manager.testResults["unrelatedTest"] as? Bool, true)
        expectEqual(manager.testResults["unawaitedAssertResolves"] as? Bool, false)
        expectEqual(manager.testResults["trailingTest"] as? Bool, true)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
