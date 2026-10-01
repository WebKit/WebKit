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
import Testing
private import TestWebKitAPILibrary
private import Recap

private let manipulationSurfaceDragInset: CGFloat = 60

extension AppKitGesturesTests {
    // Site-specific quirks match on the URL of the page or of an embedded document, which a test cannot
    // choose for content it loads locally. `internals` is the only way to override it, so unlike the other
    // gesture suites this one needs the injected bundle that installs `internals`.
    @MainActor
    @Suite(.serialized, .timeLimit(.minutes(1)))
    final class Quirks: AppKitGestureTestSuite {
        static let text = "Here's to the crazy ones."

        let recap = Recap.shared

        let page: WebPage = {
            var configuration = WebPage.Configuration.withInternals()
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

extension AppKitGesturesTests.Quirks {
    @Test(arguments: verticalDragOverManipulationSurfaceArguments)
    func verticalDragOverManipulationSurface(styleValue: String, reachesContent: Bool) async throws {
        let surface = try await loadManipulationSurface(styleValue: styleValue)

        let start = CGPoint(x: surface.bounds.midX, y: surface.bounds.maxY - manipulationSurfaceDragInset)
        let end = CGPoint(x: surface.bounds.midX, y: surface.bounds.minY + manipulationSurfaceDragInset)

        await recap.play { composer in
            composer._wk_drag(withStart: start, end: end, duration: .seconds(1))
        }

        await page.waitForNextPresentationUpdate()

        try await expectDrag(from: start, to: end, over: surface, reachesContent: reachesContent)
    }

    @Test
    func horizontalDragOverManipulationSurface() async throws {
        let surface = try await loadManipulationSurface(styleValue: "pan-x pan-y")

        let start = CGPoint(x: surface.bounds.minX + manipulationSurfaceDragInset, y: surface.bounds.midY)
        let end = CGPoint(x: surface.bounds.maxX - manipulationSurfaceDragInset, y: surface.bounds.midY)

        await recap.play { composer in
            composer._wk_drag(withStart: start, end: end, duration: .seconds(1))
        }

        await page.waitForNextPresentationUpdate()

        try await expectDrag(from: start, to: end, over: surface, reachesContent: true)
    }

    @Test
    func clickAndHoldOnUnselectableMailListItemFiresContextMenuEventOnlyOnOutlook() async throws {
        let url = try #require(Bundle.testResources.url(forResource: "unselectable-mail-list-item", withExtension: "html"))
        try await page.load(url).wait()

        try await page.callJavaScript(arguments: ["url": "https://outlook.live.com/mail/"]) {
            "internals.setTopDocumentURLForQuirks(url);"
        }

        let activeQuirks = try await page.callJavaScript(returning: [String].self) {
            "return internals.activeQuirks();"
        }
        try #require(activeQuirks.contains("ShouldTreatLongClickAsSecondaryClickQuirk"))

        await page.waitForNextPresentationUpdate()

        let rowBounds = try await screenBounds(ofElementWithID: "row")

        await recap.play { composer in
            composer._wk_click(at: rowBounds.center, for: .seconds(1))
        }

        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        let events = try await page.callJavaScript(returning: [String].self) {
            "return window.events;"
        }
        #expect(events.contains("contextmenu"))
        #expect(!events.contains("click"))
    }
}

extension AppKitGesturesTests.Quirks {
    struct ManipulationSurface {
        let bounds: CGRect

        let initialScrollY: Double
    }

    private func loadManipulationSurface(styleValue: String) async throws -> ManipulationSurface {
        let url = try #require(Bundle.testResources.url(forResource: "manipulation-surface", withExtension: "html"))
        try await page.load(url).wait()
        await page.waitForNextPresentationUpdate()

        // Installed before the frame exists, so the frame's document picks the quirk up as it is created.
        try await page.callJavaScript(arguments: ["url": "https://www.google.com/maps/embed/v1/place?q=Cupertino"]) {
            "internals.setSubframeURLForQuirks(url);"
        }

        try await page.callJavaScript(
            arguments: ["styleValue": styleValue],
            script: loadManipulationSurfaceScript
        )
        await page.waitForNextPresentationUpdate()

        let manipulationSurfaceFrameID = "surfaceFrame"

        let subframeQuirks = try await page.callJavaScript(
            returning: [String].self,
            arguments: ["frameID": manipulationSurfaceFrameID]
        ) {
            "return document.getElementById(frameID).contentWindow.internals.activeQuirks();"
        }
        try #require(subframeQuirks.contains("NeedsGoogleMapsEmbedManipulationSurfaceQuirk"))

        try await page.callJavaScript(arguments: ["frameID": manipulationSurfaceFrameID]) {
            #"document.getElementById(frameID).scrollIntoView({ block: "center" });"#
        }
        await page.waitForNextPresentationUpdate()

        let scrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        try #require(scrollPosition.y > 0)

        return ManipulationSurface(
            bounds: try await screenBounds(ofElementWithID: manipulationSurfaceFrameID),
            initialScrollY: scrollPosition.y
        )
    }

    private func manipulationSurfaceEvents() async throws -> (down: Int, move: Int, up: Int, wheel: Int, translation: CGSize) {
        let values = try await page.callJavaScript(returning: [Double].self) {
            """
            const events = window.surfaceEvents;
            return [events.down, events.move, events.up, events.wheel, events.dx, events.dy];
            """
        }

        try #require(values.count == 6)

        return (
            down: Int(values[0]),
            move: Int(values[1]),
            up: Int(values[2]),
            wheel: Int(values[3]),
            translation: CGSize(width: values[4], height: values[5])
        )
    }

    private func expectDrag(
        from start: CGPoint,
        to end: CGPoint,
        over surface: ManipulationSurface,
        reachesContent: Bool
    ) async throws {
        let events = try await manipulationSurfaceEvents()
        let scrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())

        guard reachesContent else {
            #expect(events.down == 0)
            #expect(events.up == 0)
            #expect(events.wheel > 0)

            #expect(scrollPosition.y > surface.initialScrollY)
            return
        }

        #expect(events.down == 1)
        #expect(events.up == 1)
        #expect(events.move > 0)
        #expect(events.wheel == 0)

        #expect(abs(events.translation.width - (end.x - start.x)) < manipulationSurfaceDragInset)
        #expect(abs(events.translation.height - (end.y - start.y)) < manipulationSurfaceDragInset)

        #expect(scrollPosition.y == surface.initialScrollY)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
