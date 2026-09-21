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

@objc
@implementation
extension WKPressGestureRecognizer {
    @nonobjc
    private var lastDeliveredLocationInWindow: CGPoint = .zero

    @_implementationOnly
    open override func reset() {
        lastDeliveredLocationInWindow = .zero
        super.reset()
    }

    func beginReportingMovement(fromWindowLocation location: CGPoint) {
        lastDeliveredLocationInWindow = location
    }

    func eventReportingMovement(_ event: NSEvent, atWindowLocation location: CGPoint) -> NSEvent {
        guard let cgEvent = event.cgEvent else {
            return event
        }

        let travel = CGSize(
            width: location.x - lastDeliveredLocationInWindow.x,
            height: location.y - lastDeliveredLocationInWindow.y
        )

        let delta = CGSize(width: travel.width.rounded(), height: travel.height.rounded())

        cgEvent.setIntegerValueField(.mouseEventDeltaX, value: Int64(delta.width))
        cgEvent.setIntegerValueField(.mouseEventDeltaY, value: Int64(-delta.height))

        cgEvent.setDoubleValueField(.eventUnacceleratedPointerMovementX, value: delta.width)
        cgEvent.setDoubleValueField(.eventUnacceleratedPointerMovementY, value: -delta.height)

        guard let eventWithMovement = NSEvent(cgEvent: cgEvent) else {
            return event
        }

        lastDeliveredLocationInWindow.x += delta.width
        lastDeliveredLocationInWindow.y += delta.height

        return eventWithMovement
    }

    @_implementationOnly
    open override func shouldRequireFailure(of gestureRecognizer: NSGestureRecognizer) -> Bool {
        // The inherited implementation makes a press wait on any press with a longer `minimumPressDuration`.
        // WebKit's presses are disambiguated by the content under the cursor, not by duration, so that
        // would only serialize them, holding the shorter drag behind the longer secondary click.

        false
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
