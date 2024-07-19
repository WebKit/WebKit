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
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "sdk/objc/api/peerconnection/RTCEncodedImage+Private.h"
#include "sdk/objc/api/peerconnection/RTCVideoCodecInfo+Private.h"
#include "sdk/objc/api/peerconnection/RTCVideoEncoderSettings+Private.h"
#include "sdk/objc/components/video_codec/RTCDefaultVideoEncoderFactory.h"
#include "sdk/objc/components/video_codec/RTCVideoEncoderH264.h"
#include "sdk/objc/components/video_codec/RTCVideoEncoderH265.h"
#include "sdk/objc/native/src/objc_frame_buffer.h"
#include "sdk/objc/native/api/video_encoder_factory.h"

@interface WK_RTCLocalVideoH264H265Encoder : NSObject
- (instancetype)initWithCodecInfo:(RTCVideoCodecInfo*)codecInfo scalabilityMode:(webrtc::LocalEncoderScalabilityMode)scalabilityMode;
- (webrtc::VideoCodecType)codecType;
- (void)setCallback:(RTCVideoEncoderCallback)callback;
- (NSInteger)releaseEncoder;
- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings *)settings numberOfCores:(int)numberOfCores;
- (NSInteger)encode:(RTCVideoFrame *)frame codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)info frameTypes:(NSArray<NSNumber *> *)frameTypes;
- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate;
- (void)setLowLatency:(bool)lowLatencyEnabled;
- (void)setUseAnnexB:(bool)useAnnexB;
- (void)setDescriptionCallback:(RTCVideoEncoderDescriptionCallback)callback;
- (void)setErrorCallback:(RTCVideoEncoderErrorCallback)callback;
- (void)flush;
@end

@implementation WK_RTCLocalVideoH264H265Encoder {
    RTCVideoEncoderH264 *m_h264Encoder;
    RTCVideoEncoderH265 *m_h265Encoder;
}

- (instancetype)initWithCodecInfo:(RTCVideoCodecInfo*)codecInfo scalabilityMode:(webrtc::LocalEncoderScalabilityMode)scalabilityMode {
    if (self = [super init]) {
        if ([codecInfo.name isEqualToString:@"H265"])
            m_h265Encoder = [[RTCVideoEncoderH265 alloc] initWithCodecInfo:codecInfo];
        else {
            m_h264Encoder = [[RTCVideoEncoderH264 alloc] initWithCodecInfo:codecInfo];
            if (scalabilityMode == webrtc::LocalEncoderScalabilityMode::L1T2)
                [m_h264Encoder enableL1T2ScalabilityMode];
            if (!m_h264Encoder)
                return nil;
        }
    }
    return self;
}

- (webrtc::VideoCodecType)codecType
{
    if (m_h264Encoder)
        return webrtc::kVideoCodecH264;
    return webrtc::kVideoCodecH265;
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
- (void)setLowLatency:(bool)lowLatencyEnabled {
    if (m_h264Encoder)
        [m_h264Encoder setH264LowLatencyEncoderEnabled:lowLatencyEnabled];
    [m_h265Encoder setLowLatency:lowLatencyEnabled];
}

- (void)setUseAnnexB:(bool)useAnnexB {
    if (m_h264Encoder) {
        [m_h264Encoder setUseAnnexB:useAnnexB];
        return;
    }
    [m_h265Encoder setUseAnnexB:useAnnexB];
}

- (void)setDescriptionCallback:(RTCVideoEncoderDescriptionCallback)callback {
    if (m_h264Encoder) {
        [m_h264Encoder setDescriptionCallback:callback];
        return;
    }
    [m_h265Encoder setDescriptionCallback:callback];
}

- (void)setErrorCallback:(RTCVideoEncoderErrorCallback)callback {
    if (m_h264Encoder) {
        [m_h264Encoder setErrorCallback:callback];
        return;
    }
    [m_h265Encoder setErrorCallback:callback];
}

- (void)flush {
    if (m_h264Encoder) {
        [m_h264Encoder flush];
        return;
    }
    [m_h265Encoder flush];
}
@end

namespace webrtc {

class VideoEncoderFactoryWithSimulcast final : public VideoEncoderFactory {
public:
    explicit VideoEncoderFactoryWithSimulcast(std::unique_ptr<VideoEncoderFactory>&& factory)
        : m_internalEncoderFactory(std::move(factory))
    {
    }

    std::unique_ptr<VideoEncoder> Create(const Environment&, const SdpVideoFormat& format) final;
    std::vector<SdpVideoFormat> GetSupportedFormats() const final { return m_internalEncoderFactory->GetSupportedFormats(); }

private:
    const std::unique_ptr<VideoEncoderFactory> m_internalEncoderFactory;
};

static bool h264HardwareEncoderAllowed = true;
void setH264HardwareEncoderAllowed(bool allowed)
{
    h264HardwareEncoderAllowed = allowed;
}

bool isH264HardwareEncoderAllowed()
{
    return h264HardwareEncoderAllowed;
}

std::unique_ptr<VideoEncoder> VideoEncoderFactoryWithSimulcast::Create(const webrtc::Environment& environment, const SdpVideoFormat& format)
{
    return std::make_unique<SimulcastEncoderAdapter>(environment, m_internalEncoderFactory.get(), nullptr, format);
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

class RemoteVideoEncoder final : public webrtc::VideoEncoder {
public:
    explicit RemoteVideoEncoder(WebKitVideoEncoder);
    ~RemoteVideoEncoder();

private:
    int32_t InitEncode(const VideoCodec*, const Settings&) final;
    int32_t RegisterEncodeCompleteCallback(EncodedImageCallback*) final;
    int32_t Release() final;
    int32_t Encode(const VideoFrame&, const std::vector<VideoFrameType>*) final;
    void SetRates(const RateControlParameters&) final;
    EncoderInfo GetEncoderInfo() const final;

    WebKitVideoEncoder m_internalEncoder;
};

class RemoteVideoEncoderFactory final : public VideoEncoderFactory {
public:
    explicit RemoteVideoEncoderFactory(std::unique_ptr<VideoEncoderFactory>&&);
    ~RemoteVideoEncoderFactory() = default;

private:
    std::unique_ptr<VideoEncoder> Create(const Environment&, const SdpVideoFormat& format) final;
    std::vector<SdpVideoFormat> GetSupportedFormats() const final { return m_internalFactory->GetSupportedFormats(); }

    std::unique_ptr<VideoEncoderFactory> m_internalFactory;
};

RemoteVideoEncoderFactory::RemoteVideoEncoderFactory(std::unique_ptr<VideoEncoderFactory>&& factory)
    : m_internalFactory(std::move(factory))
{
}

std::unique_ptr<VideoEncoder> RemoteVideoEncoderFactory::Create(const Environment& environment, const SdpVideoFormat& format)
{
    if (!videoEncoderCallbacks().createCallback)
        return m_internalFactory->Create(environment, format);

    auto internalEncoder = videoEncoderCallbacks().createCallback(format);
    if (!internalEncoder)
        return m_internalFactory->Create(environment, format);

    return std::make_unique<RemoteVideoEncoder>(internalEncoder);
}

std::unique_ptr<webrtc::VideoEncoderFactory> createWebKitEncoderFactory(WebKitH265 supportsH265, WebKitVP9 supportsVP9, WebKitAv1 supportsAv1)
{
    auto internalFactory = ObjCToNativeVideoEncoderFactory([[RTCDefaultVideoEncoderFactory alloc] initWithH265: supportsH265 == WebKitH265::On vp9Profile0:supportsVP9 > WebKitVP9::Off vp9Profile2:supportsVP9 == WebKitVP9::Profile0And2 av1:supportsAv1 == WebKitAv1::On]);

    return std::make_unique<VideoEncoderFactoryWithSimulcast>(std::make_unique<RemoteVideoEncoderFactory>(std::move(internalFactory)));
}

RemoteVideoEncoder::RemoteVideoEncoder(WebKitVideoEncoder internalEncoder)
    : m_internalEncoder(internalEncoder)
{
}

RemoteVideoEncoder::~RemoteVideoEncoder()
{
    videoEncoderCallbacks().releaseCallback(m_internalEncoder);
}

int32_t RemoteVideoEncoder::InitEncode(const VideoCodec* codec, const Settings&)
{
    if (SimulcastUtility::NumberOfSimulcastStreams(*codec) > 1)
        return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;

    return videoEncoderCallbacks().initializeCallback(m_internalEncoder, *codec);
}

int32_t RemoteVideoEncoder::Release()
{
    RegisterEncodeCompleteCallback(nullptr);
    return 0;
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

static inline VideoRotation fromEncoded(WebKitEncodedVideoRotation encoded)
{
    switch (encoded) {
    case WebKitEncodedVideoRotation::kVideoRotation_0:
        return VideoRotation::kVideoRotation_0;
    case WebKitEncodedVideoRotation::kVideoRotation_90:
        return VideoRotation::kVideoRotation_90;
    case WebKitEncodedVideoRotation::kVideoRotation_180:
        return VideoRotation::kVideoRotation_180;
    case WebKitEncodedVideoRotation::kVideoRotation_270:
        return VideoRotation::kVideoRotation_270;
    }
}

static inline WebKitEncodedVideoRotation toEncoded(VideoRotation rotation)
{
    switch (rotation) {
    case VideoRotation::kVideoRotation_0:
        return WebKitEncodedVideoRotation::kVideoRotation_0;
    case VideoRotation::kVideoRotation_90:
        return WebKitEncodedVideoRotation::kVideoRotation_90;
    case VideoRotation::kVideoRotation_180:
        return WebKitEncodedVideoRotation::kVideoRotation_180;
    case VideoRotation::kVideoRotation_270:
        return WebKitEncodedVideoRotation::kVideoRotation_270;
    }
}

void encoderVideoTaskComplete(void* callback, webrtc::VideoCodecType codecType, const uint8_t* buffer, size_t length, const WebKitEncodedFrameInfo& info)
{
    webrtc::EncodedImage encodedImage;
    encodedImage.SetEncodedData(EncodedImageBuffer::Create(buffer, length));

    encodedImage._encodedWidth = info.width;
    encodedImage._encodedHeight = info.height;
    encodedImage.SetRtpTimestamp(info.timeStamp);
    encodedImage.capture_time_ms_ = info.captureTimeMS;
    encodedImage.ntp_time_ms_ = info.ntpTimeMS;
    encodedImage.timing_ = info.timing;
    encodedImage._frameType = info.frameType;
    encodedImage.rotation_ = fromEncoded(info.rotation);
    encodedImage.qp_ = info.qp;
    encodedImage.content_type_ = info.contentType;

    CodecSpecificInfo codecSpecificInfo;
    codecSpecificInfo.codecType = codecType;
    if (codecType == kVideoCodecH264 || codecType == kVideoCodecH265)
        codecSpecificInfo.codecSpecific.H264.packetization_mode = H264PacketizationMode::NonInterleaved;

    static_cast<EncodedImageCallback*>(callback)->OnEncodedImage(encodedImage, &codecSpecificInfo);
}

void* createLocalEncoder(const webrtc::SdpVideoFormat& format, bool useAnnexB, webrtc::LocalEncoderScalabilityMode scalabilityMode, LocalEncoderCallback frameCallback, LocalEncoderDescriptionCallback descriptionCallback, LocalEncoderErrorCallback errorCallback)
{
    auto *codecInfo = [[RTCVideoCodecInfo alloc] initWithNativeSdpVideoFormat: format];
    auto *encoder = [[WK_RTCLocalVideoH264H265Encoder alloc] initWithCodecInfo:codecInfo scalabilityMode:scalabilityMode];

    if (!encoder)
        return nullptr;

    [encoder setCallback:^BOOL(RTCEncodedImage *_Nonnull frame, id<RTCCodecSpecificInfo> _Nonnull codecSpecificInfo, RTCRtpFragmentationHeader * _Nullable header) {
        EncodedImage encodedImage = [frame nativeEncodedImage];

        WebKitEncodedFrameInfo info;
        info.width = encodedImage._encodedWidth;
        info.height = encodedImage._encodedHeight;
        info.timeStamp = [frame timeStamp];
        if ([frame duration] != std::numeric_limits<uint64_t>::max())
            info.duration = [frame duration];
        info.ntpTimeMS = encodedImage.ntp_time_ms_;
        info.captureTimeMS = encodedImage.capture_time_ms_;
        info.frameType = encodedImage._frameType;
        info.rotation = toEncoded(encodedImage.rotation_);
        info.contentType = encodedImage.content_type_;
        info.qp = encodedImage.qp_;
        info.timing = encodedImage.timing_;
        info.temporalIndex = frame.temporalIndex;

        frameCallback(encodedImage.data(), encodedImage.size(), info);
        return YES;
    }];

    [encoder setUseAnnexB:useAnnexB];
    [encoder setDescriptionCallback:descriptionCallback];
    [encoder setErrorCallback:^(OSStatus result) {
        errorCallback(result == noErr);
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
    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);

    webrtc::VideoCodec codecSettings;
    codecSettings.width = width;
    codecSettings.height = height;
    codecSettings.startBitrate = startBitrate;
    codecSettings.maxBitrate = maxBitrate;
    codecSettings.minBitrate = minBitrate;
    codecSettings.maxFramerate = maxFramerate;
    codecSettings.codecType = encoder.codecType;

    [encoder startEncodeWithSettings:[[RTCVideoEncoderSettings alloc] initWithNativeVideoCodec:&codecSettings] numberOfCores:1];
}

void encodeLocalEncoderFrame(LocalEncoder localEncoder, CVPixelBufferRef pixelBuffer, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, webrtc::VideoRotation rotation, bool isKeyframeRequired)
{
    NSMutableArray<NSNumber *> *rtcFrameTypes = [NSMutableArray array];
    if (isKeyframeRequired)
        [rtcFrameTypes addObject:@(RTCFrameType(RTCFrameTypeVideoFrameKey))];

    auto videoFrameBuffer = pixelBufferToFrame(pixelBuffer);
    auto *videoFrame = [[RTCVideoFrame alloc] initWithBuffer:ToObjCVideoFrameBuffer(videoFrameBuffer) rotation:RTCVideoRotation(rotation) timeStampNs:timeStampNs];
    videoFrame.timeStamp = timeStamp;
    videoFrame.duration = duration.value_or(std::numeric_limits<uint64_t>::max());
    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder encode:videoFrame codecSpecificInfo:nil frameTypes:rtcFrameTypes];
}

void setLocalEncoderRates(LocalEncoder localEncoder, uint32_t bitRate, uint32_t frameRate)
{
    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder setBitrate:bitRate framerate:frameRate];
}

void setLocalEncoderLowLatency(LocalEncoder localEncoder, bool isLowLatencyEnabled)
{
    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder setLowLatency:isLowLatencyEnabled];
}

void flushLocalEncoder(LocalEncoder localEncoder)
{
    auto *encoder = (__bridge WK_RTCLocalVideoH264H265Encoder *)(localEncoder);
    [encoder flush];
}

}
