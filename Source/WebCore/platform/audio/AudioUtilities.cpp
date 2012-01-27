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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioUtilities.h"
#include <wtf/MathExtras.h>

namespace WebCore {

namespace AudioUtilities {

float decibelsToLinear(float decibels)
{
    return powf(10, 0.05f * decibels);
}

float linearToDecibels(float linear)
{
    // It's not possible to calculate decibels for a zero linear value since it would be -Inf.
    // -1000.0 dB represents a very tiny linear value in case we ever reach this case.
    ASSERT(linear);
    if (!linear)
        return -1000;
        
    return 20 * log10f(linear);
}

float discreteTimeConstantForSampleRate(float timeConstant, float sampleRate)
{
    // hardcoded value is temporary build fix for Windows.
    // FIXME: replace hardcode 2.718282 with M_E until the correct MathExtras.h solution is determined.
    return 1 - powf(1 / 2.718282f, 1 / (sampleRate * timeConstant));
}

#if OS(WINDOWS) && COMPILER(MSVC) && !_M_IX86_FP
// When compiling with MSVC with x87 FPU instructions using 80-bit
// floats, we want very precise control over the arithmetic so that
// rounding is done according to the IEEE 754 specification for
// single- and double-precision floats. We want each operation to be
// done with specified arithmetic precision and rounding consistent
// with gcc, not extended to 80 bits automatically.
//
// These pragmas are equivalent to /fp:strict flag, but we only need
// it for the function here.  (Using fp:strict everywhere can have
// severe impact on floating point performance.)
#pragma float_control(push)
#pragma float_control(precise, on)
#pragma fenv_access(on)
#pragma float_control(except, on)
#endif

size_t timeToSampleFrame(double time, double sampleRate)
{
    // DO NOT CONSOLIDATE THESE ASSIGNMENTS INTO ONE! When compiling
    // with Visual Studio, these assignments force the rounding of
    // each operation according to IEEE 754, instead of leaving
    // intermediate results in 80-bit precision which is not
    // consistent with IEEE 754 double-precision rounding.
    double r = time * sampleRate;
    r += 0.5;
    return static_cast<size_t>(r);
}
#if OS(WINDOWS) && COMPILER(MSVC) && !_M_IX86_FP
// Restore normal floating-point semantics.
#pragma float_control(pop)
#endif
} // AudioUtilites

} // WebCore

#endif // ENABLE(WEB_AUDIO)
