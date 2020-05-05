/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteVideoSample.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "GraphicsContextCG.h"
#include "IOSurface.h"
#include "Logging.h"
#include "MediaSample.h"

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

static inline std::unique_ptr<IOSurface> transferBGRAPixelBufferToIOSurface(CVPixelBufferRef pixelBuffer)
{
#if USE(ACCELERATE)
    ASSERT(CVPixelBufferGetPixelFormatType(pixelBuffer) == kCVPixelFormatType_32BGRA);

    auto result = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    ASSERT(result == kCVReturnSuccess);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "transferBGRAPixelBufferToIOSurface CVPixelBufferLockBaseAddress() returned error code %d", result);
        return nullptr;
    }

    IntSize size { static_cast<int>(CVPixelBufferGetWidth(pixelBuffer)), static_cast<int>(CVPixelBufferGetHeight(pixelBuffer)) };
    auto ioSurface =  IOSurface::create(size, sRGBColorSpaceRef(), IOSurface::Format::RGBA);

    IOSurface::Locker lock(*ioSurface);
    vImage_Buffer src;
    src.width = size.width();
    src.height = size.height();
    src.rowBytes = CVPixelBufferGetBytesPerRow(pixelBuffer);
    src.data = CVPixelBufferGetBaseAddress(pixelBuffer);

    vImage_Buffer dest;
    dest.width = size.width();
    dest.height = size.height();
    dest.rowBytes = ioSurface->bytesPerRow();
    dest.data = lock.surfaceBaseAddress();

    vImageUnpremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);

    result = CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    ASSERT(result == kCVReturnSuccess);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "transferBGRAPixelBufferToIOSurface CVPixelBufferUnlockBaseAddress() returned error code %d", result);
        return nullptr;
    }
    return ioSurface;
#else
    RELEASE_LOG_ERROR(Media, "transferBGRAPixelBufferToIOSurface cannot convert to IOSurface");
    return nullptr;
#endif
}

std::unique_ptr<RemoteVideoSample> RemoteVideoSample::create(MediaSample& sample)
{
    ASSERT(sample.platformSample().type == PlatformSample::CMSampleBufferType);

    auto imageBuffer = CMSampleBufferGetImageBuffer(sample.platformSample().sample.cmSampleBuffer);
    if (!imageBuffer) {
        RELEASE_LOG_ERROR(Media, "RemoteVideoSample::create: CMSampleBufferGetImageBuffer returned nullptr");
        return nullptr;
    }

    std::unique_ptr<IOSurface> ioSurface;
    auto surface = CVPixelBufferGetIOSurface(imageBuffer);
    if (!surface) {
        // Special case for canvas data that is RGBA, not IOSurface backed.
        auto pixelFormatType = CVPixelBufferGetPixelFormatType(imageBuffer);
        if (pixelFormatType != kCVPixelFormatType_32BGRA) {
            RELEASE_LOG_ERROR(Media, "RemoteVideoSample::create does not support non IOSurface backed samples that are not BGRA");
            return nullptr;
        }

        ioSurface = transferBGRAPixelBufferToIOSurface(imageBuffer);
        if (!ioSurface)
            return nullptr;

        surface = ioSurface->surface();
    }

    return std::unique_ptr<RemoteVideoSample>(new RemoteVideoSample(surface, sRGBColorSpaceRef(), sample.presentationTime(), sample.videoRotation(), sample.videoMirrored()));
}

std::unique_ptr<RemoteVideoSample> RemoteVideoSample::create(CVPixelBufferRef imageBuffer, MediaTime&& presentationTime, MediaSample::VideoRotation rotation)
{
    auto surface = CVPixelBufferGetIOSurface(imageBuffer);
    if (!surface) {
        RELEASE_LOG_ERROR(Media, "RemoteVideoSample::create: CVPixelBufferGetIOSurface returned nullptr");
        return nullptr;
    }

    return std::unique_ptr<RemoteVideoSample>(new RemoteVideoSample(surface, sRGBColorSpaceRef(), WTFMove(presentationTime), rotation, false));
}

RemoteVideoSample::RemoteVideoSample(IOSurfaceRef surface, CGColorSpaceRef colorSpace, MediaTime&& time, MediaSample::VideoRotation rotation, bool mirrored)
    : m_ioSurface(WebCore::IOSurface::createFromSurface(surface, colorSpace))
    , m_rotation(rotation)
    , m_time(WTFMove(time))
    , m_videoFormat(IOSurfaceGetPixelFormat(surface))
    , m_size(IntSize(IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface)))
    , m_mirrored(mirrored)
{
}

IOSurfaceRef RemoteVideoSample::surface() const
{
    if (!m_ioSurface && m_sendRight)
        const_cast<RemoteVideoSample*>(this)->m_ioSurface = WebCore::IOSurface::createFromSendRight(WTFMove(const_cast<RemoteVideoSample*>(this)->m_sendRight), sRGBColorSpaceRef());

    return m_ioSurface ? m_ioSurface->surface() : nullptr;
}

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
