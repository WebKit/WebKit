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

@MainActor
struct SampledPageTopColorTests {
    @Test
    func forcedUserGestureFromJSInjectionDoesNotClearUserInteraction() async throws {
        let webView = try makeWebViewForTopFixedContainerSampling()

        try await webView.load(testPageNamed: "top-fixed-element")
        try await waitForCondition("the initial top fixed-position color to be sampled") {
            compareColors(webView._sampledTopFixedPositionContentColor, tomato)
        }

        // Actual user interaction stops the top edge from being sampled again.
        await webView.click(at: CGPoint(x: 400, y: 300))

        // Script injection that forces a user gesture afterwards doesn't count as user interaction,
        // but it must not undo the interaction that already happened either.
        try await webView.callJavaScript(withUserGesture: true) { "'injected'" }

        let colorUpdated = Mutex(false)
        let observation = webView.observe(\._sampledTopFixedPositionContentColor) { _, _ in
            colorUpdated.withLock { $0 = true }
        }
        defer { observation.invalidate() }

        try await webView.callJavaScript(withUserGesture: false) {
            "document.querySelector('header').style.backgroundColor = 'blue'"
        }
        await webView.nextPresentationUpdate()
        await webView.nextPresentationUpdate()

        let didUpdateColor = colorUpdated.withLock { $0 }
        #expect(!didUpdateColor)
        #expect(compareColors(webView._sampledTopFixedPositionContentColor, tomato))
    }
}

#endif // ENABLE_CONTENT_INSET_BACKGROUND_FILL && (WTF_PLATFORM_MAC || WTF_PLATFORM_IOS || WTF_PLATFORM_MACCATALYST || WTF_PLATFORM_VISION)
