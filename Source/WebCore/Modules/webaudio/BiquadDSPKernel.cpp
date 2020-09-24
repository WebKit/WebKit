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

#include "BiquadDSPKernel.h"

#include "AudioUtilities.h"
#include "Biquad.h"
#include "BiquadProcessor.h"
#include "FloatConversion.h"
#include <limits.h>
#include <wtf/Vector.h>

namespace WebCore {

// FIXME: As a recursive linear filter, depending on its parameters, a biquad filter can have
// an infinite tailTime. In practice, Biquad filters do not usually (except for very high resonance values) 
// have a tailTime of longer than approx. 200ms. This value could possibly be calculated based on the
// settings of the Biquad.
static const double MaxBiquadDelayTime = 0.2;

static bool hasConstantValues(float* values, int framesToProcess)
{
    // FIXME: Use SIMD to optimize this. This would speed up processing by a factor of 4
    // because we can process 4 floats at a time.
    float value = values[0];
    for (int k = 1; k < framesToProcess; ++k) {
        if (values[k] != value)
            return false;
    }
    return true;
}

void BiquadDSPKernel::updateCoefficientsIfNecessary(size_t framesToProcess)
{
    if (biquadProcessor()->filterCoefficientsDirty()) {
        if (biquadProcessor()->hasSampleAccurateValues() && biquadProcessor()->shouldUseARate()) {
            float cutoffFrequency[AudioUtilities::renderQuantumSize];
            float q[AudioUtilities::renderQuantumSize];
            float gain[AudioUtilities::renderQuantumSize];
            float detune[AudioUtilities::renderQuantumSize]; // in Cents

            ASSERT(framesToProcess <= AudioUtilities::renderQuantumSize);

            biquadProcessor()->parameter1().calculateSampleAccurateValues(cutoffFrequency, framesToProcess);
            biquadProcessor()->parameter2().calculateSampleAccurateValues(q, framesToProcess);
            biquadProcessor()->parameter3().calculateSampleAccurateValues(gain, framesToProcess);
            biquadProcessor()->parameter4().calculateSampleAccurateValues(detune, framesToProcess);

            // If all the values are actually constant for this render (or the
            // automation rate is "k-rate" for all of the AudioParams), we don't need
            // to compute filter coefficients for each frame since they would be the
            // same as the first.
            bool isConstant = hasConstantValues(cutoffFrequency, framesToProcess)
                && hasConstantValues(q, framesToProcess)
                && hasConstantValues(gain, framesToProcess)
                && hasConstantValues(detune, framesToProcess);

            updateCoefficients(isConstant ? 1 : framesToProcess, cutoffFrequency, q, gain, detune);
        } else {
            float cutoffFrequency = biquadProcessor()->parameter1().finalValue();
            float q = biquadProcessor()->parameter2().finalValue();
            float gain = biquadProcessor()->parameter3().finalValue();
            float detune = biquadProcessor()->parameter4().finalValue();
            updateCoefficients(1, &cutoffFrequency, &q, &gain, &detune);
        }
    }
}

void BiquadDSPKernel::updateCoefficients(size_t numberOfFrames, const float* cutoffFrequency, const float* q, const float* gain, const float* detune)
{
    // Convert from Hertz to normalized frequency 0 -> 1.
    double nyquist = this->nyquist();

    m_biquad.setHasSampleAccurateValues(numberOfFrames > 1);

    for (size_t k = 0; k < numberOfFrames; ++k) {
        double normalizedFrequency = cutoffFrequency[k] / nyquist;

        // Offset frequency by detune.
        if (detune[k]) {
            // Detune multiplies the frequency by 2^(detune[k] / 1200).
            normalizedFrequency *= std::exp2(detune[k] / 1200);
        }

        // Configure the biquad with the new filter parameters for the appropriate
        // type of filter.
        switch (biquadProcessor()->type()) {
        case BiquadFilterType::Lowpass:
            m_biquad.setLowpassParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Highpass:
            m_biquad.setHighpassParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Bandpass:
            m_biquad.setBandpassParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Lowshelf:
            m_biquad.setLowShelfParams(k, normalizedFrequency, gain[k]);
            break;

        case BiquadFilterType::Highshelf:
            m_biquad.setHighShelfParams(k, normalizedFrequency, gain[k]);
            break;

        case BiquadFilterType::Peaking:
            m_biquad.setPeakingParams(k, normalizedFrequency, q[k], gain[k]);
            break;

        case BiquadFilterType::Notch:
            m_biquad.setNotchParams(k, normalizedFrequency, q[k]);
            break;

        case BiquadFilterType::Allpass:
            m_biquad.setAllpassParams(k, normalizedFrequency, q[k]);
            break;
        }
    }

    ASSERT(numberOfFrames);
    updateTailTime(numberOfFrames - 1);
}

void BiquadDSPKernel::process(const float* source, float* destination, size_t framesToProcess)
{
    ASSERT(source && destination && biquadProcessor());
    
    // Recompute filter coefficients if any of the parameters have changed.
    // FIXME: as an optimization, implement a way that a Biquad object can simply copy its internal filter coefficients from another Biquad object.
    // Then re-factor this code to only run for the first BiquadDSPKernel of each BiquadProcessor.

    updateCoefficientsIfNecessary(framesToProcess);

    m_biquad.process(source, destination, framesToProcess);
}

void BiquadDSPKernel::getFrequencyResponse(unsigned nFrequencies, const float* frequencyHz, float* magResponse, float* phaseResponse)
{
    bool isGood = nFrequencies > 0 && frequencyHz && magResponse && phaseResponse;
    ASSERT(isGood);
    if (!isGood)
        return;

    Vector<float> frequency(nFrequencies);

    double nyquist = this->nyquist();

    // Convert from frequency in Hz to normalized frequency (0 -> 1),
    // with 1 equal to the Nyquist frequency.
    for (unsigned k = 0; k < nFrequencies; ++k)
        frequency[k] = frequencyHz[k] / nyquist;

    m_biquad.getFrequencyResponse(nFrequencies, frequency.data(), magResponse, phaseResponse);
}

double BiquadDSPKernel::tailTime() const
{
    return m_tailTime;
}

double BiquadDSPKernel::latencyTime() const
{
    return 0;
}

void BiquadDSPKernel::updateTailTime(size_t coefIndex)
{
    // A reasonable upper limit for the tail time. While it's easy to
    // create biquad filters whose tail time can be much larger than
    // this, limit the maximum to this value so that we don't keep such
    // nodes alive "forever".
    // TODO: What is a reasonable upper limit?
    constexpr double maxTailTime = 30;

    double sampleRate = this->sampleRate();
    double tail = m_biquad.tailFrame(coefIndex, maxTailTime * sampleRate) / sampleRate;

    m_tailTime = std::clamp(tail, 0.0, maxTailTime);
}

bool BiquadDSPKernel::requiresTailProcessing() const
{
    // Always return true even if the tail time and latency might both
    // be zero. This is for simplicity and because TailTime() is 0
    // basically only when the filter response H(z) = 0 or H(z) = 1. And
    // it's ok to return true. It just means the node lives a little
    // longer than strictly necessary.
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
