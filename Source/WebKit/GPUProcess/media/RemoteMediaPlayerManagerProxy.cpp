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
#include "RemoteMediaPlayerManagerProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <wtf/UniqueRef.h>

#if PLATFORM(COCOA)
#include <WebCore/AVAssetMIMETypeCache.h>
#endif

namespace WebKit {

using namespace WebCore;

RemoteMediaPlayerManagerProxy::RemoteMediaPlayerManagerProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(WTF::LoggerHelper::uniqueLogIdentifier())
#endif
{
}

RemoteMediaPlayerManagerProxy::~RemoteMediaPlayerManagerProxy()
{
}

void RemoteMediaPlayerManagerProxy::createMediaPlayer(MediaPlayerPrivateRemoteIdentifier id, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& proxyConfiguration, CompletionHandler<void(RemoteMediaPlayerConfiguration&)>&& completionHandler)
{
    ASSERT(!m_proxies.contains(id));

    RemoteMediaPlayerConfiguration playerConfiguration;

    auto proxy = makeUnique<RemoteMediaPlayerProxy>(*this, id, m_gpuConnectionToWebProcess.connection(), engineIdentifier, WTFMove(proxyConfiguration));
    proxy->getConfiguration(playerConfiguration);
    m_proxies.add(id, WTFMove(proxy));

    completionHandler(playerConfiguration);
}

void RemoteMediaPlayerManagerProxy::deleteMediaPlayer(MediaPlayerPrivateRemoteIdentifier id)
{
    if (auto proxy = m_proxies.take(id))
        proxy->invalidate();
}

void RemoteMediaPlayerManagerProxy::getSupportedTypes(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        completionHandler({ });
        return;
    }

    HashSet<String, ASCIICaseInsensitiveHash> engineTypes;
    engine->getSupportedTypes(engineTypes);

    auto result = WTF::map(engineTypes, [] (auto& type) {
        return type;
    });

    completionHandler(WTFMove(result));
}

void RemoteMediaPlayerManagerProxy::supportsTypeAndCodecs(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const WebCore::MediaEngineSupportParameters&& parameters, CompletionHandler<void(MediaPlayer::SupportsType)>&& completionHandler)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        completionHandler(MediaPlayer::SupportsType::IsNotSupported);
        return;
    }

    auto result = engine->supportsTypeAndCodecs(parameters);
    completionHandler(result);
}

void RemoteMediaPlayerManagerProxy::canDecodeExtendedType(WebCore::MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const String&& mimeType, CompletionHandler<void(bool)>&& completionHandler)
{
    bool supported = false;

    switch (engineIdentifier) {
    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundation:
#if PLATFORM(COCOA)
        supported = AVAssetMIMETypeCache::singleton().canDecodeType(mimeType) == MediaPlayerEnums::SupportsType::IsSupported;
        break;
#endif

    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE:
    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMediaStream:
    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundationCF:
    case MediaPlayerEnums::MediaEngineIdentifier::GStreamer:
    case MediaPlayerEnums::MediaEngineIdentifier::GStreamerMSE:
    case MediaPlayerEnums::MediaEngineIdentifier::HolePunch:
    case MediaPlayerEnums::MediaEngineIdentifier::MediaFoundation:
    case MediaPlayerEnums::MediaEngineIdentifier::MockMSE:
        ASSERT_NOT_REACHED();
        break;
    }

    completionHandler(supported);
}

void RemoteMediaPlayerManagerProxy::originsInMediaCache(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const String&& path, CompletionHandler<void(Vector<WebCore::SecurityOriginData>&&)>&& completionHandler)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        completionHandler({ });
        return;
    }

    auto origins = engine->originsInMediaCache(path);

    Vector<WebCore::SecurityOriginData> result;
    for (auto& origin : origins)
        result.append(origin->data());

    completionHandler(WTFMove(result));
}

void RemoteMediaPlayerManagerProxy::clearMediaCache(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const String&&path, WallTime modifiedSince)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        return;
    }

    engine->clearMediaCache(path, modifiedSince);
}

void RemoteMediaPlayerManagerProxy::clearMediaCacheForOrigins(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const String&& path, Vector<WebCore::SecurityOriginData>&& originData)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        return;
    }

    HashSet<RefPtr<SecurityOrigin>> origins;
    for (auto& data : originData)
        origins.add(data.securityOrigin());

    engine->clearMediaCacheForOrigins(path, origins);
}

void RemoteMediaPlayerManagerProxy::supportsKeySystem(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const String&& keySystem, const String&& mimeType, CompletionHandler<void(bool)>&& completionHandler)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        return;
    }

    auto result = engine->supportsKeySystem(keySystem, mimeType);
    completionHandler(result);
}

void RemoteMediaPlayerManagerProxy::didReceivePlayerMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* player = m_proxies.get(makeObjectIdentifier<MediaPlayerPrivateRemoteIdentifierType>(decoder.destinationID())))
        player->didReceiveMessage(connection, decoder);
}

#if !RELEASE_LOG_DISABLED
const Logger& RemoteMediaPlayerManagerProxy::logger() const
{
    return m_gpuConnectionToWebProcess.logger();
}

WTFLogChannel& RemoteMediaPlayerManagerProxy::logChannel() const
{
    return WebKit2LogMedia;
}
#endif

} // namespace WebKit

#endif
