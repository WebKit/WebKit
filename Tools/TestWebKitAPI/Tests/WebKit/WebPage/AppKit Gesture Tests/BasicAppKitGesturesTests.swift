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
import struct _Concurrency.Task
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import Recap
private import AppKit_Private.NSMenu_Private

extension AppKitGesturesTests {
    @MainActor
    @Suite(.serialized, .timeLimit(.minutes(1)))
    struct Basic: AppKitGestureTestSuite {
        static let text = "Here's to the crazy ones."

        let recap = Recap.shared

        let page: WebPage = {
            var configuration = WebPage.Configuration()
            configuration.requiresUserActionForEditingControlsManager = true
            return WebPage(configuration: configuration)
        }()

        let window: NSWindow

        init() async throws {
            let contentSize = NSSize(width: 800, height: 600)

            self.window = NSWindow(size: contentSize) { [page] in
                WebView(page)
                    .webViewBackForwardNavigationGestures(.enabled)
            }

            self.window.setFrameOrigin(.zero)
            NSApp.activate(ignoringOtherApps: true)
            self.window.makeKeyAndOrderFront(nil)

            await NSApp.waitForActivation()
        }
    }
}

extension AppKitGesturesTests.Basic {
    @Test(
        .bug("https://webkit.org/b/314880", "Only mouse tracking mode produces pointer events"),
        arguments: [true, false]
    )
    func singleClickFiresPointerMouseAndClickEvents(contentEditable: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable)

        let expectedEvents: [DOMEventType] = [.pointerdown, .mousedown, .pointerup, .mouseup, .click]

        try await page.callJavaScript(JavaScriptMessages.InstallEventLog(in: "div", for: expectedEvents))

        if contentEditable {
            // FIXME: <rdar://177201499> This workaround establishes a selection first so that the synthetic click does not change insertion point.
            try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))
            await page.waitForNextPresentationUpdate()
        }

        let toBounds = try await screenBoundsOfText("to")

        await recap.play { composer in
            composer._wk_click(at: toBounds.center, for: .seconds(0.05))
        }

        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        let eventLog = try await page.callJavaScript(JavaScriptMessages.EventLog())

        for eventType in expectedEvents {
            #expect(eventLog.contains { $0.type == eventType })
        }
    }

    @Test(arguments: [true, false])
    func updatingTextRangeSelectionByUserInteractionUpdatesEditorState(contentEditable: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable)

        let toBounds = try await screenBoundsOfText("to")
        let crazyBounds = try await screenBoundsOfText("crazy")

        let editorStateSnapshots = page.editorStateSnapshots()

        await recap.play { composer in
            composer._wk_click(at: toBounds.center, for: .seconds(0.1))

            composer.advanceTime(0.1)

            composer.drag(withStart: toBounds.center, end: crazyBounds.center, duration: 1)

            composer.advanceTime(0.1)

            composer._wk_mouseUp()
        }

        let firstRangeEditorState = try await #require(
            editorStateSnapshots.first { @Sendable state in
                state.selectionType == .range
            }
        )

        #expect(firstRangeEditorState.postLayoutData != nil)
    }

    @Test(
        arguments: [true, false]
    )
    func draggingSelectionToWindowEdgeAutoscrolls(dragPastEdge: Bool) async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        let startViewportCoordinates = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "line97"))
        let startBounds = screenBounds(ofRectInViewportCoordinates: startViewportCoordinates)

        let initialScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(CGPoint(initialScrollPosition) == .zero)

        // Arbitrarily chosen to be close to the edge of the window.
        let dragEndYInWindow: CGFloat = dragPastEdge ? -20 : 20
        let dragEnd = screenBounds(ofPointInWindowCoordinates: CGPoint(x: window.frame.width / 2, y: dragEndYInWindow))

        await recap.play { composer in
            // Click-and-hold to begin a range selection, then drag to the bottom edge and hold.
            composer._wk_click(at: startBounds.center, for: .seconds(0.5))
            composer.advanceTime(0.1)
            composer._wk_drag(withStart: startBounds.center, end: dragEnd, duration: .seconds(1), release: false)

            // Hold at the edge so the autoscroll timer fires repeatedly.
            // The longer the duration the further down the scroll goes.
            composer.advanceTime(0.5)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        let finalScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(finalScrollPosition.x == 0)
        #expect(finalScrollPosition.y > 0)

        let expectedEndPosition: JavaScriptSelection.Position = dragPastEdge ? .init(in: "line90", at: 18) : .init(in: "line91", at: 2)

        let selection = try await page.callJavaScript(JavaScriptMessages.GetSelection())
        #expect(selection == .range(base: .init(in: "line97", at: 14), extent: expectedEndPosition))
    }

    @Test
    func holdingSelectionDragNearEdgeKeepsScrolling() async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        let startBounds = try await screenBounds(ofElementWithID: "line97")

        // Record (timestamp, scrollY) on every scroll event so we can observe that scrolling continues
        // throughout the hold, rather than scrolling in one burst and stopping early (the pre-fix bug).
        try await page.callJavaScript {
            """
            window.__wkScrollLog = [];
            window.addEventListener("scroll", () => window.__wkScrollLog.push(performance.now(), window.scrollY));
            """
        }

        // Just inside the bottom edge of the window (window coordinates have a bottom-left origin).
        let dragEnd = screenBounds(ofPointInWindowCoordinates: CGPoint(x: window.frame.width / 2, y: 20))

        await recap.play { composer in
            composer._wk_click(at: startBounds.center, for: .seconds(0.5))
            composer.advanceTime(0.1)
            composer._wk_drag(withStart: startBounds.center, end: dragEnd, duration: .seconds(1), release: false)

            // Hold near the edge for a couple of seconds so the autoscroll timer fires many times.
            composer.advanceTime(2.0)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        let log = try await page.callJavaScript(returning: [Double].self) { "return window.__wkScrollLog;" }
        let timestamps = stride(from: 0, to: log.count, by: 2).map { log[$0] }
        let offsets = stride(from: 1, to: log.count, by: 2).map { log[$0] }

        let firstTimestamp = try #require(timestamps.first)
        let lastTimestamp = try #require(timestamps.last)
        let lastOffset = try #require(offsets.last)

        // The page actually scrolled...
        #expect(lastOffset > 0)
        // ...and kept scrolling across most of the ~2s hold instead of bursting then stopping early.
        // `performance.now()` is in milliseconds.
        #expect(lastTimestamp - firstTimestamp > 1000)
    }

    @Test
    func holdingSelectionDragMidContentDoesNotAutoscroll() async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        let startBounds = try await screenBounds(ofElementWithID: "line97")

        let initialScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(CGPoint(initialScrollPosition) == .zero)

        let contentHeight = try #require(window.contentViewController?.view.frame.height)
        let midContent = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: contentHeight / 2))

        await recap.play { composer in
            composer._wk_click(at: startBounds.center, for: .seconds(0.5))
            composer.advanceTime(0.1)
            composer._wk_drag(withStart: startBounds.center, end: midContent, duration: .seconds(1), release: false)
            composer.advanceTime(1.0)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        let finalScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(CGPoint(finalScrollPosition) == .zero)
    }

    @Test
    func selectionAutoscrollStopsAfterGestureEnds() async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        let startBounds = try await screenBounds(ofElementWithID: "line97")

        let dragEnd = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: 20))

        await recap.play { composer in
            composer._wk_click(at: startBounds.center, for: .seconds(0.5))
            composer.advanceTime(0.1)
            composer._wk_drag(withStart: startBounds.center, end: dragEnd, duration: .seconds(1), release: false)
            composer.advanceTime(0.5)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        let scrollAfterLift = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(scrollAfterLift.y > 0)

        // Ending the gesture cancels autoscroll, so the page must not keep scrolling afterwards.
        try await Task.sleep(for: .seconds(0.5))

        let scrollLater = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(scrollLater.y == scrollAfterLift.y)
    }

    @Test
    func draggingSelectionToTopEdgeScrollsUp() async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        // Start partway down the page so there is room to scroll back up.
        try await page.callJavaScript { "window.scrollTo(0, 3000);" }
        await page.waitForNextPresentationUpdate()

        let initialScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(initialScrollPosition.y > 0)

        // Begin a selection on the visible text at the window's center, then drag up to the top edge.
        let contentHeight = try #require(window.contentViewController?.view.frame.height)
        let start = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: contentHeight / 2))
        let dragEnd = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: contentHeight - 20))

        await recap.play { composer in
            composer._wk_click(at: start, for: .seconds(0.5))
            composer.advanceTime(0.1)
            composer._wk_drag(withStart: start, end: dragEnd, duration: .seconds(1), release: false)
            composer.advanceTime(0.5)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        let finalScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(finalScrollPosition.y < initialScrollPosition.y)
    }

    @Test
    func selectionOriginatingNearEdgeWithShortDragDoesNotAutoscroll() async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        // Begin the selection inside the bottom edge band (~70pt from the bottom; the band is 100pt) and
        // drag only a short distance toward the edge - less than the ~50pt threshold. The selection
        // originates near the edge but the drag is too small to be a deliberate "scroll past the edge"
        // gesture, so the page must not autoscroll. (Pre-fix, being in the band alone started autoscroll.)
        let start = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: 70))
        let dragEnd = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: 45))

        await recap.play { composer in
            composer._wk_click(at: start, for: .seconds(0.5))
            composer.advanceTime(0.1)
            composer._wk_drag(withStart: start, end: dragEnd, duration: .seconds(1), release: false)

            // Hold so the autoscroll timer would fire repeatedly if it were going to.
            composer.advanceTime(1.0)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        let finalScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(CGPoint(finalScrollPosition) == .zero)
    }

    @Test
    func selectionOriginatingNearEdgeAutoscrollsAfterDraggingPastThreshold() async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        // Same near-edge origin as the short-drag test, but now drag well past the threshold (and past the
        // window edge) and hold, so the deliberate gesture engages autoscroll.
        let start = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: 70))
        let dragEnd = screenBounds(ofPointInWindowCoordinates: NSPoint(x: window.frame.width / 2, y: -40))

        await recap.play { composer in
            composer._wk_click(at: start, for: .seconds(0.5))
            composer.advanceTime(0.1)
            composer._wk_drag(withStart: start, end: dragEnd, duration: .seconds(1), release: false)
            composer.advanceTime(0.5)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        let finalScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(finalScrollPosition.y > 0)
    }

    @Test(
        .bug("rdar://176117750"),
        arguments: [true, false]
    )
    func clickingOnSelectedWordOpensContextMenu(contentEditable: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable)

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )
        try await page.callJavaScript(JavaScriptMessages.SetSelection(crazySelection))

        let crazyBoundsInScreenCoordinates = try await screenBoundsOfText("crazy")

        await page.waitForNextPresentationUpdate()

        await withSwizzledContextMenu {
            await recap.play { composer in
                composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
            }
        }

        await page.waitForNextPresentationUpdate()

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(newSelection == crazySelection)
    }

    @Test(arguments: [true, false])
    func clickingAndHoldingOnEmptyContentOpensContextMenu(contentEditable: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable)

        let middleOfWindow = screenBounds(ofPointInWindowCoordinates: window.frame.center)

        await withSwizzledContextMenu {
            await recap.play { composer in
                composer._wk_click(at: middleOfWindow, for: .seconds(1))
            }
        }

        await page.waitForNextPresentationUpdate()

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        if contentEditable {
            #expect(newSelection == .none)
        } else {
            #expect(newSelection == .collapsed(.init(in: "div", at: Self.text.count)))
        }
    }

    @Test(
        .bug(
            "rdar://179184036",
            "REGRESSION(313984@main): Long-press over non-editable text shows a context menu instead of selecting a word"
        )
    )
    func longPressOverTextSelectsWordAndDoesNotOpenContextMenu() async throws {
        try await loadHTML(contentEditable: false)

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBoundsInScreenCoordinates = try await screenBoundsOfText("crazy")
        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(1))
        }

        await page.waitForNextPresentationUpdate()

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(newSelection == crazySelection)
    }

    @Test
    func scrollingDoesNotRemoveTextSelection() async throws {
        try await loadHTML()

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBoundsInScreenCoordinates = try await screenBoundsOfText("crazy")

        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
        }

        await page.waitForNextPresentationUpdate()

        let start = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        let end = CGPoint(x: start.x, y: start.y + 200)

        await recap.play { composer in
            composer._wk_scroll(withStart: start, end: end, duration: .seconds(1))
        }

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(newSelection == crazySelection)
    }

    @Test(arguments: [true, false], [true, false])
    func doubleClickingInWordSelectsWord(contentEditable: Bool, clickHandler: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable, clickHandler: clickHandler)

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )

        let crazyBoundsInScreenCoordinates = try await screenBoundsOfText("crazy")

        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))

        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
        }

        await page.waitForNextPresentationUpdate()

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(newSelection == crazySelection)
    }

    @Test(
        .bug("https://webkit.org/b/314804", "Triple click does not generate a line selection on PDF"),
        arguments: [true, false]
    )
    func tripleClickingInPDFSelectsLine(useAlternatePDFHUD: Bool) async throws {
        page.setWebFeature("UseAlternatePDFHUD", enabled: useAlternatePDFHUD)

        let pdfURL = try #require(Bundle.testResources.url(forResource: "test", withExtension: "pdf"))
        try await page.load(pdfURL).wait()
        await page.waitForNextPresentationUpdate()

        let clickPoint = screenBounds(ofPointInWindowCoordinates: .init(x: 100, y: 350))

        await recap.play { composer in
            composer._wk_click(at: clickPoint, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: clickPoint, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: clickPoint, for: .seconds(0.1))
        }

        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        let selectedText = await page.copySelection()
        #expect(selectedText == "Test PDF Content")
    }

    @Test(arguments: [true, false])
    func clickingOnPDFHUDButtonPerformsAction(useAlternatePDFHUD: Bool) async throws {
        page.setWebFeature("UseAlternatePDFHUD", enabled: useAlternatePDFHUD)

        let pdfURL = try #require(Bundle.testResources.url(forResource: "test", withExtension: "pdf"))
        try await page.load(pdfURL).wait()
        await page.waitForNextPresentationUpdate()

        let hud = try #require(page.pdfHUDs.first?.subviews.first)
        let hudCenterInWindow = hud.convert(CGPoint(x: hud.bounds.midX, y: hud.bounds.midY), to: nil)
        let hudCenterInScreen = screenBounds(ofPointInWindowCoordinates: hudCenterInWindow)
        let zoomInButton = NSPoint(x: hudCenterInScreen.x - 20, y: hudCenterInScreen.y)

        let scaleBeforeZooming = page.pageZoom

        await recap.play { composer in
            composer._wk_click(at: zoomInButton, for: .seconds(0.1))
            composer.advanceTime(0.1)
        }

        let scaleAfterZooming = page.pageZoom

        #expect(scaleAfterZooming > scaleBeforeZooming)
    }

    @Test(arguments: [false])
    func clickingOnPDFShowsHUD(useAlternatePDFHUD: Bool) async throws {
        page.setWebFeature("UseAlternatePDFHUD", enabled: useAlternatePDFHUD)

        let pdfURL = try #require(Bundle.testResources.url(forResource: "test", withExtension: "pdf"))
        try await page.load(pdfURL).wait()
        await page.waitForNextPresentationUpdate()

        let clickPoint = screenBounds(ofPointInWindowCoordinates: .init(x: 100, y: 350))

        let hud = try #require(page.pdfHUDs.first)

        unsafe hud.perform(Selector(("_hideForTesting")))

        // Reach in to the view hierarchy and get the view whose opacity actually changes,
        // which is dependent on the implementation.
        // FIXME: Depending on implementation-specific details like this is very not great.

        let visibleView =
            if useAlternatePDFHUD {
                try #require(hud.subviews.first?.subviews.first)
            } else {
                try #require(hud.subviews.first)
            }

        try await Task.sleep(for: .seconds(1))
        #expect(visibleView.alphaValue == 0)

        await recap.play { composer in
            composer._wk_click(at: clickPoint, for: .seconds(0.1))
            composer.advanceTime(0.1)
        }

        try await Task.sleep(for: .seconds(1))
        #expect(visibleView.alphaValue == 1)
    }

    @Test(arguments: [0.1, 0.5, 0.9])
    func clickingInWordChangesSelection(fractionOfWordToClick: Double) async throws {
        try await loadHTML(contentEditable: true)

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))

        let crazyBoundsInScreenCoordinates = try await screenBoundsOfText("crazy")

        let point = CGPoint(
            x: crazyBoundsInScreenCoordinates.origin.x + (crazyBoundsInScreenCoordinates.size.width * fractionOfWordToClick),
            y: crazyBoundsInScreenCoordinates.midY
        )

        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", offset: 0))

        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: point, for: .seconds(0.1))
        }

        await page.waitForNextPresentationUpdate()

        // This is a rough approximation of the heuristic the implementation uses.
        let offset = fractionOfWordToClick < 0.2 ? crazyRange.lowerBound : crazyRange.upperBound

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())
        #expect(newSelection == .collapsed(.init(in: "div", at: offset)))
    }

    @Test
    func clickingChangesSelection() async throws {
        try await loadHTML(contentEditable: true)

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        try await page.callJavaScript(JavaScriptMessages.SetSelection(in: "div", range: crazyRange))

        let crazyBoundsInViewportCoordinates = try await CGRect(
            page.callJavaScript(JavaScriptMessages.BoundingClientRect(in: "div", range: crazyRange))
        )

        let contentHeight = try #require(window.contentViewController?.view.frame.height)

        let crazyBoundsInAppKitCoordinates = CGRect(
            x: crazyBoundsInViewportCoordinates.minX,
            y: contentHeight - crazyBoundsInViewportCoordinates.maxY,
            width: crazyBoundsInViewportCoordinates.width,
            height: crazyBoundsInViewportCoordinates.height,
        )

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

        page.click(at: crazyBoundsInAppKitCoordinates.center)

        let selection = try await newSelection
        let expected = "Here's to the cra".count
        #expect(selection == expected)
    }

    @Test(.disabled(), arguments: [6, 8], [Duration.seconds(0.1), .seconds(0.5), .seconds(1.0)])
    func scrollingOnScrollBarChangesScrollPosition(inset: Int, pressAndWait: Duration) async throws {
        let html = """
            <body style="width: 100%; height: 2000px; margin: 0; background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);">
            </body>
            """

        try await page.load(html: html).wait()

        let topOfScrollBarInWindowCoordinates = NSPoint(x: window.frame.maxX - CGFloat(inset), y: window.frame.maxY - 160)
        let start = screenBounds(ofPointInWindowCoordinates: topOfScrollBarInWindowCoordinates)
        let end = CGPoint(x: start.x, y: start.y + 200)

        await recap.play { composer in
            composer._wk_drag(withStart: start, end: end, duration: .seconds(1.0), pressAndWait: pressAndWait)
        }

        try await Task.sleep(for: .seconds(1))

        let finalScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(finalScrollPosition.x == 0)
        #expect(finalScrollPosition.y > 0)
    }

    @Test(arguments: [true, false])
    func scrollingChangesScrollPosition(scrollOnImage: Bool) async throws {
        let image = scrollOnImage ? #"<img id="img" src="400x400-green.png" style="display: block; margin: 50px;">"# : ""

        let html = """
            <body style="width: 100%; height: 2000px; background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);">
                \(image)
            </body>
            """

        let baseURL = try #require(Bundle.testResources.resourceURL)
        try await page.load(html: html, baseURL: baseURL).wait()

        await page.waitForNextPresentationUpdate()

        let initialScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(CGPoint(initialScrollPosition) == .zero)

        let start = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        let end = CGPoint(x: start.x, y: start.y - 200)

        await recap.play { composer in
            composer._wk_scroll(withStart: start, end: end, duration: .seconds(0.5))
        }

        try await Task.sleep(for: .seconds(1))

        let finalScrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(finalScrollPosition.x == 0)
        #expect(finalScrollPosition.y > 0)
    }

    @Test
    func scrollInputSourceUpdatesRubberbandHyperbolicCoefficient() async throws {
        let html = """
            <body style="margin: 0; width: 100%; height: 20000px;
                         background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);">
            </body>
            """
        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        // No gesture has run yet.
        #expect(page.rubberbandHyperbolicCoefficient() == 0)
        try await page.callJavaScript { "window.scrollTo(0, 0);" }
        #expect(page.rubberbandHyperbolicCoefficient() == 0)

        await page.waitForNextPresentationUpdate()

        let automationLocation = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        let trackpadLocation = window.frame.center

        func expectCoefficient(_ expected: CGFloat, _ message: Comment) {
            let actual = page.rubberbandHyperbolicCoefficient()
            #expect(abs(actual - Double(expected)) < 0.0001, message)
        }

        // 1. Trackpad scroll.
        page.scrollWheel(at: trackpadLocation, delta: CGSize(width: 0, height: 600))
        await page.waitForNextPresentationUpdate()
        // End the scroll by programatically scrolling back to 0,0.
        try await page.callJavaScript { "window.scrollTo(0, 0);" }

        expectCoefficient(trackpadRubberbandHyperbolicCoefficient, "after initial trackpad scroll")

        // 2. Automation scroll.
        await recap.play { composer in
            composer._wk_scroll(
                withStart: automationLocation,
                end: CGPoint(x: automationLocation.x, y: automationLocation.y + 200),
                duration: .seconds(0.5)
            )
        }
        await page.waitForNextPresentationUpdate()
        // End the scroll by programatically scrolling back to 0,0.
        try await page.callJavaScript { "window.scrollTo(0, 0);" }

        expectCoefficient(automationHyperbolicCoefficient, "after automated scroll")

        // 3. Trackpad scroll again.
        page.scrollWheel(at: trackpadLocation, delta: CGSize(width: 0, height: 600))
        await page.waitForNextPresentationUpdate()
        // End the scroll by programatically scrolling back to 0,0.
        try await page.callJavaScript { "window.scrollTo(0, 0);" }

        expectCoefficient(trackpadRubberbandHyperbolicCoefficient, "after returning to trackpad scroll")
    }

    @Test
    func interruptingDeceleratingScrollDoesNotFollowLink() async throws {
        let html = """
            <a id="link" href="about:blank"
               style="display: block; width: 100%; height: 5000px;
                      background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);">
            </a>
            """
        let initialURL = try #require(URL(string: "http://webkit.org/"))
        try await page.load(html: html, baseURL: initialURL).wait()
        await page.waitForNextPresentationUpdate()

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        let scrollEnd = CGPoint(x: center.x, y: center.y - 200)

        // Begin a momentum scroll, then catch it before it settles.
        // The interruption should not follow the link beneath it.
        await recap.play { composer in
            composer._wk_scroll(withStart: center, end: scrollEnd, duration: .seconds(0.1))
            composer.advanceTime(0.05)
            composer._wk_click(at: center, for: .seconds(0.05))
        }

        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        #expect(page.url == initialURL)
    }

    @Test
    func clickAfterScrollStillProducesClick() async throws {
        let html = """
            <body style="margin: 0;">
                <div id="content"
                     onclick="window.clicks = (window.clicks || 0) + 1;"
                     style="height: 4000px; font-size: 30px;
                            background: repeating-linear-gradient(to bottom, #eee 0 40px, #fff 40px 80px);">
                    scroll, then click
                </div>
            </body>
            """
        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)

        await recap.play { composer in
            composer._wk_drag(
                withStart: center,
                end: CGPoint(x: center.x, y: center.y - 200),
                duration: .seconds(0.5),
                release: false
            )
            composer.advanceTime(0.5)
            composer._wk_mouseUp()
        }
        await page.waitForNextPresentationUpdate()

        let scrollPosition = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())
        #expect(scrollPosition.y > 0)

        try await Task.sleep(for: .seconds(1.5))

        await recap.play { composer in
            composer._wk_click(at: center, for: .seconds(0.1))
        }
        await page.waitForPendingMouseEvents()
        await page.waitForNextPresentationUpdate()

        let clicks = try await page.callJavaScript(returning: Int.self) {
            "return window.clicks || 0;"
        }
        #expect(clicks >= 1)
    }

    @Test
    func clickingOnSelectControlsKeepsMenuOpen() async throws {
        let html = """
            <select name="pets" id="select">
                <option value="">--Please choose an option--</option>
                <option value="dog">Dog</option>
                <option value="cat">Cat</option>
                <option value="goose">Goose</option>
            </select>
            """

        try await page.load(html: html).wait()

        // This is a proxy for "keeping the menu open" since if there are two mousedown events,
        // then it toggles the menu back to being closed.

        try await page.callJavaScript {
            """
            window.mousedownCount = 0;
            window.clickCount = 0;

            document.addEventListener("mousedown", event => {
                window.mousedownCount++;
                event.preventDefault();
            }, { capture: true });

            document.addEventListener("click", () => {
                window.clickCount++;
            }, { capture: true });
            """
        }

        let bounds = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "select"))
        let convertedBounds = screenBounds(ofRectInViewportCoordinates: bounds)

        await page.waitForNextPresentationUpdate()

        await recap.play { composer in
            composer._wk_click(at: convertedBounds.center, for: .seconds(0.1))
        }

        // Hard-coded delay to give the test a chance for multiple events to happen, in the failure case.
        try await Task.sleep(for: .seconds(0.5))

        let (mouseDownCount, clickCount) = try await page.callJavaScript(returning: (Int, Int).self) {
            """
            return [window.mousedownCount, window.clickCount];
            """
        }
        #expect(mouseDownCount == 1)
        #expect(clickCount == 1)
    }

    @Test
    func clickingOtherContentDismissesDataListDropdown() async throws {
        let html = """
            <div id="other" style="height: 200px; font-size: 30px;">other content</div>
            <input id="input" list="pets" style="font-size: 30px;">
            <datalist id="pets">
                <option value="Dog">
                <option value="Cat">
                <option value="Goose">
            </datalist>
            """

        try await page.load(html: html).wait()
        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript {
            """
            window.blurCount = 0;
            document.getElementById("input").addEventListener("blur", () => window.blurCount++);
            """
        }

        let inputBounds = try await screenBounds(ofElementWithID: "input")
        let otherBounds = try await screenBounds(ofElementWithID: "other")

        await recap.play { composer in
            composer._wk_click(at: inputBounds.center, for: .seconds(0.1))
        }

        func isDataListDropdownShowing() -> Bool {
            guard let childWindows = window.childWindows else {
                return false
            }

            return childWindows.contains { NSStringFromClass(type(of: $0)) == "WKDataListSuggestionWindow" }
        }

        await page.waitForNextPresentationUpdate()
        #expect(isDataListDropdownShowing())

        await recap.play { composer in
            composer._wk_click(at: otherBounds.center, for: .seconds(0.1))
        }

        await page.waitForNextPresentationUpdate()
        #expect(!isDataListDropdownShowing())

        let (activeElementID, blurCount) = try await page.callJavaScript(returning: (String, Int).self) {
            """
            return [document.activeElement?.id ?? "", window.blurCount];
            """
        }

        #expect(activeElementID != "input")
        #expect(blurCount == 1)
    }

    // MARK: - Drag Press Disambiguation Tests

    @Test(arguments: [true, false])
    func pressDragOverRangeInputChangesInputValue(useNativeWidget: Bool) async throws {
        let maximumValue = 10
        let initialValue = maximumValue / 2

        let elementID = useNativeWidget ? "native-slider" : "custom-slider"

        let customHTML = try #require(Bundle.testResources.url(forResource: "custom-slider", withExtension: "html"))
        try await page.load(customHTML).wait()

        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript(
            arguments: ["elementID": "custom-slider", "interactive": false],
            script: styleAdjustmentForCustomWidgetScript
        )
        await page.waitForNextPresentationUpdate()

        let sliderBounds = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: elementID))
        let convertedSliderBounds = screenBounds(ofRectInViewportCoordinates: sliderBounds)

        let start = convertedSliderBounds.center
        let end = CGPoint(x: convertedSliderBounds.maxX, y: convertedSliderBounds.center.y)

        await recap.play { composer in
            composer._wk_drag(withStart: start, end: end, duration: .seconds(1.5), pressAndWait: .seconds(0.5))
        }

        await page.waitForNextPresentationUpdate()

        let finalSliderValue = try await page.callJavaScript(
            returning: Double.self,
            arguments: ["elementID": elementID, "useNativeWidget": useNativeWidget]
        ) {
            """
            if (useNativeWidget) {
                return Number(document.getElementById(elementID).value);
            } else {
                return Number(document.getElementById(elementID).getAttribute("aria-valuenow"));
            }
            """
        }

        #expect(finalSliderValue == Double(maximumValue))

        let eventLog = try await page.callJavaScript(returning: [Double].self, arguments: ["elementID": elementID]) {
            """
            return [...window.eventLog[elementID]];
            """
        }

        #expect(eventLog == (initialValue...maximumValue).map(Double.init))
    }

    @Test(
        .bug("rdar://176317069", "REGRESSION(312023@main): Text cannot be selected with press + drag gesture"),
        arguments: [true, false]
    )
    func pressDragOverTextCreatesSelection(contentEditable: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable)

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

        if contentEditable {
            // In editable text the gesture moves the insertion point rather
            // than creating a range selection. Verify the caret moved away
            // from the offset 0 we set above.
            guard case .collapsed(let position) = selection else {
                Issue.record("expected press-drag to leave a collapsed selection (caret) in editable text, got \(selection)")
                return
            }
            #expect(position.container == "div")
            #expect(position.offset > 0)
        } else {
            guard case .range = selection else {
                Issue.record("expected press-drag to create a range selection in non-editable text, got \(selection)")
                return
            }
        }
    }

    @Test(
        .bug("rdar://176317069", "REGRESSION(312023@main): Text cannot be selected with press + drag gesture")
    )
    func pressDragOnLinkInitiatesDragAndDrop() async throws {
        let html = """
            <a id="link" href="https://webkit.org" style="font-size: 30px; display: block;">WebKit Link</a>
            """
        try await page.load(html: html).wait()

        let linkRange = try #require("WebKit Link".utf16Range(of: "WebKit Link"))
        let linkBounds = try await {
            let viewportCoordinates = try await page.callJavaScript(
                JavaScriptMessages.BoundingClientRect(in: "link", range: linkRange)
            )
            return screenBounds(ofRectInViewportCoordinates: viewportCoordinates)
        }()

        let dragEnd = CGPoint(x: linkBounds.maxX + 50, y: linkBounds.midY)

        await withSwizzledDraggingSession {
            await recap.play { composer in
                composer._wk_drag(
                    withStart: linkBounds.center,
                    end: dragEnd,
                    duration: .seconds(1.5),
                    pressAndWait: .seconds(1.0)
                )
            }
        }

        await page.waitForNextPresentationUpdate()

        let selection = try await page.callJavaScript(JavaScriptMessages.GetSelection())
        #expect(selection == .none)
    }

    @Test(
        .bug("https://webkit.org/b/315155", "Gesture-driven drag-and-drop does not recognize <img> elements")
    )
    func pressDragOnImageInitiatesDragAndDrop() async throws {
        let baseURL = try #require(Bundle.testResources.resourceURL)
        let html = """
            <img id="img" src="400x400-green.png" style="display: block; margin: 50px;">
            """
        try await page.load(html: html, baseURL: baseURL).wait()

        let imgViewportBounds = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "img"))
        let imgBounds = screenBounds(ofRectInViewportCoordinates: imgViewportBounds)

        let dragEnd = CGPoint(x: imgBounds.maxX + 50, y: imgBounds.midY)

        await withSwizzledDraggingSession {
            await recap.play { composer in
                composer._wk_drag(
                    withStart: imgBounds.center,
                    end: dragEnd,
                    duration: .seconds(1.5),
                    pressAndWait: .seconds(1.0)
                )
            }
        }

        // The test succeeds if it does not timeout.
    }

    @Test(
        .bug("rdar://176317069", "REGRESSION(312023@main): Text cannot be selected with press + drag gesture")
    )
    func pressDragOnExistingSelectionDoesNotExtendSelection() async throws {
        try await loadHTML(contentEditable: false)

        let crazyRange = try #require(Self.text.utf16Range(of: "crazy"))
        let onesRange = try #require(Self.text.utf16Range(of: "ones"))
        let crazySelection = JavaScriptSelection.range(
            base: .init(in: "div", at: crazyRange.lowerBound),
            extent: .init(in: "div", at: crazyRange.upperBound)
        )
        try await page.callJavaScript(JavaScriptMessages.SetSelection(crazySelection))
        await page.waitForNextPresentationUpdate()

        let crazyBounds = try await screenBoundsOfText("crazy")
        let onesBounds = try await screenBoundsOfText("ones")

        let dragInitiated = Future()

        let implementation: @convention(block) (NSView, NSArray, NSGestureRecognizer, AnyObject) -> NSDraggingSession? = { _, _, _, _ in
            dragInitiated.signal()
            return NSDraggingSession()
        }

        await withSwizzledObjectiveCInstanceMethod(
            replacing: NSView.self,
            name: #selector(NSView.beginDraggingSession(items:gesture:source:)),
            with: implementation
        ) {
            await recap.play { composer in
                composer._wk_drag(
                    withStart: crazyBounds.center,
                    end: onesBounds.center,
                    duration: .seconds(1.5),
                    pressAndWait: .seconds(1.0)
                )
            }

            await dragInitiated.wait()
        }

        await page.waitForNextPresentationUpdate()

        let selection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(
            selection
                != .range(
                    base: .init(in: "div", at: crazyRange.lowerBound),
                    extent: .init(in: "div", at: onesRange.upperBound)
                )
        )
    }

    @Test
    func scrubbingVideoTimelineDoesNotTriggerBackNavigation() async throws {
        // Establish a back-forward history entry so that a "swipe back" gesture would have somewhere to navigate to.
        try await page.load(URL(string: "about:blank?1")).wait()

        let videoPlayerURL = try #require(Bundle.testResources.url(forResource: "playback-scrubber", withExtension: "html"))
        try await page.load(videoPlayerURL).wait()
        await page.waitForNextPresentationUpdate()

        let urlBeforeScrub = page.url
        #expect(page.backForwardList.backList.count == 1)

        let scrubberBounds = try await screenBounds(ofElementWithID: "scrubber")
        let thumbBounds = try await screenBounds(ofElementWithID: "playhead")
        let start = thumbBounds.center
        let end = CGPoint(x: scrubberBounds.maxX - 10, y: start.y)

        await recap.play { composer in
            composer._wk_scroll(withStart: start, end: end, duration: .seconds(0.5))
        }

        await page.waitForNextPresentationUpdate()

        // Allow any (incorrectly) triggered back navigation to occur before asserting it did not.
        try await Task.sleep(for: .seconds(1))

        #expect(page.url == urlBeforeScrub)
        #expect(page.backForwardList.backList.count == 1)

        let scrubbedValues = try await page.callJavaScript(returning: [Double].self) {
            "return window.scrubbedValues ?? [];"
        }
        let lastScrubbedValue = try #require(scrubbedValues.last)
        #expect(lastScrubbedValue > 0)
    }

    @Test(
        .bug("https://webkit.org/b/319256", "Trackpad swiping between spaces should not trigger back navigation")
    )
    func swipingBetweenSpacesShouldNotTriggerBackNavigation() async throws {
        // Establish a back-forward history entry so that a "swipe back" gesture would have somewhere to navigate to.
        try await page.load(URL(string: "about:blank?1")).wait()

        let testURL = try #require(Bundle.testResources.url(forResource: "red", withExtension: "html"))
        try await page.load(testURL).wait()
        await page.waitForNextPresentationUpdate()
        let urlBeforeGesture = page.url

        #expect(page.backForwardList.backList.count == 1)

        let start = screenBounds(ofPointInWindowCoordinates: CGPoint(x: window.frame.width / 4, y: window.frame.height / 2))
        let end = screenBounds(ofPointInWindowCoordinates: CGPoint(x: 3 * window.frame.width / 4, y: window.frame.height / 2))

        await recap.play { composer in
            composer._wk_scroll(withStart: start, end: end, duration: .seconds(0.5), multiFinger: true)
        }

        // Allow any (incorrectly) triggered back navigation to occur before asserting it did not.
        // FIXME: Switch over to `webViewDidBeginNavigationGesture` when we adopt it for positive swipe navigation tests.
        try await Task.sleep(for: .seconds(1))

        #expect(page.url == urlBeforeGesture)
        #expect(page.backForwardList.backList.count == 1)
    }

    @Test(arguments: [Duration.zero, .seconds(1)])
    func longPressAndDragOnImageSelectsEntireText(delay: Duration) async throws {
        let baseURL = try #require(Bundle.testResources.resourceURL)
        let html = """
            <img id="img" src="love-and-coffee.jpeg" style="display: block; height: 100vh; margin: 0">
            """
        try await page.load(html: html, baseURL: baseURL).wait()

        let imageViewportBounds = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "img"))
        let imageScreenBounds = screenBounds(ofRectInViewportCoordinates: imageViewportBounds)

        await page.waitForNextPresentationUpdate()

        let responseURL = try #require(Bundle.testResources.url(forResource: "love-and-coffee-analysis", withExtension: "json"))
        let analysis = try ImageAnalysisResult(parsing: responseURL)

        let firstWord = try #require(analysis.lines.first?.children.first?.quad)
        let lastWord = try #require(analysis.lines.last?.children.last?.quad)

        let start = firstWord.center.normalized(in: imageScreenBounds)
        let end = lastWord.center.normalized(in: imageScreenBounds)

        await withMockedImageAnalyzer(response: .success(analysis), after: delay) {
            await recap.play { composer in
                composer._wk_drag(withStart: start, end: end, duration: .seconds(2), pressAndWait: .seconds(1.0))
            }
        }

        let selection = try await page.callJavaScript(returning: String.self) {
            """
            return window.getSelection().toString();
            """
        }

        #expect(selection == "EVERYONE\nDESERVES\nLOVE AND\nCOFFEE")
    }

    @Test
    func pressDragOnImageWithoutTextInitiatesDragAndDrop() async throws {
        let baseURL = try #require(Bundle.testResources.resourceURL)
        let html = """
            <img id="img" src="400x400-green.png" style="display: block; height: 100vh; margin: 0">
            """
        try await page.load(html: html, baseURL: baseURL).wait()

        let imageViewportBounds = try await page.callJavaScript(JavaScriptMessages.BoundingClientRect(elementID: "img"))
        let imageScreenBounds = screenBounds(ofRectInViewportCoordinates: imageViewportBounds)

        await page.waitForNextPresentationUpdate()

        let dragEnd = CGPoint(x: imageScreenBounds.maxX + 50, y: imageScreenBounds.midY)

        await withMockedImageAnalyzer(response: .success(.init(lines: [])), after: .zero) {
            await withSwizzledDraggingSession {
                await recap.play { composer in
                    composer._wk_drag(
                        withStart: imageScreenBounds.center,
                        end: dragEnd,
                        duration: .seconds(1.5),
                        pressAndWait: .seconds(1.0)
                    )
                }
            }
        }

        // The test succeeds if it does not timeout.
    }

    @Test
    func consecutiveQuickFlicksAccelerateScrolling() async throws {
        let html = """
            <body style="margin: 0; width: 100%; height: 200000px;
                         background: repeating-linear-gradient(to bottom, blue 0 50px, white 50px 100px);">
            </body>
            """

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        let down = CGPoint(x: center.x, y: center.y - 250)
        let up = CGPoint(x: center.x, y: center.y + 250)

        func finalFlingDistance(flicks count: Int, reverseLast: Bool = false) async throws -> Double {
            try await page.load(html: html).wait()
            await page.waitForNextPresentationUpdate()

            await recap.play { composer in
                for i in 0..<count {
                    let isLast = i == count - 1
                    composer._wk_scroll(withStart: center, end: (isLast && reverseLast) ? up : down, duration: .seconds(0.08))
                    if !isLast {
                        composer.advanceTime(0.05)
                    }
                }
            }

            let flingStart = try await page.callJavaScript(JavaScriptMessages.ScrollPosition()).y
            let settled = try await settledScrollPosition()
            return abs(settled.y - flingStart)
        }

        let single = try await finalFlingDistance(flicks: 1) // count 1 -> multiplier 1
        let accelerated = try await finalFlingDistance(flicks: 6) // final count 5 -> ~5x
        let reversed = try await finalFlingDistance(flicks: 6, reverseLast: true) // reversal resets -> ~1x

        // Accelerated final fling travels far beyond an unaccelerated one (actual ratio ~5x).
        #expect(accelerated > single * 2)
        // Reversing the final swipe resets the chain, so that fling is not accelerated.
        #expect(accelerated > reversed * 2)
    }

    @Test
    func diagonalScrollMovesBothAxes() async throws {
        try await loadScrollableGrid()
        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript { "window.scrollTo(2000, 8000);" }
        await page.waitForNextPresentationUpdate()
        let start = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        await recap.play { composer in
            // ~45°: neither axis is locked out.
            composer._wk_scroll(
                withStart: center,
                end: CGPoint(x: center.x - 250, y: center.y - 250),
                duration: .seconds(0.3)
            )
        }
        await page.waitForNextPresentationUpdate()

        let end = try await settledScrollPosition()

        #expect(end.x - start.x > 20)
        #expect(end.y - start.y > 20)
    }

    @Test
    func shallowScrollLocksToHorizontalAxis() async throws {
        try await loadScrollableGrid()
        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript { "window.scrollTo(2000, 8000);" }
        await page.waitForNextPresentationUpdate()
        let start = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        await recap.play { composer in
            // ~7°: the vertical component is suppressed.
            composer._wk_scroll(
                withStart: center,
                end: CGPoint(x: center.x - 250, y: center.y - 30),
                duration: .seconds(0.3)
            )
        }
        await page.waitForNextPresentationUpdate()

        let end = try await settledScrollPosition()

        #expect(end.x - start.x > 20)
        #expect(abs(end.y - start.y) < 1)
    }

    @Test
    func steepScrollLocksToVerticalAxis() async throws {
        try await loadScrollableGrid()
        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript { "window.scrollTo(2000, 8000);" }
        await page.waitForNextPresentationUpdate()
        let start = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        await recap.play { composer in
            // ~7° from vertical: the horizontal component is suppressed.
            composer._wk_scroll(
                withStart: center,
                end: CGPoint(x: center.x - 30, y: center.y - 250),
                duration: .seconds(0.3)
            )
        }
        await page.waitForNextPresentationUpdate()

        let end = try await settledScrollPosition()

        #expect(end.y - start.y > 20)
        #expect(abs(end.x - start.x) < 1)
    }

    @Test
    func scrollCatchingMomentumCanChangeAxis() async throws {
        try await loadScrollableGrid()
        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript { "window.scrollTo(2000, 8000);" }
        await page.waitForNextPresentationUpdate()

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)

        await recap.play { composer in
            composer._wk_scroll(withStart: center, end: CGPoint(x: center.x - 250, y: center.y), duration: .seconds(0.08))
        }
        await page.waitForNextPresentationUpdate()

        let after = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())

        await recap.play { composer in
            composer._wk_scroll(withStart: center, end: CGPoint(x: center.x, y: center.y - 250), duration: .seconds(0.3))
        }
        await page.waitForNextPresentationUpdate()

        let end = try await settledScrollPosition()
        #expect(end.y - after.y > 20)
    }

    @Test
    func shallowScrollOnVerticalOnlyPageStillScrollsVertically() async throws {
        try await loadScrollableText()
        await page.waitForNextPresentationUpdate()

        try await page.callJavaScript { "window.scrollTo(0, 1000);" }
        await page.waitForNextPresentationUpdate()
        let start = try await page.callJavaScript(JavaScriptMessages.ScrollPosition())

        let center = screenBounds(ofPointInWindowCoordinates: window.frame.center)
        await recap.play { composer in
            // ~18°: shallow enough to be inside the horizontal lock band, but the page can't scroll
            // horizontally, so that branch is skipped. The drag is not steep enough for the vertical
            // branch either, so no lock is taken at all and both components flow — which is what lets
            // the vertical one scroll here.
            composer._wk_scroll(
                withStart: center,
                end: CGPoint(x: center.x - 250, y: center.y - 80),
                duration: .seconds(0.3)
            )
        }
        await page.waitForNextPresentationUpdate()

        let end = try await settledScrollPosition()
        #expect(end.y - start.y > 20)
    }
}

nonisolated(nonsending) private func withSwizzledContextMenu(perform body: () async -> Void) async {
    typealias CompletionHandler = @convention(block) () -> Void
    typealias ObjCImplementation = @convention(block) (NSMenu.Type, NSMenu, _NSViewMenuContext, NSView, CompletionHandler?) -> Void

    let future = Future()

    let implementation: ObjCImplementation = { _, _, _, _, completion in
        completion?()

        future.signal()
    }

    await withSwizzledObjectiveCClassMethod(
        class: NSMenu.self,
        replacing: #selector(NSMenu._popUpContextMenu(_:with:for:) as (NSMenu, _NSViewMenuContext, NSView) async -> Void),
        with: implementation
    ) {
        await body()

        await future.wait()
    }
}

nonisolated(nonsending) private func withSwizzledDraggingSession(perform body: () async -> Void) async {
    typealias ObjCImplementation = @convention(block) (NSView, NSArray, NSGestureRecognizer, AnyObject) -> NSDraggingSession?

    let dragInitiated = Future()

    let implementation: ObjCImplementation = { _, _, _, _ in
        dragInitiated.signal()
        return NSDraggingSession()
    }

    await withSwizzledObjectiveCInstanceMethod(
        replacing: NSView.self,
        name: #selector(NSView.beginDraggingSession(items:gesture:source:)),
        with: implementation
    ) {
        await body()

        await dragInitiated.wait()
    }
}

extension ImageAnalysisResult.Quad {
    fileprivate var center: CGPoint {
        let x = (topLeft.x + topRight.x + bottomLeft.x + bottomRight.x) / 4.0
        let y = (topLeft.y + topRight.y + bottomLeft.y + bottomRight.y) / 4.0
        return CGPoint(x: x, y: y)
    }
}

extension CGPoint {
    fileprivate func normalized(in rect: CGRect) -> CGPoint {
        CGPoint(x: rect.minX + x * rect.width, y: rect.minY + y * rect.height)
    }
}

extension AppKitGesturesTests.Basic {
    private func loadScrollableText() async throws {
        let lines = (0..<100)
            .reversed()
            .map { "<p id='line\($0)'>\($0) bottles of beer on the wall</p>" }
            .joined(separator: "\n")

        let html = """
            <div id="text" style="font-size: 60px; margin: 0;">\(lines)</div>
            """
        try await page.load(html: html).wait()
    }

    private func loadScrollableGrid() async throws {
        let html = """
            <body style="margin: 0; width: 5000px; height: 20000px;
                         background: repeating-linear-gradient(45deg, blue 0 50px, white 50px 100px);">
            </body>
            """
        try await page.load(html: html).wait()
    }

    private func settledScrollPosition() async throws -> CGPoint {
        func read() async throws -> CGPoint {
            try await CGPoint(page.callJavaScript(JavaScriptMessages.ScrollPosition()))
        }

        var previous = try await read()
        var stableSamples = 0

        for _ in 0..<60 {
            try await Task.sleep(for: .milliseconds(100))
            let current = try await read()

            if abs(current.x - previous.x) < 1, abs(current.y - previous.y) < 1 {
                stableSamples += 1
                if stableSamples >= 2 {
                    return current
                }
            } else {
                stableSamples = 0
            }

            previous = current
        }

        Issue.record("scroll position never settled; last sample was \(previous)")
        return previous
    }
}

#endif
