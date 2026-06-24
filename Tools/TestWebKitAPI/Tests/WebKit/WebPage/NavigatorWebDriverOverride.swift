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

import struct Swift.String
import Testing
@_spi(Testing) @_spi(CrossImportOverlay) import WebKit
import WebKit_Private.WKPreferencesPrivate
private import TestWebKitAPILibrary

@MainActor
struct NavigatorWebDriverOverrideTests {
    private func reportedWebDriver(controlledByAutomation: Bool, policy: _WKNavigatorWebDriverActivePolicy) async throws -> String {
        var configuration = WebPage.Configuration()
        configuration.isControlledByAutomation = controlledByAutomation

        let page = WebPage(configuration: configuration)
        page.backingWebView.configuration.preferences._navigatorWebDriverActivePolicy = policy

        try await page.load(html: "").wait()

        let result = try await page.callJavaScript("return String(navigator.webdriver);")
        return try #require(result as? String)
    }

    @Test(arguments: [
        (controlledByAutomation: true, policy: _WKNavigatorWebDriverActivePolicy.auto, expected: "true"),
        (controlledByAutomation: false, policy: _WKNavigatorWebDriverActivePolicy.auto, expected: "false"),
        (controlledByAutomation: true, policy: _WKNavigatorWebDriverActivePolicy.disabled, expected: "false"),
        (controlledByAutomation: false, policy: _WKNavigatorWebDriverActivePolicy.enabled, expected: "true"),
    ])
    func navigatorWebDriverReflectsPolicy(
        controlledByAutomation: Bool,
        policy: _WKNavigatorWebDriverActivePolicy,
        expected: String
    ) async throws {
        let result = try await reportedWebDriver(controlledByAutomation: controlledByAutomation, policy: policy)
        #expect(result == expected)
    }
}

#endif // ENABLE_SWIFTUI
