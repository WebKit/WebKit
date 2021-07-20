/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "AudioNode.h"

#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOptions.h"
#include "AudioNodeOutput.h"
#include "AudioParam.h"
#include "Logging.h"
#include <wtf/Atomics.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(AudioNode);

String convertEnumerationToString(AudioNode::NodeType enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("NodeTypeDestination"),
        MAKE_STATIC_STRING_IMPL("NodeTypeOscillator"),
        MAKE_STATIC_STRING_IMPL("NodeTypeAudioBufferSource"),
        MAKE_STATIC_STRING_IMPL("NodeTypeMediaElementAudioSource"),
        MAKE_STATIC_STRING_IMPL("NodeTypeMediaStreamAudioDestination"),
        MAKE_STATIC_STRING_IMPL("NodeTypeMediaStreamAudioSource"),
        MAKE_STATIC_STRING_IMPL("NodeTypeJavaScript"),
        MAKE_STATIC_STRING_IMPL("NodeTypeBiquadFilter"),
        MAKE_STATIC_STRING_IMPL("NodeTypePanner"),
        MAKE_STATIC_STRING_IMPL("NodeTypeConvolver"),
        MAKE_STATIC_STRING_IMPL("NodeTypeDelay"),
        MAKE_STATIC_STRING_IMPL("NodeTypeGain"),
        MAKE_STATIC_STRING_IMPL("NodeTypeChannelSplitter"),
        MAKE_STATIC_STRING_IMPL("NodeTypeChannelMerger"),
        MAKE_STATIC_STRING_IMPL("NodeTypeAnalyser"),
        MAKE_STATIC_STRING_IMPL("NodeTypeDynamicsCompressor"),
        MAKE_STATIC_STRING_IMPL("NodeTypeWaveShaper"),
        MAKE_STATIC_STRING_IMPL("NodeTypeConstant"),
        MAKE_STATIC_STRING_IMPL("NodeTypeStereoPanner"),
        MAKE_STATIC_STRING_IMPL("NodeTypeIIRFilter"),
        MAKE_STATIC_STRING_IMPL("NodeTypeWorklet"),
    };
    static_assert(static_cast<size_t>(AudioNode::NodeTypeDestination) == 0, "AudioNode::NodeTypeDestination is not 0 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeOscillator) == 1, "AudioNode::NodeTypeOscillator is not 1 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeAudioBufferSource) == 2, "AudioNode::NodeTypeAudioBufferSource is not 2 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeMediaElementAudioSource) == 3, "AudioNode::NodeTypeMediaElementAudioSource is not 3 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeMediaStreamAudioDestination) == 4, "AudioNode::NodeTypeMediaStreamAudioDestination is not 4 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeMediaStreamAudioSource) == 5, "AudioNode::NodeTypeMediaStreamAudioSource is not 5 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeJavaScript) == 6, "AudioNode::NodeTypeJavaScript is not 6 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeBiquadFilter) == 7, "AudioNode::NodeTypeBiquadFilter is not 7 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypePanner) == 8, "AudioNode::NodeTypePanner is not 8 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeConvolver) == 9, "AudioNode::NodeTypeConvolver is not 9 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeDelay) == 10, "AudioNode::NodeTypeDelay is not 10 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeGain) == 11, "AudioNode::NodeTypeGain is not 11 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeChannelSplitter) == 12, "AudioNode::NodeTypeChannelSplitter is not 12 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeChannelMerger) == 13, "AudioNode::NodeTypeChannelMerger is not 13 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeAnalyser) == 14, "AudioNode::NodeTypeAnalyser is not 14 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeDynamicsCompressor) == 15, "AudioNode::NodeTypeDynamicsCompressor is not 15 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeWaveShaper) == 16, "AudioNode::NodeTypeWaveShaper is not 16 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeConstant) == 17, "AudioNode::NodeTypeConstant is not 17 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeStereoPanner) == 18, "AudioNode::NodeTypeStereoPanner is not 18 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeIIRFilter) == 19, "AudioNode::NodeTypeIIRFilter is not 19 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeWorklet) == 20, "AudioNode::NodeTypeWorklet is not 20 as expected");

    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    
    return values[static_cast<size_t>(enumerationValue)];
}

auto AudioNode::toWeakOrStrongContext(BaseAudioContext& context, NodeType nodeType) -> WeakOrStrongContext
{
    // Destination nodes are owned by the BaseAudioContext so we use WeakPtr to avoid a retain cycle.
    if (nodeType == AudioNode::NodeTypeDestination)
        return makeWeakPtr(context, EnableWeakPtrThreadingAssertions::No); // WebAudio code uses locking when accessing the context.
    return makeRef(context);
}

AudioNode::AudioNode(BaseAudioContext& context, NodeType type)
    : m_nodeType(type)
    , m_context(toWeakOrStrongContext(context, type))
#if !RELEASE_LOG_DISABLED
    , m_logger(context.logger())
    , m_logIdentifier(context.nextAudioNodeLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if DEBUG_AUDIONODE_REFERENCES
    ++s_nodeCount[type];
    if (!s_isNodeCountInitialized) {
        s_isNodeCountInitialized = true;
        atexit(AudioNode::printNodeCounts);
    }
#endif

    // AudioDestinationNodes are not constructed by JS and should not cause lazy initialization.
    if (type != NodeTypeDestination)
        context.lazyInitialize();
}

AudioNode::~AudioNode()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
#if DEBUG_AUDIONODE_REFERENCES
    --s_nodeCount[nodeType()];
    fprintf(stderr, "%p: %d: AudioNode::~AudioNode() %d %d\n", this, nodeType(), m_normalRefCount.load(), m_connectionRefCount);
#endif
}

void AudioNode::initialize()
{
    m_isInitialized = true;
}

void AudioNode::uninitialize()
{
    m_isInitialized = false;
}

void AudioNode::addInput()
{
    ASSERT(isMainThread());
    INFO_LOG(LOGIDENTIFIER);
    m_inputs.append(makeUnique<AudioNodeInput>(this));
}

void AudioNode::addOutput(unsigned numberOfChannels)
{
    ASSERT(isMainThread());
    INFO_LOG(LOGIDENTIFIER);
    m_outputs.append(makeUnique<AudioNodeOutput>(this, numberOfChannels));
}

AudioNodeInput* AudioNode::input(unsigned i)
{
    if (i < m_inputs.size())
        return m_inputs[i].get();
    return nullptr;
}

AudioNodeOutput* AudioNode::output(unsigned i)
{
    if (i < m_outputs.size())
        return m_outputs[i].get();
    return nullptr;
}

ExceptionOr<void> AudioNode::connect(AudioNode& destination, unsigned outputIndex, unsigned inputIndex)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    ALWAYS_LOG(LOGIDENTIFIER, destination.nodeType(), ", output = ", outputIndex, ", input = ", inputIndex);
    
    // Sanity check input and output indices.
    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError, "Output index exceeds number of outputs"_s };

    if (inputIndex >= destination.numberOfInputs())
        return Exception { IndexSizeError, "Input index exceeds number of inputs"_s };

    if (&context() != &destination.context())
        return Exception { InvalidAccessError, "Source and destination nodes belong to different audio contexts"_s };

    auto* input = destination.input(inputIndex);
    auto* output = this->output(outputIndex);

    if (!output->numberOfChannels())
        return Exception { InvalidAccessError, "Node has zero output channels"_s };

    input->connect(output);

    updatePullStatus();

    return { };
}

ExceptionOr<void> AudioNode::connect(AudioParam& param, unsigned outputIndex)
{
    Locker locker { context().graphLock() };

    ASSERT(isMainThread());

    INFO_LOG(LOGIDENTIFIER, param.name(), ", output = ", outputIndex);

    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError, "Output index exceeds number of outputs"_s };

    if (&context() != param.context())
        return Exception { InvalidAccessError, "Node and AudioParam belong to different audio contexts"_s };

    auto* output = this->output(outputIndex);
    param.connect(output);

    return { };
}

void AudioNode::disconnect()
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    for (unsigned outputIndex = 0; outputIndex < numberOfOutputs(); ++outputIndex) {
        auto* output = this->output(outputIndex);
        INFO_LOG(LOGIDENTIFIER, output->node()->nodeType());
        output->disconnectAll();
    }

    updatePullStatus();
}

ExceptionOr<void> AudioNode::disconnect(unsigned outputIndex)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError, "output index is out of bounds"_s };

    auto* output = this->output(outputIndex);
    INFO_LOG(LOGIDENTIFIER, output->node()->nodeType());

    output->disconnectAll();
    updatePullStatus();

    return { };
}

ExceptionOr<void> AudioNode::disconnect(AudioNode& destinationNode)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    bool didDisconnection = false;
    for (unsigned outputIndex = 0; outputIndex < numberOfOutputs(); ++outputIndex) {
        auto* output = this->output(outputIndex);
        for (unsigned inputIndex = 0; inputIndex < destinationNode.numberOfInputs(); ++inputIndex) {
            auto* input = destinationNode.input(inputIndex);
            if (output->isConnectedTo(*input)) {
                input->disconnect(output);
                didDisconnection = true;
            }
        }
    }

    if (!didDisconnection)
        return Exception { InvalidAccessError, "The given destination is not connected"_s };

    updatePullStatus();
    return { };
}

ExceptionOr<void> AudioNode::disconnect(AudioNode& destinationNode, unsigned outputIndex)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError, "output index is out of bounds"_s };

    bool didDisconnection = false;
    auto* output = this->output(outputIndex);
    for (unsigned inputIndex = 0; inputIndex < destinationNode.numberOfInputs(); ++inputIndex) {
        auto* input = destinationNode.input(inputIndex);
        if (output->isConnectedTo(*input)) {
            input->disconnect(output);
            didDisconnection = true;
        }
    }

    if (!didDisconnection)
        return Exception { InvalidAccessError, "The given destination is not connected"_s };

    updatePullStatus();
    return { };
}

ExceptionOr<void> AudioNode::disconnect(AudioNode& destinationNode, unsigned outputIndex, unsigned inputIndex)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError, "output index is out of bounds"_s };

    if (inputIndex >= destinationNode.numberOfInputs())
        return Exception { IndexSizeError, "input index is out of bounds"_s };

    auto* output = this->output(outputIndex);
    auto* input = destinationNode.input(inputIndex);
    if (!output->isConnectedTo(*input))
        return Exception { InvalidAccessError, "The given destination is not connected"_s };

    input->disconnect(output);

    updatePullStatus();
    return { };
}

ExceptionOr<void> AudioNode::disconnect(AudioParam& destinationParam)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    bool didDisconnection = false;
    for (unsigned outputIndex = 0; outputIndex < numberOfOutputs(); ++outputIndex) {
        auto* output = this->output(outputIndex);
        if (output->isConnectedTo(destinationParam)) {
            destinationParam.disconnect(output);
            didDisconnection = true;
        }
    }

    if (!didDisconnection)
        return Exception { InvalidAccessError, "The given destination is not connected"_s };

    updatePullStatus();
    return { };
}

ExceptionOr<void> AudioNode::disconnect(AudioParam& destinationParam, unsigned outputIndex)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError, "output index is out of bounds"_s };

    auto* output = this->output(outputIndex);
    if (!output->isConnectedTo(destinationParam))
        return Exception { InvalidAccessError, "The given destination is not connected"_s };

    destinationParam.disconnect(output);

    updatePullStatus();
    return { };
}

float AudioNode::sampleRate() const
{
    return context().sampleRate();
}

ExceptionOr<void> AudioNode::setChannelCount(unsigned channelCount)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    ALWAYS_LOG(LOGIDENTIFIER, channelCount);

    if (!channelCount)
        return Exception { NotSupportedError, "Channel count cannot be 0"_s };
    
    if (channelCount > AudioContext::maxNumberOfChannels)
        return Exception { NotSupportedError, "Channel count exceeds maximum limit"_s };

    if (m_channelCount == channelCount)
        return { };

    m_channelCount = channelCount;
    if (m_channelCountMode != ChannelCountMode::Max)
        updateChannelsForInputs();
    return { };
}

ExceptionOr<void> AudioNode::setChannelCountMode(ChannelCountMode mode)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    ALWAYS_LOG(LOGIDENTIFIER, mode);
    
    ChannelCountMode oldMode = m_channelCountMode;
    m_channelCountMode = mode;

    if (m_channelCountMode != oldMode)
        updateChannelsForInputs();

    return { };
}

ExceptionOr<void> AudioNode::setChannelInterpretation(ChannelInterpretation interpretation)
{
    ASSERT(isMainThread());
    Locker locker { context().graphLock() };

    ALWAYS_LOG(LOGIDENTIFIER, interpretation);
    
    m_channelInterpretation = interpretation;

    return { };
}

void AudioNode::updateChannelsForInputs()
{
    for (auto& input : m_inputs)
        input->markRenderingStateAsDirty();
}

void AudioNode::initializeDefaultNodeOptions(unsigned count, ChannelCountMode mode, WebCore::ChannelInterpretation interpretation)
{
    m_channelCount = count;
    m_channelCountMode = mode;
    m_channelInterpretation = interpretation;
}

EventTargetInterface AudioNode::eventTargetInterface() const
{
    return AudioNodeEventTargetInterfaceType;
}

ScriptExecutionContext* AudioNode::scriptExecutionContext() const
{
    return static_cast<ActiveDOMObject&>(const_cast<AudioNode*>(this)->context()).scriptExecutionContext();
}

void AudioNode::processIfNecessary(size_t framesToProcess)
{
    ASSERT(context().isAudioThread());

    if (!isInitialized())
        return;

    // Ensure that we only process once per rendering quantum.
    // This handles the "fanout" problem where an output is connected to multiple inputs.
    // The first time we're called during this time slice we process, but after that we don't want to re-process,
    // instead our output(s) will already have the results cached in their bus;
    double currentTime = context().currentTime();
    if (m_lastProcessingTime != currentTime) {
        m_lastProcessingTime = currentTime; // important to first update this time because of feedback loops in the rendering graph

        pullInputs(framesToProcess);

        bool silentInputs = inputsAreSilent();
        if (!silentInputs)
            m_lastNonSilentTime = (context().currentSampleFrame() + framesToProcess) / static_cast<double>(context().sampleRate());

        if (silentInputs && propagatesSilence()) {
            silenceOutputs();
            // AudioParams still need to be processed so that the value can be updated
            // if there are automations or so that the upstream nodes get pulled if
            // any are connected to the AudioParam.
            processOnlyAudioParams(framesToProcess);
        } else
            process(framesToProcess);
    }
}

void AudioNode::checkNumberOfChannelsForInput(AudioNodeInput* input)
{
    ASSERT(context().isAudioThread() && context().isGraphOwner());

    ASSERT(m_inputs.findMatching([&](auto& associatedInput) { return associatedInput.get() == input; }) != notFound);
    input->updateInternalBus();
}

bool AudioNode::propagatesSilence() const
{
    return m_lastNonSilentTime + latencyTime() + tailTime() < context().currentTime();
}

void AudioNode::pullInputs(size_t framesToProcess)
{
    ASSERT(context().isAudioThread());
    
    // Process all of the AudioNodes connected to our inputs.
    for (auto& input : m_inputs)
        input->pull(0, framesToProcess);
}

bool AudioNode::inputsAreSilent()
{
    for (auto& input : m_inputs) {
        if (!input->bus()->isSilent())
            return false;
    }
    return true;
}

void AudioNode::silenceOutputs()
{
    for (auto& output : m_outputs)
        output->bus()->zero();
}

void AudioNode::enableOutputsIfNecessary()
{
    Locker locker { context().graphLock() };
    if (isTailProcessing())
        context().removeTailProcessingNode(*this);

    if (m_isDisabled && m_connectionRefCount > 0) {
        ASSERT(isMainThread());

        m_isDisabled = false;
        for (auto& output : m_outputs)
            output->enable();
    }
}

void AudioNode::disableOutputsIfNecessary()
{
    // Disable outputs if appropriate. We do this if the number of connections is 0 or 1. The case
    // of 0 is from decrementConnectionCountWithLock() where there are no connections left. The case of 1 is from
    // AudioNodeInput::disable() where we want to disable outputs when there's only one connection
    // left because we're ready to go away, but can't quite yet.
    if (m_connectionRefCount <= 1 && !m_isDisabled) {
        // Still may have JavaScript references, but no more "active" connection references, so put all of our outputs in a "dormant" disabled state.
        // Garbage collection may take a very long time after this time, so the "dormant" disabled nodes should not bog down the rendering...

        // As far as JavaScript is concerned, our outputs must still appear to be connected.
        // But internally our outputs should be disabled from the inputs they're connected to.
        // disable() can recursively deref connections (and call disable()) down a whole chain of connected nodes.

        // If a node requires tail processing, we defer the disabling of the outputs so that the tail for the node can be output.
        // Otherwise, we can disable the outputs right away.
        if (requiresTailProcessing())
            context().addTailProcessingNode(*this);
        else
            disableOutputs();
    }
}

void AudioNode::disableOutputs()
{
    m_isDisabled = true;
    for (auto& output : m_outputs)
        output->disable();
}

void AudioNode::incrementConnectionCount()
{
    ++m_connectionRefCount;

    // See the disabling code in decrementConnectionCountWithLock() below. This handles the case where a node
    // is being re-connected after being used at least once and disconnected.
    // In this case, we need to re-enable.
    enableOutputsIfNecessary();

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::incrementConnectionCount() %d %d\n", this, nodeType(), m_normalRefCount, m_connectionRefCount);
#endif
}

void AudioNode::decrementConnectionCount()
{
    // The actually work for deref happens completely within the audio context's graph lock.
    // In the case of the audio thread, we must use a tryLock to avoid glitches.
    if (auto locker = context().isAudioThread() ? Locker<RecursiveLock>::tryLock(context().graphLock()) : Locker { context().graphLock() }) {
        // This is where the real deref work happens.
        decrementConnectionCountWithLock();
    } else {
        // We were unable to get the lock, so put this in a list to finish up later.
        ASSERT(context().isAudioThread());
        context().addDeferredDecrementConnectionCount(this);
    }

    // Once AudioContext::uninitialize() is called there's no more chances for deleteMarkedNodes() to get called, so we call here.
    // We can't call in AudioContext::~AudioContext() since it will never be called as long as any AudioNode is alive
    // because AudioNodes keep a reference to the context.
    if (context().isAudioThreadFinished())
        context().deleteMarkedNodes();
}

void AudioNode::decrementConnectionCountWithLock()
{
    ASSERT(context().isGraphOwner());

    ASSERT(m_connectionRefCount > 0);
    --m_connectionRefCount;

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::decrementConnectionCountWithLock() %d %d\n", this, nodeType(), m_normalRefCount, m_connectionRefCount);
#endif

    if (!m_connectionRefCount && m_normalRefCount)
        disableOutputsIfNecessary();

    markNodeForDeletionIfNecessary();
}

void AudioNode::markNodeForDeletionIfNecessary()
{
    ASSERT(context().isGraphOwner());

    if (m_connectionRefCount || m_normalRefCount || m_isMarkedForDeletion)
        return;

    // AudioDestinationNodes are owned by their BaseAudioContext so there is no need to mark them for deletion.
    if (nodeType() == NodeTypeDestination)
        return;

    // All references are gone - we need to go away.
    for (auto& output : m_outputs)
        output->disconnectAll(); // This will deref() nodes we're connected to.

    // Mark for deletion at end of each render quantum or when context shuts down.
    context().markForDeletion(*this);
    m_isMarkedForDeletion = true;
}

void AudioNode::ref()
{
    ++m_normalRefCount;

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::ref() %d %d\n", this, nodeType(), m_normalRefCount, m_connectionRefCount);
#endif
}

void AudioNode::deref()
{
    ASSERT(!context().isAudioThread());

    {
        Locker locker { context().graphLock() };
        // This is where the real deref work happens.
        derefWithLock();
    }

    // Once AudioContext::uninitialize() is called there's no more chances for deleteMarkedNodes() to get called, so we call here.
    // We can't call in AudioContext::~AudioContext() since it will never be called as long as any AudioNode is alive
    // because AudioNodes keep a reference to the context.
    if (context().isAudioThreadFinished())
        context().deleteMarkedNodes();
}

void AudioNode::derefWithLock()
{
    ASSERT(context().isGraphOwner());
    
    ASSERT(m_normalRefCount > 0);
    --m_normalRefCount;
    
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::deref() %d %d\n", this, nodeType(), m_normalRefCount, m_connectionRefCount);
#endif

    markNodeForDeletionIfNecessary();
}

ExceptionOr<void> AudioNode::handleAudioNodeOptions(const AudioNodeOptions& options, const DefaultAudioNodeOptions& defaults)
{
    auto result = setChannelCount(options.channelCount.value_or(defaults.channelCount));
    if (result.hasException())
        return result.releaseException();

    result = setChannelCountMode(options.channelCountMode.value_or(defaults.channelCountMode));
    if (result.hasException())
        return result.releaseException();

    result = setChannelInterpretation(options.channelInterpretation.value_or(defaults.channelInterpretation));
    if (result.hasException())
        return result.releaseException();

    return { };
}

BaseAudioContext& AudioNode::context()
{
    return WTF::switchOn(m_context, [](Ref<BaseAudioContext>& context) -> BaseAudioContext& {
        return context.get();
    }, [](WeakPtr<BaseAudioContext>& context) -> BaseAudioContext& {
        return *context;
    });
}

const BaseAudioContext& AudioNode::context() const
{
    return WTF::switchOn(m_context, [](const Ref<BaseAudioContext>& context) -> const BaseAudioContext& {
        return context.get();
    }, [](const WeakPtr<BaseAudioContext>& context) -> const BaseAudioContext& {
        return *context;
    });
}

#if DEBUG_AUDIONODE_REFERENCES

bool AudioNode::s_isNodeCountInitialized = false;
int AudioNode::s_nodeCount[NodeTypeLast + 1];

void AudioNode::printNodeCounts()
{
    fprintf(stderr, "\n\n");
    fprintf(stderr, "===========================\n");
    fprintf(stderr, "AudioNode: reference counts\n");
    fprintf(stderr, "===========================\n");

    for (unsigned i = 0; i <= NodeTypeLast; ++i)
        fprintf(stderr, "%d: %d\n", i, s_nodeCount[i]);

    fprintf(stderr, "===========================\n\n\n");
}

#endif // DEBUG_AUDIONODE_REFERENCES

#if !RELEASE_LOG_DISABLED
WTFLogChannel& AudioNode::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
