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

#if HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)

import Foundation
internal import WebKit_Internal
import AppKit
internal import WebCore_Private
private import CxxStdlib

@objc
@implementation
extension WKTextSelectionController {
    private unowned let view: WKWebView

    @nonobjc
    private var currentRangeSelectionGranularity: NSTextSelection.Granularity? = nil

    init(view: WKWebView) {
        self.view = view
        super.init()
    }

    func addTextSelectionManager() {
        guard let page = view._protectedPage().get() else {
            return
        }

        guard page.preferences().useAppKitGestures() else {
            return
        }

        Logger.viewGestures.log("Creating a text selection manager for view \(self.view)")

        let manager = NSTextSelectionManager()
        manager._webkitDelegate = self
        view.textSelectionManager = manager

        for case let gestureRecognizer as NSPressGestureRecognizer in manager.gesturesForFailureRequirements {
            gestureRecognizer.buttonMask = 0
        }
    }

    func selectionDidChange() {
        guard let page = view._protectedPage().get() else {
            return
        }

        let editorState = unsafe page.editorState
        view.textSelectionManager?.textSelectionMode =
            unsafe editorState.isContentEditable || editorState.isContentRichlyEditable ? .editable : .selectable
    }
}

@objc(NSTextSelectionManagerDelegate)
@implementation
extension WKTextSelectionController {
    var insertionCursorRect: NSRect {
        guard let page = view._protectedPage().get() else {
            return .zero
        }

        guard let visualData = unsafe Optional(fromCxx: page.editorState.visualData) else {
            return .zero
        }

        return unsafe CGRect(visualData.caretRectAtStart)
    }

    var selectionIsInsertionPoint: Bool {
        guard let page = view._protectedPage().get() else {
            return false
        }

        let editorState = unsafe page.editorState
        return unsafe editorState.selectionType == .Caret
    }

    @objc(isTextSelectedAtPoint:)
    func isTextSelected(at point: NSPoint) -> Bool {
        // The `point` location is relative to the view.

        guard let page = view._protectedPage().get() else {
            return false
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        let editorState = unsafe page.editorState
        let hasSelection = unsafe editorState.selectionType != .None

        if unsafe !hasSelection || !editorState.hasPostLayoutAndVisualData() {
            Logger.viewGestures.log(
                "[pageProxyID=\(page.logIdentifier())] Editor state has no selection, post layout data, or visual data"
            )
            return false
        }

        let isRange = unsafe editorState.selectionType == .Range
        let isContentEditable = unsafe editorState.isContentEditable

        if !isContentEditable && !isRange {
            Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Selection is neither contenteditable nor a range")
            return false
        }

        // FIXME: If the state's selection is not a range, is the number of selection geometries always zero?
        // If so, then the rest of the logic in this function can be elided in that case.

        var selectionRects: [WKTextSelectionRect] = []
        let selectionGeometries = unsafe editorState.visualData.pointee.selectionGeometries

        // FIXME: `WTF::Vector` should be able to be used as a Swift `Sequence`.
        for i in unsafe 0..<selectionGeometries.size() {
            let selectionGeometry = unsafe selectionGeometries.__atUnsafe(i).pointee
            selectionRects.append(.init(selectionGeometry: selectionGeometry, delegate: nil))
        }

        let result = selectionRects.contains { $0.rect.contains(point) }
        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] Text is selected => \(result)")

        return result
    }

    @objc(moveInsertionCursorToPoint:placeAtWordBoundary:completionHandler:)
    func moveInsertionCursor(to point: NSPoint, placeAtWordBoundary: Bool) async -> Bool {
        // A return value of `true` indicates the selection has changed.

        guard let page = view._protectedPage().get() else {
            return false
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        let previousState = unsafe page.editorState
        let previousVisualData = unsafe Optional(fromCxx: previousState.visualData)

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

        let newState = unsafe page.editorState
        let newVisualData = unsafe Optional(fromCxx: newState.visualData)

        guard let previousVisualData = unsafe previousVisualData, let newVisualData = unsafe newVisualData else {
            return false
        }

        // FIXME: (rdar://170847912) Use the `!=` operator instead when possible.
        return unsafe !(previousVisualData.caretRectAtStart == newVisualData.caretRectAtStart)
    }

    @objc(showContextMenuAtPoint:)
    func showContextMenu(at point: NSPoint) {
        // The `point` location is relative to the window.

        guard let page = view._protectedPage().get(), let impl = unsafe view._impl() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        let timestamp = GetCurrentEventTime()
        let windowNumber = unsafe impl.windowNumber()

        let mouseDown = NSEvent.mouseEvent(
            with: .rightMouseDown,
            location: point,
            modifierFlags: [],
            timestamp: timestamp,
            windowNumber: windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: 1,
            pressure: 1
        )
        let mouseUp = NSEvent.mouseEvent(
            with: .rightMouseUp,
            location: point,
            modifierFlags: [],
            timestamp: timestamp,
            windowNumber: windowNumber,
            context: nil,
            eventNumber: 0,
            clickCount: 1,
            pressure: 0
        )

        unsafe impl.mouseDown(mouseDown, .Automation)
        unsafe impl.mouseUp(mouseUp, .Automation)
    }

    @objc(dragSelectionWithGesture:completionHandler:)
    func dragSelection(withGesture gesture: NSGestureRecognizer, completionHandler: @escaping @Sendable (NSDraggingSession) -> Void) {
        guard let page = view._protectedPage().get() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) gesture: \(String(reflecting: gesture))")
    }

    @objc(beginRangeSelectionAtPoint:withGranularity:)
    func beginRangeSelection(at point: NSPoint, with granularity: NSTextSelection.Granularity) {
        guard let page = view._protectedPage().get() else {
            return
        }

        Logger.viewGestures.log(
            "[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point)) granularity: \(String(reflecting: granularity))"
        )

        currentRangeSelectionGranularity = granularity

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
        guard let page = view._protectedPage().get() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

        guard let currentRangeSelectionGranularity else {
            assertionFailure("continueRangeSelection was called with a nil currentRangeSelectionGranularity")
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

    @objc(endRangeSelectionAtPoint:)
    func endRangeSelection(at point: NSPoint) {
        guard let page = view._protectedPage().get() else {
            return
        }

        Logger.viewGestures.log("[pageProxyID=\(page.logIdentifier())] \(#function) point: \(String(reflecting: point))")

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

#endif // HAVE_APPKIT_GESTURES_SUPPORT && compiler(>=6.2)
