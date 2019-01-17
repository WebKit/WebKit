/*
 * Copyright (C) 2018 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RealtimeOutgoingVideoSourceCocoa.h"

#if USE(LIBWEBRTC)

#import "Logging.h"
#import "MediaSample.h"
#import "PixelBufferConformerCV.h"
#import "RealtimeVideoUtilities.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"

namespace WebCore {

RetainPtr<CVPixelBufferRef> RealtimeOutgoingVideoSourceCocoa::convertToYUV(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer)
        return nullptr;

    if (!m_pixelBufferConformer)
        m_pixelBufferConformer = std::make_unique<PixelBufferConformerCV>((__bridge CFDictionaryRef)@{ (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(preferedPixelBufferFormat()) });

    return m_pixelBufferConformer->convert(pixelBuffer);
}

static inline void computeRotatedWidthAndHeight(CVPixelBufferRef pixelBuffer, webrtc::VideoRotation rotation, size_t& width, size_t& height)
{
    switch (rotation) {
    case webrtc::kVideoRotation_0:
    case webrtc::kVideoRotation_180:
        width = CVPixelBufferGetWidth(pixelBuffer);
        height = CVPixelBufferGetHeight(pixelBuffer);
        return;
    case webrtc::kVideoRotation_90:
    case webrtc::kVideoRotation_270:
        width = CVPixelBufferGetHeight(pixelBuffer);
        height = CVPixelBufferGetWidth(pixelBuffer);
        return;
    }
}

RetainPtr<CVPixelBufferRef> RealtimeOutgoingVideoSourceCocoa::rotatePixelBuffer(CVPixelBufferRef pixelBuffer, webrtc::VideoRotation rotation)
{
    ASSERT(rotation);
    if (!rotation)
        return pixelBuffer;

    if (!m_rotationSession || rotation != m_currentRotation) {
        VTImageRotationSessionRef rawRotationSession = nullptr;
        auto status = VTImageRotationSessionCreate(kCFAllocatorDefault, rotation, &rawRotationSession);
        if (status != noErr) {
            ALWAYS_LOG(LOGIDENTIFIER, "Failed creating a rotation session with error ", status);
            return nullptr;
        }

        m_rotationSession = adoptCF(rawRotationSession);
        m_currentRotation = rotation;

        VTImageRotationSessionSetProperty(rawRotationSession, kVTImageRotationPropertyKey_EnableHighSpeedTransfer, kCFBooleanTrue);
    }

    size_t rotatedWidth, rotatedHeight;
    computeRotatedWidthAndHeight(pixelBuffer, rotation, rotatedWidth, rotatedHeight);
    auto format = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (!m_rotationPool || rotatedWidth != m_rotatedWidth || rotatedHeight != m_rotatedHeight || format != m_rotatedFormat) {
        auto pixelAttributes = @{
            (__bridge NSString *)kCVPixelBufferWidthKey: @(rotatedWidth),
            (__bridge NSString *)kCVPixelBufferHeightKey: @(rotatedHeight),
            (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(format),
            (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey: @NO,
        };

        CVPixelBufferPoolRef pool = nullptr;
        auto status = CVPixelBufferPoolCreate(kCFAllocatorDefault, nullptr, (__bridge CFDictionaryRef)pixelAttributes, &pool);

        if (status != kCVReturnSuccess) {
            ALWAYS_LOG(LOGIDENTIFIER, "Failed creating a pixel buffer pool with error ", status);
            return nullptr;
        }
        m_rotationPool = adoptCF(pool);

        m_rotatedWidth = rotatedWidth;
        m_rotatedHeight = rotatedHeight;
        m_rotatedFormat = format;
    }

    CVPixelBufferRef rawRotatedBuffer = nullptr;
    auto status = CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_rotationPool.get(), &rawRotatedBuffer);

    if (status != kCVReturnSuccess) {
        ALWAYS_LOG(LOGIDENTIFIER, "Failed creating a pixel buffer with error ", status);
        return nullptr;
    }
    RetainPtr<CVPixelBufferRef> rotatedBuffer = adoptCF(rawRotatedBuffer);

    status = VTImageRotationSessionTransferImage(m_rotationSession.get(), pixelBuffer, rotatedBuffer.get());

    if (status != noErr) {
        ALWAYS_LOG(LOGIDENTIFIER, "Failed rotating with error ", status);
        return nullptr;
    }
    return rotatedBuffer;
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
