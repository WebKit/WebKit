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

// A type to track directional scroll lock state. Drags within 20° of an axis lock to it; the ambiguous
// 20°-70° band stays unlocked so both axes flow (free diagonal).
//
// The axis decision stays open for the first `panHysteresis` of each gesture, and re-opens after a
// >= `pauseInterval` pause, but only a definite axis ever replaces an established lock: a user whose scroll is
// locked can transition to the other axis, not scroll freely around.
struct WKDirectionalScrollLockTracker {
    private var lockedAxis: Axis?
    private var lastLockedAxis: Axis?
    private var lastEventTime: TimeInterval = 0
    private var caughtMomentum = false
    private var uncommittedTranslation = CGSize.zero // Translation accumulated since the axis decision was last opened.

    init() {
    }

    // Takes one incremental (per-event) gesture delta and returns it with the locked-out axis zeroed,
    // or unchanged while unlocked.
    mutating func update(
        delta: CGSize,
        canScrollHorizontally: Bool,
        canScrollVertically: Bool,
        prefersUnlocked: Bool,
        time: TimeInterval
    ) -> CGSize {
        // A long gap between events without mouse up re-opens the axis decision, but deliberately
        // leaves any existing lock in place until a new definite axis replaces it below.
        if lastEventTime != 0, time - lastEventTime >= Constants.pauseInterval {
            uncommittedTranslation = .zero
        }
        lastEventTime = time

        // The axis decision is reconsidered on each event until the gesture commits to a direction.
        if hypot(uncommittedTranslation.width, uncommittedTranslation.height) < Constants.panHysteresis {
            uncommittedTranslation.width += delta.width
            uncommittedTranslation.height += delta.height

            // A drag on both axes locks only if its angle is within `lockAngle` of one of them; a drag on
            // a single axis locks to that axis outright. A zero translation carries no direction.
            let dx = abs(uncommittedTranslation.width)
            let dy = abs(uncommittedTranslation.height)

            var newAxis: Axis?
            if dx != 0, dy != 0 {
                let dragAngle = Angle2D.atan(dy / dx)
                if canScrollHorizontally, dragAngle <= Constants.lockAngle {
                    newAxis = .horizontal
                } else if canScrollVertically, dragAngle >= (.degrees(90) - Constants.lockAngle) {
                    newAxis = .vertical
                }
                // Otherwise the ambiguous 20°-70° band: stay unlocked so both axes flow.
            } else if dx != 0, canScrollHorizontally {
                newAxis = .horizontal
            } else if dy != 0, canScrollVertically {
                newAxis = .vertical
            }

            // Only a definite axis replaces an established lock; the ambiguous band leaves it alone.
            if let newAxis {
                lockedAxis = newAxis
            }
        }

        return Self.apply(prefersUnlocked ? nil : lockedAxis, to: delta)
    }

    // Applies the current lock to a velocity vector without mutating state, so a locked drag continues
    // its scroll on the locked axis rather than reintroducing diagonal drift.
    func filterVelocity(_ velocity: CGSize, prefersUnlocked: Bool) -> CGSize {
        Self.apply(prefersUnlocked ? nil : lockedAxis, to: velocity)
    }

    // A fresh gesture re-decides its axis, seeded from the previous drag's axis if it caught that
    // drag's momentum, so the user cannot change direction immediately. The decision stays open for
    // `panHysteresis`, so their actual direction can still supersede the seed.
    mutating func didStartGesture() {
        lockedAxis = exchange(&caughtMomentum, with: false) ? lastLockedAxis : nil
        lastEventTime = 0
        uncommittedTranslation = .zero
    }

    // Hands this gesture's final axis to `lastLockedAxis` for the next gesture to potentially catch.
    mutating func didEndGesture() {
        lastLockedAxis = lockedAxis
        lockedAxis = nil
        // A gesture that failed before it began never consumed the carry-over; drop it so it cannot
        // seed an unrelated later gesture.
        caughtMomentum = false
    }

    mutating func didCatchMomentum() {
        caughtMomentum = true
    }

    mutating func reset() {
        lockedAxis = nil
        lastLockedAxis = nil
        caughtMomentum = false
        lastEventTime = 0
        uncommittedTranslation = .zero
    }

    private static func apply(_ axis: Axis?, to vector: CGSize) -> CGSize {
        switch axis {
        case .horizontal:
            CGSize(width: vector.width, height: 0)
        case .vertical:
            CGSize(width: 0, height: vector.height)
        case nil:
            vector
        }
    }
}

extension WKDirectionalScrollLockTracker {
    fileprivate enum Axis {
        case horizontal
        case vertical
    }
}

extension WKDirectionalScrollLockTracker {
    fileprivate enum Constants {
        static let lockAngle: Angle2D = .degrees(20)
        static let pauseInterval: TimeInterval = 0.6
        static let panHysteresis: CGFloat = 10
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
