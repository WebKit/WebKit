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

import Foundation
import WebKit_Internal

#if canImport(SwiftUI)

@_weakLinked import SwiftUI

private let pitchSettleDuration: Float = 0.4
private let maxDecelerationDuration: Float = 2.0
private let decelerationDampening: Float = 0.95
private let velocityTerminationThreshold: Float = 0.0001

private let pitchSettleCurve = UnitCurve.bezier(startControlPoint: UnitPoint(x: 0.08, y: 0.6), endControlPoint: UnitPoint(x: 0.4, y: 1.0))

@objc
@implementation
extension WKStageModeOrbitSimulator {
    var currentYaw: Float { yaw }
    var currentPitch: Float { pitch }

    private var yaw: Float = 0
    private var pitch: Float = 0

    private var yawAtGestureStart: Float = 0
    private var pitchAtGestureStart: Float = 0
    private var previousDeltaX: Float = 0
    private var yawVelocity: Float = 0
    private var isDecelerating: Bool = false
    private var isPitchSettling: Bool = false
    private var pitchSettleStartPitch: Float = 0
    private var pitchSettleTime: Float = 0
    private var decelerationTime: Float = 0

    func gestureDidBegin() {
        yawAtGestureStart = yaw
        pitchAtGestureStart = pitch
        previousDeltaX = 0
        yawVelocity = 0
        isDecelerating = false
        isPitchSettling = false
    }

    func gestureDidUpdate(withDeltaX dx: Float, deltaY dy: Float) {
        yaw = yawAtGestureStart + dx
        pitch = pitchAtGestureStart + dy
        yawVelocity = dx - previousDeltaX
        previousDeltaX = dx
    }

    func gestureDidEnd() {
        isDecelerating = true
        isPitchSettling = true
        pitchSettleStartPitch = pitch
        pitchSettleTime = 0
        decelerationTime = 0
    }

    func step(withElapsedTime dt: Float) -> Bool {
        if isPitchSettling {
            pitchSettleTime += dt
            let t = min(pitchSettleTime / pitchSettleDuration, 1.0)
            pitch = pitchSettleStartPitch * (1.0 - Float(pitchSettleCurve.value(at: Double(t))))
            if t >= 1.0 {
                isPitchSettling = false
            }
        }

        if isDecelerating {
            decelerationTime += dt
            yaw += yawVelocity
            yawVelocity *= decelerationDampening
            if abs(yawVelocity) <= velocityTerminationThreshold || decelerationTime >= maxDecelerationDuration {
                isDecelerating = false
            }
        }

        return isPitchSettling || isDecelerating
    }
}

#else

@objc
@implementation
extension WKStageModeOrbitSimulator {
    var currentYaw: Float { 0 }
    var currentPitch: Float { 0 }
    func gestureDidBegin() {}
    func gestureDidUpdate(withDeltaX dx: Float, deltaY dy: Float) {}
    func gestureDidEnd() {}
    func step(withElapsedTime dt: Float) -> Bool { false }
}

#endif
