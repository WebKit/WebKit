/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "MediaPlayerPrivate.h"

#if ENABLE(VIDEO)

#include "VideoFrame.h"
#include "VideoFrameMetadata.h"
#include <wtf/NativePromise.h>

namespace WebCore {

MediaPlayerPrivateInterface::MediaPlayerPrivateInterface() = default;
MediaPlayerPrivateInterface::~MediaPlayerPrivateInterface() = default;

RefPtr<VideoFrame> MediaPlayerPrivateInterface::videoFrameForCurrentTime()
{
    return nullptr;
}

std::optional<VideoFrameMetadata> MediaPlayerPrivateInterface::videoFrameMetadata()
{
    return { };
}

const PlatformTimeRanges& MediaPlayerPrivateInterface::seekable() const
{
    if (maxMediaTimeSeekable() == MediaTime::zeroTime())
        return PlatformTimeRanges::emptyRanges();
    m_seekable = { minMediaTimeSeekable(), maxMediaTimeSeekable() };
    return m_seekable;
}

auto MediaPlayerPrivateInterface::asyncVideoPlaybackQualityMetrics() -> Ref<VideoPlaybackQualityMetricsPromise>
{
    if (auto metrics = videoPlaybackQualityMetrics())
        return VideoPlaybackQualityMetricsPromise::createAndResolve(WTFMove(*metrics));
    return VideoPlaybackQualityMetricsPromise::createAndReject(PlatformMediaError::NotSupportedError);
}

}

#endif

