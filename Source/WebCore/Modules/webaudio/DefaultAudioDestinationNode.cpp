/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "DefaultAudioDestinationNode.h"

#include "AudioContext.h"
#include "AudioDestination.h"
#include "AudioWorklet.h"
#include "AudioWorkletMessagingProxy.h"
#include "Logging.h"
#include "MediaStrategy.h"
#include "PlatformStrategies.h"
#include "ScriptExecutionContext.h"
#include "WorkerRunLoop.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MainThread.h>

constexpr unsigned EnabledInputChannels = 2;

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DefaultAudioDestinationNode);

DefaultAudioDestinationNode::DefaultAudioDestinationNode(BaseAudioContext& context, Optional<float> sampleRate)
    : AudioDestinationNode(context)
    , m_sampleRate(sampleRate.valueOr(AudioDestination::hardwareSampleRate()))
{
    initializeDefaultNodeOptions(2, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
}

DefaultAudioDestinationNode::~DefaultAudioDestinationNode()
{
    uninitialize();
}

void DefaultAudioDestinationNode::initialize()
{
    ASSERT(isMainThread()); 
    if (isInitialized())
        return;
    ALWAYS_LOG(LOGIDENTIFIER);

    createDestination();
    AudioNode::initialize();
}

void DefaultAudioDestinationNode::uninitialize()
{
    ASSERT(isMainThread()); 
    if (!isInitialized())
        return;

    ALWAYS_LOG(LOGIDENTIFIER);
    m_destination->stop();
    m_destination = nullptr;
    m_numberOfInputChannels = 0;

    AudioNode::uninitialize();
}

void DefaultAudioDestinationNode::createDestination()
{
    ALWAYS_LOG(LOGIDENTIFIER, "contextSampleRate = ", m_sampleRate, ", hardwareSampleRate = ", AudioDestination::hardwareSampleRate());
    m_destination = platformStrategies()->mediaStrategy().createAudioDestination(*this, m_inputDeviceId, m_numberOfInputChannels, channelCount(), m_sampleRate);
}

void DefaultAudioDestinationNode::enableInput(const String& inputDeviceId)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    if (m_numberOfInputChannels != EnabledInputChannels) {
        m_numberOfInputChannels = EnabledInputChannels;
        m_inputDeviceId = inputDeviceId;

        if (isInitialized()) {
            // Re-create destination.
            m_destination->stop();
            createDestination();
            m_destination->start(dispatchToRenderThreadFunction());
        }
    }
}

Function<void(Function<void()>&&)> DefaultAudioDestinationNode::dispatchToRenderThreadFunction()
{
    if (auto* workletProxy = context().audioWorklet().proxy()) {
        return [workletProxy = makeRef(*workletProxy)](Function<void()>&& function) {
            workletProxy->postTaskForModeToWorkletGlobalScope([function = WTFMove(function)](ScriptExecutionContext&) mutable {
                function();
            }, WorkerRunLoop::defaultMode());
        };
    }
    return { };
}

ExceptionOr<void> DefaultAudioDestinationNode::startRendering()
{
    ASSERT(isInitialized());
    if (!isInitialized())
        return Exception { InvalidStateError };

    m_destination->start(dispatchToRenderThreadFunction());
    return { };
}

void DefaultAudioDestinationNode::resume(Function<void ()>&& function)
{
    ASSERT(isInitialized());
    if (isInitialized())
        m_destination->start(dispatchToRenderThreadFunction());
    context().postTask(WTFMove(function));
}

void DefaultAudioDestinationNode::suspend(Function<void ()>&& function)
{
    ASSERT(isInitialized());
    if (isInitialized())
        m_destination->stop();
    context().postTask(WTFMove(function));
}

void DefaultAudioDestinationNode::close(Function<void()>&& function)
{
    ASSERT(isInitialized());
    uninitialize();
    context().postTask(WTFMove(function));
}

unsigned DefaultAudioDestinationNode::maxChannelCount() const
{
    return AudioDestination::maxChannelCount();
}

ExceptionOr<void> DefaultAudioDestinationNode::setChannelCount(unsigned channelCount)
{
    // The channelCount for the input to this node controls the actual number of channels we
    // send to the audio hardware. It can only be set depending on the maximum number of
    // channels supported by the hardware.

    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER, channelCount);

    if (channelCount > maxChannelCount())
        return Exception { IndexSizeError, "Channel count exceeds maximum limit"_s };

    auto oldChannelCount = this->channelCount();
    auto result = AudioNode::setChannelCount(channelCount);
    if (result.hasException())
        return result;

    if (this->channelCount() != oldChannelCount && isInitialized()) {
        // Re-create destination.
        m_destination->stop();
        createDestination();
        m_destination->start(dispatchToRenderThreadFunction());
    }

    return { };
}

bool DefaultAudioDestinationNode::isPlaying()
{
    return m_destination && m_destination->isPlaying();
}

unsigned DefaultAudioDestinationNode::framesPerBuffer() const
{
    return m_destination ? m_destination->framesPerBuffer() : 0.;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
