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

#if ENABLE(VIDEO) && PLATFORM(COCOA)

#include "CVUtilities.h"
#include "IOSurface.h"
#include "Logging.h"
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>

#if USE(LIBWEBRTC)

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_COMMA_BEGIN

#include <webrtc/api/video/video_frame.h>
#include <webrtc/sdk/WebKit/WebKitUtilities.h>

ALLOW_UNUSED_PARAMETERS_END
ALLOW_COMMA_END

#endif // USE(LIBWEBRTC)

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {

SharedVideoFrameInfo SharedVideoFrameInfo::fromCVPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    auto type = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (type == kCVPixelFormatType_32BGRA || type == kCVPixelFormatType_32ARGB)
        return { type, static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer)), static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRow(pixelBuffer)) };
    return { type, static_cast<uint32_t>(CVPixelBufferGetWidthOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetHeightOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetWidthOfPlane(pixelBuffer, 1)), static_cast<uint32_t>(CVPixelBufferGetHeightOfPlane(pixelBuffer, 1)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1)) };
}

bool SharedVideoFrameInfo::isReadWriteSupported() const
{
    return m_bufferType == kCVPixelFormatType_32BGRA
        || m_bufferType == kCVPixelFormatType_32ARGB
        || m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
        || m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
        || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange
        || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
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

static const uint8_t* copyToCVPixelBufferPlane(CVPixelBufferRef pixelBuffer, size_t planeIndex, const uint8_t* source, size_t height, uint32_t bytesPerRowSource)
{
    auto* destination = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, planeIndex));
    uint32_t bytesPerRowDestination = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, planeIndex);
    for (unsigned i = 0; i < height; ++i) {
        std::memcpy(destination, source, std::min(bytesPerRowSource, bytesPerRowDestination));
        source += bytesPerRowSource;
        destination += bytesPerRowDestination;
    }
    return source;
}

RetainPtr<CVPixelBufferRef> SharedVideoFrameInfo::createPixelBufferFromMemory(const uint8_t* data, CVPixelBufferPoolRef bufferPool)
{
    ASSERT(isReadWriteSupported());
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

    data = copyToCVPixelBufferPlane(rawPixelBuffer, 0, data, m_height, m_bytesPerRow);
    if (CVPixelBufferGetPlaneCount(rawPixelBuffer) == 2) {
        if (CVPixelBufferGetWidthOfPlane(rawPixelBuffer, 1) != m_widthPlaneB || CVPixelBufferGetHeightOfPlane(rawPixelBuffer, 1) != m_heightPlaneB)
            return nullptr;
        copyToCVPixelBufferPlane(rawPixelBuffer, 1, data, m_heightPlaneB, m_bytesPerRowPlaneB);
    }

    return pixelBuffer;
}

bool SharedVideoFrameInfo::writePixelBuffer(CVPixelBufferRef pixelBuffer, uint8_t* data)
{
    auto result = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(WebRTC, "SharedVideoFrameInfo::writePixelBuffer lock failed");
        return false;
    }

    auto scope = makeScopeExit([&pixelBuffer] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    });

    encode(data);
    data += sizeof(SharedVideoFrameInfo);

    auto* planeA = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
    if (!planeA) {
        RELEASE_LOG_ERROR(WebRTC, "SharedVideoFrameInfo::writePixelBuffer plane A is null");
        return false;
    }

    size_t planeASize = m_height * m_bytesPerRow;
    std::memcpy(data, planeA, planeASize);

    if (CVPixelBufferGetPlaneCount(pixelBuffer) == 2) {
        auto* planeB = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
        if (!planeB) {
            RELEASE_LOG_ERROR(WebRTC, "SharedVideoFrameInfo::writePixelBuffer plane B is null");
            return false;
        }

        size_t planeBSize = m_heightPlaneB * m_bytesPerRowPlaneB;
        std::memcpy(data + planeASize, planeB, planeBSize);
    }

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
SharedVideoFrameInfo SharedVideoFrameInfo::fromVideoFrameBuffer(const webrtc::VideoFrameBuffer& frame)
{
    if (frame.type() == webrtc::VideoFrameBuffer::Type::kNative)
        return SharedVideoFrameInfo { };

    auto type = frame.type();
    if (type == webrtc::VideoFrameBuffer::Type::kI420)
        return SharedVideoFrameInfo { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
            static_cast<uint32_t>(frame.width()), static_cast<uint32_t>(frame.height()), static_cast<uint32_t>(frame.width()),
            static_cast<uint32_t>(frame.width()) / 2, static_cast<uint32_t>(frame.height()) / 2, static_cast<uint32_t>(frame.width()) };

    if (type == webrtc::VideoFrameBuffer::Type::kI010)
        return SharedVideoFrameInfo { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange,
            static_cast<uint32_t>(frame.width()), static_cast<uint32_t>(frame.height()), static_cast<uint32_t>(frame.width() * 2),
            static_cast<uint32_t>(frame.width()) / 2, static_cast<uint32_t>(frame.height()) / 2, static_cast<uint32_t>(frame.width()) * 2 };

    return SharedVideoFrameInfo { };
}

bool SharedVideoFrameInfo::writeVideoFrameBuffer(webrtc::VideoFrameBuffer& frameBuffer, uint8_t* data)
{
    ASSERT(m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange);
    encode(data);
    data += sizeof(SharedVideoFrameInfo);
    return webrtc::copyVideoFrameBuffer(frameBuffer, data);
}
#endif

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
