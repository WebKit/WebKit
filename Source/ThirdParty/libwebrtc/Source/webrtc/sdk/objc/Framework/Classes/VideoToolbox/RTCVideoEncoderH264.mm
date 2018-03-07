/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#import "WebRTC/RTCVideoCodecH264.h"

#import <VideoToolbox/VideoToolbox.h>
#include <vector>

#if defined(WEBRTC_IOS)
#import "Common/RTCUIApplicationStatusObserver.h"
#import "WebRTC/UIDevice+RTCDevice.h"
#endif
#import "PeerConnection/RTCVideoCodec+Private.h"
#import "WebRTC/RTCVideoCodec.h"
#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"
#import "helpers.h"
#include "libyuv/convert_from.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/profile_level_id.h"
#include "common_video/include/bitrate_adjuster.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "sdk/objc/Framework/Classes/VideoToolbox/nalu_rewriter.h"
#include "system_wrappers/include/clock.h"

@interface RTCVideoEncoderH264 ()

- (void)frameWasEncoded:(OSStatus)status
                  flags:(VTEncodeInfoFlags)infoFlags
           sampleBuffer:(CMSampleBufferRef)sampleBuffer
      codecSpecificInfo:(id<RTCCodecSpecificInfo>)codecSpecificInfo
                  width:(int32_t)width
                 height:(int32_t)height
           renderTimeMs:(int64_t)renderTimeMs
              timestamp:(uint32_t)timestamp
               rotation:(RTCVideoRotation)rotation;

@end

namespace {  // anonymous namespace

// The ratio between kVTCompressionPropertyKey_DataRateLimits and
// kVTCompressionPropertyKey_AverageBitRate. The data rate limit is set higher
// than the average bit rate to avoid undershooting the target.
const float kLimitToAverageBitRateFactor = 1.5f;
// These thresholds deviate from the default h264 QP thresholds, as they
// have been found to work better on devices that support VideoToolbox
const int kLowH264QpThreshold = 28;
const int kHighH264QpThreshold = 39;

const OSType kNV12PixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;

// Struct that we pass to the encoder per frame to encode. We receive it again
// in the encoder callback.
struct RTCFrameEncodeParams {
  RTCFrameEncodeParams(RTCVideoEncoderH264 *e,
                       RTCCodecSpecificInfoH264 *csi,
                       int32_t w,
                       int32_t h,
                       int64_t rtms,
                       uint32_t ts,
                       RTCVideoRotation r)
      : encoder(e), width(w), height(h), render_time_ms(rtms), timestamp(ts), rotation(r) {
    if (csi) {
      codecSpecificInfo = csi;
    } else {
      codecSpecificInfo = [[RTCCodecSpecificInfoH264 alloc] init];
    }
  }

  RTCVideoEncoderH264 *encoder;
  RTCCodecSpecificInfoH264 *codecSpecificInfo;
  int32_t width;
  int32_t height;
  int64_t render_time_ms;
  uint32_t timestamp;
  RTCVideoRotation rotation;
};

// We receive I420Frames as input, but we need to feed CVPixelBuffers into the
// encoder. This performs the copy and format conversion.
// TODO(tkchin): See if encoder will accept i420 frames and compare performance.
bool CopyVideoFrameToNV12PixelBuffer(id<RTCI420Buffer> frameBuffer, CVPixelBufferRef pixelBuffer) {
  RTC_DCHECK(pixelBuffer);
  RTC_DCHECK_EQ(CVPixelBufferGetPixelFormatType(pixelBuffer), kNV12PixelFormat);
  RTC_DCHECK_EQ(CVPixelBufferGetHeightOfPlane(pixelBuffer, 0), frameBuffer.height);
  RTC_DCHECK_EQ(CVPixelBufferGetWidthOfPlane(pixelBuffer, 0), frameBuffer.width);

  CVReturn cvRet = CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  if (cvRet != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock base address: " << cvRet;
    return false;
  }
  uint8_t *dstY = reinterpret_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
  int dstStrideY = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  uint8_t *dstUV = reinterpret_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
  int dstStrideUV = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
  // Convert I420 to NV12.
  int ret = libyuv::I420ToNV12(frameBuffer.dataY,
                               frameBuffer.strideY,
                               frameBuffer.dataU,
                               frameBuffer.strideU,
                               frameBuffer.dataV,
                               frameBuffer.strideV,
                               dstY,
                               dstStrideY,
                               dstUV,
                               dstStrideUV,
                               frameBuffer.width,
                               frameBuffer.height);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  if (ret) {
    RTC_LOG(LS_ERROR) << "Error converting I420 VideoFrame to NV12 :" << ret;
    return false;
  }
  return true;
}

CVPixelBufferRef CreatePixelBuffer(CVPixelBufferPoolRef pixel_buffer_pool) {
  if (!pixel_buffer_pool) {
    RTC_LOG(LS_ERROR) << "Failed to get pixel buffer pool.";
    return nullptr;
  }
  CVPixelBufferRef pixel_buffer;
  CVReturn ret = CVPixelBufferPoolCreatePixelBuffer(nullptr, pixel_buffer_pool, &pixel_buffer);
  if (ret != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to create pixel buffer: " << ret;
    // We probably want to drop frames here, since failure probably means
    // that the pool is empty.
    return nullptr;
  }
  return pixel_buffer;
}

// This is the callback function that VideoToolbox calls when encode is
// complete. From inspection this happens on its own queue.
void compressionOutputCallback(void *encoder,
                               void *params,
                               OSStatus status,
                               VTEncodeInfoFlags infoFlags,
                               CMSampleBufferRef sampleBuffer) {
  if (!params) {
    // If there are pending callbacks when the encoder is destroyed, this can happen.
    return;
  }
  std::unique_ptr<RTCFrameEncodeParams> encodeParams(
      reinterpret_cast<RTCFrameEncodeParams *>(params));
  [encodeParams->encoder frameWasEncoded:status
                                   flags:infoFlags
                            sampleBuffer:sampleBuffer
                       codecSpecificInfo:encodeParams->codecSpecificInfo
                                   width:encodeParams->width
                                  height:encodeParams->height
                            renderTimeMs:encodeParams->render_time_ms
                               timestamp:encodeParams->timestamp
                                rotation:encodeParams->rotation];
}

// Extract VideoToolbox profile out of the cricket::VideoCodec. If there is no
// specific VideoToolbox profile for the specified level, AutoLevel will be
// returned. The user must initialize the encoder with a resolution and
// framerate conforming to the selected H264 level regardless.
CFStringRef ExtractProfile(webrtc::SdpVideoFormat videoFormat) {
  const rtc::Optional<webrtc::H264::ProfileLevelId> profile_level_id =
      webrtc::H264::ParseSdpProfileLevelId(videoFormat.parameters);
  RTC_DCHECK(profile_level_id);
  switch (profile_level_id->profile) {
    case webrtc::H264::kProfileConstrainedBaseline:
    case webrtc::H264::kProfileBaseline:
      switch (profile_level_id->level) {
        case webrtc::H264::kLevel3:
          return kVTProfileLevel_H264_Baseline_3_0;
        case webrtc::H264::kLevel3_1:
          return kVTProfileLevel_H264_Baseline_3_1;
        case webrtc::H264::kLevel3_2:
          return kVTProfileLevel_H264_Baseline_3_2;
        case webrtc::H264::kLevel4:
          return kVTProfileLevel_H264_Baseline_4_0;
        case webrtc::H264::kLevel4_1:
          return kVTProfileLevel_H264_Baseline_4_1;
        case webrtc::H264::kLevel4_2:
          return kVTProfileLevel_H264_Baseline_4_2;
        case webrtc::H264::kLevel5:
          return kVTProfileLevel_H264_Baseline_5_0;
        case webrtc::H264::kLevel5_1:
          return kVTProfileLevel_H264_Baseline_5_1;
        case webrtc::H264::kLevel5_2:
          return kVTProfileLevel_H264_Baseline_5_2;
        case webrtc::H264::kLevel1:
        case webrtc::H264::kLevel1_b:
        case webrtc::H264::kLevel1_1:
        case webrtc::H264::kLevel1_2:
        case webrtc::H264::kLevel1_3:
        case webrtc::H264::kLevel2:
        case webrtc::H264::kLevel2_1:
        case webrtc::H264::kLevel2_2:
          return kVTProfileLevel_H264_Baseline_AutoLevel;
      }

    case webrtc::H264::kProfileMain:
      switch (profile_level_id->level) {
        case webrtc::H264::kLevel3:
          return kVTProfileLevel_H264_Main_3_0;
        case webrtc::H264::kLevel3_1:
          return kVTProfileLevel_H264_Main_3_1;
        case webrtc::H264::kLevel3_2:
          return kVTProfileLevel_H264_Main_3_2;
        case webrtc::H264::kLevel4:
          return kVTProfileLevel_H264_Main_4_0;
        case webrtc::H264::kLevel4_1:
          return kVTProfileLevel_H264_Main_4_1;
        case webrtc::H264::kLevel4_2:
          return kVTProfileLevel_H264_Main_4_2;
        case webrtc::H264::kLevel5:
          return kVTProfileLevel_H264_Main_5_0;
        case webrtc::H264::kLevel5_1:
          return kVTProfileLevel_H264_Main_5_1;
        case webrtc::H264::kLevel5_2:
          return kVTProfileLevel_H264_Main_5_2;
        case webrtc::H264::kLevel1:
        case webrtc::H264::kLevel1_b:
        case webrtc::H264::kLevel1_1:
        case webrtc::H264::kLevel1_2:
        case webrtc::H264::kLevel1_3:
        case webrtc::H264::kLevel2:
        case webrtc::H264::kLevel2_1:
        case webrtc::H264::kLevel2_2:
          return kVTProfileLevel_H264_Main_AutoLevel;
      }

    case webrtc::H264::kProfileConstrainedHigh:
    case webrtc::H264::kProfileHigh:
      switch (profile_level_id->level) {
        case webrtc::H264::kLevel3:
          return kVTProfileLevel_H264_High_3_0;
        case webrtc::H264::kLevel3_1:
          return kVTProfileLevel_H264_High_3_1;
        case webrtc::H264::kLevel3_2:
          return kVTProfileLevel_H264_High_3_2;
        case webrtc::H264::kLevel4:
          return kVTProfileLevel_H264_High_4_0;
        case webrtc::H264::kLevel4_1:
          return kVTProfileLevel_H264_High_4_1;
        case webrtc::H264::kLevel4_2:
          return kVTProfileLevel_H264_High_4_2;
        case webrtc::H264::kLevel5:
          return kVTProfileLevel_H264_High_5_0;
        case webrtc::H264::kLevel5_1:
          return kVTProfileLevel_H264_High_5_1;
        case webrtc::H264::kLevel5_2:
          return kVTProfileLevel_H264_High_5_2;
        case webrtc::H264::kLevel1:
        case webrtc::H264::kLevel1_b:
        case webrtc::H264::kLevel1_1:
        case webrtc::H264::kLevel1_2:
        case webrtc::H264::kLevel1_3:
        case webrtc::H264::kLevel2:
        case webrtc::H264::kLevel2_1:
        case webrtc::H264::kLevel2_2:
          return kVTProfileLevel_H264_High_AutoLevel;
      }
  }
}
}  // namespace

@implementation RTCVideoEncoderH264 {
  RTCVideoCodecInfo *_codecInfo;
  std::unique_ptr<webrtc::BitrateAdjuster> _bitrateAdjuster;
  uint32_t _targetBitrateBps;
  uint32_t _encoderBitrateBps;
  RTCH264PacketizationMode _packetizationMode;
  CFStringRef _profile;
  RTCVideoEncoderCallback _callback;
  int32_t _width;
  int32_t _height;
  VTCompressionSessionRef _compressionSession;
  RTCVideoCodecMode _mode;

  webrtc::H264BitstreamParser _h264BitstreamParser;
  std::vector<uint8_t> _frameScaleBuffer;
}

// .5 is set as a mininum to prevent overcompensating for large temporary
// overshoots. We don't want to degrade video quality too badly.
// .95 is set to prevent oscillations. When a lower bitrate is set on the
// encoder than previously set, its output seems to have a brief period of
// drastically reduced bitrate, so we want to avoid that. In steady state
// conditions, 0.95 seems to give us better overall bitrate over long periods
// of time.
- (instancetype)initWithCodecInfo:(RTCVideoCodecInfo *)codecInfo {
  if (self = [super init]) {
    _codecInfo = codecInfo;
    _bitrateAdjuster.reset(new webrtc::BitrateAdjuster(
        webrtc::Clock::GetRealTimeClock(), .5, .95));
    _packetizationMode = RTCH264PacketizationModeNonInterleaved;
    _profile = ExtractProfile([codecInfo nativeSdpVideoFormat]);
    RTC_LOG(LS_INFO) << "Using profile " << CFStringToString(_profile);
    RTC_CHECK([codecInfo.name isEqualToString:kRTCVideoCodecH264Name]);

#if defined(WEBRTC_IOS)
    [RTCUIApplicationStatusObserver prepareForUse];
#endif
  }
  return self;
}

- (void)dealloc {
  [self destroyCompressionSession];
}

- (NSInteger)startEncodeWithSettings:(RTCVideoEncoderSettings *)settings
                       numberOfCores:(int)numberOfCores {
  RTC_DCHECK(settings);
  RTC_DCHECK([settings.name isEqualToString:kRTCVideoCodecH264Name]);

  _width = settings.width;
  _height = settings.height;
  _mode = settings.mode;

  // We can only set average bitrate on the HW encoder.
  _targetBitrateBps = settings.startBitrate;
  _bitrateAdjuster->SetTargetBitrateBps(_targetBitrateBps);

  // TODO(tkchin): Try setting payload size via
  // kVTCompressionPropertyKey_MaxH264SliceBytes.

  return [self resetCompressionSessionWithPixelFormat:kNV12PixelFormat];
}

- (NSInteger)encode:(RTCVideoFrame *)frame
    codecSpecificInfo:(id<RTCCodecSpecificInfo>)codecSpecificInfo
           frameTypes:(NSArray<NSNumber *> *)frameTypes {
  RTC_DCHECK_EQ(frame.width, _width);
  RTC_DCHECK_EQ(frame.height, _height);
  if (!_callback || !_compressionSession) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
#if defined(WEBRTC_IOS)
  if (![[RTCUIApplicationStatusObserver sharedInstance] isApplicationActive]) {
    // Ignore all encode requests when app isn't active. In this state, the
    // hardware encoder has been invalidated by the OS.
    return WEBRTC_VIDEO_CODEC_OK;
  }
#endif
  BOOL isKeyframeRequired = NO;

  // Get a pixel buffer from the pool and copy frame data over.
  CVPixelBufferPoolRef pixelBufferPool =
      VTCompressionSessionGetPixelBufferPool(_compressionSession);
  if ([self resetCompressionSessionIfNeededForPool:pixelBufferPool withFrame:frame]) {
    pixelBufferPool = VTCompressionSessionGetPixelBufferPool(_compressionSession);
    isKeyframeRequired = YES;
  }

  CVPixelBufferRef pixelBuffer = nullptr;
  if ([frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]) {
    // Native frame buffer
    RTCCVPixelBuffer *rtcPixelBuffer = (RTCCVPixelBuffer *)frame.buffer;
    if (![rtcPixelBuffer requiresCropping]) {
      // This pixel buffer might have a higher resolution than what the
      // compression session is configured to. The compression session can
      // handle that and will output encoded frames in the configured
      // resolution regardless of the input pixel buffer resolution.
      pixelBuffer = rtcPixelBuffer.pixelBuffer;
      CVBufferRetain(pixelBuffer);
    } else {
      // Cropping required, we need to crop and scale to a new pixel buffer.
      pixelBuffer = CreatePixelBuffer(pixelBufferPool);
      if (!pixelBuffer) {
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      int dstWidth = CVPixelBufferGetWidth(pixelBuffer);
      int dstHeight = CVPixelBufferGetHeight(pixelBuffer);
      if ([rtcPixelBuffer requiresScalingToWidth:dstWidth height:dstHeight]) {
        int size =
            [rtcPixelBuffer bufferSizeForCroppingAndScalingToWidth:dstWidth height:dstHeight];
        _frameScaleBuffer.resize(size);
      } else {
        _frameScaleBuffer.clear();
      }
      _frameScaleBuffer.shrink_to_fit();
      if (![rtcPixelBuffer cropAndScaleTo:pixelBuffer withTempBuffer:_frameScaleBuffer.data()]) {
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
    }
  }

  if (!pixelBuffer) {
    // We did not have a native frame buffer
    pixelBuffer = CreatePixelBuffer(pixelBufferPool);
    if (!pixelBuffer) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    RTC_DCHECK(pixelBuffer);
    if (!CopyVideoFrameToNV12PixelBuffer([frame.buffer toI420], pixelBuffer)) {
      RTC_LOG(LS_ERROR) << "Failed to copy frame data.";
      CVBufferRelease(pixelBuffer);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  // Check if we need a keyframe.
  if (!isKeyframeRequired && frameTypes) {
    for (NSNumber *frameType in frameTypes) {
      if ((RTCFrameType)frameType.intValue == RTCFrameTypeVideoFrameKey) {
        isKeyframeRequired = YES;
        break;
      }
    }
  }

  CMTime presentationTimeStamp = CMTimeMake(frame.timeStampNs / rtc::kNumNanosecsPerMillisec, 1000);
  CFDictionaryRef frameProperties = nullptr;
  if (isKeyframeRequired) {
    CFTypeRef keys[] = {kVTEncodeFrameOptionKey_ForceKeyFrame};
    CFTypeRef values[] = {kCFBooleanTrue};
    frameProperties = CreateCFTypeDictionary(keys, values, 1);
  }

  std::unique_ptr<RTCFrameEncodeParams> encodeParams;
  encodeParams.reset(new RTCFrameEncodeParams(self,
                                              codecSpecificInfo,
                                              _width,
                                              _height,
                                              frame.timeStampNs / rtc::kNumNanosecsPerMillisec,
                                              frame.timeStamp,
                                              frame.rotation));
  encodeParams->codecSpecificInfo.packetizationMode = _packetizationMode;

  // Update the bitrate if needed.
  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps()];

  OSStatus status = VTCompressionSessionEncodeFrame(_compressionSession,
                                                    pixelBuffer,
                                                    presentationTimeStamp,
                                                    kCMTimeInvalid,
                                                    frameProperties,
                                                    encodeParams.release(),
                                                    nullptr);
  if (frameProperties) {
    CFRelease(frameProperties);
  }
  if (pixelBuffer) {
    CVBufferRelease(pixelBuffer);
  }
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to encode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoEncoderCallback)callback {
  _callback = callback;
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
  _targetBitrateBps = 1000 * bitrateKbit;
  _bitrateAdjuster->SetTargetBitrateBps(_targetBitrateBps);
  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps()];
  return WEBRTC_VIDEO_CODEC_OK;
}

#pragma mark - Private

- (NSInteger)releaseEncoder {
  // Need to destroy so that the session is invalidated and won't use the
  // callback anymore. Do not remove callback until the session is invalidated
  // since async encoder callbacks can occur until invalidation.
  [self destroyCompressionSession];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

- (BOOL)resetCompressionSessionIfNeededForPool:(CVPixelBufferPoolRef)pixelBufferPool
                                     withFrame:(RTCVideoFrame *)frame {
  BOOL resetCompressionSession = NO;

#if defined(WEBRTC_IOS)
  if (!pixelBufferPool) {
    // Kind of a hack. On backgrounding, the compression session seems to get
    // invalidated, which causes this pool call to fail when the application
    // is foregrounded and frames are being sent for encoding again.
    // Resetting the session when this happens fixes the issue.
    // In addition we request a keyframe so video can recover quickly.
    resetCompressionSession = YES;
    RTC_LOG(LS_INFO) << "Resetting compression session due to invalid pool.";
  }
#endif

  // If we're capturing native frames in another pixel format than the compression session is
  // configured with, make sure the compression session is reset using the correct pixel format.
  OSType framePixelFormat = kNV12PixelFormat;
  if (pixelBufferPool && [frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]) {
    RTCCVPixelBuffer *rtcPixelBuffer = (RTCCVPixelBuffer *)frame.buffer;
    framePixelFormat = CVPixelBufferGetPixelFormatType(rtcPixelBuffer.pixelBuffer);

    // The pool attribute `kCVPixelBufferPixelFormatTypeKey` can contain either an array of pixel
    // formats or a single pixel format.
    NSDictionary *poolAttributes =
        (__bridge NSDictionary *)CVPixelBufferPoolGetPixelBufferAttributes(pixelBufferPool);
    id pixelFormats =
        [poolAttributes objectForKey:(__bridge NSString *)kCVPixelBufferPixelFormatTypeKey];
    NSArray<NSNumber *> *compressionSessionPixelFormats = nil;
    if ([pixelFormats isKindOfClass:[NSArray class]]) {
      compressionSessionPixelFormats = (NSArray *)pixelFormats;
    } else {
      compressionSessionPixelFormats = @[ (NSNumber *)pixelFormats ];
    }

    if (![compressionSessionPixelFormats
            containsObject:[NSNumber numberWithLong:framePixelFormat]]) {
      resetCompressionSession = YES;
      RTC_LOG(LS_INFO) << "Resetting compression session due to non-matching pixel format.";
    }
  }

  if (resetCompressionSession) {
    [self resetCompressionSessionWithPixelFormat:framePixelFormat];
  }
  return resetCompressionSession;
}

- (int)resetCompressionSessionWithPixelFormat:(OSType)framePixelFormat {
  [self destroyCompressionSession];

  // Set source image buffer attributes. These attributes will be present on
  // buffers retrieved from the encoder's pixel buffer pool.
  const size_t attributesSize = 3;
  CFTypeRef keys[attributesSize] = {
#if defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
    kCVPixelBufferOpenGLCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef ioSurfaceValue = CreateCFTypeDictionary(nullptr, nullptr, 0);
  int64_t pixelFormatType = framePixelFormat;
  CFNumberRef pixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &pixelFormatType);
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue, pixelFormat};
  CFDictionaryRef sourceAttributes = CreateCFTypeDictionary(keys, values, attributesSize);
  if (ioSurfaceValue) {
    CFRelease(ioSurfaceValue);
    ioSurfaceValue = nullptr;
  }
  if (pixelFormat) {
    CFRelease(pixelFormat);
    pixelFormat = nullptr;
  }
  CFMutableDictionaryRef encoder_specs = nullptr;
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  // Currently hw accl is supported above 360p on mac, below 360p
  // the compression session will be created with hw accl disabled.
  encoder_specs = CFDictionaryCreateMutable(
      nullptr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(encoder_specs,
                       kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
                       kCFBooleanTrue);
#endif
  OSStatus status =
      VTCompressionSessionCreate(nullptr,  // use default allocator
                                 _width,
                                 _height,
                                 kCMVideoCodecType_H264,
                                 encoder_specs,  // use hardware accelerated encoder if available
                                 sourceAttributes,
                                 nullptr,  // use default compressed data allocator
                                 compressionOutputCallback,
                                 nullptr,
                                 &_compressionSession);
  if (sourceAttributes) {
    CFRelease(sourceAttributes);
    sourceAttributes = nullptr;
  }
  if (encoder_specs) {
    CFRelease(encoder_specs);
    encoder_specs = nullptr;
  }
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create compression session: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  CFBooleanRef hwaccl_enabled = nullptr;
  status = VTSessionCopyProperty(_compressionSession,
                                 kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
                                 nullptr,
                                 &hwaccl_enabled);
  if (status == noErr && (CFBooleanGetValue(hwaccl_enabled))) {
    RTC_LOG(LS_INFO) << "Compression session created with hw accl enabled";
  } else {
    RTC_LOG(LS_INFO) << "Compression session created with hw accl disabled";
  }
#endif
  [self configureCompressionSession];
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureCompressionSession {
  RTC_DCHECK(_compressionSession);
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_RealTime, true);
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_ProfileLevel, _profile);
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_AllowFrameReordering, false);
  [self setEncoderBitrateBps:_targetBitrateBps];
  // TODO(tkchin): Look at entropy mode and colorspace matrices.
  // TODO(tkchin): Investigate to see if there's any way to make this work.
  // May need it to interop with Android. Currently this call just fails.
  // On inspecting encoder output on iOS8, this value is set to 6.
  // internal::SetVTSessionProperty(compression_session_,
  //     kVTCompressionPropertyKey_MaxFrameDelayCount,
  //     1);

  // Set a relatively large value for keyframe emission (7200 frames or 4 minutes).
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200);
  SetVTSessionProperty(
      _compressionSession, kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240);
}

- (void)destroyCompressionSession {
  if (_compressionSession) {
    VTCompressionSessionInvalidate(_compressionSession);
    CFRelease(_compressionSession);
    _compressionSession = nullptr;
  }
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

- (void)setBitrateBps:(uint32_t)bitrateBps {
  if (_encoderBitrateBps != bitrateBps) {
    [self setEncoderBitrateBps:bitrateBps];
  }
}

- (void)setEncoderBitrateBps:(uint32_t)bitrateBps {
  if (_compressionSession) {
    SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_AverageBitRate, bitrateBps);

    // TODO(tkchin): Add a helper method to set array value.
    int64_t dataLimitBytesPerSecondValue =
        static_cast<int64_t>(bitrateBps * kLimitToAverageBitRateFactor / 8);
    CFNumberRef bytesPerSecond =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &dataLimitBytesPerSecondValue);
    int64_t oneSecondValue = 1;
    CFNumberRef oneSecond =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &oneSecondValue);
    const void *nums[2] = {bytesPerSecond, oneSecond};
    CFArrayRef dataRateLimits = CFArrayCreate(nullptr, nums, 2, &kCFTypeArrayCallBacks);
    OSStatus status = VTSessionSetProperty(
        _compressionSession, kVTCompressionPropertyKey_DataRateLimits, dataRateLimits);
    if (bytesPerSecond) {
      CFRelease(bytesPerSecond);
    }
    if (oneSecond) {
      CFRelease(oneSecond);
    }
    if (dataRateLimits) {
      CFRelease(dataRateLimits);
    }
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to set data rate limit";
    }

    _encoderBitrateBps = bitrateBps;
  }
}

- (void)frameWasEncoded:(OSStatus)status
                  flags:(VTEncodeInfoFlags)infoFlags
           sampleBuffer:(CMSampleBufferRef)sampleBuffer
      codecSpecificInfo:(id<RTCCodecSpecificInfo>)codecSpecificInfo
                  width:(int32_t)width
                 height:(int32_t)height
           renderTimeMs:(int64_t)renderTimeMs
              timestamp:(uint32_t)timestamp
               rotation:(RTCVideoRotation)rotation {
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "H264 encode failed.";
    return;
  }
  if (infoFlags & kVTEncodeInfo_FrameDropped) {
    RTC_LOG(LS_INFO) << "H264 encode dropped frame.";
    return;
  }

  BOOL isKeyframe = NO;
  CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, 0);
  if (attachments != nullptr && CFArrayGetCount(attachments)) {
    CFDictionaryRef attachment =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
    isKeyframe = !CFDictionaryContainsKey(attachment, kCMSampleAttachmentKey_NotSync);
  }

  if (isKeyframe) {
    RTC_LOG(LS_INFO) << "Generated keyframe";
  }

  // Convert the sample buffer into a buffer suitable for RTP packetization.
  // TODO(tkchin): Allocate buffers through a pool.
  std::unique_ptr<rtc::Buffer> buffer(new rtc::Buffer());
  RTCRtpFragmentationHeader *header;
  {
    std::unique_ptr<webrtc::RTPFragmentationHeader> header_cpp;
    bool result =
        H264CMSampleBufferToAnnexBBuffer(sampleBuffer, isKeyframe, buffer.get(), &header_cpp);
    header = [[RTCRtpFragmentationHeader alloc] initWithNativeFragmentationHeader:header_cpp.get()];
    if (!result) {
      return;
    }
  }

  RTCEncodedImage *frame = [[RTCEncodedImage alloc] init];
  frame.buffer = [NSData dataWithBytesNoCopy:buffer->data() length:buffer->size() freeWhenDone:NO];
  frame.encodedWidth = width;
  frame.encodedHeight = height;
  frame.completeFrame = YES;
  frame.frameType = isKeyframe ? RTCFrameTypeVideoFrameKey : RTCFrameTypeVideoFrameDelta;
  frame.captureTimeMs = renderTimeMs;
  frame.timeStamp = timestamp;
  frame.rotation = rotation;
  frame.contentType = (_mode == RTCVideoCodecModeScreensharing) ? RTCVideoContentTypeScreenshare :
                                                                  RTCVideoContentTypeUnspecified;
  frame.flags = webrtc::TimingFrameFlags::kInvalid;

  int qp;
  _h264BitstreamParser.ParseBitstream(buffer->data(), buffer->size());
  _h264BitstreamParser.GetLastSliceQp(&qp);
  frame.qp = @(qp);

  BOOL res = _callback(frame, codecSpecificInfo, header);
  if (!res) {
    RTC_LOG(LS_ERROR) << "Encode callback failed";
    return;
  }
  _bitrateAdjuster->Update(frame.buffer.length);
}

- (RTCVideoEncoderQpThresholds *)scalingSettings {
  return [[RTCVideoEncoderQpThresholds alloc] initWithThresholdsLow:kLowH264QpThreshold
                                                               high:kHighH264QpThreshold];
}

@end
