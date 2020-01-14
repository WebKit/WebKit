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

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

#include "Decoder.h"
#include "RemoteAudioMediaStreamTrackRenderer.h"

namespace WebKit {

RemoteAudioMediaStreamTrackRendererManager::RemoteAudioMediaStreamTrackRendererManager() = default;

RemoteAudioMediaStreamTrackRendererManager::~RemoteAudioMediaStreamTrackRendererManager() = default;

void RemoteAudioMediaStreamTrackRendererManager::didReceiveRendererMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* renderer = m_renderers.get(makeObjectIdentifier<AudioMediaStreamTrackRendererIdentifierType>(decoder.destinationID())))
        renderer->didReceiveMessage(connection, decoder);
}

void RemoteAudioMediaStreamTrackRendererManager::createRenderer(AudioMediaStreamTrackRendererIdentifier identifier)
{
    ASSERT(!m_renderers.contains(identifier));
    m_renderers.add(identifier, makeUnique<RemoteAudioMediaStreamTrackRenderer>());
}

void RemoteAudioMediaStreamTrackRendererManager::releaseRenderer(AudioMediaStreamTrackRendererIdentifier identifier)
{
    ASSERT(m_renderers.contains(identifier));
    m_renderers.remove(identifier);
}

}

#endif // PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)

