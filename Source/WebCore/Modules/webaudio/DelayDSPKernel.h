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

#include "AudioArray.h"
#include "AudioDSPKernel.h"
#include "DelayProcessor.h"

namespace WebCore {

class DelayProcessor;
    
class DelayDSPKernel final : public AudioDSPKernel {
public:  
    explicit DelayDSPKernel(DelayProcessor*);
    DelayDSPKernel(double maxDelayTime, float sampleRate);
    
    void process(const float* source, float* destination, size_t framesToProcess) override;
    void processOnlyAudioParams(size_t framesToProcess) final;
    void reset() override;
    
    double maxDelayTime() const { return m_maxDelayTime; }
    
    void setDelayFrames(double numberOfFrames) { m_desiredDelayFrames = numberOfFrames; }

    double tailTime() const override;
    double latencyTime() const override;
    bool requiresTailProcessing() const final;

private:
    void processARate(const float* source, float* destination, size_t framesToProcess);
    void processKRate(const float* source, float* destination, size_t framesToProcess);

    AudioFloatArray m_buffer;
    double m_maxDelayTime;
    size_t m_writeIndex { 0 };
    double m_desiredDelayFrames;

    AudioFloatArray m_delayTimes;
    // Temporary buffer used to hold the second sample for interpolation if needed.
    AudioFloatArray m_tempBuffer;

    DelayProcessor* delayProcessor() { return static_cast<DelayProcessor*>(processor()); }
};

} // namespace WebCore
