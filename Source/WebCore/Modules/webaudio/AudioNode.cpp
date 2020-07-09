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
#include "AudioNodeOutput.h"
#include "AudioParam.h"
#include "Logging.h"
#include "WebKitAudioContext.h"
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
        MAKE_STATIC_STRING_IMPL("NodeTypeUnknown"),
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
        MAKE_STATIC_STRING_IMPL("NodeTypeBasicInspector"),
        MAKE_STATIC_STRING_IMPL("NodeTypeEnd"),
    };
    static_assert(static_cast<size_t>(AudioNode::NodeTypeUnknown) == 0, "AudioNode::NodeTypeUnknown is not 0 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeDestination) == 1, "AudioNode::NodeTypeDestination is not 1 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeOscillator) == 2, "AudioNode::NodeTypeOscillator is not 2 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeAudioBufferSource) == 3, "AudioNode::NodeTypeAudioBufferSource is not 3 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeMediaElementAudioSource) == 4, "AudioNode::NodeTypeMediaElementAudioSource is not 4 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeMediaStreamAudioDestination) == 5, "AudioNode::NodeTypeMediaStreamAudioDestination is not 5 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeMediaStreamAudioSource) == 6, "AudioNode::NodeTypeMediaStreamAudioSource is not 6 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeJavaScript) == 7, "AudioNode::NodeTypeJavaScript is not 7 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeBiquadFilter) == 8, "AudioNode::NodeTypeBiquadFilter is not 8 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypePanner) == 9, "AudioNode::NodeTypePanner is not 9 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeConvolver) == 10, "AudioNode::NodeTypeConvolver is not 10 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeDelay) == 11, "AudioNode::NodeTypeDelay is not 11 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeGain) == 12, "AudioNode::NodeTypeGain is not 12 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeChannelSplitter) == 13, "AudioNode::NodeTypeChannelSplitter is not 13 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeChannelMerger) == 14, "AudioNode::NodeTypeChannelMerger is not 14 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeAnalyser) == 15, "AudioNode::NodeTypeAnalyser is not 15 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeDynamicsCompressor) == 16, "AudioNode::NodeTypeDynamicsCompressor is not 16 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeWaveShaper) == 17, "AudioNode::NodeTypeWaveShaper is not 17 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeBasicInspector) == 18, "AudioNode::NodeTypeBasicInspector is not 18 as expected");
    static_assert(static_cast<size_t>(AudioNode::NodeTypeEnd) == 19, "AudioNode::NodeTypeEnd is not 19 as expected");

    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    
    return values[static_cast<size_t>(enumerationValue)];
}


// FIXME: Remove once dependencies on old constructor are removed
AudioNode::AudioNode(BaseAudioContext& context, float sampleRate)
    : m_context(context)
    , m_sampleRate(sampleRate)
#if !RELEASE_LOG_DISABLED
    , m_logger(context.logger())
    , m_logIdentifier(context.nextAudioNodeLogIdentifier())
#endif
    , m_channelCount(2)
    , m_channelCountMode(ChannelCountMode::Max)
    , m_channelInterpretation(ChannelInterpretation::Speakers)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    
#if DEBUG_AUDIONODE_REFERENCES
    if (!s_isNodeCountInitialized) {
        s_isNodeCountInitialized = true;
        atexit(AudioNode::printNodeCounts);
    }
#endif
}

AudioNode::AudioNode(BaseAudioContext& context)
    : m_context(context)
    , m_sampleRate(context.sampleRate())
#if !RELEASE_LOG_DISABLED
    , m_logger(context.logger())
    , m_logIdentifier(context.nextAudioNodeLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if DEBUG_AUDIONODE_REFERENCES
    if (!s_isNodeCountInitialized) {
        s_isNodeCountInitialized = true;
        atexit(AudioNode::printNodeCounts);
    }
#endif
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

void AudioNode::setNodeType(NodeType type)
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, type);

    m_nodeType = type;

#if DEBUG_AUDIONODE_REFERENCES
    ++s_nodeCount[type];
#endif
}

void AudioNode::lazyInitialize()
{
    if (!isInitialized())
        initialize();
}

void AudioNode::addInput(std::unique_ptr<AudioNodeInput> input)
{
    ASSERT(isMainThread());
    INFO_LOG(LOGIDENTIFIER, input->node()->nodeType());
    m_inputs.append(WTFMove(input));
}

void AudioNode::addOutput(std::unique_ptr<AudioNodeOutput> output)
{
    ASSERT(isMainThread());
    INFO_LOG(LOGIDENTIFIER, output->node()->nodeType());
    m_outputs.append(WTFMove(output));
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
    BaseAudioContext::AutoLocker locker(context());

    ALWAYS_LOG(LOGIDENTIFIER, destination.nodeType(), ", output = ", outputIndex, ", input = ", inputIndex);
    
    // Sanity check input and output indices.
    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError };

    if (inputIndex >= destination.numberOfInputs())
        return Exception { IndexSizeError };

    if (&context() != &destination.context())
        return Exception { SyntaxError };

    auto* input = destination.input(inputIndex);
    auto* output = this->output(outputIndex);
    input->connect(output);

    // Let context know that a connection has been made.
    context().incrementConnectionCount();

    return { };
}

ExceptionOr<void> AudioNode::connect(AudioParam& param, unsigned outputIndex)
{
    BaseAudioContext::AutoLocker locker(context());

    ASSERT(isMainThread());

    INFO_LOG(LOGIDENTIFIER, param.name(), ", output = ", outputIndex);

    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError };

    if (&context() != &param.context())
        return Exception { SyntaxError };

    auto* output = this->output(outputIndex);
    param.connect(output);

    return { };
}

ExceptionOr<void> AudioNode::disconnect(unsigned outputIndex)
{
    ASSERT(isMainThread());
    BaseAudioContext::AutoLocker locker(context());

    // Sanity check input and output indices.
    if (outputIndex >= numberOfOutputs())
        return Exception { IndexSizeError };

    auto* output = this->output(outputIndex);
    INFO_LOG(LOGIDENTIFIER, output->node()->nodeType());

    output->disconnectAll();

    return { };
}

ExceptionOr<void> AudioNode::setChannelCount(unsigned channelCount)
{
    ASSERT(isMainThread());
    BaseAudioContext::AutoLocker locker(context());

    ALWAYS_LOG(LOGIDENTIFIER, channelCount);
    
    if (!(channelCount > 0 && channelCount <= AudioContext::maxNumberOfChannels()))
        return Exception { InvalidStateError };

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
    BaseAudioContext::AutoLocker locker(context());

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
    BaseAudioContext::AutoLocker locker(context());

    ALWAYS_LOG(LOGIDENTIFIER, interpretation);
    
    m_channelInterpretation = interpretation;

    return { };
}

void AudioNode::updateChannelsForInputs()
{
    for (auto& input : m_inputs)
        input->changedOutputs();
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
            m_lastNonSilentTime = (context().currentSampleFrame() + framesToProcess) / static_cast<double>(m_sampleRate);

        if (silentInputs && propagatesSilence())
            silenceOutputs();
        else
            process(framesToProcess);
    }
}

void AudioNode::checkNumberOfChannelsForInput(AudioNodeInput* input)
{
    ASSERT(context().isAudioThread() && context().isGraphOwner());

    for (auto& savedInput : m_inputs) {
        if (input == savedInput.get()) {
            input->updateInternalBus();
            return;
        }
    }

    ASSERT_NOT_REACHED();
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
    if (m_isDisabled && m_connectionRefCount > 0) {
        ASSERT(isMainThread());
        BaseAudioContext::AutoLocker locker(context());

        m_isDisabled = false;
        for (auto& output : m_outputs)
            output->enable();
    }
}

void AudioNode::disableOutputsIfNecessary()
{
    // Disable outputs if appropriate. We do this if the number of connections is 0 or 1. The case
    // of 0 is from finishDeref() where there are no connections left. The case of 1 is from
    // AudioNodeInput::disable() where we want to disable outputs when there's only one connection
    // left because we're ready to go away, but can't quite yet.
    if (m_connectionRefCount <= 1 && !m_isDisabled) {
        // Still may have JavaScript references, but no more "active" connection references, so put all of our outputs in a "dormant" disabled state.
        // Garbage collection may take a very long time after this time, so the "dormant" disabled nodes should not bog down the rendering...

        // As far as JavaScript is concerned, our outputs must still appear to be connected.
        // But internally our outputs should be disabled from the inputs they're connected to.
        // disable() can recursively deref connections (and call disable()) down a whole chain of connected nodes.

        // FIXME: we special case the convolver and delay since they have a significant tail-time and shouldn't be disconnected simply
        // because they no longer have any input connections. This needs to be handled more generally where AudioNodes have
        // a tailTime attribute. Then the AudioNode only needs to remain "active" for tailTime seconds after there are no
        // longer any active connections.
        if (nodeType() != NodeTypeConvolver && nodeType() != NodeTypeDelay) {
            m_isDisabled = true;
            for (auto& output : m_outputs)
                output->disable();
        }
    }
}

void AudioNode::ref(RefType refType)
{
    switch (refType) {
    case RefTypeNormal:
        ++m_normalRefCount;
        break;
    case RefTypeConnection:
        ++m_connectionRefCount;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::ref(%d) %d %d\n", this, nodeType(), refType, m_normalRefCount, m_connectionRefCount);
#endif

    // See the disabling code in finishDeref() below. This handles the case where a node
    // is being re-connected after being used at least once and disconnected.
    // In this case, we need to re-enable.
    if (refType == RefTypeConnection)
        enableOutputsIfNecessary();
}

void AudioNode::deref(RefType refType)
{
    // The actually work for deref happens completely within the audio context's graph lock.
    // In the case of the audio thread, we must use a tryLock to avoid glitches.
    bool hasLock = false;
    bool mustReleaseLock = false;

    if (context().isAudioThread()) {
        // Real-time audio thread must not contend lock (to avoid glitches).
        hasLock = context().tryLock(mustReleaseLock);
    } else {
        context().lock(mustReleaseLock);
        hasLock = true;
    }

    if (hasLock) {
        // This is where the real deref work happens.
        finishDeref(refType);

        if (mustReleaseLock)
            context().unlock();
    } else {
        // We were unable to get the lock, so put this in a list to finish up later.
        ASSERT(context().isAudioThread());
        ASSERT(refType == RefTypeConnection);
        context().addDeferredFinishDeref(this);
    }

    // Once AudioContext::uninitialize() is called there's no more chances for deleteMarkedNodes() to get called, so we call here.
    // We can't call in AudioContext::~AudioContext() since it will never be called as long as any AudioNode is alive
    // because AudioNodes keep a reference to the context.
    if (context().isAudioThreadFinished())
        context().deleteMarkedNodes();
}

Variant<RefPtr<BaseAudioContext>, RefPtr<WebKitAudioContext>> AudioNode::contextForBindings() const
{
    if (m_context->isWebKitAudioContext())
        return makeRefPtr(static_cast<WebKitAudioContext&>(m_context.get()));
    return makeRefPtr(m_context.get());
}

void AudioNode::finishDeref(RefType refType)
{
    ASSERT(context().isGraphOwner());
    
    switch (refType) {
    case RefTypeNormal:
        ASSERT(m_normalRefCount > 0);
        --m_normalRefCount;
        break;
    case RefTypeConnection:
        ASSERT(m_connectionRefCount > 0);
        --m_connectionRefCount;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
#if DEBUG_AUDIONODE_REFERENCES
    fprintf(stderr, "%p: %d: AudioNode::deref(%d) %d %d\n", this, nodeType(), refType, m_normalRefCount, m_connectionRefCount);
#endif

    if (!m_connectionRefCount) {
        if (!m_normalRefCount) {
            if (!m_isMarkedForDeletion) {
                // All references are gone - we need to go away.
                for (auto& output : m_outputs)
                    output->disconnectAll(); // This will deref() nodes we're connected to.

                // Mark for deletion at end of each render quantum or when context shuts down.
                context().markForDeletion(*this);
                m_isMarkedForDeletion = true;
                didBecomeMarkedForDeletion();
            }
        } else if (refType == RefTypeConnection)
            disableOutputsIfNecessary();
    }
}

#if DEBUG_AUDIONODE_REFERENCES

bool AudioNode::s_isNodeCountInitialized = false;
int AudioNode::s_nodeCount[NodeTypeEnd];

void AudioNode::printNodeCounts()
{
    fprintf(stderr, "\n\n");
    fprintf(stderr, "===========================\n");
    fprintf(stderr, "AudioNode: reference counts\n");
    fprintf(stderr, "===========================\n");

    for (unsigned i = 0; i < NodeTypeEnd; ++i)
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
