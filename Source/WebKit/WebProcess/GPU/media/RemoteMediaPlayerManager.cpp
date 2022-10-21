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

#include "MediaPlayerPrivateRemote.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerMIMETypeCache.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "SampleBufferDisplayLayerManager.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/ContentTypeUtilities.h>
#include <WebCore/MediaPlayer.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(COCOA)
#include <WebCore/MediaPlayerPrivateMediaStreamAVFObjC.h>
#endif

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

    HashSet<SecurityOriginData> originsInMediaCache(const String& path) const final
    {
        return m_manager.originsInMediaCache(m_remoteEngineIdentifier, path);
    }

    void clearMediaCache(const String& path, WallTime modifiedSince) const final
    {
        return m_manager.clearMediaCache(m_remoteEngineIdentifier, path, modifiedSince);
    }

    void clearMediaCacheForOrigins(const String& path, const HashSet<SecurityOriginData>& origins) const final
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
#if PLATFORM(COCOA)
    if (parameters.mediaMIMETypes.isEmpty())
        return;

    auto& cache = typeCache(MediaPlayerEnums::MediaEngineIdentifier::AVFoundation);
    if (cache.isEmpty())
        cache.addSupportedTypes(parameters.mediaMIMETypes);
#else
    UNUSED_PARAM(parameters);
#endif
}

std::unique_ptr<MediaPlayerPrivateInterface> RemoteMediaPlayerManager::createRemoteMediaPlayer(MediaPlayer* player, MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier)
{
    RemoteMediaPlayerProxyConfiguration proxyConfiguration;
    proxyConfiguration.referrer = player->referrer();
    proxyConfiguration.userAgent = player->userAgent();
    proxyConfiguration.sourceApplicationIdentifier = player->sourceApplicationIdentifier();
#if PLATFORM(IOS_FAMILY)
    proxyConfiguration.networkInterfaceName = player->mediaPlayerNetworkInterfaceName();
#endif
    proxyConfiguration.mediaContentTypesRequiringHardwareSupport = player->mediaContentTypesRequiringHardwareSupport();
    proxyConfiguration.renderingCanBeAccelerated = player->renderingCanBeAccelerated();
    proxyConfiguration.preferredAudioCharacteristics = player->preferredAudioCharacteristics();
#if !RELEASE_LOG_DISABLED
    proxyConfiguration.logIdentifier = reinterpret_cast<uint64_t>(player->mediaPlayerLogIdentifier());
#endif
    proxyConfiguration.shouldUsePersistentCache = player->shouldUsePersistentCache();
    proxyConfiguration.isVideo = player->isVideoPlayer();

#if ENABLE(AVF_CAPTIONS)
    proxyConfiguration.outOfBandTrackData = player->outOfBandTrackSources().map([](auto& track) {
        return track->data();
    });
#endif

    auto documentSecurityOrigin = player->documentSecurityOrigin();
    proxyConfiguration.documentSecurityOrigin = documentSecurityOrigin;

    proxyConfiguration.presentationSize = player->presentationSize();

    proxyConfiguration.allowedMediaContainerTypes = player->allowedMediaContainerTypes();
    proxyConfiguration.allowedMediaCodecTypes = player->allowedMediaCodecTypes();
    proxyConfiguration.allowedMediaVideoCodecIDs = player->allowedMediaVideoCodecIDs();
    proxyConfiguration.allowedMediaAudioCodecIDs = player->allowedMediaAudioCodecIDs();
    proxyConfiguration.allowedMediaCaptionFormatTypes = player->allowedMediaCaptionFormatTypes();
    proxyConfiguration.playerContentBoxRect = player->playerContentBoxRect();

    proxyConfiguration.prefersSandboxedParsing = player->prefersSandboxedParsing();

    auto identifier = MediaPlayerIdentifier::generate();
    gpuProcessConnection().connection().send(Messages::RemoteMediaPlayerManagerProxy::CreateMediaPlayer(identifier, remoteEngineIdentifier, proxyConfiguration), 0);

    auto remotePlayer = MediaPlayerPrivateRemote::create(player, remoteEngineIdentifier, identifier, *this);
    m_players.add(identifier, *remotePlayer);

    return remotePlayer;
}

void RemoteMediaPlayerManager::deleteRemoteMediaPlayer(MediaPlayerIdentifier identifier)
{
    m_players.take(identifier);
    gpuProcessConnection().connection().send(Messages::RemoteMediaPlayerManagerProxy::DeleteMediaPlayer(identifier), 0);
}

MediaPlayerIdentifier RemoteMediaPlayerManager::findRemotePlayerId(const MediaPlayerPrivateInterface* player)
{
    for (auto pair : m_players) {
        if (pair.value == player)
            return pair.key;
    }

    return { };
}

void RemoteMediaPlayerManager::getSupportedTypes(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, HashSet<String, ASCIICaseInsensitiveHash>& result)
{
    result = typeCache(remoteEngineIdentifier).supportedTypes();
}

MediaPlayer::SupportsType RemoteMediaPlayerManager::supportsTypeAndCodecs(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const MediaEngineSupportParameters& parameters)
{
#if ENABLE(MEDIA_STREAM)
    if (parameters.isMediaStream)
        return MediaPlayer::SupportsType::IsNotSupported;
#endif

    if (!contentTypeMeetsContainerAndCodecTypeRequirements(parameters.type, parameters.allowedMediaContainerTypes, parameters.allowedMediaCodecTypes))
        return MediaPlayer::SupportsType::IsNotSupported;

    return typeCache(remoteEngineIdentifier).supportsTypeAndCodecs(parameters);
}

bool RemoteMediaPlayerManager::supportsKeySystem(MediaPlayerEnums::MediaEngineIdentifier, const String& keySystem, const String& mimeType)
{
    return false;
}

HashSet<SecurityOriginData> RemoteMediaPlayerManager::originsInMediaCache(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path)
{
    auto sendResult = gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::OriginsInMediaCache(remoteEngineIdentifier, path), 0);
    auto [originData] = sendResult.takeReplyOr(HashSet<SecurityOriginData> { });
    return originData;
}

void RemoteMediaPlayerManager::clearMediaCache(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path, WallTime modifiedSince)
{
    gpuProcessConnection().connection().send(Messages::RemoteMediaPlayerManagerProxy::ClearMediaCache(remoteEngineIdentifier, path, modifiedSince), 0);
}

void RemoteMediaPlayerManager::clearMediaCacheForOrigins(MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier, const String& path, const HashSet<SecurityOriginData>& origins)
{
    gpuProcessConnection().connection().send(Messages::RemoteMediaPlayerManagerProxy::ClearMediaCacheForOrigins(remoteEngineIdentifier, path, origins), 0);
}

void RemoteMediaPlayerManager::didReceivePlayerMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (const auto& player = m_players.get(makeObjectIdentifier<MediaPlayerIdentifierType>(decoder.destinationID())))
        player->didReceiveMessage(connection, decoder);
}

void RemoteMediaPlayerManager::setUseGPUProcess(bool useGPUProcess)
{
    auto registerEngine = [this](MediaEngineRegistrar registrar, MediaPlayerEnums::MediaEngineIdentifier remoteEngineIdentifier) {
        registrar(makeUnique<MediaPlayerRemoteFactory>(remoteEngineIdentifier, *this));
    };

    RemoteMediaPlayerSupport::setRegisterRemotePlayerCallback(useGPUProcess ? WTFMove(registerEngine) : RemoteMediaPlayerSupport::RegisterRemotePlayerCallback());

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    if (useGPUProcess) {
        WebCore::SampleBufferDisplayLayer::setCreator([](auto& client) {
            return WebProcess::singleton().ensureGPUProcessConnection().sampleBufferDisplayLayerManager().createLayer(client);
        });
        WebCore::MediaPlayerPrivateMediaStreamAVFObjC::setNativeImageCreator([](auto& videoFrame) {
            return WebProcess::singleton().ensureGPUProcessConnection().videoFrameObjectHeapProxy().getNativeImage(videoFrame);
        });
    }
#endif
}

GPUProcessConnection& RemoteMediaPlayerManager::gpuProcessConnection() const
{
    if (!m_gpuProcessConnection) {
        m_gpuProcessConnection = &WebProcess::singleton().ensureGPUProcessConnection();
        m_gpuProcessConnection->addClient(*this);
    }

    return *m_gpuProcessConnection;
}

void RemoteMediaPlayerManager::gpuProcessConnectionDidClose(GPUProcessConnection& connection)
{
    ASSERT(m_gpuProcessConnection == &connection);
    connection.removeClient(*this);

    m_gpuProcessConnection = nullptr;

    auto players = m_players;
    for (auto& player : players.values()) {
        if (player) {
            player->player()->reloadAndResumePlaybackIfNeeded();
            ASSERT_WITH_MESSAGE(!player, "reloadAndResumePlaybackIfNeeded should destroy this player and construct a new one");
        }
    }
}

} // namespace WebKit

#endif
