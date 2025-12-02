/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "AudioNodeInput.h"
#include "AudioWorklet.h"
#include "AudioWorkletMessagingProxy.h"
#include "Document.h"
#include "Exception.h"
#include "ExceptionOr.h"
#include "Logging.h"
#include "MediaStrategy.h"
#include "PlatformStrategies.h"
#include "ScriptExecutionContext.h"
#include "WorkerRunLoop.h"
#include <wtf/MainThread.h>
#include <wtf/MediaTime.h>
#include <wtf/TZoneMallocInlines.h>

constexpr unsigned EnabledInputChannels = 2;

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(DefaultAudioDestinationNode);

DefaultAudioDestinationNode::DefaultAudioDestinationNode(AudioContext& context, std::optional<float> sampleRate)
    : AudioDestinationNode(context, sampleRate.value_or(AudioDestination::hardwareSampleRate()))
{
    ASSERT(BaseAudioContext::isSupportedSampleRate(AudioDestination::hardwareSampleRate()));
    initializeDefaultNodeOptions(2, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
}

DefaultAudioDestinationNode::~DefaultAudioDestinationNode()
{
    uninitialize();
}

AudioContext& DefaultAudioDestinationNode::context()
{
    return downcast<AudioContext>(AudioDestinationNode::context());
}

bool DefaultAudioDestinationNode::isConnected() const
{
    CheckedPtr input = const_cast<DefaultAudioDestinationNode*>(this)->input(0);
    return input ? !!input->numberOfConnections() : false;
}

const AudioContext& DefaultAudioDestinationNode::context() const
{
    return downcast<AudioContext>(AudioDestinationNode::context());
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
    clearDestination();
    m_numberOfInputChannels = 0;

    AudioNode::uninitialize();
}

void DefaultAudioDestinationNode::clearDestination()
{
    ASSERT(m_destination);
    if (m_wasDestinationStarted) {
        m_destination->stop();
        m_wasDestinationStarted = false;
    }
    m_destination->clearCallback();
    m_destination = nullptr;
}

void DefaultAudioDestinationNode::createDestination()
{
    ALWAYS_LOG(LOGIDENTIFIER, "contextSampleRate = ", sampleRate(), ", hardwareSampleRate = ", AudioDestination::hardwareSampleRate());
    ASSERT(!m_destination);
    m_destination = platformStrategies()->mediaStrategy()->createAudioDestination({ *this, m_inputDeviceId, m_numberOfInputChannels, channelCount(), sampleRate()
#if PLATFORM(IOS_FAMILY)
        , context().sceneIdentifier()
#endif
        });
}

void DefaultAudioDestinationNode::recreateDestination()
{
    bool wasDestinationStarted = m_wasDestinationStarted;
    clearDestination();
    createDestination();
    if (wasDestinationStarted) {
        m_wasDestinationStarted = true;
        m_destination->start(dispatchToRenderThreadFunction());
    }
}

void DefaultAudioDestinationNode::enableInput(const String& inputDeviceId)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    ASSERT(isMainThread());
    if (m_numberOfInputChannels != EnabledInputChannels) {
        m_numberOfInputChannels = EnabledInputChannels;
        m_inputDeviceId = inputDeviceId;

        if (isInitialized())
            recreateDestination();
    }
}

Function<void(Function<void()>&&)> DefaultAudioDestinationNode::dispatchToRenderThreadFunction()
{
    if (RefPtr workletProxy = context().audioWorklet().proxy()) {
        return [workletProxy](Function<void()>&& function) {
            workletProxy->postTaskForModeToWorkletGlobalScope([function = WTFMove(function)](ScriptExecutionContext&) mutable {
                function();
            }, WorkerRunLoop::defaultMode());
        };
    }
    return nullptr;
}

void DefaultAudioDestinationNode::startRendering(CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    ASSERT(isInitialized());
    if (!isInitialized())
        return completionHandler(Exception { ExceptionCode::InvalidStateError, "AudioDestinationNode is not initialized"_s });

    auto innerCompletionHandler = [completionHandler = WTFMove(completionHandler)](bool success) mutable {
        completionHandler(success ? std::nullopt : std::make_optional(Exception { ExceptionCode::InvalidStateError, "Failed to start the audio device"_s }));
    };

    m_wasDestinationStarted = true;
    m_destination->start(dispatchToRenderThreadFunction(), WTFMove(innerCompletionHandler));
}

void DefaultAudioDestinationNode::resume(CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    ASSERT(isInitialized());
    if (!isInitialized()) {
        context().postTask([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(Exception { ExceptionCode::InvalidStateError, "AudioDestinationNode is not initialized"_s });
        });
        return;
    }
    m_wasDestinationStarted = true;
    m_destination->start(dispatchToRenderThreadFunction(), [completionHandler = WTFMove(completionHandler)](bool success) mutable {
        completionHandler(success ? std::nullopt : std::make_optional(Exception { ExceptionCode::InvalidStateError, "Failed to start the audio device"_s }));
    });
}

void DefaultAudioDestinationNode::suspend(CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    ASSERT(isInitialized());
    if (!isInitialized()) {
        context().postTask([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(Exception { ExceptionCode::InvalidStateError, "AudioDestinationNode is not initialized"_s });
        });
        return;
    }

    m_wasDestinationStarted = false;
    m_destination->stop([completionHandler = WTFMove(completionHandler)](bool success) mutable {
        completionHandler(success ? std::nullopt : std::make_optional(Exception { ExceptionCode::InvalidStateError, "Failed to stop the audio device"_s }));
    });
}

void DefaultAudioDestinationNode::restartRendering()
{
    if (!m_wasDestinationStarted)
        return;

    m_destination->stop();
    m_destination->start(dispatchToRenderThreadFunction());
}

void DefaultAudioDestinationNode::close(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isInitialized());
    uninitialize();
    context().postTask(WTFMove(completionHandler));
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
        return Exception { ExceptionCode::IndexSizeError, "Channel count exceeds maximum limit"_s };

    auto oldChannelCount = this->channelCount();
    auto result = AudioNode::setChannelCount(channelCount);
    if (result.hasException())
        return result;

    if (this->channelCount() != oldChannelCount && isInitialized())
        recreateDestination();

    return { };
}

unsigned DefaultAudioDestinationNode::framesPerBuffer() const
{
    return m_destination ? m_destination->framesPerBuffer() : 0;
}

MediaTime DefaultAudioDestinationNode::outputLatency() const
{
    return m_destination ? m_destination->outputLatency() : MediaTime::zeroTime();
}

void DefaultAudioDestinationNode::render(AudioBus& destinationBus, size_t numberOfFrames, const AudioIOPosition& outputPosition)
{
    renderQuantum(destinationBus, numberOfFrames, outputPosition);

    setIsSilent(destinationBus.isSilent());

    // The reason we are handling mute after the call to setIsSilent() is because the muted state does
    // not affect the audio destination node's effective playing state.
    if (m_muted)
        destinationBus.zero();
}

void DefaultAudioDestinationNode::setIsSilent(bool isSilent)
{
    if (m_isSilent == isSilent)
        return;

    m_isSilent = isSilent;
    updateIsEffectivelyPlayingAudio();
}

void DefaultAudioDestinationNode::isPlayingDidChange()
{
    updateIsEffectivelyPlayingAudio();
}

void DefaultAudioDestinationNode::updateIsEffectivelyPlayingAudio()
{
    bool isEffectivelyPlayingAudio = m_destination && m_destination->isPlaying() && !m_isSilent;
    if (m_isEffectivelyPlayingAudio == isEffectivelyPlayingAudio)
        return;

    m_isEffectivelyPlayingAudio = isEffectivelyPlayingAudio;
    context().isPlayingAudioDidChange();
}

#if PLATFORM(IOS_FAMILY)
void DefaultAudioDestinationNode::setSceneIdentifier(const String& sceneIdentifier)
{
    if (m_destination)
        m_destination->setSceneIdentifier(sceneIdentifier);
}
#endif


} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
