/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)
#include "AudioWorkletNode.h"

#include "AudioContext.h"
#include "AudioNodeOutput.h"
#include "AudioParamMap.h"
#include "AudioWorkletNodeOptions.h"
#include "BaseAudioContext.h"
#include "MessageChannel.h"
#include "MessagePort.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioWorkletNode);

ExceptionOr<Ref<AudioWorkletNode>> AudioWorkletNode::create(BaseAudioContext& context, String&& name, AudioWorkletNodeOptions&& options)
{
    if (!options.numberOfInputs && !options.numberOfOutputs)
        return Exception { NotSupportedError, "Number of inputs and outputs cannot both be 0"_s };

    if (options.outputChannelCount) {
        if (options.numberOfOutputs != options.outputChannelCount->size())
            return Exception { IndexSizeError, "Length of specified outputChannelCount does not match the given number of outputs"_s };

        for (auto& channelCount : *options.outputChannelCount) {
            if (channelCount < 1 || channelCount > AudioContext::maxNumberOfChannels())
                return Exception { NotSupportedError, "Provided number of channels for output is outside supported range"_s };
        }
    }

    // FIXME: Throw an InvalidStateError if |name| is already present in the context's node name to parameter descriptor map.

    if (context.isClosed() || !context.scriptExecutionContext())
        return Exception { InvalidStateError, "Context is closed"_s };

    auto messageChannel = MessageChannel::create(*context.scriptExecutionContext());
    // FIXME: Pass messageChannel's port2 to the AudioWorkletProcessor.

    auto node = adoptRef(*new AudioWorkletNode(context, WTFMove(name), WTFMove(options), *messageChannel->port1()));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    return node;
}

AudioWorkletNode::AudioWorkletNode(BaseAudioContext& context, String&& name, AudioWorkletNodeOptions&& options, Ref<MessagePort>&& port)
    : AudioNode(context, NodeTypeWorklet)
    , m_name(WTFMove(name))
    , m_parameters(AudioParamMap::create())
    , m_port(WTFMove(port))
{
    for (unsigned i = 0; i < options.numberOfInputs; ++i)
        addInput();
    for (unsigned i = 0; i < options.numberOfOutputs; ++i)
        addOutput(options.outputChannelCount ? options.outputChannelCount->at(i): 1);

    initialize();
}

AudioWorkletNode::~AudioWorkletNode()
{
    uninitialize();
}

void AudioWorkletNode::process(size_t framesToProcess)
{
    UNUSED_PARAM(framesToProcess);

    // FIXME: Do actual processing.
    for (unsigned i = 0; i < numberOfOutputs(); ++i)
        output(i)->bus()->zero();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
