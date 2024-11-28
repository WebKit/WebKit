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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "RemoteMediaPlayerConfiguration.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "RemoteVideoFrameObjectHeap.h"
#include "ScopedRenderingResourcesRequest.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <wtf/Logger.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>

#if PLATFORM(COCOA)
#include <WebCore/AVAssetMIMETypeCache.h>
#endif

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaPlayerManagerProxy);

RemoteMediaPlayerManagerProxy::RemoteMediaPlayerManagerProxy(GPUConnectionToWebProcess& connection)
    : m_gpuConnectionToWebProcess(connection)
{
}

RemoteMediaPlayerManagerProxy::~RemoteMediaPlayerManagerProxy()
{
    clear();
}

void RemoteMediaPlayerManagerProxy::clear()
{
    auto proxies = std::exchange(m_proxies, { });

    for (auto& proxy : proxies.values())
        proxy->invalidate();

#if ENABLE(MEDIA_SOURCE)
    m_pendingMediaSources.clear();
#endif
}

void RemoteMediaPlayerManagerProxy::createMediaPlayer(MediaPlayerIdentifier identifier, MediaPlayerClientIdentifier clientIdentifier, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& proxyConfiguration)
{
    auto connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;
    ASSERT(RunLoop::isMain());
    ASSERT(!m_proxies.contains(identifier));

    auto proxy = RemoteMediaPlayerProxy::create(*this, identifier, clientIdentifier, connection->protectedConnection(), engineIdentifier, WTFMove(proxyConfiguration), Ref { connection->videoFrameObjectHeap() }, connection->webProcessIdentity());
    m_proxies.add(identifier, WTFMove(proxy));
}

void RemoteMediaPlayerManagerProxy::deleteMediaPlayer(MediaPlayerIdentifier identifier)
{
    ASSERT(RunLoop::isMain());

    if (auto proxy = m_proxies.take(identifier))
        proxy->invalidate();

    auto connection = m_gpuConnectionToWebProcess.get();
    if (!connection)
        return;

    if (!hasOutstandingRenderingResourceUsage())
        connection->protectedGPUProcess()->tryExitIfUnusedAndUnderMemoryPressure();
}

void RemoteMediaPlayerManagerProxy::getSupportedTypes(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    auto engine = MediaPlayer::mediaEngine(engineIdentifier);
    if (!engine) {
        WTFLogAlways("Failed to find media engine.");
        completionHandler({ });
        return;
    }

    HashSet<String> engineTypes;
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
    if (ObjectIdentifier<MediaPlayerIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr player = m_proxies.get(ObjectIdentifier<MediaPlayerIdentifierType>(decoder.destinationID())))
            player->didReceiveMessage(connection, decoder);
    }
}

bool RemoteMediaPlayerManagerProxy::didReceiveSyncPlayerMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder)
{
    ASSERT(RunLoop::isMain());
    if (ObjectIdentifier<MediaPlayerIdentifierType>::isValidIdentifier(decoder.destinationID())) {
        if (RefPtr player = m_proxies.get(ObjectIdentifier<MediaPlayerIdentifierType>(decoder.destinationID())))
            return player->didReceiveSyncMessage(connection, decoder, encoder);
    }
    return false;
}

RefPtr<MediaPlayer> RemoteMediaPlayerManagerProxy::mediaPlayer(std::optional<MediaPlayerIdentifier> identifier)
{
    ASSERT(RunLoop::isMain());
    if (!identifier)
        return nullptr;
    auto results = m_proxies.find(*identifier);
    if (results != m_proxies.end())
        return results->value->mediaPlayer();
    return nullptr;
}

#if !RELEASE_LOG_DISABLED
Logger& RemoteMediaPlayerManagerProxy::logger()
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        RefPtr connection { m_gpuConnectionToWebProcess.get() };
        m_logger->setEnabled(this, connection && connection->isAlwaysOnLoggingAllowed());
    }

    return *m_logger;
}
#endif

std::optional<ShareableBitmap::Handle> RemoteMediaPlayerManagerProxy::bitmapImageForCurrentTime(WebCore::MediaPlayerIdentifier identifier)
{
    auto player = mediaPlayer(identifier);
    if (!player)
        return { };

    auto image = player->nativeImageForCurrentTime();
    if (!image)
        return { };

    auto imageSize = image->size();
    auto bitmap = ShareableBitmap::create({ imageSize, player->colorSpace() });
    if (!bitmap)
        return { };

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return { };

    context->drawNativeImage(*image, FloatRect { { }, imageSize }, FloatRect { { }, imageSize });

    return bitmap->createHandle();
}

#if ENABLE(MEDIA_SOURCE)
void RemoteMediaPlayerManagerProxy::registerMediaSource(RemoteMediaSourceIdentifier identifier, RemoteMediaSourceProxy& mediaSource)
{
    ASSERT(RunLoop::isMain());

    ASSERT(!m_pendingMediaSources.contains(identifier));
    m_pendingMediaSources.add(identifier, &mediaSource);
}

void RemoteMediaPlayerManagerProxy::invalidateMediaSource(RemoteMediaSourceIdentifier identifier)
{
    ASSERT(RunLoop::isMain());

    ASSERT(m_pendingMediaSources.contains(identifier));
    m_pendingMediaSources.remove(identifier);
}

RefPtr<RemoteMediaSourceProxy> RemoteMediaPlayerManagerProxy::pendingMediaSource(RemoteMediaSourceIdentifier identifier)
{
    ASSERT(RunLoop::isMain());

    auto iterator = m_pendingMediaSources.find(identifier);
    if (iterator == m_pendingMediaSources.end())
        return nullptr;
    return iterator->value;
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
