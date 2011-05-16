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

#ifndef DynamicsCompressor_h
#define DynamicsCompressor_h

#include "AudioArray.h"
#include "DynamicsCompressorKernel.h"
#include "ZeroPole.h"

namespace WebCore {

class AudioBus;

// DynamicsCompressor implements a flexible audio dynamics compression effect such as
// is commonly used in musical production and game audio. It lowers the volume
// of the loudest parts of the signal and raises the volume of the softest parts,
// making the sound richer, fuller, and more controlled.

class DynamicsCompressor {
public:
    enum {
        ParamThreshold,
        ParamHeadroom,
        ParamAttack,
        ParamRelease,
        ParamPreDelay,
        ParamReleaseZone1,
        ParamReleaseZone2,
        ParamReleaseZone3,
        ParamReleaseZone4,
        ParamPostGain,
        ParamFilterStageGain,
        ParamFilterStageRatio,
        ParamFilterAnchor,
        ParamEffectBlend,
        ParamLast
    };

    DynamicsCompressor(bool isStereo, double sampleRate);

    void process(AudioBus* sourceBus, AudioBus* destinationBus, unsigned framesToProcess);
    void reset();

    double parameterValue(unsigned parameterID);

    bool isStereo() const { return m_isStereo; }
    double sampleRate() const { return m_sampleRate; }
    double nyquist() const { return 0.5 * m_sampleRate; }

protected:
    // m_parameters holds the tweakable compressor parameters.
    // FIXME: expose some of the most important ones (such as threshold, attack, release)
    // as DynamicsCompressorNode attributes.
    double m_parameters[ParamLast];
    void initializeParameters();

    bool m_isStereo;
    double m_sampleRate;

    // Emphasis filter controls.
    float m_lastFilterStageRatio;
    float m_lastAnchor;
    float m_lastFilterStageGain;

    // Emphasis filters.
    ZeroPole m_preFilter[4];
    ZeroPole m_preFilterR[4];
    ZeroPole m_postFilter[4];
    ZeroPole m_postFilterR[4];

    void setEmphasisStageParameters(unsigned stageIndex, float gain, float normalizedFrequency /* 0 -> 1 */);
    void setEmphasisParameters(float gain, float anchorFreq, float filterStageRatio);

    // The core compressor.
    DynamicsCompressorKernel m_compressor;
};

} // namespace WebCore

#endif // DynamicsCompressor_h
