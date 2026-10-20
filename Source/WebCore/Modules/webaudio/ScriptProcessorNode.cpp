/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "EventTargetInlines.h"
#include "ExceptionOr.h"
#include <JavaScriptCore/Float32Array.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScriptProcessorNode);

Ref<ScriptProcessorNode> ScriptProcessorNode::create(BaseAudioContext& context, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels)
{
    auto node = adoptRef(*new ScriptProcessorNode(context, bufferSize, numberOfInputChannels, numberOfOutputChannels));
    node->suspendIfNeeded();
    return node;
}

ScriptProcessorNode::ScriptProcessorNode(BaseAudioContext& context, size_t bufferSize, unsigned numberOfInputChannels, unsigned numberOfOutputChannels)
    : AudioNode(context, NodeTypeJavaScript)
    , ActiveDOMObject(protect(context.scriptExecutionContext()))
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

    float sampleRate = protect(context())->sampleRate();

    // Create double buffers on both the input and output sides.
    // These AudioBuffers will be directly accessed in the main thread by JavaScript.
    for (unsigned i = 0; i < bufferCount; ++i) {
        // We prevent detaching the AudioBuffers here since we pass those to JS and reuse them.
        m_inputBuffers[i] = m_numberOfInputChannels ? AudioBuffer::create(m_numberOfInputChannels, bufferSize(), sampleRate, AudioBuffer::LegacyPreventDetaching::Yes) : nullptr;
        m_outputBuffers[i] = m_numberOfOutputChannels ? AudioBuffer::create(m_numberOfOutputChannels, bufferSize(), sampleRate, AudioBuffer::LegacyPreventDetaching::Yes) : nullptr;
    }

    if (!protect(context())->isOfflineContext())
        initializeQueue(sampleRate);

    AudioNode::initialize();
}

void ScriptProcessorNode::initializeQueue(float sampleRate)
{
    if (m_queueSize)
        return;

    constexpr double minimumMainThreadSlack = 0.1;
    constexpr double queueDuration = 2;

    auto periods = [&](double seconds) {
        return static_cast<size_t>(std::ceil(seconds * sampleRate / bufferSize()));
    };
    unsigned outputLatencyPeriods = std::max<unsigned>(bufferCount, 1 + periods(minimumMainThreadSlack));
    size_t queueSize = std::max<size_t>(outputLatencyPeriods + 1, periods(queueDuration));

    Vector<RefPtr<AudioBuffer>> inputs;
    Vector<RefPtr<AudioBuffer>> outputs;
    for (size_t i = 0; i < queueSize; ++i) {
        RefPtr<AudioBuffer> input = m_numberOfInputChannels ? AudioBuffer::create(m_numberOfInputChannels, bufferSize(), sampleRate) : nullptr;
        RefPtr<AudioBuffer> output = m_numberOfOutputChannels ? AudioBuffer::create(m_numberOfOutputChannels, bufferSize(), sampleRate) : nullptr;
        if ((m_numberOfInputChannels && !input) || (m_numberOfOutputChannels && !output))
            return;
        inputs.append(WTF::move(input));
        outputs.append(WTF::move(output));
    }

    m_queueOutputPeriods.reserveInitialCapacity(queueSize);
    for (size_t i = 0; i < queueSize; ++i)
        m_queueOutputPeriods.append(std::nullopt);
    m_queueInputs = WTF::move(inputs);
    m_queueOutputs = WTF::move(outputs);
    m_outputLatencyPeriods = outputLatencyPeriods;
    m_queueSize = queueSize;
}

RefPtr<AudioBuffer> ScriptProcessorNode::createInputBufferForJS(AudioBuffer* inputBuffer) const
{
    if (!inputBuffer)
        return nullptr;

    // As an optimization, we reuse the same buffer as last time when possible.
    RefPtr cachedInputBufferForJS = m_cachedInputBufferForJS;
    if (!cachedInputBufferForJS || !inputBuffer->copyTo(*cachedInputBufferForJS))
        m_cachedInputBufferForJS = inputBuffer->clone();

    return m_cachedInputBufferForJS;
}

RefPtr<AudioBuffer> ScriptProcessorNode::createOutputBufferForJS(AudioBuffer& outputBuffer) const
{
    // As an optimization, we reuse the same buffer as last time when possible.
    RefPtr cachedOutputBufferForJS = m_cachedOutputBufferForJS;
    if (!cachedOutputBufferForJS || !cachedOutputBufferForJS->topologyMatches(outputBuffer))
        m_cachedOutputBufferForJS = outputBuffer.clone(AudioBuffer::ShouldCopyChannelData::No);
    else
        cachedOutputBufferForJS->zero();

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
    CheckedPtr firstInput = input(0);
    Ref inputBus = firstInput->bus();
    CheckedPtr firstOutput = output(0);
    Ref outputBus = firstOutput->bus();

    if (m_queueSize) {
        processWithQueue(inputBus, outputBus, framesToProcess);
        return;
    }

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

    RefPtr inputBuffer = m_inputBuffers[bufferIndex];
    RefPtr outputBuffer = m_outputBuffers[bufferIndex];

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
        m_internalInputBus->setChannelMemory(i, inputBuffer->rawChannelData(i).subspan(m_bufferReadWriteIndex).first(framesToProcess));

    if (numberOfInputChannels)
        m_internalInputBus->copyFrom(inputBus);

    // Copy from the output buffer to the output. 
    for (unsigned i = 0; i < numberOfOutputChannels; ++i)
        memcpySpan(outputBus->channel(i)->mutableSpan(), outputBuffer->rawChannelData(i).subspan(m_bufferReadWriteIndex, framesToProcess));

    // Update the buffering index.
    m_bufferReadWriteIndex = (m_bufferReadWriteIndex + framesToProcess) % bufferSize();

    // m_bufferReadWriteIndex will wrap back around to 0 when the current input and output buffers are full.
    // When this happens, fire an event and swap buffers.
    if (!m_bufferReadWriteIndex) {
        // Heap allocations are forbidden on the audio thread for performance reasons so we need to
        // explicitly allow the following allocation(s).
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

        // Calculate playbackTime with the buffersize which needs to be processed each time when onaudioprocess is called.
        // The outputBuffer being passed to JS will be played after exhausting previous outputBuffer by double-buffering.
        auto fireDoubleBufferedEvent = [this, protector = Ref { *this }](unsigned bufferIndex) {
            double playbackTime = (protect(context())->currentSampleFrame() + m_bufferSize) / static_cast<double>(protect(context())->sampleRate());
            fireProcessEvent(m_inputBuffers[bufferIndex].get(), m_outputBuffers[bufferIndex].get(), playbackTime);
        };

        // Reference ourself so we don't accidentally get deleted before fireProcessEvent() gets called.
        // We only wait for script code execution when the context is an offline one for performance reasons.
        if (protect(context())->isOfflineContext()) {
            callOnMainThreadAndWait([bufferIndex, fireDoubleBufferedEvent, protector = Ref { *this }] {
                fireDoubleBufferedEvent(bufferIndex);
            });
        } else {
            callOnMainThread([this, bufferIndex, fireDoubleBufferedEvent, protector = Ref { *this }] {
                Locker locker { m_bufferLocks[bufferIndex] };
                fireDoubleBufferedEvent(bufferIndex);
            });
        }

        swapBuffers();
    }
}

void ScriptProcessorNode::processWithQueue(AudioBus& inputBus, AudioBus& outputBus, size_t framesToProcess)
{
    bool isFramesToProcessGood = framesToProcess && bufferSize() >= framesToProcess && !(bufferSize() % framesToProcess);
    ASSERT(isFramesToProcessGood);
    if (!isFramesToProcessGood || m_bufferReadWriteIndex + framesToProcess > bufferSize()
        || m_internalInputBus->numberOfChannels() != m_numberOfInputChannels || outputBus.numberOfChannels() != m_numberOfOutputChannels) {
        outputBus.zero();
        return;
    }

    uint64_t period = m_queuePeriod.load(std::memory_order_relaxed);
    size_t slot = queueSlot(period);
    if (!m_bufferReadWriteIndex) {
        // Once every posted event has run, no slot is in use, however far behind the main thread was.
        uint64_t consumed = m_queueConsumed.load(std::memory_order_acquire);
        m_isRecordingPeriod = consumed == m_queuePosted || period - consumed < m_queueSize;
        m_isPlayingPeriod = m_queueOutputPeriods[slot].value_or(std::numeric_limits<uint64_t>::max()) == period;
        m_queuePeriodStartFrame = protect(context())->currentSampleFrame();
    }

    if (m_isRecordingPeriod && m_numberOfInputChannels) {
        auto& input = m_queueInputs[slot];
        for (unsigned i = 0; i < m_numberOfInputChannels; i++)
            m_internalInputBus->setChannelMemory(i, input->rawChannelData(i).subspan(m_bufferReadWriteIndex).first(framesToProcess));
        m_internalInputBus->copyFrom(inputBus);
    }

    if (m_isPlayingPeriod && m_numberOfOutputChannels) {
        auto& output = m_queueOutputs[slot];
        for (unsigned i = 0; i < m_numberOfOutputChannels; ++i)
            memcpySpan(outputBus.channel(i)->mutableSpan(), output->rawChannelData(i).subspan(m_bufferReadWriteIndex, framesToProcess));
    } else
        outputBus.zero();

    m_bufferReadWriteIndex = (m_bufferReadWriteIndex + framesToProcess) % bufferSize();
    if (m_bufferReadWriteIndex)
        return;

    m_queuePeriod.store(period + 1, std::memory_order_release);
    if (!m_isRecordingPeriod)
        return;

    m_queuePosted = period + 1;
    double playbackTime = (m_queuePeriodStartFrame + m_outputLatencyPeriods * bufferSize()) / static_cast<double>(protect(context())->sampleRate());
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    callOnMainThread([this, period, playbackTime, protector = Ref { *this }] {
        fireQueuedProcessEvent(period, playbackTime);
    });
}

void ScriptProcessorNode::fireQueuedProcessEvent(uint64_t period, double playbackTime)
{
    ASSERT(isMainThread());

    uint64_t outputPeriod = period + m_outputLatencyPeriods;
    size_t outputSlot = queueSlot(outputPeriod);
    fireProcessEvent(m_queueInputs[queueSlot(period)].get(), m_queueOutputs[outputSlot].get(), playbackTime);
    m_queueOutputPeriods[outputSlot] = outputPeriod;
    m_queueConsumed.store(period + 1, std::memory_order_release);
}

void ScriptProcessorNode::fireProcessEvent(AudioBuffer* inputBuffer, AudioBuffer* outputBuffer, double playbackTime)
{
    ASSERT(isMainThread());

    ASSERT(outputBuffer);
    if (!outputBuffer)
        return;

    // Avoid firing the event if the document has already gone away.
    Ref context = this->context();
    if (context->isStopped())
        return;

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
        return Exception { ExceptionCode::IndexSizeError, "ScriptProcessorNode's channelCount cannot be changed"_s };
    return { };
}

ExceptionOr<void> ScriptProcessorNode::setChannelCountMode(ChannelCountMode mode)
{
    ASSERT(isMainThread());

    if (mode != this->channelCountMode())
        return Exception { ExceptionCode::NotSupportedError, "ScriptProcessorNode's channelCountMode cannot be changed from 'explicit'"_s };

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
