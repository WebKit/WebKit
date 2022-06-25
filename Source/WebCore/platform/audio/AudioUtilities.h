/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioUtilities_h
#define AudioUtilities_h

#include <cmath>

namespace WebCore {

namespace AudioUtilities {

// https://www.w3.org/TR/webaudio/#render-quantum-size
constexpr size_t renderQuantumSize = 128;

// Standard functions for converting to and from decibel values from linear.
float linearToDecibels(float);
float decibelsToLinear(float);

// timeConstant is the time it takes a first-order linear time-invariant system
// to reach the value 1 - 1/e (around 63.2%) given a step input response.
// discreteTimeConstantForSampleRate() will return the discrete time-constant for the specific sampleRate.
double discreteTimeConstantForSampleRate(double timeConstant, double sampleRate);

// How to do rounding when converting time to sample frame.
enum class SampleFrameRounding {
    Nearest,
    Down,
    Up
};

// Convert the time to a sample frame at the given sample rate.
size_t timeToSampleFrame(double time, double sampleRate, SampleFrameRounding = SampleFrameRounding::Nearest);

inline float linearToDecibels(float linear)
{
    ASSERT(linear >= 0);
    return 20 * log10f(linear);
}

inline float decibelsToLinear(float decibels)
{
    return powf(10, 0.05f * decibels);
}

} // AudioUtilites

} // WebCore

#endif // AudioUtilities_h
