/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatSize.h>
#include <WebCore/ScrollTypes.h>
#include <array>
#include <numbers>
#include <wtf/Platform.h>
#include <wtf/Seconds.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

WEBCORE_EXPORT FloatSize LEGACY_NODELETE unitVectorForScrollDirection(ScrollDirection);

struct KeyboardScroll {
    FloatSize offset; // Points per increment.
    FloatSize maximumVelocity; // Points per second.
    FloatSize force;

    ScrollGranularity granularity { ScrollGranularity::Line };
    ScrollDirection direction { ScrollDirection::ScrollUp };

    friend bool operator==(const KeyboardScroll&, const KeyboardScroll&) = default;
};

struct KeyboardScrollParameters {
    const float springMass;
    const float springStiffness;
    const float springDamping;

    const float maximumVelocityMultiplier;
    const float timeToMaximumVelocity;

    const float rubberBandForce;

    static constexpr KeyboardScrollParameters parameters()
    {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
        return {
            .springMass = 1, 
            .springStiffness = 175, 
            .springDamping = 20, 
            .maximumVelocityMultiplier = 25, 
            .timeToMaximumVelocity = 0.2, 
            .rubberBandForce = 3000 
        };
#else
        return {
            .springMass = 1, 
            .springStiffness = 109, 
            .springDamping = 20, 
            .maximumVelocityMultiplier = 25, 
            .timeToMaximumVelocity = 1.0, 
            .rubberBandForce = 5000 
        };
#endif
    }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const KeyboardScroll&);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const KeyboardScrollParameters&);

// The spring for animated scrolls to an edge of the document: a critically damped spring with a response of 0.6s,
// with time warped by cubic-bezier(0, 0.2, 1, 1), as in UIKit's smooth deceleration animation.
namespace DocumentEdgeScrollSpring {
inline constexpr double response = 0.6;
inline constexpr double mass = 1;
inline constexpr double angularFrequency = 2 * std::numbers::pi / response;
inline constexpr double stiffness = angularFrequency * angularFrequency * mass;
// What -[CASpringAnimation durationForEpsilon:FLT_MIN] returns for this spring.
inline constexpr Seconds duration = 1.6_s;
inline constexpr std::array<double, 4> timingFunctionControlPoints { 0, 0.2, 1, 1 };
}

} // namespace WebCore
