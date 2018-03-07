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

#include "webrtc/sdk/WebKit/encoder.h"
#include "VideoToolBoxEncoderFactory.h"

#include <memory>
#include <string>
#include <vector>

#if defined(WEBRTC_IOS)  && !defined(WEBRTC_WEBKIT_BUILD)
#import "Common/RTCUIApplicationStatusObserver.h"
#import "WebRTC/UIDevice+RTCDevice.h"
#endif
#include "libyuv/convert_from.h"
#include "webrtc/common_video/h264/profile_level_id.h"
#include "webrtc/sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h"
#include "webrtc/sdk/objc/Framework/Classes/VideoToolbox/nalu_rewriter.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace internal {

// The ratio between kVTCompressionPropertyKey_DataRateLimits and
// kVTCompressionPropertyKey_AverageBitRate. The data rate limit is set higher
// than the average bit rate to avoid undershooting the target.
const float kLimitToAverageBitRateFactor = 1.5f;
// These thresholds deviate from the default h264 QP thresholds, as they
// have been found to work better on devices that support VideoToolbox
const int kLowH264QpThreshold = 28;
const int kHighH264QpThreshold = 39;

// Convenience function for creating a dictionary.
inline CFDictionaryRef CreateCFDictionary(CFTypeRef* keys,
                                          CFTypeRef* values,
                                          size_t size) {
  return CFDictionaryCreate(kCFAllocatorDefault, keys, values, size,
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

// Copies characters from a CFStringRef into a std::string.
static std::string CFStringToString(const CFStringRef cf_string) {
  RTC_DCHECK(cf_string);
  std::string std_string;
  // Get the size needed for UTF8 plus terminating character.
  size_t buffer_size =
      CFStringGetMaximumSizeForEncoding(CFStringGetLength(cf_string),
                                        kCFStringEncodingUTF8) +
      1;
  std::unique_ptr<char[]> buffer(new char[buffer_size]);
  if (CFStringGetCString(cf_string, buffer.get(), buffer_size,
                         kCFStringEncodingUTF8)) {
    // Copy over the characters.
    std_string.assign(buffer.get());
  }
  return std_string;
}

// Convenience function for setting a VT property.
static void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          int32_t value) {
  CFNumberRef cfNum =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
  OSStatus status = VTSessionSetProperty(session, key, cfNum);
  CFRelease(cfNum);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                  << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
static void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          uint32_t value) {
  int64_t value_64 = value;
  CFNumberRef cfNum =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value_64);
  OSStatus status = VTSessionSetProperty(session, key, cfNum);
  CFRelease(cfNum);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                  << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
static void SetVTSessionProperty(VTSessionRef session, CFStringRef key, bool value) {
  CFBooleanRef cf_bool = (value) ? kCFBooleanTrue : kCFBooleanFalse;
  OSStatus status = VTSessionSetProperty(session, key, cf_bool);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                  << " to " << value << ": " << status;
  }
}

// Convenience function for setting a VT property.
static void SetVTSessionProperty(VTSessionRef session,
                          CFStringRef key,
                          CFStringRef value) {
  OSStatus status = VTSessionSetProperty(session, key, value);
  if (status != noErr) {
    std::string key_string = CFStringToString(key);
    std::string val_string = CFStringToString(value);
    RTC_LOG(LS_ERROR) << "VTSessionSetProperty failed to set: " << key_string
                  << " to " << val_string << ": " << status;
  }
}

// Struct that we pass to the encoder per frame to encode. We receive it again
// in the encoder callback.
struct FrameEncodeParams {
  FrameEncodeParams(webrtc::H264VideoToolboxEncoder* e,
                    const webrtc::CodecSpecificInfo* csi,
                    int32_t w,
                    int32_t h,
                    int64_t rtms,
                    uint32_t ts,
                    webrtc::VideoRotation r)
      : encoder(e),
        width(w),
        height(h),
        render_time_ms(rtms),
        timestamp(ts),
        rotation(r) {
    if (csi) {
      codec_specific_info = *csi;
    } else {
      codec_specific_info.codecType = webrtc::kVideoCodecH264;
    }
  }

  webrtc::H264VideoToolboxEncoder* encoder;
  webrtc::CodecSpecificInfo codec_specific_info;
  int32_t width;
  int32_t height;
  int64_t render_time_ms;
  uint32_t timestamp;
  webrtc::VideoRotation rotation;
};

// We receive I420Frames as input, but we need to feed CVPixelBuffers into the
// encoder. This performs the copy and format conversion.
// TODO(tkchin): See if encoder will accept i420 frames and compare performance.
static bool CopyVideoFrameToPixelBuffer(const rtc::scoped_refptr<webrtc::I420BufferInterface>& frame,
                                 CVPixelBufferRef pixel_buffer) {
  RTC_DCHECK(pixel_buffer);
  RTC_DCHECK_EQ(CVPixelBufferGetPixelFormatType(pixel_buffer),
                kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
  RTC_DCHECK_EQ(CVPixelBufferGetHeightOfPlane(pixel_buffer, 0),
                static_cast<size_t>(frame->height()));
  RTC_DCHECK_EQ(CVPixelBufferGetWidthOfPlane(pixel_buffer, 0),
                static_cast<size_t>(frame->width()));

  CVReturn cvRet = CVPixelBufferLockBaseAddress(pixel_buffer, 0);
  if (cvRet != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock base address: " << cvRet;
    return false;
  }
  uint8_t* dst_y = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 0));
  int dst_stride_y = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 0);
  uint8_t* dst_uv = reinterpret_cast<uint8_t*>(
      CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, 1));
  int dst_stride_uv = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, 1);
  // Convert I420 to NV12.
  int ret = libyuv::I420ToNV12(
      frame->DataY(), frame->StrideY(),
      frame->DataU(), frame->StrideU(),
      frame->DataV(), frame->StrideV(),
      dst_y, dst_stride_y, dst_uv, dst_stride_uv,
      frame->width(), frame->height());
  CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
  if (ret) {
    RTC_LOG(LS_ERROR) << "Error converting I420 VideoFrame to NV12 :" << ret;
    return false;
  }
  return true;
}

static CVPixelBufferRef CreatePixelBuffer(CVPixelBufferPoolRef pixel_buffer_pool) {
  if (!pixel_buffer_pool) {
    RTC_LOG(LS_ERROR) << "Failed to get pixel buffer pool.";
    return nullptr;
  }
  CVPixelBufferRef pixel_buffer;
  CVReturn ret = CVPixelBufferPoolCreatePixelBuffer(nullptr, pixel_buffer_pool,
                                                    &pixel_buffer);
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
static void VTCompressionOutputCallback(void* encoder,
                                 void* params,
                                 OSStatus status,
                                 VTEncodeInfoFlags info_flags,
                                 CMSampleBufferRef sample_buffer) {
  std::unique_ptr<FrameEncodeParams> encode_params(
      reinterpret_cast<FrameEncodeParams*>(params));
  encode_params->encoder->OnEncodedFrame(
      status, info_flags, sample_buffer, encode_params->codec_specific_info,
      encode_params->width, encode_params->height,
      encode_params->render_time_ms, encode_params->timestamp,
      encode_params->rotation);
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

}  // namespace internal

namespace webrtc {

// .5 is set as a mininum to prevent overcompensating for large temporary
// overshoots. We don't want to degrade video quality too badly.
// .95 is set to prevent oscillations. When a lower bitrate is set on the
// encoder than previously set, its output seems to have a brief period of
// drastically reduced bitrate, so we want to avoid that. In steady state
// conditions, 0.95 seems to give us better overall bitrate over long periods
// of time.
H264VideoToolboxEncoder::H264VideoToolboxEncoder(const webrtc::SdpVideoFormat& format, VideoToolboxVideoEncoderFactory& factory)
    : callback_(nullptr),
      compression_session_(nullptr),
      bitrate_adjuster_(Clock::GetRealTimeClock(), .5, .95),
      packetization_mode_(H264PacketizationMode::NonInterleaved),
      profile_(internal::ExtractProfile(format)),
      factory_(&factory) {
  RTC_LOG(LS_INFO) << "Using profile " << internal::CFStringToString(profile_);
  RTC_CHECK(cricket::CodecNamesEq(format.name, cricket::kH264CodecName));

  factory_->Add(*this);
}

H264VideoToolboxEncoder::~H264VideoToolboxEncoder() {
  DestroyCompressionSession();

  if (factory_)
    factory_->Remove(*this);
}

int H264VideoToolboxEncoder::InitEncode(const VideoCodec* codec_settings,
                                        int number_of_cores,
                                        size_t max_payload_size) {
  RTC_DCHECK(codec_settings);
  RTC_DCHECK_EQ(codec_settings->codecType, kVideoCodecH264);

  width_ = codec_settings->width;
  height_ = codec_settings->height;
  mode_ = codec_settings->mode;
  // We can only set average bitrate on the HW encoder.
  target_bitrate_bps_ = codec_settings->startBitrate;
  bitrate_adjuster_.SetTargetBitrateBps(target_bitrate_bps_);

  // TODO(tkchin): Try setting payload size via
  // kVTCompressionPropertyKey_MaxH264SliceBytes.

  return ResetCompressionSession();
}

int H264VideoToolboxEncoder::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  // |input_frame| size should always match codec settings.
  RTC_DCHECK_EQ(frame.width(), width_);
  RTC_DCHECK_EQ(frame.height(), height_);
  if (!callback_ || !compression_session_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

#if defined(WEBRTC_IOS)
#if !defined(WEBRTC_WEBKIT_BUILD)
    if (![[RTCUIApplicationStatusObserver sharedInstance] isApplicationActive]) {
#else
    if (!is_active_) {
#endif
    // Ignore all encode requests when app isn't active. In this state, the
    // hardware encoder has been invalidated by the OS.
    return WEBRTC_VIDEO_CODEC_OK;
  }
#endif
  bool is_keyframe_required = false;

  // Get a pixel buffer from the pool and copy frame data over.
  CVPixelBufferPoolRef pixel_buffer_pool =
      VTCompressionSessionGetPixelBufferPool(compression_session_);
#if defined(WEBRTC_IOS)
  if (!pixel_buffer_pool) {
    // Kind of a hack. On backgrounding, the compression session seems to get
    // invalidated, which causes this pool call to fail when the application
    // is foregrounded and frames are being sent for encoding again.
    // Resetting the session when this happens fixes the issue.
    // In addition we request a keyframe so video can recover quickly.
    ResetCompressionSession();
    pixel_buffer_pool =
        VTCompressionSessionGetPixelBufferPool(compression_session_);
    is_keyframe_required = true;
    RTC_LOG(LS_INFO) << "Resetting compression session due to invalid pool.";
  }
#endif

  CVPixelBufferRef pixel_buffer;
  if (frame.video_frame_buffer()->type() == VideoFrameBuffer::Type::kNative) {
    rtc::scoped_refptr<CoreVideoFrameBuffer> core_video_frame_buffer(
        static_cast<CoreVideoFrameBuffer*>(frame.video_frame_buffer().get()));
    if (!core_video_frame_buffer->RequiresCropping()) {
      pixel_buffer = core_video_frame_buffer->pixel_buffer();
      // This pixel buffer might have a higher resolution than what the
      // compression session is configured to. The compression session can
      // handle that and will output encoded frames in the configured
      // resolution regardless of the input pixel buffer resolution.
      CVBufferRetain(pixel_buffer);
    } else {
      // Cropping required, we need to crop and scale to a new pixel buffer.
      pixel_buffer = internal::CreatePixelBuffer(pixel_buffer_pool);
      if (!pixel_buffer) {
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      if (!core_video_frame_buffer->CropAndScaleTo(&nv12_scale_buffer_,
                                                   pixel_buffer)) {
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
    }
  } else {
    pixel_buffer = internal::CreatePixelBuffer(pixel_buffer_pool);
    if (!pixel_buffer) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    RTC_DCHECK(pixel_buffer);
    if (!internal::CopyVideoFrameToPixelBuffer(frame.video_frame_buffer()->ToI420(),
                                               pixel_buffer)) {
      RTC_LOG(LS_ERROR) << "Failed to copy frame data.";
      CVBufferRelease(pixel_buffer);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  // Check if we need a keyframe.
  if (!is_keyframe_required && frame_types) {
    for (auto frame_type : *frame_types) {
      if (frame_type == kVideoFrameKey) {
        is_keyframe_required = true;
        break;
      }
    }
  }

  CMTime presentation_time_stamp =
      CMTimeMake(frame.render_time_ms(), 1000);
  CFDictionaryRef frame_properties = nullptr;
  if (is_keyframe_required) {
    CFTypeRef keys[] = {kVTEncodeFrameOptionKey_ForceKeyFrame};
    CFTypeRef values[] = {kCFBooleanTrue};
    frame_properties = internal::CreateCFDictionary(keys, values, 1);
  }
  std::unique_ptr<internal::FrameEncodeParams> encode_params;
  encode_params.reset(new internal::FrameEncodeParams(
      this, codec_specific_info, width_, height_, frame.render_time_ms(),
      frame.timestamp(), frame.rotation()));

  encode_params->codec_specific_info.codecSpecific.H264.packetization_mode =
      packetization_mode_;

  // Update the bitrate if needed.
  SetBitrateBps(bitrate_adjuster_.GetAdjustedBitrateBps());

  OSStatus status = VTCompressionSessionEncodeFrame(
      compression_session_, pixel_buffer, presentation_time_stamp,
      kCMTimeInvalid, frame_properties, encode_params.release(), nullptr);
  if (frame_properties) {
    CFRelease(frame_properties);
  }
  if (pixel_buffer) {
    CVBufferRelease(pixel_buffer);
  }
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to encode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::SetChannelParameters(uint32_t packet_loss,
                                                  int64_t rtt) {
  // Encoder doesn't know anything about packet loss or rtt so just return.
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::SetRates(uint32_t new_bitrate_kbit,
                                      uint32_t frame_rate) {
  target_bitrate_bps_ = 1000 * new_bitrate_kbit;
  bitrate_adjuster_.SetTargetBitrateBps(target_bitrate_bps_);
  SetBitrateBps(bitrate_adjuster_.GetAdjustedBitrateBps());
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::Release() {
  // Need to reset so that the session is invalidated and won't use the
  // callback anymore. Do not remove callback until the session is invalidated
  // since async encoder callbacks can occur until invalidation.
  int ret = ResetCompressionSession();
  callback_ = nullptr;
  return ret;
}

int H264VideoToolboxEncoder::ResetCompressionSession() {
  DestroyCompressionSession();

  int status = CreateCompressionSession(compression_session_, internal::VTCompressionOutputCallback, width_, height_, m_h264HardwareEncoderAllowed);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create compression session: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ConfigureCompressionSession();
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxEncoder::CreateCompressionSession(VTCompressionSessionRef& compressionSession, VTCompressionOutputCallback outputCallback, int32_t width, int32_t height, bool useHardwareEncoder) {

  // Set source image buffer attributes. These attributes will be present on
  // buffers retrieved from the encoder's pixel buffer pool.
  const size_t attributes_size = 3;
  CFTypeRef keys[attributes_size] = {
#if defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
    kCVPixelBufferOpenGLCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef io_surface_value =
      internal::CreateCFDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixel_format =
      CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
  CFTypeRef values[attributes_size] = {kCFBooleanTrue, io_surface_value,
                                       pixel_format};
  CFDictionaryRef source_attributes =
      internal::CreateCFDictionary(keys, values, attributes_size);
  if (io_surface_value) {
    CFRelease(io_surface_value);
    io_surface_value = nullptr;
  }
  if (pixel_format) {
    CFRelease(pixel_format);
    pixel_format = nullptr;
  }

#if defined(WEBRTC_USE_VTB_HARDWARE_ENCODER)
  CFTypeRef sessionKeys[] = {kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder};
  CFTypeRef sessionValues[] = { useHardwareEncoder ? kCFBooleanTrue : kCFBooleanFalse };
  CFDictionaryRef encoderSpecification = internal::CreateCFDictionary(sessionKeys, sessionValues, 1);
#else
  CFDictionaryRef encoderSpecification = nullptr;
#endif

  OSStatus status = VTCompressionSessionCreate(
      nullptr,  // use default allocator
      width, height, kCMVideoCodecType_H264,
      encoderSpecification,  // use default encoder
      source_attributes,
      nullptr,  // use default compressed data allocator
      outputCallback, this, &compression_session_);
  if (source_attributes) {
    CFRelease(source_attributes);
    source_attributes = nullptr;
  }

#if defined(WEBRTC_USE_VTB_HARDWARE_ENCODER)
  if (encoderSpecification) {
    CFRelease(encoderSpecification);
    encoderSpecification = nullptr;
  }
#endif
  return status;
}

void H264VideoToolboxEncoder::ConfigureCompressionSession() {
  RTC_DCHECK(compression_session_);
  internal::SetVTSessionProperty(compression_session_,
                                 kVTCompressionPropertyKey_RealTime, true);
  internal::SetVTSessionProperty(compression_session_,
                                 kVTCompressionPropertyKey_ProfileLevel,
                                 profile_);
  internal::SetVTSessionProperty(compression_session_,
                                 kVTCompressionPropertyKey_AllowFrameReordering,
                                 false);
  SetEncoderBitrateBps(target_bitrate_bps_);
  // TODO(tkchin): Look at entropy mode and colorspace matrices.
  // TODO(tkchin): Investigate to see if there's any way to make this work.
  // May need it to interop with Android. Currently this call just fails.
  // On inspecting encoder output on iOS8, this value is set to 6.
  // internal::SetVTSessionProperty(compression_session_,
  //     kVTCompressionPropertyKey_MaxFrameDelayCount,
  //     1);

  // Set a relatively large value for keyframe emission (7200 frames or
  // 4 minutes).
  internal::SetVTSessionProperty(
      compression_session_,
      kVTCompressionPropertyKey_MaxKeyFrameInterval, 7200);
  internal::SetVTSessionProperty(
      compression_session_,
      kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, 240);
}

void H264VideoToolboxEncoder::DestroyCompressionSession() {
  if (compression_session_) {
    VTCompressionSessionInvalidate(compression_session_);
    CFRelease(compression_session_);
    compression_session_ = nullptr;
  }
}

const char* H264VideoToolboxEncoder::ImplementationName() const {
  return "VideoToolbox";
}

bool H264VideoToolboxEncoder::SupportsNativeHandle() const {
  return true;
}

void H264VideoToolboxEncoder::SetBitrateBps(uint32_t bitrate_bps) {
  if (encoder_bitrate_bps_ != bitrate_bps) {
    SetEncoderBitrateBps(bitrate_bps);
  }
}

void H264VideoToolboxEncoder::SetEncoderBitrateBps(uint32_t bitrate_bps) {
  if (compression_session_) {
    internal::SetVTSessionProperty(compression_session_,
                                   kVTCompressionPropertyKey_AverageBitRate,
                                   bitrate_bps);

    // TODO(tkchin): Add a helper method to set array value.
    int64_t data_limit_bytes_per_second_value = static_cast<int64_t>(
        bitrate_bps * internal::kLimitToAverageBitRateFactor / 8);
    CFNumberRef bytes_per_second =
        CFNumberCreate(kCFAllocatorDefault,
                       kCFNumberSInt64Type,
                       &data_limit_bytes_per_second_value);
    int64_t one_second_value = 1;
    CFNumberRef one_second =
        CFNumberCreate(kCFAllocatorDefault,
                       kCFNumberSInt64Type,
                       &one_second_value);
    const void* nums[2] = { bytes_per_second, one_second };
    CFArrayRef data_rate_limits =
        CFArrayCreate(nullptr, nums, 2, &kCFTypeArrayCallBacks);
    OSStatus status =
        VTSessionSetProperty(compression_session_,
                             kVTCompressionPropertyKey_DataRateLimits,
                             data_rate_limits);
    if (bytes_per_second) {
      CFRelease(bytes_per_second);
    }
    if (one_second) {
      CFRelease(one_second);
    }
    if (data_rate_limits) {
      CFRelease(data_rate_limits);
    }
    if (status != noErr) {
      RTC_LOG(LS_ERROR) << "Failed to set data rate limit";
    }

    encoder_bitrate_bps_ = bitrate_bps;
  }
}

void H264VideoToolboxEncoder::OnEncodedFrame(
    OSStatus status,
    VTEncodeInfoFlags info_flags,
    CMSampleBufferRef sample_buffer,
    CodecSpecificInfo codec_specific_info,
    int32_t width,
    int32_t height,
    int64_t render_time_ms,
    uint32_t timestamp,
    VideoRotation rotation) {
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "H264 encode failed.";
    return;
  }
  if (info_flags & kVTEncodeInfo_FrameDropped) {
    RTC_LOG(LS_INFO) << "H264 encode dropped frame.";
    return;
  }

  bool is_keyframe = false;
  CFArrayRef attachments =
      CMSampleBufferGetSampleAttachmentsArray(sample_buffer, 0);
  if (attachments != nullptr && CFArrayGetCount(attachments)) {
    CFDictionaryRef attachment =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
    is_keyframe =
        !CFDictionaryContainsKey(attachment, kCMSampleAttachmentKey_NotSync);
  }

  if (is_keyframe) {
    RTC_LOG(LS_INFO) << "Generated keyframe";
  }

  // Convert the sample buffer into a buffer suitable for RTP packetization.
  // TODO(tkchin): Allocate buffers through a pool.
  std::unique_ptr<rtc::Buffer> buffer(new rtc::Buffer());
  std::unique_ptr<webrtc::RTPFragmentationHeader> header;
  {
    bool result = H264CMSampleBufferToAnnexBBuffer(sample_buffer, is_keyframe,
                                                   buffer.get(), &header);
    if (!result) {
      return;
    }
  }
  webrtc::EncodedImage frame(buffer->data(), buffer->size(), buffer->size());
  frame._encodedWidth = width;
  frame._encodedHeight = height;
  frame._completeFrame = true;
  frame._frameType =
      is_keyframe ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta;
  frame.capture_time_ms_ = render_time_ms;
  frame._timeStamp = timestamp;
  frame.rotation_ = rotation;
  frame.content_type_ =
      (mode_ == kScreensharing) ? VideoContentType::SCREENSHARE : VideoContentType::UNSPECIFIED;

  h264_bitstream_parser_.ParseBitstream(buffer->data(), buffer->size());
  h264_bitstream_parser_.GetLastSliceQp(&frame.qp_);

  EncodedImageCallback::Result res =
      callback_->OnEncodedImage(frame, &codec_specific_info, header.get());
  if (res.error != EncodedImageCallback::Result::OK) {
    RTC_LOG(LS_ERROR) << "Encode callback failed: " << res.error;
    return;
  }
  bitrate_adjuster_.Update(frame._length);
}

VideoEncoder::ScalingSettings H264VideoToolboxEncoder::GetScalingSettings()
    const {
  return VideoEncoder::ScalingSettings(true, internal::kLowH264QpThreshold,
                                       internal::kHighH264QpThreshold);
}
}  // namespace webrtc
