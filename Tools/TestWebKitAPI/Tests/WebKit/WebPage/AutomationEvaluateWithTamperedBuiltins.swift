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

#if ENABLE_SWIFTUI

import Foundation
import Testing
import TestWebKitAPILibrary
@_spi(Testing) @_spi(CrossImportOverlay) import WebKit
import WebKit_Private.WKProcessPoolPrivate
import WebKit_Private._WKAutomationSession
import WebKit_Private._WKAutomationSessionConfiguration
import WebKit_Private._WKAutomationSessionPrivateForTesting

import struct Swift.String

private func makeEvaluateEnvelope(id: Int, handle: String, function: String) throws -> String {
    let message: [String: Any] = [
        "id": id,
        "method": "Automation.evaluateJavaScriptFunction",
        "params": [
            "browsingContextHandle": handle,
            "function": function,
            "arguments": [] as [String],
            "expectsImplicitCallbackArgument": false,
        ] as [String: Any],
    ]
    let data = try JSONSerialization.data(withJSONObject: message, options: [])
    return String(data: data, encoding: .utf8) ?? ""
}

private func decode(_ jsonString: String) -> [String: Any]? {
    guard let data = jsonString.data(using: .utf8),
        let object = try? JSONSerialization.jsonObject(with: data, options: []),
        let dict = object as? [String: Any]
    else {
        return nil
    }
    return dict
}

private func findMessage(withId id: Int, in captured: [String]) -> [String: Any]? {
    for raw in captured {
        guard let parsed = decode(raw),
            let idValue = parsed["id"] as? Int,
            idValue == id
        else { continue }
        return parsed
    }
    return nil
}

private func waitForCaptured(
    maxIterations: Int = 200,
    predicate: @MainActor () -> Bool
) async {
    for _ in 0..<maxIterations {
        if await predicate() { return }
        try? await Task.sleep(for: .milliseconds(50))
    }
}

private final class CapturedMessages {
    var strings: [String] = []
}

@MainActor
private final class TamperedBuiltinsFixture {
    let page: WebPage
    let session: _WKAutomationSession
    let handle: String
    private let capturedStorage = CapturedMessages()

    var captured: [String] { capturedStorage.strings }

    init(tamperingScript: String) async throws {
        let page = WebPage()
        self.page = page

        let session = _WKAutomationSession(configuration: _WKAutomationSessionConfiguration())
        session.sessionIdentifier = "TamperedBuiltinsTestSession"
        self.session = session

        let processPool = page.backingWebView.configuration.processPool
        processPool._setAutomationSession(session)

        let registeredHandle = session._registerWebView(forTesting: page.backingWebView)
        self.handle = registeredHandle

        let messages = capturedStorage
        session._setMessageToFrontendHandler(forTesting: { message in
            messages.strings.append(message)
        })

        // The tampering runs as page script, so it happens after the automation script object is
        // created but before any script is evaluated on its behalf -- the order the WPT test uses.
        try await page.load(
            html: "<html><body><script>\(tamperingScript)</script></body></html>"
        )
        .wait()

        #expect(!registeredHandle.isEmpty)
    }

    func evaluate(id: Int, function: String) async throws -> [String: Any] {
        let envelope = try makeEvaluateEnvelope(id: id, handle: handle, function: function)
        session._dispatchMessageFromRemote(forTesting: envelope)

        await waitForCaptured { findMessage(withId: id, in: self.captured) != nil }

        return try #require(findMessage(withId: id, in: captured))
    }
}

// Tampering performed by imported/w3c/web-platform-tests/css/css-highlight-api/
// HighlightRegistry-maplike-tampered-Map-prototype.html. That test cannot restore Map.prototype
// afterwards (it freezes it), so every later automation script evaluation ran in a poisoned realm.
private let tamperMapPrototype = """
    delete Map.prototype.size;
    Map.prototype.entries = null;
    Map.prototype.forEach = undefined;
    Map.prototype.get = "foo";
    Map.prototype.has = 0;
    Map.prototype.keys = Symbol();
    Map.prototype.values = 1;
    Map.prototype[Symbol.iterator] = true;
    Map.prototype.clear = false;
    Map.prototype.delete = "";
    Map.prototype.set = 3.14;
    Object.freeze(Map.prototype);
    """

// The built-ins called out in webkit.org/b/259594.
private let tamperPromiseAndJSON = """
    Promise.prototype.finally = null;
    Promise.race = null;
    window.Promise = function Poisoned() { throw new Error("page replaced Promise"); };
    JSON.stringify = () => { throw new Error("tampered stringify"); };
    JSON.parse = () => { throw new Error("tampered parse"); };
    Array.prototype.map = null;
    Array.prototype.push = null;
    String.prototype.split = null;
    """

@MainActor
@Suite("AutomationEvaluateWithTamperedBuiltins")
struct AutomationEvaluateWithTamperedBuiltinsTests {
    @Test
    func evaluatesAfterMapPrototypeIsTampered() async throws {
        let fx = try await TamperedBuiltinsFixture(tamperingScript: tamperMapPrototype)

        let response = try await fx.evaluate(id: 1, function: "function() { return 7; }")

        #expect(response["error"] == nil)
        let result = try #require(response["result"] as? [String: Any])
        #expect((result["result"] as? String) == "7")
    }

    @Test
    func evaluatesAfterPromiseAndJSONAreTampered() async throws {
        let fx = try await TamperedBuiltinsFixture(tamperingScript: tamperPromiseAndJSON)

        let response = try await fx.evaluate(id: 1, function: "function() { return 7; }")

        #expect(response["error"] == nil)
        let result = try #require(response["result"] as? [String: Any])
        #expect((result["result"] as? String) == "7")
    }

    @Test
    func returnsNodeHandleAfterMapPrototypeIsTampered() async throws {
        let fx = try await TamperedBuiltinsFixture(tamperingScript: tamperMapPrototype)

        // Node handles are kept in the two Maps that _clearStaleNodes iterates.
        let response = try await fx.evaluate(id: 1, function: "function() { return document.body; }")

        #expect(response["error"] == nil)
        let result = try #require(response["result"] as? [String: Any])
        let encodedNode = try #require(result["result"] as? String)
        #expect(encodedNode.contains("session-node-"))
    }
}

#endif // ENABLE_SWIFTUI
