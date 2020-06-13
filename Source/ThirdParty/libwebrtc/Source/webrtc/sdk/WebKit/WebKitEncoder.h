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

#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video/encoded_image.h"
#include "modules/include/module_common_types.h"

using CVPixelBufferRef = struct __CVBuffer*;

namespace webrtc {

class EncodedImageCallback;
class VideoCodec;
class VideoFrame;
struct SdpVideoFormat;
class Settings;

class VideoEncoderFactoryWithSimulcast final : public VideoEncoderFactory {
public:
    explicit VideoEncoderFactoryWithSimulcast(std::unique_ptr<VideoEncoderFactory>&& factory)
        : m_internalEncoderFactory(std::move(factory))
    {
    }

    VideoEncoderFactory::CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const final { return m_internalEncoderFactory->QueryVideoEncoder(format); }

    std::unique_ptr<VideoEncoder> CreateVideoEncoder(const SdpVideoFormat& format) final;
 
    std::vector<SdpVideoFormat> GetSupportedFormats() const final { return m_internalEncoderFactory->GetSupportedFormats(); }

private:
    const std::unique_ptr<VideoEncoderFactory> m_internalEncoderFactory;
};

using WebKitVideoEncoder = void*;
using VideoEncoderCreateCallback = WebKitVideoEncoder(*)(const SdpVideoFormat& format);
using VideoEncoderReleaseCallback = int32_t(*)(WebKitVideoEncoder);
using VideoEncoderInitializeCallback = int32_t(*)(WebKitVideoEncoder, const VideoCodec&);
using VideoEncoderEncodeCallback = int32_t(*)(WebKitVideoEncoder, const VideoFrame&, bool shouldEncodeAsKeyFrame);
using VideoEncoderRegisterEncodeCompleteCallback = int32_t(*)(WebKitVideoEncoder, void* encodedImageCallback);
using VideoEncoderSetRatesCallback = void(*)(WebKitVideoEncoder, const VideoEncoder::RateControlParameters&);

void setVideoEncoderCallbacks(VideoEncoderCreateCallback, VideoEncoderReleaseCallback, VideoEncoderInitializeCallback, VideoEncoderEncodeCallback, VideoEncoderRegisterEncodeCompleteCallback, VideoEncoderSetRatesCallback);

using WebKitEncodedFrameTiming = EncodedImage::Timing;
struct WebKitEncodedFrameInfo {
    uint32_t width { 0 };
    uint32_t height { 0 };
    int64_t timeStamp { 0 };
    int64_t ntpTimeMS { 0 };
    int64_t captureTimeMS { 0 };
    VideoFrameType frameType { VideoFrameType::kVideoFrameDelta };
    VideoRotation rotation { kVideoRotation_0 };
    VideoContentType contentType { VideoContentType::UNSPECIFIED };
    bool completeFrame = false;
    int qp { -1 };
    WebKitEncodedFrameTiming timing;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, WebKitEncodedFrameInfo&);
};

class WebKitRTPFragmentationHeader {
public:
    explicit WebKitRTPFragmentationHeader(webrtc::RTPFragmentationHeader* = nullptr);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, WebKitRTPFragmentationHeader&);

    webrtc::RTPFragmentationHeader* value() { return m_value; };

private:
    webrtc::RTPFragmentationHeader* m_value { nullptr };
    std::unique_ptr<webrtc::RTPFragmentationHeader> m_ownedHeader;
};

class RemoteVideoEncoder final : public webrtc::VideoEncoder {
public:
    explicit RemoteVideoEncoder(WebKitVideoEncoder);
    ~RemoteVideoEncoder() = default;

    static void encodeComplete(void* callback, uint8_t* buffer, size_t length, const WebKitEncodedFrameInfo&, const webrtc::RTPFragmentationHeader*);

private:
    int32_t InitEncode(const VideoCodec*, const Settings&) final;
    int32_t RegisterEncodeCompleteCallback(EncodedImageCallback*) final;
    int32_t Release() final;
    int32_t Encode(const VideoFrame&, const std::vector<VideoFrameType>*) final;
    void SetRates(const RateControlParameters&) final;
    EncoderInfo GetEncoderInfo() const final;

    WebKitVideoEncoder m_internalEncoder;
};

using LocalEncoder = void*;
using LocalEncoderCallback = void (^)(const uint8_t* buffer, size_t size, const webrtc::WebKitEncodedFrameInfo&, webrtc::RTPFragmentationHeader*);
void* createLocalEncoder(const webrtc::SdpVideoFormat&, LocalEncoderCallback);
void releaseLocalEncoder(LocalEncoder);
void initializeLocalEncoder(LocalEncoder, uint16_t width, uint16_t height, unsigned int startBitrate, unsigned int maxBitrate, unsigned int minBitrate, uint32_t maxFramerate);
void encodeLocalEncoderFrame(LocalEncoder, CVPixelBufferRef, int64_t timeStamp, webrtc::VideoRotation, bool isKeyframeRequired);
void setLocalEncoderRates(LocalEncoder, uint32_t bitRate, uint32_t frameRate);

template<class Decoder>
bool WebKitEncodedFrameInfo::decode(Decoder& decoder, WebKitEncodedFrameInfo& info)
{
    if (!decoder.decode(info.width))
        return false;
    if (!decoder.decode(info.height))
        return false;
    if (!decoder.decode(info.timeStamp))
        return false;
    if (!decoder.decode(info.ntpTimeMS))
        return false;
    if (!decoder.decode(info.captureTimeMS))
        return false;
    if (!decoder.decode(info.frameType))
        return false;
    if (!decoder.decode(info.rotation))
        return false;
    if (!decoder.decode(info.contentType))
        return false;
    if (!decoder.decode(info.completeFrame))
        return false;
    if (!decoder.decode(info.qp))
        return false;

    if (!decoder.decode(info.timing.flags))
        return false;

    if (!decoder.decode(info.timing.encode_start_ms))
        return false;

    if (!decoder.decode(info.timing.encode_finish_ms))
        return false;

    if (!decoder.decode(info.timing.packetization_finish_ms))
        return false;

    if (!decoder.decode(info.timing.pacer_exit_ms))
        return false;

    if (!decoder.decode(info.timing.network_timestamp_ms))
        return false;

    if (!decoder.decode(info.timing.network2_timestamp_ms))
        return false;

    if (!decoder.decode(info.timing.receive_start_ms))
        return false;

    return true;
}

template<class Encoder>
void WebKitEncodedFrameInfo::encode(Encoder& encoder) const
{
    encoder << width;
    encoder << height;
    encoder << timeStamp;
    encoder << ntpTimeMS;
    encoder << captureTimeMS;
    encoder << frameType;
    encoder << rotation;
    encoder << contentType;
    encoder << completeFrame;
    encoder << qp;

    encoder << timing.flags;
    encoder << timing.encode_start_ms;
    encoder << timing.encode_finish_ms;
    encoder << timing.packetization_finish_ms;
    encoder << timing.pacer_exit_ms;
    encoder << timing.network_timestamp_ms;
    encoder << timing.network2_timestamp_ms;
    encoder << timing.receive_start_ms;
    encoder << timing.receive_finish_ms;
}

inline WebKitRTPFragmentationHeader::WebKitRTPFragmentationHeader(webrtc::RTPFragmentationHeader* header)
    : m_value(header)
{
}

template<class Encoder>
void WebKitRTPFragmentationHeader::encode(Encoder& encoder) const
{
    encoder << !!m_value;
    if (!m_value)
        return;
    encoder << static_cast<unsigned>(m_value->Size());
    for (unsigned i = 0; i < m_value->Size(); ++i) {
        encoder << static_cast<unsigned>(m_value->Offset(i));
        encoder << static_cast<unsigned>(m_value->Length(i));
    }
}

template<class Decoder>
bool WebKitRTPFragmentationHeader::decode(Decoder& decoder, WebKitRTPFragmentationHeader& header)
{
    bool isNull;
    if (!decoder.decode(isNull))
        return false;
    
    if (isNull) {
        header.m_value = nullptr;
        return true;
    }

    unsigned size;
    if (!decoder.decode(size))
        return false;

    header.m_ownedHeader = std::make_unique<webrtc::RTPFragmentationHeader>();
    header.m_value = header.m_ownedHeader.get();

    header.m_value->VerifyAndAllocateFragmentationHeader(size);
    for (size_t i = 0; i < header.m_value->Size(); ++i) {
        unsigned offset, length;
        if (!decoder.decode(offset))
            return false;
        if (!decoder.decode(length))
            return false;

        header.m_value->fragmentationOffset[i] = offset;
        header.m_value->fragmentationLength[i] = length;
    }

    return true;
}

}
