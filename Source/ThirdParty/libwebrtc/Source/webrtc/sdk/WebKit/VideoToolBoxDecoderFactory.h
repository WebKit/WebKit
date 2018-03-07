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

#include "api/video_codecs/video_decoder_factory.h"

namespace webrtc {

class H264VideoToolboxDecoder;

class VideoToolboxVideoDecoderFactory final : public VideoDecoderFactory {
public:
    class DestructorObserver {
    public:
        virtual ~DestructorObserver() = default;
        virtual void notifyOfDecoderFactoryDestruction(VideoToolboxVideoDecoderFactory&) = 0;
    };

    explicit VideoToolboxVideoDecoderFactory(DestructorObserver* observer)
        : m_destructorObserver(observer)
    {
    }

    ~VideoToolboxVideoDecoderFactory();
    void ClearDestructorObserver() { m_destructorObserver = nullptr; }

    void SetActive(bool isActive);

    void Add(H264VideoToolboxDecoder&);
    void Remove(H264VideoToolboxDecoder&);

private:
    std::vector<SdpVideoFormat> GetSupportedFormats() const final;
    std::unique_ptr<VideoDecoder> CreateVideoDecoder(const SdpVideoFormat& format) final;

    std::vector<std::reference_wrapper<H264VideoToolboxDecoder>> m_decoders;
    bool m_isActive { true };
    DestructorObserver* m_destructorObserver { nullptr };
};

}

