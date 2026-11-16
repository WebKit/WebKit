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

#if WTF_PLATFORM_MAC && ENABLE_SWIFTUI

import AppKit
import Foundation
import SwiftUI
import Testing
@_spi(Testing) @_spi(CrossImportOverlay) import WebKit
private import TestWebKitAPILibrary

// Certain event delivery (such as mouseMove) is gated behind -isKeyWindow.
private final class KeyWindow: NSWindow {
    override var isKeyWindow: Bool { true }
}

@MainActor
struct WebPageMouseEventsTests {
    private let page = WebPage()
    private let window: NSWindow

    init() async throws {
        self.window = KeyWindow(size: NSSize(width: 400, height: 400)) { [page] in
            WebView(page)
        }
        self.window.setFrameOrigin(.zero)
        self.window.makeKeyAndOrderFront(nil)
    }

    @Test
    func mouseDownUpFiresClickHandler() async throws {
        let html = """
            <body style="margin:0;width:100%;height:100vh"
                  onclick="window.clicked=true">x</body>
            """
        try await page.load(html: html).wait()

        let center = CGPoint(x: 200, y: 200)
        page.mouseDown(at: center)
        page.mouseUp(at: center)
        await page.waitForPendingMouseEvents()

        let fired = try await page.callJavaScript("return window.clicked === true;") as? Bool
        #expect(fired == true)
    }

    @Test
    func mouseMoveFiresMouseMoveHandler() async throws {
        try await loadMouseMoveRecorder()

        page.mouseMove(to: NSPoint(x: 120, y: 200))
        await page.waitForPendingMouseEvents()

        page.mouseMove(to: NSPoint(x: 280, y: 200))
        await page.waitForPendingMouseEvents()

        let moves = try await recordedMoves()
        #expect(moves.count >= 2)

        let first = try #require(moves.first)
        let last = try #require(moves.last)
        #expect(last > first)
    }

    @Test
    func sendingMouseMovedToWebViewDoesNotReachPage() async throws {
        try await loadMouseMoveRecorder()

        let event = try #require(
            NSEvent.mouseEvent(
                with: .mouseMoved,
                location: NSPoint(x: 150, y: 200),
                modifierFlags: [],
                timestamp: ProcessInfo.processInfo.systemUptime,
                windowNumber: window.windowNumber,
                context: nil,
                eventNumber: 0,
                clickCount: 0,
                pressure: 0
            )
        )

        page.backingWebView.mouseMoved(with: event)
        await page.waitForPendingMouseEvents()

        let afterResponderChain = try await recordedMoves()
        #expect(afterResponderChain.isEmpty)
    }
}

extension WebPageMouseEventsTests {
    private func loadMouseMoveRecorder() async throws {
        let html = """
            <body style="margin: 0; width: 100%; height: 100vh;"></body>
            """

        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript {
            """
            window.moves = [];
            document.addEventListener("mousemove", event => window.moves.push(event.clientX));
            """
        }
    }

    private func recordedMoves() async throws -> [Double] {
        try await page.callJavaScript(returning: [Double].self) {
            "return window.moves;"
        }
    }
}

#endif // WTF_PLATFORM_MAC && ENABLE_SWIFTUI
