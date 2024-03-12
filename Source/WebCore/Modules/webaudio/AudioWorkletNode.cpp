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

#include "AudioArray.h"
#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "AudioParam.h"
#include "AudioParamDescriptor.h"
#include "AudioParamMap.h"
#include "AudioUtilities.h"
#include "AudioWorklet.h"
#include "AudioWorkletGlobalScope.h"
#include "AudioWorkletMessagingProxy.h"
#include "AudioWorkletNodeOptions.h"
#include "AudioWorkletProcessor.h"
#include "BaseAudioContext.h"
#include "ErrorEvent.h"
#include "EventNames.h"
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
        return Exception { ExceptionCode::NotSupportedError, "Number of inputs and outputs cannot both be 0"_s };

    if (options.outputChannelCount) {
        if (options.numberOfOutputs != options.outputChannelCount->size())
            return Exception { ExceptionCode::IndexSizeError, "Length of specified outputChannelCount does not match the given number of outputs"_s };

        for (auto& channelCount : *options.outputChannelCount) {
            if (channelCount < 1 || channelCount > AudioContext::maxNumberOfChannels)
                return Exception { ExceptionCode::NotSupportedError, "Provided number of channels for output is outside supported range"_s };
        }
    }

    auto it = context.parameterDescriptorMap().find(name);
    if (it == context.parameterDescriptorMap().end())
        return Exception { ExceptionCode::InvalidStateError, "No ScriptProcessor was registered with this name"_s };
    auto& parameterDescriptors = it->value;

    if (!context.scriptExecutionContext())
        return Exception { ExceptionCode::InvalidStateError, "Audio context's frame is detached"_s };

    auto messageChannel = MessageChannel::create(*context.scriptExecutionContext());
    auto& nodeMessagePort = messageChannel->port1();
    auto& processorMessagePort = messageChannel->port2();

    RefPtr<SerializedScriptValue> serializedOptions;
    {
        auto lock = JSC::JSLockHolder { &globalObject };
        auto* jsOptions = convertDictionaryToJS(globalObject, *JSC::jsCast<JSDOMGlobalObject*>(&globalObject), options);
        serializedOptions = SerializedScriptValue::create(globalObject, jsOptions, SerializationForStorage::No, SerializationErrorMode::NonThrowing, SerializationContext::WorkerPostMessage);
        if (!serializedOptions)
            serializedOptions = SerializedScriptValue::nullValue();
    }

    auto parameterData = WTFMove(options.parameterData);
    auto node = adoptRef(*new AudioWorkletNode(context, name, WTFMove(options), nodeMessagePort));
    node->suspendIfNeeded();

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::Max, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    node->initializeAudioParameters(parameterDescriptors, parameterData);

    // Will cause the context to ref the node until playback has finished.
    // Note that a node with zero outputs cannot be a source node.
    if (node->numberOfOutputs() > 0)
        context.sourceNodeWillBeginPlayback(node);

    context.audioWorklet().createProcessor(name, processorMessagePort.disentangle(), serializedOptions.releaseNonNull(), node);

    {
        // The node should be manually added to the automatic pull node list, even without a connect() call.
        Locker contextLocker { context.graphLock() };
        node->updatePullStatus();
    }

    return node;
}

AudioWorkletNode::AudioWorkletNode(BaseAudioContext& context, const String& name, AudioWorkletNodeOptions&& options, Ref<MessagePort>&& port)
    : AudioNode(context, NodeTypeWorklet)
    , ActiveDOMObject(context.scriptExecutionContext())
    , m_name(name)
    , m_parameters(AudioParamMap::create())
    , m_port(WTFMove(port))
    , m_inputs(options.numberOfInputs)
    , m_outputs(options.numberOfOutputs)
    , m_wasOutputChannelCountGiven(!!options.outputChannelCount)
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
        Locker locker { m_processLock };
        if (m_processor) {
            if (RefPtr workletProxy = context().audioWorklet().proxy()) {
                workletProxy->postTaskForModeToWorkletGlobalScope([processor = WTFMove(m_processor)](ScriptExecutionContext& context) {
                    downcast<AudioWorkletGlobalScope>(context).processorIsNoLongerNeeded(*processor);
                }, WorkerRunLoop::defaultMode());
            }
        }
    }
    uninitialize();
}

void AudioWorkletNode::initializeAudioParameters(const Vector<AudioParamDescriptor>& descriptors, const std::optional<Vector<KeyValuePair<String, double>>>& paramValues)
{
    ASSERT(isMainThread());
    ASSERT(m_parameters->map().isEmpty());

    Locker locker { m_processLock };

    for (auto& descriptor : descriptors) {
        auto parameter = AudioParam::create(context(), descriptor.name, descriptor.defaultValue, descriptor.minValue, descriptor.maxValue, descriptor.automationRate);
        m_parameters->add(descriptor.name, WTFMove(parameter));
    }

    if (paramValues) {
        for (auto& paramValue : *paramValues) {
            if (RefPtr audioParam = m_parameters->map().get(paramValue.key))
                audioParam->setValue(paramValue.value);
        }
    }

    for (auto& parameterName : m_parameters->map().keys())
        m_paramValuesMap.add(parameterName, makeUnique<AudioFloatArray>(AudioUtilities::renderQuantumSize));
}

void AudioWorkletNode::setProcessor(RefPtr<AudioWorkletProcessor>&& processor)
{
    ASSERT(!isMainThread());
    if (processor) {
        Locker locker { m_processLock };
        m_processor = WTFMove(processor);
        m_workletThread = &Thread::current();
    } else
        fireProcessorErrorOnMainThread(ProcessorError::ConstructorError);
}

void AudioWorkletNode::process(size_t framesToProcess)
{
    ASSERT(!isMainThread());

    auto zeroOutput = [&] {
        for (unsigned i = 0; i < numberOfOutputs(); ++i)
            output(i)->bus()->zero();
    };

    if (!m_processLock.tryLock()) {
        zeroOutput();
        return;
    }
    Locker locker { AdoptLock, m_processLock };
    if (!m_processor || &Thread::current() != m_workletThread) {
        // We're not ready yet or we are getting destroyed. In this case, we output silence.
        zeroOutput();
        return;
    }

    // If the input is not connected, pass nullptr to the processor.
    for (unsigned i = 0; i < numberOfInputs(); ++i)
        m_inputs[i] = input(i)->isConnected() ? input(i)->bus() : nullptr;
    for (unsigned i = 0; i < numberOfOutputs(); ++i)
        m_outputs[i] = *output(i)->bus();

    if (noiseInjectionPolicy() == NoiseInjectionPolicy::Minimal) {
        for (unsigned inputIndex = 0; inputIndex < numberOfInputs(); ++inputIndex) {
            if (auto& input = m_inputs[inputIndex]) {
                for (unsigned channelIndex = 0; channelIndex < input->numberOfChannels(); ++channelIndex) {
                    auto* channel = input->channel(channelIndex);
                    AudioUtilities::applyNoise(channel->mutableData(), channel->length(), 0.01);
                }
            }
        }
    }

    for (auto& audioParam : m_parameters->map().values()) {
        auto* paramValues = m_paramValuesMap.get(audioParam->name());
        ASSERT(paramValues);
        RELEASE_ASSERT(paramValues->size() >= framesToProcess);
        if (audioParam->hasSampleAccurateValues() && audioParam->automationRate() == AutomationRate::ARate)
            audioParam->calculateSampleAccurateValues(paramValues->data(), framesToProcess);
        else
            std::fill_n(paramValues->data(), framesToProcess, audioParam->finalValue());
    }

    bool threwException = false;
    if (!m_processor->process(m_inputs, m_outputs, m_paramValuesMap, threwException))
        didFinishProcessingOnRenderingThread(threwException);
}

void AudioWorkletNode::didFinishProcessingOnRenderingThread(bool threwException)
{
    if (threwException)
        fireProcessorErrorOnMainThread(ProcessorError::ProcessError);

    m_processor = nullptr;
    m_tailTime = 0;

    if (numberOfOutputs() > 0)
        context().sourceNodeDidFinishPlayback(*this);
}

void AudioWorkletNode::updatePullStatus()
{
    ASSERT(context().isGraphOwner());

    bool hasConnectedOutput = false;
    for (unsigned i = 0; i < numberOfOutputs(); ++i) {
        if (output(i)->isConnected()) {
            hasConnectedOutput = true;
            break;
        }
    }

    // If no output is connected, add the node to the automatic pull list.
    // Otherwise, remove it out of the list.
    if (!hasConnectedOutput)
        context().addAutomaticPullNode(*this);
    else
        context().removeAutomaticPullNode(*this);
}

void AudioWorkletNode::checkNumberOfChannelsForInput(AudioNodeInput* input)
{
    ASSERT(context().isAudioThread() && context().isGraphOwner());

    // Dynamic channel count only works when the node has 1 input, 1 output and |outputChannelCount|
    // is not given. Otherwise the channel count(s) should not be dynamically changed.
    if (numberOfInputs() == 1 && numberOfOutputs() == 1 && !m_wasOutputChannelCountGiven) {
        ASSERT(input == this->input(0));
        unsigned numberOfInputChannels = input->numberOfChannels();
        if (numberOfInputChannels != output(0)->numberOfChannels()) {
            // This will propagate the channel count to any nodes connected further downstream in the graph.
            output(0)->setNumberOfChannels(numberOfInputChannels);
        }
    }

    // Update the input's internal bus if needed.
    AudioNode::checkNumberOfChannelsForInput(input);
    updatePullStatus();
}

void AudioWorkletNode::fireProcessorErrorOnMainThread(ProcessorError error)
{
    ASSERT(!isMainThread());

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    callOnMainThread([this, protectedThis = Ref { *this }, error]() mutable {
        String errorMessage;
        switch (error) {
        case ProcessorError::ConstructorError:
            errorMessage = "An error was thrown from AudioWorkletProcessor constructor"_s;
            break;
        case ProcessorError::ProcessError:
            errorMessage = "An error was thrown from AudioWorkletProcessor::process() method"_s;
            break;
        }
        queueTaskToDispatchEvent(*this, TaskSource::MediaElement, ErrorEvent::create(eventNames().processorerrorEvent, errorMessage, { }, 0, 0, { }));
    });
}

const char* AudioWorkletNode::activeDOMObjectName() const
{
    return "AudioWorkletNode";
}

bool AudioWorkletNode::virtualHasPendingActivity() const
{
    return !context().isClosed();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
