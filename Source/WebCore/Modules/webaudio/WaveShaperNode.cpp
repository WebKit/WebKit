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
#include "WaveShaperNode.h"

#if ENABLE(WEB_AUDIO)

#include "AudioContext.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WaveShaperNode);

WaveShaperNode::WaveShaperNode(AudioContextBase& context)
    : AudioBasicProcessorNode(context, context.sampleRate())
{
    setNodeType(NodeTypeWaveShaper);
    m_processor = makeUnique<WaveShaperProcessor>(context.sampleRate(), 1);

    initialize();
}

void WaveShaperNode::setCurve(Float32Array& curve)
{
    ASSERT(isMainThread()); 
    DEBUG_LOG(LOGIDENTIFIER);
    waveShaperProcessor()->setCurve(&curve);
}

Float32Array* WaveShaperNode::curve()
{
    return waveShaperProcessor()->curve();
}

static inline WaveShaperProcessor::OverSampleType processorType(WaveShaperNode::OverSampleType type)
{
    switch (type) {
    case WaveShaperNode::OverSampleType::None:
        return WaveShaperProcessor::OverSampleNone;
    case WaveShaperNode::OverSampleType::_2x:
        return WaveShaperProcessor::OverSample2x;
    case WaveShaperNode::OverSampleType::_4x:
        return WaveShaperProcessor::OverSample4x;
    }
    ASSERT_NOT_REACHED();
    return WaveShaperProcessor::OverSampleNone;
}

void WaveShaperNode::setOversample(OverSampleType type)
{
    ASSERT(isMainThread());
    INFO_LOG(LOGIDENTIFIER, type);

    // Synchronize with any graph changes or changes to channel configuration.
    AudioContext::AutoLocker contextLocker(context());
    waveShaperProcessor()->setOversample(processorType(type));
}

auto WaveShaperNode::oversample() const -> OverSampleType
{
    switch (const_cast<WaveShaperNode*>(this)->waveShaperProcessor()->oversample()) {
    case WaveShaperProcessor::OverSampleNone:
        return OverSampleType::None;
    case WaveShaperProcessor::OverSample2x:
        return OverSampleType::_2x;
    case WaveShaperProcessor::OverSample4x:
        return OverSampleType::_4x;
    }
    ASSERT_NOT_REACHED();
    return OverSampleType::None;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
