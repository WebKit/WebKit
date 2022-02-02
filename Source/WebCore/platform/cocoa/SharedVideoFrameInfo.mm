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
#include "SharedVideoFrameInfo.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)

#include "CVUtilities.h"
#include "IOSurface.h"
#include "Logging.h"
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#if USE(LIBWEBRTC)

ALLOW_UNUSED_PARAMETERS_BEGIN
#include <webrtc/api/video/video_frame.h>
#include <webrtc/sdk/WebKit/WebKitUtilities.h>
ALLOW_UNUSED_PARAMETERS_END

#endif // USE(LIBWEBRTC)

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {

SharedVideoFrameInfo SharedVideoFrameInfo::fromCVPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    auto type = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (type == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || type == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
        return { type, static_cast<uint32_t>(CVPixelBufferGetWidthOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetHeightOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetWidthOfPlane(pixelBuffer, 1)), static_cast<uint32_t>(CVPixelBufferGetHeightOfPlane(pixelBuffer, 1)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1)) };
    return { type, static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer)), static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRow(pixelBuffer)) };
}

bool SharedVideoFrameInfo::isReadWriteSupported() const
{
    return m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
#if USE(ACCELERATE)
            || m_bufferType == kCVPixelFormatType_32BGRA
#endif
            || m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
            || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange;
}

size_t SharedVideoFrameInfo::storageSize() const
{
    return (m_bytesPerRow * m_height) + (m_bytesPerRowPlaneB * m_heightPlaneB) + sizeof(SharedVideoFrameInfo);
}

void SharedVideoFrameInfo::encode(uint8_t* destination)
{
    WTF::Persistence::Encoder encoder;
    encoder << (uint32_t)m_bufferType;
    encoder << m_width;
    encoder << m_height;
    encoder << m_bytesPerRow;
    encoder << m_widthPlaneB;
    encoder << m_heightPlaneB;
    encoder << m_bytesPerRowPlaneB;
    ASSERT(sizeof(SharedVideoFrameInfo) == encoder.bufferSize());
    std::memcpy(destination, encoder.buffer(), encoder.bufferSize());
}

std::optional<SharedVideoFrameInfo> SharedVideoFrameInfo::decode(Span<const uint8_t> span)
{
    WTF::Persistence::Decoder decoder(span);

    std::optional<uint32_t> bufferType;
    decoder >> bufferType;
    if (!bufferType)
        return std::nullopt;

    std::optional<uint32_t> width;
    decoder >> width;
    if (!width)
        return std::nullopt;

    std::optional<uint32_t> height;
    decoder >> height;
    if (!height)
        return std::nullopt;

    std::optional<uint32_t> bytesPerRow;
    decoder >> bytesPerRow;
    if (!bytesPerRow)
        return std::nullopt;

    std::optional<uint32_t> widthPlaneB;
    decoder >> widthPlaneB;
    if (!widthPlaneB)
        return std::nullopt;

    std::optional<uint32_t> heightPlaneB;
    decoder >> heightPlaneB;
    if (!heightPlaneB)
        return std::nullopt;

    std::optional<uint32_t> bytesPerRowPlaneB;
    decoder >> bytesPerRowPlaneB;
    if (!bytesPerRowPlaneB)
        return std::nullopt;

    return SharedVideoFrameInfo { *bufferType, *width, *height, *bytesPerRow , *widthPlaneB, *heightPlaneB, *bytesPerRowPlaneB };
}

RetainPtr<CVPixelBufferRef> SharedVideoFrameInfo::createPixelBufferFromMemory(const uint8_t* data, CVPixelBufferPoolRef bufferPool)
{
    ASSERT(isReadWriteSupported());
    if (m_bufferType == kCVPixelFormatType_32BGRA) {
#if USE(ACCELERATE)
        IntSize size { static_cast<int>(m_width), static_cast<int>(m_height) };
        auto ioSurface = IOSurface::create(size, DestinationColorSpace::SRGB(), IOSurface::Format::BGRA);

        IOSurface::Locker lock(*ioSurface);
        vImage_Buffer src;
        src.width = m_width;
        src.height = m_height;
        src.rowBytes = m_bytesPerRow;
        src.data = const_cast<uint8_t*>(data);

        vImage_Buffer dest;
        dest.width = m_width;
        dest.height = m_height;
        dest.rowBytes = ioSurface->bytesPerRow();
        dest.data = lock.surfaceBaseAddress();

        vImageUnpremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);

        auto pixelBuffer = WebCore::createCVPixelBuffer(ioSurface->surface());
        if (!pixelBuffer)
            return nullptr;
        return WTFMove(*pixelBuffer);
#else
        RELEASE_LOG_ERROR(Media, "createIOSurfaceFromSharedMemory cannot convert to IOSurface");
        return nullptr;
#endif
    }

    CVPixelBufferRef rawPixelBuffer = nullptr;
    if (bufferPool) {
        auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, bufferPool, &rawPixelBuffer);
        if (status != noErr || !rawPixelBuffer)
            return nullptr;

        ASSERT(CVPixelBufferGetWidthOfPlane(rawPixelBuffer, 0) == m_width);
        ASSERT(CVPixelBufferGetHeightOfPlane(rawPixelBuffer, 0) == m_height);
        ASSERT(CVPixelBufferGetPixelFormatType(rawPixelBuffer) == m_bufferType);
    } else {
        auto status = CVPixelBufferCreate(kCFAllocatorDefault, m_width, m_height, m_bufferType, nullptr, &rawPixelBuffer);
        if (status != noErr || !rawPixelBuffer)
            return nullptr;
    }

    auto pixelBuffer = adoptCF(rawPixelBuffer);
    auto status = CVPixelBufferLockBaseAddress(rawPixelBuffer, 0);
    if (status != noErr)
        return nullptr;

    auto scope = makeScopeExit([&rawPixelBuffer] {
        CVPixelBufferUnlockBaseAddress(rawPixelBuffer, 0);
    });

    if (CVPixelBufferGetWidthOfPlane(rawPixelBuffer, 1) != m_widthPlaneB || CVPixelBufferGetHeightOfPlane(rawPixelBuffer, 1) != m_heightPlaneB)
        return nullptr;

    auto* planeA = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(rawPixelBuffer, 0));
    uint32_t bytesPerRowPlaneA = CVPixelBufferGetBytesPerRowOfPlane(rawPixelBuffer, 0);
    for (unsigned i = 0; i < m_height; ++i) {
        std::memcpy(planeA, data, std::min(bytesPerRowPlaneA, m_bytesPerRow));
        planeA += bytesPerRowPlaneA;
        data += m_bytesPerRow;
    }

    auto* planeB = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(rawPixelBuffer, 1));
    uint32_t bytesPerRowPlaneB = CVPixelBufferGetBytesPerRowOfPlane(rawPixelBuffer, 1);
    for (unsigned i = 0; i < m_heightPlaneB; ++i) {
        std::memcpy(planeB, data, std::min(bytesPerRowPlaneB, m_bytesPerRowPlaneB));
        planeB += bytesPerRowPlaneB;
        data += m_bytesPerRowPlaneB;
    }

    CVPixelBufferUnlockBaseAddress(rawPixelBuffer, 0);
    return pixelBuffer;
}

bool SharedVideoFrameInfo::writePixelBuffer(CVPixelBufferRef pixelBuffer, uint8_t* data)
{
    auto result = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess)
        return false;

    auto scope = makeScopeExit([&pixelBuffer] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    });

    encode(data);
    data += sizeof(SharedVideoFrameInfo);

    if (m_bufferType == kCVPixelFormatType_32BGRA) {
#if USE(ACCELERATE)
        vImage_Buffer src;
        src.width = m_width;
        src.height = m_height;
        src.rowBytes = m_bytesPerRow;
        src.data = CVPixelBufferGetBaseAddress(pixelBuffer);

        vImage_Buffer dest;
        dest.width = m_width;
        dest.height = m_height;
        dest.rowBytes = m_bytesPerRow;
        dest.data = data;

        vImageUnpremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);

        return true;
#else
        return false;
#endif
    }

    const uint8_t *planeA = static_cast<const uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
    const uint8_t *planeB = static_cast<const uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));

    size_t planeASize = m_height * m_bytesPerRow;
    std::memcpy(data, planeA, planeASize);
    size_t planeBSize = m_heightPlaneB * m_bytesPerRowPlaneB;
    std::memcpy(data + planeASize, planeB, planeBSize);

    return true;
}

RetainPtr<CVPixelBufferPoolRef> SharedVideoFrameInfo::createCompatibleBufferPool() const
{
    auto result = createIOSurfaceCVPixelBufferPool(m_width, m_height, m_bufferType);
    if (!result)
        return { };
    return *result;
}

#if USE(LIBWEBRTC)
SharedVideoFrameInfo SharedVideoFrameInfo::fromVideoFrame(const webrtc::VideoFrame& frame)
{
    auto buffer = frame.video_frame_buffer();
    if (buffer->type() == webrtc::VideoFrameBuffer::Type::kNative)
        return SharedVideoFrameInfo { };

    auto type = buffer->type();

    if (type == webrtc::VideoFrameBuffer::Type::kI420)
        return SharedVideoFrameInfo { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
            static_cast<uint32_t>(frame.width()), static_cast<uint32_t>(frame.height()), static_cast<uint32_t>(frame.width()),
            static_cast<uint32_t>(frame.width()), static_cast<uint32_t>(frame.height()), static_cast<uint32_t>(frame.width()) / 2 };

    if (type == webrtc::VideoFrameBuffer::Type::kI010)
        return SharedVideoFrameInfo { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange,
            static_cast<uint32_t>(frame.width()), static_cast<uint32_t>(frame.height()), static_cast<uint32_t>(frame.width() * 2),
            static_cast<uint32_t>(frame.width()), static_cast<uint32_t>(frame.height()), static_cast<uint32_t>(frame.width()) };

    return SharedVideoFrameInfo { };
}

bool SharedVideoFrameInfo::writeVideoFrame(const webrtc::VideoFrame& frame, uint8_t* data)
{
    ASSERT(m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange);
    return webrtc::copyVideoFrame(frame, data);
}
#endif

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
