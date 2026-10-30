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

#if ENABLE_CONTENT_INSET_BACKGROUND_FILL && (WTF_PLATFORM_MAC || WTF_PLATFORM_IOS || WTF_PLATFORM_MACCATALYST || WTF_PLATFORM_VISION)

import CoreGraphics
import Foundation
private import Synchronization
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import WebKit
private import WebKit_Private.WKPreferencesPrivate
private import WebKit_Private.WKWebViewConfigurationPrivate
private import WebKit_Private.WKWebViewPrivate
private import WebKit_Private.WKWebViewPrivateForTesting

import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
#endif

private let tomato = makeSRGBColor(red: 1, green: 99.0 / 255.0, blue: 71.0 / 255.0)
private let blue = makeSRGBColor(red: 0, green: 0, blue: 1)

private func makeSRGBColor(red: CGFloat, green: CGFloat, blue: CGFloat) -> CocoaColor {
    #if WTF_PLATFORM_IOS_FAMILY
    UIColor(red: red, green: green, blue: blue, alpha: 1)
    #else
    NSColor(srgbRed: red, green: green, blue: blue, alpha: 1)
    #endif
}

@MainActor
private func makeWebViewForTopFixedContainerSampling() throws -> TestWKWebView {
    let configuration = WKWebViewConfiguration()
    configuration._sampledPageTopColorMaxDifference = 5
    configuration._sampledPageTopColorMinHeight = 0

    // On iPhone, top fixed-container sampling remains enabled after user interaction by default.
    // Disable it here so the test exercises the macOS and iPadOS freeze policy on all platforms.
    let canChangeAfterScrolling = try #require(
        WKPreferences._features().first { $0.key == "TopContentInsetBackgroundCanChangeAfterScrolling" }
    )
    configuration.preferences._setEnabled(false, for: canChangeAfterScrolling)

    let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 800, height: 600), configuration: configuration)
    #if WTF_PLATFORM_IOS_FAMILY
    let insets = UIEdgeInsets(top: 75, left: 0, bottom: 0, right: 0)
    let insetSize = webView.bounds.inset(by: insets).size
    webView._obscuredInsets = insets
    let scrollView = webView.scrollView
    scrollView.contentInsetAdjustmentBehavior = .never
    scrollView.contentInset = insets
    webView._overrideLayoutParameters(
        withMinimumLayoutSize: insetSize,
        minimumUnobscuredSizeOverride: insetSize,
        maximumUnobscuredSizeOverride: insetSize
    )
    #else
    webView._topContentInset = 75
    #endif
    return webView
}

// The header is tomato in the light color scheme and blue in the dark one. It has a child because an empty fixed
// element with a light-dark() background isn't sampled.
private func colorSchemeHeaderPageHTML(rootColorScheme: String) -> String {
    """
    <!DOCTYPE html>
    <html style='color-scheme: \(rootColorScheme)'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        html, body { margin: 0; width: 100%; height: 100%; }
        .tall { height: 4000px; }
        header { position: fixed; top: 0; left: 0; width: 100%; height: 100px; background: light-dark(tomato, blue); }
    </style>
    <body>
        <header><div></div></header>
        <div class='tall'></div>
    </body>
    """
}

@MainActor
struct SampledPageTopColorTests {
    @Test
    func colorSchemeChangeRefreshesTopEdgeAfterUserInteraction() async throws {
        let webView = try makeWebViewForTopFixedContainerSampling()

        try await webView.load(html: colorSchemeHeaderPageHTML(rootColorScheme: "light"))
        try await waitForCondition("the initial top fixed-position color to be sampled") {
            compareColors(webView._sampledTopFixedPositionContentColor, tomato)
        }

        await webView.click(at: CGPoint(x: 400, y: 300))

        // After user interaction, a plain style change of the header doesn't change the color.
        do {
            let colorUpdated = Mutex(false)
            let observation = webView.observe(\._sampledTopFixedPositionContentColor) { _, _ in
                colorUpdated.withLock { $0 = true }
            }
            defer { observation.invalidate() }

            try await webView.evaluateJavaScriptWithoutUserGesture("document.querySelector('header').style.backgroundColor = 'green'")
            await webView.nextPresentationUpdate()
            await webView.nextPresentationUpdate()

            let didUpdateColor = colorUpdated.withLock { $0 }
            #expect(!didUpdateColor)
            #expect(compareColors(webView._sampledTopFixedPositionContentColor, tomato))
        }

        try await webView.evaluateJavaScriptWithoutUserGesture("document.querySelector('header').style.removeProperty('background-color')")
        await webView.nextPresentationUpdate()

        // A change of color scheme does, as it is still the same header.
        try await webView.evaluateJavaScriptWithoutUserGesture("document.documentElement.style.colorScheme = 'dark'")
        try await waitForCondition("the sampled top fixed-position color to refresh to blue") {
            compareColors(webView._sampledTopFixedPositionContentColor, blue)
        }

        #expect(webView._fixedContainerEdges.contains(.top))
    }

    #if WTF_PLATFORM_MAC
    @Test
    func appearanceChangeRefreshesTopEdgeAfterUserInteraction() async throws {
        let webView = try makeWebViewForTopFixedContainerSampling()
        webView.appearance = NSAppearance(named: .aqua)

        try await webView.load(html: colorSchemeHeaderPageHTML(rootColorScheme: "light dark"))
        try await waitForCondition("the initial top fixed-position color to be sampled") {
            compareColors(webView._sampledTopFixedPositionContentColor, tomato)
        }

        await webView.click(at: CGPoint(x: 400, y: 300))

        // The page supports both color schemes, so nothing in its own style changes with the appearance of the system.
        webView.appearance = NSAppearance(named: .darkAqua)
        try await waitForCondition("the sampled top fixed-position color to refresh to blue") {
            compareColors(webView._sampledTopFixedPositionContentColor, blue)
        }

        #expect(webView._fixedContainerEdges.contains(.top))
    }
    #endif // WTF_PLATFORM_MAC
}

#endif // ENABLE_CONTENT_INSET_BACKGROUND_FILL && (WTF_PLATFORM_MAC || WTF_PLATFORM_IOS || WTF_PLATFORM_MACCATALYST || WTF_PLATFORM_VISION)
