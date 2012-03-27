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

#include "ExceptionCode.h"

namespace WebCore {

BiquadFilterNode::BiquadFilterNode(AudioContext* context, float sampleRate)
    : AudioBasicProcessorNode(context, sampleRate)
{
    // Initially setup as lowpass filter.
    m_processor = adoptPtr(new BiquadProcessor(sampleRate, 1, false));
    biquadProcessor()->parameter1()->setContext(context);
    biquadProcessor()->parameter2()->setContext(context);
    biquadProcessor()->parameter3()->setContext(context);
    setNodeType(NodeTypeBiquadFilter);
}

void BiquadFilterNode::setType(unsigned short type, ExceptionCode& ec)
{
    if (type > BiquadProcessor::Allpass) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    
    biquadProcessor()->setType(static_cast<BiquadProcessor::FilterType>(type));
}


void BiquadFilterNode::getFrequencyResponse(const Float32Array* frequencyHz,
                                            Float32Array* magResponse,
                                            Float32Array* phaseResponse)
{
    if (!frequencyHz || !magResponse || !phaseResponse)
        return;
    
    int n = std::min(frequencyHz->length(),
                     std::min(magResponse->length(), phaseResponse->length()));

    if (n) {
        biquadProcessor()->getFrequencyResponse(n,
                                                frequencyHz->data(),
                                                magResponse->data(),
                                                phaseResponse->data());
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
