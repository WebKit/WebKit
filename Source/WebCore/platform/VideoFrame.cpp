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

#include <cmath>
#include <wtf/StdLibExtras.h>

#if USE(GSTREAMER)
#include "VideoFrameGStreamer.h"
#endif

#if !PLATFORM(COCOA) && !USE(GSTREAMER)
#include "ImageOrientation.h"
#endif

#if PLATFORM(COCOA)
#include "CVUtilities.h"
#include <CoreGraphics/CGAffineTransform.h>
#include <numbers>

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"
#endif

namespace WebCore {

VideoFrame::VideoFrame(MediaTime presentationTime, bool isMirrored, Rotation rotation, PlatformVideoColorSpace&& colorSpace)
    : m_presentationTime(presentationTime)
    , m_isMirrored(isMirrored)
    , m_rotation(rotation)
    , m_colorSpace(WTF::move(colorSpace))
{
}

void VideoFrame::initializeCharacteristics(MediaTime presentationTime, bool isMirrored, Rotation rotation)
{
    const_cast<MediaTime&>(m_presentationTime) = presentationTime;
    const_cast<bool&>(m_isMirrored) = isMirrored;
    const_cast<Rotation&>(m_rotation) = rotation;
}

Ref<VideoFrame> VideoFrame::updateTimestamp(MediaTime mediaTime, ShouldClone shouldCloneIfUpdating)
{
    if (m_presentationTime == mediaTime)
        return *this;

    Ref updatedVideoFrame = shouldCloneIfUpdating == ShouldClone::Yes ? clone() : Ref { *this };
    const_cast<MediaTime&>(updatedVideoFrame->m_presentationTime) = mediaTime;
    return updatedVideoFrame;
}

Ref<VideoFrame> VideoFrame::updateOrientation(Rotation rotation, bool isMirrored, ShouldClone shouldCloneIfUpdating)
{
    if (m_rotation == rotation && m_isMirrored == isMirrored)
        return *this;

    Ref updatedVideoFrame = shouldCloneIfUpdating == ShouldClone::Yes ? clone() : Ref { *this };
    const_cast<Rotation&>(updatedVideoFrame->m_rotation) = rotation;
    const_cast<bool&>(updatedVideoFrame->m_isMirrored) = isMirrored;
    return updatedVideoFrame;
}

ImageOrientation VideoFrame::orientation() const
{
    // The frame is rotated clockwise, then mirrored horizontally.
    switch (m_rotation) {
    case Rotation::None:
        return m_isMirrored ? ImageOrientation::Orientation::OriginTopRight : ImageOrientation::Orientation::OriginTopLeft;
    case Rotation::Right:
        return m_isMirrored ? ImageOrientation::Orientation::OriginLeftTop : ImageOrientation::Orientation::OriginRightTop;
    case Rotation::UpsideDown:
        return m_isMirrored ? ImageOrientation::Orientation::OriginBottomLeft : ImageOrientation::Orientation::OriginBottomRight;
    case Rotation::Left:
        return m_isMirrored ? ImageOrientation::Orientation::OriginRightBottom : ImageOrientation::Orientation::OriginLeftBottom;
    }
    ASSERT_NOT_REACHED();
    return ImageOrientation::Orientation::OriginTopLeft;
}

#if PLATFORM(COCOA)
CGAffineTransform VideoFrame::affineTransform(RotationDirection direction) const
{
    if (hasNoTransformation())
        return CGAffineTransformIdentity;

    // The frame is rotated first, then mirrored.
    double rotationRadians = static_cast<int>(m_rotation) * std::numbers::pi / 180;
    if (direction == RotationDirection::CounterClockwise)
        rotationRadians = -rotationRadians;
    if (m_isMirrored)
        return CGAffineTransformRotate(CGAffineTransformMakeScale(-1, 1), rotationRadians);
    return CGAffineTransformMakeRotation(rotationRadians);
}
#endif

Ref<VideoFrame> VideoFrame::updateShouldIgnoreAlpha(bool shouldIgnore, ShouldClone shouldCloneIfUpdating)
{
    if (shouldIgnoreAlpha() == shouldIgnore)
        return *this;

    Ref updatedVideoFrame = shouldCloneIfUpdating == ShouldClone::Yes ? clone() : Ref { *this };
    updatedVideoFrame->setShouldIgnoreAlpha(shouldIgnore);
    return updatedVideoFrame;
}

bool VideoFrame::copyToPlanarYUV(std::span<uint8_t> destination, VideoPixelFormat format)
{
#if PLATFORM(COCOA)
    // Only 8-bit 4:2:0 pixel buffers hold I420 samples unchanged.
    auto pixelFormat = this->pixelFormat();
    if (format == VideoPixelFormat::I420 && pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange && pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
        return false;
#endif
    auto layouts = planarBufferLayout(presentationSize(), format);
    if (layouts.isEmpty())
        return false;
    // VideoFrame::copyTo on Cocoa calls back before returning.
    bool succeeded = false;
    copyTo(destination, format, ColorSpace::SRGB(), WTF::move(layouts), [&](std::optional<Vector<PlaneLayout>>&& planeLayouts) {
        succeeded = planeLayouts && !planeLayouts->isEmpty();
    });
    return succeeded;
}

bool VideoFrame::copyLumaPlane(std::span<uint8_t> destination)
{
#if PLATFORM(COCOA)
    RetainPtr pixelBuffer = this->pixelBuffer();
    return pixelBuffer && copyAlphaFromLumaPlane(pixelBuffer.get(), destination, presentationSize().width());
#else
    UNUSED_PARAM(destination);
    return false;
#endif
}

Vector<ComputedPlaneLayout> VideoFrame::planarBufferLayout(IntSize size, VideoPixelFormat format)
{
    bool hasAlpha = pixelFormatHasAlpha(format);
    auto layout = planarYUVFormatLayout(pixelFormatWithoutAlpha(format));
    if (!layout || size.isEmpty())
        return { };

    size_t width = size.width();
    size_t height = size.height();
    size_t chromaWidth = PlanarImageConverter::chromaWidth(layout->subsampling, width);
    size_t chromaHeight = PlanarImageConverter::chromaHeight(layout->subsampling, height);
    size_t sampleSize = layout->format.sampleSize();

    Vector<ComputedPlaneLayout> layouts;
    size_t offset = 0;
    auto appendPlane = [&](size_t planeWidth, size_t planeHeight) {
        size_t rowBytes = planeWidth * sampleSize;
        layouts.append({ .destinationOffset = offset, .destinationStride = rowBytes, .sourceHeight = planeHeight, .sourceWidthBytes = rowBytes });
        offset += rowBytes * planeHeight;
    };
    appendPlane(width, height);
    appendPlane(chromaWidth, chromaHeight);
    appendPlane(chromaWidth, chromaHeight);
    if (hasAlpha)
        appendPlane(width, height);
    return layouts;
}

std::optional<size_t> VideoFrame::planarBufferSize(IntSize size, VideoPixelFormat format)
{
    auto layouts = planarBufferLayout(size, format);
    if (layouts.isEmpty())
        return { };
    auto& lastPlane = layouts.last();
    return lastPlane.destinationOffset + lastPlane.destinationStride * lastPlane.sourceHeight;
}

bool VideoFrame::copyToPlanarBuffer(std::span<uint8_t> destination, VideoPixelFormat format, VideoFrame* alphaFrame)
{
    auto size = presentationSize();
    ASSERT(!size.isEmpty());
    ASSERT(!alphaFrame || (pixelFormatHasAlpha(format) && alphaFrame->presentationSize() == size));
    auto byteSize = planarBufferSize(size, format);
    if (!byteSize || destination.size() != *byteSize)
        return false;

    if (!pixelFormatHasAlpha(format))
        return copyToPlanarYUV(destination, format);

    size_t alphaOffset = planarBufferLayout(size, format)[3].destinationOffset;
    if (!copyToPlanarYUV(destination.first(alphaOffset), pixelFormatWithoutAlpha(format)))
        return false;
    auto alphaPlane = destination.subspan(alphaOffset);
    if (!alphaFrame || !alphaFrame->copyLumaPlane(alphaPlane))
        memsetSpan(alphaPlane, 255);
    return true;
}

static int makeSureRotationIsBetween0And360(int rotation)
{
    return ((rotation % 360) + 360) % 360;
}

// https://w3c.github.io/webcodecs/#videoframe-parse-rotation
VideoFrameRotation parseVideoFrameRotation(double rotation)
{
    ASSERT(std::isfinite(rotation));

    int rotation360 = makeSureRotationIsBetween0And360(static_cast<int>(std::floor(std::fmod(rotation, 360))));
    return static_cast<VideoFrameRotation>(makeSureRotationIsBetween0And360(((rotation360 + 45) / 90) * 90));
}

// https://w3c.github.io/webcodecs/#videoframe-add-rotations
VideoFrame::Rotation addVideoFrameRotations(VideoFrame::Rotation baseRotation, bool baseFlip, VideoFrame::Rotation rotation)
{
    int combinedRotation = static_cast<int>(baseRotation) + (baseFlip ? - static_cast<int>(rotation) : static_cast<int>(rotation));
    return static_cast<VideoFrame::Rotation>(makeSureRotationIsBetween0And360(combinedRotation));
}

VideoFrameRotation subtractVideoFrameRotations(VideoFrameRotation baseRotation, VideoFrameRotation rotation)
{
    constexpr bool baseFlipToEmulateSubtract = true;
    return addVideoFrameRotations(baseRotation, baseFlipToEmulateSubtract, rotation);
}

VideoFrame::Rotation combineVideoFrameRotations(const VideoFrame& frame, double initRotation)
{
    return addVideoFrameRotations(frame.rotation(), frame.isMirrored(), parseVideoFrameRotation(initRotation));
}

#if PLATFORM(COCOA)
bool VideoFrame::is10bits() const
{
    auto pixelFormat = this->pixelFormat();
    return pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange || pixelFormat == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
}
#endif

#if !PLATFORM(COCOA) && !USE(GSTREAMER)
RefPtr<VideoFrame> VideoFrame::fromNativeImage(NativeImage&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createFromPixelBuffer(Ref<PixelBuffer>&&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createNV12(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, const ComputedPlaneLayout&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createRGBA(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createRGBX(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createBGRA(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createBGRX(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createI420(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, const ComputedPlaneLayout&, const ComputedPlaneLayout&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createI420A(std::span<const uint8_t>, size_t, size_t, const ComputedPlaneLayout&, const ComputedPlaneLayout&, const ComputedPlaneLayout&, const ComputedPlaneLayout&, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

RefPtr<VideoFrame> VideoFrame::createPlanarYUV(VideoPixelFormat, std::span<const uint8_t>, size_t, size_t, std::span<const ComputedPlaneLayout>, Init&&)
{
    // FIXME: Add support.
    return nullptr;
}

void VideoFrame::copyTo(std::span<uint8_t>, VideoPixelFormat, ColorSpace, Vector<ComputedPlaneLayout>&&, CopyCallback&& callback)
{
    // FIXME: Add support.
    callback({ });
}

RefPtr<NativeImage> VideoFrame::copyNativeImage() const
{
    // FIXME: Add support.
    return nullptr;
}

bool VideoFrame::is10bits() const
{
    // FIXME: Add support.
    return false;
}

#endif // !PLATFORM(COCOA) && !USE(GSTREAMER)

}

#endif // ENABLE(VIDEO)
