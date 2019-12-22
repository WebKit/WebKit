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
#include "RemoteMediaPlayerManager.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteMediaPlayerManagerMessages.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Settings.h>
#include <wtf/Assertions.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

class MediaPlayerRemoteFactory final : public MediaPlayerFactory {
public:
    MediaPlayerRemoteFactory(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, RemoteMediaPlayerManager& manager)
        : m_remoteEngineIdentifier(remoteEngineIdentifier)
        , m_manager(manager)
    {
    }

    MediaPlayerEnums::MediaEngineIdentifier identifier() const final { return m_remoteEngineIdentifier; };

    std::unique_ptr<MediaPlayerPrivateInterface> createMediaEnginePlayer(MediaPlayer* player) const final
    {
        return m_manager.createRemoteMediaPlayer(player, m_remoteEngineIdentifier);
    }

    void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types) const final
    {
        return m_manager.getSupportedTypes(m_remoteEngineIdentifier, types);
    }

    MediaPlayer::SupportsType supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters) const final
    {
        return m_manager.supportsTypeAndCodecs(m_remoteEngineIdentifier, parameters);
    }

    HashSet<RefPtr<SecurityOrigin>> originsInMediaCache(const String& path) const final
    {
        return m_manager.originsInMediaCache(m_remoteEngineIdentifier, path);
    }

    void clearMediaCache(const String& path, WallTime modifiedSince) const final
    {
        return m_manager.clearMediaCache(m_remoteEngineIdentifier, path, modifiedSince);
    }

    void clearMediaCacheForOrigins(const String& path, const HashSet<RefPtr<SecurityOrigin>>& origins) const final
    {
        return m_manager.clearMediaCacheForOrigins(m_remoteEngineIdentifier, path, origins);
    }

    bool supportsKeySystem(const String& keySystem, const String& mimeType) const final
    {
        return m_manager.supportsKeySystem(m_remoteEngineIdentifier, keySystem, mimeType);
    }

private:
    MediaPlayerEnums::MediaEngineIdentifier m_remoteEngineIdentifier;
    RemoteMediaPlayerManager& m_manager;
};

RemoteMediaPlayerManager::RemoteMediaPlayerManager(WebProcess& process)
    : m_process(process)
{
}

RemoteMediaPlayerManager::~RemoteMediaPlayerManager()
{
}

const char* RemoteMediaPlayerManager::supplementName()
{
    return "RemoteMediaPlayerManager";
}

void RemoteMediaPlayerManager::initialize(const WebProcessCreationParameters&)
{
// FIXME: Use parameters.mediaMIMETypes.
}

std::unique_ptr<MediaPlayerPrivateInterface> RemoteMediaPlayerManager::createRemoteMediaPlayer(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier)
{
    auto id = MediaPlayerPrivateRemoteIdentifier::generate();

    RemoteMediaPlayerProxyConfiguration configuration;
    configuration.mediaKeysStorageDirectory = player->mediaKeysStorageDirectory();
    configuration.referrer = player->referrer();
    configuration.userAgent = player->userAgent();
    configuration.sourceApplicationIdentifier = player->sourceApplicationIdentifier();
#if PLATFORM(IOS_FAMILY)
    configuration.networkInterfaceName = player->mediaPlayerNetworkInterfaceName();
#endif
    configuration.mediaCacheDirectory = player->mediaCacheDirectory();
    configuration.mediaContentTypesRequiringHardwareSupport = player->mediaContentTypesRequiringHardwareSupport();
    configuration.preferredAudioCharacteristics = player->preferredAudioCharacteristics();
    configuration.logIdentifier = reinterpret_cast<uint64_t>(player->mediaPlayerLogIdentifier());
    configuration.shouldUsePersistentCache = player->shouldUsePersistentCache();
    configuration.isVideo = player->isVideoPlayer();

    bool haveRemoteProxy;
    bool sendSucceeded = gpuProcessConnection().sendSync(Messages::RemoteMediaPlayerManagerProxy::CreateMediaPlayer(id, remoteEngineIdentifier, configuration), Messages::RemoteMediaPlayerManagerProxy::CreateMediaPlayer::Reply(haveRemoteProxy), 0);
    if (!haveRemoteProxy || !sendSucceeded) {
        WTFLogAlways("Failed to create remote media player.");
        return nullptr;
    }

    auto remotePlayer = MediaPlayerPrivateRemote::create(player, remoteEngineIdentifier, id, *this);
    m_players.add(id, makeWeakPtr(*remotePlayer));
    return remotePlayer;
}

void RemoteMediaPlayerManager::deleteRemoteMediaPlayer(MediaPlayerPrivateRemoteIdentifier id)
{
    m_players.take(id);
    gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::DeleteMediaPlayer(id), 0);
}

void RemoteMediaPlayerManager::getSupportedTypes(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, HashSet<String, ASCIICaseInsensitiveHash>& result)
{
    // FIXME: supported types don't change, cache them.

    Vector<String> types;
    if (!gpuProcessConnection().sendSync(Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes(remoteEngineIdentifier), Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes::Reply(types), 0))
        return;

    result = HashSet<String, ASCIICaseInsensitiveHash>();
    for (auto& type : types)
        result.add(type);
}

MediaPlayer::SupportsType RemoteMediaPlayerManager::supportsTypeAndCodecs(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const WebCore::MediaEngineSupportParameters& parameters)
{
    // FIXME: supported types don't change, cache them.

    MediaPlayer::SupportsType result;
    if (!gpuProcessConnection().sendSync(Messages::RemoteMediaPlayerManagerProxy::SupportsType(remoteEngineIdentifier, parameters), Messages::RemoteMediaPlayerManagerProxy::SupportsType::Reply(result), 0))
        return MediaPlayer::SupportsType::IsNotSupported;

    return result;
}

bool RemoteMediaPlayerManager::supportsKeySystem(MediaPlayerEnums::MediaEngineIdentifier, const String& keySystem, const String& mimeType)
{
    return false;
}

HashSet<RefPtr<WebCore::SecurityOrigin>> RemoteMediaPlayerManager::originsInMediaCache(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path)
{
    Vector<WebCore::SecurityOriginData> originData;
    if (!gpuProcessConnection().sendSync(Messages::RemoteMediaPlayerManagerProxy::OriginsInMediaCache(remoteEngineIdentifier, path), Messages::RemoteMediaPlayerManagerProxy::OriginsInMediaCache::Reply(originData), 0))
        return { };

    HashSet<RefPtr<SecurityOrigin>> origins;
    for (auto& data : originData)
        origins.add(data.securityOrigin());

    return origins;
}

void RemoteMediaPlayerManager::clearMediaCache(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path, WallTime modifiedSince)
{
    gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::ClearMediaCache(remoteEngineIdentifier, path, modifiedSince), 0);
}

void RemoteMediaPlayerManager::clearMediaCacheForOrigins(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path, const HashSet<RefPtr<WebCore::SecurityOrigin>>& origins)
{
    auto originData = WTF::map(origins, [] (auto& origin) {
        return origin->data();
    });

    gpuProcessConnection().send(Messages::RemoteMediaPlayerManagerProxy::ClearMediaCacheForOrigins(remoteEngineIdentifier, path, originData), 0);
}

void RemoteMediaPlayerManager::networkStateChanged(MediaPlayerPrivateRemoteIdentifier id, RemoteMediaPlayerState&& state)
{
    if (auto player = m_players.get(id))
        player->networkStateChanged(WTFMove(state));
}

void RemoteMediaPlayerManager::readyStateChanged(MediaPlayerPrivateRemoteIdentifier id, RemoteMediaPlayerState&& state)
{
    if (auto player = m_players.get(id))
        player->readyStateChanged(WTFMove(state));
}

void RemoteMediaPlayerManager::volumeChanged(WebKit::MediaPlayerPrivateRemoteIdentifier id, double volume)
{
    if (auto player = m_players.get(id))
        player->volumeChanged(volume);
}

void RemoteMediaPlayerManager::muteChanged(WebKit::MediaPlayerPrivateRemoteIdentifier id, bool mute)
{
    if (auto player = m_players.get(id))
        player->muteChanged(mute);
}

void RemoteMediaPlayerManager::timeChanged(WebKit::MediaPlayerPrivateRemoteIdentifier id, RemoteMediaPlayerState&& state)
{
    if (auto player = m_players.get(id))
        player->timeChanged(WTFMove(state));
}

void RemoteMediaPlayerManager::durationChanged(WebKit::MediaPlayerPrivateRemoteIdentifier id, RemoteMediaPlayerState&& state)
{
    if (auto player = m_players.get(id))
        player->durationChanged(WTFMove(state));
}

void RemoteMediaPlayerManager::rateChanged(WebKit::MediaPlayerPrivateRemoteIdentifier id, double rate)
{
    if (auto player = m_players.get(id))
        player->rateChanged(rate);
}

void RemoteMediaPlayerManager::playbackStateChanged(WebKit::MediaPlayerPrivateRemoteIdentifier id, bool paused)
{
    if (auto player = m_players.get(id))
        player->playbackStateChanged(paused);
}

void RemoteMediaPlayerManager::engineFailedToLoad(WebKit::MediaPlayerPrivateRemoteIdentifier id, long platformErrorCode)
{
    if (auto player = m_players.get(id))
        player->engineFailedToLoad(platformErrorCode);
}

void RemoteMediaPlayerManager::characteristicChanged(WebKit::MediaPlayerPrivateRemoteIdentifier id, bool hasAudio, bool hasVideo, WebCore::MediaPlayerEnums::MovieLoadType loadType)
{
    if (auto player = m_players.get(id))
        player->characteristicChanged(hasAudio, hasVideo, loadType);
}

void RemoteMediaPlayerManager::updatePreferences(const Settings& settings)
{
    auto registerEngine = [this](MediaEngineRegistrar registrar, MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier) {
        registrar(makeUnique<MediaPlayerRemoteFactory>(remoteEngineIdentifier, *this));
    };

    RemoteMediaPlayerSupport::setRegisterRemotePlayerCallback(settings.useGPUProcessForMedia() ? WTFMove(registerEngine) : RemoteMediaPlayerSupport::RegisterRemotePlayerCallback());
}

void RemoteMediaPlayerManager::updateCachedState(WebKit::MediaPlayerPrivateRemoteIdentifier id, RemoteMediaPlayerState&& state)
{
    if (auto player = m_players.get(id))
        player->updateCachedState(WTFMove(state));
}

IPC::Connection& RemoteMediaPlayerManager::gpuProcessConnection() const
{
    return WebProcess::singleton().ensureGPUProcessConnection().connection();
}

}

#endif
