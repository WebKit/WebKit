/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AudioDSPKernel_h
#define AudioDSPKernel_h

#include "AudioDSPKernelProcessor.h"

namespace WebCore {

// AudioDSPKernel does the processing for one channel of an AudioDSPKernelProcessor.

class AudioDSPKernel {
    WTF_MAKE_NONCOPYABLE(AudioDSPKernel);
    WTF_MAKE_FAST_ALLOCATED;
public:
    AudioDSPKernel(AudioDSPKernelProcessor* kernelProcessor)
        : m_kernelProcessor(kernelProcessor)
        , m_sampleRate(kernelProcessor->sampleRate())
    {
    }

    AudioDSPKernel(float sampleRate)
        : m_kernelProcessor(0)
        , m_sampleRate(sampleRate)
    {
    }

    virtual ~AudioDSPKernel() { };

    // Subclasses must override process() to do the processing and reset() to reset DSP state.
    virtual void process(const float* source, float* destination, size_t framesToProcess) = 0;

    // Subclasses that have AudioParams must override this to process the AudioParams.
    virtual void processOnlyAudioParams(size_t) { }

    virtual void reset() = 0;

    float sampleRate() const { return m_sampleRate; }
    double nyquist() const { return 0.5 * sampleRate(); }

    AudioDSPKernelProcessor* processor() { return m_kernelProcessor; }
    const AudioDSPKernelProcessor* processor() const { return m_kernelProcessor; }

    virtual double tailTime() const = 0;
    virtual double latencyTime() const = 0;
    virtual bool requiresTailProcessing() const = 0;

protected:
    AudioDSPKernelProcessor* m_kernelProcessor;
    float m_sampleRate;
};

} // namespace WebCore

#endif // AudioDSPKernel_h
