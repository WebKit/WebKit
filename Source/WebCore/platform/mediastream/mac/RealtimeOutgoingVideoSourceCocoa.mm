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

#import "AffineTransform.h"
#import "CVUtilities.h"
#import "ImageRotationSessionVT.h"
#import "Logging.h"
#import "RealtimeVideoUtilities.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"

namespace WebCore {

static inline unsigned rotationToAngle(webrtc::VideoRotation rotation)
{
    switch (rotation) {
    case webrtc::kVideoRotation_0:
        return 0;
    case webrtc::kVideoRotation_90:
        return 90;
    case webrtc::kVideoRotation_180:
        return 180;
    case webrtc::kVideoRotation_270:
        return 270;
    }
}

RetainPtr<CVPixelBufferRef> RealtimeOutgoingVideoSourceCocoa::rotatePixelBuffer(CVPixelBufferRef pixelBuffer, webrtc::VideoRotation rotation)
{
    ASSERT(rotation);
    if (!rotation)
        return pixelBuffer;

    auto pixelWidth = CVPixelBufferGetWidth(pixelBuffer);
    auto pixelHeight = CVPixelBufferGetHeight(pixelBuffer);
    if (!m_rotationSession || rotation != m_currentRotationSessionAngle || pixelWidth != m_rotatedWidth || pixelHeight != m_rotatedHeight) {
        RELEASE_LOG_INFO(WebRTC, "RealtimeOutgoingVideoSourceCocoa::rotatePixelBuffer creating rotation session for rotation %u", rotationToAngle(rotation));
        AffineTransform transform;
        transform.rotate(rotationToAngle(rotation));
        m_rotationSession = makeUnique<ImageRotationSessionVT>(WTFMove(transform), FloatSize { static_cast<float>(pixelWidth), static_cast<float>(pixelHeight) }, ImageRotationSessionVT::IsCGImageCompatible::No, ImageRotationSessionVT::ShouldUseIOSurface::No);

        m_currentRotationSessionAngle = rotation;
        m_rotatedWidth = pixelWidth;
        m_rotatedHeight = pixelHeight;
    }

    return m_rotationSession->rotate(pixelBuffer);
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
