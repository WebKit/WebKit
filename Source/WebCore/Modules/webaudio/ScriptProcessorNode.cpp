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

#include "ScriptProcessorNode.h"

#include "AudioBuffer.h"
#include "AudioBus.h"
#include "AudioContext.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "AudioProcessingEvent.h"
#include "AudioUtilities.h"
#include "Document.h"
#include "EventNames.h"
#include <JavaScriptCore/Float32Array.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ScriptProcessorNode);

Ref<ScriptProcessorNode> ScriptProcessorNode::create(BaseAudioContext& context, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels)
{
    return adoptRef(*new ScriptProcessorNode(context, bufferSize, numberOfInputChannels, numberOfOutputChannels));
}

ScriptProcessorNode::ScriptProcessorNode(BaseAudioContext& context, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels)
    : AudioNode(context, NodeTypeJavaScript)
    , ActiveDOMObject(context.scriptExecutionContext())
    , m_bufferSize(bufferSize)
    , m_numberOfInputChannels(numberOfInputChannels)
    , m_numberOfOutputChannels(numberOfOutputChannels)
    , m_internalInputBus(AudioBus::create(numberOfInputChannels, AudioUtilities::renderQuantumSize, false))
{
    // Regardless of the allowed buffer sizes, we still need to process at the granularity of the AudioNode.
    if (m_bufferSize < AudioUtilities::renderQuantumSize)
        m_bufferSize = AudioUtilities::renderQuantumSize;

    ASSERT(numberOfInputChannels <= AudioContext::maxNumberOfChannels);

    initializeDefaultNodeOptions(numberOfInputChannels, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
    addInput();
    addOutput(numberOfOutputChannels);

    initialize();
    suspendIfNeeded();
}

ScriptProcessorNode::~ScriptProcessorNode()
{
    ASSERT(!hasPendingActivity());
    uninitialize();
}

void ScriptProcessorNode::initialize()
{
    if (isInitialized())
        return;

    float sampleRate = context().sampleRate();

    // Create double buffers on both the input and output sides.
    // These AudioBuffers will be directly accessed in the main thread by JavaScript.
    for (unsigned i = 0; i < bufferCount; ++i) {
        // We prevent detaching the AudioBuffers here since we pass those to JS and reuse them.
        m_inputBuffers[i] = m_numberOfInputChannels ? AudioBuffer::create(m_numberOfInputChannels, bufferSize(), sampleRate, AudioBuffer::LegacyPreventDetaching::Yes) : 0;
        m_outputBuffers[i] = m_numberOfOutputChannels ? AudioBuffer::create(m_numberOfOutputChannels, bufferSize(), sampleRate, AudioBuffer::LegacyPreventDetaching::Yes) : 0;
    }

    AudioNode::initialize();
}

RefPtr<AudioBuffer> ScriptProcessorNode::createInputBufferForJS(AudioBuffer* inputBuffer) const
{
    if (!inputBuffer)
        return nullptr;

    // As an optimization, we reuse the same buffer as last time when possible.
    if (!m_cachedInputBufferForJS || !inputBuffer->copyTo(*m_cachedInputBufferForJS))
        m_cachedInputBufferForJS = inputBuffer->clone();

    return m_cachedInputBufferForJS;
}

RefPtr<AudioBuffer> ScriptProcessorNode::createOutputBufferForJS(AudioBuffer& outputBuffer) const
{
    // As an optimization, we reuse the same buffer as last time when possible.
    if (!m_cachedOutputBufferForJS || !m_cachedOutputBufferForJS->topologyMatches(outputBuffer))
        m_cachedOutputBufferForJS = outputBuffer.clone(AudioBuffer::ShouldCopyChannelData::No);
    else
        m_cachedOutputBufferForJS->zero();

    return m_cachedOutputBufferForJS;
}

void ScriptProcessorNode::uninitialize()
{
    if (!isInitialized())
        return;

    for (unsigned i = 0; i < bufferCount; ++i) {
        Locker locker { m_bufferLocks[i] };
        m_inputBuffers[i] = nullptr;
        m_outputBuffers[i] = nullptr;
    }

    AudioNode::uninitialize();
}

void ScriptProcessorNode::process(size_t framesToProcess)
{
    // Discussion about inputs and outputs:
    // As in other AudioNodes, ScriptProcessorNode uses an AudioBus for its input and output (see inputBus and outputBus below).
    // Additionally, there is a double-buffering for input and output (see inputBuffer and outputBuffer below).
    // This node is the producer for inputBuffer and the consumer for outputBuffer.
    // The JavaScript code is the consumer of inputBuffer and the producer for outputBuffer. The JavaScript gets its own copy
    // of the buffers for safety reasons.

    // Get input and output busses.
    AudioBus* inputBus = this->input(0)->bus();
    AudioBus* outputBus = this->output(0)->bus();

    // Get input and output buffers. We double-buffer both the input and output sides.
    unsigned bufferIndex = this->bufferIndex();
    ASSERT(bufferIndex < bufferCount);

    if (!m_bufferLocks[bufferIndex].tryLock()) {
        // We're late in handling the previous request. The main thread must be
        // very busy. The best we can do is clear out the buffer ourself here.
        outputBus->zero();
        return;
    }
    Locker locker { AdoptLock, m_bufferLocks[bufferIndex] };
    
    AudioBuffer* inputBuffer = m_inputBuffers[bufferIndex].get();
    AudioBuffer* outputBuffer = m_outputBuffers[bufferIndex].get();

    // Check the consistency of input and output buffers.
    unsigned numberOfInputChannels = m_internalInputBus->numberOfChannels();
    bool buffersAreGood = outputBuffer && bufferSize() == outputBuffer->length() && m_bufferReadWriteIndex + framesToProcess <= bufferSize();

    // If the number of input channels is zero, it's ok to have inputBuffer = 0.
    if (m_internalInputBus->numberOfChannels())
        buffersAreGood = buffersAreGood && inputBuffer && bufferSize() == inputBuffer->length();

    ASSERT(buffersAreGood);
    if (!buffersAreGood)
        return;

    // We assume that bufferSize() is evenly divisible by framesToProcess - should always be true, but we should still check.
    bool isFramesToProcessGood = framesToProcess && bufferSize() >= framesToProcess && !(bufferSize() % framesToProcess);
    ASSERT(isFramesToProcessGood);
    if (!isFramesToProcessGood)
        return;

    unsigned numberOfOutputChannels = outputBus->numberOfChannels();

    bool channelsAreGood = (numberOfInputChannels == m_numberOfInputChannels) && (numberOfOutputChannels == m_numberOfOutputChannels);
    ASSERT(channelsAreGood);
    if (!channelsAreGood)
        return;

    for (unsigned i = 0; i < numberOfInputChannels; i++)
        m_internalInputBus->setChannelMemory(i, inputBuffer->rawChannelData(i) + m_bufferReadWriteIndex, framesToProcess);

    if (numberOfInputChannels)
        m_internalInputBus->copyFrom(*inputBus);

    // Copy from the output buffer to the output. 
    for (unsigned i = 0; i < numberOfOutputChannels; ++i)
        memcpy(outputBus->channel(i)->mutableData(), outputBuffer->rawChannelData(i) + m_bufferReadWriteIndex, sizeof(float) * framesToProcess);

    // Update the buffering index.
    m_bufferReadWriteIndex = (m_bufferReadWriteIndex + framesToProcess) % bufferSize();

    // m_bufferReadWriteIndex will wrap back around to 0 when the current input and output buffers are full.
    // When this happens, fire an event and swap buffers.
    if (!m_bufferReadWriteIndex) {
        // Heap allocations are forbidden on the audio thread for performance reasons so we need to
        // explicitly allow the following allocation(s).
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

        // Reference ourself so we don't accidentally get deleted before fireProcessEvent() gets called.
        // We only wait for script code execution when the context is an offline one for performance reasons.
        if (context().isOfflineContext()) {
            callOnMainThreadAndWait([this, bufferIndex, protector = makeRef(*this)] {
                fireProcessEvent(bufferIndex);
            });
        } else {
            callOnMainThread([this, bufferIndex, protector = makeRef(*this)] {
                Locker locker { m_bufferLocks[bufferIndex] };
                fireProcessEvent(bufferIndex);
            });
        }

        swapBuffers();
    }
}

void ScriptProcessorNode::fireProcessEvent(unsigned bufferIndex)
{
    ASSERT(isMainThread());

    AudioBuffer* inputBuffer = m_inputBuffers[bufferIndex].get();
    AudioBuffer* outputBuffer = m_outputBuffers[bufferIndex].get();
    ASSERT(outputBuffer);
    if (!outputBuffer)
        return;

    // Avoid firing the event if the document has already gone away.
    if (context().isStopped())
        return;

    // Calculate playbackTime with the buffersize which needs to be processed each time when onaudioprocess is called.
    // The outputBuffer being passed to JS will be played after exhausting previous outputBuffer by double-buffering.
    double playbackTime = (context().currentSampleFrame() + m_bufferSize) / static_cast<double>(context().sampleRate());

    auto inputBufferForJS = createInputBufferForJS(inputBuffer);
    auto outputBufferForJS = createOutputBufferForJS(*outputBuffer);

    // Call the JavaScript event handler which will do the audio processing.
    dispatchEvent(AudioProcessingEvent::create(inputBufferForJS.get(), outputBufferForJS.get(), playbackTime));

    if (!outputBufferForJS->copyTo(*outputBuffer))
        outputBuffer->zero();
}

ExceptionOr<void> ScriptProcessorNode::setChannelCount(unsigned channelCount)
{
    ASSERT(isMainThread());

    if (channelCount != this->channelCount())
        return Exception { IndexSizeError, "ScriptProcessorNode's channelCount cannot be changed"_s };
    return { };
}

ExceptionOr<void> ScriptProcessorNode::setChannelCountMode(ChannelCountMode mode)
{
    ASSERT(isMainThread());

    if (mode != this->channelCountMode())
        return Exception { NotSupportedError, "ScriptProcessorNode's channelCountMode cannot be changed from 'explicit'"_s };

    return { };
}

double ScriptProcessorNode::tailTime() const
{
    return std::numeric_limits<double>::infinity();
}

double ScriptProcessorNode::latencyTime() const
{
    return std::numeric_limits<double>::infinity();
}

bool ScriptProcessorNode::requiresTailProcessing() const
{
    // Always return true since the tail and latency are never zero.
    return true;
}

void ScriptProcessorNode::eventListenersDidChange()
{
    m_hasAudioProcessEventListener = hasEventListeners(eventNames().audioprocessEvent);
}

bool ScriptProcessorNode::virtualHasPendingActivity() const
{
    if (context().isClosed())
        return false;

    return m_hasAudioProcessEventListener;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
