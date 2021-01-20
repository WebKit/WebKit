/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)

#include <webrtc/api/video/video_content_type.h>
#include <webrtc/api/video/video_frame_type.h>
#include <webrtc/api/video/video_rotation.h>
#include <webrtc/rtc_base/dscp.h>
#include <wtf/EnumTraits.h>

namespace WTF {

template<> struct EnumTraits<rtc::DiffServCodePoint> {
    using values = EnumValues<
        rtc::DiffServCodePoint,
        rtc::DiffServCodePoint::DSCP_NO_CHANGE,
        rtc::DiffServCodePoint::DSCP_DEFAULT,
        rtc::DiffServCodePoint::DSCP_CS0,
        rtc::DiffServCodePoint::DSCP_CS1,
        rtc::DiffServCodePoint::DSCP_AF11,
        rtc::DiffServCodePoint::DSCP_AF12,
        rtc::DiffServCodePoint::DSCP_AF13,
        rtc::DiffServCodePoint::DSCP_CS2,
        rtc::DiffServCodePoint::DSCP_AF21,
        rtc::DiffServCodePoint::DSCP_AF22,
        rtc::DiffServCodePoint::DSCP_AF23,
        rtc::DiffServCodePoint::DSCP_CS3,
        rtc::DiffServCodePoint::DSCP_AF31,
        rtc::DiffServCodePoint::DSCP_AF32,
        rtc::DiffServCodePoint::DSCP_AF33,
        rtc::DiffServCodePoint::DSCP_CS4,
        rtc::DiffServCodePoint::DSCP_AF41,
        rtc::DiffServCodePoint::DSCP_AF42,
        rtc::DiffServCodePoint::DSCP_AF43,
        rtc::DiffServCodePoint::DSCP_CS5,
        rtc::DiffServCodePoint::DSCP_EF,
        rtc::DiffServCodePoint::DSCP_CS6,
        rtc::DiffServCodePoint::DSCP_CS7
    >;
};

template<> struct EnumTraits<webrtc::VideoContentType> {
    using values = EnumValues<
        webrtc::VideoContentType,
        webrtc::VideoContentType::UNSPECIFIED,
        webrtc::VideoContentType::SCREENSHARE
    >;
};

template<> struct EnumTraits<webrtc::VideoFrameType> {
    using values = EnumValues<
        webrtc::VideoFrameType,
        webrtc::VideoFrameType::kEmptyFrame,
        webrtc::VideoFrameType::kVideoFrameKey,
        webrtc::VideoFrameType::kVideoFrameDelta
    >;
};

template<> struct EnumTraits<webrtc::VideoRotation> {
    using values = EnumValues<
        webrtc::VideoRotation,
        webrtc::VideoRotation::kVideoRotation_0,
        webrtc::VideoRotation::kVideoRotation_90,
        webrtc::VideoRotation::kVideoRotation_180,
        webrtc::VideoRotation::kVideoRotation_270
    >;
};

} // namespace WTF

#endif // USE(LIBWEBRTC)
