/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "ImageRotationSessionVT.h"

#include "AffineTransform.h"

#include "CoreVideoSoftLink.h"
#include "VideoToolboxSoftLink.h"

#if USE(VIDEOTOOLBOX)

namespace WebCore {

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

ImageRotationSessionVT::ImageRotationSessionVT(AffineTransform&& transform, FloatSize size, OSType pixelFormat, IsCGImageCompatible cvImageCompatibility)
    : ImageRotationSessionVT(transformToRotationProperties(transform), size, pixelFormat, cvImageCompatibility)
{
    m_transform = WTFMove(transform);
}


ImageRotationSessionVT::ImageRotationSessionVT(RotationProperties&& rotation, FloatSize size, OSType pixelFormat, IsCGImageCompatible cvImageCompatibility)
    : m_rotationProperties(WTFMove(rotation))
    , m_size(size)
{
    if (m_rotationProperties.angle == 90 || m_rotationProperties.angle == 270)
        size = size.transposedSize();

    m_rotatedSize = expandedIntSize(size);

    VTImageRotationSessionRef rawRotationSession = nullptr;
    VTImageRotationSessionCreate(kCFAllocatorDefault, m_rotationProperties.angle, &rawRotationSession);
    m_rotationSession = rawRotationSession;
    VTImageRotationSessionSetProperty(m_rotationSession.get(), kVTImageRotationPropertyKey_EnableHighSpeedTransfer, kCFBooleanTrue);

    if (m_rotationProperties.flipY)
        VTImageRotationSessionSetProperty(m_rotationSession.get(), kVTImageRotationPropertyKey_FlipVerticalOrientation, kCFBooleanTrue);
    if (m_rotationProperties.flipX)
        VTImageRotationSessionSetProperty(m_rotationSession.get(), kVTImageRotationPropertyKey_FlipHorizontalOrientation, kCFBooleanTrue);

    auto pixelAttributes = @{
        (__bridge NSString *)kCVPixelBufferWidthKey: @(m_rotatedSize.width()),
        (__bridge NSString *)kCVPixelBufferHeightKey: @(m_rotatedSize.height()),
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(pixelFormat),
        (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey: (cvImageCompatibility == IsCGImageCompatible::Yes ? @YES : @NO),
    };
    CVPixelBufferPoolRef rawPool = nullptr;
    if (auto err = CVPixelBufferPoolCreate(kCFAllocatorDefault, nullptr, (__bridge CFDictionaryRef)pixelAttributes, &rawPool); err != noErr)
        LOG_ERROR("CVPixelBufferPool create returned error code %d", err);
    m_rotationPool = adoptCF(rawPool);
}

RetainPtr<CVPixelBufferRef> ImageRotationSessionVT::rotate(CVPixelBufferRef pixelBuffer)
{
    CVPixelBufferRef rawRotatedBuffer = nullptr;
    CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, m_rotationPool.get(), &rawRotatedBuffer);
    auto status = VTImageRotationSessionTransferImage(m_rotationSession.get(), pixelBuffer, rawRotatedBuffer);
    if (status == noErr)
        return adoptCF(rawRotatedBuffer);
    return nullptr;
}

}

#endif
