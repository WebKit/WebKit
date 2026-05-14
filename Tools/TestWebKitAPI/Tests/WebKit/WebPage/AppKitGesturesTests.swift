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
@_spi(WebKitAdditions_Testing) @_spi(Testing) @_spi(CrossImportOverlay) import WebKit
import SwiftUI
import struct Swift.String
import struct _Concurrency.Task
private import struct TestWebKitAPILibrary.DOMRect
import Testing
private import TestWebKitAPILibrary
private import Recap
private import AppKit_Private.NSMenu_Private

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
@Suite(.serialized)
struct AppKitGesturesTests {
    private static let text = "Here's to the crazy ones."

    private let recap = Recap.shared

    private let page: WebPage = {
        var configuration = WebPage.Configuration()
        configuration.requiresUserActionForEditingControlsManager = true
        return WebPage(configuration: configuration)
    }()

    private let window: NSWindow

    init() async throws {
        let contentSize = NSSize(width: 800, height: 600)

        self.window = NSWindow(size: contentSize) { [page] in
            WebView(page)
        }

        self.window.setFrameOrigin(.zero)
        NSApp.activate(ignoringOtherApps: true)
        self.window.makeKeyAndOrderFront(nil)
    }

    @Test(arguments: [true, false])
    func updatingTextRangeSelectionByUserInteractionUpdatesEditorState(contentEditable: Bool) async throws {
        try await loadHTML(contentEditable: contentEditable)

        let toBounds = try await screenBoundsOfText("to")
        let crazyBounds = try await screenBoundsOfText("crazy")

        let editorStateSnapshots = page.editorStateSnapshots()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

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

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        let future = Future()

        let implementation: @convention(block) (NSMenu.Type, NSMenu, _NSViewMenuContext, NSView, (@convention(block) () -> Void)?) -> Void =
            { _, menu, context, view, completion in
                completion?()

                #expect(view is WKWebView)

                future.signal()
            }

        await withSwizzledObjectiveCClassMethod(
            class: NSMenu.self,
            replacing: #selector(NSMenu._popUpContextMenu(_:with:for:) as (NSMenu, _NSViewMenuContext, NSView) async -> Void),
            with: implementation
        ) {
            await recap.play { composer in
                composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
            }

            await future.wait()
        }

        await page.waitForNextPresentationUpdate()

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

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        await recap.play { composer in
            composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
            composer.advanceTime(0.1)
            composer._wk_click(at: crazyBoundsInScreenCoordinates.center, for: .seconds(0.1))
        }

        await page.waitForNextPresentationUpdate()

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())

        #expect(newSelection == crazySelection)
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

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        await recap.play { composer in
            composer._wk_click(at: point, for: .seconds(0.1))
        }

        await page.waitForNextPresentationUpdate()

        // This is a rough approximation of the heuristic the implementation uses.
        let offset = fractionOfWordToClick < 0.2 ? crazyRange.lowerBound : crazyRange.upperBound

        let newSelection = try await page.callJavaScript(JavaScriptMessages.GetSelection())
        #expect(newSelection == .collapsed(.init(in: "div", at: offset)))
    }

    @Test(.bug("rdar://170502266"))
    func scrollOverNonScrollableWebViewPropagatesToEnclosingScrollView() async throws {
        let windowSize = NSSize(width: 800, height: 600)
        let documentHeight: CGFloat = 2000
        let page = WebPage()
        let webView = page.backingWebView
        webView.frame = NSRect(x: 0, y: 0, width: windowSize.width, height: windowSize.height)

        let documentView = FlippedDocumentView(
            frame: NSRect(x: 0, y: 0, width: windowSize.width, height: documentHeight)
        )
        documentView.addSubview(webView)

        let scrollView = NSScrollView(frame: NSRect(origin: .zero, size: windowSize))
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = false
        scrollView.documentView = documentView

        let scrollViewController = NSViewController()
        scrollViewController.view = scrollView
        let scrollWindow = NSWindow(contentViewController: scrollViewController)
        scrollWindow.setContentSize(windowSize)
        scrollWindow.setFrameOrigin(.zero)
        NSApp.activate(ignoringOtherApps: true)
        scrollWindow.makeKeyAndOrderFront(nil)

        try await page.load(html: "<body style='margin:0'>not scrollable</body>").wait()
        await page.waitForNextPresentationUpdate()

        // Recap requires this test to be ran within an app host.
        guard NSApp.isActive else {
            return
        }

        let viewportInDOMCoords = try #require(
            DOMRect(decodedRepresentation: [
                "x": 0.0,
                "y": 0.0,
                "width": Double(webView.frame.width),
                "height": Double(webView.frame.height),
            ])
        )
        let viewportInCGScreen = convertToCoreGraphicsScreenCoordinates(
            rectInViewportCoordinates: viewportInDOMCoords,
            window: scrollWindow
        )

        let dragDistance: CGFloat = 100
        let center = viewportInCGScreen.center
        let startPoint = CGPoint(x: center.x, y: center.y + dragDistance / 2)
        let endPoint = CGPoint(x: center.x, y: center.y - dragDistance / 2)

        let initialDocumentVisibleRect = scrollView.documentVisibleRect

        await recap.play { composer in
            composer.drag(withStart: startPoint, end: endPoint, duration: 0.5)
            composer.advanceTime(0.1)
            composer._wk_mouseUp()
        }

        await page.waitForNextPresentationUpdate()

        // The test passes if the outer scroller scrolled.
        #expect(scrollView.documentVisibleRect.origin != initialDocumentVisibleRect.origin)
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
}

// MARK: Helpers

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

extension AppKitGesturesTests {
    private func loadHTML(contentEditable: Bool = false, clickHandler: Bool = false) async throws {
        let contentEditableMarkup = contentEditable ? "contenteditable" : ""
        let clickHandlerMarkup = clickHandler ? "onclick='void(0)'" : ""

        let html = """
            <div \(contentEditableMarkup) \(clickHandlerMarkup) id="div" style="font-size: 30px;">\(Self.text)</div>
            """

        try await page.load(html: html).wait()
    }

    private func screenBoundsOfText(_ text: String) async throws -> CGRect {
        let range = try #require(Self.text.utf16Range(of: text))

        let viewportCoordinates = try await page.callJavaScript(
            JavaScriptMessages.BoundingClientRect(in: "div", range: range)
        )

        let screenCoordinates = convertToCoreGraphicsScreenCoordinates(
            rectInViewportCoordinates: viewportCoordinates,
            window: window
        )

        return screenCoordinates
    }
}

private final class FlippedDocumentView: NSView {
    override var isFlipped: Bool { true }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
