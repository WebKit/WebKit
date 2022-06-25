/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Copyright (C) 2020, Apple Inc. All rights reserved.
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

#include "AudioDSPKernelProcessor.h"
#include <wtf/Vector.h>

namespace WebCore {

class IIRDSPKernel;

class IIRProcessor final : public AudioDSPKernelProcessor {
public:
    IIRProcessor(float sampleRate, unsigned numberOfChannels, const Vector<double>& feedforward, const Vector<double>& feedback, bool isFilterStable);
    ~IIRProcessor();

    // AudioDSPKernelProcessor.
    void process(const AudioBus* source, AudioBus* destination, size_t framesToProcess) final;
    std::unique_ptr<AudioDSPKernel> createKernel() final;

    // Get the magnitude and phase response of the filter at the given set of frequencies (in Hz). The phase response is in radians.
    void getFrequencyResponse(unsigned length, const float* frequencyHz, float* magResponse, float* phaseResponse);

    const Vector<double>& feedforward() const { return m_feedforward; }
    const Vector<double>& feedback() const { return m_feedback; }
    bool isFilterStable() const { return m_isFilterStable; }

private:
    Vector<double> m_feedforward;
    Vector<double> m_feedback;
    bool m_isFilterStable;

    std::unique_ptr<IIRDSPKernel> m_responseKernel;
};

} // namespace WebCore
