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
private import CxxStdlib
private import WebCore_Private
import struct Swift.String

extension WKAppKitGestureController {
    private func positionInformationRequestIsValid(at locationInViewCoordinates: NSPoint, radius: Int) -> Bool {
        let request = WebKit.InteractionInformationRequest(.init(locationInViewCoordinates))
        return hasValidPositionInformation && positionInformation.request.isApproximatelyValidForRequest(request, Int32(radius))
    }

    private static func representsDraggableElement(_ info: WebKit.InteractionInformationAtPosition) -> Bool {
        info.isLink
            || info.isImage
            || info.isAttachment
            || info.isDHTMLDraggable
            || info.isColorInput
            || info.prefersDraggingOverTextSelection
    }

    fileprivate func secondaryClickShouldBegin(at locationInViewCoordinates: NSPoint) -> Bool {
        let request = WebKit.InteractionInformationRequest(.init(locationInViewCoordinates))

        let requestIsValid = hasValidPositionInformation && positionInformation.request.isValidForRequest(request)
        let isSelectable = positionInformation.isSelectable()
        let isOverSelectableText = positionInformation.isOverSelectableText

        // The secondary click owns selectable points that are not over actual text (e.g. the page
        // background). Over a run of selectable text, the text selection manager should win so that a
        // long press selects a word instead of synthesizing a context menu.
        let shouldBegin = requestIsValid && isSelectable && !isOverSelectableText

        if !requestIsValid {
            _invalidateCurrentPositionInformation()
        }

        return shouldBegin
    }

    fileprivate func dragPressShouldBegin(at locationInViewCoordinates: NSPoint) -> Bool {
        guard let dragPressGestureRecognizer else {
            preconditionFailure("precondition violated: dragPressGestureRecognizer has not been created yet.")
        }

        let radius = Int(dragPressGestureRecognizer.allowableMovement.rounded(.up))

        // FIXME: Migrate to requestDragStart: IPC for an authoritative decision.
        // The heuristic below approximates DragController::draggableElement() by consulting the same element-type and style signals.
        let isDraggable = Self.representsDraggableElement(positionInformation)
        let requestIsValid = positionInformationRequestIsValid(at: locationInViewCoordinates, radius: radius)
        let shouldDrag = requestIsValid && isDraggable

        Logger.viewGestures.log(
            "\(#function) Drag-press shouldBegin -> \(shouldDrag) (hasInfo=\(self.hasValidPositionInformation) link=\(self.positionInformation.isLink) image=\(self.positionInformation.isImage) attachment=\(self.positionInformation.isAttachment) dhtml=\(self.positionInformation.isDHTMLDraggable) color=\(self.positionInformation.isColorInput) prefersDrag=\(self.positionInformation.prefersDraggingOverTextSelection) radius=\(radius))"
        )

        if !requestIsValid {
            _invalidateCurrentPositionInformation()
        }

        return shouldDrag
    }

    fileprivate func panShouldBegin(at locationInViewCoordinates: NSPoint) -> Bool {
        let panPositionInformationToleranceRadius = 15
        let requestIsValid = positionInformationRequestIsValid(at: locationInViewCoordinates, radius: panPositionInformationToleranceRadius)

        // FIXME: (rdar://181964604) Because of this logic, vertically scrolling over these elements likely will not work.
        let prefersInteraction = positionInformation.isRangeInput || positionInformation.isARIASlider
        let yieldToContent = requestIsValid && prefersInteraction

        Logger.viewGestures.log(
            "\(#function) Pan shouldBegin -> \(!yieldToContent) (hasInfo=\(self.hasValidPositionInformation) valid=\(requestIsValid) prefersInteraction=\(prefersInteraction))"
        )

        return !yieldToContent
    }

    fileprivate func isScrollOrZoomGestureRecognizer(_ gesture: NSGestureRecognizer) -> Bool {
        gesture == panGestureRecognizer || gesture.isBuiltInScrollViewPan || gesture is NSMagnificationGestureRecognizer
    }
}

@objc(NSGestureRecognizerDelegate)
@implementation
extension WKAppKitGestureController {
    @objc(gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:)
    func gestureRecognizer(
        _ gestureRecognizer: NSGestureRecognizer,
        shouldRecognizeSimultaneouslyWith otherGestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        Logger.viewGestures.debug(
            "\(#function) Gesture: \(Self.loggingDescription(for: gestureRecognizer)), Other gesture: \(Self.loggingDescription(for: otherGestureRecognizer))"
        )

        if isSamePair(gestureRecognizer, otherGestureRecognizer, singleClickGestureRecognizer, panGestureRecognizer) {
            return true
        }

        if gestureRecognizer is WKDeferringGestureRecognizer || otherGestureRecognizer is WKDeferringGestureRecognizer {
            return true
        }

        if isSamePair(gestureRecognizer, otherGestureRecognizer, mouseTrackingGestureRecognizer, singleClickGestureRecognizer) {
            return true
        }

        if isSamePair(gestureRecognizer, otherGestureRecognizer, dragPressGestureRecognizer, singleClickGestureRecognizer) {
            return true
        }

        if isSamePair(gestureRecognizer, otherGestureRecognizer, dragPressGestureRecognizer, mouseTrackingGestureRecognizer) {
            return true
        }

        if gestureRecognizer == singleClickGestureRecognizer
            && otherGestureRecognizer.isBuiltInScrollViewPan
            && otherGestureRecognizer.view is NSScrollView
        {
            return true
        }

        guard let webView else {
            return false
        }

        // Allow the single click or mouse tracking GRs to be simultaneously
        // recognized with any of those from the text selection manager.
        for gestureForFailureRequirements in webView.textSelectionManager?.gesturesForFailureRequirements ?? [] {
            if isSamePair(gestureRecognizer, otherGestureRecognizer, singleClickGestureRecognizer, gestureForFailureRequirements) {
                return true
            }

            if isSamePair(gestureRecognizer, otherGestureRecognizer, mouseTrackingGestureRecognizer, gestureForFailureRequirements) {
                return true
            }
        }

        return false
    }

    @objc(gestureRecognizer:shouldBeRequiredToFailByGestureRecognizer:)
    func gestureRecognizer(
        _ gestureRecognizer: NSGestureRecognizer,
        shouldBeRequiredToFailBy otherGestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        Logger.viewGestures.debug(
            "\(#function) Gesture: \(Self.loggingDescription(for: gestureRecognizer)), Other gesture: \(Self.loggingDescription(for: otherGestureRecognizer))"
        )

        guard let webView else {
            return false
        }

        if let deferringGestureRecognizer = gestureRecognizer as? WKDeferringGestureRecognizer {
            return deferringGestureRecognizer.shouldDefer(otherGestureRecognizer)
        }

        // Fail any gestures from the text selection manager if the secondary click GR handles them.
        let failForTextSelection = webView.textSelectionManager?.gesturesForFailureRequirements
            .contains { gestureForFailureRequirements in
                gestureRecognizer === secondaryClickGestureRecognizer && otherGestureRecognizer === gestureForFailureRequirements
            }

        if let failForTextSelection, failForTextSelection {
            return true
        }

        return false
    }

    @objc(gestureRecognizer:shouldRequireFailureOfGestureRecognizer:)
    func gestureRecognizer(
        _ gestureRecognizer: NSGestureRecognizer,
        shouldRequireFailureOf otherGestureRecognizer: NSGestureRecognizer
    ) -> Bool {
        Logger.viewGestures.debug(
            "\(#function) Gesture: \(Self.loggingDescription(for: gestureRecognizer)), Other gesture: \(Self.loggingDescription(for: otherGestureRecognizer))"
        )

        guard webView != nil else {
            return false
        }

        if gestureRecognizer === singleClickGestureRecognizer && otherGestureRecognizer === doubleClickGestureRecognizer {
            return true
        }

        if gestureRecognizer === mouseTrackingGestureRecognizer && otherGestureRecognizer === panGestureRecognizer {
            let panCanScroll = panGestureRecognizerCanScroll()
            Logger.viewGestures.debug("\(#function) Mouse tracking requires pan to fail: \(panCanScroll)")
            return panCanScroll
        }

        return false
    }

    @objc(gestureRecognizerShouldBegin:)
    func gestureRecognizerShouldBegin(_ gestureRecognizer: NSGestureRecognizer) -> Bool {
        Logger.viewGestures.debug("\(#function) Gesture: \(Self.loggingDescription(for: gestureRecognizer))")

        guard let webView, let impl = webView._impl() else {
            return false
        }

        let locationInViewCoordinates = gestureRecognizer.location(in: webView)

        // While catching a decelerating scroll, only select gestures are allowed to begin:
        // - single click, so it can reset the interruption state
        // - pan, so it can continue with successive scrolls
        if caughtDeceleratingScroll {
            if gestureRecognizer === singleClickGestureRecognizer {
                return true
            }
            if gestureRecognizer !== panGestureRecognizer {
                return false
            }
        }

        if gestureRecognizer === doubleClickGestureRecognizer {
            return impl.allowsMagnification()
        }

        if gestureRecognizer === secondaryClickGestureRecognizer {
            return secondaryClickShouldBegin(at: locationInViewCoordinates)
        }

        if gestureRecognizer === dragPressGestureRecognizer {
            return dragPressShouldBegin(at: locationInViewCoordinates)
        }

        if gestureRecognizer === panGestureRecognizer {
            return panShouldBegin(at: locationInViewCoordinates)
        }

        if gestureRecognizer === singleClickGestureRecognizer {
            return !impl.isTextSelectedAtPoint(locationInViewCoordinates)
        }

        if gestureRecognizer === mouseTrackingGestureRecognizer {
            return !impl.isTextSelectedAtPoint(locationInViewCoordinates)
        }

        return true
    }

    // swift-format-ignore: NoLeadingUnderscores
    @objc(_gestureRecognizer:canPreventGestureRecognizer:)
    func _gestureRecognizer(
        _ preventingGestureRecognizer: NSGestureRecognizer?,
        canPrevent preventedGestureRecognizer: NSGestureRecognizer?
    ) -> Bool {
        guard let preventingGestureRecognizer, let preventedGestureRecognizer else {
            return false
        }

        Logger.viewGestures.debug(
            "\(#function) Preventing gesture: \(Self.loggingDescription(for: preventingGestureRecognizer)), Prevented gesture: \(Self.loggingDescription(for: preventedGestureRecognizer))"
        )

        guard let webView else {
            return false
        }

        // None of our gesture recognizers may prevent an enclosing scroll view's pan (or any other
        // scroll/zoom) gesture, so that a scroll can always be handed off to the enclosing scroll view
        // e.g. a scroll over a draggable <img> in a non-scrollable web view.
        if isScrollOrZoomGestureRecognizer(preventedGestureRecognizer) {
            return false
        }

        let isOurClickGesture =
            preventingGestureRecognizer === singleClickGestureRecognizer
            || preventingGestureRecognizer === secondaryClickGestureRecognizer
            || preventingGestureRecognizer === mouseTrackingGestureRecognizer
            || preventingGestureRecognizer === dragPressGestureRecognizer

        guard isOurClickGesture else {
            return true
        }

        // Don't let other click gestures prevent the secondary click GR; it must be allowed to fire its
        // press timer (0.72s) without being short-circuited by gestures that recognize earlier
        // (e.g. single click and mouse-tracking, which both transition to Began at mouse-down).
        if preventedGestureRecognizer === secondaryClickGestureRecognizer {
            return false
        }

        // Don't let our click gestures prevent text selection manager gestures;
        // they should be allowed to recognize simultaneously (per shouldRecognizeSimultaneouslyWithGestureRecognizer:).
        let preventsTextSelection = webView.textSelectionManager?.gesturesForFailureRequirements
            .contains { textSelectionGesture in
                preventedGestureRecognizer === textSelectionGesture
            }

        if let preventsTextSelection, preventsTextSelection {
            return false
        }

        return true
    }
}

private func isSamePair(_ a: NSGestureRecognizer?, _ b: NSGestureRecognizer?, _ x: NSGestureRecognizer?, _ y: NSGestureRecognizer?) -> Bool
{
    (a === x && b === y) || (b === x && a === y)
}

extension NSGestureRecognizer {
    fileprivate var isBuiltInScrollViewPan: Bool {
        guard let scrollViewPanGestureClass = NSClassFromString("NSScrollViewPanGestureRecognizer") else {
            assertionFailure("Class 'NSScrollViewPanGestureRecognizer' not found")
            return false
        }
        return isKind(of: scrollViewPanGestureClass)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
