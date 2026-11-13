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

import AppKit
import WebKit_Internal
import AppKit_Private.NSPanGestureRecognizer_Private
private import WebCore_Private

final class WKPanGestureRecognizer: NSPanGestureRecognizer {
    /// FXIME: <rdar://188400680> Adopt NSPanGestureRecognizer._defaultTouchHysteresis once it is not IPI.
    private static let touchHysteresis: CGFloat = 10

    /// The widest angle away from a permitted axis at which a pan is still treated as ambiguous.
    ///
    /// We match `maximumBeginAngleFromScrollableAxis` from UIScrollViewPanGestureRecognizer.
    private static let maximumAmbiguousDragAngle: Angle2D = .degrees(60)

    private static let stalenessWindow: TimeInterval = 0.2

    private weak var webView: WKWebView?

    var gestureStartTime: TimeInterval?
    var gestureStartLocationInWindow: NSPoint = .zero

    var lastMovementTime: TimeInterval?
    var lastMovementLocationInWindow: NSPoint = .zero

    var reachedChangedState = false

    init(webView: WKWebView, target: Any?, action: Selector?) {
        self.webView = webView
        super.init(target: target, action: action)
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @objc
    public required dynamic init?(coder: NSCoder) {
        super.init(coder: coder)
    }

    // swift-format-ignore: AlwaysUseLowerCamelCase
    func wk_velocity(in view: NSView?) -> NSPoint {
        if let lastMovementTime, timestamp - lastMovementTime >= Self.stalenessWindow {
            return .zero
        }

        let appKitVelocity = velocity(in: view)

        // Prefer AppKit's velocity if available.
        // If we've seen "changed" events and AppKit gave us zero, then the velocity is legitimately zero.
        if reachedChangedState || appKitVelocity != .zero {
            return appKitVelocity
        }

        // If we have seen less than two events, we can't work out a velocity at all.
        guard let gestureStartTime, let lastMovementTime, lastMovementTime > gestureStartTime
        else {
            return .zero
        }

        let start = view.map { $0.convert(gestureStartLocationInWindow, from: nil) } ?? gestureStartLocationInWindow
        let end = view.map { $0.convert(lastMovementLocationInWindow, from: nil) } ?? lastMovementLocationInWindow

        let duration = lastMovementTime - gestureStartTime
        return NSPoint(x: (end.x - start.x) / duration, y: (end.y - start.y) / duration)
    }
}

// MARK: Helpers

extension WKPanGestureRecognizer {
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

    private func failIfCommittedToRestrictedAxis() {
        guard state == .possible, hasCommittedToRestrictedAxis else {
            return
        }

        state = .failed
    }
}

// MARK: Overrides

extension WKPanGestureRecognizer {
    override func reset() {
        gestureStartTime = nil
        gestureStartLocationInWindow = .zero
        lastMovementTime = nil
        lastMovementLocationInWindow = .zero
        reachedChangedState = false
        super.reset()
    }

    // This selector is internal-only in AppKit.
    // swift-format-ignore: AlwaysUseLowerCamelCase
    @objc(_allowedDuringDnDRestrictions)
    func wk_allowedDuringDnDRestrictions() -> Bool {
        false
    }

    #if canImport(AppKit, _version: "2759")
    // swift-format-ignore: NoLeadingUnderscores
    override func _shouldRecognize(forDelta delta: NSPoint) -> NSPanShouldRecognizeResponse {
        guard let webView, let page = webView._protectedPage().get() else {
            return .fail
        }

        guard webView.enclosingScrollView != nil else {
            return .recognize
        }

        let locationInView = location(in: webView)
        let pinnedState = page.pinnedStateIncludingAncestorsAtPoint(.init(locationInView))

        // Safety: Accessing these functions on `pinnedState` is safe because a copy is immediately made,
        // and the functions's return value's lifetime is bound to `pinnedState` anyways.
        // FIXME: (rdar://145054011) Remove `unsafe` when possible.

        if abs(delta.x) > abs(delta.y) {
            if unsafe delta.x < 0 && pinnedState.__rightUnsafe().pointee {
                return .fail
            }
            if unsafe delta.x > 0 && pinnedState.__leftUnsafe().pointee {
                return .fail
            }
        } else {
            if unsafe delta.y < 0 && pinnedState.__topUnsafe().pointee {
                return .fail
            }
            if unsafe delta.y > 0 && pinnedState.__bottomUnsafe().pointee {
                return .fail
            }
        }

        return .recognize
    }
    #endif // canImport(AppKit, _version: "2759")

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
}

#endif
