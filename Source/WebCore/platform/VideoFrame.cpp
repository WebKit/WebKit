/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "VideoFrame.h"

#if ENABLE(VIDEO)

#if USE(GSTREAMER)
#include "VideoFrameGStreamer.h"
#endif

namespace WebCore {

VideoFrame::VideoFrame(MediaTime presentationTime, bool isMirrored, Rotation rotation, PlatformVideoColorSpace&& colorSpace)
    : m_presentationTime(presentationTime)
    , m_isMirrored(isMirrored)
    , m_rotation(rotation)
    , m_colorSpace(WTFMove(colorSpace))
{
}

void VideoFrame::initializeCharacteristics(MediaTime presentationTime, bool isMirrored, Rotation rotation)
{
    const_cast<MediaTime&>(m_presentationTime) = presentationTime;
    const_cast<bool&>(m_isMirrored) = isMirrored;
    const_cast<Rotation&>(m_rotation) = rotation;
}

Ref<VideoFrame> VideoFrame::updateTimestamp(MediaTime mediaTime, ShouldCloneWithDifferentTimestamp shouldCloneWithDifferentTimestamp)
{
    if (m_presentationTime == mediaTime)
        return *this;

    Ref updatedVideoFrame = shouldCloneWithDifferentTimestamp == ShouldCloneWithDifferentTimestamp::Yes ? clone() : Ref { *this };
    const_cast<MediaTime&>(updatedVideoFrame->m_presentationTime) = mediaTime;
    return updatedVideoFrame;
}

#if !PLATFORM(COCOA) && !USE(GSTREAMER)
RefPtr<VideoFrame> VideoFrame::fromNativeImage(NativeImage&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> createFromPixelBuffer(Ref<PixelBuffer>&&, PlatformVideoColorSpace&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createNV12(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, const ComputedPlaneLayout&, PlatformVideoColorSpace&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createRGBA(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, PlatformVideoColorSpace&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createBGRA(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, PlatformVideoColorSpace&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createI420(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, const ComputedPlaneLayout&, const ComputedPlaneLayout&, PlatformVideoColorSpace&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createI420A(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, const ComputedPlaneLayout&, const ComputedPlaneLayout&, const ComputedPlaneLayout&, PlatformVideoColorSpace&&)
{
    // FIXME: Add support.
    return nullptr;
}

void VideoFrame::copyTo(std::span<uint8_t>, VideoPixelFormat, Vector<ComputedPlaneLayout>&&, CopyCallback&& callback)
{
    // FIXME: Add support.
    callback({ });
}

void VideoFrame::paintInContext(GraphicsContext&, const FloatRect&, const ImageOrientation&, bool)
{
    // FIXME: Add support.
}
#endif // !PLATFORM(COCOA)

}

#endif // ENABLE(VIDEO)
