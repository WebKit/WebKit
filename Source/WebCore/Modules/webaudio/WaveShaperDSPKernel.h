/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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
#include "DownSampler.h"
#include "UpSampler.h"
#include "WaveShaperProcessor.h"
#include <memory>

namespace WebCore {

// WaveShaperDSPKernel is an AudioDSPKernel and is responsible for non-linear distortion on one channel.

class WaveShaperDSPKernel final : public AudioDSPKernel {
public:
    explicit WaveShaperDSPKernel(WaveShaperProcessor*);

    // AudioDSPKernel
    void process(const float* source, float* dest, size_t framesToProcess) override;
    void reset() override;
    double tailTime() const override { return 0; }
    double latencyTime() const override;

    // Oversampling requires more resources, so let's only allocate them if needed.
    void lazyInitializeOversampling();

private:
    // Apply the shaping curve.
    void processCurve(const float* source, float* dest, size_t framesToProcess);

    // Use up-sampling, process at the higher sample-rate, then down-sample.
    void processCurve2x(const float* source, float* dest, size_t framesToProcess);
    void processCurve4x(const float* source, float* dest, size_t framesToProcess);

    WaveShaperProcessor* waveShaperProcessor() { return static_cast<WaveShaperProcessor*>(processor()); }

    // Oversampling.
    std::unique_ptr<AudioFloatArray> m_tempBuffer;
    std::unique_ptr<AudioFloatArray> m_tempBuffer2;
    std::unique_ptr<UpSampler> m_upSampler;
    std::unique_ptr<DownSampler> m_downSampler;
    std::unique_ptr<UpSampler> m_upSampler2;
    std::unique_ptr<DownSampler> m_downSampler2;
};

} // namespace WebCore
