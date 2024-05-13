/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "VideoReceiverEndpointMessage.h"
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/VideoTargetFactory.h>

namespace WebKit {

#if ENABLE(LINEAR_MEDIA_PLAYER)
PlatformVideoTarget RemoteMediaPlayerManagerProxy::videoTargetForMediaElementIdentifier(WebCore::HTMLMediaElementIdentifier mediaElementIdentifier)
{
    auto cachedEntry = m_videoReceiverEndpointCache.find(mediaElementIdentifier);
    if (cachedEntry == m_videoReceiverEndpointCache.end())
        return nullptr;

    return cachedEntry->value.videoTarget;
}

void RemoteMediaPlayerManagerProxy::handleVideoReceiverEndpointMessage(const VideoReceiverEndpointMessage& endpointMessage)
{
    auto cachedEntry = m_videoReceiverEndpointCache.find(endpointMessage.mediaElementIdentifier());
    if (cachedEntry == m_videoReceiverEndpointCache.end()) {
        // If no entry for the specified mediaElementIdentifier exists, add a new entry to
        // the cache, and set the new t on the specified MediaPlayer.
        auto videoTarget = WebCore::VideoTargetFactory::createTargetFromEndpoint(endpointMessage.endpoint());
        m_videoReceiverEndpointCache.set(endpointMessage.mediaElementIdentifier(), VideoRecevierEndpointCacheEntry { endpointMessage.playerIdentifier(), endpointMessage.endpoint(), videoTarget });

        if (RefPtr mediaPlayer = this->mediaPlayer(endpointMessage.playerIdentifier()))
            mediaPlayer->setVideoTarget(videoTarget);

        return;
    }

    // A cached entry implies a MediaPlayer has already been given this endpoint.
    auto cachedPlayerIdentifier = cachedEntry->value.playerIdentifier;
    auto cachedEndpoint = cachedEntry->value.endpoint;

    // If nothing has actually changed, bail.
    if (cachedPlayerIdentifier == endpointMessage.playerIdentifier()
        && cachedEndpoint.get() == endpointMessage.endpoint().get())
        return;

    RefPtr cachedPlayer = mediaPlayer(cachedPlayerIdentifier);
    auto cachedTarget = cachedEntry->value.videoTarget;

    if (cachedEndpoint.get() != endpointMessage.endpoint().get()) {
        // If the endpoint changed, create a new VideoTarget from that new endpoint.
        if (endpointMessage.endpoint())
            cachedTarget = WebCore::VideoTargetFactory::createTargetFromEndpoint(endpointMessage.endpoint());
        else
            cachedTarget = nullptr;
    }

    if (cachedPlayerIdentifier != endpointMessage.playerIdentifier()) {
        // A endpoint can only be used by one MediaPlayer at a time, so if the playerIdentifier
        // has changed, first remove the endpoint from that cached MediaPlayer.
        if (cachedPlayer)
            cachedPlayer->setVideoTarget(nullptr);
    }

    // Then set the new target, which may have changed, on the specified MediaPlayer.
    if (RefPtr mediaPlayer = this->mediaPlayer(endpointMessage.playerIdentifier()))
        mediaPlayer->setVideoTarget(cachedTarget);

    // If the endpoint has been cleared, remove the entry from the cache entirely.
    if (!endpointMessage.endpoint()) {
        m_videoReceiverEndpointCache.remove(cachedEntry);
        return;
    }

    // Otherwise, update the cache entry with updated values.
    cachedEntry->value.playerIdentifier = endpointMessage.playerIdentifier();
    cachedEntry->value.endpoint = endpointMessage.endpoint();
    cachedEntry->value.videoTarget = cachedTarget;
}
#endif

}

#endif
