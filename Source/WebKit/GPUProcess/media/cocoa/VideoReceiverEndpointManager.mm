/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include "VideoReceiverEndpointManager.h"

#if ENABLE(GPU_PROCESS) && ENABLE(LINEAR_MEDIA_PLAYER)

#include "Logging.h"
#include "RemoteAudioVideoRendererProxyManager.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "VideoReceiverEndpointMessage.h"
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/VideoTargetFactory.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoReceiverEndpointManager);

VideoReceiverEndpointManager::VideoReceiverEndpointManager(GPUConnectionToWebProcess& gpuConnection)
    : m_gpuConnection(gpuConnection)
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier { LoggerHelper::uniqueLogIdentifier() }
    , m_logger(gpuConnection.logger())
#endif
{
}

PlatformVideoTarget VideoReceiverEndpointManager::videoTargetForIdentifier(const std::optional<WebCore::VideoReceiverEndpointIdentifier>& identifier)
{
    if (identifier)
        return m_videoTargetCache.get(*identifier);
    return nullptr;
}

PlatformVideoTarget VideoReceiverEndpointManager::takeVideoTargetForMediaElementIdentifier(WebCore::HTMLMediaElementIdentifier mediaElementIdentifier, WebCore::MediaPlayerIdentifier playerIdentifier)
{
    auto cachedEntry = m_videoReceiverEndpointCache.find(mediaElementIdentifier);
    if (cachedEntry == m_videoReceiverEndpointCache.end())
        return nullptr;

    if (cachedEntry->value.playerIdentifier != playerIdentifier) {
        ALWAYS_LOG(LOGIDENTIFIER, "moving target from player ", cachedEntry->value.playerIdentifier->loggingString(), " to player ", playerIdentifier.loggingString());
        setVideoTargetIfValidIdentifier(cachedEntry->value.playerIdentifier, nullptr);
        cachedEntry->value.playerIdentifier = playerIdentifier;
    }

    return videoTargetForIdentifier(cachedEntry->value.endpointIdentifier);
}

void VideoReceiverEndpointManager::handleVideoReceiverEndpointMessage(const VideoReceiverEndpointMessage& endpointMessage)
{
    // A message with an empty endpoint signals that the VideoTarget should be uncached and
    // removed from the existing player.
    if (!endpointMessage.endpoint()) {
        m_videoTargetCache.remove(endpointMessage.endpointIdentifier());
        auto cacheEntry = m_videoReceiverEndpointCache.takeOptional(endpointMessage.mediaElementIdentifier());
        if (!cacheEntry)
            return;

        setVideoTargetIfValidIdentifier(cacheEntry->playerIdentifier, nullptr);
        return;
    }

    // Handle caching or uncaching of VideoTargets. Because a VideoTarget can only be created
    // once during the lifetime of an endpoint, we should avoid re-creating these VideoTargets.
    auto ensureVideoTargetResult = m_videoTargetCache.ensure(endpointMessage.endpointIdentifier(), [&] {
        return WebCore::VideoTargetFactory::createTargetFromEndpoint(endpointMessage.endpoint());
    });
    PlatformVideoTarget cachedVideoTarget = ensureVideoTargetResult.iterator->value;

    auto cacheResult = m_videoReceiverEndpointCache.add(endpointMessage.mediaElementIdentifier(), VideoReceiverEndpointCacheEntry { endpointMessage.playerIdentifier(), endpointMessage.endpointIdentifier() });
    if (cacheResult.isNewEntry) {
        // If no entry for the specified mediaElementIdentifier exists, set the new target
        // on the specified player.
        setVideoTargetIfValidIdentifier(endpointMessage.playerIdentifier(), cachedVideoTarget);
        return;
    }

    // A previously cached entry already exists
    auto& cachedEntry = cacheResult.iterator->value;
    auto cachedPlayerIdentifier = cachedEntry.playerIdentifier;
    auto cachedEndpointIdentifier = cachedEntry.endpointIdentifier;

    // If nothing has actually changed, bail.
    if (cachedPlayerIdentifier == endpointMessage.playerIdentifier()
        && cachedEndpointIdentifier == endpointMessage.endpointIdentifier())
        return;

    // If the VideoTarget has been cleared, remove the entry from the cache entirely.
    if (!cachedVideoTarget) {
        setVideoTargetIfValidIdentifier(cachedPlayerIdentifier, nullptr);
        m_videoReceiverEndpointCache.remove(cacheResult.iterator);
        return;
    }

    if (cachedPlayerIdentifier != endpointMessage.playerIdentifier()) {
        // A endpoint can only be used by one MediaPlayer at a time, so if the playerIdentifier
        // has changed, first remove the endpoint from that cached MediaPlayer.
        setVideoTargetIfValidIdentifier(cachedPlayerIdentifier, nullptr);
    }

    // Then set the new target, which may have changed, on the specified MediaPlayer.
    setVideoTargetIfValidIdentifier(endpointMessage.playerIdentifier(), cachedVideoTarget);

    // Otherwise, update the cache entry with updated values.
    cachedEntry.playerIdentifier = endpointMessage.playerIdentifier();
    cachedEntry.endpointIdentifier = endpointMessage.endpointIdentifier();
}

void VideoReceiverEndpointManager::handleVideoReceiverSwapEndpointsMessage(const VideoReceiverSwapEndpointsMessage& swapMessage)
{
    auto sourceCacheEntry = m_videoReceiverEndpointCache.takeOptional(swapMessage.sourceMediaElementIdentifier());
    auto sourceTarget = sourceCacheEntry ? videoTargetForIdentifier(sourceCacheEntry->endpointIdentifier) : nullptr;

    auto destinationCacheEntry = m_videoReceiverEndpointCache.takeOptional(swapMessage.destinationMediaElementIdentifier());
    auto destinationTarget = destinationCacheEntry ? videoTargetForIdentifier(destinationCacheEntry->endpointIdentifier) : nullptr;

    ALWAYS_LOG(LOGIDENTIFIER, "swapping from media element ", swapMessage.sourceMediaElementIdentifier().loggingString(), " to media element ", swapMessage.destinationMediaElementIdentifier().loggingString());

    // To avoid two media players using the VideoTarget simultaneously, set both players
    // to have null targets before continuing
    setVideoTargetIfValidIdentifier(swapMessage.sourceMediaPlayerIdentifier(), nullptr);
    setVideoTargetIfValidIdentifier(swapMessage.destinationMediaPlayerIdentifier(), nullptr);

    setVideoTargetIfValidIdentifier(swapMessage.sourceMediaPlayerIdentifier(), destinationTarget);
    setVideoTargetIfValidIdentifier(swapMessage.destinationMediaPlayerIdentifier(), sourceTarget);

    if (sourceCacheEntry) {
        sourceCacheEntry->playerIdentifier = swapMessage.destinationMediaPlayerIdentifier();
        m_videoReceiverEndpointCache.set(swapMessage.destinationMediaElementIdentifier(), *sourceCacheEntry);
    }

    if (destinationCacheEntry) {
        destinationCacheEntry->playerIdentifier = swapMessage.sourceMediaPlayerIdentifier();
        m_videoReceiverEndpointCache.set(swapMessage.sourceMediaElementIdentifier(), *destinationCacheEntry);
    }
}

void VideoReceiverEndpointManager::setVideoTargetIfValidIdentifier(std::optional<WebCore::MediaPlayerIdentifier> playerIdentifier, const PlatformVideoTarget& videoTarget) const
{
    if (!playerIdentifier)
        return;

    if (RefPtr player = protect(protectedConnection()->remoteMediaPlayerManagerProxy())->mediaPlayer(playerIdentifier)) {
        ALWAYS_LOG(LOGIDENTIFIER, "Update entry; ", !videoTarget ? "removing target" : "setting target", " on player ", playerIdentifier->loggingString());
        player->setVideoTarget(videoTarget);
        return;
    }

    if (RefPtr renderer = protect(protectedConnection()->remoteAudioVideoRendererProxyManager())->rendererFor(playerIdentifier)) {
        ALWAYS_LOG(LOGIDENTIFIER, "Update entry; ", !videoTarget ? "removing target" : "setting target", " on A/V renderer ", playerIdentifier->loggingString());
        renderer->setVideoTarget(videoTarget);
    }
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& VideoReceiverEndpointManager::logChannel() const
{
    return WebKit2LogMedia;
}
#endif

} // namespace WebKit

#endif
