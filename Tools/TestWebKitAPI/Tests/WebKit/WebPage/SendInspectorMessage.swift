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
import WebKit_Private._WKAutomationSessionDelegate
import WebKit_Private._WKAutomationSessionPrivateForTesting

import struct Swift.String

@MainActor
private final class TestSendInspectorMessageDelegate: NSObject, @preconcurrency _WKAutomationSessionDelegate {
    var allowInspectorTesting: Bool = false

    // swift-format-ignore: NoLeadingUnderscores
    func _automationSessionShouldEnableInspectorTesting(_ automationSession: _WKAutomationSession) -> Bool {
        allowInspectorTesting
    }
}

// MARK: - Encodable envelope

private struct InnerCommand: Encodable {
    let id: Int
    let method: String
    let params: [String: AnyEncodable]
}

private struct OuterParams: Encodable {
    let browsingContextHandle: String
    let message: String
}

private struct OuterEnvelope: Encodable {
    let id: Int
    let method: String
    let params: OuterParams
}

// Type-erasing wrapper so heterogeneous JSON parameter dicts (Bool, Int, String, etc.)
// can be encoded with JSONEncoder. Inspector inner-command params are a freeform
// JSON object, so the values must be heterogeneous.
private struct AnyEncodable: Encodable {
    private let encodeValue: (any Encoder) throws -> Void

    init(_ value: some Encodable) {
        self.encodeValue = value.encode
    }

    func encode(to encoder: any Encoder) throws {
        try encodeValue(encoder)
    }
}

private func makeSendInspectorMessageEnvelope(
    outerId: Int,
    handle: String,
    innerId: Int,
    method: String,
    params: [String: AnyEncodable] = [:]
) throws -> String {
    let encoder = JSONEncoder()
    let inner = InnerCommand(id: innerId, method: method, params: params)
    let innerData = try encoder.encode(inner)
    let innerString = String(data: innerData, encoding: .utf8) ?? ""

    let outer = OuterEnvelope(
        id: outerId,
        method: "Automation.sendInspectorMessage",
        params: OuterParams(browsingContextHandle: handle, message: innerString)
    )
    let outerData = try encoder.encode(outer)
    return String(data: outerData, encoding: .utf8) ?? ""
}

// Inspector protocol replies are freeform JSON whose `result` and `params`
// shapes vary per command, so responses are introspected dynamically as
// [String: Any] via JSONSerialization rather than decoded into a fixed
// Codable type.
private func decode(_ jsonString: String) -> [String: Any]? {
    guard let data = jsonString.data(using: .utf8),
        let object = try? JSONSerialization.jsonObject(with: data, options: []),
        let dict = object as? [String: Any]
    else {
        return nil
    }
    return dict
}

private func findMessage(withOuterId outerId: Int, in captured: [String]) -> [String: Any]? {
    for raw in captured {
        guard let parsed = decode(raw),
            let idValue = parsed["id"] as? Int,
            idValue == outerId
        else { continue }
        return parsed
    }
    return nil
}

private func findEvent(named eventName: String, in captured: [String]) -> [String: Any]? {
    for raw in captured {
        guard let parsed = decode(raw),
            parsed["id"] == nil,
            let method = parsed["method"] as? String,
            method == eventName
        else { continue }
        return parsed
    }
    return nil
}

private func findReceiveEvent(withInnerId innerId: Int, in captured: [String]) -> [String: Any]? {
    for raw in captured {
        guard let parsed = decode(raw),
            parsed["id"] == nil,
            let method = parsed["method"] as? String,
            method == "Automation.receiveInspectorMessage",
            let params = parsed["params"] as? [String: Any],
            let innerJSON = params["message"] as? String,
            let inner = decode(innerJSON),
            let idValue = inner["id"] as? Int,
            idValue == innerId
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
        try? await _Concurrency.Task.sleep(for: .milliseconds(50))
    }
}

// MARK: - Fixture

private final class CapturedMessages {
    var strings: [String] = []
}

@MainActor
private final class SessionFixture {
    let page: WebPage
    let session: _WKAutomationSession
    let delegate: TestSendInspectorMessageDelegate
    let handle: String
    private let capturedStorage = CapturedMessages()

    var captured: [String] { capturedStorage.strings }

    init(allowInspectorTesting: Bool) async throws {
        let page = WebPage()
        self.page = page

        let config = _WKAutomationSessionConfiguration()
        let session = _WKAutomationSession(configuration: config)
        self.session = session

        let delegate = TestSendInspectorMessageDelegate()
        delegate.allowInspectorTesting = allowInspectorTesting
        self.delegate = delegate
        session.delegate = delegate

        let processPool = page.backingWebView.configuration.processPool
        processPool._setAutomationSession(session)

        let registeredHandle = session._registerWebView(forTesting: page.backingWebView)
        self.handle = registeredHandle

        let messages = capturedStorage
        session._setMessageToFrontendHandler(forTesting: { message in
            messages.strings.append(message)
        })

        try await page.load(
            html: "<html><head><title>SendInspectorMessage Test</title></head><body></body></html>"
        )
        .wait()

        #expect(!registeredHandle.isEmpty)
    }
}

// MARK: - Tests

@MainActor
@Suite("AutomationSendInspectorMessage")
struct AutomationSendInspectorMessageTests {
    @Test
    func roundTripTargetSetPauseOnStart() async throws {
        let fx = try await SessionFixture(allowInspectorTesting: true)

        let envelope = try makeSendInspectorMessageEnvelope(
            outerId: 1,
            handle: fx.handle,
            innerId: 99,
            method: "Target.setPauseOnStart",
            params: ["pauseOnStart": AnyEncodable(false)]
        )
        fx.session._dispatchMessageFromRemote(forTesting: envelope)

        await waitForCaptured {
            findMessage(withOuterId: 1, in: fx.captured) != nil
                && findReceiveEvent(withInnerId: 99, in: fx.captured) != nil
        }

        let outerResponse = try #require(findMessage(withOuterId: 1, in: fx.captured))
        #expect(outerResponse["error"] == nil)

        let event = try #require(findReceiveEvent(withInnerId: 99, in: fx.captured))
        let params = try #require(event["params"] as? [String: Any])
        #expect((params["browsingContextHandle"] as? String) == fx.handle)
        let innerMessage = try #require(params["message"] as? String)

        let inner = try #require(decode(innerMessage))
        #expect((inner["id"] as? Int) == 99)
        #expect(inner["error"] == nil)
        #expect(inner["result"] is [String: Any])
    }

    @Test
    func deniedWhenGateReturnsFalse() async throws {
        let fx = try await SessionFixture(allowInspectorTesting: false)

        let envelope = try makeSendInspectorMessageEnvelope(
            outerId: 2,
            handle: fx.handle,
            innerId: 99,
            method: "Page.getResourceTree"
        )
        fx.session._dispatchMessageFromRemote(forTesting: envelope)

        await waitForCaptured(maxIterations: 60) {
            findMessage(withOuterId: 2, in: fx.captured) != nil
        }

        let outerResponse = try #require(findMessage(withOuterId: 2, in: fx.captured))
        #expect(outerResponse["error"] != nil)
        #expect(findEvent(named: "Automation.receiveInspectorMessage", in: fx.captured) == nil)
    }

    @Test
    func windowNotFoundForUnknownHandle() async throws {
        let fx = try await SessionFixture(allowInspectorTesting: true)

        let envelope = try makeSendInspectorMessageEnvelope(
            outerId: 3,
            handle: "this-handle-does-not-exist",
            innerId: 99,
            method: "Page.getResourceTree"
        )
        fx.session._dispatchMessageFromRemote(forTesting: envelope)

        await waitForCaptured(maxIterations: 60) {
            findMessage(withOuterId: 3, in: fx.captured) != nil
        }

        let outerResponse = try #require(findMessage(withOuterId: 3, in: fx.captured))
        #expect(outerResponse["error"] != nil)
        #expect(findEvent(named: "Automation.receiveInspectorMessage", in: fx.captured) == nil)
    }
}

#endif // ENABLE_SWIFTUI
