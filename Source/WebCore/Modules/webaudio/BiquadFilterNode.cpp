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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(BiquadFilterNode);

ExceptionOr<Ref<BiquadFilterNode>> BiquadFilterNode::create(BaseAudioContext& context, const BiquadFilterOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    auto node = adoptRef(*new BiquadFilterNode(context));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    node->setType(options.type);
    node->q().setValue(options.Q);
    node->detune().setValue(options.detune);
    node->frequency().setValue(options.frequency);
    node->gain().setValue(options.gain);

    return node;
}

BiquadFilterNode::BiquadFilterNode(BaseAudioContext& context)
    : AudioBasicProcessorNode(context, context.sampleRate())
{
    setNodeType(NodeTypeBiquadFilter);

    // Initially setup as lowpass filter.
    m_processor = makeUnique<BiquadProcessor>(context, context.sampleRate(), 1, false);
}

BiquadFilterType BiquadFilterNode::type() const
{
    return const_cast<BiquadFilterNode*>(this)->biquadProcessor()->type();
}

void BiquadFilterNode::setType(BiquadFilterType type)
{
    biquadProcessor()->setType(type);
}

ExceptionOr<void> BiquadFilterNode::getFrequencyResponse(const Ref<Float32Array>& frequencyHz, const Ref<Float32Array>& magResponse, const Ref<Float32Array>& phaseResponse)
{
    auto length = frequencyHz->length();
    if (magResponse->length() != length || phaseResponse->length() != length)
        return Exception { InvalidStateError, "The arrays passed as arguments must have the same length" };

    if (length)
        biquadProcessor()->getFrequencyResponse(length, frequencyHz->data(), magResponse->data(), phaseResponse->data());
    return { };
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
