/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#include <WebCore/PlatformVideoColorSpace.h>
#include <WebCore/VideoFrame.h>

#if ENABLE(VIDEO) && USE(LIBWEBRTC)

namespace webrtc {
class ColorSpace;
class VideoFrame;
}

namespace WebCore {

WEBCORE_EXPORT std::optional<PlatformVideoColorSpace> colorSpaceFromLibWebRTCColorSpace(const webrtc::ColorSpace&);
WEBCORE_EXPORT std::optional<PlatformVideoColorSpace> colorSpaceFromLibWebRTCVideoFrame(const webrtc::VideoFrame&);
WEBCORE_EXPORT VideoFrameRotation videoRotationFromLibWebRTCVideoFrame(const webrtc::VideoFrame&);

static inline std::optional<PlatformVideoColorSpace> colorSpaceFromLibWebRTCColorSpace(const webrtc::ColorSpace* webrtcColorSpace)
{
    return webrtcColorSpace ? colorSpaceFromLibWebRTCColorSpace(*webrtcColorSpace) : std::nullopt;
}

} // namespace WebCore

#endif
