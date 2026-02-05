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

#include "GraphicsContext.h"
#include "MediaPlaybackTarget.h"
#include "ShareableBitmap.h"
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

RefPtr<ShareableBitmap> MediaPlayerPrivateInterface::bitmapFromImage(NativeImage& image)
{
    auto imageSize = image.size();
    RefPtr bitmap = ShareableBitmap::create({ imageSize, image.colorSpace() });
    if (!bitmap)
        return nullptr;

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    context->drawNativeImage(image, FloatRect { { }, imageSize }, FloatRect { { }, imageSize });

    return bitmap;
}

RefPtr<ShareableBitmap> MediaPlayerPrivateInterface::bitmapImageForCurrentTimeSync()
{
    if (RefPtr image = nativeImageForCurrentTime())
        return bitmapFromImage(*image);
    return nullptr;
}

Ref<MediaPlayer::BitmapImagePromise> MediaPlayerPrivateInterface::bitmapImageForCurrentTime()
{
    if (RefPtr shareableBitmap = bitmapImageForCurrentTimeSync())
        return BitmapImagePromise::createAndResolve(shareableBitmap.releaseNonNull());

    return BitmapImagePromise::createAndReject();
}

const PlatformTimeRanges& MediaPlayerPrivateInterface::seekable() const
{
    auto maxTimeSeekable = this->maxTimeSeekable();
    if (maxTimeSeekable == MediaTime::zeroTime())
        return PlatformTimeRanges::emptyRanges();
    ASSERT(maxTimeSeekable.isValid());
    m_seekable = { minTimeSeekable(), maxTimeSeekable };
    return m_seekable;
}

auto MediaPlayerPrivateInterface::asyncVideoPlaybackQualityMetrics() -> Ref<VideoPlaybackQualityMetricsPromise>
{
    if (auto metrics = videoPlaybackQualityMetrics())
        return VideoPlaybackQualityMetricsPromise::createAndResolve(WTF::move(*metrics));
    return VideoPlaybackQualityMetricsPromise::createAndReject(PlatformMediaError::NotSupportedError);
}

MediaTime MediaPlayerPrivateInterface::currentOrPendingSeekTime() const
{
    auto pendingSeekTime = this->pendingSeekTime();
    if (pendingSeekTime.isValid())
        return pendingSeekTime;
    return currentTime();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
OptionSet<MediaPlaybackTargetType> MediaPlayerPrivateInterface::supportedPlaybackTargetTypes() const
{
    return { };
}
#endif

} // namespace WebCore

#endif

