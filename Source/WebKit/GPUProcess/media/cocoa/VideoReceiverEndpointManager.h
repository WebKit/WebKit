/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(LINEAR_MEDIA_PLAYER)

#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/VideoReceiverEndpoint.h>
#include <WebCore/VideoTarget.h>
#include <wtf/LoggerHelper.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class AudioVideoRenderer;
class MediaPlayer;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class VideoReceiverEndpointMessage;
class VideoReceiverSwapEndpointsMessage;

class VideoReceiverEndpointManager final
#if !RELEASE_LOG_DISABLED
    : private LoggerHelper
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(VideoReceiverEndpointManager);
public:
    VideoReceiverEndpointManager(GPUConnectionToWebProcess&);
    WebCore::PlatformVideoTarget videoTargetForIdentifier(const std::optional<WebCore::VideoReceiverEndpointIdentifier>&);
    WebCore::PlatformVideoTarget takeVideoTargetForMediaElementIdentifier(WebCore::HTMLMediaElementIdentifier, WebCore::MediaPlayerIdentifier);
    void handleVideoReceiverEndpointMessage(const VideoReceiverEndpointMessage&);
    void handleVideoReceiverSwapEndpointsMessage(const VideoReceiverSwapEndpointsMessage&);

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final { return "VideoReceiverEndpointManager"; }
    WTFLogChannel& logChannel() const final;
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    const Logger& logger() const final { return m_logger; }
#endif

private:
    HashMap<WebCore::VideoReceiverEndpointIdentifier, WebCore::PlatformVideoTarget> m_videoTargetCache;
    struct VideoReceiverEndpointCacheEntry {
        Markable<WebCore::MediaPlayerIdentifier> playerIdentifier;
        Markable<WebCore::VideoReceiverEndpointIdentifier> endpointIdentifier;
    };
    HashMap<WebCore::HTMLMediaElementIdentifier, VideoReceiverEndpointCacheEntry> m_videoReceiverEndpointCache;
    void setVideoTargetIfValidIdentifier(std::optional<WebCore::MediaPlayerIdentifier>, const WebCore::PlatformVideoTarget&) const;

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnection;
#if !RELEASE_LOG_DISABLED
    uint64_t m_logIdentifier { 0 };
    const Ref<Logger> m_logger;
#endif
};

} // namespace WebKit

#endif
