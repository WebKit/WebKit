// Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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
import WebKit_Internal
import os
private import WebCore_Private

extension WKAppKitGestureController {
    // A text-selection drag WebViewImpl hands over; the completion handler is taken when the drag session begins.
    struct TextSelectionDrag {
        let gesture: NSGestureRecognizer
        var completionHandler: ((NSDraggingSession) -> Void)?
    }

    // MARK: Gesture Recognizers

    // Matching _UIDragInteractionDefaultLiftDelay.
    private static let dragPressMinimumDuration: TimeInterval = 0.325

    func makeDragPressGestureRecognizer() -> WKPressGestureRecognizer {
        makeGestureRecognizer(
            WKPressGestureRecognizer(target: self, action: #selector(dragPressGestureRecognized)),
            name: "WKDragPressGesture"
        ) {
            $0.minimumPressDuration = Self.dragPressMinimumDuration
            $0.cancelPastAllowableMovement = false
            $0.buttonMask = 0
            $0.numberOfTouchesRequired = 1
        }
    }

    // MARK: Drag Handling

    @objc
    private func dragPressGestureRecognized(_ gesture: WKPressGestureRecognizer) {
        guard let webView = view else {
            return
        }

        logGestureAction(gesture)

        guard let dragPressGestureRecognizer, dragPressGestureRecognizer === gesture else {
            return
        }

        guard let impl = webView._impl() else {
            return
        }

        if impl.ignoresAllEvents() {
            Logger.viewGestures.debug("[pageProxyID=\(self.pageLogIdentifier)] \(#function): Ignored gesture")
            return
        }

        let locationInWindow = gesture.location(in: nil)
        let timestamp = GetCurrentEventTime()
        func sendMouseEvent(_ type: NSEvent.EventType, pressure: Float, reportingMovement: Bool) {
            sendSyntheticMouseEvent(
                type,
                at: locationInWindow,
                to: impl,
                modifierFlags: gesture.modifierFlags,
                pressure: pressure,
                canInitiateDrag: .Yes,
                timestamp: timestamp,
                reportingMovementFor: reportingMovement ? dragPressGestureRecognizer : nil
            )
        }

        switch gesture.state {
        case .began:
            handleTapCancelled()
            dragGestureDidReceiveDragStart = false
            dragIdentifier &+= 1

            sendMouseEvent(.leftMouseDown, pressure: 1, reportingMovement: false)
            dragGestureHasSentMouseDown = true

            dragPressGestureRecognizer.beginReportingMovement(fromWindowLocation: locationInWindow)

        case .changed:
            // Drive WebCore's drag-initiation hysteresis. Once the session exists, AppKit tracks the
            // gesture itself and WebCore is driven by the platform drag callbacks, so we stop feeding it.
            guard dragGestureHasSentMouseDown, gestureDraggingSession == nil else {
                break
            }

            sendMouseEvent(.leftMouseDragged, pressure: 1, reportingMovement: true)

        case .ended, .cancelled, .failed:
            guard dragGestureHasSentMouseDown else {
                break
            }

            sendMouseEvent(.leftMouseUp, pressure: 0, reportingMovement: true)

            // Normally, drag state is cleared in draggingSessionEnded() (normal completion) or in startDrag() when
            // beginDraggingSession(with:gesture:source:) returns nil (gesture ended before session started).
            // But, if the page prevents dragstart, we won't get startDrag(), so clear the state ourselves.
            support.doAfterProcessingAllPendingMouseEvents { [weak self, expectedDragIdentifier = dragIdentifier] in
                guard let self, dragIdentifier == expectedDragIdentifier, dragGestureHasSentMouseDown, !dragGestureDidReceiveDragStart,
                    gestureDraggingSession == nil
                else {
                    return
                }
                dragGestureHasSentMouseDown = false
            }

        case .possible:
            break

        @unknown default:
            break
        }
    }

    func dragPressShouldBegin(at locationInView: NSPoint) -> Bool {
        guard view != nil else {
            return false
        }

        let radius = positionInformationToleranceRadius(for: dragPressGestureRecognizer)

        let information = positionInformation.currentInformation

        // FIXME: Migrate to requestDragStart: IPC for an authoritative decision.
        // The heuristic below approximates DragController::draggableElement() by consulting the same element-type and style signals.
        let isDraggable = information.representsDraggableElement
        let requestIsValid = isPositionInformationValid(at: locationInView, radius: radius)
        let shouldDrag = requestIsValid && isDraggable && !information.isHoverActivatedByClick

        Logger.viewGestures.log(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            Drag-press shouldBegin → \(shouldDrag) \
            (hasInfo=\(self.positionInformation.hasValidCurrentInformation) \
            link=\(information.isLink) image=\(information.isImage) \
            attachment=\(information.isAttachment) \
            dhtml=\(information.isDHTMLDraggable) color=\(information.isColorInput) \
            prefersDrag=\(information.prefersDraggingOverTextSelection) \
            hoverActivatedByClick=\(information.isHoverActivatedByClick) radius=\(radius))
            """
        )

        if !requestIsValid {
            positionInformation.invalidate()
        }

        return shouldDrag
    }
}

@objc(Drag)
@implementation
extension WKAppKitGestureController {
    var activeDragGestureRecognizer: NSGestureRecognizer? {
        if let textSelectionDrag {
            return textSelectionDrag.gesture
        }
        if dragGestureHasSentMouseDown {
            return dragPressGestureRecognizer
        }
        return nil
    }

    func setTextSelectionDragGesture(_ gesture: NSGestureRecognizer, completionHandler: @escaping (NSDraggingSession) -> Void) {
        if let textSelectionDrag {
            Logger.viewGestures.error(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                Replacing prior text-selection drag gesture \
                \(textSelectionDrag.gesture.loggingDescription) (completion handler set: \
                \(textSelectionDrag.completionHandler != nil)); prior drag never reached \
                setGestureDraggingSession(_:)
                """
            )
            assertionFailure("A text selection drag gesture is already set")
        }

        textSelectionDrag = TextSelectionDrag(gesture: gesture, completionHandler: completionHandler)
    }

    func didReceiveDragStart() {
        dragGestureDidReceiveDragStart = true
    }

    func setGestureDraggingSession(_ session: NSDraggingSession?) {
        gestureDraggingSession = session
        guard let session else {
            return
        }

        cancelPointerTrackingIfNeeded()

        Logger.viewGestures.log("[pageProxyID=\(self.pageLogIdentifier)] \(#function): Drag session began")

        if let handler = textSelectionDrag?.completionHandler.take() {
            handler(session)
        }
    }

    func clearGestureDragState() {
        gestureDraggingSession = nil
        textSelectionDrag = nil
        dragGestureHasSentMouseDown = false
        dragGestureDidReceiveDragStart = false
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
