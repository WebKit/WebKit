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
#include "VideoFrameCV.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#include "CoreVideoSoftLink.h"

namespace WebCore {

Ref<VideoFrameCV> VideoFrameCV::create(MediaTime presentationTime, bool isMirrored, VideoRotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer)
{
    ASSERT(pixelBuffer);
    return adoptRef(*new VideoFrameCV(presentationTime, isMirrored, rotation, WTFMove(pixelBuffer)));
}

VideoFrameCV::VideoFrameCV(MediaTime presentationTime, bool isMirrored, VideoRotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer)
    : VideoFrame(presentationTime, isMirrored, rotation)
    , m_pixelBuffer(WTFMove(pixelBuffer))
{
}

VideoFrameCV::~VideoFrameCV() = default;

WebCore::FloatSize VideoFrameCV::presentationSize() const
{
    return { static_cast<float>(CVPixelBufferGetWidth(m_pixelBuffer.get())), static_cast<float>(CVPixelBufferGetHeight(m_pixelBuffer.get())) };
}

uint32_t VideoFrameCV::videoPixelFormat() const
{
    return CVPixelBufferGetPixelFormatType(m_pixelBuffer.get());
}

ImageOrientation VideoFrameCV::orientation() const
{
    // Sample transform first flips x-coordinates, then rotates.
    switch (m_rotation) {
    case MediaSample::VideoRotation::None:
        return m_isMirrored ? ImageOrientation::OriginTopRight : ImageOrientation::OriginTopLeft;
    case MediaSample::VideoRotation::Right:
        return m_isMirrored ? ImageOrientation::OriginRightBottom : ImageOrientation::OriginRightTop;
    case MediaSample::VideoRotation::UpsideDown:
        return m_isMirrored ? ImageOrientation::OriginBottomLeft : ImageOrientation::OriginBottomRight;
    case MediaSample::VideoRotation::Left:
        return m_isMirrored ? ImageOrientation::OriginLeftTop : ImageOrientation::OriginLeftBottom;
    }
}

}

#endif
