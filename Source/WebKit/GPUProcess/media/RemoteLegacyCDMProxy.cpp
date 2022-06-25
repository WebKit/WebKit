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
#include "RemoteLegacyCDMProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteLegacyCDMProxy> RemoteLegacyCDMProxy::create(WeakPtr<RemoteLegacyCDMFactoryProxy> factory, MediaPlayerIdentifier&& playerId, std::unique_ptr<WebCore::LegacyCDM>&& cdm)
{
    return std::unique_ptr<RemoteLegacyCDMProxy>(new RemoteLegacyCDMProxy(WTFMove(factory), WTFMove(playerId), WTFMove(cdm)));
}

RemoteLegacyCDMProxy::RemoteLegacyCDMProxy(WeakPtr<RemoteLegacyCDMFactoryProxy>&& factory, MediaPlayerIdentifier&& playerId, std::unique_ptr<WebCore::LegacyCDM>&& cdm)
    : m_factory(WTFMove(factory))
    , m_playerId(WTFMove(playerId))
    , m_cdm(WTFMove(cdm))
{
    m_cdm->setClient(this);
}

RemoteLegacyCDMProxy::~RemoteLegacyCDMProxy() = default;

void RemoteLegacyCDMProxy::supportsMIMEType(const String& mimeType, SupportsMIMETypeCallback&& callback)
{
    if (!m_cdm) {
        callback(false);
        return;
    }

    callback(m_cdm->supportsMIMEType(mimeType));
}

void RemoteLegacyCDMProxy::createSession(const String& keySystem, uint64_t logIdentifier, CreateSessionCallback&& callback)
{
    if (!m_cdm || !m_factory) {
        callback({ });
        return;
    }

    auto sessionIdentifier = RemoteLegacyCDMSessionIdentifier::generate();
    auto session = RemoteLegacyCDMSessionProxy::create(*m_factory, logIdentifier, sessionIdentifier, *m_cdm);
    m_factory->addSession(sessionIdentifier, WTFMove(session));
    callback(WTFMove(sessionIdentifier));
}

void RemoteLegacyCDMProxy::setPlayerId(std::optional<MediaPlayerIdentifier>&& playerId)
{
    if (!playerId)
        m_playerId = { };
    m_playerId = WTFMove(*playerId);
}

RefPtr<MediaPlayer> RemoteLegacyCDMProxy::cdmMediaPlayer(const LegacyCDM*) const
{
    if (!m_playerId || !m_factory)
        return nullptr;

    auto* gpuConnectionToWebProcess = m_factory->gpuConnectionToWebProcess();
    if (!gpuConnectionToWebProcess)
        return nullptr;

    return gpuConnectionToWebProcess->remoteMediaPlayerManagerProxy().mediaPlayer(m_playerId);
}

}

#endif
