/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "PixelBufferConformerCV.h"

#include "CVUtilities.h"
#include "DestinationColorSpace.h"
#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
#include "Logging.h"

#include "CoreVideoSoftLink.h"
#include "VideoToolboxSoftLink.h"

namespace WebCore {

#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
static void configureSessionForDisplayP3(VTPixelTransferSessionRef session)
{
    VTSessionSetProperty(session, kVTPixelTransferPropertyKey_DestinationColorPrimaries, kCVImageBufferColorPrimaries_DCI_P3);
    VTSessionSetProperty(session, kVTPixelTransferPropertyKey_DestinationYCbCrMatrix, kCVImageBufferYCbCrMatrix_ITU_R_709_2);
    VTSessionSetProperty(session, kVTPixelTransferPropertyKey_DestinationTransferFunction, kCVImageBufferTransferFunction_sRGB);
}

static bool isPixelBufferConfiguredForDisplayP3(CVPixelBufferRef buffer)
{
    return CFEqual(CVBufferGetAttachment(buffer, kCVImageBufferColorPrimariesKey, nullptr), kCVImageBufferColorPrimaries_DCI_P3)
        && CFEqual(CVBufferGetAttachment(buffer, kCVImageBufferYCbCrMatrixKey, nullptr), kCVImageBufferYCbCrMatrix_ITU_R_709_2)
        && CFEqual(CVBufferGetAttachment(buffer, kCVImageBufferTransferFunctionKey, nullptr), kCVImageBufferTransferFunction_sRGB);
}
#endif

static void configureSessionForSRGB(VTPixelTransferSessionRef session)
{
    VTSessionSetProperty(session, kVTPixelTransferPropertyKey_DestinationColorPrimaries, kCVImageBufferColorPrimaries_ITU_R_709_2);
    VTSessionSetProperty(session, kVTPixelTransferPropertyKey_DestinationYCbCrMatrix, kCVImageBufferYCbCrMatrix_ITU_R_709_2);
    VTSessionSetProperty(session, kVTPixelTransferPropertyKey_DestinationTransferFunction, kCVImageBufferTransferFunction_sRGB);
}

static bool isPixelBufferConfiguredForSRGB(CVPixelBufferRef buffer)
{
    return CFEqual(CVBufferGetAttachment(buffer, kCVImageBufferColorPrimariesKey, nullptr), kCVImageBufferColorPrimaries_ITU_R_709_2)
        && CFEqual(CVBufferGetAttachment(buffer, kCVImageBufferYCbCrMatrixKey, nullptr), kCVImageBufferYCbCrMatrix_ITU_R_709_2)
        && CFEqual(CVBufferGetAttachment(buffer, kCVImageBufferTransferFunctionKey, nullptr), kCVImageBufferTransferFunction_sRGB);
}

static void configureSessionForColorSpace(VTPixelTransferSessionRef session, CGColorSpaceRef colorSpace)
{
    if (auto iccData = RetainPtr { CGColorSpaceCopyICCData(colorSpace) }) {
        VTSessionSetProperty(session, kVTPixelTransferPropertyKey_DestinationICCProfile, iccData.get());
        return;
    }

    // VTPixelTransferSession requires either color properties or ICC data.
    ASSERT_NOT_REACHED();
}

static bool isPixelBufferConfiguredForColorSpace(CVPixelBufferRef buffer, CGColorSpaceRef colorSpace)
{
    return CFEqual(CVBufferGetAttachment(buffer, kCVImageBufferCGColorSpaceKey, nullptr), colorSpace);
}

PixelBufferConformerCV::PixelBufferConformerCV(FourCC&& pixelFormat, const DestinationColorSpace& colorSpace)
    : m_pixelFormat(WTFMove(pixelFormat))
    , m_destinationColorSpace(colorSpace)
{
    VTPixelTransferSessionRef transferSession = nullptr;
    VTPixelTransferSessionCreate(kCFAllocatorDefault, &transferSession);
    ASSERT(transferSession);

    if (colorSpace == DestinationColorSpace::SRGB())
        configureSessionForSRGB(transferSession);
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    else if (colorSpace == DestinationColorSpace::DisplayP3())
        configureSessionForDisplayP3(transferSession);
#endif
    else if (auto* platformColorSpace = colorSpace.platformColorSpace())
        configureSessionForColorSpace(transferSession, platformColorSpace);

    m_pixelTransferSession = adoptCF(transferSession);
}

struct CVPixelBufferInfo {
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    int lockCount { 0 };
};

static const void* CVPixelBufferGetBytePointerCallback(void* refcon)
{
    ASSERT(refcon);
    if (!refcon) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferGetBytePointerCallback() called with NULL refcon");
        RELEASE_LOG_STACKTRACE(Media);
        return nullptr;
    }
    auto info = static_cast<CVPixelBufferInfo*>(refcon);

    CVReturn result = CVPixelBufferLockBaseAddress(info->pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);

    ASSERT(result == kCVReturnSuccess);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferLockBaseAddress() returned error code %d", result);
        RELEASE_LOG_STACKTRACE(Media);
        return nullptr;
    }

    ++info->lockCount;
    void* address = CVPixelBufferGetBaseAddress(info->pixelBuffer.get());
    size_t byteLength = CVPixelBufferGetBytesPerRow(info->pixelBuffer.get()) * CVPixelBufferGetHeight(info->pixelBuffer.get());

    verifyImageBufferIsBigEnough(address, byteLength);
    RELEASE_LOG_INFO(Media, "CVPixelBufferGetBytePointerCallback() returning bytePointer: %p, size: %zu", address, byteLength);
    return address;
}

static void CVPixelBufferReleaseBytePointerCallback(void* refcon, const void*)
{
    ASSERT(refcon);
    if (!refcon) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseBytePointerCallback() called with NULL refcon");
        RELEASE_LOG_STACKTRACE(Media);
        return;
    }
    auto info = static_cast<CVPixelBufferInfo*>(refcon);

    CVReturn result = CVPixelBufferUnlockBaseAddress(info->pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    ASSERT(result == kCVReturnSuccess);
    if (result != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferLockBaseAddress() returned error code %d", result);
        RELEASE_LOG_STACKTRACE(Media);
        return;
    }

    ASSERT(info->lockCount);
    if (!info->lockCount) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseBytePointerCallback() called without matching CVPixelBufferGetBytePointerCallback()");
        RELEASE_LOG_STACKTRACE(Media);
    }
    --info->lockCount;
}

static void CVPixelBufferReleaseInfoCallback(void* refcon)
{
    ASSERT(refcon);
    if (!refcon) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseInfoCallback() called with NULL refcon");
        RELEASE_LOG_STACKTRACE(Media);
        return;
    }
    auto info = static_cast<CVPixelBufferInfo*>(refcon);

    ASSERT(!info->lockCount);
    if (info->lockCount) {
        RELEASE_LOG_ERROR(Media, "CVPixelBufferReleaseInfoCallback() called with a non-zero lockCount: %d", info->lockCount);
        RELEASE_LOG_STACKTRACE(Media);

        CVReturn result = CVPixelBufferUnlockBaseAddress(info->pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
        ASSERT(result == kCVReturnSuccess);
        if (result != kCVReturnSuccess) {
            RELEASE_LOG_ERROR(Media, "CVPixelBufferLockBaseAddress() returned error code %d", result);
            RELEASE_LOG_STACKTRACE(Media);
        }
    }

    info->pixelBuffer = nullptr;
    delete info;
}

RetainPtr<CVPixelBufferRef> PixelBufferConformerCV::convert(CVPixelBufferRef rawBuffer)
{
    if (isConformantPixelBuffer(rawBuffer))
        return rawBuffer;

    RetainPtr<CVPixelBufferRef> buffer { rawBuffer };

    size_t bufferWidth = CVPixelBufferGetWidth(rawBuffer);
    size_t bufferHeight = CVPixelBufferGetHeight(rawBuffer);

    if (!m_pixelBufferPool || m_lastWidth != bufferWidth || m_lastHeight != bufferHeight) {
        auto expectedPixelBufferPool = createCVPixelBufferPool(bufferWidth, bufferHeight, m_pixelFormat.value, 0, true, true);
        ASSERT(expectedPixelBufferPool);
        if (!expectedPixelBufferPool)
            return nullptr;
        m_pixelBufferPool = WTFMove(expectedPixelBufferPool.value());
    }

    auto expectedPixelBuffer = createCVPixelBufferFromPool(m_pixelBufferPool.get());
    ASSERT(expectedPixelBuffer);
    if (!expectedPixelBuffer)
        return nullptr;

    auto outputPixelBuffer = WTFMove(expectedPixelBuffer.value());

    OSStatus status = VTPixelTransferSessionTransferImage(m_pixelTransferSession.get(), buffer.get(), outputPixelBuffer.get());
    if (status != noErr || !outputPixelBuffer)
        return nullptr;

    return outputPixelBuffer;
}

RetainPtr<CGImageRef> PixelBufferConformerCV::createImageFromPixelBuffer(CVPixelBufferRef rawBuffer)
{
    RetainPtr<CVPixelBufferRef> buffer = convert(rawBuffer);
    return imageFrom32BGRAPixelBuffer(WTFMove(buffer), m_destinationColorSpace.platformColorSpace());
}

RetainPtr<CGImageRef> PixelBufferConformerCV::imageFrom32BGRAPixelBuffer(RetainPtr<CVPixelBufferRef>&& buffer, CGColorSpaceRef colorSpace)
{
    size_t width = CVPixelBufferGetWidth(buffer.get());
    size_t height = CVPixelBufferGetHeight(buffer.get());

    CGBitmapInfo bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaFirst);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer.get());
    size_t byteLength = bytesPerRow * height;

    ASSERT(byteLength);
    if (!byteLength)
        return nullptr;

    CVPixelBufferInfo* info = new CVPixelBufferInfo();
    info->pixelBuffer = WTFMove(buffer);
    info->lockCount = 0;

    CGDataProviderDirectCallbacks providerCallbacks = { 0, CVPixelBufferGetBytePointerCallback, CVPixelBufferReleaseBytePointerCallback, 0, CVPixelBufferReleaseInfoCallback };
    RetainPtr<CGDataProviderRef> provider = adoptCF(CGDataProviderCreateDirect(info, byteLength, &providerCallbacks));

    return adoptCF(CGImageCreate(width, height, 8, 32, bytesPerRow, colorSpace, bitmapInfo, provider.get(), nullptr, false, kCGRenderingIntentDefault));
}

bool PixelBufferConformerCV::isConformantPixelBuffer(CVPixelBufferRef pixelBuffer) const
{
    if (CVPixelBufferGetPixelFormatType(pixelBuffer) != m_pixelFormat.value)
        return false;

    if (m_destinationColorSpace == DestinationColorSpace::SRGB())
        return isPixelBufferConfiguredForSRGB(pixelBuffer);
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
    if (m_destinationColorSpace == DestinationColorSpace::DisplayP3())
        return isPixelBufferConfiguredForDisplayP3(pixelBuffer);
#endif
    if (auto* platformColorSpace = m_destinationColorSpace.platformColorSpace())
        return isPixelBufferConfiguredForColorSpace(pixelBuffer, platformColorSpace);

    ASSERT_NOT_REACHED();
    return false;
}

}
