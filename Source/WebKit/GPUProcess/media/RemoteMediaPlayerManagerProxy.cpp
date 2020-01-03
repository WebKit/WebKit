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
#include "RemoteMediaPlayerManagerMessages.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <wtf/UniqueRef.h>

#define MESSAGE_CHECK_CONTEXTID(identifier) MESSAGE_CHECK_BASE(m_proxies.isValidKey(identifier), &m_gpuConnectionToWebProcess.connection())

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
    MESSAGE_CHECK_CONTEXTID(id);

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

void RemoteMediaPlayerManagerProxy::supportsType(MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, const WebCore::MediaEngineSupportParameters&& parameters, CompletionHandler<void(MediaPlayer::SupportsType)>&& completionHandler)
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

void RemoteMediaPlayerManagerProxy::prepareForPlayback(MediaPlayerPrivateRemoteIdentifier id, bool privateMode, WebCore::MediaPlayerEnums::Preload preload, bool preservesPitch, bool prepareForRendering)
{
    if (auto player = m_proxies.get(id))
        player->prepareForPlayback(privateMode, preload, preservesPitch, prepareForRendering);
}

void RemoteMediaPlayerManagerProxy::load(MediaPlayerPrivateRemoteIdentifier id, URL&& url, WebCore::ContentType&& contentType, String&& keySystem, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&& completionHandler)
{
    if (auto player = m_proxies.get(id))
        player->load(WTFMove(url), WTFMove(contentType), WTFMove(keySystem), WTFMove(completionHandler));
}

void RemoteMediaPlayerManagerProxy::cancelLoad(MediaPlayerPrivateRemoteIdentifier id)
{
    if (auto player = m_proxies.get(id))
        player->cancelLoad();
}

void RemoteMediaPlayerManagerProxy::prepareToPlay(MediaPlayerPrivateRemoteIdentifier id)
{
    if (auto player = m_proxies.get(id))
        player->prepareToPlay();
}

void RemoteMediaPlayerManagerProxy::play(MediaPlayerPrivateRemoteIdentifier id)
{
    if (auto player = m_proxies.get(id))
        player->play();
}

void RemoteMediaPlayerManagerProxy::pause(MediaPlayerPrivateRemoteIdentifier id)
{
    if (auto player = m_proxies.get(id))
        player->pause();
}

void RemoteMediaPlayerManagerProxy::seek(MediaPlayerPrivateRemoteIdentifier id, MediaTime&& time)
{
    if (auto player = m_proxies.get(id))
        player->seek(WTFMove(time));
}

void RemoteMediaPlayerManagerProxy::seekWithTolerance(MediaPlayerPrivateRemoteIdentifier id, MediaTime&& time, MediaTime&& negativeTolerance, MediaTime&& positiveTolerance)
{
    if (auto player = m_proxies.get(id))
        player->seekWithTolerance(WTFMove(time), WTFMove(negativeTolerance), WTFMove(positiveTolerance));
}

void RemoteMediaPlayerManagerProxy::setVolume(MediaPlayerPrivateRemoteIdentifier id, double volume)
{
    if (auto player = m_proxies.get(id))
        player->setVolume(volume);
}

void RemoteMediaPlayerManagerProxy::setMuted(MediaPlayerPrivateRemoteIdentifier id, bool muted)
{
    if (auto player = m_proxies.get(id))
        player->setMuted(muted);
}

void RemoteMediaPlayerManagerProxy::setPreload(MediaPlayerPrivateRemoteIdentifier id, WebCore::MediaPlayerEnums::Preload preload)
{
    if (auto player = m_proxies.get(id))
        player->setPreload(preload);
}

void RemoteMediaPlayerManagerProxy::setPrivateBrowsingMode(MediaPlayerPrivateRemoteIdentifier id, bool privateMode)
{
    if (auto player = m_proxies.get(id))
        player->setPrivateBrowsingMode(privateMode);
}

void RemoteMediaPlayerManagerProxy::setPreservesPitch(MediaPlayerPrivateRemoteIdentifier id, bool preservesPitch)
{
    if (auto player = m_proxies.get(id))
        player->setPreservesPitch(preservesPitch);
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

#undef MESSAGE_CHECK_CONTEXTID

#endif
