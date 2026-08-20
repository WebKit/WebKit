/*
 * Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ImageRotationSessionVT.h"

#import "AffineTransform.h"
#import "CVPixelBufferUtilities.h"
#import "CVUtilities.h"
#import "Logging.h"
#import "VideoFrameCV.h"

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageRotationSessionVT);

static ImageRotationSessionVT::RotationProperties transformToRotationProperties(const AffineTransform& inTransform)
{
    ImageRotationSessionVT::RotationProperties rotation;
    if (inTransform.isIdentity())
        return rotation;

    AffineTransform::DecomposedType decomposed { };
    if (!inTransform.decompose(decomposed))
        return rotation;

    rotation.flipY = WTF::areEssentiallyEqual(decomposed.scaleX, -1.);
    rotation.flipX = WTF::areEssentiallyEqual(decomposed.scaleY, -1.);
    auto degrees = rad2deg(decomposed.angle);
    while (degrees < 0)
        degrees += 360;

    // Only support rotation in multiples of 90º:
    if (WTF::areEssentiallyEqual(fmod(degrees, 90.), 0.))
        rotation.angle = clampToUnsigned(degrees);

    return rotation;
}

ImageRotationSessionVT::ImageRotationSessionVT(AffineTransform&& transform, FloatSize size, IsCGImageCompatible isCGImageCompatible, ShouldUseIOSurface shouldUseIOSurface)
    : ImageRotationSessionVT(transformToRotationProperties(transform), size, isCGImageCompatible, shouldUseIOSurface)
{
    m_transform = WTF::move(transform);
}

ImageRotationSessionVT::ImageRotationSessionVT(const RotationProperties& rotation, FloatSize size, IsCGImageCompatible isCGImageCompatible, ShouldUseIOSurface shouldUseIOSurface)
    : m_shouldUseIOSurface(shouldUseIOSurface == ShouldUseIOSurface::Yes)
{
    initialize(rotation, size, isCGImageCompatible);
}

ImageRotationSessionVT::ImageRotationSessionVT(ShouldUseIOSurface shouldUseIOSurface)
    : m_shouldUseIOSurface(shouldUseIOSurface == ShouldUseIOSurface::Yes)
{
}

void ImageRotationSessionVT::initialize(const RotationProperties& rotation, FloatSize size, IsCGImageCompatible isCGImageCompatible)
{
    m_rotationProperties = rotation;
    m_size = size;
    m_isCGImageCompatible = isCGImageCompatible;

    if (m_rotationProperties.angle == 90 || m_rotationProperties.angle == 270)
        size = size.transposedSize();

    m_rotatedSize = expandedIntSize(size);
    m_rotationPool = nullptr;

    VTImageRotationSessionRef rawRotationSession = nullptr;
    VTImageRotationSessionCreate(kCFAllocatorDefault, m_rotationProperties.angle, &rawRotationSession);
    m_rotationSession = adoptCF(rawRotationSession);
    VTImageRotationSessionSetProperty(m_rotationSession.get(), kVTImageRotationPropertyKey_EnableHighSpeedTransfer, kCFBooleanTrue);

    if (m_rotationProperties.flipY)
        VTImageRotationSessionSetProperty(m_rotationSession.get(), kVTImageRotationPropertyKey_FlipVerticalOrientation, kCFBooleanTrue);
    if (m_rotationProperties.flipX)
        VTImageRotationSessionSetProperty(m_rotationSession.get(), kVTImageRotationPropertyKey_FlipHorizontalOrientation, kCFBooleanTrue);
}

RetainPtr<CVPixelBufferRef> ImageRotationSessionVT::rotate(CVPixelBufferRef pixelBuffer)
{
    auto pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (pixelFormat != m_pixelFormat || !m_rotationPool) {
        m_pixelFormat = pixelFormat;
        auto bufferPool = createCVPixelBufferPool(m_rotatedSize.width(), m_rotatedSize.height(), m_pixelFormat, 0u, m_isCGImageCompatible == IsCGImageCompatible::Yes, m_shouldUseIOSurface);

        if (!bufferPool) {
            RELEASE_LOG_ERROR(WebRTC, "ImageRotationSessionVT failed creating buffer pool with error %d", (int)bufferPool.error());
            return nullptr;
        }

        m_rotationPool = WTF::move(*bufferPool);
    }

    RetainPtr<CVPixelBufferRef> result;
    CVPixelBufferRef rawRotatedBuffer = nullptr;
    auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_rotationPool.get(), &rawRotatedBuffer);
    if (status != kCVReturnSuccess || !rawRotatedBuffer) {
        RELEASE_LOG_ERROR(WebRTC, "ImageRotationSessionVT failed creating buffer from pool with error %d", status);
        return nullptr;
    }
    result = adoptCF(rawRotatedBuffer);

    status = VTImageRotationSessionTransferImage(m_rotationSession.get(), pixelBuffer, rawRotatedBuffer);
    if (status != noErr) {
        RELEASE_LOG_ERROR(WebRTC, "ImageRotationSessionVT failed rotating buffer with error %d", status);
        return nullptr;
    }

    return result;
}

RetainPtr<CVPixelBufferRef> ImageRotationSessionVT::rotate(CVPixelBufferRef pixelBuffer, const RotationProperties& rotation, IsCGImageCompatible cgImageCompatible)
{
    m_pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

    IntSize size { static_cast<int>(CVPixelBufferGetWidth(pixelBuffer)), static_cast<int>(CVPixelBufferGetHeight(pixelBuffer)) };
    if (rotation != m_rotationProperties || m_size != size)
        initialize(rotation, size, cgImageCompatible);

    return rotate(pixelBuffer);
}

RetainPtr<CVPixelBufferRef> ImageRotationSessionVT::rotate(VideoFrame& videoFrame, const RotationProperties& rotation, IsCGImageCompatible cgImageCompatible)
{
    if (RetainPtr pixelBuffer = videoFrame.pixelBuffer())
        return rotate(pixelBuffer, rotation, cgImageCompatible);
    ASSERT_NOT_REACHED();
    return nullptr;
}

RetainPtr<CVPixelBufferRef> ImageRotationSessionVT::rotate(NativeImage& image, const RotationProperties& rotation, IsCGImageCompatible cgImageCompatible)
{
    if (RetainPtr pixelBuffer = createCVPixelBufferFromImage(image.platformImage()))
        return rotate(pixelBuffer, rotation, cgImageCompatible);
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<VideoFrame> ImageRotationSessionVT::applyRotation(VideoFrame& videoFrame, IsCGImageCompatible cgImageCompatible)
{
    RotationProperties rotation {
        .flipY = videoFrame.isMirrored(),
        .angle = static_cast<uint16_t>(videoFrame.rotation())
    };

    auto pixelBuffer = rotate(videoFrame, rotation, cgImageCompatible);
    if (!pixelBuffer)
        return nullptr;

    return VideoFrameCV::create(videoFrame.presentationTime(), false, VideoFrameRotation::None, WTF::move(pixelBuffer), PlatformVideoColorSpace { videoFrame.colorSpace() });
}

RefPtr<NativeImage> ImageRotationSessionVT::applyRotation(NativeImage& nativeImage, ImageOrientation orientation, IsCGImageCompatible cgImageCompatible)
{
    if (!canLoad_VideoToolbox_VTCreateCGImageFromCVPixelBuffer())
        return nullptr;

    RotationProperties rotation;

    switch (orientation.orientation()) {
    case ImageOrientation::Orientation::FromImage:
        ASSERT_NOT_REACHED();
        return nullptr;
    case ImageOrientation::Orientation::OriginTopLeft:
        break;
    case ImageOrientation::Orientation::OriginTopRight:
        rotation.flipX = true;
        break;
    case ImageOrientation::Orientation::OriginBottomRight:
        rotation.angle = 180;
        break;
    case ImageOrientation::Orientation::OriginBottomLeft:
        rotation.flipY = true;
        break;
    case ImageOrientation::Orientation::OriginLeftTop:
        rotation.angle = 90;
        rotation.flipX = true;
        break;
    case ImageOrientation::Orientation::OriginRightTop:
        rotation.angle = 90;
        break;
    case ImageOrientation::Orientation::OriginRightBottom:
        rotation.angle = 270;
        rotation.flipX = true;
        break;
    case ImageOrientation::Orientation::OriginLeftBottom:
        rotation.angle = 270;
        break;
    }

    RetainPtr pixelBuffer = rotate(nativeImage, rotation, cgImageCompatible);
    if (!pixelBuffer)
        return nullptr;

    CGImageRef cgImage;
    VTCreateCGImageFromCVPixelBuffer(pixelBuffer, nullptr, &cgImage);
    return NativeImage::create(adoptCF(cgImage));
}

} // namespace WebCore
