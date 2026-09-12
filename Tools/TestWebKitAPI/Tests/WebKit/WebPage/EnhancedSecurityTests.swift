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

import Testing
@_spi(Testing) @_spi(CrossImportOverlay) import WebKit
import WebKit_Private.WKWebViewPrivate
private import TestWebKitAPILibrary

// MARK: Supporting test types

@MainActor
private class TestNavigationDecider: WebPage.NavigationDeciding {
    var preferencesMutation: (inout WebPage.NavigationPreferences) -> Void = { _ in }

    func decidePolicy(
        for action: WebPage.NavigationAction,
        preferences: inout WebPage.NavigationPreferences
    ) async -> WKNavigationActionPolicy {
        preferencesMutation(&preferences)
        return .allow
    }
}

// MARK: Tests

@MainActor
struct EnhancedSecurityTests {
    @Test(arguments: [
        (mode: WebPage.NavigationPreferences.SecurityRestrictionMode.maximizeCompatibility, expected: true),
        (mode: WebPage.NavigationPreferences.SecurityRestrictionMode.none, expected: false),
    ])
    func securityRestrictionModeFromConfiguration(
        mode: WebPage.NavigationPreferences.SecurityRestrictionMode,
        expected: Bool
    ) async throws {
        var configuration = WebPage.Configuration()
        configuration.defaultNavigationPreferences.securityRestrictionMode = mode

        let page = WebPage(configuration: configuration)
        try await page.load(html: "<body></body>").wait()

        #expect(page.backingWebView._webProcessIdentifier != 0)

        let result = await page.backingWebView._isEnhancedSecurityEnabled()
        #expect(result == expected)
    }

    @Test(arguments: [true, false])
    func securityRestrictionModeFromNavigationDecider(enabled: Bool) async throws {
        let decider = TestNavigationDecider()
        decider.preferencesMutation = { preferences in
            preferences.securityRestrictionMode = enabled ? .maximizeCompatibility : .none
        }

        let page = WebPage(navigationDecider: decider)
        try await page.load(html: "<body></body>").wait()

        #expect(page.backingWebView._webProcessIdentifier != 0)

        let result = await page.backingWebView._isEnhancedSecurityEnabled()
        #expect(result == enabled)
    }
}

#endif
