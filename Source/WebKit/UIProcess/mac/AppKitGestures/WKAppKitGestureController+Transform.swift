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
    struct TransformGestureTracker {
        private static let driveTimeout: TimeInterval = 0.050

        // Magnification and rotation GRs report their values cumulatively over a gesture.
        // These variables track the last reported value so that we can forward deltas.
        private var lastCumulativeMagnification: Double = 0
        private var lastCumulativeRotation: Double = 0

        var contentDeclinedGesture = false
        private var lastDriveTime: TimeInterval?

        mutating func takeMagnificationDelta(cumulative magnification: Double, phase: WebKit.WebEventPhase) -> Double {
            if phase == .Began {
                lastCumulativeMagnification = 0
            }
            let delta = (1 + magnification) / (1 + lastCumulativeMagnification) - 1
            lastCumulativeMagnification = magnification
            return delta
        }

        mutating func takeRotationDelta(cumulative rotation: Double, phase: WebKit.WebEventPhase) -> Double {
            if phase == .Began {
                lastCumulativeRotation = 0
            }
            let delta = rotation - lastCumulativeRotation
            lastCumulativeRotation = rotation
            return delta
        }

        mutating func didDrive() {
            lastDriveTime = ProcessInfo.processInfo.systemUptime
        }

        var droveRecently: Bool {
            guard let lastDriveTime else {
                return false
            }
            return ProcessInfo.processInfo.systemUptime - lastDriveTime < Self.driveTimeout
        }

        // Keeps the cumulative values, which the next gesture's Began resets.
        mutating func reset() {
            contentDeclinedGesture = false
            lastDriveTime = nil
        }
    }

    // MARK: Gesture Recognizers

    func makeMagnificationGestureRecognizer() -> NSMagnificationGestureRecognizer {
        makeGestureRecognizer(
            NSMagnificationGestureRecognizer(target: self, action: #selector(magnificationGestureRecognized)),
            name: "WKMagnificationGesture"
        )
    }

    func makeRotationGestureRecognizer() -> NSRotationGestureRecognizer {
        makeGestureRecognizer(
            NSRotationGestureRecognizer(target: self, action: #selector(rotationGestureRecognized)),
            name: "WKRotationGesture"
        )
    }

    // MARK: Magnification and Rotation

    @objc
    private func magnificationGestureRecognized(_ gesture: NSMagnificationGestureRecognizer) {
        guard let webView = view else {
            return
        }

        let phase = WebKit.WebEventPhase(gesture.state)
        let magnification = transformGestureTracker.takeMagnificationDelta(cumulative: gesture.magnification, phase: phase)
        transformGestureDidUpdate(phase)

        let event = makeTransformGestureEvent(
            .Magnification,
            for: gesture,
            phase: phase,
            in: webView,
            scale: Float(magnification),
            allowsNativeZoom: magnificationGestureRecognizerCanZoom
        )

        guard let impl = webView._impl() else {
            return
        }

        guard impl.allowsMagnification() else {
            send(event, from: webView)
            return
        }

        if phase == .Began {
            impl.dismissContentRelativeChildWindowsWithAnimation(false)
        }

        let gestureController = impl.ensureGestureController()
        if gestureController.hasActiveMagnificationGesture() {
            gestureController.handleMagnificationGesture(
                magnification,
                phase,
                event.map { WebCore.FloatPoint($0.position()) } ?? WebCore.FloatPoint(),
                .Touch
            )
            return
        }

        send(event, from: webView)
    }

    @objc
    private func rotationGestureRecognized(_ gesture: NSRotationGestureRecognizer) {
        guard let webView = view else {
            return
        }

        let phase = WebKit.WebEventPhase(gesture.state)
        transformGestureDidUpdate(phase)

        let rotation = Float(transformGestureTracker.takeRotationDelta(cumulative: gesture.rotationInDegrees, phase: phase))
        send(makeTransformGestureEvent(.Rotation, for: gesture, phase: phase, in: webView, rotation: rotation), from: webView)
    }

    private func transformGestureDidUpdate(_ phase: WebKit.WebEventPhase) {
        if phase == .Began || phase == .Changed {
            transformGestureDidDrive()
        } else {
            transformGestureDidEnd()
        }
    }

    // Nil when the phase maps to no web event.
    private func makeTransformGestureEvent(
        _ kind: WebKit.NativeWebGestureEvent.Kind,
        for gesture: NSGestureRecognizer,
        phase: WebKit.WebEventPhase,
        in webView: WKWebView,
        scale: Float = 0,
        rotation: Float = 0,
        allowsNativeZoom: Bool = true
    ) -> WebKit.NativeWebGestureEvent? {
        let eventInit = WebKit.NativeWebGestureEvent.Init(
            kind: kind,
            phase: phase,
            locationInWindow: gestureCentroidInWindow(for: gesture),
            gestureScale: scale,
            gestureRotation: rotation,
            timestamp: WTF.MonotonicTime.fromRawSeconds(GetCurrentEventTime()),
            allowsNativeZoom: allowsNativeZoom,
            inputSource: .Touch
        )
        return WebKit.NativeWebGestureEvent.create(eventInit, webView).get()
    }

    private func send(_ event: WebKit.NativeWebGestureEvent?, from webView: WKWebView) {
        if let event {
            webView._protectedPage().get()?.handleGestureEvent(consuming: .init(event))
        }
    }

    // FIXME: rdar://185815023 (-[NSRotationGestureRecognizer locationInView:] should reflect live centroid of the gesture)
    // The rotation GR's location(in:) does not reflect its live centroid over
    // the course of a twist, so we compute it directly and use it to feed our
    // gesture machinery. Otherwise, the rotation and magnification locations stomp
    // over each other in ViewGestureController.
    private func gestureCentroidInWindow(for gesture: NSGestureRecognizer) -> WebCore.FloatPoint {
        let touches: [NSTouch]
        if let magnificationGesture = gesture as? NSMagnificationGestureRecognizer {
            touches = magnificationGesture.touches
        } else if let rotationGesture = gesture as? NSRotationGestureRecognizer {
            touches = rotationGesture.touches
        } else {
            touches = []
        }

        let count = touches.count

        if count == 0 {
            return WebCore.FloatPoint(gesture.location(in: nil))
        }

        var centroidX: Float = 0
        var centroidY: Float = 0
        for touch in touches {
            let locationInWindow = touch.location(in: nil)
            centroidX = Float(Double(centroidX) + locationInWindow.x)
            centroidY = Float(Double(centroidY) + locationInWindow.y)
        }
        return WebCore.FloatPoint(centroidX, centroidY).scaled(Float(1.0 / Double(count)))
    }

    private var magnificationGestureRecognizerCanZoom: Bool {
        guard let webView = view else {
            return false
        }

        return touchActions(at: magnificationGestureRecognizer?.location(in: webView) ?? .zero).allowsPinchZoom
    }

    // MARK: Mouse Tracking Suppression

    private var isRecognizingTransformGesture: Bool {
        magnificationGestureRecognizer?.state.isRecognizing ?? false || rotationGestureRecognizer?.state.isRecognizing ?? false
    }

    var isTransformGestureActiveOrEnding: Bool {
        func isActiveOrEnding(_ gesture: NSGestureRecognizer?) -> Bool {
            guard let state = gesture?.state else {
                return false
            }
            return state.isRecognizing || state == .ended
        }

        return isActiveOrEnding(magnificationGestureRecognizer) || isActiveOrEnding(rotationGestureRecognizer)
    }

    var shouldSuppressMouseTrackingForTransformGesture: Bool {
        transformGestureTracker.contentDeclinedGesture && isRecognizingTransformGesture && transformGestureTracker.droveRecently
    }

    private func transformGestureDidEnd() {
        guard !isRecognizingTransformGesture else {
            return
        }

        transformGestureTracker.contentDeclinedGesture = false
        scrollState.resetCaughtDeceleratingScroll()
    }

    private func transformGestureDidDrive() {
        transformGestureTracker.didDrive()

        guard transformGestureTracker.contentDeclinedGesture, mouseTrackingDelivery == .mouseDown, let impl = view?._impl(),
            !impl.ignoresAllEvents()
        else {
            return
        }

        let activeMouseTrackingGesture = activeMouseTrackingGestureRecognizer
        let modifierFlags = activeMouseTrackingGesture?.modifierFlags ?? []
        let locationInWindow =
            activeMouseTrackingGesture?.mouseLocationInWindow
            ?? NSPoint(x: CGFloat(mouseTrackingStartLocationInWindow.x()), y: CGFloat(mouseTrackingStartLocationInWindow.y()))

        sendSyntheticMouseEvent(
            .leftMouseUp,
            at: locationInWindow,
            to: impl,
            modifierFlags: modifierFlags,
            pressure: 0,
            canInitiateDrag: .No,
            reportingMovementFor: activeMouseTrackingGesture
        )

        mouseTrackingDelivery = .none
        mouseTrackingIsSuppressedForTransformGesture = true
    }
}

@objc(TransformGestures)
@implementation
extension WKAppKitGestureController {
    func transformGestureWasNotHandledByContent() {
        guard isRecognizingTransformGesture else {
            return
        }

        transformGestureTracker.contentDeclinedGesture = true
        transformGestureDidDrive()
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
