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

// MARK: - Fixture

// Wires a `_WKAutomationSessionConfiguration` end to end through the same
// `@_spi(Testing)` `_WKAutomationSession*` bridge used by SendInspectorMessage.swift:
// WebPage() -> _WKAutomationSession(configuration:) ->
// processPool._setAutomationSession(_:) -> session._registerWebView(forTesting:).
// The `controlledByExternalAgent` flag is set on the configuration before the
// session is built so `session.configuration` round-trips it back through
// `-copyWithZone:` (the session copies on init and again on read).
@MainActor
private final class SessionFixture {
    let page: WebPage
    let session: _WKAutomationSession

    init(controlledByExternalAgent: Bool) {
        let page = WebPage()
        self.page = page

        let config = _WKAutomationSessionConfiguration()
        config.controlledByExternalAgent = controlledByExternalAgent
        let session = _WKAutomationSession(configuration: config)
        self.session = session

        let processPool = page.backingWebView.configuration.processPool
        processPool._setAutomationSession(session)
        _ = session._registerWebView(forTesting: page.backingWebView)
    }
}

// MARK: - Tests

@MainActor
@Suite("AutomationControlledByExternalAgent")
struct AutomationControlledByExternalAgentTests {
    // (a) positive: a configuration with the flag set to true, driven through
    // _WKAutomationSession(configuration:), reads the property back == true.
    @Test
    func flagRoundTripsThroughSessionWhenSet() {
        let fx = SessionFixture(controlledByExternalAgent: true)
        #expect(fx.session.configuration.controlledByExternalAgent == true)
    }

    // (b) copyWithZone: preservation — the NSCopying regression guard.
    @Test
    func flagSurvivesCopyWithZone() {
        let config = _WKAutomationSessionConfiguration()
        config.controlledByExternalAgent = true

        let copy = config.copy() as! _WKAutomationSessionConfiguration
        #expect(copy.controlledByExternalAgent == true)
    }

    // (c) negative: a configuration without the flag set reads back == false,
    // proving the flag is not hard-coded YES.
    @Test
    func flagDefaultsToFalseWhenUnset() {
        let fx = SessionFixture(controlledByExternalAgent: false)
        #expect(fx.session.configuration.controlledByExternalAgent == false)
    }
}

#endif // ENABLE_SWIFTUI
