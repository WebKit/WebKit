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
#include "AudioDSPKernelProcessor.h"
#include "AudioNode.h"
#include "AudioParam.h"
#include "Biquad.h"
#include "BiquadFilterType.h"
#include <memory>
#include <wtf/RefPtr.h>

namespace WebCore {

// BiquadProcessor is an AudioDSPKernelProcessor which uses Biquad objects to implement several common filters.

class BiquadProcessor final : public AudioDSPKernelProcessor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BiquadProcessor(BaseAudioContext&, float sampleRate, size_t numberOfChannels, bool autoInitialize);

    virtual ~BiquadProcessor();
    
    std::unique_ptr<AudioDSPKernel> createKernel() override;
        
    void process(const AudioBus* source, AudioBus* destination, size_t framesToProcess) override;

    // Get the magnitude and phase response of the filter at the given
    // set of frequencies (in Hz). The phase response is in radians.
    void getFrequencyResponse(int nFrequencies,
                              const float* frequencyHz,
                              float* magResponse,
                              float* phaseResponse);

    void checkForDirtyCoefficients();
    
    bool filterCoefficientsDirty() const { return m_filterCoefficientsDirty; }
    bool hasSampleAccurateValues() const { return m_hasSampleAccurateValues; }

    AudioParam& parameter1() { return m_parameter1.get(); }
    AudioParam& parameter2() { return m_parameter2.get(); }
    AudioParam& parameter3() { return m_parameter3.get(); }
    AudioParam& parameter4() { return m_parameter4.get(); }

    BiquadFilterType type() const { return m_type; }
    void setType(BiquadFilterType);

private:
    BiquadFilterType m_type;

    Ref<AudioParam> m_parameter1;
    Ref<AudioParam> m_parameter2;
    Ref<AudioParam> m_parameter3;
    Ref<AudioParam> m_parameter4;

    // so DSP kernels know when to re-compute coefficients
    bool m_filterCoefficientsDirty;

    // Set to true if any of the filter parameters are sample-accurate.
    bool m_hasSampleAccurateValues;
};

} // namespace WebCore
