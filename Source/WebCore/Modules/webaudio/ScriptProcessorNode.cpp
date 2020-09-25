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
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ScriptProcessorNode);

Ref<ScriptProcessorNode> ScriptProcessorNode::create(BaseAudioContext& context, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels)
{
    return adoptRef(*new ScriptProcessorNode(context, bufferSize, numberOfInputChannels, numberOfOutputChannels));
}

ScriptProcessorNode::ScriptProcessorNode(BaseAudioContext& context, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels)
    : AudioNode(context)
    , ActiveDOMObject(context.scriptExecutionContext())
    , m_bufferSize(bufferSize)
    , m_numberOfInputChannels(numberOfInputChannels)
    , m_numberOfOutputChannels(numberOfOutputChannels)
    , m_internalInputBus(AudioBus::create(numberOfInputChannels, AudioUtilities::renderQuantumSize, false))
{
    // Regardless of the allowed buffer sizes, we still need to process at the granularity of the AudioNode.
    if (m_bufferSize < AudioUtilities::renderQuantumSize)
        m_bufferSize = AudioUtilities::renderQuantumSize;

    ASSERT(numberOfInputChannels <= AudioContext::maxNumberOfChannels());

    setNodeType(NodeTypeJavaScript);
    initializeDefaultNodeOptions(numberOfInputChannels, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
    addInput();
    addOutput(numberOfOutputChannels);

    initialize();
    suspendIfNeeded();
    m_pendingActivity = makePendingActivity(*this);
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
    for (unsigned i = 0; i < 2; ++i) {
        auto inputBuffer = m_numberOfInputChannels ? AudioBuffer::create(m_numberOfInputChannels, bufferSize(), sampleRate) : 0;
        auto outputBuffer = m_numberOfOutputChannels ? AudioBuffer::create(m_numberOfOutputChannels, bufferSize(), sampleRate) : 0;

        m_inputBuffers.append(inputBuffer);
        m_outputBuffers.append(outputBuffer);
    }

    AudioNode::initialize();
}

void ScriptProcessorNode::uninitialize()
{
    if (!isInitialized())
        return;

    m_inputBuffers.clear();
    m_outputBuffers.clear();

    AudioNode::uninitialize();
}

void ScriptProcessorNode::didBecomeMarkedForDeletion()
{
    ASSERT(context().isGraphOwner());
    m_pendingActivity = nullptr;
    ASSERT(!hasPendingActivity());
}

void ScriptProcessorNode::process(size_t framesToProcess)
{
    // Discussion about inputs and outputs:
    // As in other AudioNodes, ScriptProcessorNode uses an AudioBus for its input and output (see inputBus and outputBus below).
    // Additionally, there is a double-buffering for input and output which is exposed directly to JavaScript (see inputBuffer and outputBuffer below).
    // This node is the producer for inputBuffer and the consumer for outputBuffer.
    // The JavaScript code is the consumer of inputBuffer and the producer for outputBuffer.

    // Get input and output busses.
    AudioBus* inputBus = this->input(0)->bus();
    AudioBus* outputBus = this->output(0)->bus();

    // Get input and output buffers. We double-buffer both the input and output sides.
    unsigned doubleBufferIndex = this->doubleBufferIndex();
    bool isDoubleBufferIndexGood = doubleBufferIndex < 2 && doubleBufferIndex < m_inputBuffers.size() && doubleBufferIndex < m_outputBuffers.size();
    ASSERT(isDoubleBufferIndexGood);
    if (!isDoubleBufferIndexGood)
        return;
    
    AudioBuffer* inputBuffer = m_inputBuffers[doubleBufferIndex].get();
    AudioBuffer* outputBuffer = m_outputBuffers[doubleBufferIndex].get();

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
        m_internalInputBus->setChannelMemory(i, inputBuffer->channelData(i)->data() + m_bufferReadWriteIndex, framesToProcess);

    if (numberOfInputChannels)
        m_internalInputBus->copyFrom(*inputBus);

    // Copy from the output buffer to the output. 
    for (unsigned i = 0; i < numberOfOutputChannels; ++i)
        memcpy(outputBus->channel(i)->mutableData(), outputBuffer->channelData(i)->data() + m_bufferReadWriteIndex, sizeof(float) * framesToProcess);

    // Update the buffering index.
    m_bufferReadWriteIndex = (m_bufferReadWriteIndex + framesToProcess) % bufferSize();

    // m_bufferReadWriteIndex will wrap back around to 0 when the current input and output buffers are full.
    // When this happens, fire an event and swap buffers.
    if (!m_bufferReadWriteIndex) {
        // Reference ourself so we don't accidentally get deleted before fireProcessEvent() gets called.
        // We only wait for script code execution when the context is an offline one for performance reasons.
        if (context().isOfflineContext()) {
            BinarySemaphore semaphore;
            callOnMainThread([this, &semaphore, doubleBufferIndex = m_doubleBufferIndex, protector = makeRef(*this)] {
                fireProcessEvent(doubleBufferIndex);
                semaphore.signal();
            });
            semaphore.wait();
        } else {
            std::unique_lock<Lock> lock(m_processLock, std::try_to_lock);
            if (!lock.owns_lock()) {
                // We're late in handling the previous request. The main thread must be
                // very busy. The best we can do is clear out the buffer ourself here.
                outputBuffer->zero();
                return;
            }

            callOnMainThread([this, doubleBufferIndex = m_doubleBufferIndex, protector = makeRef(*this)] {
                auto locker = holdLock(m_processLock);
                fireProcessEvent(doubleBufferIndex);
            });
        }

        swapBuffers();
    }
}

void ScriptProcessorNode::fireProcessEvent(unsigned doubleBufferIndex)
{
    ASSERT(isMainThread());
    
    bool isIndexGood = doubleBufferIndex < 2;
    ASSERT(isIndexGood);
    if (!isIndexGood)
        return;
        
    AudioBuffer* inputBuffer = m_inputBuffers[doubleBufferIndex].get();
    AudioBuffer* outputBuffer = m_outputBuffers[doubleBufferIndex].get();
    ASSERT(outputBuffer);
    if (!outputBuffer)
        return;

    // Avoid firing the event if the document has already gone away.
    if (!context().isStopped()) {
        // Calculate playbackTime with the buffersize which needs to be processed each time when onaudioprocess is called.
        // The outputBuffer being passed to JS will be played after exhausting previous outputBuffer by double-buffering.
        double playbackTime = (context().currentSampleFrame() + m_bufferSize) / static_cast<double>(context().sampleRate());

        // Call the JavaScript event handler which will do the audio processing.
        dispatchEvent(AudioProcessingEvent::create(inputBuffer, outputBuffer, playbackTime));
    }
}

ExceptionOr<void> ScriptProcessorNode::setChannelCount(unsigned channelCount)
{
    ASSERT(isMainThread());

    if (channelCount != this->channelCount())
        return Exception { NotSupportedError, "ScriptProcessorNode's channelCount cannot be changed"_s };
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

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
