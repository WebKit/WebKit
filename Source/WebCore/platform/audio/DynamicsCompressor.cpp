/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "DynamicsCompressor.h"

#include "AudioBus.h"
#include "AudioUtilities.h"
#include <wtf/MathExtras.h>

namespace WebCore {

using namespace AudioUtilities;
    
DynamicsCompressor::DynamicsCompressor(bool isStereo, double sampleRate)
    : m_isStereo(isStereo)
    , m_sampleRate(sampleRate)
    , m_compressor(sampleRate)
{
    // Uninitialized state - for parameter recalculation.
    m_lastFilterStageRatio = -1;
    m_lastAnchor = -1;
    m_lastFilterStageGain = -1;

    initializeParameters();
}

void DynamicsCompressor::initializeParameters()
{
    // Initializes compressor to default values.
    
    m_parameters[ParamThreshold] = -24; // dB
    m_parameters[ParamHeadroom] = 21; // dB
    m_parameters[ParamAttack] = 0.003; // seconds
    m_parameters[ParamRelease] = 0.250; // seconds
    m_parameters[ParamPreDelay] = 0.006; // seconds

    // Release zone values 0 -> 1.
    m_parameters[ParamReleaseZone1] = 0.09;
    m_parameters[ParamReleaseZone2] = 0.16;
    m_parameters[ParamReleaseZone3] = 0.42;
    m_parameters[ParamReleaseZone4] = 0.98;

    m_parameters[ParamFilterStageGain] = 4.4; // dB
    m_parameters[ParamFilterStageRatio] = 2;
    m_parameters[ParamFilterAnchor] = 15000 / nyquist();
    
    m_parameters[ParamPostGain] = 0; // dB

    // Linear crossfade (0 -> 1).
    m_parameters[ParamEffectBlend] = 1;
}

double DynamicsCompressor::parameterValue(unsigned parameterID)
{
    ASSERT(parameterID < ParamLast);
    return m_parameters[parameterID];
}

void DynamicsCompressor::setEmphasisStageParameters(unsigned stageIndex, float gain, float normalizedFrequency /* 0 -> 1 */)
{
    float gk = 1 - gain / 20;
    float f1 = normalizedFrequency * gk;
    float f2 = normalizedFrequency / gk;
    float r1 = exp(-f1 * piDouble);
    float r2 = exp(-f2 * piDouble);

    // Set pre-filter zero and pole to create an emphasis filter.
    m_preFilter[stageIndex].setZero(r1);
    m_preFilter[stageIndex].setPole(r2);
    m_preFilterR[stageIndex].setZero(r1);
    m_preFilterR[stageIndex].setPole(r2);

    // Set post-filter with zero and pole reversed to create the de-emphasis filter.
    // If there were no compressor kernel in between, they would cancel each other out (allpass filter).
    m_postFilter[stageIndex].setZero(r2);
    m_postFilter[stageIndex].setPole(r1);
    m_postFilterR[stageIndex].setZero(r2);
    m_postFilterR[stageIndex].setPole(r1);
}

void DynamicsCompressor::setEmphasisParameters(float gain, float anchorFreq, float filterStageRatio)
{
    setEmphasisStageParameters(0, gain, anchorFreq);
    setEmphasisStageParameters(1, gain, anchorFreq / filterStageRatio);
    setEmphasisStageParameters(2, gain, anchorFreq / (filterStageRatio * filterStageRatio));
    setEmphasisStageParameters(3, gain, anchorFreq / (filterStageRatio * filterStageRatio * filterStageRatio));
}

void DynamicsCompressor::process(AudioBus* sourceBus, AudioBus* destinationBus, unsigned framesToProcess)
{
    float* sourceL = sourceBus->channel(0)->data();
    float* sourceR;

    if (sourceBus->numberOfChannels() > 1)
        sourceR = sourceBus->channel(1)->data();
    else
        sourceR = sourceL;

    ASSERT(destinationBus->numberOfChannels() == 2);

    float* destinationL = destinationBus->channel(0)->data();
    float* destinationR = destinationBus->channel(1)->data();

    float filterStageGain = parameterValue(ParamFilterStageGain);
    float filterStageRatio = parameterValue(ParamFilterStageRatio);
    float anchor = parameterValue(ParamFilterAnchor);

    if (filterStageGain != m_lastFilterStageGain || filterStageRatio != m_lastFilterStageRatio || anchor != m_lastAnchor) {
        m_lastFilterStageGain = filterStageGain;
        m_lastFilterStageRatio = filterStageRatio;
        m_lastAnchor = anchor;

        setEmphasisParameters(filterStageGain, anchor, filterStageRatio);
    }

    // Apply pre-emphasis filter.
    // Note that the final three stages are computed in-place in the destination buffer.
    m_preFilter[0].process(sourceL, destinationL, framesToProcess);
    m_preFilter[1].process(destinationL, destinationL, framesToProcess);
    m_preFilter[2].process(destinationL, destinationL, framesToProcess);
    m_preFilter[3].process(destinationL, destinationL, framesToProcess);

    if (isStereo()) {
        m_preFilterR[0].process(sourceR, destinationR, framesToProcess);
        m_preFilterR[1].process(destinationR, destinationR, framesToProcess);
        m_preFilterR[2].process(destinationR, destinationR, framesToProcess);
        m_preFilterR[3].process(destinationR, destinationR, framesToProcess);
    }

    float dbThreshold = parameterValue(ParamThreshold);
    float dbHeadroom = parameterValue(ParamHeadroom);
    float attackTime = parameterValue(ParamAttack);
    float releaseTime = parameterValue(ParamRelease);
    float preDelayTime = parameterValue(ParamPreDelay);

    // This is effectively a master volume on the compressed signal (pre-blending).
    float dbPostGain = parameterValue(ParamPostGain);

    // Linear blending value from dry to completely processed (0 -> 1)
    // 0 means the signal is completely unprocessed.
    // 1 mixes in only the compressed signal.
    float effectBlend = parameterValue(ParamEffectBlend);

    double releaseZone1 = parameterValue(ParamReleaseZone1);
    double releaseZone2 = parameterValue(ParamReleaseZone2);
    double releaseZone3 = parameterValue(ParamReleaseZone3);
    double releaseZone4 = parameterValue(ParamReleaseZone4);

    // Apply compression to the pre-filtered signal.
    // The processing is performed in place.
    m_compressor.process(destinationL,
                         destinationL,
                         destinationR,
                         destinationR,
                         framesToProcess,

                         dbThreshold,
                         dbHeadroom,
                         attackTime,
                         releaseTime,
                         preDelayTime,
                         dbPostGain,
                         effectBlend,

                         releaseZone1,
                         releaseZone2,
                         releaseZone3,
                         releaseZone4
                         );

    // Apply de-emphasis filter.
    m_postFilter[0].process(destinationL, destinationL, framesToProcess);
    m_postFilter[1].process(destinationL, destinationL, framesToProcess);
    m_postFilter[2].process(destinationL, destinationL, framesToProcess);
    m_postFilter[3].process(destinationL, destinationL, framesToProcess);

    if (isStereo()) {
        m_postFilterR[0].process(destinationR, destinationR, framesToProcess);
        m_postFilterR[1].process(destinationR, destinationR, framesToProcess);
        m_postFilterR[2].process(destinationR, destinationR, framesToProcess);
        m_postFilterR[3].process(destinationR, destinationR, framesToProcess);
    }
}

void DynamicsCompressor::reset()
{
    m_lastFilterStageRatio = -1; // for recalc
    m_lastAnchor = -1;
    m_lastFilterStageGain = -1;

    for (unsigned i = 0; i < 4; ++i) {
        m_preFilter[i].reset();
        m_preFilterR[i].reset();
        m_postFilter[i].reset();
        m_postFilterR[i].reset();
    }

    m_compressor.reset();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
