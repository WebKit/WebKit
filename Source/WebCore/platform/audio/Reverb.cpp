/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "Reverb.h"

#include "AudioBus.h"
#include "AudioFileReader.h"
#include "ReverbConvolver.h"
#include "VectorMath.h"
#include <math.h>
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace VectorMath;

// Empirical gain calibration tested across many impulse responses to ensure perceived volume is same as dry (unprocessed) signal
const float GainCalibration = -58;
const float GainCalibrationSampleRate = 44100;

// A minimum power value to when normalizing a silent (or very quiet) impulse response
const float MinPower = 0.000125f;
    
static float calculateNormalizationScale(AudioBus* response)
{
    // Normalize by RMS power
    size_t numberOfChannels = response->numberOfChannels();
    size_t length = response->length();

    float power = 0;

    for (size_t i = 0; i < numberOfChannels; ++i) {
        float channelPower = 0;
        vsvesq(response->channel(i)->data(), 1, &channelPower, length);
        power += channelPower;
    }

    power = sqrt(power / (numberOfChannels * length));

    // Protect against accidental overload
    if (std::isinf(power) || std::isnan(power) || power < MinPower)
        power = MinPower;

    float scale = 1 / power;

    scale *= powf(10, GainCalibration * 0.05f); // calibrate to make perceived volume same as unprocessed

    // Scale depends on sample-rate.
    if (response->sampleRate())
        scale *= GainCalibrationSampleRate / response->sampleRate();

    // True-stereo compensation
    if (response->numberOfChannels() == 4)
        scale *= 0.5f;

    return scale;
}

Reverb::Reverb(AudioBus* impulseResponse, size_t renderSliceSize, size_t maxFFTSize, bool useBackgroundThreads, bool normalize)
{
    float scale = 1;

    if (normalize)
        scale = calculateNormalizationScale(impulseResponse);

    initialize(impulseResponse, renderSliceSize, maxFFTSize, useBackgroundThreads, scale);
}

void Reverb::initialize(AudioBus* impulseResponseBuffer, size_t renderSliceSize, size_t maxFFTSize, bool useBackgroundThreads, float scale)
{
    m_impulseResponseLength = impulseResponseBuffer->length();

    // The reverb can handle a mono impulse response and still do stereo processing
    m_numberOfResponseChannels = impulseResponseBuffer->numberOfChannels();
    unsigned convolverCount = std::max(m_numberOfResponseChannels, 2u);
    m_convolvers.reserveCapacity(convolverCount);

    int convolverRenderPhase = 0;
    for (unsigned i = 0; i < convolverCount; ++i) {
        auto* channel = impulseResponseBuffer->channel(std::min(i, m_numberOfResponseChannels - 1));

        m_convolvers.append(makeUnique<ReverbConvolver>(channel, renderSliceSize, maxFFTSize, convolverRenderPhase, useBackgroundThreads, scale));

        convolverRenderPhase += renderSliceSize;
    }

    // For "True" stereo processing we allocate a temporary buffer to avoid repeatedly allocating it in the process() method.
    // It can be bad to allocate memory in a real-time thread.
    if (m_numberOfResponseChannels == 4)
        m_tempBuffer = AudioBus::create(2, MaxFrameSize);
}

void Reverb::process(const AudioBus* sourceBus, AudioBus* destinationBus, size_t framesToProcess)
{
    // Do a fairly comprehensive sanity check.
    // If these conditions are satisfied, all of the source and destination
    // pointers will be valid for the various matrixing cases.
    ASSERT(sourceBus);
    ASSERT(destinationBus);
    ASSERT(sourceBus->numberOfChannels() > 0u);
    ASSERT(destinationBus->numberOfChannels() > 0u);
    ASSERT(framesToProcess <= MaxFrameSize);
    ASSERT(framesToProcess <= sourceBus->length());
    ASSERT(framesToProcess <= destinationBus->length());

    // For now only handle mono or stereo output
    if (destinationBus->numberOfChannels() > 2) {
        destinationBus->zero();
        return;
    }

    AudioChannel* destinationChannelL = destinationBus->channel(0);
    const AudioChannel* sourceChannelL = sourceBus->channel(0);

    // Handle input -> output matrixing...
    size_t numInputChannels = sourceBus->numberOfChannels();
    size_t numOutputChannels = destinationBus->numberOfChannels();
    size_t numberOfResponseChannels = m_numberOfResponseChannels;

    ASSERT(numInputChannels <= 2ul);
    ASSERT(numOutputChannels <= 2ul);
    ASSERT(numberOfResponseChannels == 1 || numberOfResponseChannels == 2 || numberOfResponseChannels == 4);

    // These are the possible combinations of number inputs, response
    // channels and outputs channels that need to be supported:
    //
    //   numInputChannels:         1 or 2
    //   numberOfResponseChannels: 1, 2, or 4
    //   numOutputChannels:        1 or 2
    //
    // Not all possible combinations are valid. numOutputChannels is
    // one only if both numInputChannels and numberOfResponseChannels are 1.
    // Otherwise numOutputChannels MUST be 2.
    //
    // The valid combinations are
    //
    //   Case     in -> resp -> out
    //   1        1 -> 1 -> 1
    //   2        1 -> 2 -> 2
    //   3        1 -> 4 -> 2
    //   4        2 -> 1 -> 2
    //   5        2 -> 2 -> 2
    //   6        2 -> 4 -> 2

    if (numInputChannels == 2 && (numberOfResponseChannels == 1 || numberOfResponseChannels == 2) && numOutputChannels == 2) {
        // Case 4 and 5: 2 -> 2 -> 2 or 2 -> 1 -> 2.
        //
        // These can be handled in the same way because in the latter
        // case, two connvolvers are still created with the second being a
        // copy of the first.
        const AudioChannel* sourceChannelR = sourceBus->channel(1);
        AudioChannel* destinationChannelR = destinationBus->channel(1);
        m_convolvers[0]->process(sourceChannelL, destinationChannelL, framesToProcess);
        m_convolvers[1]->process(sourceChannelR, destinationChannelR, framesToProcess);
    } else  if (numInputChannels == 1 && numOutputChannels == 2 && numberOfResponseChannels == 2) {
        // Case 2: 1 -> 2 -> 2
        for (int i = 0; i < 2; ++i) {
            AudioChannel* destinationChannel = destinationBus->channel(i);
            m_convolvers[i]->process(sourceChannelL, destinationChannel, framesToProcess);
        }
    } else if (numInputChannels == 1 && numberOfResponseChannels == 1) {
        // Case 1: 1 -> 1 -> 1
        ASSERT(numOutputChannels == 1ul);
        m_convolvers[0]->process(sourceChannelL, destinationChannelL, framesToProcess);
    } else if (numInputChannels == 2 && numberOfResponseChannels == 4 && numOutputChannels == 2) {
        // Case 6: 2 -> 4 -> 2 ("True" stereo)
        const AudioChannel* sourceChannelR = sourceBus->channel(1);
        AudioChannel* destinationChannelR = destinationBus->channel(1);

        AudioChannel* tempChannelL = m_tempBuffer->channel(0);
        AudioChannel* tempChannelR = m_tempBuffer->channel(1);

        // Process left virtual source
        m_convolvers[0]->process(sourceChannelL, destinationChannelL, framesToProcess);
        m_convolvers[1]->process(sourceChannelL, destinationChannelR, framesToProcess);

        // Process right virtual source
        m_convolvers[2]->process(sourceChannelR, tempChannelL, framesToProcess);
        m_convolvers[3]->process(sourceChannelR, tempChannelR, framesToProcess);

        destinationBus->sumFrom(*m_tempBuffer);
    } else if (numInputChannels == 1 && numberOfResponseChannels == 4 && numOutputChannels == 2) {
        // Case 3: 1 -> 4 -> 2 (Processing mono with "True" stereo impulse
        // response) This is an inefficient use of a four-channel impulse
        // response, but we should handle the case.
        AudioChannel* destinationChannelR = destinationBus->channel(1);

        AudioChannel* tempChannelL = m_tempBuffer->channel(0);
        AudioChannel* tempChannelR = m_tempBuffer->channel(1);

        // Process left virtual source
        m_convolvers[0]->process(sourceChannelL, destinationChannelL, framesToProcess);
        m_convolvers[1]->process(sourceChannelL, destinationChannelR, framesToProcess);

        // Process right virtual source
        m_convolvers[2]->process(sourceChannelL, tempChannelL, framesToProcess);
        m_convolvers[3]->process(sourceChannelL, tempChannelR, framesToProcess);

        destinationBus->sumFrom(*m_tempBuffer);
    } else {
        ASSERT_NOT_REACHED();
        destinationBus->zero();
    }
}

void Reverb::reset()
{
    for (size_t i = 0; i < m_convolvers.size(); ++i)
        m_convolvers[i]->reset();
}

size_t Reverb::latencyFrames() const
{
    return !m_convolvers.isEmpty() ? m_convolvers.first()->latencyFrames() : 0;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
