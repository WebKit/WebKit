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

import AppKit
import Foundation
@_spi(WebKitAdditions_Testing) @_spi(Testing) import WebKit
import SwiftUI
import struct Swift.String
import Testing
private import TestWebKitAPILibrary
private import Recap

extension AppKitGesturesTests {
    @MainActor
    @Suite(.serialized, .timeLimit(.minutes(1)))
    final class RefreshControl: AppKitGestureTestSuite {
        static let text = "Here's to the crazy ones."

        @MainActor
        private final class RefreshCounter {
            var count = 0
        }

        private static let dragInset: CGFloat = 80

        private static let refreshDispatchWindow = Duration.seconds(1)

        let recap = Recap.shared

        let page = WebPage()

        let windowHost: TestWindowHost

        private let refreshes = RefreshCounter()

        init() async throws {
            let contentSize = NSSize(width: 800, height: 600)

            self.windowHost = TestWindowHost(size: contentSize) { [page, refreshes] in
                WebView(page)
                    .refreshable {
                        await MainActor.run {
                            refreshes.count += 1
                        }
                    }
            }

            await NSApp.waitForActivation()
        }
    }
}

extension AppKitGesturesTests.RefreshControl {
    // MARK: - Tests

    @Test
    func trackpadSwipeDownTriggersRefresh() async throws {
        try await loadTallPage()
        await page.waitForNextPresentationUpdate()

        let (scrollStart, scrollEnd) = try verticalDrag()

        await recap.play { composer in
            composer._wk_scroll(withStart: scrollStart, end: scrollEnd, duration: .seconds(0.1))
        }

        let pollInterval = Duration.milliseconds(100)
        for _ in 0..<Int(Self.refreshDispatchWindow / pollInterval) {
            if refreshes.count > 0 {
                break
            }

            try await Task.sleep(for: pollInterval)
        }

        #expect(refreshes.count == 1)
    }

    @Test(
        .bug("https://webkit.org/b/322154", "App exposé gesture should not trigger pull to refresh"),
        arguments: [false, true]
    )
    func trackpadAppExposeGestureDoesNotTriggerRefresh(gesturesForGestureEvents: Bool) async throws {
        page.setWebFeature("UseAppKitGesturesForGestureEvents", enabled: gesturesForGestureEvents)

        try await loadTallPage()
        await page.waitForNextPresentationUpdate()

        let (scrollStart, scrollEnd) = try verticalDrag()

        await recap.play { composer in
            composer._wk_scroll(withStart: scrollStart, end: scrollEnd, duration: .seconds(0.1), multiFinger: true)
        }

        await page.waitForNextPresentationUpdate()

        try await Task.sleep(for: Self.refreshDispatchWindow)

        #expect(refreshes.count == 0)
    }

    // MARK: - Helpers

    private func loadTallPage() async throws {
        let html = """
            <body style="margin: 0; width: 100%; height: 4000px;
                         background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);">
            </body>
            """
        try await page.load(html: html).wait()
    }

    private func verticalDrag() throws -> (start: CGPoint, end: CGPoint) {
        let contentHeight = try #require(window.contentViewController?.view.frame.height)
        let x = window.frame.width / 2

        let nearTop = screenBounds(ofPointInWindowCoordinates: NSPoint(x: x, y: contentHeight - Self.dragInset))
        let nearBottom = screenBounds(ofPointInWindowCoordinates: NSPoint(x: x, y: Self.dragInset))

        return (nearTop, nearBottom)
    }
}

#endif
