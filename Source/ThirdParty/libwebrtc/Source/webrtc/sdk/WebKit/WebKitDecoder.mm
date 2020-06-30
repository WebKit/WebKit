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

#include "WebKitDecoder.h"

#import "Framework/Headers/WebRTC/RTCVideoCodecFactory.h"
#import "Framework/Headers/WebRTC/RTCVideoFrameBuffer.h"
#import "Framework/Native/api/video_decoder_factory.h"
#import "WebKitUtilities.h"
#import "sdk/objc/components/video_codec/RTCVideoDecoderH264.h"
#import "sdk/objc/components/video_codec/RTCVideoDecoderH265.h"

@interface WK_RTCLocalVideoH264H265Decoder : NSObject
- (instancetype)initH264DecoderWithCallback:(webrtc::LocalDecoderCallback)callback;
- (instancetype)initH265DecoderWithCallback:(webrtc::LocalDecoderCallback)callback;
- (NSInteger)decodeData:(const uint8_t *)data size:(size_t)size timeStamp:(uint32_t)timeStamp;
- (NSInteger)releaseDecoder;
@end

@implementation WK_RTCLocalVideoH264H265Decoder {
    RTCVideoDecoderH264 *m_h264Decoder;
    RTCVideoDecoderH265 *m_h265Decoder;
}

- (instancetype)initH264DecoderWithCallback:(webrtc::LocalDecoderCallback)callback {
    if (self = [super init]) {
        m_h264Decoder = [[RTCVideoDecoderH264 alloc] init];
        [m_h264Decoder setCallback:^(RTCVideoFrame *frame) {
            auto *buffer = (RTCCVPixelBuffer *)frame.buffer;
            callback(buffer.pixelBuffer, frame.timeStampNs, frame.timeStamp);
        }];
    }
    return self;
}

- (instancetype)initH265DecoderWithCallback:(webrtc::LocalDecoderCallback)callback {
    if (self = [super init]) {
        m_h265Decoder = [[RTCVideoDecoderH265 alloc] init];
        [m_h265Decoder setCallback:^(RTCVideoFrame *frame) {
            auto *buffer = (RTCCVPixelBuffer *)frame.buffer;
            callback(buffer.pixelBuffer, frame.timeStampNs, frame.timeStamp);
        }];
    }
    return self;
}

- (NSInteger)decodeData:(const uint8_t *)data size:(size_t)size timeStamp:(uint32_t)timeStamp {
    if (m_h264Decoder)
        return [m_h264Decoder decodeData:data size:size timeStamp:timeStamp];
    if (m_h265Decoder)
        return [m_h265Decoder decodeData:data size:size timeStamp:timeStamp];
    return 0;
}

- (NSInteger)releaseDecoder {
    if (m_h264Decoder)
        return [m_h264Decoder releaseDecoder];
    return [m_h265Decoder releaseDecoder];
}
@end

namespace webrtc {

struct VideoDecoderCallbacks {
    VideoDecoderCreateCallback createCallback;
    VideoDecoderReleaseCallback releaseCallback;
    VideoDecoderDecodeCallback decodeCallback;
    VideoDecoderRegisterDecodeCompleteCallback registerDecodeCompleteCallback;
};

static VideoDecoderCallbacks& videoDecoderCallbacks() {
    static VideoDecoderCallbacks callbacks;
    return callbacks;
}

void setVideoDecoderCallbacks(VideoDecoderCreateCallback createCallback, VideoDecoderReleaseCallback releaseCallback, VideoDecoderDecodeCallback decodeCallback, VideoDecoderRegisterDecodeCompleteCallback registerDecodeCompleteCallback)
{
    auto& callbacks = videoDecoderCallbacks();
    callbacks.createCallback = createCallback;
    callbacks.releaseCallback = releaseCallback;
    callbacks.decodeCallback = decodeCallback;
    callbacks.registerDecodeCompleteCallback = registerDecodeCompleteCallback;
}

RemoteVideoDecoder::RemoteVideoDecoder(WebKitVideoDecoder internalDecoder)
    : m_internalDecoder(internalDecoder)
{
}

void RemoteVideoDecoder::decodeComplete(void* callback, uint32_t timeStamp, CVPixelBufferRef pixelBuffer, uint32_t timeStampRTP)
{
    auto videoFrame = VideoFrame::Builder().set_video_frame_buffer(pixelBufferToFrame(pixelBuffer))
        .set_timestamp_rtp(timeStampRTP)
        .set_timestamp_ms(0)
        .set_rotation((VideoRotation)RTCVideoRotation_0)
        .build();
    videoFrame.set_timestamp(timeStamp);

    static_cast<DecodedImageCallback*>(callback)->Decoded(videoFrame);
}

int32_t RemoteVideoDecoder::InitDecode(const VideoCodec* codec_settings, int32_t number_of_cores)
{
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RemoteVideoDecoder::Decode(const EncodedImage& input_image, bool missing_frames, int64_t render_time_ms)
{
    return videoDecoderCallbacks().decodeCallback(m_internalDecoder, input_image.Timestamp(), input_image.data(), input_image.size());
}

int32_t RemoteVideoDecoder::RegisterDecodeCompleteCallback(DecodedImageCallback* callback)
{
    return videoDecoderCallbacks().registerDecodeCompleteCallback(m_internalDecoder, callback);
}

int32_t RemoteVideoDecoder::Release()
{
    return videoDecoderCallbacks().releaseCallback(m_internalDecoder);
}

RemoteVideoDecoderFactory::RemoteVideoDecoderFactory(std::unique_ptr<VideoDecoderFactory>&& internalFactory)
    : m_internalFactory(std::move(internalFactory))
{
}

std::vector<SdpVideoFormat> RemoteVideoDecoderFactory::GetSupportedFormats() const
{
    std::vector<SdpVideoFormat> supported_formats;
    supported_formats.push_back(SdpVideoFormat { "H264" });
    supported_formats.push_back(SdpVideoFormat { "H265" });
    supported_formats.push_back(SdpVideoFormat { "VP8" });
    return supported_formats;
}

std::unique_ptr<VideoDecoder> RemoteVideoDecoderFactory::CreateVideoDecoder(const SdpVideoFormat& format)
{
    if (!videoDecoderCallbacks().createCallback)
        return m_internalFactory->CreateVideoDecoder(format);

    auto identifier = videoDecoderCallbacks().createCallback(format);
    if (!identifier)
        return m_internalFactory->CreateVideoDecoder(format);

    return std::make_unique<RemoteVideoDecoder>(identifier);
}

std::unique_ptr<webrtc::VideoDecoderFactory> createWebKitDecoderFactory(WebKitH265 supportsH265, WebKitVP9 supportsVP9)
{
    auto internalFactory = ObjCToNativeVideoDecoderFactory([[RTCDefaultVideoDecoderFactory alloc] initWithH265: supportsH265 == WebKitH265::On vp9: supportsVP9 == WebKitVP9::On]);
    if (videoDecoderCallbacks().createCallback)
        return std::make_unique<RemoteVideoDecoderFactory>(std::move(internalFactory));
    return internalFactory;
}

void* createLocalH264Decoder(LocalDecoderCallback callback)
{
    auto decoder = [[WK_RTCLocalVideoH264H265Decoder alloc] initH264DecoderWithCallback: callback];
    return (__bridge_retained void*)decoder;
}

void* createLocalH265Decoder(LocalDecoderCallback callback)
{
    auto decoder = [[WK_RTCLocalVideoH264H265Decoder alloc] initH265DecoderWithCallback: callback];
    return (__bridge_retained void*)decoder;
}

void releaseLocalDecoder(LocalDecoder localDecoder)
{
    auto* decoder = (__bridge_transfer WK_RTCLocalVideoH264H265Decoder *)(localDecoder);
    [decoder releaseDecoder];
}

int32_t decodeFrame(LocalDecoder localDecoder, uint32_t timeStamp, const uint8_t* data, size_t size)
{
    auto* decoder = (__bridge WK_RTCLocalVideoH264H265Decoder *)(localDecoder);
    return [decoder decodeData: data size: size timeStamp: timeStamp];
}

}
