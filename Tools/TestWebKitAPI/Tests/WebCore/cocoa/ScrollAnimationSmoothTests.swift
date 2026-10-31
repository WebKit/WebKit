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

#if ENABLE_CXX_INTEROP

// FIXME: (rdar://191085916) Remove this import.
import JavaScriptCore_Private.Cxx
import Testing
import WebCore_Private

struct ScrollAnimationSmoothTests {
    struct Sample {
        let time: Duration
        let value: Double
    }

    // Sampled from a CASpringAnimation with mass 1, stiffness (2π / 0.6)², critical damping,
    // a duration of -durationForEpsilon:FLT_MIN, and a timing function of cubic-bezier(0, 0.2, 1, 1).
    private static let coreAnimationSamples = [
        Sample(time: .milliseconds(8), value: 0.080183),
        Sample(time: .milliseconds(16), value: 0.153033),
        Sample(time: .milliseconds(33), value: 0.287729),
        Sample(time: .milliseconds(50), value: 0.399335),
        Sample(time: .milliseconds(100), value: 0.632400),
        Sample(time: .milliseconds(150), value: 0.772445),
        Sample(time: .milliseconds(200), value: 0.857946),
        Sample(time: .milliseconds(300), value: 0.943574),
        Sample(time: .milliseconds(400), value: 0.977152),
        Sample(time: .milliseconds(500), value: 0.990611),
        Sample(time: .milliseconds(750), value: 0.998932),
        Sample(time: .milliseconds(1000), value: 0.999872),
        Sample(time: .milliseconds(1500), value: 0.999998),
    ]

    @Test(arguments: coreAnimationSamples)
    func springProgressMatchesCoreAnimation(sample: Sample) {
        let progress = springProgress(sample.time)

        // Core Animation solves the timing function less precisely near the start of the curve, where it is steepest,
        // so allow for a small difference.
        #expect(abs(progress - sample.value) <= 2e-4)
    }

    @Test
    func springProgressEndpoints() {
        #expect(abs(springProgress(.zero)) <= 1e-9)
        #expect(abs(springProgress(.seconds(1.6)) - 1) <= 1e-5)
        #expect(springProgress(.seconds(1.6)) == springProgress(.seconds(10)))
    }

    @Test
    func springProgressIsMonotonic() {
        var previousProgress = 0.0
        for milliseconds in 1...1600 {
            let progress = springProgress(.milliseconds(milliseconds))
            #expect(progress >= previousProgress)
            previousProgress = progress
        }
    }
}

private func springProgress(_ t: Duration) -> Double {
    unsafe WebCore.ScrollAnimationSmooth.springProgress(WTF.Seconds(t / .seconds(1)))
}

#endif // ENABLE_CXX_INTEROP
