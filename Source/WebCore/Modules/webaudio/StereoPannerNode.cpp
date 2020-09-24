/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "StereoPannerNode.h"

#if ENABLE(WEB_AUDIO)

#include "AudioBus.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "AudioUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(StereoPannerNode);

ExceptionOr<Ref<StereoPannerNode>> StereoPannerNode::create(BaseAudioContext& context, const StereoPannerOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };
    
    context.lazyInitialize();
    
    auto stereo = adoptRef(*new StereoPannerNode(context, options.pan));
    
    auto result = stereo->handleAudioNodeOptions(options, { 2, ChannelCountMode::ClampedMax, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();
    
    return stereo;
}

StereoPannerNode::StereoPannerNode(BaseAudioContext& context, float pan)
    : AudioNode(context)
    , m_pan(AudioParam::create(context, "pan"_s, pan, -1, 1, AutomationRate::ARate))
    , m_sampleAccurateValues(AudioUtilities::renderQuantumSize)
{
    setNodeType(NodeTypeStereo);
    
    addInput();
    addOutput(2);
    
    initialize();
}

StereoPannerNode::~StereoPannerNode()
{
    uninitialize();
}

void StereoPannerNode::process(size_t framesToProcess)
{
    AudioBus* destination = output(0)->bus();
    
    if (!isInitialized() || !input(0)->isConnected()) {
        destination->zero();
        return;
    }
    
    AudioBus* source = input(0)->bus();
    if (!source) {
        destination->zero();
        return;
    }

    if (m_pan->hasSampleAccurateValues() && m_pan->automationRate() == AutomationRate::ARate) {
        float* panValues = m_sampleAccurateValues.data();
        m_pan->calculateSampleAccurateValues(panValues, framesToProcess);
        StereoPanner::panWithSampleAccurateValues(source, destination, panValues, framesToProcess);
        return;
    }
    
    // The pan value is not sample-accurate or not a-rate. In this case, we have
    // a fixed pan value for the render and just need to incorporate any inputs to
    // the value, if any.
    float panValue = m_pan->hasSampleAccurateValues() ? m_pan->finalValue() : m_pan->value();
    StereoPanner::panToTargetValue(source, destination, panValue, framesToProcess);
}

void StereoPannerNode::processOnlyAudioParams(size_t framesToProcess)
{
    float values[AudioUtilities::renderQuantumSize];
    ASSERT(framesToProcess <= AudioUtilities::renderQuantumSize);

    m_pan->calculateSampleAccurateValues(values, framesToProcess);
}

ExceptionOr<void> StereoPannerNode::setChannelCount(unsigned channelCount)
{
    if (channelCount > 2)
        return Exception { NotSupportedError, "StereoPannerNode's channelCount cannot be greater than 2."_s };
    
    return AudioNode::setChannelCount(channelCount);
}

ExceptionOr<void> StereoPannerNode::setChannelCountMode(ChannelCountMode mode)
{
    if (mode == ChannelCountMode::Max)
        return Exception { NotSupportedError, "StereoPannerNode's channelCountMode cannot be max."_s };
    
    return AudioNode::setChannelCountMode(mode);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
