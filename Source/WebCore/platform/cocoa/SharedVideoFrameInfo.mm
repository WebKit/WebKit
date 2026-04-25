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
#include <wtf/StdLibExtras.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/ParsingUtilities.h>

#if USE(LIBWEBRTC)

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/api/video/video_frame.h>
#include <webrtc/webkit_sdk/WebKit/WebKitUtilities.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#endif // USE(LIBWEBRTC)

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

namespace WebCore {

SharedVideoFrameInfo SharedVideoFrameInfo::fromCVPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    auto type = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (type == kCVPixelFormatType_32BGRA || type == kCVPixelFormatType_32ARGB)
        return { type, static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer)), static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRow(pixelBuffer)) };

    return { type, static_cast<uint32_t>(CVPixelBufferGetWidthOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetHeightOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0)), static_cast<uint32_t>(CVPixelBufferGetWidthOfPlane(pixelBuffer, 1)), static_cast<uint32_t>(CVPixelBufferGetHeightOfPlane(pixelBuffer, 1)), static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1)),
        type == kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar ? static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 2)) : 0
    };
}

bool SharedVideoFrameInfo::isReadWriteSupported() const
{
    return m_bufferType == kCVPixelFormatType_32BGRA
        || m_bufferType == kCVPixelFormatType_32ARGB
        || m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
        || m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
        || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange
        || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange
        || m_bufferType == kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar;
}

size_t SharedVideoFrameInfo::storageSize() const
{
    if (m_storageSize)
        return m_storageSize;

    size_t sizePlaneA;
    if (!WTF::safeMultiply(m_bytesPerRow, m_height, sizePlaneA))
        return 0;

    size_t sizePlaneB;
    if (!WTF::safeMultiply(m_bytesPerRowPlaneB, m_heightPlaneB, sizePlaneB))
        return 0;

    size_t size;
    if (!WTF::safeAdd(sizePlaneA, sizePlaneB, size) || !WTF::safeAdd(size, sizeof(SharedVideoFrameInfo), size))
        return 0;

    if (m_bufferType == kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar) {
        size_t sizePlaneAlpha;
        if (!WTF::safeMultiply(m_bytesPerRowPlaneAlpha, m_height, sizePlaneAlpha) || !WTF::safeAdd(sizePlaneAlpha, size, size))
            return 0;
    }

    const_cast<SharedVideoFrameInfo*>(this)->m_storageSize = size;
    return m_storageSize;
}

void SharedVideoFrameInfo::encode(std::span<uint8_t> destination)
{
    WTF::Persistence::Encoder encoder;

    encoder << (uint32_t)m_bufferType;
    encoder << m_width;
    encoder << m_height;
    encoder << m_bytesPerRow;
    encoder << m_widthPlaneB;
    encoder << m_heightPlaneB;
    encoder << m_bytesPerRowPlaneB;
    encoder << m_bytesPerRowPlaneAlpha;
    ASSERT(sizeof(SharedVideoFrameInfo) == encoder.bufferSize() + sizeof(size_t));
    memcpySpan(destination, encoder.span());
}

std::optional<SharedVideoFrameInfo> SharedVideoFrameInfo::decode(std::span<const uint8_t> span)
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

    std::optional<uint32_t> bytesPerRowPlaneAlpha;
    decoder >> bytesPerRowPlaneAlpha;
    if (!bytesPerRowPlaneAlpha)
        return std::nullopt;

    SharedVideoFrameInfo info { *bufferType, *width, *height, *bytesPerRow , *widthPlaneB, *heightPlaneB, *bytesPerRowPlaneB, *bytesPerRowPlaneAlpha };
    if (!info.storageSize())
        return std::nullopt;

    return info;
}

static std::span<const uint8_t> copyToCVPixelBufferPlane(CVPixelBufferRef pixelBuffer, size_t planeIndex, std::span<const uint8_t> source, size_t height, uint32_t bytesPerRowSource)
{
    auto destination = CVPixelBufferGetSpanOfPlane(pixelBuffer, planeIndex);
    uint32_t bytesPerRowDestination = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, planeIndex);
    for (unsigned i = 0; i < height; ++i) {
        memcpySpan(destination, source.first(std::min(bytesPerRowSource, bytesPerRowDestination)));
        skip(source, bytesPerRowSource);
        skip(destination, bytesPerRowDestination);
    }
    return source;
}

RetainPtr<CVPixelBufferRef> SharedVideoFrameInfo::createPixelBufferFromMemory(std::span<const uint8_t> data, CVPixelBufferPoolRef bufferPool)
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

    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);
    auto status = CVPixelBufferLockBaseAddress(pixelBuffer.get(), 0);
    if (status != noErr)
        return nullptr;

    auto scope = makeScopeExit([pixelBuffer] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), 0);
    });

    data = copyToCVPixelBufferPlane(rawPixelBuffer, 0, data, m_height, m_bytesPerRow);
    if (CVPixelBufferGetPlaneCount(rawPixelBuffer) >= 2) {
        data = copyToCVPixelBufferPlane(rawPixelBuffer, 1, data, std::min<size_t>(m_heightPlaneB, CVPixelBufferGetHeightOfPlane(rawPixelBuffer, 1)), m_bytesPerRowPlaneB);
        if (CVPixelBufferGetPlaneCount(rawPixelBuffer) == 3)
            copyToCVPixelBufferPlane(rawPixelBuffer, 2, data, m_height, m_bytesPerRowPlaneAlpha);
    }

    return pixelBuffer;
}

bool SharedVideoFrameInfo::writePixelBuffer(CVPixelBufferRef pixelBuffer, std::span<uint8_t> data)
{
    auto result = CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(WebRTC, "SharedVideoFrameInfo::writePixelBuffer lock failed");
        return false;
    }

    auto scope = makeScopeExit([pixelBuffer = RetainPtr { pixelBuffer }] {
        CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    });

    encode(data);
    skip(data, sizeof(SharedVideoFrameInfo));

    auto planeA = CVPixelBufferGetSpanOfPlane(pixelBuffer, 0);
    if (!planeA.data()) {
        RELEASE_LOG_FAULT(WebRTC, "SharedVideoFrameInfo::writePixelBuffer plane A is null");
        return false;
    }

    memcpySpan(data, planeA);

    if (CVPixelBufferGetPlaneCount(pixelBuffer) >= 2) {
        auto planeB = CVPixelBufferGetSpanOfPlane(pixelBuffer, 1);
        if (!planeB.data()) {
            RELEASE_LOG_ERROR(WebRTC, "SharedVideoFrameInfo::writePixelBuffer plane B is null");
            return false;
        }

        memcpySpan(data.subspan(planeA.size()), planeB);

        if (CVPixelBufferGetPlaneCount(pixelBuffer) == 3) {
            auto planeAlpha = CVPixelBufferGetSpanOfPlane(pixelBuffer, 2);
            if (!planeAlpha.data()) {
                RELEASE_LOG_ERROR(WebRTC, "SharedVideoFrameInfo::writePixelBuffer plane A is null");
                return false;
            }

            memcpySpan(data.subspan(planeA.size() + planeB.size()), planeAlpha);
        }
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

bool SharedVideoFrameInfo::writeVideoFrameBuffer(webrtc::VideoFrameBuffer& frameBuffer, std::span<uint8_t> data)
{
    ASSERT(m_bufferType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || m_bufferType == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange);
    encode(data);
    return webrtc::copyVideoFrameBuffer(frameBuffer, data.subspan(sizeof(SharedVideoFrameInfo)).data());
}
#endif

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(COCOA)
