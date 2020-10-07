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
#include "AudioParam.h"
#include "AudioParamMap.h"
#include "AudioWorklet.h"
#include "AudioWorkletMessagingProxy.h"
#include "AudioWorkletNodeOptions.h"
#include "AudioWorkletProcessor.h"
#include "BaseAudioContext.h"
#include "JSAudioWorkletNodeOptions.h"
#include "MessageChannel.h"
#include "MessagePort.h"
#include "SerializedScriptValue.h"
#include "WorkerRunLoop.h"
#include <JavaScriptCore/JSLock.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioWorkletNode);

ExceptionOr<Ref<AudioWorkletNode>> AudioWorkletNode::create(JSC::JSGlobalObject& globalObject, BaseAudioContext& context, String&& name, AudioWorkletNodeOptions&& options)
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

    auto it = context.parameterDescriptorMap().find(name);
    if (it == context.parameterDescriptorMap().end())
        return Exception { InvalidStateError, "No ScriptProcessor was registered with this name"_s };
    auto& parameterDescriptors = it->value;

    if (context.isClosed() || !context.scriptExecutionContext())
        return Exception { InvalidStateError, "Context is closed"_s };

    auto messageChannel = MessageChannel::create(*context.scriptExecutionContext());
    auto nodeMessagePort = messageChannel->port1();
    auto processorMessagePort = messageChannel->port2();

    RefPtr<SerializedScriptValue> serializedOptions;
    {
        auto lock = JSC::JSLockHolder { &globalObject };
        auto* jsOptions = convertDictionaryToJS(globalObject, *JSC::jsCast<JSDOMGlobalObject*>(&globalObject), options);
        serializedOptions = SerializedScriptValue::create(globalObject, jsOptions, SerializationErrorMode::NonThrowing);
        if (!serializedOptions)
            serializedOptions = SerializedScriptValue::nullValue();
    }

    auto parameterData = WTFMove(options.parameterData);
    auto node = adoptRef(*new AudioWorkletNode(context, name, WTFMove(options), *nodeMessagePort));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    for (auto& descriptor : parameterDescriptors) {
        auto parameter = AudioParam::create(context, descriptor.name, descriptor.defaultValue, descriptor.minValue, descriptor.maxValue, descriptor.automationRate);
        node->parameters().add(descriptor.name, WTFMove(parameter));
    }

    if (parameterData) {
        for (auto& parameter : *parameterData) {
            if (auto* audioParam = node->parameters().map().get(parameter.key))
                audioParam->setValue(parameter.value);
        }
    }

    context.audioWorklet().createProcessor(name, processorMessagePort->disentangle(), serializedOptions.releaseNonNull(), node);

    return node;
}

AudioWorkletNode::AudioWorkletNode(BaseAudioContext& context, const String& name, AudioWorkletNodeOptions&& options, Ref<MessagePort>&& port)
    : AudioNode(context, NodeTypeWorklet)
    , m_name(name)
    , m_parameters(AudioParamMap::create())
    , m_port(WTFMove(port))
{
    ASSERT(isMainThread());
    for (unsigned i = 0; i < options.numberOfInputs; ++i)
        addInput();
    for (unsigned i = 0; i < options.numberOfOutputs; ++i)
        addOutput(options.outputChannelCount ? options.outputChannelCount->at(i): 1);

    initialize();
}

AudioWorkletNode::~AudioWorkletNode()
{
    ASSERT(isMainThread());
    {
        auto locker = holdLock(m_processorLock);
        if (m_processor) {
            if (auto* workletProxy = context().audioWorklet().proxy())
                workletProxy->postTaskForModeToWorkletGlobalScope([m_processor = WTFMove(m_processor)](ScriptExecutionContext&) { }, WorkerRunLoop::defaultMode());
        }
    }
    uninitialize();
}

void AudioWorkletNode::setProcessor(RefPtr<AudioWorkletProcessor>&& processor)
{
    ASSERT(!isMainThread());
    auto locker = holdLock(m_processorLock);
    m_processor = WTFMove(processor);
}

void AudioWorkletNode::process(size_t framesToProcess)
{
    ASSERT(!isMainThread());
    UNUSED_PARAM(framesToProcess);

    // FIXME: Do actual processing via m_processor.
    for (unsigned i = 0; i < numberOfOutputs(); ++i)
        output(i)->bus()->zero();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
