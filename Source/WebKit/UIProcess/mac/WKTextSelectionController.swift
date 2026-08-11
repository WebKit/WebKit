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
import WebKit_Internal
import AppKit
import WebCore_Private
private import CxxStdlib

@objc
@implementation
extension WKTextSelectionController {
    private weak var view: WKWebView?

    @nonobjc
    private var currentRangeSelectionGranularity: NSTextSelection.Granularity? = nil

    // The most recent extent point of an ongoing range-selection drag, in view coordinates. Used to
    // re-extend the selection as the page autoscrolls while the gesture holds near the window edge.
    @nonobjc
    private var lastRangeSelectionExtentPoint: NSPoint? = nil

    init(view: WKWebView) {
        self.view = view
        super.init()
    }

    func addTextSelectionManager() {
        guard let view, let page = view._protectedPage().get() else {
            return
        }

        guard page.preferences().useAppKitGestures() else {
            return
        }

        Logger.viewGestures.log("Creating a text selection manager for view \(view)")

        let manager = NSTextSelectionManager()
        manager._webkitDelegate = self
        view.textSelectionManager = manager

        for case let gestureRecognizer as NSPressGestureRecognizer in manager.gesturesForFailureRequirements {
            gestureRecognizer.buttonMask = 0
        }
    }

    func selectionDidChange() {
        guard let view, let page = view._protectedPage().get() else {
            return
        }

        let editorState = page.editorState
        view.textSelectionManager?.textSelectionMode =
            editorState.isContentEditable || editorState.isContentRichlyEditable ? .editable : .selectable
    }

    // Re-extends the selection toward the last drag point as the page autoscrolls. Invoked when the
    // page scrolls so the selection keeps growing into the newly revealed content, mirroring the iOS
    // `-updateSelection` re-extension. The web process decides when to start/stop autoscrolling (see
    // `updateSelectionWithExtentPointAndBoundary` in WebPageCocoa.mm); here we only need to know a
    // range-selection drag is in flight, which `lastRangeSelectionExtentPoint` tracks.
    func reextendSelectionForAutoscrollIfNeeded() {
        guard let page = view?._protectedPage().get() else {
            return
        }

        guard let currentRangeSelectionGranularity, let point = lastRangeSelectionExtentPoint else {
            return
        }

        Task.immediate {
            await page.updateSelection(
                withExtentPoint: WebCore.IntPoint(point),
                by: .init(currentRangeSelectionGranularity),
                isInteractingWithFocusedElement: true, // FIXME: Properly handle the case where this isn't actually true.
                source: .Mouse
            )
        }
    }
}

@objc(NSTextSelectionManagerDelegate)
@implementation
extension WKTextSelectionController {
    var insertionCursorRect: NSRect {
        guard let view, let page = view._protectedPage().get() else {
            return .zero
        }

        guard let visualData = Optional(fromCxx: page.editorState.visualData) else {
            return .zero
        }

        return CGRect(visualData.caretRectAtStart)
    }

    var selectionIsInsertionPoint: Bool {
        guard let view, let page = view._protectedPage().get() else {
            return false
        }

        let editorState = page.editorState
        return editorState.selectionType == .Caret
    }

    @objc(isTextSelectedAtPoint:)
    func isTextSelected(at point: NSPoint) -> Bool {
        // The `point` location is relative to the view.

        guard let view, let page = view._protectedPage().get() else {
            return false
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        let editorState = page.editorState

        guard editorState.selectionType == .Range else {
            Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Selection is not a range")
            return false
        }

        guard let visualData = Optional(fromCxx: editorState.visualData), Optional(fromCxx: editorState.postLayoutData) != nil else {
            Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Editor state has no post layout data or visual data")
            return false
        }

        let selectionGeometries = Array(visualData.selectionGeometries)

        let result = selectionGeometries.contains {
            let selectionRect = WKTextSelectionRect(selectionGeometry: $0, delegate: nil)
            return selectionRect.rect.contains(point)
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Text is selected => \(result)")

        return result
    }

    @objc(moveInsertionCursorToPoint:placeAtWordBoundary:completionHandler:)
    func moveInsertionCursor(to point: NSPoint, placeAtWordBoundary: Bool) async -> Bool {
        // A return value of `true` indicates the selection has changed.

        guard let view, let page = view._protectedPage().get() else {
            return false
        }

        Logger.viewGestures.log(
            "[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point)) placeAtWordBoundary: \(placeAtWordBoundary)"
        )

        let previousState = page.editorState
        let previousVisualData = Optional(fromCxx: previousState.visualData)

        // FIXME: Properly handle the case where this isn't actually true.
        let isInteractingWithFocusedElement = true

        if placeAtWordBoundary {
            await page.selectWithGesture(
                at: WebCore.IntPoint(point),
                type: .OneFingerTap,
                state: .Ended,
                isInteractingWithFocusedElement: isInteractingWithFocusedElement,
            )
        } else {
            await page.selectPosition(
                at: WebCore.IntPoint(point),
                isInteractingWithFocusedElement: isInteractingWithFocusedElement,
            )
        }

        let newState = page.editorState
        let newVisualData = Optional(fromCxx: newState.visualData)

        return switch (previousVisualData, newVisualData) {
        case (let previousVisualData?, let newVisualData?):
            // FIXME: (rdar://170847912) Use the `!=` operator instead when possible.
            !(previousVisualData.caretRectAtStart == newVisualData.caretRectAtStart)
        case (nil, nil):
            false
        default:
            true
        }
    }

    @objc(showContextMenuAtPoint:)
    func showContextMenu(at point: NSPoint) {
        // The `point` location is relative to the window.

        guard let view, let page = view._protectedPage().get(), let impl = view._impl() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        let windowNumber = impl.windowNumber()

        guard
            let mouseDown = NSEvent.syntheticMouseEvent(.rightMouseDown, location: point, windowNumber: windowNumber, pressure: 1),
            let mouseUp = NSEvent.syntheticMouseEvent(.rightMouseUp, location: point, windowNumber: windowNumber, pressure: 0)
        else {
            assertionFailure("NSEvent.mouseEvent(with:...) returned nil for context-menu synthesis")
            return
        }

        impl.mouseDown(mouseDown, .Automation)
        impl.mouseUp(mouseUp, .Automation)
    }

    @objc(dragSelectionWithGesture:completionHandler:)
    func dragSelection(withGesture gesture: NSGestureRecognizer, completionHandler: @escaping @Sendable (NSDraggingSession) -> Void) {
        guard let view, let page = view._protectedPage().get(), let impl = view._impl() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) gesture: \(String(reflecting: gesture))")

        let locationInWindow = gesture.location(in: nil)
        let windowNumber = impl.windowNumber()
        let modifierFlags = gesture.modifierFlags

        let mouseDown = NSEvent.syntheticMouseEvent(
            .leftMouseDown,
            location: locationInWindow,
            modifierFlags: modifierFlags,
            windowNumber: windowNumber,
            pressure: 1
        )
        let mouseDragged = NSEvent.syntheticMouseEvent(
            .leftMouseDragged,
            location: locationInWindow,
            modifierFlags: modifierFlags,
            windowNumber: windowNumber,
            pressure: 1
        )

        guard let mouseDown, let mouseDragged else {
            assertionFailure("NSEvent.mouseEvent(with:...) returned nil for drag-selection synthesis")
            return
        }

        impl.setTextSelectionDragGesture(gesture) { session in
            guard let session else { return }
            completionHandler(session)
        }

        impl.mouseDown(mouseDown, .Automation, .Yes)
        impl.mouseDragged(mouseDragged, .Automation, .Yes)

        gesture.addTarget(self, action: #selector(textSelectionDragGestureUpdated(_:)))
    }

    @objc
    private func textSelectionDragGestureUpdated(_ gesture: NSGestureRecognizer) {
        guard let view, let impl = view._impl() else {
            gesture.removeTarget(self, action: #selector(textSelectionDragGestureUpdated(_:)))
            return
        }

        let locationInWindow = gesture.location(in: nil)
        let windowNumber = impl.windowNumber()
        let modifierFlags = gesture.modifierFlags

        switch gesture.state {
        case .changed:
            guard
                let mouseDragged = NSEvent.syntheticMouseEvent(
                    .leftMouseDragged,
                    location: locationInWindow,
                    modifierFlags: modifierFlags,
                    windowNumber: windowNumber,
                    pressure: 1
                )
            else {
                assertionFailure("NSEvent.mouseEvent(with:...) returned nil for drag-update synthesis")
                return
            }

            impl.mouseDragged(mouseDragged, .Automation, .Yes)

        case .ended, .cancelled, .failed:
            guard
                let mouseUp = NSEvent.syntheticMouseEvent(
                    .leftMouseUp,
                    location: locationInWindow,
                    modifierFlags: modifierFlags,
                    windowNumber: windowNumber,
                    pressure: 0
                )
            else {
                assertionFailure("NSEvent.mouseEvent(with:...) returned nil for drag-end synthesis")
                break
            }

            impl.mouseUp(mouseUp, .Automation, .Yes)
            gesture.removeTarget(self, action: #selector(textSelectionDragGestureUpdated(_:)))

        default:
            break
        }
    }

    @objc(beginRangeSelectionAtPoint:withGranularity:)
    func beginRangeSelection(at point: NSPoint, with granularity: NSTextSelection.Granularity) {
        guard let view, let page = view._protectedPage().get(), let impl = view._impl() else {
            return
        }

        Logger.viewGestures.log(
            "[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point)) granularity: \(String(reflecting: granularity))"
        )

        currentRangeSelectionGranularity = granularity

        // Start each gesture from a clean slate: if a previous gesture ended abnormally (no
        // endRangeSelection), a stale extent point or live autoscroll could otherwise leak into this one.
        lastRangeSelectionExtentPoint = nil
        page.cancelAutoscroll()

        impl.beginSuppressingSingleClickGestureForTextSelection()

        Task.immediate {
            await page.selectText(
                at: WebCore.IntPoint(point),
                by: .init(granularity),
                isInteractingWithFocusedElement: true // FIXME: Properly handle the case where this isn't actually true.
            )
        }
    }

    @objc(continueRangeSelectionAtPoint:)
    func continueRangeSelection(at point: NSPoint) {
        guard let page = view?._protectedPage().get() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        guard let currentRangeSelectionGranularity else {
            assertionFailure("continueRangeSelection was called with a nil currentRangeSelectionGranularity")
            return
        }

        lastRangeSelectionExtentPoint = point

        Task.immediate {
            await page.updateSelection(
                withExtentPoint: WebCore.IntPoint(point),
                by: .init(currentRangeSelectionGranularity),
                isInteractingWithFocusedElement: true, // FIXME: Properly handle the case where this isn't actually true.
                source: .Mouse
            )
        }
    }

    @objc(endRangeSelectionAtPoint:)
    func endRangeSelection(at point: NSPoint) {
        guard let view, let page = view._protectedPage().get(), let impl = view._impl() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        impl.endSuppressingSingleClickGestureForTextSelection()

        page.cancelAutoscroll()
        lastRangeSelectionExtentPoint = nil

        guard currentRangeSelectionGranularity != nil else {
            assertionFailure("endRangeSelection was called with a nil currentRangeSelectionGranularity")
            return
        }

        currentRangeSelectionGranularity = nil
    }
}

extension WebCore.TextGranularity {
    fileprivate init(_ value: NSTextSelection.Granularity) {
        self =
            switch value {
            case .character: .CharacterGranularity
            case .word: .WordGranularity
            case .line: .LineGranularity
            case .sentence: .SentenceGranularity
            case .paragraph: .ParagraphGranularity
            @unknown default: .CharacterGranularity
            }
    }
}

extension NSEvent {
    fileprivate static func syntheticMouseEvent(
        _ type: NSEvent.EventType,
        location: NSPoint,
        modifierFlags: NSEvent.ModifierFlags = [],
        windowNumber: Int,
        pressure: Float
    ) -> NSEvent? {
        NSEvent.mouseEvent(
            with: type,
            location: location,
            modifierFlags: modifierFlags,
            timestamp: GetCurrentEventTime(),
            windowNumber: windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: 1,
            pressure: pressure
        )
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
