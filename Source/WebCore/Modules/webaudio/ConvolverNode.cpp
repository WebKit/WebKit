/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2016-2020, Apple Inc. All rights reserved.
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

#include "ConvolverNode.h"

#include "AudioBuffer.h"
#include "AudioNodeInput.h"
#include "AudioNodeOutput.h"
#include "AudioUtilities.h"
#include "Reverb.h"
#include <wtf/IsoMallocInlines.h>

// Note about empirical tuning:
// The maximum FFT size affects reverb performance and accuracy.
// If the reverb is single-threaded and processes entirely in the real-time audio thread,
// it's important not to make this too high.  In this case 8192 is a good value.
// But, the Reverb object is multi-threaded, so we want this as high as possible without losing too much accuracy.
// Very large FFTs will have worse phase errors. Given these constraints 32768 is a good compromise.
constexpr size_t MaxFFTSize = 32768;

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ConvolverNode);

static unsigned computeNumberOfOutputChannels(unsigned inputChannels, unsigned responseChannels)
{
    // The number of output channels for a Convolver must be one or two. And can only be one if
    // there's a mono source and a mono response buffer.
    return (inputChannels == 1 && responseChannels == 1) ? 1u : 2u;
}

ExceptionOr<Ref<ConvolverNode>> ConvolverNode::create(BaseAudioContext& context, ConvolverOptions&& options)
{
    auto node = adoptRef(*new ConvolverNode(context));

    auto result = node->handleAudioNodeOptions(options, { 2, ChannelCountMode::ClampedMax, ChannelInterpretation::Speakers });
    if (result.hasException())
        return result.releaseException();

    node->setNormalizeForBindings(!options.disableNormalization);

    result = node->setBufferForBindings(WTFMove(options.buffer));
    if (result.hasException())
        return result.releaseException();

    return node;
}

ConvolverNode::ConvolverNode(BaseAudioContext& context)
    : AudioNode(context, NodeTypeConvolver)
{
    addInput();
    addOutput(1);
    
    initialize();
}

ConvolverNode::~ConvolverNode()
{
    uninitialize();
}

void ConvolverNode::process(size_t framesToProcess)
{
    AudioBus* outputBus = output(0)->bus();
    ASSERT(outputBus);

    // Synchronize with possible dynamic changes to the impulse response.
    if (!m_processLock.tryLock()) {
        // Too bad - tryLock() failed. We must be in the middle of setting a new impulse response.
        outputBus->zero();
        return;
    }
    Locker locker { AdoptLock, m_processLock };

    if (!isInitialized() || !m_reverb.get())
        outputBus->zero();
    else {
        // Process using the convolution engine.
        // Note that we can handle the case where nothing is connected to the input, in which case we'll just feed silence into the convolver.
        // FIXME: If we wanted to get fancy we could try to factor in the 'tail time' and stop processing once the tail dies down if
        // we keep getting fed silence.
        m_reverb->process(input(0)->bus(), outputBus, framesToProcess);
    }
}

ExceptionOr<void> ConvolverNode::setBufferForBindings(RefPtr<AudioBuffer>&& buffer)
{
    ASSERT(isMainThread());
    
    if (!buffer)
        return { };

    if (buffer->sampleRate() != context().sampleRate())
        return Exception { NotSupportedError, "Buffer sample rate does not match the context's sample rate"_s };

    unsigned numberOfChannels = buffer->numberOfChannels();
    size_t bufferLength = buffer->length();

    // The current implementation supports only 1-, 2-, or 4-channel impulse responses, with the
    // 4-channel response being interpreted as true-stereo (see Reverb class).
    bool isChannelCountGood = (numberOfChannels == 1 || numberOfChannels == 2 || numberOfChannels == 4);

    if (!isChannelCountGood)
        return Exception { NotSupportedError, "Buffer should have 1, 2 or 4 channels"_s };

    // Wrap the AudioBuffer by an AudioBus. It's an efficient pointer set and not a memcpy().
    // This memory is simply used in the Reverb constructor and no reference to it is kept for later use in that class.
    auto bufferBus = AudioBus::create(numberOfChannels, bufferLength, false);
    for (unsigned i = 0; i < numberOfChannels; ++i)
        bufferBus->setChannelMemory(i, buffer->channelData(i)->data(), bufferLength);

    bufferBus->setSampleRate(buffer->sampleRate());

    // Create the reverb with the given impulse response.
    bool useBackgroundThreads = !context().isOfflineContext();
    auto reverb = makeUnique<Reverb>(bufferBus.get(), AudioUtilities::renderQuantumSize, MaxFFTSize, useBackgroundThreads, m_normalize);

    {
        // The context must be locked since changing the buffer can re-configure the number of channels that are output.
        Locker contextLocker { context().graphLock() };

        // Synchronize with process().
        Locker locker { m_processLock };

        m_reverb = WTFMove(reverb);
        m_buffer = WTFMove(buffer);
        if (m_buffer) {
            // This will propagate the channel count to any nodes connected further downstream in the graph.
            output(0)->setNumberOfChannels(computeNumberOfOutputChannels(input(0)->numberOfChannels(), m_buffer->numberOfChannels()));
        }
    }

    return { };
}

AudioBuffer* ConvolverNode::bufferForBindings() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    ASSERT(isMainThread());
    return m_buffer.get();
}

void ConvolverNode::setNormalizeForBindings(bool normalize)
{
    ASSERT(isMainThread());
    m_normalize = normalize;
}

double ConvolverNode::tailTime() const
{
    ASSERT(context().isAudioThread());
    if (!m_processLock.tryLock())
        return std::numeric_limits<double>::infinity();
    Locker locker { AdoptLock, m_processLock };
    return m_reverb ? m_reverb->impulseResponseLength() / static_cast<double>(sampleRate()) : 0;
}

double ConvolverNode::latencyTime() const
{
    ASSERT(context().isAudioThread());
    if (!m_processLock.tryLock())
        return std::numeric_limits<double>::infinity();
    Locker locker { AdoptLock, m_processLock };
    return m_reverb ? m_reverb->latencyFrames() / static_cast<double>(sampleRate()) : 0;
}

bool ConvolverNode::requiresTailProcessing() const
{
    // Always return true even if the tail time and latency might both be zero.
    return true;
}

ExceptionOr<void> ConvolverNode::setChannelCount(unsigned count)
{
    if (count > 2)
        return Exception { NotSupportedError, "ConvolverNode's channel count cannot be greater than 2"_s };
    return AudioNode::setChannelCount(count);
}

ExceptionOr<void> ConvolverNode::setChannelCountMode(ChannelCountMode mode)
{
    if (mode == ChannelCountMode::Max)
        return Exception { NotSupportedError, "ConvolverNode's channel count mode cannot be 'max'"_s };
    return AudioNode::setChannelCountMode(mode);
}

void ConvolverNode::checkNumberOfChannelsForInput(AudioNodeInput* input)
{
    ASSERT(context().isAudioThread() && context().isGraphOwner());
    std::optional<unsigned> numberOfBufferChannels;
    if (m_processLock.tryLock()) {
        Locker locker { AdoptLock, m_processLock };
        if (m_buffer)
            numberOfBufferChannels = m_buffer->numberOfChannels();
    }

    if (numberOfBufferChannels) {
        unsigned numberOfOutputChannels = computeNumberOfOutputChannels(input->numberOfChannels(), *numberOfBufferChannels);

        if (isInitialized() && numberOfOutputChannels != output(0)->numberOfChannels()) {
            // We're already initialized but the channel count has changed.
            uninitialize();
        }

        if (!isInitialized()) {
            // This will propagate the channel count to any nodes connected further
            // downstream in the graph.
            output(0)->setNumberOfChannels(numberOfOutputChannels);
            initialize();
        }
    }

    // Update the input's internal bus if needed.
    AudioNode::checkNumberOfChannelsForInput(input);
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
