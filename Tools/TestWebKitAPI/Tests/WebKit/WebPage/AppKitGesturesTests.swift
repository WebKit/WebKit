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
@_spi(WebKitAdditions_Testing) import WebKit
import SwiftUI
import struct Swift.String
import struct _Concurrency.Task
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import Recap

private actor Recap {
    static let shared = Recap()

    func play(events: @Sendable (_ composer: any RCPEventStreamComposer) -> Void) async {
        let eventStream: RCPSyntheticEventStream = RCPSyntheticEventStream { composer in
            guard let composer else {
                preconditionFailure()
            }
            composer.senderProperties = ._wk_trackpadSender()
            events(composer)
        }

        await RCPInlinePlayer.play(eventStream, options: .init())
    }
}

@MainActor
private func convertToCoreGraphicsScreenCoordinates(rectInViewportCoordinates: DOMRect, window: NSWindow) -> CGRect {
    guard let contentViewController = window.contentViewController else {
        preconditionFailure()
    }

    guard let screen = window.screen else {
        preconditionFailure()
    }

    let inViewportCoordinates = CGRect(rectInViewportCoordinates)

    let inWindowCoordinates = CGRect(
        x: inViewportCoordinates.origin.x,
        y: contentViewController.view.frame.height - inViewportCoordinates.origin.y - inViewportCoordinates.size.height,
        width: inViewportCoordinates.size.width,
        height: inViewportCoordinates.size.height,
    )

    let inAppKitScreenCoordinates = window.convertToScreen(inWindowCoordinates)

    let inCoreGraphicsScreenCoordinates = CGRect(
        x: inAppKitScreenCoordinates.origin.x,
        y: screen.frame.maxY - inAppKitScreenCoordinates.maxY,
        width: inAppKitScreenCoordinates.width,
        height: inAppKitScreenCoordinates.height,
    )

    return inCoreGraphicsScreenCoordinates
}

@MainActor
@Suite(.serialized)
struct AppKitGesturesTests {
    private static let text = "Here's to the crazy ones."

    private static let html = """
        <div id="div" contenteditable style="font-size: 30px;">\(text)</div>
        """

    private let recap = Recap.shared
    private let page = WebPage()
    private let window: NSWindow

    init() async throws {
        try await self.page.load(html: Self.html).wait()

        let contentSize = NSSize(width: 800, height: 600)

        self.window = NSWindow(size: contentSize) { [page] in
            WebView(page)
        }

        self.window.setFrameOrigin(.zero)
        NSApp.activate(ignoringOtherApps: true)
        self.window.makeKeyAndOrderFront(nil)
    }

    @Test
    func doubleClickingInWordSelectsWord() async throws {
        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )
        try await page.callJavaScript(JavaScriptMessages.SetSelection(crazySelection))

        let crazyBoundsInViewportCoordinates = try await page.callJavaScript(JavaScriptMessages.SelectionBoundingClientRect())

        let crazyBoundsInScreenCoordinates = convertToCoreGraphicsScreenCoordinates(
            rectInViewportCoordinates: crazyBoundsInViewportCoordinates,
            window: window
        )

        let point = CGPoint(
            x: crazyBoundsInScreenCoordinates.midX,
            y: crazyBoundsInScreenCoordinates.midY
        )

        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))

        await page.waitForNextPresentationUpdate()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        await recap.play { composer in
            composer._wk_click(point)
            composer.advanceTime(0.1)
            composer._wk_click(point)
        }

        await page.waitForNextPresentationUpdate()

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(newSelection == crazySelection)
    }

    @Test(arguments: [0.1, 0.5, 0.9])
    func clickingInWordChangesSelection(fractionOfWordToClick: Double) async throws {
        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", range: crazyRange))

        let crazyBoundsInViewportCoordinates = try await page.callJavaScript(JavaScriptMessages.SelectionBoundingClientRect())

        let crazyBoundsInScreenCoordinates = convertToCoreGraphicsScreenCoordinates(
            rectInViewportCoordinates: crazyBoundsInViewportCoordinates,
            window: window
        )

        let point = CGPoint(
            x: crazyBoundsInScreenCoordinates.origin.x + (crazyBoundsInScreenCoordinates.size.width * fractionOfWordToClick),
            y: crazyBoundsInScreenCoordinates.midY
        )

        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))

        await page.waitForNextPresentationUpdate()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        await recap.play { composer in
            composer._wk_click(point)
        }

        await page.waitForNextPresentationUpdate()

        // This is a rough approximation of the heuristic the implementation uses.
        let offset = fractionOfWordToClick < 0.2 ? crazyRange.lowerBound : crazyRange.upperBound

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())
        #expect(newSelection == .collapsed(.init(in: "div", at: offset)))
    }

    @Test
    func clickingChangesSelection() async throws {
        let page = WebPage()

        let text = "Here's to the crazy ones."
        let html = """
            <div id="div" contenteditable style="font-size: 30px;">\(text)</div>
            """

        try await page.load(html: html).wait()

        let contentSize = NSSize(width: 800, height: 600)

        let window = NSWindow(size: contentSize) {
            WebView(page)
        }

        window.setFrameOrigin(.zero)
        window.makeKeyAndOrderFront(nil)

        let crazyRange = try #require(text.utf16Range(of: "crazy"))
        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", range: crazyRange))

        let crazyBoundsInViewportCoordinates = try await CGRect(page.callJavaScript(JavaScriptMessages.SelectionBoundingClientRect()))

        let crazyBoundsInAppKitCoordinates = CGRect(
            x: crazyBoundsInViewportCoordinates.minX,
            y: contentSize.height - crazyBoundsInViewportCoordinates.maxY,
            width: crazyBoundsInViewportCoordinates.width,
            height: crazyBoundsInViewportCoordinates.height,
        )

        let middleOfCrazy = CGPoint(x: crazyBoundsInAppKitCoordinates.midX, y: crazyBoundsInAppKitCoordinates.midY)

        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))

        let waitForSelectionChange = """
            return await new Promise(resolve => {
                document.addEventListener("selectionchange", () => {
                    const offset = window.getSelection().focusOffset;
                    resolve(offset);
                });
            });
            """

        async let newSelection = page.callJavaScript(waitForSelectionChange) as? Int

        // Ensure the JS `selectionchange` event listener is installed before performing the click.
        await Task.yield()

        page.click(at: middleOfCrazy)

        let selection = try await newSelection
        let expected = "Here's to the cra".count
        #expect(selection == expected)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
