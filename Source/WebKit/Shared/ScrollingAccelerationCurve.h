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

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)

#include <WebCore/FloatSize.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class NativeWebWheelEvent;

class ScrollingAccelerationCurve {
public:
    ScrollingAccelerationCurve(float gainLinear, float gainParabolic, float gainCubic, float gainQuartic, float tangentSpeedLinear, float tangentSpeedParabolicRoot, float resolution, float frameRate);

    static std::optional<ScrollingAccelerationCurve> fromNativeWheelEvent(const NativeWebWheelEvent&);

    static ScrollingAccelerationCurve interpolate(const ScrollingAccelerationCurve& from, const ScrollingAccelerationCurve& to, float amount);

    float accelerationFactor(float);
    float frameRate() const { return m_parameters.frameRate; }

    void encode(IPC::Encoder&) const;
    static std::optional<ScrollingAccelerationCurve> decode(IPC::Decoder&);

    bool operator==(const ScrollingAccelerationCurve& other) const
    {
        return m_parameters.gainLinear == other.m_parameters.gainLinear
            && m_parameters.gainParabolic == other.m_parameters.gainParabolic
            && m_parameters.gainCubic == other.m_parameters.gainCubic
            && m_parameters.gainQuartic == other.m_parameters.gainQuartic
            && m_parameters.tangentSpeedLinear == other.m_parameters.tangentSpeedLinear
            && m_parameters.tangentSpeedParabolicRoot == other.m_parameters.tangentSpeedParabolicRoot
            && m_parameters.resolution == other.m_parameters.resolution
            && m_parameters.frameRate == other.m_parameters.frameRate;
    }
    
private:
    friend TextStream& operator<<(TextStream&, const ScrollingAccelerationCurve&);

    void computeIntermediateValuesIfNeeded();

    float evaluateQuartic(float) const;

    struct {
        float gainLinear { 0 };
        float gainParabolic { 0 };
        float gainCubic { 0 };
        float gainQuartic { 0 };
        float tangentSpeedLinear { 0 };
        float tangentSpeedParabolicRoot { 0 };

        // FIXME: Resolution and frame rate are not technically properties
        // of the curve, just required to use it; they should be plumbed separately.
        float resolution { 0 };
        float frameRate { 0 };
    } m_parameters;

    struct ComputedIntermediateValues {
        float tangentStartX { 0 };
        float tangentSlope { 0 };
        float tangentIntercept { 0 };
        float falloffStartX { 0 };
        float falloffSlope { 0 };
        float falloffIntercept { 0 };
    };

    std::optional<ComputedIntermediateValues> m_intermediates;
};

TextStream& operator<<(TextStream&, const ScrollingAccelerationCurve&);

} // namespace WebKit

#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER)
