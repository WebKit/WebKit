/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#include "AudioMediaStreamTrackRenderer.h"
#include "MediaPlayerPrivateRemote.h"
#include "RemoteAudioMediaStreamTrackRenderer.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerMIMETypeCache.h"
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

using RemotePlayerTypeCache = HashMap<MediaPlayerEnums::MediaEngineIdentifier, std::unique_ptr<RemoteMediaPlayerMIMETypeCache>, WTF::IntHash<MediaPlayerEnums::MediaEngineIdentifier>, WTF::StrongEnumHashTraits<MediaPlayerEnums::MediaEngineIdentifier>>;
static RemotePlayerTypeCache& mimeCaches()
{
    static NeverDestroyed<RemotePlayerTypeCache> caches;
    return caches;
}

RemoteMediaPlayerMIMETypeCache& RemoteMediaPlayerManager::typeCache(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier)
{
    auto& cachePtr = mimeCaches().add(remoteEngineIdentifier, nullptr).iterator->value;
    if (!cachePtr)
        cachePtr = makeUnique<RemoteMediaPlayerMIMETypeCache>(*this, remoteEngineIdentifier);

    return *cachePtr;
}

void RemoteMediaPlayerManager::initialize(const WebProcessCreationParameters& parameters)
{
    UNUSED_PARAM(parameters);

#if PLATFORM(COCOA)
    if (parameters.mediaMIMETypes.isEmpty())
        return;

    auto& cache = typeCache(MediaPlayerEnums::MediaEngineIdentifier::AVFoundation);
    if (cache.isEmpty())
        cache.addSupportedTypes(parameters.mediaMIMETypes);
#endif
}

std::unique_ptr<MediaPlayerPrivateInterface> RemoteMediaPlayerManager::createRemoteMediaPlayer(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier)
{
    auto id = MediaPlayerPrivateRemoteIdentifier::generate();

    RemoteMediaPlayerProxyConfiguration proxyConfiguration;
    proxyConfiguration.referrer = player->referrer();
    proxyConfiguration.userAgent = player->userAgent();
    proxyConfiguration.sourceApplicationIdentifier = player->sourceApplicationIdentifier();
#if PLATFORM(IOS_FAMILY)
    proxyConfiguration.networkInterfaceName = player->mediaPlayerNetworkInterfaceName();
#endif
    proxyConfiguration.mediaContentTypesRequiringHardwareSupport = player->mediaContentTypesRequiringHardwareSupport();
    proxyConfiguration.preferredAudioCharacteristics = player->preferredAudioCharacteristics();
    proxyConfiguration.logIdentifier = reinterpret_cast<uint64_t>(player->mediaPlayerLogIdentifier());
    proxyConfiguration.shouldUsePersistentCache = player->shouldUsePersistentCache();
    proxyConfiguration.isVideo = player->isVideoPlayer();

    RemoteMediaPlayerConfiguration playerConfiguration;
    bool sendSucceeded = gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::CreateMediaPlayer(id, remoteEngineIdentifier, proxyConfiguration), Messages::RemoteMediaPlayerManagerProxy::CreateMediaPlayer::Reply(playerConfiguration), 0);
    if (!sendSucceeded) {
        WTFLogAlways("Failed to create remote media player.");
        return nullptr;
    }

    auto remotePlayer = MediaPlayerPrivateRemote::create(player, remoteEngineIdentifier, id, *this, playerConfiguration);
    m_players.add(id, makeWeakPtr(*remotePlayer));
    return remotePlayer;
}

void RemoteMediaPlayerManager::deleteRemoteMediaPlayer(MediaPlayerPrivateRemoteIdentifier id)
{
    m_players.take(id);
    gpuProcessConnection().connection().send(Messages::RemoteMediaPlayerManagerProxy::DeleteMediaPlayer(id), 0);
}

void RemoteMediaPlayerManager::getSupportedTypes(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, HashSet<String, ASCIICaseInsensitiveHash>& result)
{
    auto& cache = typeCache(remoteEngineIdentifier);
    if (!cache.isEmpty()) {
        result = HashSet<String, ASCIICaseInsensitiveHash>();
        for (auto& type : cache.supportedTypes())
            result.add(type);
        return;
    }

    Vector<String> types;
    if (!gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes(remoteEngineIdentifier), Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes::Reply(types), 0))
        return;

    result = HashSet<String, ASCIICaseInsensitiveHash>();
    for (auto& type : types)
        result.add(type);
    cache.addSupportedTypes(types);
}

MediaPlayer::SupportsType RemoteMediaPlayerManager::supportsTypeAndCodecs(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const MediaEngineSupportParameters& parameters)
{
    return typeCache(remoteEngineIdentifier).supportsTypeAndCodecs(parameters);
}

bool RemoteMediaPlayerManager::supportsKeySystem(MediaPlayerEnums::MediaEngineIdentifier, const String& keySystem, const String& mimeType)
{
    return false;
}

HashSet<RefPtr<SecurityOrigin>> RemoteMediaPlayerManager::originsInMediaCache(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path)
{
    Vector<SecurityOriginData> originData;
    if (!gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::OriginsInMediaCache(remoteEngineIdentifier, path), Messages::RemoteMediaPlayerManagerProxy::OriginsInMediaCache::Reply(originData), 0))
        return { };

    HashSet<RefPtr<SecurityOrigin>> origins;
    for (auto& data : originData)
        origins.add(data.securityOrigin());

    return origins;
}

void RemoteMediaPlayerManager::clearMediaCache(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path, WallTime modifiedSince)
{
    gpuProcessConnection().connection().send(Messages::RemoteMediaPlayerManagerProxy::ClearMediaCache(remoteEngineIdentifier, path, modifiedSince), 0);
}

void RemoteMediaPlayerManager::clearMediaCacheForOrigins(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path, const HashSet<RefPtr<SecurityOrigin>>& origins)
{
    auto originData = WTF::map(origins, [] (auto& origin) {
        return origin->data();
    });

    gpuProcessConnection().connection().send(Messages::RemoteMediaPlayerManagerProxy::ClearMediaCacheForOrigins(remoteEngineIdentifier, path, originData), 0);
}

void RemoteMediaPlayerManager::didReceivePlayerMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (const auto& player = m_players.get(makeObjectIdentifier<MediaPlayerPrivateRemoteIdentifierType>(decoder.destinationID())))
        player->didReceiveMessage(connection, decoder);
}

void RemoteMediaPlayerManager::updatePreferences(const Settings& settings)
{
    auto registerEngine = [this](MediaEngineRegistrar registrar, MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier) {
        registrar(makeUnique<MediaPlayerRemoteFactory>(remoteEngineIdentifier, *this));
    };

    RemoteMediaPlayerSupport::setRegisterRemotePlayerCallback(settings.useGPUProcessForMedia() ? WTFMove(registerEngine) : RemoteMediaPlayerSupport::RegisterRemotePlayerCallback());

#if PLATFORM(COCOA) && ENABLE(VIDEO_TRACK) && ENABLE(MEDIA_STREAM)
    if (settings.useGPUProcessForMedia()) {
        WebCore::AudioMediaStreamTrackRenderer::setCreator(WebKit::AudioMediaStreamTrackRenderer::create);
        WebCore::SampleBufferDisplayLayer::setCreator([](auto& client, bool hideRootLayer, auto size) {
            return WebProcess::singleton().ensureGPUProcessConnection().sampleBufferDisplayLayerManager().createLayer(client, hideRootLayer, size);
        });
    }
#endif
}

GPUProcessConnection& RemoteMediaPlayerManager::gpuProcessConnection() const
{
    if (!m_gpuProcessConnection)
        m_gpuProcessConnection = &WebProcess::singleton().ensureGPUProcessConnection();

    return *m_gpuProcessConnection;
}

} // namespace WebKit

#endif
