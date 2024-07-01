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

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "IIRProcessor.h"

#include "IIRDSPKernel.h"

namespace WebCore {

IIRProcessor::IIRProcessor(float sampleRate, unsigned numberOfChannels, const Vector<double>& feedforward, const Vector<double>& feedback, bool isFilterStable)
    : AudioDSPKernelProcessor(sampleRate, numberOfChannels)
    , m_feedforward(feedforward)
    , m_feedback(feedback)
    , m_isFilterStable(isFilterStable)
{
    unsigned feedbackLength = feedback.size();
    unsigned feedforwardLength = feedforward.size();
    ASSERT(feedbackLength > 0);
    ASSERT(feedforwardLength > 0);

    // Need to scale the feedback and feedforward coefficients appropriately.
    // (It's up to the caller to ensure feedbackCoef[0] is not 0.)
    ASSERT(!!feedback[0]);

    if (feedback[0] != 1) {
        // The provided filter is:
        //
        //   a[0]*y(n) + a[1]*y(n-1) + ... = b[0]*x(n) + b[1]*x(n-1) + ...
        //
        // We want the leading coefficient of y(n) to be 1:
        //
        //   y(n) + a[1]/a[0]*y(n-1) + ... = b[0]/a[0]*x(n) + b[1]/a[0]*x(n-1) + ...
        //
        // Thus, the feedback and feedforward coefficients need to be scaled by 1/a[0].
        float scale = feedback[0];
        for (unsigned k = 1; k < feedbackLength; ++k)
            m_feedback[k] /= scale;

        for (unsigned k = 0; k < feedforwardLength; ++k)
            m_feedforward[k] /= scale;

        // The IIRFilter checks to make sure this coefficient is 1, so make it so.
        m_feedback[0] = 1;
    }

    m_responseKernel = makeUnique<IIRDSPKernel>(*this);
}

IIRProcessor::~IIRProcessor()
{
    if (isInitialized())
        uninitialize();
}

std::unique_ptr<AudioDSPKernel> IIRProcessor::createKernel()
{
    return makeUnique<IIRDSPKernel>(*this);
}

void IIRProcessor::process(const AudioBus* source, AudioBus* destination, size_t framesToProcess)
{
    if (!isInitialized()) {
        destination->zero();
        return;
    }

    // For each channel of our input, process using the corresponding IIRDSPKernel
    // into the output channel.
    for (size_t i = 0; i < m_kernels.size(); ++i)
        m_kernels[i]->process(source->channel(i)->data(), destination->channel(i)->mutableData(), framesToProcess);
}

void IIRProcessor::getFrequencyResponse(unsigned length, const float* frequencyHz, float* magResponse, float* phaseResponse)
{
    m_responseKernel->getFrequencyResponse(length, frequencyHz, magResponse, phaseResponse);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
