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
            let webView = try #require(manager.defaultTab?.webView)

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
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            manager.load()

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.waitForTestMessage("Ready for Message")

            manager.sendTestMessage("Test", withArgument: ["key": "value"])

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
        #expect(receivedMessage as? [String: String] == ["key": "value"])
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
        #expect(firstMessage as? [String: String] == ["key": "One"])

        let secondMessage = try await manager.waitForTestMessage("Test")
        #expect(secondMessage as? [String: String] == ["key": "Two"])

        let thirdMessage = try await manager.waitForTestMessage("Test")
        #expect(thirdMessage as? [String: String] == ["key": "Three"])
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
        #expect(secondMessage as? [String: String] == ["key": "Two"])

        let thirdMessage = try await manager.waitForTestMessage("Message 3")
        #expect(thirdMessage as? [String: String] == ["key": "Three"])

        let firstMessage = try await manager.waitForTestMessage("Message 1")
        #expect(firstMessage as? [String: String] == ["key": "One"])
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
            let context = try #require(manager.context)

            context.setPermissionStatus(.grantedExplicitly, for: configuration.localhostAddress)

            manager.load()

            manager.sendTestMessage(testMessage)

            let webView = try #require(manager.defaultTab?.webView)
            webView.load(URLRequest(url: configuration.localhostAddress))

            try await manager.run()
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

        #expect(manager.testsAdded.isEmpty)
        #expect(manager.testsStarted.isEmpty)
        #expect(manager.testResults.isEmpty)
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

        #expect(manager.testsAdded == [testName])
        #expect(manager.testsStarted == [testName])
        #expect(manager.testResults as? [String: Bool] == [testName: true])
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

        #expect(manager.testsAdded == [testName])
        #expect(manager.testsStarted == [testName])
        #expect(manager.testResults as? [String: Bool] == [testName: false])
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

        #expect(manager.testsAdded == [testName])
        #expect(manager.testsStarted == [testName])
        #expect(manager.testResults as? [String: Bool] == [testName: false])
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

        #expect(manager.testsAdded == testNames)
        #expect(manager.testsStarted == testNames)
        #expect(manager.testResults as? [String: Bool] == [testNames[0]: true, testNames[1]: true])
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

        #expect(manager.testsAdded == testNames)
        #expect(manager.testsStarted == testNames)
        #expect(manager.testResults as? [String: Bool] == [testNames[0]: false, testNames[1]: true])
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

        #expect(manager.testsAdded.isEmpty)
        #expect(manager.testsStarted.isEmpty)
        #expect(manager.testResults.isEmpty)
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

        #expect(manager.testsAdded == [testName])
        #expect(manager.testsStarted == [testName])
        #expect(manager.testResults as? [String: Bool] == [testName: true])
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

        #expect(manager.testsAdded == [testName])
        #expect(manager.testsStarted == [testName])
        #expect(manager.testResults as? [String: Bool] == [testName: false])
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

        #expect(manager.testsAdded == [testName])
        #expect(manager.testsStarted == [testName])
        #expect(manager.testResults as? [String: Bool] == [testName: false])
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

        #expect(manager.testsAdded == testNames)
        #expect(manager.testsStarted == testNames)
        #expect(manager.testResults as? [String: Bool] == [testNames[0]: true, testNames[1]: true])
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

        #expect(manager.testsAdded == testNames)
        #expect(manager.testsStarted == testNames)
        #expect(manager.testResults as? [String: Bool] == [testNames[0]: false, testNames[1]: true])
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

        #expect(manager.testsAdded.isEmpty)
        #expect(manager.testsStarted.isEmpty)
        #expect(manager.testResults.isEmpty)
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

        #expect(manager.testsAdded == testNames)
        #expect(manager.testsStarted == testNames)
        #expect(manager.testResults as? [String: Bool] == [testNames[0]: true, testNames[1]: true])
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

        #expect(manager.testsAdded == testNames)
        #expect(manager.testsStarted == testNames)
        #expect(manager.testResults as? [String: Bool] == [testNames[0]: false, testNames[1]: true])
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

        #expect(manager.testsAdded == testNames)
        #expect(manager.testsStarted == testNames)
        #expect(manager.testResults as? [String: Bool] == [testNames[0]: true, testNames[1]: false])
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

        #expect(manager.testResults.count == testNames.count)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS
