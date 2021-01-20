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

#pragma once

#include "AffineTransform.h"
#include "FloatSize.h"

typedef struct OpaqueVTImageRotationSession* VTImageRotationSessionRef;
typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct __CVPixelBufferPool* CVPixelBufferPoolRef;

namespace WebCore {

class MediaSample;

class ImageRotationSessionVT final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct RotationProperties {
        bool flipX { false };
        bool flipY { false };
        unsigned angle { 0 };

        bool isIdentity() const { return !flipX && !flipY && !angle; }
    };

    enum class IsCGImageCompatible {
        No,
        Yes,
    };

    ImageRotationSessionVT(AffineTransform&&, FloatSize, OSType, IsCGImageCompatible);
    ImageRotationSessionVT(const RotationProperties&, FloatSize, OSType, IsCGImageCompatible);
    ImageRotationSessionVT() = default;

    const Optional<AffineTransform>& transform() const { return m_transform; }
    const RotationProperties& rotationProperties() const { return m_rotationProperties; }
    const FloatSize& size() { return m_size; }
    const FloatSize& rotatedSize() { return m_rotatedSize; }

    RetainPtr<CVPixelBufferRef> rotate(CVPixelBufferRef);
    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> rotate(MediaSample&, const RotationProperties&, IsCGImageCompatible);

    bool isMatching(MediaSample&, const RotationProperties&);

private:
    void initialize(const RotationProperties&, FloatSize, OSType pixelFormat, IsCGImageCompatible);

    Optional<AffineTransform> m_transform;
    RotationProperties m_rotationProperties;
    FloatSize m_size;
    FloatSize m_rotatedSize;
    RetainPtr<VTImageRotationSessionRef> m_rotationSession;
    RetainPtr<CVPixelBufferPoolRef> m_rotationPool;
};

inline bool operator==(const ImageRotationSessionVT::RotationProperties& rotation1, const ImageRotationSessionVT::RotationProperties& rotation2)
{
    return rotation1.flipX == rotation2.flipX && rotation1.flipY == rotation2.flipY && rotation1.angle == rotation2.angle;
}

inline bool operator!=(const ImageRotationSessionVT::RotationProperties& rotation1, const ImageRotationSessionVT::RotationProperties& rotation2)
{
    return !(rotation1 == rotation2);
}

}
