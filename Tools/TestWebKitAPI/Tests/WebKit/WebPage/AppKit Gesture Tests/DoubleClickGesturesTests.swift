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
import SwiftUI
import struct Swift.String
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import Recap
private import AppKit_Private.NSMenu_Private

extension AppKitGesturesTests {
    @MainActor
    @Suite(.serialized, .timeLimit(.minutes(1)))
    final class DoubleClick: AppKitGestureTestSuite {
        static let text = "Here's to the crazy ones."

        let recap = Recap.shared

        let page: WebPage = {
            var configuration = WebPage.Configuration()
            configuration.requiresUserActionForEditingControlsManager = true
            return WebPage(configuration: configuration)
        }()

        let windowHost: TestWindowHost

        init() async throws {
            let contentSize = NSSize(width: 800, height: 600)

            self.windowHost = TestWindowHost(size: contentSize) { [page] in
                WebView(page)
                    .webViewBackForwardNavigationGestures(.enabled)
            }

            await NSApp.waitForActivation()
        }
    }
}

extension AppKitGesturesTests.DoubleClick {
    // MARK: - DOM dblclick / detail==2 coverage

    @Test(.disabled())
    func doubleClickWithListenerFiresDblclick() async throws {
        try await loadHTML(dblclickHandler: true)
        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: [.click, .dblclick]))

        let crazyBounds = try await screenBoundsOfText("crazy")

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }
        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        #expect(try await page.callJavaScript(JavaScriptMessages.EventLog()).contains(.init(type: .dblclick, detail: 2)))
    }

    @Test(.disabled())
    func doubleClickReportsDetailTwo() async throws {
        try await loadHTML(dblclickHandler: true)
        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: [.click, .dblclick]))

        let crazyBounds = try await screenBoundsOfText("crazy")

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }
        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        #expect(try await page.callJavaScript(JavaScriptMessages.EventLog()).contains(.init(type: .click, detail: 2)))
    }

    @Test(.disabled())
    func doubleClickWithListenerFiresDblclickAndSelectsWord() async throws {
        // A dblclick listener and a word selection coexist (only smart magnification
        // could suppress the selection, and this content is not zoomable).
        try await loadHTML(dblclickHandler: true)
        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: [.click, .dblclick]))

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBounds = try await screenBoundsOfText("crazy")
        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))
        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }
        await page.waitForNextPresentationUpdate()

        #expect(try await page.callJavaScript(JavaScriptMessages.EventLog()).contains(.init(type: .dblclick, detail: 2)))
        #expect(try await page.callJavaScript(JavaScriptMessages.GetSelection()) == crazySelection)
    }

    @Test(.disabled(), arguments: [true, false])
    func doubleClickWithListenerFiresDblclickRegardlessOfEditability(contentEditable: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable, dblclickHandler: true)
        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: [.click, .dblclick]))

        if contentEditable {
            // Establish a selection first so the synthetic click does not just move the insertion point.
            try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))
            await page.waitForNextPresentationUpdate()
        }

        let crazyBounds = try await screenBoundsOfText("crazy")

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }
        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        #expect(try await page.callJavaScript(JavaScriptMessages.EventLog()).contains(.init(type: .dblclick, detail: 2)))
    }

    @Test
    func singleClickReportsDetailOne() async throws {
        try await loadHTML()
        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: [.click, .dblclick]))

        let toBounds = try await screenBoundsOfText("to")

        await recap.play { composer in
            composer._wk_click(at: toBounds.center, for: .seconds(0.05))
        }
        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        let eventLog = try await page.callJavaScript(JavaScriptMessages.EventLog())
        #expect(eventLog.contains(.init(type: .click, detail: 1)))
        #expect(!eventLog.contains { $0.type == .dblclick })
    }

    @Test(.disabled())
    func clickingTwoDifferentWordsDoesNotFireDblclick() async throws {
        // Two clicks far apart in space are two single clicks, disambiguated by location — not a double click.
        try await loadHTML(dblclickHandler: true)
        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: [.click, .dblclick]))

        let toBounds = try await screenBoundsOfText("to")
        let onesBounds = try await screenBoundsOfText("ones")

        await recap.play { composer in
            composer._wk_click(at: toBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: onesBounds.center, for: .seconds(0.1))
        }
        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        let eventLog = try await page.callJavaScript(JavaScriptMessages.EventLog())
        #expect(!eventLog.contains { $0.type == .dblclick })
    }

    // MARK: - Smart magnification

    @Test(.disabled())
    func smartMagnificationGestureOnZoomableColumnDoesNotSelectWord() async throws {
        try await loadZoomableHTML()

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBounds = try await screenBoundsOfText("crazy")
        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))
        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }
        await page.waitForNextPresentationUpdate()

        #expect(try await page.callJavaScript(JavaScriptMessages.GetSelection()) != crazySelection)
    }

    @Test(.disabled(), arguments: [false, true])
    func styleAdjustmentCanBlockSmartMagnification(interactive: Bool) async throws {
        try await loadZoomableHTML()
        try await page.callJavaScript(
            arguments: ["elementID": "div", "interactive": interactive],
            script: styleAdjustmentForCustomWidgetScript
        )

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBounds = try await screenBoundsOfText("crazy")
        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))
        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }
        await page.waitForNextPresentationUpdate()

        #expect(try await page.callJavaScript(JavaScriptMessages.GetSelection()) == crazySelection)
    }

    @Test(.disabled())
    func doubleClickZoomableColumnWithListenerSelectsWordAndFiresDblclick() async throws {
        // A dblclick listener suppresses smart magnification, so
        // the dblclick fires AND the word is selected (no zoom).
        try await loadZoomableHTML(dblclickHandler: true)
        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: [.click, .dblclick]))

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBounds = try await screenBoundsOfText("crazy")
        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))
        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBounds.center, for: .seconds(0.1))
        }
        await page.waitForNextPresentationUpdate()

        #expect(try await page.callJavaScript(JavaScriptMessages.EventLog()).contains(.init(type: .dblclick, detail: 2)))
        #expect(try await page.callJavaScript(JavaScriptMessages.GetSelection()) == crazySelection)
    }

    private func loadZoomableHTML(dblclickHandler: Bool = false) async throws {
        let dblclickHandlerMarkup = dblclickHandler ? "ondblclick='void(0)'" : ""

        let html = """
            <div \(dblclickHandlerMarkup) id="div" style="width: 160px; font-size: 30px;">\(Self.text)</div>
            """

        try await page.load(html: html).wait()
    }
}

#endif
