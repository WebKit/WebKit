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
private import CoreGraphics

struct WKFastScrollTracker {
    private var multiplier: CGFloat = 1
    private var endTime: TimeInterval = 0
    private var count = 0
    private var lastHorizontalDirection: Direction = .negative
    private var lastVerticalDirection: Direction = .negative

    // Set when this gesture begins atop a live (decelerating) momentum. Unlike _caughtDeceleratingScroll it
    // is not cleared by the async didEndSyntheticMomentumScrolling callback, so it remains valid until the
    // gesture ends and consumes it.
    private var caughtMomentum = false

    private var gestureStartLocation = CGPoint.zero

    init() {
    }

    // Advances the swipe chain for a swipe that just ended and returns the momentum multiplier to apply
    // to it (1 = unaccelerated): expire the chain if stale or not continuing a live momentum, break it on
    // a direction reversal or too-slow swipe, then (once enough same-direction swipes have stacked up)
    // scale the multiplier by this swipe's drag distance and how deep it is in the chain.
    mutating func update(location: CGPoint, velocity: CGSize, time: TimeInterval) -> Double {
        let speed = hypot(velocity.width, velocity.height)

        // Whether this gesture began atop a decelerating momentum (consumed here).
        let currentCaughtMomentum = exchange(&caughtMomentum, with: false)

        // Chain-start reset: drop any accumulated acceleration if this swipe did not catch a decelerating
        // momentum, or if too long elapsed since the previous fast scroll.
        if !currentCaughtMomentum || .seconds(time - endTime) > Constants.timeout {
            multiplier = 1
            count = 0
        }

        var startMultiplier = multiplier

        // Reset if a moving axis reversed direction, or the swipe was too slow.
        let directionChanged =
            (velocity.width != 0 && lastHorizontalDirection != Direction(velocity.width))
            || (velocity.height != 0 && lastVerticalDirection != Direction(velocity.height))

        if directionChanged || speed < Constants.minimumDragSpeed {
            multiplier = 1
            count = 0
            startMultiplier = 1
        }

        if velocity.width != 0 {
            lastHorizontalDirection = Direction(velocity.width)
        }
        if velocity.height != 0 {
            lastVerticalDirection = Direction(velocity.height)
        }

        // Compute this swipe's multiplier from the pre-increment count. Acceleration only engages once more
        // than two consecutive same-direction swipes have accumulated, so the first swipe that is actually
        // boosted is the 4th.
        var currentMultiplier: CGFloat = 1
        if count > 2 {
            let endLocation = location
            let dragDistance = hypot(endLocation.x - gestureStartLocation.x, endLocation.y - gestureStartLocation.y)
            // rampFactor grows 1.0, 1.5, 2.0, ... with each further swipe, scaling the (distance-capped)
            // boost so deeper chains accelerate harder. speedup is added onto the previous multiplier, so
            // the effect compounds across swipes, capped at maximumMultiplier.
            let rampFactor = 1 + CGFloat(count - 3) * 0.5
            let speedup = min(dragDistance / Constants.distanceScale, Constants.maximumSpeedup) * rampFactor
            currentMultiplier = min(startMultiplier + speedup, Constants.maximumMultiplier)
        }
        multiplier = currentMultiplier

        // Advance the count for the next swipe, or reset if this one ended too slowly to keep the chain.
        if speed < Constants.minimumEndSpeed {
            count = 0
            multiplier = 1
        } else {
            count += 1
        }

        endTime = time

        // `multiplier` may have just been reset to 1 for the *next* swipe; this swipe still returns the
        // value it computed above.
        return currentMultiplier
    }

    // Called when a new gesture interrupts a still-decelerating momentum (the signal that this swipe is
    // continuing an in-flight fling rather than starting from rest). The next `update(...)` consumes it;
    // without it, that update treats the swipe as the start of a new (unaccelerated) chain.
    mutating func didCatchMomentum() {
        caughtMomentum = true
    }

    mutating func didStartGesture(atLocation location: CGPoint) {
        gestureStartLocation = location
    }

    mutating func reset() {
        count = 0
        multiplier = 1
        endTime = 0
        lastHorizontalDirection = .negative
        lastVerticalDirection = .negative
        caughtMomentum = false
    }
}

extension WKFastScrollTracker {
    fileprivate enum Direction {
        case negative
        case positive

        init(_ value: CGFloat) {
            self = value < 0 ? .negative : .positive
        }
    }
}

extension WKFastScrollTracker {
    fileprivate enum Constants {
        static let minimumEndSpeed: CGFloat = 600
        static let minimumDragSpeed: CGFloat = 130
        static let distanceScale: CGFloat = 240
        static let maximumSpeedup: CGFloat = 0.9
        static let maximumMultiplier: CGFloat = 16
        static let timeout: Duration = .seconds(1)
    }
}

#endif // HAVE_APPKIT_GESTURES_SUPPORT
