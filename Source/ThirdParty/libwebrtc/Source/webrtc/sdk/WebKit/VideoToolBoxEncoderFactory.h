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

#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

class H264VideoToolboxEncoder;

class VideoToolboxVideoEncoderFactory final : public webrtc::VideoEncoderFactory {
public:
    class DestructorObserver {
    public:
        virtual ~DestructorObserver() = default;
        virtual void notifyOfEncoderFactoryDestruction(VideoToolboxVideoEncoderFactory&) = 0;
    };

    explicit VideoToolboxVideoEncoderFactory(DestructorObserver* observer)
        : m_destructorObserver(observer)
    {
    }
    ~VideoToolboxVideoEncoderFactory();
    void ClearDestructorObserver() { m_destructorObserver = nullptr; }

    void SetActive(bool isActive);
    void Add(H264VideoToolboxEncoder&);
    void Remove(H264VideoToolboxEncoder&);
    void setH264HardwareEncoderAllowed(bool allowed);

private:
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const final;
    CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat&) const final;
    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat&) final;

    std::vector<std::reference_wrapper<H264VideoToolboxEncoder>> m_encoders;
    bool m_isActive { true };
    DestructorObserver* m_destructorObserver { nullptr };
    bool m_h264HardwareEncoderAllowed { true };
};

}
