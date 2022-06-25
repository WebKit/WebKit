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

#pragma once

#include "AudioDSPKernel.h"
#include "Biquad.h"
#include "BiquadProcessor.h"

namespace WebCore {

class BiquadProcessor;

// BiquadDSPKernel is an AudioDSPKernel and is responsible for filtering one channel of a BiquadProcessor using a Biquad object.

class BiquadDSPKernel final : public AudioDSPKernel {
public:  
    explicit BiquadDSPKernel(BiquadProcessor* processor)
    : AudioDSPKernel(processor)
    {
    }
    
    // AudioDSPKernel
    void process(const float* source, float* dest, size_t framesToProcess) override;
    void reset() override { m_biquad.reset(); }

    // Get the magnitude and phase response of the filter at the given
    // set of frequencies (in Hz). The phase response is in radians.
    void getFrequencyResponse(unsigned nFrequencies, const float* frequencyHz, float* magResponse, float* phaseResponse);

    double tailTime() const override;
    double latencyTime() const override;

    void updateCoefficients(size_t numberOfFrames, const float* cutoffFrequency, const float* q, const float* gain, const float* detune);

    bool requiresTailProcessing() const final;

private:
    Biquad m_biquad;
    BiquadProcessor* biquadProcessor() { return static_cast<BiquadProcessor*>(processor()); }

    // To prevent audio glitches when parameters are changed,
    // dezippering is used to slowly change the parameters.
    // |useSmoothing| implies that we want to update using the
    // smoothed values. Otherwise the final target values are
    // used. If |forceUpdate| is true, we update the coefficients even
    // if they are not dirty. (Used when computing the frequency
    // response.)
    void updateCoefficientsIfNecessary(size_t framesToProcess);
    void updateTailTime(size_t coefIndex);

    double m_tailTime { std::numeric_limits<double>::infinity() };
};

} // namespace WebCore
