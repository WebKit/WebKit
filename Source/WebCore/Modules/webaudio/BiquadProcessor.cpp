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

#include "BiquadProcessor.h"

#include "AudioUtilities.h"
#include "BiquadDSPKernel.h"

namespace WebCore {
    
BiquadProcessor::BiquadProcessor(BaseAudioContext& context, float sampleRate, size_t numberOfChannels, bool autoInitialize)
    : AudioDSPKernelProcessor(sampleRate, numberOfChannels)
    , m_parameter1(AudioParam::create(context, "frequency", 350.0, 0.0, 0.5 * sampleRate, AutomationRate::ARate))
    , m_parameter2(AudioParam::create(context, "Q", 1, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_parameter3(AudioParam::create(context, "gain", 0.0, -FLT_MAX, 40 * std::log10(std::numeric_limits<float>::max()), AutomationRate::ARate))
    , m_parameter4(AudioParam::create(context, "detune", 0.0, -153600, 153600, AutomationRate::ARate))
{
    if (autoInitialize)
        initialize();
}

BiquadProcessor::~BiquadProcessor()
{
    if (isInitialized())
        uninitialize();
}

std::unique_ptr<AudioDSPKernel> BiquadProcessor::createKernel()
{
    return makeUnique<BiquadDSPKernel>(this);
}

void BiquadProcessor::checkForDirtyCoefficients()
{
    // Deal with smoothing / de-zippering. Start out assuming filter parameters are not changing.

    // The BiquadDSPKernel objects rely on this value to see if they need to re-compute their internal filter coefficients.
    m_filterCoefficientsDirty = false;
    m_hasSampleAccurateValues = false;
    
    if (m_parameter1->hasSampleAccurateValues() || m_parameter2->hasSampleAccurateValues() || m_parameter3->hasSampleAccurateValues() || m_parameter4->hasSampleAccurateValues()) {
        m_filterCoefficientsDirty = true;
        m_hasSampleAccurateValues = true;

        m_shouldUseARate = m_parameter1->automationRate() == AutomationRate::ARate
            || m_parameter2->automationRate() == AutomationRate::ARate
            || m_parameter3->automationRate() == AutomationRate::ARate
            || m_parameter4->automationRate() == AutomationRate::ARate;
    } else {
        if (m_hasJustReset) {
            // Snap to exact values first time after reset, then smooth for subsequent changes.
            m_parameter1->resetSmoothedValue();
            m_parameter2->resetSmoothedValue();
            m_parameter3->resetSmoothedValue();
            m_parameter4->resetSmoothedValue();
            m_filterCoefficientsDirty = true;
            m_hasJustReset = false;
        } else {
            // Smooth all of the filter parameters. If they haven't yet converged to their target value then mark coefficients as dirty.
            bool isStable1 = m_parameter1->smooth();
            bool isStable2 = m_parameter2->smooth();
            bool isStable3 = m_parameter3->smooth();
            bool isStable4 = m_parameter4->smooth();
            if (!(isStable1 && isStable2 && isStable3 && isStable4))
                m_filterCoefficientsDirty = true;
        }
    }
}

void BiquadProcessor::process(const AudioBus* source, AudioBus* destination, size_t framesToProcess)
{
    if (!isInitialized()) {
        destination->zero();
        return;
    }
        
    checkForDirtyCoefficients();
            
    // For each channel of our input, process using the corresponding BiquadDSPKernel into the output channel.
    for (unsigned i = 0; i < m_kernels.size(); ++i)
        m_kernels[i]->process(source->channel(i)->data(), destination->channel(i)->mutableData(), framesToProcess);
}

void BiquadProcessor::processOnlyAudioParams(size_t framesToProcess)
{
    float values[AudioUtilities::renderQuantumSize];
    ASSERT(framesToProcess <= AudioUtilities::renderQuantumSize);

    m_parameter1->calculateSampleAccurateValues(values, framesToProcess);
    m_parameter2->calculateSampleAccurateValues(values, framesToProcess);
    m_parameter3->calculateSampleAccurateValues(values, framesToProcess);
    m_parameter4->calculateSampleAccurateValues(values, framesToProcess);
}

void BiquadProcessor::setType(BiquadFilterType type)
{
    if (type != m_type) {
        m_type = type;
        reset(); // The filter state must be reset only if the type has changed.
    }
}

void BiquadProcessor::getFrequencyResponse(unsigned nFrequencies, const float* frequencyHz, float* magResponse, float* phaseResponse)
{
    // Compute the frequency response on a separate temporary kernel
    // to avoid interfering with the processing running in the audio
    // thread on the main kernels.
    
    auto responseKernel = makeUnique<BiquadDSPKernel>(this);

    // Get a copy of the current biquad filter coefficients so we can update
    // |responseKernel| with these values.
    float cutoffFrequency = parameter1().value();
    float q = parameter2().value();
    float gain = parameter3().value();
    float detune = parameter4().value();

    responseKernel->updateCoefficients(1, &cutoffFrequency, &q, &gain, &detune);
    responseKernel->getFrequencyResponse(nFrequencies, frequencyHz, magResponse, phaseResponse);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
