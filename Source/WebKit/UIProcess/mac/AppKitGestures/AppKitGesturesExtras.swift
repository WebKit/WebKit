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

#if canImport(WebKit_Internal)
import WebKit_Internal
#endif

import AppKit_Private.NSPanGestureRecognizer_Private

extension WKPanGestureRecognizer {
    /// FXIME: <rdar://188400680> Adopt NSPanGestureRecognizer._defaultTouchHysteresis once it is not IPI.
    private static let touchHysteresis: CGFloat = 10

    /// The widest angle away from a permitted axis at which a pan is still treated as ambiguous.
    ///
    /// We match `maximumBeginAngleFromScrollableAxis` from UIScrollViewPanGestureRecognizer.
    private static let maximumAmbiguousDragAngle: Angle2D = .degrees(60)

    override func acceptsFirst(touch: NSTouch, event: NSEvent) -> Bool {
        true
    }

    override func touchesBegan(with event: NSEvent) {
        super.touchesBegan(with: event)

        guard state == .possible else {
            return
        }
        gestureStartTime = event.timestamp
        gestureStartLocationInWindow = location(in: nil)
    }

    override func touchesMoved(with event: NSEvent) {
        super.touchesMoved(with: event)

        failIfCommittedToRestrictedAxis()

        guard state == .began || state == .changed else {
            return
        }
        if state == .changed {
            reachedChangedState = true
        }
        lastMovementTime = event.timestamp
        lastMovementLocationInWindow = location(in: nil)
    }

    private func failIfCommittedToRestrictedAxis() {
        guard state == .possible, hasCommittedToRestrictedAxis else {
            return
        }

        state = .failed
    }

    private var hasCommittedToRestrictedAxis: Bool {
        guard _canPanHorizontally != _canPanVertically else {
            return false
        }

        // Only a one-finger pan is relevant here. Single-finger mouse tracking fails when
        // a second finger lands, and two-finger mouse tracking never begins where panning
        // is allowed.
        guard touches.count == 1, let touch = touches.first else {
            return false
        }

        // location(in: nil) can't give live location while the state == .possible.
        let currentLocationInWindow = touch.location(in: nil)

        // FIXME: <rdar://188247170> Use -_startLocationInView: here when it lands in builds.
        let startLocationInWindow = gestureStartLocationInWindow
        let travel = CGSize(
            width: currentLocationInWindow.x - startLocationInWindow.x,
            height: currentLocationInWindow.y - startLocationInWindow.y
        )

        let alongPermittedAxis = abs(_canPanHorizontally ? travel.width : travel.height)
        let alongRestrictedAxis = abs(_canPanHorizontally ? travel.height : travel.width)

        guard alongRestrictedAxis >= Self.touchHysteresis else {
            return false
        }

        guard alongPermittedAxis > 0 else {
            return true
        }

        let dragAngle = Angle2D.atan(alongRestrictedAxis / alongPermittedAxis)
        return dragAngle > Self.maximumAmbiguousDragAngle
    }
}

extension WKDeferringGestureRecognizer {
    @_implementationOnly
    open override var state: NSGestureRecognizer.State {
        get {
            super.state
        }
        set {
            let previousState = state
            super.state = newValue

            internalSetState(previousState: previousState, newValue: newValue)
        }
    }

    @_implementationOnly
    open override func touchesBegan(with event: NSEvent) {
        internalActionBegan(with: event) {
            super.touchesBegan(with: event)
        }
    }

    @_implementationOnly
    open override func touchesEnded(with event: NSEvent) {
        internalActionEnded(with: event) {
            super.touchesEnded(with: event)
        }
    }

    @_implementationOnly
    open override func touchesCancelled(with event: NSEvent) {
        internalActionCancelled(with: event) {
            super.touchesCancelled(with: event)
        }
    }

    @_implementationOnly
    open override func canBePrevented(by gestureRecognizer: NSGestureRecognizer) -> Bool {
        internalCanBePrevented(by: gestureRecognizer)
    }

    // swift-format-ignore: NoLeadingUnderscores
    @_implementationOnly
    open override func _acceptedEventMask() -> NSEvent.EventTypeMask {
        // Intentionally restricted to direct touches; expand if other event masks are ever needed.
        .directTouch
    }
}

/// Loosely inspired by WKTouchEventsGestureRecognizer.
extension WKPointerTrackingGestureRecognizer {
    private func touchesOnView(of event: NSEvent) -> Set<NSTouch> {
        event.touches(matching: .touching, in: view)
    }

    @_implementationOnly
    open override func touchesBegan(with event: NSEvent) {
        super.touchesBegan(with: event)

        // For now, the pointer follows a single touch.
        let touches = touchesOnView(of: event)
        guard touches.count == 1, let touch = touches.first else {
            internalPressCancelled()
            return
        }

        internalPressBegan(atLocationInWindow: touch.location(in: nil), modifierFlags: event.modifierFlags)
    }

    @_implementationOnly
    open override func touchesMoved(with event: NSEvent) {
        super.touchesMoved(with: event)

        let touches = touchesOnView(of: event)
        guard touches.count == 1, let touch = touches.first else {
            return
        }

        internalPressMoved(toLocationInWindow: touch.location(in: nil), modifierFlags: event.modifierFlags)
    }

    @_implementationOnly
    open override func touchesEnded(with event: NSEvent) {
        super.touchesEnded(with: event)

        guard let touch = event.touches(matching: .ended, in: view).first else {
            return
        }

        internalPressEnded(atLocationInWindow: touch.location(in: nil), modifierFlags: event.modifierFlags)
    }

    @_implementationOnly
    open override func touchesCancelled(with event: NSEvent) {
        super.touchesCancelled(with: event)
        internalPressCancelled()
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
