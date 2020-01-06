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

#include "WebKitUtilities.h"

//#include "Common/RTCUIApplicationStatusObserver.h"
#import "WebRTC/RTCVideoCodecH264.h"

#include "api/video/video_frame.h"
#include "helpers/scoped_cftyperef.h"
#include "native/src/objc_frame_buffer.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "webrtc/rtc_base/checks.h"
#include "Framework/Headers/WebRTC/RTCVideoCodecFactory.h"
#include "Framework/Headers/WebRTC/RTCVideoFrame.h"
#include "Framework/Headers/WebRTC/RTCVideoFrameBuffer.h"
#include "Framework/Native/api/video_decoder_factory.h"
#include "Framework/Native/api/video_encoder_factory.h"
#include "VideoProcessingSoftLink.h"
#include "WebKitEncoder.h"

#include <mutex>

/*
#if !defined(WEBRTC_IOS)
__attribute__((objc_runtime_name("WK_RTCUIApplicationStatusObserver")))
@interface RTCUIApplicationStatusObserver : NSObject

+ (instancetype)sharedInstance;
+ (void)prepareForUse;

- (BOOL)isApplicationActive;

@end
#endif

@implementation RTCUIApplicationStatusObserver {
    BOOL _isActive;
}

+ (instancetype)sharedInstance {
    static id sharedInstance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });

    return sharedInstance;
}

+ (void)prepareForUse {
    __unused RTCUIApplicationStatusObserver *observer = [self sharedInstance];
}

- (id)init {
    _isActive = YES;
    return self;
}

- (void)setActive {
    _isActive = YES;
}

- (void)setInactive {
    _isActive = NO;
}

- (BOOL)isApplicationActive {
    return _isActive;
}

@end
*/
namespace webrtc {

void setApplicationStatus(bool isActive)
{
/*
    if (isActive)
        [[RTCUIApplicationStatusObserver sharedInstance] setActive];
    else
        [[RTCUIApplicationStatusObserver sharedInstance] setInactive];
 */
}

std::unique_ptr<webrtc::VideoEncoderFactory> createWebKitEncoderFactory(WebKitCodecSupport codecSupport)
{
#if ENABLE_VCP_ENCODER || ENABLE_VCP_VTB_ENCODER
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        webrtc::VPModuleInitialize();
    });
#endif

    auto internalFactory = ObjCToNativeVideoEncoderFactory(codecSupport == WebKitCodecSupport::H264AndVP8 ? [[RTCDefaultVideoEncoderFactory alloc] init] : [[RTCVideoEncoderFactoryH264 alloc] init]);
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

rtc::scoped_refptr<webrtc::VideoFrameBuffer> pixelBufferToFrame(CVPixelBufferRef pixelBuffer)
{
    RTCCVPixelBuffer *frameBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBuffer];
    return new rtc::RefCountedObject<ObjCFrameBuffer>(frameBuffer);
}

static bool CopyVideoFrameToPixelBuffer(const webrtc::I420BufferInterface* frame, CVPixelBufferRef pixel_buffer) {
    RTC_DCHECK(pixel_buffer);
    RTC_DCHECK(CVPixelBufferGetPixelFormatType(pixel_buffer) == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || CVPixelBufferGetPixelFormatType(pixel_buffer) == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
    RTC_DCHECK_EQ(CVPixelBufferGetHeightOfPlane(pixel_buffer, 0), static_cast<size_t>(frame->height()));
    RTC_DCHECK_EQ(CVPixelBufferGetWidthOfPlane(pixel_buffer, 0), static_cast<size_t>(frame->width()));

    if (CVPixelBufferLockBaseAddress(pixel_buffer, 0) != kCVReturnSuccess)
        return false;

    uint8_t* dst_y = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
    int dst_stride_y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);

    uint8_t* dst_uv = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
    int dst_stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);

    int result = libyuv::I420ToNV12(
        frame->DataY(), frame->StrideY(),
        frame->DataU(), frame->StrideU(),
        frame->DataV(), frame->StrideV(),
        dst_y, dst_stride_y, dst_uv, dst_stride_uv,
        frame->width(), frame->height());

    CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);

    if (result)
        return false;

    return true;
}

CVPixelBufferRef pixelBufferFromFrame(const VideoFrame& frame, const std::function<CVPixelBufferRef(size_t, size_t)>& makePixelBuffer)
{
    if (frame.video_frame_buffer()->type() != VideoFrameBuffer::Type::kNative) {
        rtc::scoped_refptr<const I420BufferInterface> buffer = frame.video_frame_buffer()->GetI420();

        auto pixelBuffer = makePixelBuffer(buffer->width(), buffer->height());
        if (pixelBuffer)
            CopyVideoFrameToPixelBuffer(frame.video_frame_buffer()->GetI420(), pixelBuffer);
        return pixelBuffer;
    }

    auto *frameBuffer = static_cast<ObjCFrameBuffer*>(frame.video_frame_buffer().get())->wrapped_frame_buffer();
    if (![frameBuffer isKindOfClass:[RTCCVPixelBuffer class]])
        return nullptr;

    auto *rtcPixelBuffer = (RTCCVPixelBuffer *)frameBuffer;
    return rtcPixelBuffer.pixelBuffer;
}

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

std::unique_ptr<webrtc::VideoDecoderFactory> createWebKitDecoderFactory(WebKitCodecSupport codecSupport)
{
    auto internalFactory = ObjCToNativeVideoDecoderFactory(codecSupport == WebKitCodecSupport::H264AndVP8 ? [[RTCDefaultVideoDecoderFactory alloc] init] : [[RTCVideoDecoderFactoryH264 alloc] init]);
    if (videoDecoderCallbacks().createCallback)
        return std::make_unique<RemoteVideoDecoderFactory>(std::move(internalFactory));
    return internalFactory;
}

void* createLocalDecoder(LocalDecoderCallback callback)
{
    auto decoder = [[RTCVideoDecoderH264 alloc] init];
    [decoder setCallback:^(RTCVideoFrame *frame) {
        auto *buffer = (RTCCVPixelBuffer *)frame.buffer;
        callback(buffer.pixelBuffer, frame.timeStampNs, frame.timeStamp);
    }];
    return (__bridge_retained void*)decoder;
}

void releaseLocalDecoder(LocalDecoder localDecoder)
{
    RTCVideoDecoderH264* decoder = (__bridge_transfer RTCVideoDecoderH264 *)(localDecoder);
    [decoder releaseDecoder];
}

int32_t decodeFrame(LocalDecoder localDecoder, uint32_t timeStamp, const uint8_t* data, size_t size)
{
    RTCVideoDecoderH264* decoder = (__bridge RTCVideoDecoderH264 *)(localDecoder);
    return [decoder decodeData: data size: size timeStamp: timeStamp];
}

}
