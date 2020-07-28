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

#import "RTCVideoEncoderH264.h"

#import <VideoToolbox/VideoToolbox.h>
#include <vector>

#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif
#import "RTCCodecSpecificInfoH264.h"
#import "RTCH264ProfileLevelId.h"
#import "api/peerconnection/RTCRtpFragmentationHeader+Private.h"
#import "api/peerconnection/RTCVideoCodecInfo+Private.h"
#import "base/RTCCodecSpecificInfo.h"
#import "base/RTCI420Buffer.h"
#import "base/RTCVideoEncoder.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"

#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/profile_level_id.h"
#include "common_video/include/bitrate_adjuster.h"
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

#include "sdk/WebKit/WebKitUtilities.h"

#import <dlfcn.h>
#import <objc/runtime.h>

VT_EXPORT const CFStringRef kVTVideoEncoderSpecification_RequiredLowLatency;
VT_EXPORT const CFStringRef kVTVideoEncoderSpecification_Usage;
VT_EXPORT const CFStringRef kVTCompressionPropertyKey_Usage;

@interface RTCVideoEncoderH264 ()

- (void)frameWasEncoded:(OSStatus)status
                  flags:(VTEncodeInfoFlags)infoFlags
           sampleBuffer:(CMSampleBufferRef)sampleBuffer
      codecSpecificInfo:(id<RTCCodecSpecificInfo>)codecSpecificInfo
                  width:(int32_t)width
                 height:(int32_t)height
           renderTimeMs:(int64_t)renderTimeMs
              timestamp:(uint32_t)timestamp
               rotation:(RTCVideoRotation)rotation
     isKeyFrameRequired:(bool)isKeyFrameRequired;

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
                       RTCVideoRotation r,
                       bool isKeyFrameRequired)
      : encoder(e), width(w), height(h), render_time_ms(rtms), timestamp(ts), rotation(r), isKeyFrameRequired(isKeyFrameRequired) {
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
  bool isKeyFrameRequired;
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
                                rotation:encodeParams->rotation
                      isKeyFrameRequired:encodeParams->isKeyFrameRequired];
}

// Extract VideoToolbox profile out of the webrtc::SdpVideoFormat. If there is
// no specific VideoToolbox profile for the specified level, AutoLevel will be
// returned. The user must initialize the encoder with a resolution and
// framerate conforming to the selected H264 level regardless.
CFStringRef ExtractProfile(const webrtc::H264::ProfileLevelId &profile_level_id) {
  switch (profile_level_id.profile) {
    case webrtc::H264::kProfileConstrainedBaseline:
    case webrtc::H264::kProfileBaseline:
      switch (profile_level_id.level) {
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
      switch (profile_level_id.level) {
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
      switch (profile_level_id.level) {
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

// The function returns the max allowed sample rate (pixels per second) that
// can be processed by given encoder with |profile_level_id|.
// See https://www.itu.int/rec/dologin_pub.asp?lang=e&id=T-REC-H.264-201610-S!!PDF-E&type=items
// for details.
NSUInteger GetMaxSampleRate(const webrtc::H264::ProfileLevelId &profile_level_id) {
  switch (profile_level_id.level) {
    case webrtc::H264::kLevel3:
      return 10368000;
    case webrtc::H264::kLevel3_1:
      return 27648000;
    case webrtc::H264::kLevel3_2:
      return 55296000;
    case webrtc::H264::kLevel4:
    case webrtc::H264::kLevel4_1:
      return 62914560;
    case webrtc::H264::kLevel4_2:
      return 133693440;
    case webrtc::H264::kLevel5:
      return 150994944;
    case webrtc::H264::kLevel5_1:
      return 251658240;
    case webrtc::H264::kLevel5_2:
      return 530841600;
    case webrtc::H264::kLevel1:
    case webrtc::H264::kLevel1_b:
    case webrtc::H264::kLevel1_1:
    case webrtc::H264::kLevel1_2:
    case webrtc::H264::kLevel1_3:
    case webrtc::H264::kLevel2:
    case webrtc::H264::kLevel2_1:
    case webrtc::H264::kLevel2_2:
      // Zero means auto rate setting.
      return 0;
  }
}
}  // namespace

@implementation RTCVideoEncoderH264 {
  RTCVideoCodecInfo *_codecInfo;
  std::unique_ptr<webrtc::BitrateAdjuster> _bitrateAdjuster;
  uint32_t _targetBitrateBps;
  uint32_t _encoderBitrateBps;
  uint32_t _encoderFrameRate;
  uint32_t _maxAllowedFrameRate;
  RTCH264PacketizationMode _packetizationMode;
  absl::optional<webrtc::H264::ProfileLevelId> _profile_level_id;
  RTCVideoEncoderCallback _callback;
  int32_t _width;
  int32_t _height;
  bool _useVCP;
  VTCompressionSessionRef _vtCompressionSession;
  VCPCompressionSessionRef _vcpCompressionSession;
  CVPixelBufferPoolRef _pixelBufferPool;
  RTCVideoCodecMode _mode;

  webrtc::H264BitstreamParser _h264BitstreamParser;
  std::vector<uint8_t> _frameScaleBuffer;
  bool _disableEncoding;
  bool _isKeyFrameRequired;
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
    _bitrateAdjuster.reset(new webrtc::BitrateAdjuster(.5, .95));
    _packetizationMode = RTCH264PacketizationModeNonInterleaved;
    _profile_level_id =
        webrtc::H264::ParseSdpProfileLevelId([codecInfo nativeSdpVideoFormat].parameters);
    if (_profile_level_id) {
      auto profile = ExtractProfile(*_profile_level_id);
      _useVCP = [(__bridge NSString *)profile containsString: @"High"];
    } else {
      _useVCP = false;
    }
    RTC_DCHECK(_profile_level_id);
    RTC_LOG(LS_INFO) << "Using profile " << CFStringToString(ExtractProfile(*_profile_level_id));
    RTC_CHECK([codecInfo.name isEqualToString:kRTCVideoCodecH264Name]);
  }
  _isKeyFrameRequired = false;

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

  uint32_t aligned_width = (((_width + 15) >> 4) << 4);
  uint32_t aligned_height = (((_height + 15) >> 4) << 4);
  _maxAllowedFrameRate = static_cast<uint32_t>(GetMaxSampleRate(*_profile_level_id) /
                                               (aligned_width * aligned_height));

  // We can only set average bitrate on the HW encoder.
  _targetBitrateBps = settings.startBitrate * 1000;  // startBitrate is in kbps.
  _bitrateAdjuster->SetTargetBitrateBps(_targetBitrateBps);
  _encoderFrameRate = MIN(settings.maxFramerate, _maxAllowedFrameRate);
  if (settings.maxFramerate > _maxAllowedFrameRate && _maxAllowedFrameRate > 0) {
    RTC_LOG(LS_WARNING) << "Initial encoder frame rate setting " << settings.maxFramerate
                        << " is larger than the "
                        << "maximal allowed frame rate " << _maxAllowedFrameRate << ".";
  }

  // TODO(tkchin): Try setting payload size via
  // kVTCompressionPropertyKey_MaxH264SliceBytes.

  return [self resetCompressionSessionWithPixelFormat:kNV12PixelFormat];
}

- (bool)hasCompressionSession
{
    return _vtCompressionSession || _vcpCompressionSession;
}

- (NSInteger)encode:(RTCVideoFrame *)frame
    codecSpecificInfo:(nullable id<RTCCodecSpecificInfo>)codecSpecificInfo
           frameTypes:(NSArray<NSNumber *> *)frameTypes {
  if (!_callback || ![self hasCompressionSession]) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  BOOL isKeyframeRequired = _isKeyFrameRequired;
  _isKeyFrameRequired = false;

  // Get a pixel buffer from the pool and copy frame data over.
  if ([self resetCompressionSessionIfNeededWithFrame:frame]) {
    isKeyframeRequired = YES;
  }

  if (_disableEncoding) {
    return WEBRTC_VIDEO_CODEC_ERROR;
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
      pixelBuffer = CreatePixelBuffer(_pixelBufferPool);
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
        CVBufferRelease(pixelBuffer);
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
    }
  }

  if (!pixelBuffer) {
    // We did not have a native frame buffer
    pixelBuffer = CreatePixelBuffer(_pixelBufferPool);
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
        RTC_LOG(LS_INFO) << "keyframe requested";
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
                                              frame.rotation,
                                              isKeyframeRequired));
  encodeParams->codecSpecificInfo.packetizationMode = _packetizationMode;

  // Update the bitrate if needed.
  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps() frameRate:_encoderFrameRate];

  OSStatus status;
  if (_vtCompressionSession) {
    status = VTCompressionSessionEncodeFrame(_vtCompressionSession,
                                                    pixelBuffer,
                                                    presentationTimeStamp,
                                                    kCMTimeInvalid,
                                                    frameProperties,
                                                    encodeParams.release(),
                                                    nullptr);
  } else {
#if ENABLE_VCP_ENCODER
    status = webrtc::VCPCompressionSessionEncodeFrame(_vcpCompressionSession,
                                                    pixelBuffer,
                                                    presentationTimeStamp,
                                                    kCMTimeInvalid,
                                                    frameProperties,
                                                    encodeParams.release(),
                                                    nullptr);
#else
    status = 1;
#endif
  }
  if (frameProperties) {
    CFRelease(frameProperties);
  }
  if (pixelBuffer) {
    CVBufferRelease(pixelBuffer);
  }

  if (status == kVTInvalidSessionErr) {
    // This error occurs when entering foreground after backgrounding the app.
    RTC_LOG(LS_ERROR) << "Invalid compression session, resetting.";
    [self resetCompressionSessionWithPixelFormat:[self pixelFormatOfFrame:frame]];

    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  } else if (status == kVTVideoEncoderMalfunctionErr) {
    // Sometimes the encoder malfunctions and needs to be restarted.
    RTC_LOG(LS_ERROR)
        << "Encountered video encoder malfunction error. Resetting compression session.";
    [self resetCompressionSessionWithPixelFormat:[self pixelFormatOfFrame:frame]];

    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  } else if (status != noErr) {
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
  if (framerate > _maxAllowedFrameRate && _maxAllowedFrameRate > 0) {
    RTC_LOG(LS_WARNING) << "Encoder frame rate setting " << framerate << " is larger than the "
                        << "maximal allowed frame rate " << _maxAllowedFrameRate << ".";
  }
  framerate = MIN(framerate, _maxAllowedFrameRate);
  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps() frameRate:framerate];
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

- (OSType)pixelFormatOfFrame:(RTCVideoFrame *)frame {
  // Use NV12 for non-native frames.
  if ([frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]) {
    RTCCVPixelBuffer *rtcPixelBuffer = (RTCCVPixelBuffer *)frame.buffer;
    return CVPixelBufferGetPixelFormatType(rtcPixelBuffer.pixelBuffer);
  }

  return kNV12PixelFormat;
}

- (BOOL)resetCompressionSessionIfNeededWithFrame:(RTCVideoFrame *)frame {
  BOOL resetCompressionSession = NO;

  // If we're capturing native frames in another pixel format than the compression session is
  // configured with, make sure the compression session is reset using the correct pixel format.
  OSType framePixelFormat = [self pixelFormatOfFrame:frame];

  if ([self hasCompressionSession]) {
#if defined(WEBRTC_WEBKIT_BUILD)
    if (!_pixelBufferPool) {
      return NO;
    }
#endif
    // The pool attribute `kCVPixelBufferPixelFormatTypeKey` can contain either an array of pixel
    // formats or a single pixel format.
    NSDictionary *poolAttributes =
        (__bridge NSDictionary *)CVPixelBufferPoolGetPixelBufferAttributes(_pixelBufferPool);
    id pixelFormats =
        [poolAttributes objectForKey:(__bridge NSString *)kCVPixelBufferPixelFormatTypeKey];
    NSArray<NSNumber *> *compressionSessionPixelFormats = nil;
    if ([pixelFormats isKindOfClass:[NSArray class]]) {
      compressionSessionPixelFormats = (NSArray *)pixelFormats;
    } else if ([pixelFormats isKindOfClass:[NSNumber class]]) {
      compressionSessionPixelFormats = @[ (NSNumber *)pixelFormats ];
    }

    if (![compressionSessionPixelFormats
            containsObject:[NSNumber numberWithLong:framePixelFormat]]) {
      resetCompressionSession = YES;
      RTC_LOG(LS_INFO) << "Resetting compression session due to non-matching pixel format.";
    }
  } else {
    resetCompressionSession = YES;
  }

  if (resetCompressionSession) {
    [self resetCompressionSessionWithPixelFormat:framePixelFormat];
  }
  return resetCompressionSession;
}

- (int)resetCompressionSessionWithPixelFormat:(OSType)framePixelFormat {
  [self destroyCompressionSession];
  _disableEncoding = false;

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
  CFMutableDictionaryRef encoderSpecs = CFDictionaryCreateMutable(nullptr, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
#if !defined(WEBRTC_IOS)
  auto useHardwareEncoder = webrtc::isH264HardwareEncoderAllowed() ? kCFBooleanTrue : kCFBooleanFalse;
  // Currently hw accl is supported above 360p on mac, below 360p
  // the compression session will be created with hw accl disabled.
  CFDictionarySetValue(encoderSpecs, kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, useHardwareEncoder);
#if !HAVE_VTB_REQUIREDLOWLATENCY
  CFDictionarySetValue(encoderSpecs, kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder, useHardwareEncoder);
#endif
#endif
  CFDictionarySetValue(encoderSpecs, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);

#if ENABLE_VCP_ENCODER
  if (_useVCP) {
    int usageValue = 1;
    auto usage = CFNumberCreate(nullptr, kCFNumberIntType, &usageValue);
    CFDictionarySetValue(encoderSpecs, kVTCompressionPropertyKey_Usage, usage);
    CFRelease(usage);
#if ENABLE_VCP_VTB_ENCODER
    CFDictionarySetValue(encoderSpecs, kVTVideoEncoderList_EncoderID, CFSTR("com.apple.videotoolbox.videoencoder.h264.rtvc"));
#endif
  }
#elif HAVE_VTB_REQUIREDLOWLATENCY
  if (webrtc::isH264LowLatencyEncoderEnabled() && _useVCP)
    CFDictionarySetValue(encoderSpecs, kVTVideoEncoderSpecification_RequiredLowLatency, kCFBooleanTrue);
#endif

  OSStatus status =
      VTCompressionSessionCreate(nullptr,  // use default allocator
                                 _width,
                                 _height,
                                 kCMVideoCodecType_H264,
                                 encoderSpecs,  // use hardware accelerated encoder if available
                                 sourceAttributes,
                                 nullptr,  // use default compressed data allocator
                                 compressionOutputCallback,
                                 nullptr,
                                 &_vtCompressionSession);

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#if ENABLE_VCP_ENCODER
  CFBooleanRef hwaccl_enabled = nullptr;
  if (status == noErr) {
    status = VTSessionCopyProperty(_vtCompressionSession,
                               kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
                               nullptr,
                               &hwaccl_enabled);
  }
  if (status == noErr && (CFBooleanGetValue(hwaccl_enabled))) {
    RTC_LOG(LS_INFO) << "Compression session created with hw accl enabled";
  } else {
    [self destroyCompressionSession];

    // Use VCP instead.
    int usageValue = 1;
    auto usage = CFNumberCreate(nullptr, kCFNumberIntType, &usageValue);
    CFDictionarySetValue(encoderSpecs, kVTCompressionPropertyKey_Usage, usage);
    CFRelease(usage);

    RTC_LOG(LS_INFO) << "Compression session created with VCP";
    status =
        webrtc::VCPCompressionSessionCreate(nullptr,  // use default allocator
                               _width,
                               _height,
                               kVCPCodecType4CC_H264,
                               encoderSpecs,
                               sourceAttributes,
                               nullptr,  // use default compressed data allocator
                               compressionOutputCallback,
                               nullptr,
                               &_vcpCompressionSession);
  }
#elif HAVE_VTB_REQUIREDLOWLATENCY
  // In case VCP is disabled, we will use it anyway if using software encoder.
  if (webrtc::isH264LowLatencyEncoderEnabled() && !_useVCP) {
    CFBooleanRef hwaccl_enabled = nullptr;
    if (status == noErr) {
      status = VTSessionCopyProperty(_vtCompressionSession,
                               kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
                               nullptr,
                               &hwaccl_enabled);
    }
    if (status == noErr && (CFBooleanGetValue(hwaccl_enabled))) {
      RTC_LOG(LS_INFO) << "Compression session created with hw accl enabled";
    } else {
      [self destroyCompressionSession];
      CFDictionarySetValue(encoderSpecs, kVTVideoEncoderSpecification_RequiredLowLatency, kCFBooleanTrue);
      status = VTCompressionSessionCreate(nullptr,  // use default allocator
                                 _width,
                                 _height,
                                 kCMVideoCodecType_H264,
                                 encoderSpecs,  // use hardware accelerated encoder if available
                                 sourceAttributes,
                                 nullptr,  // use default compressed data allocator
                                 compressionOutputCallback,
                                 nullptr,
                                 &_vtCompressionSession);
    }
  }
#else
  if (status != noErr) {
    if (encoderSpecs) {
        CFRelease(encoderSpecs);
        encoderSpecs = nullptr;
    }
    if (sourceAttributes) {
      CFRelease(sourceAttributes);
      sourceAttributes = nullptr;
    }

    auto isStandardFrameSize = [](int32_t width, int32_t height) {
        // FIXME: Envision relaxing this rule, something like width and height dividable by 4 or 8 should be good enough.
        if (width == 1280)
            return height == 720;
        if (width == 720)
            return height == 1280;
        if (width == 960)
            return height == 540;
        if (width == 540)
            return height == 960;
        if (width == 640)
            return height == 480;
        if (width == 480)
            return height == 640;
        if (width == 288)
            return height == 352;
        if (width == 352)
            return height == 288;
        if (width == 320)
            return height == 240;
        if (width == 240)
            return height == 320;
        return false;
    };

    if (!isStandardFrameSize(_width, _height)) {
      _disableEncoding = true;
      RTC_LOG(LS_ERROR) << "Using H264 software encoder with non standard size is not supported";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    [self destroyCompressionSession];

    CFDictionaryRef ioSurfaceValue = CreateCFTypeDictionary(nullptr, nullptr, 0);
    int64_t pixelFormatType = framePixelFormat;
    CFNumberRef pixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &pixelFormatType);

    const size_t attributesSize = 3;
    CFTypeRef keys[attributesSize] = {
      kCVPixelBufferOpenGLCompatibilityKey,
      kCVPixelBufferIOSurfacePropertiesKey,
      kCVPixelBufferPixelFormatTypeKey
    };
    CFTypeRef values[attributesSize] = {
      kCFBooleanTrue,
      ioSurfaceValue,
      pixelFormat};
    sourceAttributes = CreateCFTypeDictionary(keys, values, attributesSize);

    if (ioSurfaceValue) {
      CFRelease(ioSurfaceValue);
      ioSurfaceValue = nullptr;
    }
    if (pixelFormat) {
      CFRelease(pixelFormat);
      pixelFormat = nullptr;
    }

    encoderSpecs = CFDictionaryCreateMutable(nullptr, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(encoderSpecs, kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, kCFBooleanFalse);
    int usageValue = 1;
    CFNumberRef usage = CFNumberCreate(nullptr, kCFNumberIntType, &usageValue);
    CFDictionarySetValue(encoderSpecs, kVTVideoEncoderSpecification_Usage, usage);
    if (usage) {
      CFRelease(usage);
      usage = nullptr;
    }
    status = VTCompressionSessionCreate(nullptr,  // use default allocator
                                 _width,
                                 _height,
                                 kCMVideoCodecType_H264,
                                 encoderSpecs,  // use hardware accelerated encoder if available
                                 sourceAttributes,
                                 nullptr,  // use default compressed data allocator
                                 compressionOutputCallback,
                                 nullptr,
                                 &_vtCompressionSession);
  }
#endif // !HAVE_VTB_REQUIREDLOWLATENCY
#endif // defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  if (sourceAttributes) {
    CFRelease(sourceAttributes);
    sourceAttributes = nullptr;
  }
  if (encoderSpecs) {
    CFRelease(encoderSpecs);
    encoderSpecs = nullptr;
  }
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create compression session: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  [self configureCompressionSession];

#if !defined(WEBRTC_WEBKIT_BUILD)
  // The pixel buffer pool is dependent on the compression session so if the session is reset, the
  // pool should be reset as well.
  if (_vtCompressionSession)
    _pixelBufferPool = VTCompressionSessionGetPixelBufferPool(_vtCompressionSession);
#if ENABLE_VCP_ENCODER
  else
    _pixelBufferPool = webrtc::VCPCompressionSessionGetPixelBufferPool(_vtCompressionSession);
#endif
#endif
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureCompressionSession {
  RTC_DCHECK([self hasCompressionSession]);
  SetVTSessionProperty(_vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_RealTime, true);
  SetVTSessionProperty(_vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_ProfileLevel, ExtractProfile(*_profile_level_id));
  SetVTSessionProperty(_vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_AllowFrameReordering, false);
#if ENABLE_VCP_ENCODER
  if (_useVCP) {
    SetVTSessionProperty(_vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_Usage, 1);
  }
#endif
  [self setEncoderBitrateBps:_targetBitrateBps frameRate:_encoderFrameRate];
  // TODO(tkchin): Look at entropy mode and colorspace matrices.
  // TODO(tkchin): Investigate to see if there's any way to make this work.
  // May need it to interop with Android. Currently this call just fails.
  // On inspecting encoder output on iOS8, this value is set to 6.
  // internal::SetVTSessionProperty(compression_session_,
  //     kVTCompressionPropertyKey_MaxFrameDelayCount,
  //     1);

  // Set a relatively large value for keyframe emission (7200 frames or 4 minutes).
  SetVTSessionProperty(_vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200);
  SetVTSessionProperty(
      _vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240);
}

- (void)destroyCompressionSession {
  if (_vtCompressionSession) {
    VTCompressionSessionInvalidate(_vtCompressionSession);
    CFRelease(_vtCompressionSession);
    _vtCompressionSession = nullptr;
  }
#if ENABLE_VCP_ENCODER
  if (_vcpCompressionSession) {
    webrtc::VCPCompressionSessionInvalidate(_vcpCompressionSession);
    CFRelease(_vcpCompressionSession);
    _vcpCompressionSession = nullptr;
  }
#endif
  _pixelBufferPool = nullptr;
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

- (void)setBitrateBps:(uint32_t)bitrateBps frameRate:(uint32_t)frameRate {
  if (_encoderBitrateBps != bitrateBps || _encoderFrameRate != frameRate) {
    [self setEncoderBitrateBps:bitrateBps frameRate:frameRate];
  }
}

- (void)setEncoderBitrateBps:(uint32_t)bitrateBps frameRate:(uint32_t)frameRate {
  if ([self hasCompressionSession]) {
    SetVTSessionProperty(_vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_AverageBitRate, bitrateBps);

    // With zero |_maxAllowedFrameRate|, we fall back to automatic frame rate detection.
    if (_maxAllowedFrameRate > 0) {
      SetVTSessionProperty(
          _vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_ExpectedFrameRate, frameRate);
    }

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

    SetVTSessionProperty(_vtCompressionSession, _vcpCompressionSession, kVTCompressionPropertyKey_DataRateLimits, dataRateLimits);

    if (bytesPerSecond) {
      CFRelease(bytesPerSecond);
    }
    if (oneSecond) {
      CFRelease(oneSecond);
    }
    if (dataRateLimits) {
      CFRelease(dataRateLimits);
    }

    _encoderBitrateBps = bitrateBps;
    _encoderFrameRate = frameRate;
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
               rotation:(RTCVideoRotation)rotation
     isKeyFrameRequired:(bool)isKeyFrameRequired {
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "H264 encode failed with code: " << status;
    if (isKeyFrameRequired)
      _isKeyFrameRequired = true;
    return;
  }
  if (infoFlags & kVTEncodeInfo_FrameDropped) {
    if (isKeyFrameRequired)
      _isKeyFrameRequired = true;
    RTC_LOG(LS_INFO) << "H264 encode dropped frame.";
    return;
  }
  _isKeyFrameRequired = false;

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

  __block std::unique_ptr<rtc::Buffer> buffer = std::make_unique<rtc::Buffer>();
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
  // This assumes ownership of `buffer` and is responsible for freeing it when done.
  frame.buffer = [[NSData alloc] initWithBytesNoCopy:buffer->data()
                                              length:buffer->size()
                                         deallocator:^(void *bytes, NSUInteger size) {
                                           buffer.reset();
                                         }];
  frame.encodedWidth = width;
  frame.encodedHeight = height;
  frame.completeFrame = YES;
  frame.frameType = isKeyframe ? RTCFrameTypeVideoFrameKey : RTCFrameTypeVideoFrameDelta;
  frame.captureTimeMs = renderTimeMs;
  frame.timeStamp = timestamp;
  frame.rotation = rotation;
  frame.contentType = (_mode == RTCVideoCodecModeScreensharing) ? RTCVideoContentTypeScreenshare :
                                                                  RTCVideoContentTypeUnspecified;
  frame.flags = webrtc::VideoSendTiming::kInvalid;

  int qp;
  _h264BitstreamParser.ParseBitstream(buffer->data(), buffer->size());
  _h264BitstreamParser.GetLastSliceQp(&qp);
  frame.qp = @(qp);

  BOOL res = _callback(frame, codecSpecificInfo, header);
  if (!res) {
    RTC_LOG(LS_ERROR) << "Encode callback failed";
    if (isKeyFrameRequired)
      _isKeyFrameRequired = true;
    return;
  }
  _bitrateAdjuster->Update(frame.buffer.length);
}

- (nullable RTCVideoEncoderQpThresholds *)scalingSettings {
  return [[RTCVideoEncoderQpThresholds alloc] initWithThresholdsLow:kLowH264QpThreshold
                                                               high:kHighH264QpThreshold];
}

@end
