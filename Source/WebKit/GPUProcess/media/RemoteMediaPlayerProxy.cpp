/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "RemoteMediaPlayerProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteMediaPlayerManagerProxy.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>

namespace WebKit {

using namespace WebCore;

RemoteMediaPlayerProxy::RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy& manager, MediaPlayerPrivateRemoteIdentifier id, Ref<IPC::Connection>&& connection, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier)
    : m_id(id)
    , m_webProcessConnection(WTFMove(connection))
    , m_manager(manager)
    , m_engineIdentifier(engineIdentifier)
#if !RELEASE_LOG_DISABLED
    , m_logger(m_manager.logger())
#endif
{
    m_player = MediaPlayer::create(*this, m_engineIdentifier);
}

void RemoteMediaPlayerProxy::invalidate()
{
    m_player->invalidate();
}

void RemoteMediaPlayerProxy::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    m_player->load(url, contentType, keySystem);
}

void RemoteMediaPlayerProxy::prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload preload, bool preservesPitch, bool prepareForRendering)
{
    m_player->setPrivateBrowsingMode(privateMode);
    m_player->setPreload(preload);
    m_player->setPreservesPitch(preservesPitch);
    m_player->prepareForRendering();
}

void RemoteMediaPlayerProxy::cancelLoad()
{
    m_player->cancelLoad();
}

void RemoteMediaPlayerProxy::play()
{
    m_player->play();
}

void RemoteMediaPlayerProxy::pause()
{
    m_player->pause();
}

void RemoteMediaPlayerProxy::setVolume(double volume)
{
    m_player->setVolume(volume);
}

void RemoteMediaPlayerProxy::setMuted(bool muted)
{
    m_player->setMuted(muted);
}

// MediaPlayerClient
void RemoteMediaPlayerProxy::mediaPlayerNetworkStateChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::NetworkStateChanged(m_id, m_player->networkState()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerReadyStateChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::ReadyStateChanged(m_id, m_player->readyState()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerVolumeChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::VolumeChanged(m_id, m_player->volume()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerMuteChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::MuteChanged(m_id, m_player->muted()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerTimeChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::TimeChanged(m_id, m_player->currentTime()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerDurationChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::DurationChanged(m_id, m_player->duration()), 0);
}

void RemoteMediaPlayerProxy::mediaPlayerRateChanged()
{
    m_webProcessConnection->send(Messages::RemoteMediaPlayerManager::RateChanged(m_id, m_player->rate()), 0);
}

} // namespace WebKit

#undef MESSAGE_CHECK_CONTEXTID

#endif // ENABLE(GPU_PROCESS)
