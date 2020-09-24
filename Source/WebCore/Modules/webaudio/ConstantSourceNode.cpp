/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "ConstantSourceNode.h"

#include "AudioNodeOutput.h"
#include "AudioParam.h"
#include "AudioUtilities.h"
#include "ConstantSourceOptions.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ConstantSourceNode);

ExceptionOr<Ref<ConstantSourceNode>> ConstantSourceNode::create(BaseAudioContext& context, const ConstantSourceOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };
    
    context.lazyInitialize();
    
    auto node = adoptRef(*new ConstantSourceNode(context, options.offset));
    
    context.refNode(node);
    
    return node;
}

ConstantSourceNode::ConstantSourceNode(BaseAudioContext& context, float offset)
    : AudioScheduledSourceNode(context)
    , m_offset(AudioParam::create(context, "offset"_s, offset, -FLT_MAX, FLT_MAX, AutomationRate::ARate))
    , m_sampleAccurateValues(AudioUtilities::renderQuantumSize)
{
    setNodeType(NodeTypeConstant);
    addOutput(makeUnique<AudioNodeOutput>(this, 1));
    initialize();
}

ConstantSourceNode::~ConstantSourceNode()
{
    uninitialize();
}

void ConstantSourceNode::process(size_t framesToProcess)
{
    auto& outputBus = *output(0)->bus();
    
    if (!isInitialized() || !outputBus.numberOfChannels()) {
        outputBus.zero();
        return;
    }
    
    size_t quantumFrameOffset = 0;
    size_t nonSilentFramesToProcess = 0;
    double startFrameOffset = 0;
    updateSchedulingInfo(framesToProcess, outputBus, quantumFrameOffset, nonSilentFramesToProcess, startFrameOffset);
    
    if (!nonSilentFramesToProcess) {
        outputBus.zero();
        return;
    }
    
    bool isSampleAccurate = m_offset->hasSampleAccurateValues();
    if (isSampleAccurate && m_offset->automationRate() == AutomationRate::ARate) {
        float* offsets = m_sampleAccurateValues.data();
        m_offset->calculateSampleAccurateValues(offsets, framesToProcess);
        if (nonSilentFramesToProcess > 0) {
            memcpy(outputBus.channel(0)->mutableData() + quantumFrameOffset, offsets + quantumFrameOffset, nonSilentFramesToProcess * sizeof(*offsets));
            outputBus.clearSilentFlag();
        } else
            outputBus.zero();
        return;
    }
    
    float value = isSampleAccurate ? m_offset->finalValue() : m_offset->value();
    if (!value)
        outputBus.zero();
    else {
        float* dest = outputBus.channel(0)->mutableData();
        dest += quantumFrameOffset;
        for (unsigned i = 0; i < nonSilentFramesToProcess; ++i)
            dest[i] = value;
        outputBus.clearSilentFlag();
    }
}

bool ConstantSourceNode::propagatesSilence() const
{
    return !isPlayingOrScheduled() || hasFinished();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
