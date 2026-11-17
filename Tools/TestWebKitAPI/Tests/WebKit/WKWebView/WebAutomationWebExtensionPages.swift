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

#if ENABLE_WK_WEB_EXTENSIONS_IN_WEBDRIVER

import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWebExtensionsDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WebExtensionUtilities
import Testing
import WebKit
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebExtensionContextPrivate
private import WebKit_Private.WKWebExtensionPrivate
private import WebKit_Private._WKAutomationSession
private import WebKit_Private._WKAutomationSessionConfiguration
private import WebKit_Private._WKAutomationSessionDelegate
private import WebKit_Private._WKAutomationSessionPrivateForTesting

import struct Swift.String

private let testExtensionIdentifier = "org.webkit.automation-test-extension"

private let commandIdentifier = 1

private struct IncomingMessage: Decodable {
    let id: Int?
}

private struct Reply<Result: Decodable>: Decodable {
    struct Failure: Decodable {
        let message: String
    }

    let result: Result?
    let error: Failure?
}

private struct WebExtensionPages: Decodable {
    enum PageType: String, Decodable {
        case background = "Background"
        case popup = "Popup"
        case offscreen = "Offscreen"
        case sidebar = "Sidebar"
        case tab = "Tab"
    }

    struct Page: Decodable {
        let handle: String
        let type: PageType
        let url: String
    }

    let baseURL: String
    let pages: [Page]

    func pages(ofType type: PageType) -> [Page] {
        pages.filter { $0.type == type }
    }
}

private struct EvaluationResult: Decodable {
    let result: String
}

@MainActor
private final class TestWebExtensionPagesAutomationDelegate: NSObject, @preconcurrency _WKAutomationSessionDelegate {
    private let extensionContext: WKWebExtensionContext

    init(extensionContext: WKWebExtensionContext) {
        self.extensionContext = extensionContext
    }

    // swift-format-ignore: NoLeadingUnderscores
    func _automationSession(
        _ automationSession: _WKAutomationSession,
        webExtensionContextForIdentifier identifier: String
    ) -> WKWebExtensionContext? {
        identifier == testExtensionIdentifier ? extensionContext : nil
    }
}

@MainActor
private final class SessionFixture {
    let session = _WKAutomationSession(configuration: _WKAutomationSessionConfiguration())
    private let delegate: TestWebExtensionPagesAutomationDelegate

    init(extensionContext: WKWebExtensionContext) {
        delegate = TestWebExtensionPagesAutomationDelegate(extensionContext: extensionContext)
        session.delegate = delegate
    }

    func webExtensionPages(identifier: String = testExtensionIdentifier) async throws -> Reply<WebExtensionPages> {
        try await send("Automation.getWebExtensionPages", parameters: ["identifier": identifier])
    }

    func send<Result: Decodable>(_ method: String, parameters: [String: Any]) async throws -> Reply<Result> {
        let commandObject: [String: Any] = ["id": commandIdentifier, "method": method, "params": parameters]
        let command = try JSONSerialization.data(withJSONObject: commandObject)

        let reply: Data = await withCheckedContinuation { continuation in
            var pendingContinuation: CheckedContinuation<Data, Never>? = continuation

            session._setMessageToFrontendHandler(forTesting: { message in
                let data = Data(message.utf8)
                guard (try? JSONDecoder().decode(IncomingMessage.self, from: data))?.id == commandIdentifier else {
                    return
                }

                pendingContinuation?.resume(returning: data)
                pendingContinuation = nil
            })

            session._dispatchMessageFromRemote(forTesting: String(decoding: command, as: UTF8.self))
        }

        session._setMessageToFrontendHandler(forTesting: nil)

        return try JSONDecoder().decode(Reply<Result>.self, from: reply)
    }
}

@MainActor
struct WebAutomationWebExtensionPagesTests {
    private let backgroundPageManifest: [String: Any] = [
        "manifest_version": 3,

        "name": "Automation Test",
        "description": "Automation Test",
        "version": "1",

        "background": [
            "scripts": ["background.js"],
            "persistent": false,
        ],

        "action": ["default_popup": "popup.html"],
    ]

    @Test
    func listsBackgroundPage() async throws {
        let manager = try loadWebExtension(
            manifest: backgroundPageManifest,
            resources: ["background.js": "browser.test.sendMessage('Background Ready')"]
        )
        try await manager.waitForTestMessage("Background Ready")

        let context = try #require(manager.context)
        let fixture = SessionFixture(extensionContext: context)

        let reply = try await fixture.webExtensionPages()
        let result = try #require(reply.result)
        let baseURL = context.baseURL.absoluteString
        #expect(result.baseURL == baseURL)

        let backgroundPages = result.pages(ofType: .background)
        #expect(backgroundPages.count == 1)
        #expect(backgroundPages.first?.url.hasPrefix(baseURL) == true)
    }

    @Test
    func wakesUnloadedBackgroundPage() async throws {
        let manager = try loadWebExtension(
            manifest: backgroundPageManifest,
            resources: ["background.js": "browser.test.sendMessage('Background Ready')"]
        )
        try await manager.waitForTestMessage("Background Ready")

        let context = try #require(manager.context)
        context._unloadBackgroundContentForTesting()
        #expect(context._backgroundWebView == nil)

        let fixture = SessionFixture(extensionContext: context)

        let reply = try await fixture.webExtensionPages()
        let result = try #require(reply.result)
        #expect(result.pages(ofType: .background).count == 1)
        #expect(context._backgroundWebView != nil)
    }

    @Test
    func listsOpenPopup() async throws {
        let manager = try loadWebExtension(
            manifest: backgroundPageManifest,
            resources: [
                "background.js": "browser.action.openPopup()",
                "popup.html": "<script src='popup.js'></script>",
                "popup.js": "browser.test.sendMessage('Popup Ready')",
            ]
        )

        manager.internalDelegate.presentPopupForAction = { _ in }

        try await manager.waitForTestMessage("Popup Ready")

        let context = try #require(manager.context)
        let fixture = SessionFixture(extensionContext: context)

        let reply = try await fixture.webExtensionPages()
        let result = try #require(reply.result)
        let popupPages = result.pages(ofType: .popup)
        #expect(popupPages.count == 1)
        #expect(popupPages.first?.url.hasSuffix("/popup.html") == true)
    }

    @Test
    func listsExtensionTab() async throws {
        let manager = try loadWebExtension(
            manifest: backgroundPageManifest,
            resources: [
                "background.js": "browser.tabs.create({ url: 'page.html' })",
                "page.html": "<script src='page.js'></script>",
                "page.js": "browser.test.sendMessage('Tab Ready')",
            ]
        )

        try await manager.waitForTestMessage("Tab Ready")

        let context = try #require(manager.context)
        let fixture = SessionFixture(extensionContext: context)

        let reply = try await fixture.webExtensionPages()
        let result = try #require(reply.result)
        let tabPages = result.pages(ofType: .tab)
        #expect(tabPages.count == 1)
        #expect(tabPages.first?.url.hasSuffix("/page.html") == true)
    }

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses process pool SPI")
    #endif
    @Test
    func handleRunsScriptInBackgroundPage() async throws {
        let manager = try loadWebExtension(
            manifest: backgroundPageManifest,
            resources: ["background.js": "browser.test.sendMessage('Background Ready')"]
        )
        try await manager.waitForTestMessage("Background Ready")

        let context = try #require(manager.context)
        let fixture = SessionFixture(extensionContext: context)

        let processPool = try #require(context._backgroundWebView?.configuration.processPool)
        processPool._setAutomationSession(fixture.session)
        defer { processPool._setAutomationSession(nil) }

        let pagesReply = try await fixture.webExtensionPages()
        let handle = try #require(pagesReply.result?.pages(ofType: .background).first?.handle)

        let reply: Reply<EvaluationResult> = try await fixture.send(
            "Automation.evaluateJavaScriptFunction",
            parameters: [
                "browsingContextHandle": handle,
                "function": "function() { return typeof browser.runtime.id }",
                "arguments": [] as [String],
            ]
        )
        #expect(reply.result?.result == "\"string\"")
    }

    @Test
    func excludesServiceWorkerBackground() async throws {
        let manifest: [String: Any] = [
            "manifest_version": 3,

            "name": "Automation Test",
            "description": "Automation Test",
            "version": "1",

            "background": ["service_worker": "background.js"],
        ]

        let manager = try loadWebExtension(manifest: manifest, resources: ["background.js": "browser.test.sendMessage('Background Ready')"])
        try await manager.waitForTestMessage("Background Ready")

        let context = try #require(manager.context)
        let fixture = SessionFixture(extensionContext: context)

        let reply = try await fixture.webExtensionPages()
        let result = try #require(reply.result)
        #expect(result.pages(ofType: .background).isEmpty)
    }

    @Test
    func rejectsUnknownExtension() async throws {
        let manager = try loadWebExtension(
            manifest: backgroundPageManifest,
            resources: ["background.js": "browser.test.sendMessage('Background Ready')"]
        )
        try await manager.waitForTestMessage("Background Ready")

        let context = try #require(manager.context)
        let fixture = SessionFixture(extensionContext: context)

        let reply = try await fixture.webExtensionPages(identifier: "org.webkit.not-loaded")
        #expect(reply.error?.message.contains("NoSuchExtension") == true)
    }

    @Test
    func rejectsUnloadedExtension() async throws {
        let webExtension = try #require(
            WKWebExtension(
                manifestDictionary: backgroundPageManifest,
                resources: ["background.js": "browser.test.sendMessage('Background Ready')"]
            )
        )
        let context = WKWebExtensionContext(for: webExtension)
        let fixture = SessionFixture(extensionContext: context)

        let reply = try await fixture.webExtensionPages()
        #expect(reply.error?.message.contains("NoSuchExtension") == true)
    }
}

#endif // ENABLE_WK_WEB_EXTENSIONS_IN_WEBDRIVER
