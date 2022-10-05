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
#include "GPUProcess.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "ScopedRenderingResourcesRequest.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <wtf/Logger.h>
#include <wtf/UniqueRef.h>

#if PLATFORM(COCOA)
#include <WebCore/AVAssetMIMETypeCache.h>
#endif

namespace WebKit {

using namespace WebCore;

RemoteMediaPlayerManagerProxy::RemoteMediaPlayerManagerProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteMediaPlayerManagerProxy::~RemoteMediaPlayerManagerProxy()
{
    auto proxies = std::exchange(m_proxies, { });

    for (auto& proxy : proxies.values())
        proxy->invalidate();
}

void RemoteMediaPlayerManagerProxy::createMediaPlayer(MediaPlayerIdentifier identifier, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& proxyConfiguration)
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_gpuConnectionToWebProcess);
    ASSERT(!m_proxies.contains(identifier));

    auto proxy = makeUnique<RemoteMediaPlayerProxy>(*this, identifier, m_gpuConnectionToWebProcess->connection(), engineIdentifier, WTFMove(proxyConfiguration), m_gpuConnectionToWebProcess->videoFrameObjectHeap(), m_gpuConnectionToWebProcess->webProcessIdentity());
    m_proxies.add(identifier, WTFMove(proxy));
}

void RemoteMediaPlayerManagerProxy::deleteMediaPlayer(MediaPlayerIdentifier identifier)
{
    ASSERT(RunLoop::isMain());

    if (auto proxy = m_proxies.take(identifier))
        proxy->invalidate();

    if (m_gpuConnectionToWebProcess && !hasOutstandingRenderingResourceUsage())
        m_gpuConnectionToWebProcess->gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
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

void RemoteMediaPlayerManagerProxy::supportsTypeAndCodecs(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const MediaEngineSupportParameters&& parameters, CompletionHandler<void(MediaPlayer::SupportsType)>&& completionHandler)
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

void RemoteMediaPlayerManagerProxy::originsInMediaCache(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const String&& path, CompletionHandler<void(HashSet<WebCore::SecurityOriginData>&&)>&& completionHandler)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        completionHandler({ });
        return;
    }

    completionHandler(engine->originsInMediaCache(path));
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

void RemoteMediaPlayerManagerProxy::clearMediaCacheForOrigins(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const String&& path, HashSet<WebCore::SecurityOriginData>&& origins)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        return;
    }
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
    ASSERT(RunLoop::isMain());
    if (auto* player = m_proxies.get(makeObjectIdentifier<MediaPlayerIdentifierType>(decoder.destinationID())))
        player->didReceiveMessage(connection, decoder);
}

bool RemoteMediaPlayerManagerProxy::didReceiveSyncPlayerMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    ASSERT(RunLoop::isMain());
    if (auto* player = m_proxies.get(makeObjectIdentifier<MediaPlayerIdentifierType>(decoder.destinationID())))
        return player->didReceiveSyncMessage(connection, decoder, encoder);
    return false;
}

RefPtr<MediaPlayer> RemoteMediaPlayerManagerProxy::mediaPlayer(const MediaPlayerIdentifier& identifier)
{
    ASSERT(RunLoop::isMain());
    auto results = m_proxies.find(identifier);
    if (results != m_proxies.end())
        return results->value->mediaPlayer();
    return nullptr;
}

#if !RELEASE_LOG_DISABLED
Logger& RemoteMediaPlayerManagerProxy::logger()
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        m_logger->setEnabled(this, m_gpuConnectionToWebProcess ? m_gpuConnectionToWebProcess->sessionID().isAlwaysOnLoggingAllowed() : false);
    }

    return *m_logger;
}
#endif

ShareableBitmapHandle RemoteMediaPlayerManagerProxy::bitmapImageForCurrentTime(WebCore::MediaPlayerIdentifier identifier)
{
    auto player = mediaPlayer(identifier);
    if (!player)
        return { };

    auto image = player->nativeImageForCurrentTime();
    if (!image)
        return { };

    auto imageSize = image->size();
    auto bitmap = ShareableBitmap::create(imageSize, { player->colorSpace() });
    if (!bitmap)
        return { };

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return { };

    context->drawNativeImage(*image, imageSize, FloatRect { { }, imageSize }, FloatRect { { }, imageSize });

    ShareableBitmapHandle bitmapHandle;
    bitmap->createHandle(bitmapHandle);
    return bitmapHandle;
}

} // namespace WebKit

#endif
