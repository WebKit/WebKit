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
#include "RemoteAudioMediaStreamTrackRendererManager.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "Decoder.h"
#include "GPUProcess.h"
#include "RemoteAudioMediaStreamTrackRenderer.h"
#include "RemoteAudioMediaStreamTrackRendererManagerMessages.h"
#include "RemoteAudioMediaStreamTrackRendererMessages.h"

namespace WebKit {


RemoteAudioMediaStreamTrackRendererManager::RemoteAudioMediaStreamTrackRendererManager(GPUConnectionToWebProcess& connectionToWebProcess)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_queue(connectionToWebProcess.gpuProcess().audioMediaStreamTrackRendererQueue())
{
    m_connectionToWebProcess.connection().addThreadMessageReceiver(Messages::RemoteAudioMediaStreamTrackRenderer::messageReceiverName(), this);
    m_connectionToWebProcess.connection().addThreadMessageReceiver(Messages::RemoteAudioMediaStreamTrackRendererManager::messageReceiverName(), this);
}

RemoteAudioMediaStreamTrackRendererManager::~RemoteAudioMediaStreamTrackRendererManager()
{
    m_connectionToWebProcess.connection().removeThreadMessageReceiver(Messages::RemoteAudioMediaStreamTrackRenderer::messageReceiverName());
    m_connectionToWebProcess.connection().removeThreadMessageReceiver(Messages::RemoteAudioMediaStreamTrackRendererManager::messageReceiverName());
}

void RemoteAudioMediaStreamTrackRendererManager::close()
{
    dispatchToThread([this, protectedThis = makeRef(*this)] {
        m_renderers.clear();
    });
}

void RemoteAudioMediaStreamTrackRendererManager::dispatchToThread(Function<void()>&& callback)
{
    m_queue->dispatch(WTFMove(callback));
}

void RemoteAudioMediaStreamTrackRendererManager::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (!decoder.destinationID()) {
        if (decoder.messageName() == Messages::RemoteAudioMediaStreamTrackRendererManager::CreateRenderer::name()) {
            IPC::handleMessage<Messages::RemoteAudioMediaStreamTrackRendererManager::CreateRenderer>(decoder, this, &RemoteAudioMediaStreamTrackRendererManager::createRenderer);
            return;
        }
        if (decoder.messageName() == Messages::RemoteAudioMediaStreamTrackRendererManager::ReleaseRenderer::name()) {
            IPC::handleMessage<Messages::RemoteAudioMediaStreamTrackRendererManager::ReleaseRenderer>(decoder, this, &RemoteAudioMediaStreamTrackRendererManager::releaseRenderer);
            return;
        }
        return;
    }
    if (auto* renderer = m_renderers.get(makeObjectIdentifier<AudioMediaStreamTrackRendererIdentifierType>(decoder.destinationID())))
        renderer->didReceiveMessage(connection, decoder);
}

void RemoteAudioMediaStreamTrackRendererManager::createRenderer(AudioMediaStreamTrackRendererIdentifier identifier)
{
    ASSERT(!m_renderers.contains(identifier));
    m_renderers.add(identifier, makeUnique<RemoteAudioMediaStreamTrackRenderer>(*this));
}

void RemoteAudioMediaStreamTrackRendererManager::releaseRenderer(AudioMediaStreamTrackRendererIdentifier identifier)
{
    ASSERT(m_renderers.contains(identifier));
    m_renderers.remove(identifier);
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)
