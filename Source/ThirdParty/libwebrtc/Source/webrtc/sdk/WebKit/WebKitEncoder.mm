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

#include "WebKitEncoder.h"

#include "WebKitUtilities.h"
#include "api/video/video_frame.h"
#include "components/video_codec/RTCCodecSpecificInfoH264+Private.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "sdk/objc/api/peerconnection/RTCEncodedImage+Private.h"
#include "sdk/objc/api/peerconnection/RTCRtpFragmentationHeader+Private.h"
#include "sdk/objc/api/peerconnection/RTCVideoCodecInfo+Private.h"
#include "sdk/objc/api/peerconnection/RTCVideoEncoderSettings+Private.h"
#include "sdk/objc/components/video_codec/RTCDefaultVideoEncoderFactory.h"
#include "sdk/objc/components/video_codec/RTCVideoEncoderH264.h"
#include "sdk/objc/components/video_codec/RTCVideoEncoderH265.h"
#include "sdk/objc/native/src/objc_frame_buffer.h"
#include "sdk/objc/native/api/video_encoder_factory.h"

@interface WK_RTCLocalVideoH264H265Encoder : NSObject
- (instancetype)initWithCodecInfo:(RTCVideoCodecInfo*)codecInfo;
- (void)setCallback:(RTCVideoEncoderCallback)callback;
- (NSInteger)releaseEncoder;
- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings *)settings numberOfCores:(int)numberOfCores;
- (NSInteger)encode:(RTCVideoFrame *)frame codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info frameTypes:(NSArray<NSNumber *> *)frameTypes;
- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate;
@end

@implementation WK_RTCLocalVideoH264H265Encoder {
    RTCVideoEncoderH264 *m_h264Encoder;
    RTCVideoEncoderH265 *m_h265Encoder;
}

- (instancetype)initWithCodecInfo:(RTCVideoCodecInfo*)codecInfo {
    if (self = [super init]) {
        if ([codecInfo.name isEqualToString:@"H265"])
            m_h265Encoder = [[RTCVideoEncoderH265 alloc] init];
        else
            m_h264Encoder = [[RTCVideoEncoderH264 alloc] init];
    }
    return self;
}

- (void)setCallback:(RTCVideoEncoderCallback)callback {
    if (m_h264Encoder)
        return [m_h264Encoder setCallback:callback];
    return [m_h265Encoder setCallback:callback];
}

- (NSInteger)releaseEncoder {
    if (m_h264Encoder)
        return [m_h264Encoder releaseEncoder];
    return [m_h265Encoder releaseEncoder];
}

- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings *)settings numberOfCores:(int)numberOfCores {
    if (m_h264Encoder)
        return [m_h264Encoder startEncodeWithSettings:settings numberOfCores:numberOfCores];
    return [m_h265Encoder startEncodeWithSettings:settings numberOfCores:numberOfCores];
}

- (NSInteger)encode:(RTCVideoFrame *)frame codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info frameTypes:(NSArray<NSNumber *> *)frameTypes {
    if (m_h264Encoder)
        return [m_h264Encoder encode:frame codecSpecificInfo:info frameTypes:frameTypes];
    return [m_h265Encoder encode:frame codecSpecificInfo:info frameTypes:frameTypes];
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
    if (m_h264Encoder)
        return [m_h264Encoder setBitrate:bitrateKbit framerate:framerate];
    return [m_h265Encoder setBitrate:bitrateKbit framerate:framerate];
}
@end
namespace webrtc {

std::unique_ptr<webrtc::VideoEncoderFactory> createWebKitEncoderFactory(WebKitH265 supportsH265, WebKitVP9 supportsVP9)
{
#if ENABLE_VCP_ENCODER || ENABLE_VCP_VTB_ENCODER
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        webrtc::VPModuleInitialize();
    });
#endif

    auto internalFactory = ObjCToNativeVideoEncoderFactory([[RTCDefaultVideoEncoderFactory alloc] initWithH265: supportsH265 == WebKitH265::On vp9:supportsVP9 == WebKitVP9::On]);
    return std::make_unique<VideoEncoderFactoryWithSimulcast>(std::move(internalFactory));
}

static bool h264HardwareEncoderAllowed = true;
void setH264HardwareEncoderAllowed(bool allowed)
{
    h264HardwareEncoderAllowed = allowed;
}

bool isH264HardwareEncoderAllowed()
{
    return h264HardwareEncoderAllowed;
}

static bool h264LowLatencyEncoderEnabled = false;
void setH264LowLatencyEncoderEnabled(bool enabled)
{
    h264LowLatencyEncoderEnabled = enabled;
}

bool isH264LowLatencyEncoderEnabled()
{
    return h264LowLatencyEncoderEnabled;
}

std::unique_ptr<VideoEncoder> VideoEncoderFactoryWithSimulcast::CreateVideoEncoder(const SdpVideoFormat& format)
{
    return std::make_unique<EncoderSimulcastProxy>(m_internalEncoderFactory.get(), format);
}

struct VideoEncoderCallbacks {
    VideoEncoderCreateCallback createCallback;
    VideoEncoderReleaseCallback releaseCallback;
    VideoEncoderInitializeCallback initializeCallback;
    VideoEncoderEncodeCallback encodeCallback;
    VideoEncoderRegisterEncodeCompleteCallback registerEncodeCompleteCallback;
    VideoEncoderSetRatesCallback setRatesCallback;
};

static VideoEncoderCallbacks& videoEncoderCallbacks() {
    static VideoEncoderCallbacks callbacks;
    return callbacks;
}

void setVideoEncoderCallbacks(VideoEncoderCreateCallback createCallback, VideoEncoderReleaseCallback releaseCallback, VideoEncoderInitializeCallback initializeCallback, VideoEncoderEncodeCallback encodeCallback, VideoEncoderRegisterEncodeCompleteCallback registerEncodeCompleteCallback, VideoEncoderSetRatesCallback setRatesCallback)
{
    auto& callbacks = videoEncoderCallbacks();
    callbacks.createCallback = createCallback;
    callbacks.releaseCallback = releaseCallback;
    callbacks.initializeCallback = initializeCallback;
    callbacks.encodeCallback = encodeCallback;
    callbacks.registerEncodeCompleteCallback = registerEncodeCompleteCallback;
    callbacks.setRatesCallback = setRatesCallback;
}

RemoteVideoEncoder::RemoteVideoEncoder(WebKitVideoEncoder internalEncoder)
    : m_internalEncoder(internalEncoder)
{
}

int32_t RemoteVideoEncoder::InitEncode(const VideoCodec* codec, const Settings&)
{
    if (SimulcastUtility::NumberOfSimulcastStreams(*codec) > 1)
        return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;

    return videoEncoderCallbacks().initializeCallback(m_internalEncoder, *codec);
}

int32_t RemoteVideoEncoder::Release()
{
    return videoEncoderCallbacks().releaseCallback(m_internalEncoder);
}

int32_t RemoteVideoEncoder::Encode(const VideoFrame& frame, const std::vector<VideoFrameType>* frameTypes)
{
    bool shouldEncodeKeyFrame = false;
    for (size_t i = 0; i < frameTypes->size(); ++i) {
        if (frameTypes->at(i) == VideoFrameType::kVideoFrameKey) {
            shouldEncodeKeyFrame = true;
            break;
        }
    }

    return videoEncoderCallbacks().encodeCallback(m_internalEncoder, frame, shouldEncodeKeyFrame);
}

void RemoteVideoEncoder::SetRates(const RateControlParameters& rates)
{
    videoEncoderCallbacks().setRatesCallback(m_internalEncoder, rates);
}

RemoteVideoEncoder::EncoderInfo RemoteVideoEncoder::GetEncoderInfo() const
{
    EncoderInfo info;
    info.supports_native_handle = true;
    info.implementation_name = "RemoteVideoToolBox";
    info.is_hardware_accelerated = true;
    info.has_internal_source = false;

    // Values taken from RTCVideoEncoderH264.mm
    const int kLowH264QpThreshold = 28;
    const int kHighH264QpThreshold = 39;
    info.scaling_settings = ScalingSettings(kLowH264QpThreshold, kHighH264QpThreshold);

    return info;
}

int32_t RemoteVideoEncoder::RegisterEncodeCompleteCallback(EncodedImageCallback* callback)
{
    return videoEncoderCallbacks().registerEncodeCompleteCallback(m_internalEncoder, callback);
}

void RemoteVideoEncoder::encodeComplete(void* callback, uint8_t* buffer, size_t length, const WebKitEncodedFrameInfo& info, const webrtc::RTPFragmentationHeader* header)
{
    webrtc::EncodedImage encodedImage(buffer, length, length);
    encodedImage._encodedWidth = info.width;
    encodedImage._encodedHeight = info.height;
    encodedImage.SetTimestamp(info.timeStamp);
    encodedImage.capture_time_ms_ = info.captureTimeMS;
    encodedImage.ntp_time_ms_ = info.ntpTimeMS;
    encodedImage.timing_ = info.timing;
    encodedImage._frameType = info.frameType;
    encodedImage.rotation_ = info.rotation;
    encodedImage._completeFrame = info.completeFrame;
    encodedImage.qp_ = info.qp;
    encodedImage.content_type_ = info.contentType;

    CodecSpecificInfo codecSpecificInfo;
    codecSpecificInfo.codecType = webrtc::kVideoCodecH264;
    codecSpecificInfo.codecSpecific.H264.packetization_mode = H264PacketizationMode::NonInterleaved;

    static_cast<EncodedImageCallback*>(callback)->OnEncodedImage(encodedImage, &codecSpecificInfo, header);
}

void* createLocalEncoder(const webrtc::SdpVideoFormat& format, LocalEncoderCallback callback)
{
    auto *codecInfo = [[RTCVideoCodecInfo alloc] initWithNativeSdpVideoFormat: format];
    auto *encoder = [[WK_RTCLocalVideoH264H265Encoder alloc] initWithCodecInfo:codecInfo];

    [encoder setCallback:^BOOL(RTCEncodedImage *_Nonnull frame, id<RTCCodecSpecificInfo> _Nonnull codecSpecificInfo,  RTCRtpFragmentationHeader *_Nonnull header) {
        EncodedImage encodedImage = [frame nativeEncodedImage];

        WebKitEncodedFrameInfo info;
        info.width = encodedImage._encodedWidth;
        info.height = encodedImage._encodedHeight;
        info.timeStamp = encodedImage.Timestamp();
        info.ntpTimeMS = encodedImage.ntp_time_ms_;
        info.captureTimeMS = encodedImage.capture_time_ms_;
        info.frameType = encodedImage._frameType;
        info.rotation = encodedImage.rotation_;
        info.contentType = encodedImage.content_type_;
        info.completeFrame = encodedImage._completeFrame;
        info.qp = encodedImage.qp_;
        info.timing = encodedImage.timing_;

        callback(encodedImage.data(), encodedImage.size(), info, [header createNativeFragmentationHeader].get());
        return YES;
    }];

    return (__bridge_retained void*)encoder;

}

void releaseLocalEncoder(LocalEncoder localEncoder)
{
    auto *encoder = (__bridge_transfer WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder releaseEncoder];
}

void initializeLocalEncoder(LocalEncoder localEncoder, uint16_t width, uint16_t height, unsigned int startBitrate, unsigned int maxBitrate, unsigned int minBitrate, uint32_t maxFramerate)
{
    webrtc::VideoCodec codecSettings;
    codecSettings.width = width;
    codecSettings.height = height;
    codecSettings.startBitrate = startBitrate;
    codecSettings.maxBitrate = maxBitrate;
    codecSettings.minBitrate = minBitrate;
    codecSettings.maxFramerate = maxFramerate;

    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder startEncodeWithSettings:[[RTCVideoEncoderSettings alloc] initWithNativeVideoCodec:&codecSettings] numberOfCores:1];
}

void encodeLocalEncoderFrame(LocalEncoder localEncoder, CVPixelBufferRef pixelBuffer, int64_t timeStamp, webrtc::VideoRotation rotation, bool isKeyframeRequired)
{
    NSMutableArray<NSNumber *> *rtcFrameTypes = [NSMutableArray array];
    if (isKeyframeRequired)
        [rtcFrameTypes addObject:@(RTCFrameType(RTCFrameTypeVideoFrameKey))];

    auto *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:ToObjCVideoFrameBuffer(pixelBufferToFrame(pixelBuffer)) rotation:RTCVideoRotation(rotation) timeStampNs:timeStamp];
    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder encode:videoFrame codecSpecificInfo:nil frameTypes:rtcFrameTypes];
}

void setLocalEncoderRates(LocalEncoder localEncoder, uint32_t bitRate, uint32_t frameRate)
{
    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder setBitrate:bitRate framerate:frameRate];
}

}
