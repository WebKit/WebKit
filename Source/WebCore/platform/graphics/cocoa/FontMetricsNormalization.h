/*
 * Copyright (C) 2017-2023 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WebCore {

// These values were calculated by performing a linear regression on the CSS weights/widths/slopes and Core Text weights/widths/slopes of San Francisco.
// FIXME: <rdar://problem/31312602> Get the real values from Core Text.
inline float normalizeGXWeight(float value)
{
    return 523.7 * value - 109.3;
}

// These values were experimentally gathered from the various named weights of San Francisco.
static struct {
    float ctWeight;
    float cssWeight;
} keyframes[] = {
    { -0.8, 30 },
    { -0.4, 274 },
    { 0, 400 },
    { 0.23, 510 },
    { 0.3, 590 },
    { 0.4, 700 },
    { 0.56, 860 },
    { 0.62, 1000 },
};
static_assert(std::size(keyframes) > 0);

inline float normalizeCTWeight(float value)
{
    if (value < keyframes[0].ctWeight)
        return keyframes[0].cssWeight;
    for (size_t i = 0; i < std::size(keyframes) - 1; ++i) {
        auto& before = keyframes[i];
        auto& after = keyframes[i + 1];
        if (value >= before.ctWeight && value <= after.ctWeight) {
            float ratio = (value - before.ctWeight) / (after.ctWeight - before.ctWeight);
            return ratio * (after.cssWeight - before.cssWeight) + before.cssWeight;
        }
    }
    return keyframes[std::size(keyframes) - 1].cssWeight;
}

inline float normalizeSlope(float value)
{
    return value * 300;
}

inline float denormalizeGXWeight(float value)
{
    return (value + 109.3) / 523.7;
}

inline float denormalizeCTWeight(float value)
{
    if (value < keyframes[0].cssWeight)
        return keyframes[0].ctWeight;
    for (size_t i = 0; i < std::size(keyframes) - 1; ++i) {
        auto& before = keyframes[i];
        auto& after = keyframes[i + 1];
        if (value >= before.cssWeight && value <= after.cssWeight) {
            float ratio = (value - before.cssWeight) / (after.cssWeight - before.cssWeight);
            return ratio * (after.ctWeight - before.ctWeight) + before.ctWeight;
        }
    }
    return keyframes[std::size(keyframes) - 1].ctWeight;
}

inline float denormalizeSlope(float value)
{
    return value / 300;
}

inline float denormalizeVariationWidth(float value)
{
    if (value <= 125)
        return value / 100;
    if (value <= 150)
        return (value + 125) / 200;
    return (value + 400) / 400;
}

inline float normalizeVariationWidth(float value)
{
    if (value <= 1.25)
        return value * 100;
    if (value <= 1.375)
        return value * 200 - 125;
    return value * 400 - 400;
}

} // namespace WebCore
