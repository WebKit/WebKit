/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteAudioDestinationProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcessConnection.h"
#include "RemoteAudioDestinationManager.h"
#include "RemoteAudioDestinationManagerMessages.h"
#include "RemoteAudioDestinationProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/AudioBus.h>
#include <WebCore/AudioDestination.h>

namespace WebKit {

using AudioBus = WebCore::AudioBus;
using AudioDestination = WebCore::AudioDestination;
using AudioIOCallback = WebCore::AudioIOCallback;

Ref<AudioDestination> RemoteAudioDestinationProxy::create(AudioIOCallback& callback,
    const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    return adoptRef(*new RemoteAudioDestinationProxy(callback, inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate));
}

RemoteAudioDestinationProxy::RemoteAudioDestinationProxy(AudioIOCallback& callback, const String& inputDeviceId, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
    : m_callback(callback)
    , m_sampleRate(sampleRate)
{
    RemoteAudioDestinationIdentifier destinationID;
    unsigned framesPerBuffer { 0 };

    auto& connection = WebProcess::singleton().ensureGPUProcessConnection();
    connection.connection().sendSync(
        Messages::RemoteAudioDestinationManager::CreateAudioDestination(inputDeviceId, numberOfInputChannels, numberOfOutputChannels, sampleRate),
        Messages::RemoteAudioDestinationManager::CreateAudioDestination::Reply(destinationID, framesPerBuffer), 0);
    connection.messageReceiverMap().addMessageReceiver(Messages::RemoteAudioDestinationProxy::messageReceiverName(), destinationID.toUInt64(), *this);

    m_destinationID = destinationID;
    m_framesPerBuffer = framesPerBuffer;
}

RemoteAudioDestinationProxy::~RemoteAudioDestinationProxy()
{
    auto& connection =  WebProcess::singleton().ensureGPUProcessConnection();
    connection.messageReceiverMap().removeMessageReceiver(Messages::RemoteAudioDestinationProxy::messageReceiverName(), m_destinationID.toUInt64());

    connection.connection().sendWithAsyncReply(
        Messages::RemoteAudioDestinationManager::DeleteAudioDestination(m_destinationID), [] {
        // Can't remove this from proxyMap() here because the object would have been already deleted.
    });
}

void RemoteAudioDestinationProxy::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread)
{
    m_dispatchToRenderThread = WTFMove(dispatchToRenderThread);
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendSync(
        Messages::RemoteAudioDestinationManager::StartAudioDestination(m_destinationID),
        Messages::RemoteAudioDestinationManager::StartAudioDestination::Reply(m_isPlaying), 0);
}

void RemoteAudioDestinationProxy::stop()
{
    WebProcess::singleton().ensureGPUProcessConnection().connection().sendSync(
        Messages::RemoteAudioDestinationManager::StopAudioDestination(m_destinationID),
        Messages::RemoteAudioDestinationManager::StopAudioDestination::Reply(m_isPlaying), 0);
    m_dispatchToRenderThread = nullptr;
}

void RemoteAudioDestinationProxy::renderBuffer(const WebKit::RemoteAudioBusData& audioBusData, CompletionHandler<void()>&& completionHandler)
{
    // FIXME: This does rendering on the main thread when AudioWorklet is not active, which is likely not a good idea.
    ASSERT(isMainThread());
    ASSERT(audioBusData.framesToProcess);
    ASSERT(audioBusData.channelBuffers.size());
    auto audioBus = AudioBus::create(audioBusData.channelBuffers.size(), audioBusData.framesToProcess, false);
    for (unsigned i = 0; i < audioBusData.channelBuffers.size(); ++i)
        audioBus->setChannelMemory(i, (float*)audioBusData.channelBuffers[i]->data(), audioBusData.framesToProcess);

    auto doRender = [this, protectedThis = makeRef(*this), audioBus = WTFMove(audioBus), framesToProcess = audioBusData.framesToProcess, outputPosition = audioBusData.outputPosition] {
        m_callback.render(0, audioBus.get(), framesToProcess, outputPosition);
    };
    if (m_dispatchToRenderThread) {
        m_dispatchToRenderThread([doRender = WTFMove(doRender), completionHandler = WTFMove(completionHandler)]() mutable {
            doRender();
            callOnMainThread(WTFMove(completionHandler));
        });
    } else {
        doRender();
        completionHandler();
    }
}

void RemoteAudioDestinationProxy::didChangeIsPlaying(bool isPlaying)
{
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)
