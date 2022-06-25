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
#include "CVUtilities.h"
#include "PixelBuffer.h"
#include "ProcessIdentity.h"
#include "CoreVideoSoftLink.h"

namespace WebCore {

Ref<VideoFrameCV> VideoFrameCV::create(CMSampleBufferRef sampleBuffer, bool isMirrored, Rotation rotation)
{
    auto pixelBuffer = static_cast<CVPixelBufferRef>(PAL::CMSampleBufferGetImageBuffer(sampleBuffer));
    auto timeStamp = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sampleBuffer);
    if (CMTIME_IS_INVALID(timeStamp))
        timeStamp = PAL::CMSampleBufferGetPresentationTimeStamp(sampleBuffer);

    return VideoFrameCV::create(PAL::toMediaTime(timeStamp), isMirrored, rotation, pixelBuffer);
}

Ref<VideoFrameCV> VideoFrameCV::create(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer)
{
    ASSERT(pixelBuffer);
    return adoptRef(*new VideoFrameCV(presentationTime, isMirrored, rotation, WTFMove(pixelBuffer)));
}

RefPtr<VideoFrameCV> VideoFrameCV::createFromPixelBuffer(Ref<PixelBuffer>&& pixelBuffer)
{
    auto size = pixelBuffer->size();
    auto width = size.width();
    auto height = size.height();

    auto dataBaseAddress = pixelBuffer->bytes();
    auto leakedBuffer = &pixelBuffer.leakRef();
    
    auto derefBuffer = [] (void* context, const void*) {
        static_cast<PixelBuffer*>(context)->deref();
    };

    CVPixelBufferRef cvPixelBufferRaw = nullptr;
    auto status = CVPixelBufferCreateWithBytes(kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, dataBaseAddress, width * 4, derefBuffer, leakedBuffer, nullptr, &cvPixelBufferRaw);

    auto cvPixelBuffer = adoptCF(cvPixelBufferRaw);
    if (!cvPixelBuffer) {
        derefBuffer(leakedBuffer, nullptr);
        return nullptr;
    }
    ASSERT_UNUSED(status, !status);
    return create({ }, false, Rotation::None, WTFMove(cvPixelBuffer));
}

VideoFrameCV::VideoFrameCV(MediaTime presentationTime, bool isMirrored, Rotation rotation, RetainPtr<CVPixelBufferRef>&& pixelBuffer)
    : VideoFrame(presentationTime, isMirrored, rotation)
    , m_pixelBuffer(WTFMove(pixelBuffer))
{
}

VideoFrameCV::~VideoFrameCV() = default;

WebCore::FloatSize VideoFrameCV::presentationSize() const
{
    return { static_cast<float>(CVPixelBufferGetWidth(m_pixelBuffer.get())), static_cast<float>(CVPixelBufferGetHeight(m_pixelBuffer.get())) };
}

uint32_t VideoFrameCV::pixelFormat() const
{
    return CVPixelBufferGetPixelFormatType(m_pixelBuffer.get());
}

void VideoFrameCV::setOwnershipIdentity(const ProcessIdentity& resourceOwner)
{
    ASSERT(resourceOwner);
    auto buffer = pixelBuffer();
    ASSERT(buffer);
    setOwnershipIdentityForCVPixelBuffer(buffer, resourceOwner);
}

ImageOrientation VideoFrameCV::orientation() const
{
    // Sample transform first flips x-coordinates, then rotates.
    switch (rotation()) {
    case VideoFrame::Rotation::None:
        return isMirrored() ? ImageOrientation::OriginTopRight : ImageOrientation::OriginTopLeft;
    case VideoFrame::Rotation::Right:
        return isMirrored() ? ImageOrientation::OriginRightBottom : ImageOrientation::OriginRightTop;
    case VideoFrame::Rotation::UpsideDown:
        return isMirrored() ? ImageOrientation::OriginBottomLeft : ImageOrientation::OriginBottomRight;
    case VideoFrame::Rotation::Left:
        return isMirrored() ? ImageOrientation::OriginLeftTop : ImageOrientation::OriginLeftBottom;
    }
}

}

#endif
