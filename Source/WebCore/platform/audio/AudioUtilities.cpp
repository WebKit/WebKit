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
#include <random>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/WeakRandom.h>

namespace WebCore {

namespace AudioUtilities {

double discreteTimeConstantForSampleRate(double timeConstant, double sampleRate)
{
    return 1 - exp(-1 / (sampleRate * timeConstant));
}

size_t timeToSampleFrame(double time, double sampleRate, SampleFrameRounding rounding)
{
    // To compute the desired frame number, we pretend we're actually running the
    // context at a much higher sample rate (by a factor of |oversampleFactor|).
    // Round this to get the nearest frame number at the higher rate. Then
    // convert back to the original rate to get a new frame number that may not be
    // an integer. Then use the specified |rounding_mode| to round this to the
    // integer frame number that we need.
    //
    // Doing this partially solves the issue where Fs * (k / Fs) != k when doing
    // floating point arithmtic for integer k and Fs is the sample rate. By
    // oversampling and rounding, we'll get k back most of the time.
    //
    // The oversampling factor MUST be a power of two so as not to introduce
    // additional round-off in computing the oversample frame number.
    constexpr double oversampleFactor = 1024;
    double frame = std::round(time * sampleRate * oversampleFactor) / oversampleFactor;

    switch (rounding) {
    case SampleFrameRounding::Nearest:
        frame = std::round(frame);
        break;
    case SampleFrameRounding::Down:
        frame = std::floor(frame);
        break;
    case SampleFrameRounding::Up:
        frame = std::ceil(frame);
        break;
    }

    // Just return the largest possible size_t value if necessary.
    if (frame >= static_cast<double>(std::numeric_limits<size_t>::max()))
        return std::numeric_limits<size_t>::max();

    return static_cast<size_t>(frame);
}

void applyNoise(float* values, size_t numberOfElementsToProcess, float standardDeviation)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::normal_distribution<float> distribution(1, standardDeviation);

    HashMap<float, float> noiseMultipliers;
    auto computeNoiseMultiplier = [&](float rawValue) {
        if (!noiseMultipliers.isValidKey(rawValue))
            return distribution(generator);

        auto result = noiseMultipliers.ensure(rawValue, [&] {
            return distribution(generator);
        }).iterator->value;

        static constexpr auto maxNoiseMultiplierMapSize = 250000;
        if (noiseMultipliers.size() >= maxNoiseMultiplierMapSize)
            noiseMultipliers.remove(noiseMultipliers.random());

        return result;
    };

    for (size_t i = 0; i < numberOfElementsToProcess; ++i)
        values[i] *= computeNoiseMultiplier(values[i]);
}

} // AudioUtilites

} // WebCore

#endif // ENABLE(WEB_AUDIO)
