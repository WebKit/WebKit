// Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

import CoreGraphics
import Foundation
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.TestWKWebView
import Testing
import UniformTypeIdentifiers
import WebKit
private import WebKit_Private.WKWebViewPrivate

import struct Swift.String

#if WTF_PLATFORM_IOS_FAMILY
private import UIKit
#else
private import AppKit
#endif

@MainActor
struct CopyRTFTests {
    private func readRTFDataFromPasteboard() -> Data? {
        #if USE_APPKIT
        NSPasteboard.general.data(forType: .rtf)
        #else
        UIPasteboard.general.value(forPasteboardType: UTType.rtf.identifier) as? Data
        #endif
    }

    private func copyAttributedStringFromHTML(
        _ html: String,
        forceDarkMode: Bool,
        disableUnprefixedUserSelect: Bool = false,
        sourceLocation: SourceLocation = #_sourceLocation
    ) async throws -> NSAttributedString {
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))

        webView.configuration.setFeature("WriteRichTextDataWhenCopyingOrDragging", enabled: true)

        if disableUnprefixedUserSelect {
            webView.configuration.setFeature("CSSUserSelectEnabled", enabled: false)
        }

        if forceDarkMode {
            webView.forceDarkMode()
        }

        try await webView.load(html: html)

        webView.selectAll(nil)
        _ = await webView._executeEditCommand("Copy", argument: nil)

        let rtfData = try #require(readRTFDataFromPasteboard(), sourceLocation: sourceLocation)
        return try unsafe NSAttributedString(data: rtfData, options: [:], documentAttributes: nil)
    }

    private func checkColor(
        _ value: Any?,
        red: CGFloat,
        green: CGFloat,
        blue: CGFloat,
        alpha: CGFloat,
        sourceLocation: SourceLocation = #_sourceLocation
    ) {
        let color = value as? CocoaColor
        #expect(color != nil, sourceLocation: sourceLocation)

        var observedRed: CGFloat = 0
        var observedGreen: CGFloat = 0
        var observedBlue: CGFloat = 0
        var observedAlpha: CGFloat = 0
        unsafe color?.getRed(&observedRed, green: &observedGreen, blue: &observedBlue, alpha: &observedAlpha)

        #expect(observedRed == red, sourceLocation: sourceLocation)
        #expect(observedGreen == green, sourceLocation: sourceLocation)
        #expect(observedBlue == blue, sourceLocation: sourceLocation)
        #expect(observedAlpha == alpha, sourceLocation: sourceLocation)
    }

    @Test
    func stripsDefaultTextColorOfDarkContent() async throws {
        let attributedString = try await copyAttributedStringFromHTML(
            #"<style>:root { color-scheme: dark }</style> Default <span style="color: black">Black</span> <span style="color: white">White</span>"#,
            forceDarkMode: true
        )

        var index = 0
        let range = NSRange(location: 0, length: attributedString.length)
        unsafe attributedString.enumerateAttribute(.foregroundColor, in: range) { value, _, _ in
            switch index {
            case 0:
                #expect(value == nil)
            case 1:
                #expect(value != nil)
                checkColor(value, red: 0, green: 0, blue: 0, alpha: 1)
            case 2:
                #expect(value == nil)
            default:
                Issue.record("Unexpected foreground color run \(index)")
            }
            index += 1
        }

        #expect(index == 3)
    }

    @Test
    func stripsDefaultTextColorOfLightContent() async throws {
        let attributedString = try await copyAttributedStringFromHTML(
            #"Default <span style="color: white">White</span> <span style="color: black">Black</span>"#,
            forceDarkMode: false
        )

        var index = 0
        let range = NSRange(location: 0, length: attributedString.length)
        unsafe attributedString.enumerateAttribute(.foregroundColor, in: range) { value, _, _ in
            switch index {
            case 0:
                #expect(value == nil)
            case 1:
                #expect(value != nil)
                checkColor(value, red: 1, green: 1, blue: 1, alpha: 1)
            case 2:
                #expect(value == nil)
            default:
                Issue.record("Unexpected foreground color run \(index)")
            }
            index += 1
        }

        #expect(index == 3)
    }

    #if ENABLE_DATA_DETECTION
    @Test
    func stripsDataDetectorsLinks() async throws {
        let attributedString = try await copyAttributedStringFromHTML(
            #"<a href="https://www.apple.com">Apple</a> "#
                + #"<a href="x-apple-data-detectors://0" dir="ltr" x-apple-data-detectors="true" "#
                + #"x-apple-data-detectors-type="calendar-event" x-apple-data-detectors-result="0" "#
                + #"style="color: currentcolor; text-decoration-color: rgba(128, 128, 128, 0.38);">on Friday 11/6 at 4pm</a>"#,
            forceDarkMode: false
        )

        var index = 0
        let range = NSRange(location: 0, length: attributedString.length)
        unsafe attributedString.enumerateAttribute(.link, in: range) { value, _, _ in
            switch index {
            case 0:
                #expect(value != nil)
            case 1:
                #expect(value == nil)
            default:
                Issue.record("Unexpected link run \(index)")
            }
            index += 1
        }

        #expect(index == 2)
    }
    #endif // ENABLE_DATA_DETECTION

    @Test
    func stripsUserSelectNone() async throws {
        let attributedString = try await copyAttributedStringFromHTML(
            "hello <span style='-webkit-user-select: none; user-select: none;'>world "
                + "<span style='-webkit-user-select: initial; user-select: initial;'>WebKit </span></span>"
                + "<div style='-webkit-user-select: none; user-select: none;'>some<br>user-select-none<br>content</div><span inert>foo </span>bar",
            forceDarkMode: false
        )

        #expect(attributedString.string == "hello bar")
    }

    @Test
    func stripsUserSelectNonewithLegacyUserSelectBehavior() async throws {
        // Same content as stripsUserSelectNone. On legacy behavior,
        // 'initial' is resolved to 'text':
        let attributedString = try await copyAttributedStringFromHTML(
            "hello <span style='-webkit-user-select: none; user-select: none;'>world "
                + "<span style='-webkit-user-select: initial; user-select: initial;'>WebKit </span></span>"
                + "<div style='-webkit-user-select: none; user-select: none;'>some<br>user-select-none<br>content</div><span inert>foo </span>bar",
            forceDarkMode: false,
            disableUnprefixedUserSelect: true
        )

        #expect(attributedString.string == "hello WebKit bar")
    }

    @Test
    func stripsUserSelectNoneQuirks() async throws {
        let attributedString = try await copyAttributedStringFromHTML(
            "<meta name='confluence-request-time' content='1689029750234'>"
                + "hello <span style='-webkit-user-select: none; user-select: none;'>world "
                + "<span style='-webkit-user-select: initial; user-select: initial;'>WebKit </span></span>"
                + "<div style='-webkit-user-select: none; user-select: none;'>some<br>user-select-none<br>content</div><span inert>foo </span>bar",
            forceDarkMode: false
        )

        #expect(attributedString.string == "hello world WebKit\nsome\nuser-select-none\ncontent\nfoo bar")
    }

    @Test
    func doesNotCrashWithDirectoryFileWrapperFromImageSrcset() async throws {
        // Regression test: copying an <img> with srcset="." could produce a directory-backed NSFileWrapper.
        // Calling -regularFileContents on a non-regular file wrapper throws an NSException, which would
        // terminate the web content process as it propagated through C++ frames in HTMLConverter.
        let webView = TestWKWebView(frame: CGRect(x: 0, y: 0, width: 400, height: 400))

        let navigationDelegate = TestNavigationDelegate()
        var terminated = false
        navigationDelegate.webContentProcessDidTerminate = { _, _ in
            terminated = true
        }
        webView.navigationDelegate = navigationDelegate

        webView.configuration.setFeature("WriteRichTextDataWhenCopyingOrDragging", enabled: true)

        try await webView.load(html: "<img srcset='.'>")
        webView.selectAll(nil)
        _ = await webView._executeEditCommand("Copy", argument: nil)

        try await Task.sleep(for: .milliseconds(500))

        // The web view only holds its navigation delegate weakly.
        withExtendedLifetime(navigationDelegate) {
            #expect(!terminated)
        }
    }
}
