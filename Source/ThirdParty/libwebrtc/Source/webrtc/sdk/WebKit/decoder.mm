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

#include "webrtc/sdk/WebKit/decoder.h"
#include "VideoToolBoxDecoderFactory.h"

#include <memory>

#include "libyuv/convert.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/common_video/include/video_frame.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/sdk/objc/Framework/Classes/Video/corevideo_frame_buffer.h"
#include "webrtc/sdk/objc/Framework/Classes/VideoToolbox/nalu_rewriter.h"

#if defined(WEBRTC_IOS) && !defined(WEBRTC_WEBKIT_BUILD)
#import "Common/RTCUIApplicationStatusObserver.h"
#endif

namespace webrtc {
namespace {

static const int64_t kMsPerSec = 1000;

// Convenience function for creating a dictionary.
inline CFDictionaryRef CreateCFDictionary(CFTypeRef* keys,
                                          CFTypeRef* values,
                                          size_t size) {
  return CFDictionaryCreate(nullptr, keys, values, size,
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct FrameDecodeParams {
  FrameDecodeParams(DecodedImageCallback* cb, int64_t ts)
      : callback(cb), timestamp(ts) {}
  DecodedImageCallback* callback;
  int64_t timestamp;
};

// This is the callback function that VideoToolbox calls when decode is
// complete.
void VTDecompressionOutputCallback(void* decoder,
                                   void* params,
                                   OSStatus status,
                                   VTDecodeInfoFlags info_flags,
                                   CVImageBufferRef image_buffer,
                                   CMTime timestamp,
                                   CMTime duration) {
  std::unique_ptr<FrameDecodeParams> decode_params(
      reinterpret_cast<FrameDecodeParams*>(params));
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }
  // TODO(tkchin): Handle CVO properly.
  rtc::scoped_refptr<VideoFrameBuffer> buffer =
      new rtc::RefCountedObject<CoreVideoFrameBuffer>(image_buffer);
  VideoFrame decoded_frame(buffer, decode_params->timestamp,
                           CMTimeGetSeconds(timestamp) * kMsPerSec,
                           kVideoRotation_0);
  decode_params->callback->Decoded(decoded_frame);
}

}  // namespace

H264VideoToolboxDecoder::H264VideoToolboxDecoder(VideoToolboxVideoDecoderFactory& factory)
    : callback_(nullptr), video_format_(nullptr), decompression_session_(nullptr), factory_(&factory) {
  factory_->Add(*this);
}

H264VideoToolboxDecoder::~H264VideoToolboxDecoder() {
  DestroyDecompressionSession();
  SetVideoFormat(nullptr);
  if (factory_)
    factory_->Remove(*this);
}

void H264VideoToolboxDecoder::ClearFactory() {
  factory_ = nullptr;
}

int H264VideoToolboxDecoder::InitDecode(const VideoCodec* video_codec,
                                        int number_of_cores) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxDecoder::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  RTC_DCHECK(input_image._buffer);

#if defined(WEBRTC_IOS)
#if !defined(WEBRTC_WEBKIT_BUILD)
  if (![[RTCUIApplicationStatusObserver sharedInstance] isApplicationActive]) {
#else
  if (!is_active_) {
#endif
    // Ignore all decode requests when app isn't active. In this state, the
    // hardware decoder has been invalidated by the OS.
    // Reset video format so that we won't process frames until the next
    // keyframe.
    SetVideoFormat(nullptr);
    return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
  }
#endif
  CMVideoFormatDescriptionRef input_format = nullptr;
  if (H264AnnexBBufferHasVideoFormatDescription(input_image._buffer,
                                                input_image._length)) {
    input_format = CreateVideoFormatDescription(input_image._buffer,
                                                input_image._length);
    if (input_format) {
      // Check if the video format has changed, and reinitialize decoder if
      // needed.
      if (!CMFormatDescriptionEqual(input_format, video_format_)) {
        SetVideoFormat(input_format);
        ResetDecompressionSession();
      }
      CFRelease(input_format);
    }
  }
  if (!video_format_) {
    // We received a frame but we don't have format information so we can't
    // decode it.
    // This can happen after backgrounding. We need to wait for the next
    // sps/pps before we can resume so we request a keyframe by returning an
    // error.
    RTC_LOG(LS_WARNING) << "Missing video format. Frame with sps/pps required.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  CMSampleBufferRef sample_buffer = nullptr;
  if (!H264AnnexBBufferToCMSampleBuffer(input_image._buffer,
                                        input_image._length, video_format_,
                                        &sample_buffer)) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sample_buffer);
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  std::unique_ptr<FrameDecodeParams> frame_decode_params;
  frame_decode_params.reset(
      new FrameDecodeParams(callback_, input_image._timeStamp));
  OSStatus status = VTDecompressionSessionDecodeFrame(
      decompression_session_, sample_buffer, decode_flags,
      frame_decode_params.release(), nullptr);
#if defined(WEBRTC_IOS)
  // Re-initialize the decoder if we have an invalid session while the app is
  // active and retry the decode request.
  if (status == kVTInvalidSessionErr &&
      ResetDecompressionSession() == WEBRTC_VIDEO_CODEC_OK) {
    frame_decode_params.reset(
        new FrameDecodeParams(callback_, input_image._timeStamp));
    status = VTDecompressionSessionDecodeFrame(
        decompression_session_, sample_buffer, decode_flags,
        frame_decode_params.release(), nullptr);
  }
#endif
  CFRelease(sample_buffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to decode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  RTC_DCHECK(!callback_);
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxDecoder::Release() {
  // Need to invalidate the session so that callbacks no longer occur and it
  // is safe to null out the callback.
  DestroyDecompressionSession();
  SetVideoFormat(nullptr);
  callback_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

int H264VideoToolboxDecoder::ResetDecompressionSession() {
  DestroyDecompressionSession();

  // Need to wait for the first SPS to initialize decoder.
  if (!video_format_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Set keys for OpenGL and IOSurface compatibilty, which makes the encoder
  // create pixel buffers with GPU backed memory. The intent here is to pass
  // the pixel buffers directly so we avoid a texture upload later during
  // rendering. This currently is moot because we are converting back to an
  // I420 frame after decode, but eventually we will be able to plumb
  // CVPixelBuffers directly to the renderer.
  // TODO(tkchin): Maybe only set OpenGL/IOSurface keys if we know that that
  // we can pass CVPixelBuffers as native handles in decoder output.
  static size_t const attributes_size = 3;
  CFTypeRef keys[attributes_size] = {
#if defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
    kCVPixelBufferOpenGLCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef io_surface_value = CreateCFDictionary(nullptr, nullptr, 0);
  int64_t nv12type = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
  CFNumberRef pixel_format =
      CFNumberCreate(nullptr, kCFNumberLongType, &nv12type);
  CFTypeRef values[attributes_size] = {kCFBooleanTrue, io_surface_value,
                                       pixel_format};
  CFDictionaryRef attributes =
      CreateCFDictionary(keys, values, attributes_size);
  if (io_surface_value) {
    CFRelease(io_surface_value);
    io_surface_value = nullptr;
  }
  if (pixel_format) {
    CFRelease(pixel_format);
    pixel_format = nullptr;
  }
  VTDecompressionOutputCallbackRecord record = {
      VTDecompressionOutputCallback, this,
  };
  OSStatus status =
      VTDecompressionSessionCreate(nullptr, video_format_, nullptr, attributes,
                                   &record, &decompression_session_);
  CFRelease(attributes);
  if (status != noErr) {
    DestroyDecompressionSession();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ConfigureDecompressionSession();

  return WEBRTC_VIDEO_CODEC_OK;
}

void H264VideoToolboxDecoder::ConfigureDecompressionSession() {
  RTC_DCHECK(decompression_session_);
#if defined(WEBRTC_IOS)
  VTSessionSetProperty(decompression_session_,
                       kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

void H264VideoToolboxDecoder::DestroyDecompressionSession() {
  if (decompression_session_) {
    VTDecompressionSessionInvalidate(decompression_session_);
    CFRelease(decompression_session_);
    decompression_session_ = nullptr;
  }
}

void H264VideoToolboxDecoder::SetVideoFormat(
    CMVideoFormatDescriptionRef video_format) {
  if (video_format_ == video_format) {
    return;
  }
  if (video_format_) {
    CFRelease(video_format_);
  }
  video_format_ = video_format;
  if (video_format_) {
    CFRetain(video_format_);
  }
}

const char* H264VideoToolboxDecoder::ImplementationName() const {
  return "VideoToolbox";
}

}  // namespace webrtc
