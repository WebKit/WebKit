/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "DynamicsCompressorNode.h"

#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "DynamicsCompressor.h"
#include "WebKitDynamicsCompressorNode.h"
#include <wtf/IsoMallocInlines.h>

// Set output to stereo by default.
static const unsigned defaultNumberOfOutputChannels = 2;

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DynamicsCompressorNode);
WTF_MAKE_ISO_ALLOCATED_IMPL(WebKitDynamicsCompressorNode);

ExceptionOr<Ref<DynamicsCompressorNode>> DynamicsCompressorNode::create(BaseAudioContext& context, const DynamicsCompressorOptions& options)
{
    if (context.isStopped())
        return Exception { InvalidStateError };

    context.lazyInitialize();

    auto node = adoptRef(*new DynamicsCompressorNode(context, options));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::ClampedMax, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    return node;
}

DynamicsCompressorNode::DynamicsCompressorNode(BaseAudioContext& context, const DynamicsCompressorOptions& options)
    : AudioNode(context)
    , m_threshold(AudioParam::create(context, "threshold"_s, options.threshold, -100, 0))
    , m_knee(AudioParam::create(context, "knee"_s, options.knee, 0, 40))
    , m_ratio(AudioParam::create(context, "ratio"_s, options.ratio, 1, 20))
    , m_attack(AudioParam::create(context, "attack"_s, options.attack, 0, 1))
    , m_release(AudioParam::create(context, "release"_s, options.release, 0, 1))
{
    setNodeType(NodeTypeDynamicsCompressor);

    addInput(makeUnique<AudioNodeInput>(this));
    addOutput(makeUnique<AudioNodeOutput>(this, defaultNumberOfOutputChannels));

    initialize();
}

DynamicsCompressorNode::~DynamicsCompressorNode()
{
    uninitialize();
}

void DynamicsCompressorNode::process(size_t framesToProcess)
{
    AudioBus* outputBus = output(0)->bus();
    ASSERT(outputBus);

    float threshold = m_threshold->value();
    float knee = m_knee->value();
    float ratio = m_ratio->value();
    float attack = m_attack->value();
    float release = m_release->value();

    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamThreshold, threshold);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamKnee, knee);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamRatio, ratio);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamAttack, attack);
    m_dynamicsCompressor->setParameterValue(DynamicsCompressor::ParamRelease, release);

    m_dynamicsCompressor->process(input(0)->bus(), outputBus, framesToProcess);

    setReduction(m_dynamicsCompressor->parameterValue(DynamicsCompressor::ParamReduction));
}

void DynamicsCompressorNode::reset()
{
    m_dynamicsCompressor->reset();
}

void DynamicsCompressorNode::initialize()
{
    if (isInitialized())
        return;

    AudioNode::initialize();    
    m_dynamicsCompressor = makeUnique<DynamicsCompressor>(sampleRate(), defaultNumberOfOutputChannels);
}

void DynamicsCompressorNode::uninitialize()
{
    if (!isInitialized())
        return;

    m_dynamicsCompressor = nullptr;
    AudioNode::uninitialize();
}

double DynamicsCompressorNode::tailTime() const
{
    return m_dynamicsCompressor->tailTime();
}

double DynamicsCompressorNode::latencyTime() const
{
    return m_dynamicsCompressor->latencyTime();
}

ExceptionOr<void> DynamicsCompressorNode::setChannelCount(unsigned count)
{
    if (count > 2)
        return Exception { NotSupportedError, "DynamicsCompressorNode's channel count cannot be greater than 2"_s };
    return AudioNode::setChannelCount(count);
}

ExceptionOr<void> DynamicsCompressorNode::setChannelCountMode(ChannelCountMode mode)
{
    if (mode == ChannelCountMode::Max)
        return Exception { NotSupportedError, "DynamicsCompressorNode's channel count mode cannot be set to 'max'"_s };
    return AudioNode::setChannelCountMode(mode);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
