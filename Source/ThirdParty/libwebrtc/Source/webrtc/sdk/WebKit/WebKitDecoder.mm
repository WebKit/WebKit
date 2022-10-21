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
#import "api/video_codecs/video_decoder.h"
#import "api/video_codecs/video_decoder_factory.h"
#import "api/video_codecs/video_decoder_software_fallback_wrapper.h"
#import "modules/video_coding/codecs/h264/include/h264.h"
#import "modules/video_coding/include/video_error_codes.h"
#import "sdk/objc/components/video_codec/RTCVideoDecoderH264.h"
#import "sdk/objc/components/video_codec/RTCVideoDecoderH265.h"
#import "sdk/objc/components/video_codec/RTCVideoDecoderVTBVP9.h"
#import "sdk/objc/native/src/objc_frame_buffer.h"

@interface WK_RTCLocalVideoH264H265VP9Decoder : NSObject
- (instancetype)initH264DecoderWithCallback:(webrtc::LocalDecoderCallback)callback;
- (instancetype)initH265DecoderWithCallback:(webrtc::LocalDecoderCallback)callback;
- (NSInteger)setFormat:(const uint8_t *)data size:(size_t)size width:(uint16_t)width height:(uint16_t)height;
- (NSInteger)decodeData:(const uint8_t *)data size:(size_t)size timeStamp:(int64_t)timeStamp;
- (NSInteger)releaseDecoder;
- (void)flush;
@end

@implementation WK_RTCLocalVideoH264H265VP9Decoder {
    RTCVideoDecoderH264 *m_h264Decoder;
    RTCVideoDecoderH265 *m_h265Decoder;
    RTCVideoDecoderVTBVP9 *m_vp9Decoder;
}

- (instancetype)initH264DecoderWithCallback:(webrtc::LocalDecoderCallback)callback {
    if (self = [super init]) {
        m_h264Decoder = [[RTCVideoDecoderH264 alloc] init];
        [m_h264Decoder setCallback:^(RTCVideoFrame *frame) {
            if (!frame) {
              callback(nil, 0, 0);
              return;
            }
            auto *buffer = (RTCCVPixelBuffer *)frame.buffer;
            callback(buffer.pixelBuffer, frame.timeStamp, frame.timeStampNs);
        }];
    }
    return self;
}

- (instancetype)initH265DecoderWithCallback:(webrtc::LocalDecoderCallback)callback {
    if (self = [super init]) {
        m_h265Decoder = [[RTCVideoDecoderH265 alloc] init];
        [m_h265Decoder setCallback:^(RTCVideoFrame *frame) {
            if (!frame) {
              callback(nil, 0, 0);
              return;
            }
            auto *buffer = (RTCCVPixelBuffer *)frame.buffer;
            callback(buffer.pixelBuffer, frame.timeStamp, frame.timeStampNs);
        }];
    }
    return self;
}

- (instancetype)initVP9DecoderWithCallback:(webrtc::LocalDecoderCallback)callback {
    if (self = [super init]) {
        m_vp9Decoder = [[RTCVideoDecoderVTBVP9 alloc] init];
        [m_vp9Decoder setCallback:^(RTCVideoFrame *frame) {
            if (!frame) {
              callback(nil, 0, 0);
              return;
            }
            auto *buffer = (RTCCVPixelBuffer *)frame.buffer;
            callback(buffer.pixelBuffer, frame.timeStamp, frame.timeStampNs);
        }];
    }
    return self;
}

- (NSInteger)setFormat:(const uint8_t *)data size:(size_t)size width:(uint16_t)width height:(uint16_t)height {
    if (m_h264Decoder)
        return [m_h264Decoder setAVCFormat:data size:size width:width height:height];
    if (m_h265Decoder)
        return [m_h265Decoder setAVCFormat:data size:size width:width height:height];
    return 0;
}

- (NSInteger)decodeData:(const uint8_t *)data size:(size_t)size timeStamp:(int64_t)timeStamp {
    if (m_h264Decoder)
        return [m_h264Decoder decodeData:data size:size timeStamp:timeStamp];
    if (m_h265Decoder)
        return [m_h265Decoder decodeData:data size:size timeStamp:timeStamp];
    if (m_vp9Decoder)
        return [m_vp9Decoder decodeData:data size:size timeStamp:timeStamp];
    return 0;
}

- (void)setWidth:(uint16_t)width height:(uint16_t)height {
    if (!m_vp9Decoder)
        return;
    [m_vp9Decoder setWidth:width height:height];
}

- (NSInteger)releaseDecoder {
    if (m_h264Decoder)
        return [m_h264Decoder releaseDecoder];
    if (m_h265Decoder)
        return [m_h265Decoder releaseDecoder];
    return [m_vp9Decoder releaseDecoder];
}

- (void)flush {
    if (m_h264Decoder) {
        [m_h264Decoder flush];
        return;
    }
    if (m_h265Decoder) {
        [m_h265Decoder flush];
        return;
    }
    [m_vp9Decoder flush];
}

@end

namespace webrtc {

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
    RemoteVideoDecoder(WebKitVideoDecoder, bool isVP9);
    ~RemoteVideoDecoder();

    bool isVP9() const { return m_isVP9; }

private:
    bool Configure(const Settings&) final;
    int32_t Decode(const EncodedImage&, bool missing_frames, int64_t render_time_ms) final;
    int32_t RegisterDecodeCompleteCallback(DecodedImageCallback*) final;
    int32_t Release() final;
    const char* ImplementationName() const final { return "RemoteVideoToolBox"; }

    WebKitVideoDecoder m_internalDecoder;
    bool m_isVP9 { false };
};

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

RemoteVideoDecoder::RemoteVideoDecoder(WebKitVideoDecoder internalDecoder, bool isVP9)
    : m_internalDecoder(internalDecoder)
    , m_isVP9(isVP9)
{
}

RemoteVideoDecoder::~RemoteVideoDecoder()
{
    videoDecoderCallbacks().releaseCallback(m_internalDecoder);
}

void videoDecoderTaskComplete(void* callback, uint32_t timeStamp, uint32_t timeStampRTP, CVPixelBufferRef pixelBuffer)
{
    auto videoFrame = VideoFrame::Builder().set_video_frame_buffer(pixelBufferToFrame(pixelBuffer))
        .set_timestamp_rtp(timeStampRTP)
        .set_timestamp_ms(0)
        .set_rotation((VideoRotation)RTCVideoRotation_0)
        .build();
    videoFrame.set_timestamp(timeStamp);

    static_cast<DecodedImageCallback*>(callback)->Decoded(videoFrame);
}

void videoDecoderTaskComplete(void* callback, uint32_t timeStamp, uint32_t timeStampRTP, void* pointer, GetBufferCallback getBufferCallback, ReleaseBufferCallback releaseBufferCallback, int width, int height)
{
    auto videoFrame = VideoFrame::Builder().set_video_frame_buffer(toWebRTCVideoFrameBuffer(pointer, getBufferCallback, releaseBufferCallback, width, height))
        .set_timestamp_rtp(timeStampRTP)
        .set_timestamp_ms(0)
        .set_rotation((VideoRotation)RTCVideoRotation_0)
        .build();
    videoFrame.set_timestamp(timeStamp);

    static_cast<DecodedImageCallback*>(callback)->Decoded(videoFrame);
}

bool RemoteVideoDecoder::Configure(const Settings&)
{
    return true;
}

int32_t RemoteVideoDecoder::Decode(const EncodedImage& input_image, bool missing_frames, int64_t render_time_ms)
{
    uint16_t encodedWidth = 0;
    uint16_t encodedHeight = 0;
    if (input_image._frameType == VideoFrameType::kVideoFrameKey) {
        encodedWidth = rtc::dchecked_cast<uint16_t>(input_image._encodedWidth);
        encodedHeight = rtc::dchecked_cast<uint16_t>(input_image._encodedHeight);
    }

    // VP9 VTB does not support SVC
    if (m_isVP9 && input_image._frameType == VideoFrameType::kVideoFrameKey && input_image.SpatialIndex() && *input_image.SpatialIndex() > 0)
        return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;

    return videoDecoderCallbacks().decodeCallback(m_internalDecoder, input_image.Timestamp(), input_image.data(), input_image.size(), encodedWidth, encodedHeight);
}

int32_t RemoteVideoDecoder::RegisterDecodeCompleteCallback(DecodedImageCallback* callback)
{
    return videoDecoderCallbacks().registerDecodeCompleteCallback(m_internalDecoder, callback);
}

int32_t RemoteVideoDecoder::Release()
{
    RegisterDecodeCompleteCallback(nullptr);
    return 0;
}

RemoteVideoDecoderFactory::RemoteVideoDecoderFactory(std::unique_ptr<VideoDecoderFactory>&& internalFactory)
    : m_internalFactory(std::move(internalFactory))
{
}

std::vector<SdpVideoFormat> RemoteVideoDecoderFactory::GetSupportedFormats() const
{
    return m_internalFactory->GetSupportedFormats();
}

std::unique_ptr<VideoDecoder> RemoteVideoDecoderFactory::CreateVideoDecoder(const SdpVideoFormat& format)
{
    if (!videoDecoderCallbacks().createCallback)
        return m_internalFactory->CreateVideoDecoder(format);

    auto identifier = videoDecoderCallbacks().createCallback(format);
    if (!identifier)
        return m_internalFactory->CreateVideoDecoder(format);

    auto decoder = std::make_unique<RemoteVideoDecoder>(identifier, format.name == "VP9");
    if (!decoder->isVP9())
        return decoder;
    return webrtc::CreateVideoDecoderSoftwareFallbackWrapper(m_internalFactory->CreateVideoDecoder(format), std::move(decoder));
}

std::unique_ptr<webrtc::VideoDecoderFactory> createWebKitDecoderFactory(WebKitH265 supportsH265, WebKitVP9 supportsVP9, WebKitVP9VTB supportsVP9VTB)
{
    auto internalFactory = ObjCToNativeVideoDecoderFactory([[RTCDefaultVideoDecoderFactory alloc] initWithH265: supportsH265 == WebKitH265::On vp9Profile0:supportsVP9 > WebKitVP9::Off vp9Profile2:supportsVP9 == WebKitVP9::Profile0And2 vp9VTB: supportsVP9VTB == WebKitVP9VTB::On]);
    return std::make_unique<RemoteVideoDecoderFactory>(std::move(internalFactory));
}

void* createLocalH264Decoder(LocalDecoderCallback callback)
{
    auto decoder = [[WK_RTCLocalVideoH264H265VP9Decoder alloc] initH264DecoderWithCallback: callback];
    return (__bridge_retained void*)decoder;
}

void* createLocalH265Decoder(LocalDecoderCallback callback)
{
    auto decoder = [[WK_RTCLocalVideoH264H265VP9Decoder alloc] initH265DecoderWithCallback: callback];
    return (__bridge_retained void*)decoder;
}

void* createLocalVP9Decoder(LocalDecoderCallback callback)
{
    auto decoder = [[WK_RTCLocalVideoH264H265VP9Decoder alloc] initVP9DecoderWithCallback: callback];
    return (__bridge_retained void*)decoder;
}

void releaseLocalDecoder(LocalDecoder localDecoder)
{
    auto* decoder = (__bridge_transfer WK_RTCLocalVideoH264H265VP9Decoder *)(localDecoder);
    [decoder releaseDecoder];
}

void flushLocalDecoder(LocalDecoder localDecoder)
{
    auto* decoder = (__bridge WK_RTCLocalVideoH264H265VP9Decoder *)(localDecoder);
    [decoder flush];
}

int32_t setDecodingFormat(LocalDecoder localDecoder, const uint8_t* data, size_t size, uint16_t width, uint16_t height)
{
    auto* decoder = (__bridge WK_RTCLocalVideoH264H265VP9Decoder *)(localDecoder);
    return [decoder setFormat:data size:size width:width height:height];
}

int32_t decodeFrame(LocalDecoder localDecoder, int64_t timeStamp, const uint8_t* data, size_t size)
{
    auto* decoder = (__bridge WK_RTCLocalVideoH264H265VP9Decoder *)(localDecoder);
    return [decoder decodeData:data size:size timeStamp:timeStamp];
}

void setDecoderFrameSize(LocalDecoder localDecoder, uint16_t width, uint16_t height)
{
    auto* decoder = (__bridge WK_RTCLocalVideoH264H265VP9Decoder *)(localDecoder);
    [decoder setWidth:width height:height];
}

}
