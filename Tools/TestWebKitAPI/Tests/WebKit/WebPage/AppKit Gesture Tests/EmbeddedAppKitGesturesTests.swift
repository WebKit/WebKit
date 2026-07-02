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

#if HAVE_APPKIT_GESTURES_SUPPORT

import Foundation
import struct Foundation.URL
@_spi(WebKitAdditions_Testing) @_spi(Testing) import WebKit
@_spi(Testing) import _WebKit_SwiftUI
import SwiftUI
import struct Swift.String
import struct _Concurrency.Task
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import Recap
private import AppKit_Private.NSMenu_Private

extension AppKitGesturesTests {
    @MainActor
    @Suite(.serialized, .timeLimit(.minutes(1)))
    struct Embedded: AppKitGestureTestSuite {
        @MainActor
        private final class ContentOffsetStorage {
            var value = CGPoint.zero
        }

        static let text = "Here's to the crazy ones."

        static let topInset: CGFloat = 100

        let recap = Recap.shared

        let page: WebPage = {
            var configuration = WebPage.Configuration()
            configuration.requiresUserActionForEditingControlsManager = true
            return WebPage(configuration: configuration)
        }()

        let window: NSWindow

        private let windowSize = NSSize(width: 800, height: 600)
        private let contentHeight: CGFloat = 2000

        private var contentOffset = ContentOffsetStorage()

        init() async throws {
            self.window = NSWindow(size: windowSize) { [windowSize, contentHeight, contentOffset, page] in
                ScrollView {
                    VStack(spacing: 0) {
                        WebView(page)
                            .frame(width: windowSize.width, height: windowSize.height)
                            .webViewObscuredContentInsets(EdgeInsets(top: Self.topInset, leading: 0, bottom: 0, trailing: 0))
                        Color.clear
                            .frame(height: contentHeight - windowSize.height)
                    }
                }
                .onScrollGeometryChange(for: CGPoint.self) { geometry in
                    geometry.contentOffset
                } action: { _, newContentOffset in
                    contentOffset.value = newContentOffset
                }
            }

            self.window.setFrameOrigin(.zero)
            NSApp.activate(ignoringOtherApps: true)
            self.window.makeKeyAndOrderFront(nil)
        }
    }
}

extension AppKitGesturesTests.Embedded {
    @Test
    func scrollOverNonScrollableWebViewPropagatesToEnclosingScrollView() async throws {
        let html = """
            <body style='margin:0; background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);'>
                <p>not scrollable</p>
                <img id="img" src="400x400-green.png" style="display: block; margin: 50px;">
            </body>
            """

        let baseURL = try #require(Bundle.testResources.resourceURL)

        try await page.load(html: html, baseURL: baseURL).wait()
        await page.waitForNextPresentationUpdate()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        let cgScreenOrigin = screenBounds(ofPointInWindowCoordinates: .init(x: 0, y: windowSize.height))
        let viewportInCGScreen = CGRect(origin: cgScreenOrigin, size: windowSize)

        let dragDistance: CGFloat = 100
        let center = viewportInCGScreen.center
        let startPoint = CGPoint(x: center.x, y: center.y + dragDistance / 2)
        let endPoint = CGPoint(x: center.x, y: center.y - dragDistance / 2)

        let initialContentOffset = contentOffset.value

        await recap.play { composer in
            composer.drag(withStart: startPoint, end: endPoint, duration: 0.5)
            composer.advanceTime(0.1)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        // The test passes if the enclosing SwiftUI ScrollView scrolled.
        #expect(contentOffset.value != initialContentOffset)
    }

    @Test
    func pressDragOverTextInWebViewInsideEnclosingScrollViewCreatesSelection() async throws {
        // Center the text vertically so the press-drag stays well within the visible scroll area.
        let html = """
            <body style='margin:0;'>
                <div id="div" style="font-size: 30px; padding-top: 280px;">\(Self.text)</div>
            </body>
            """

        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        let toBounds = try await screenBoundsOfText("to")
        let crazyBounds = try await screenBoundsOfText("crazy")

        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))
        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_drag(
                withStart: toBounds.center,
                end: crazyBounds.center,
                duration: .seconds(1.5),
                pressAndWait: .seconds(1.0)
            )
        }

        await page.waitForNextPresentationUpdate()

        let selection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        guard case .range = selection else {
            Issue.record("expected press-drag over text in an enclosing scroll view to create a range selection, got \(selection)")
            return
        }
    }

    @Test
    func quickDragOverTextInWebViewInsideEnclosingScrollViewScrolls() async throws {
        let html = """
            <body style='margin:0; background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);'>
                <div id="div" style="font-size: 30px; padding-top: 280px;">\(Self.text)</div>
            </body>
            """

        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        let crazyBounds = try await screenBoundsOfText("crazy")

        // A quick (non-press) drag whose touch-down is over selectable text must still hand off to the
        // enclosing scroll view, just like over an image.
        let dragDistance: CGFloat = 100
        let center = crazyBounds.center
        let startPoint = CGPoint(x: center.x, y: center.y + dragDistance / 2)
        let endPoint = CGPoint(x: center.x, y: center.y - dragDistance / 2)

        let initialContentOffset = contentOffset.value

        await recap.play { composer in
            composer.drag(withStart: startPoint, end: endPoint, duration: 0.5)
            composer.advanceTime(0.1)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        #expect(contentOffset.value != initialContentOffset)
    }

    @Test
    func doubleClickingSelectsWordWhenScrolledToTopWithObscuredContentInset() async throws {
        let html = """
            <body style="margin: 0">
                <h2 id="div" style="display: inline-block; margin: 0; font-size: 30px;">\(Self.text)</h2>
                <input id="search" type="search" style="display: block; margin: 0; width: 320px; height: 300px;">
            </body>
            """
        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBounds = try await screenBoundsOfText("crazy")

        await page.waitForNextPresentationUpdate()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }

        await page.waitForNextPresentationUpdate()

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())
        #expect(newSelection == crazySelection)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
