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

#ifndef DynamicsCompressorKernel_h
#define DynamicsCompressorKernel_h

#include "AudioArray.h"

namespace WebCore {

class DynamicsCompressorKernel {
public:
    DynamicsCompressorKernel(float sampleRate);

    // Performs stereo-linked compression.
    void process(const float *sourceL,
                 float *destinationL,
                 const float *sourceR,
                 float *destinationR,
                 unsigned framesToProcess,

                 float dbThreshold,
                 float dbHeadroom,
                 float attackTime,
                 float releaseTime,
                 float preDelayTime,
                 float dbPostGain,
                 float effectBlend,

                 float releaseZone1,
                 float releaseZone2,
                 float releaseZone3,
                 float releaseZone4
                 );

    void reset();

    unsigned latencyFrames() const { return m_lastPreDelayFrames; }

    float sampleRate() const { return m_sampleRate; }

protected:
    float m_sampleRate;
    
    float m_detectorAverage;
    float m_compressorGain;

    // Metering
    float m_meteringReleaseK;
    float m_meteringGain;

    // Lookahead section.
    enum { MaxPreDelayFrames = 1024 };
    enum { MaxPreDelayFramesMask = MaxPreDelayFrames - 1 };
    enum { DefaultPreDelayFrames = 256 }; // setPreDelayTime() will override this initial value
    unsigned m_lastPreDelayFrames;
    void setPreDelayTime(float);

    AudioFloatArray m_preDelayBufferL;
    AudioFloatArray m_preDelayBufferR;
    int m_preDelayReadIndex;
    int m_preDelayWriteIndex;

    float m_maxAttackCompressionDiffDb;
};

} // namespace WebCore

#endif // DynamicsCompressorKernel_h
