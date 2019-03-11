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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "BiquadFilterNode.h"

namespace WebCore {

BiquadFilterNode::BiquadFilterNode(AudioContext& context, float sampleRate)
    : AudioBasicProcessorNode(context, sampleRate)
{
    setNodeType(NodeTypeBiquadFilter);

    // Initially setup as lowpass filter.
    m_processor = std::make_unique<BiquadProcessor>(context, sampleRate, 1, false);
}

BiquadFilterType BiquadFilterNode::type() const
{
    return const_cast<BiquadFilterNode*>(this)->biquadProcessor()->type();
}

void BiquadFilterNode::setType(BiquadFilterType type)
{
    biquadProcessor()->setType(type);
}

void BiquadFilterNode::getFrequencyResponse(const RefPtr<Float32Array>& frequencyHz, const RefPtr<Float32Array>& magResponse, const RefPtr<Float32Array>& phaseResponse)
{
    if (!frequencyHz || !magResponse || !phaseResponse)
        return;
    
    int n = std::min(frequencyHz->length(), std::min(magResponse->length(), phaseResponse->length()));

    if (n)
        biquadProcessor()->getFrequencyResponse(n, frequencyHz->data(), magResponse->data(), phaseResponse->data());
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
