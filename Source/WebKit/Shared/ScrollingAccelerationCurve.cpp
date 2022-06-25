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

#include "config.h"
#include "ScrollingAccelerationCurve.h"

#if ENABLE(MOMENTUM_EVENT_DISPATCHER)

#include "ArgumentCoders.h"
#include "WebCoreArgumentCoders.h"
#include <wtf/text/TextStream.h>

namespace WebKit {

ScrollingAccelerationCurve::ScrollingAccelerationCurve(float gainLinear, float gainParabolic, float gainCubic, float gainQuartic, float tangentSpeedLinear, float tangentSpeedParabolicRoot, float resolution, float frameRate)
    : m_parameters { gainLinear, gainParabolic, gainCubic, gainQuartic, tangentSpeedLinear, tangentSpeedParabolicRoot, resolution, frameRate }
{
}

ScrollingAccelerationCurve ScrollingAccelerationCurve::interpolate(const ScrollingAccelerationCurve& from, const ScrollingAccelerationCurve& to, float amount)
{
    auto interpolate = [&] (float a, float b) -> float {
        return a + amount * (b - a);
    };

    auto gainLinear = interpolate(from.m_parameters.gainLinear, to.m_parameters.gainLinear);
    auto gainParabolic = interpolate(from.m_parameters.gainParabolic, to.m_parameters.gainParabolic);
    auto gainCubic = interpolate(from.m_parameters.gainCubic, to.m_parameters.gainCubic);
    auto gainQuartic = interpolate(from.m_parameters.gainQuartic, to.m_parameters.gainQuartic);

    auto tangentSpeedLinear = interpolate(from.m_parameters.tangentSpeedLinear, to.m_parameters.tangentSpeedLinear);
    auto tangentSpeedParabolicRoot = interpolate(from.m_parameters.tangentSpeedParabolicRoot, to.m_parameters.tangentSpeedParabolicRoot);

    return { gainLinear, gainParabolic, gainCubic, gainQuartic, tangentSpeedLinear, tangentSpeedParabolicRoot, from.m_parameters.resolution, from.m_parameters.frameRate };
}

void ScrollingAccelerationCurve::computeIntermediateValuesIfNeeded()
{
    if (m_intermediates)
        return;
    m_intermediates = ComputedIntermediateValues { };

    float tangentInitialY;
    m_intermediates->tangentStartX = std::numeric_limits<float>::max();
    m_intermediates->falloffStartX = std::numeric_limits<float>::max();

    auto quarticSlope = [&](float x) {
        return (4 * std::pow(x, 3) * std::pow(m_parameters.gainQuartic, 4)) + (3 * std::pow(x, 2) * std::pow(m_parameters.gainCubic, 3)) + (2 * x * std::pow(m_parameters.gainParabolic, 2)) + m_parameters.gainLinear;
    };

    if (m_parameters.tangentSpeedLinear) {
        tangentInitialY = evaluateQuartic(m_parameters.tangentSpeedLinear);
        m_intermediates->tangentSlope = quarticSlope(m_parameters.tangentSpeedLinear);
        m_intermediates->tangentIntercept = tangentInitialY - m_intermediates->tangentSlope * m_parameters.tangentSpeedLinear;
        m_intermediates->tangentStartX = m_parameters.tangentSpeedLinear;

        if (m_parameters.tangentSpeedParabolicRoot) {
            float y1 = m_intermediates->tangentSlope * m_parameters.tangentSpeedParabolicRoot + m_intermediates->tangentIntercept;
            m_intermediates->falloffSlope = 2 * y1 * m_intermediates->tangentSlope;
            m_intermediates->falloffIntercept = std::pow(y1, 2) - m_intermediates->falloffSlope * m_parameters.tangentSpeedParabolicRoot;
            m_intermediates->falloffStartX = m_parameters.tangentSpeedParabolicRoot;
        }
    } else if (m_parameters.tangentSpeedParabolicRoot) {
        tangentInitialY = evaluateQuartic(m_parameters.tangentSpeedParabolicRoot);
        m_intermediates->falloffSlope = quarticSlope(m_parameters.tangentSpeedParabolicRoot);
        m_intermediates->falloffIntercept = std::pow(tangentInitialY, 2) - m_intermediates->falloffSlope *  m_parameters.tangentSpeedParabolicRoot;
        m_intermediates->tangentStartX = m_parameters.tangentSpeedParabolicRoot;
    }
}

float ScrollingAccelerationCurve::evaluateQuartic(float x) const
{
    return std::pow(m_parameters.gainQuartic * x, 4) + std::pow(m_parameters.gainCubic * x, 3) + std::pow(m_parameters.gainParabolic * x, 2) + m_parameters.gainLinear * x;
}

float ScrollingAccelerationCurve::accelerationFactor(float value)
{
    constexpr float frameRate = 67.f; // Why is this 67 and not 60?
    constexpr float cursorScale = 96.f / frameRate;

    computeIntermediateValuesIfNeeded();

    float multiplier;
    float deviceScale = m_parameters.resolution / frameRate;
    value /= deviceScale;

    if (value <= m_intermediates->tangentStartX)
        multiplier = evaluateQuartic(value);
    else if (value <= m_intermediates->falloffStartX && m_intermediates->tangentStartX == m_parameters.tangentSpeedLinear)
        multiplier = m_intermediates->tangentSlope * value + m_intermediates->tangentIntercept;
    else
        multiplier = std::sqrt(m_intermediates->falloffSlope * value + m_intermediates->falloffIntercept);

    return multiplier * cursorScale;
}

void ScrollingAccelerationCurve::encode(IPC::Encoder& encoder) const
{
    encoder << m_parameters.gainLinear;
    encoder << m_parameters.gainParabolic;
    encoder << m_parameters.gainCubic;
    encoder << m_parameters.gainQuartic;

    encoder << m_parameters.tangentSpeedLinear;
    encoder << m_parameters.tangentSpeedParabolicRoot;

    encoder << m_parameters.resolution;
    encoder << m_parameters.frameRate;
}

std::optional<ScrollingAccelerationCurve> ScrollingAccelerationCurve::decode(IPC::Decoder& decoder)
{
    float gainLinear;
    if (!decoder.decode(gainLinear))
        return std::nullopt;
    float gainParabolic;
    if (!decoder.decode(gainParabolic))
        return std::nullopt;
    float gainCubic;
    if (!decoder.decode(gainCubic))
        return std::nullopt;
    float gainQuartic;
    if (!decoder.decode(gainQuartic))
        return std::nullopt;

    float tangentSpeedLinear;
    if (!decoder.decode(tangentSpeedLinear))
        return std::nullopt;
    float tangentSpeedParabolicRoot;
    if (!decoder.decode(tangentSpeedParabolicRoot))
        return std::nullopt;

    float resolution;
    if (!decoder.decode(resolution))
        return std::nullopt;
    float frameRate;
    if (!decoder.decode(frameRate))
        return std::nullopt;

    return { { gainLinear, gainParabolic, gainCubic, gainQuartic, tangentSpeedLinear, tangentSpeedParabolicRoot, resolution, frameRate } };
}

TextStream& operator<<(TextStream& ts, const ScrollingAccelerationCurve& curve)
{
    TextStream::GroupScope group(ts);

    ts << "ScrollingAccelerationCurve";

    ts.dumpProperty("gainLinear", curve.m_parameters.gainLinear);
    ts.dumpProperty("gainParabolic", curve.m_parameters.gainParabolic);
    ts.dumpProperty("gainCubic", curve.m_parameters.gainCubic);
    ts.dumpProperty("gainQuartic", curve.m_parameters.gainQuartic);
    ts.dumpProperty("tangentSpeedLinear", curve.m_parameters.tangentSpeedLinear);
    ts.dumpProperty("tangentSpeedParabolicRoot", curve.m_parameters.tangentSpeedParabolicRoot);
    ts.dumpProperty("resolution", curve.m_parameters.resolution);
    ts.dumpProperty("frameRate", curve.m_parameters.frameRate);

    return ts;
}

#if !PLATFORM(MAC)

std::optional<ScrollingAccelerationCurve> ScrollingAccelerationCurve::fromNativeWheelEvent(const NativeWebWheelEvent&)
{
    return std::nullopt;
}

#endif

} // namespace WebKit

#endif // ENABLE(MOMENTUM_EVENT_DISPATCHER)
