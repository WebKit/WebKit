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
private import AppKit_Private.NSPressGestureRecognizer_Private
private import WebCore_Private

extension WKAppKitGestureController {
    // What a mouse-tracking gesture delivers to the page: nothing yet, mouse moves with no button held, or a pressed
    // mouse, after the mouse down it sent.
    enum MouseTrackingDelivery {
        case none
        case mouseMoves
        case mouseDown
    }

    struct TrackedPointer {
        let startLocationInWindow: NSPoint
        var lastLocationInWindow: NSPoint
        var hasMoved = false
    }

    // MARK: Gesture Recognizers

    func makeMouseTrackingGestureRecognizer(numberOfTouchesRequired: Int, name: String) -> WKMouseTrackingGestureRecognizer {
        makeGestureRecognizer(
            WKMouseTrackingGestureRecognizer(target: self, action: #selector(mouseTrackingGestureRecognized)),
            name: name
        ) {
            $0.minimumPressDuration = 0
            $0.cancelPastAllowableMovement = false
            $0.buttonMask = 0
            $0.numberOfTouchesRequired = numberOfTouchesRequired
        }
    }

    func makePointerTrackingGestureRecognizer() -> WKPointerTrackingGestureRecognizer {
        makeGestureRecognizer(
            WKPointerTrackingGestureRecognizer(target: self, action: #selector(pointerTrackingGestureRecognized)),
            name: "WKPointerTrackingGesture"
        ) {
            $0.minimumPressDuration = 0
            $0.allowableMovement = CGFloat.greatestFiniteMagnitude
            $0.cancelPastAllowableMovement = false
            $0.buttonMask = 0
            // FIXME: Tracking more than one touch needs a pointer per touch.
            $0.numberOfTouchesRequired = 1
            $0.refusesToBeFailureRequirement = true
            $0.pointerTrackingDelegate = self
        }
    }

    // MARK: Mouse Tracking

    @objc
    private func mouseTrackingGestureRecognized(_ gesture: WKMouseTrackingGestureRecognizer) {
        guard let webView = view else {
            return
        }

        logGestureAction(gesture)

        let stateAtEntry = gesture.state
        defer {
            if scrollState.caughtDeceleratingScroll && stateAtEntry.isTerminal {
                scrollState.resetCaughtDeceleratingScroll()
            }
        }

        if dragGestureHasSentMouseDown {
            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                Exiting early because dragGestureHasSentMouseDown is true
                """
            )
            return
        }

        if isSuppressingSingleClickGestureForTextSelection {
            Logger.viewGestures.debug(
                """
                [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                Exiting early because isSuppressingSingleClickGestureForTextSelection is true
                """
            )
            return
        }

        guard let impl = webView._impl(), !impl.ignoresAllEvents() else {
            return
        }

        let timestamp = GetCurrentEventTime()

        switch gesture.state {
        case .began:
            mouseTrackingDidBegin(gesture)
        case .changed:
            mouseTrackingDidChange(gesture, in: webView, impl: impl, timestamp: timestamp)
        case .ended, .cancelled, .failed:
            mouseTrackingDidEnd(gesture, impl: impl, timestamp: timestamp)
        default:
            break
        }
    }

    private func mouseTrackingDidBegin(_ gesture: WKMouseTrackingGestureRecognizer) {
        mouseTrackingIsSuppressedForTransformGesture = false
        activeMouseTrackingGestureRecognizer = gesture
        if mouseTrackingDelivery == .mouseDown {
            gesture.beginTrackingMouseInherited(
                fromWindowLocation: NSPoint(
                    x: CGFloat(mouseTrackingStartLocationInWindow.x()),
                    y: CGFloat(mouseTrackingStartLocationInWindow.y())
                )
            )
        } else {
            gesture.beginTrackingMouse()
        }
    }

    private func mouseTrackingDidChange(
        _ gesture: WKMouseTrackingGestureRecognizer,
        in webView: WKWebView,
        impl: WebKit.WebViewImpl,
        timestamp: TimeInterval
    ) {
        guard activeMouseTrackingGestureRecognizer === gesture else {
            return
        }

        if shouldSuppressMouseTrackingForTransformGesture {
            mouseTrackingIsSuppressedForTransformGesture = true
            return
        }

        guard startMouseTrackingDeliveryIfNeeded(gesture, in: webView, impl: impl, timestamp: timestamp) else {
            return
        }

        if mouseTrackingDelivery == .mouseMoves {
            let locationInWindow = gesture.mouseLocationInWindow
            let mouseMoved = syntheticMouseEvent(
                .mouseMoved,
                at: locationInWindow,
                modifierFlags: gesture.modifierFlags,
                windowNumber: impl.windowNumber(),
                pressure: 0,
                timestamp: timestamp,
                clickCount: 0,
                reportingMovementFor: gesture
            )
            impl.mouseMoved(mouseMoved, .Touch)
            return
        }

        sendSyntheticMouseEvent(
            .leftMouseDragged,
            at: gesture.mouseLocationInWindow,
            to: impl,
            modifierFlags: gesture.modifierFlags,
            pressure: 1,
            canInitiateDrag: .No,
            timestamp: timestamp,
            reportingMovementFor: gesture
        )
    }

    // Returns whether the gesture delivers mouse events yet.
    private func startMouseTrackingDeliveryIfNeeded(
        _ gesture: WKMouseTrackingGestureRecognizer,
        in webView: WKWebView,
        impl: WebKit.WebViewImpl,
        timestamp: TimeInterval
    ) -> Bool {
        guard mouseTrackingDelivery == .none else {
            return true
        }

        // Either the synthetic single-click path or this mouse-tracking path delivers a mouse
        // down for a given interaction, but never both. An event that stays within the single-click
        // gesture's allowable movement is a click: that gesture stays alive and, when it ends, the
        // synthetic click path delivers the mouse down, mouse up, and click.
        // Only once the event moves past that threshold — at which point the single-click gesture
        // cancels and this becomes a drag — does mouse tracking take over event delivery. Sending a
        // mouse down here for a stationary click would deliver a second mouse down to the content,
        // which should be avoided.
        guard movementExceedsClickAllowableMovement(gesture.movementInWindowSinceStart) else {
            return false
        }

        let isResumingAfterTransformGesture = exchange(&mouseTrackingIsSuppressedForTransformGesture, with: false)
        let mouseDownLocation = isResumingAfterTransformGesture ? gesture.mouseLocationInWindow : gesture.startLocationInWindow

        if isResumingAfterTransformGesture {
            gesture.beginReportingMovement(fromWindowLocation: mouseDownLocation)
        }

        if isHoverActivatedByClick(at: webView.convert(mouseDownLocation, from: nil)) {
            mouseTrackingDelivery = .mouseMoves
            return true
        }

        sendSyntheticMouseEvent(
            .leftMouseDown,
            at: adjustedMouseDownLocation(inWindow: mouseDownLocation),
            to: impl,
            modifierFlags: gesture.modifierFlags,
            pressure: 1,
            canInitiateDrag: .No,
            timestamp: timestamp
        )
        mouseTrackingDelivery = .mouseDown
        return true
    }

    private func mouseTrackingDidEnd(_ gesture: WKMouseTrackingGestureRecognizer, impl: WebKit.WebViewImpl, timestamp: TimeInterval) {
        guard activeMouseTrackingGestureRecognizer === gesture else {
            return
        }

        activeMouseTrackingGestureRecognizer = nil

        switch mouseTrackingDelivery {
        case .none:
            break

        case .mouseMoves:
            mouseTrackingDelivery = .none

        case .mouseDown:
            if mouseTrackingWillBeHandedOff {
                Logger.viewGestures.debug(
                    """
                    [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
                    Handing off mouse tracking to another gesture
                    """
                )
                mouseTrackingStartLocationInWindow = WebCore.FloatPoint(gesture.mouseLocationInWindow)
                return
            }

            mouseTrackingDelivery = .none
            sendSyntheticMouseEvent(
                .leftMouseUp,
                at: gesture.mouseLocationInWindow,
                to: impl,
                modifierFlags: gesture.modifierFlags,
                pressure: 0,
                canInitiateDrag: .No,
                timestamp: timestamp,
                reportingMovementFor: gesture
            )
        }
    }

    func canTrackMouse(with gesture: NSPressGestureRecognizer) -> Bool {
        guard let webView = view else {
            return false
        }

        if gesture.numberOfTouchesRequired <= 1 {
            return true
        }

        // If the web content permits panning, the pan GR will win.
        // This multi-touch tracking recognizer covers the remaining case, where
        // the content has opted out of panning. Web apps typically depend on
        // touch/pointer/mousemove events for a notion of "translation" in this
        // case, such as on maps.apple.com. As such, we complement the logic here
        // with the predicate in panGestureRecognizerCanScroll.
        return !touchActions(at: gesture.location(in: webView)).allowsPanning
    }

    private var mouseTrackingWillBeHandedOff: Bool {
        guard mouseTrackingDelivery == .mouseDown else {
            return false
        }

        return [singleTouchMouseTrackingGestureRecognizer, multiTouchMouseTrackingGestureRecognizer]
            .contains { $0?.state.isRecognizing ?? false }
    }

    private func movementExceedsClickAllowableMovement(_ movementInWindow: NSSize) -> Bool {
        let allowableMovement = singleClickGestureRecognizer?.allowableMovement ?? 0
        return abs(movementInWindow.width) > allowableMovement || abs(movementInWindow.height) > allowableMovement
    }

    private static let maximumInteractionAdjustmentDistance: CGFloat = 48

    func adjustedMouseDownLocation(inWindow locationInWindow: NSPoint) -> NSPoint {
        guard let webView = view,
            let adjustedLocationInView = positionInformation.currentInformation.nearbySliderInteractionLocation.value.map({ CGPoint($0) })
        else {
            return locationInWindow
        }

        let locationInView = webView.convert(locationInWindow, from: nil)
        guard isPositionInformationValid(at: locationInView, radius: Self.mouseDownPositionInformationToleranceRadius) else {
            return locationInWindow
        }

        if abs(adjustedLocationInView.x - locationInView.x) > Self.maximumInteractionAdjustmentDistance
            || abs(adjustedLocationInView.y - locationInView.y) > Self.maximumInteractionAdjustmentDistance
        {
            return locationInWindow
        }

        guard webView.bounds.contains(adjustedLocationInView) else {
            return locationInWindow
        }

        return webView.convert(adjustedLocationInView, to: nil)
    }

    // MARK: Pointer Tracking

    @objc
    private func pointerTrackingGestureRecognized(_ gesture: NSGestureRecognizer) {
        guard view != nil else {
            return
        }

        logGestureAction(gesture)

        precondition(pointerTrackingGestureRecognizer === gesture)

        switch gesture.state {
        case .cancelled, .failed:
            cancelPointerTrackingIfNeeded()
        default:
            break
        }
    }

    private func dispatchTrackedPointerEvent(
        _ phase: WebKit.WebEventPhase,
        atLocationInWindow locationInWindow: NSPoint,
        modifierFlags: NSEvent.ModifierFlags
    ) {
        guard let webView = view, let page = webView._protectedPage().get() else {
            return
        }

        let location = WebCore.FloatPoint(webView.convert(locationInWindow, from: nil))
        let modifiers = WebKit.WebEventFactory.toWebEventModifierFlags(modifierFlags)
        page.dispatchTrackedPointerEvent(
            nil,
            phase,
            location,
            modifiers,
            consuming: .init { [weak self] wasCanceled in
                guard phase == .Began else {
                    return
                }
                self?.didDispatchPointerDown(wasCanceled: wasCanceled)
            }
        )
    }

    private func didDispatchPointerDown(wasCanceled: Bool) {
        guard let pointerDownDeferringGestureRecognizer, pointerDownDeferringGestureRecognizer.state == .possible else {
            return
        }

        Logger.viewGestures.debug(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            deferral resolved: pointerdown canceled=\(wasCanceled)
            """
        )

        pointerDownDeferringGestureRecognizer.endDeferralShouldPreventGestures(wasCanceled)
    }

    func pointerTrackingShouldBegin(at locationInView: NSPoint, modifierFlags: NSEvent.ModifierFlags) -> Bool {
        guard let webView = view, pointerTrackingGestureRecognizer?.isEnabled ?? false else {
            return false
        }

        if !(webView._impl()?.window()?.isKeyWindow ?? false) && !modifierFlags.contains(.command) {
            return false
        }

        // A pan here is delivered as mouse moves with no button held, so the page must not see a pressed pointer.
        if isHoverActivatedByClick(at: locationInView) {
            return false
        }

        return !isPointInScrollbar(at: locationInView)
    }

    func cancelPointerTrackingIfNeeded() {
        guard let trackedPointer = trackedPointer.take() else {
            return
        }

        Logger.viewGestures.debug(
            "[pageProxyID=\(self.pageLogIdentifier)] \(#function): Canceling the tracked pointer"
        )

        dispatchTrackedPointerEvent(.Cancelled, atLocationInWindow: trackedPointer.lastLocationInWindow, modifierFlags: [])
    }
}

@objc(WKPointerTrackingGestureRecognizerDelegate)
@implementation
extension WKAppKitGestureController {
    @objc(pointerTrackingGestureRecognizer:didUpdateTouchWithState:locationInWindow:modifierFlags:)
    func pointerTrackingGestureRecognizer(
        _ gestureRecognizer: WKPointerTrackingGestureRecognizer,
        didUpdateTouchWith state: NSGestureRecognizer.State,
        locationInWindow: NSPoint,
        modifierFlags: NSEvent.ModifierFlags
    ) {
        guard let webView = view else {
            return
        }

        Logger.viewGestures.debug(
            """
            [pageProxyID=\(self.pageLogIdentifier)] \(#function): \
            \(gestureRecognizer.loggingDescription) touch state \
            \(state.name) at \(NSStringFromPoint(locationInWindow))
            """
        )

        switch state {
        case .began:
            cancelPointerTrackingIfNeeded()

            guard webView._impl()?.ignoresAllEvents() == false else {
                break
            }

            let locationInView = webView.convert(locationInWindow, from: nil)
            guard pointerTrackingShouldBegin(at: locationInView, modifierFlags: modifierFlags) else {
                // The page must not treat the click that follows as the release of the last tracked touch.
                if isHoverActivatedByClick(at: locationInView) {
                    dispatchTrackedPointerEvent(
                        .Cancelled,
                        atLocationInWindow: locationInWindow,
                        modifierFlags: modifierFlags
                    )
                }
                break
            }

            trackedPointer = TrackedPointer(startLocationInWindow: locationInWindow, lastLocationInWindow: locationInWindow)
            dispatchTrackedPointerEvent(.Began, atLocationInWindow: locationInWindow, modifierFlags: modifierFlags)

        case .changed:
            guard var pointer = trackedPointer else {
                break
            }

            pointer.lastLocationInWindow = locationInWindow

            // Report no movement until the touch has moved too far to be a tap, so that a
            // touch that is held still reads as still.
            if !pointer.hasMoved {
                let movementInWindow = NSSize(
                    width: locationInWindow.x - pointer.startLocationInWindow.x,
                    height: locationInWindow.y - pointer.startLocationInWindow.y
                )
                pointer.hasMoved = movementExceedsClickAllowableMovement(movementInWindow)
            }
            trackedPointer = pointer

            if pointer.hasMoved {
                dispatchTrackedPointerEvent(.Changed, atLocationInWindow: locationInWindow, modifierFlags: modifierFlags)
            }

        case .ended:
            guard let pointer = trackedPointer.take() else {
                break
            }

            dispatchTrackedPointerEvent(
                .Ended,
                atLocationInWindow: pointer.hasMoved ? locationInWindow : pointer.startLocationInWindow,
                modifierFlags: modifierFlags
            )

        case .cancelled:
            cancelPointerTrackingIfNeeded()

        default:
            break
        }
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
