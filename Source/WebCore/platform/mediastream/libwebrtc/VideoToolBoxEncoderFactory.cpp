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

#include "config.h"
#include "VideoToolBoxEncoderFactory.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

#include "H264VideoToolBoxEncoder.h"
#include <webrtc/sdk/objc/Framework/Classes/VideoToolbox/encoder.h>

namespace WebCore {

void VideoToolboxVideoEncoderFactory::setActive(bool isActive)
{
    if (m_isActive == isActive)
        return;

    m_isActive = isActive;
    for (auto& encoder : m_encoders)
        encoder.get().SetActive(isActive);
}

webrtc::VideoEncoder* VideoToolboxVideoEncoderFactory::CreateSupportedVideoEncoder(const cricket::VideoCodec& codec)
{
#if ENABLE_VCP_ENCODER
    auto* encoder = webrtc::createH264VideoToolboxEncoderVCP(codec);
#else
    auto* encoder = new H264VideoToolboxEncoder(codec);
#endif
    m_encoders.append(*encoder);

    return encoder;
}

void VideoToolboxVideoEncoderFactory::DestroyVideoEncoder(webrtc::VideoEncoder* encoder)
{
    m_encoders.removeFirstMatching([&] (const auto& item) {
        auto result = &item.get() == encoder;
        if (result) {
#if ENABLE_VCP_ENCODER
            webrtc::deleteH264VideoToolboxEncoderVCP(&item.get());
#else
            delete encoder;
#endif
        }
        return result;
    });
}

}
#endif
