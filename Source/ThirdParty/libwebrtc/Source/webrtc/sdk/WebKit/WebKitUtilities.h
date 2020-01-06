/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "api/video/video_frame_buffer.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/engine/encoder_simulcast_proxy.h"

typedef struct __CVBuffer* CVPixelBufferRef;

namespace webrtc {

class VideoDecoderFactory;
class VideoEncoderFactory;
class VideoFrame;

enum class WebKitCodecSupport { H264, H264AndVP8 };

std::unique_ptr<webrtc::VideoEncoderFactory> createWebKitEncoderFactory(WebKitCodecSupport);
std::unique_ptr<webrtc::VideoDecoderFactory> createWebKitDecoderFactory(WebKitCodecSupport);

void setApplicationStatus(bool isActive);

void setH264HardwareEncoderAllowed(bool);
bool isH264HardwareEncoderAllowed();

CVPixelBufferRef pixelBufferFromFrame(const VideoFrame&, const std::function<CVPixelBufferRef(size_t, size_t)>&);
rtc::scoped_refptr<webrtc::VideoFrameBuffer> pixelBufferToFrame(CVPixelBufferRef);

using WebKitVideoDecoder = void*;
using VideoDecoderCreateCallback = WebKitVideoDecoder(*)(const SdpVideoFormat& format);
using VideoDecoderReleaseCallback = int32_t(*)(WebKitVideoDecoder);
using VideoDecoderDecodeCallback = int32_t(*)(WebKitVideoDecoder, uint32_t timeStamp, const uint8_t*, size_t length);
using VideoDecoderRegisterDecodeCompleteCallback = int32_t(*)(WebKitVideoDecoder, void* decodedImageCallback);

void setVideoDecoderCallbacks(VideoDecoderCreateCallback, VideoDecoderReleaseCallback, VideoDecoderDecodeCallback, VideoDecoderRegisterDecodeCompleteCallback);

class RemoteVideoDecoderFactory final : public VideoDecoderFactory {
public:
    explicit RemoteVideoDecoderFactory(std::unique_ptr<VideoDecoderFactory>&&);
    ~RemoteVideoDecoderFactory() = default;

private:
    std::vector<SdpVideoFormat> GetSupportedFormats() const final;
    std::unique_ptr<VideoDecoder> CreateVideoDecoder(const SdpVideoFormat& format) final;

    std::unique_ptr<VideoDecoderFactory> m_internalFactory;
};

class RemoteVideoDecoder final : public webrtc::VideoDecoder {
public:
    explicit RemoteVideoDecoder(WebKitVideoDecoder);
    ~RemoteVideoDecoder() = default;

    static void decodeComplete(void* callback, uint32_t timeStamp, CVPixelBufferRef, uint32_t timeStampRTP);

private:
    int32_t InitDecode(const VideoCodec*, int32_t number_of_cores) final;
    int32_t Decode(const EncodedImage&, bool missing_frames, int64_t render_time_ms) final;
    int32_t RegisterDecodeCompleteCallback(DecodedImageCallback*) final;
    int32_t Release() final;
    const char* ImplementationName() const final { return "RemoteVideoToolBox"; }

    WebKitVideoDecoder m_internalDecoder;
};

using LocalDecoder = void*;
using LocalDecoderCallback = void (^)(CVPixelBufferRef, uint32_t timeStamp, uint32_t timeStampNs);
void* createLocalDecoder(LocalDecoderCallback);
void releaseLocalDecoder(LocalDecoder);
int32_t decodeFrame(LocalDecoder, uint32_t timeStamp, const uint8_t*, size_t);

}
