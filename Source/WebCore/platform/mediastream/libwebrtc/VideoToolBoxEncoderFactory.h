/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "LibWebRTCMacros.h"
#include <webrtc/sdk/objc/Framework/Classes/VideoProcessing/encoder_vcp.h>
#include <webrtc/sdk/objc/Framework/Classes/VideoToolbox/videocodecfactory.h>
#include <wtf/Vector.h>

namespace webrtc {
class H264VideoToolboxEncoderVCP;
}

namespace WebCore {

class H264VideoToolboxEncoder;

class VideoToolboxVideoEncoderFactory final : public webrtc::VideoToolboxVideoEncoderFactory {
public:
    VideoToolboxVideoEncoderFactory() = default;

    void setActive(bool isActive);

private:
    webrtc::VideoEncoder* CreateSupportedVideoEncoder(const cricket::VideoCodec&) final;
    void DestroyVideoEncoder(webrtc::VideoEncoder*) final;

#if ENABLE_VCP_ENCODER
    Vector<std::reference_wrapper<webrtc::H264VideoToolboxEncoderVCP>> m_encoders;
#else
    Vector<std::reference_wrapper<H264VideoToolboxEncoder>> m_encoders;
#endif
    bool m_isActive { true };
};

}

#endif
