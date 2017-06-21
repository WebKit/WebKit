/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <deque>
#include <memory>
#include <vector>

// NOTICE: androidmediadecoder_jni.h must be included before
// androidmediacodeccommon.h to avoid build errors.
#include "webrtc/sdk/android/src/jni/androidmediadecoder_jni.h"

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_video/h264/h264_bitstream_parser.h"
#include "webrtc/common_video/include/i420_buffer_pool.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/vp8_header_parser.h"
#include "webrtc/sdk/android/src/jni/androidmediacodeccommon.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/native_handle_impl.h"
#include "webrtc/sdk/android/src/jni/surfacetexturehelper_jni.h"

using rtc::Bind;
using rtc::Thread;
using rtc::ThreadManager;

using webrtc::CodecSpecificInfo;
using webrtc::DecodedImageCallback;
using webrtc::EncodedImage;
using webrtc::VideoFrame;
using webrtc::RTPFragmentationHeader;
using webrtc::VideoCodec;
using webrtc::VideoCodecType;
using webrtc::kVideoCodecH264;
using webrtc::kVideoCodecVP8;
using webrtc::kVideoCodecVP9;

namespace webrtc_jni {

// Logging macros.
#define TAG_DECODER "MediaCodecVideoDecoder"
#ifdef TRACK_BUFFER_TIMING
#define ALOGV(...)
  __android_log_print(ANDROID_LOG_VERBOSE, TAG_DECODER, __VA_ARGS__)
#else
#define ALOGV(...)
#endif
#define ALOGD LOG_TAG(rtc::LS_INFO, TAG_DECODER)
#define ALOGW LOG_TAG(rtc::LS_WARNING, TAG_DECODER)
#define ALOGE LOG_TAG(rtc::LS_ERROR, TAG_DECODER)

enum { kMaxWarningLogFrames = 2 };

class MediaCodecVideoDecoder : public webrtc::VideoDecoder,
                               public rtc::MessageHandler {
 public:
  explicit MediaCodecVideoDecoder(
      JNIEnv* jni, VideoCodecType codecType, jobject render_egl_context);
  virtual ~MediaCodecVideoDecoder();

  int32_t InitDecode(const VideoCodec* codecSettings, int32_t numberOfCores)
      override;

  int32_t Decode(
      const EncodedImage& inputImage, bool missingFrames,
      const RTPFragmentationHeader* fragmentation,
      const CodecSpecificInfo* codecSpecificInfo = NULL,
      int64_t renderTimeMs = -1) override;

  int32_t RegisterDecodeCompleteCallback(DecodedImageCallback* callback)
      override;

  int32_t Release() override;

  bool PrefersLateDecoding() const override { return true; }

  // rtc::MessageHandler implementation.
  void OnMessage(rtc::Message* msg) override;

  const char* ImplementationName() const override;

 private:
  // CHECK-fail if not running on |codec_thread_|.
  void CheckOnCodecThread();

  int32_t InitDecodeOnCodecThread();
  int32_t ResetDecodeOnCodecThread();
  int32_t ReleaseOnCodecThread();
  int32_t DecodeOnCodecThread(const EncodedImage& inputImage);
  // Deliver any outputs pending in the MediaCodec to our |callback_| and return
  // true on success.
  bool DeliverPendingOutputs(JNIEnv* jni, int dequeue_timeout_us);
  int32_t ProcessHWErrorOnCodecThread();
  void EnableFrameLogOnWarning();
  void ResetVariables();

  // Type of video codec.
  VideoCodecType codecType_;

  // Render EGL context - owned by factory, should not be allocated/destroyed
  // by VideoDecoder.
  jobject render_egl_context_;

  bool key_frame_required_;
  bool inited_;
  bool sw_fallback_required_;
  bool use_surface_;
  VideoCodec codec_;
  webrtc::I420BufferPool decoded_frame_pool_;
  rtc::scoped_refptr<SurfaceTextureHelper> surface_texture_helper_;
  DecodedImageCallback* callback_;
  int frames_received_;  // Number of frames received by decoder.
  int frames_decoded_;  // Number of frames decoded by decoder.
  // Number of decoded frames for which log information is displayed.
  int frames_decoded_logged_;
  int64_t start_time_ms_;  // Start time for statistics.
  int current_frames_;  // Number of frames in the current statistics interval.
  int current_bytes_;  // Encoded bytes in the current statistics interval.
  int current_decoding_time_ms_;  // Overall decoding time in the current second
  int current_delay_time_ms_;  // Overall delay time in the current second.
  uint32_t max_pending_frames_;  // Maximum number of pending input frames.
  webrtc::H264BitstreamParser h264_bitstream_parser_;
  std::deque<rtc::Optional<uint8_t>> pending_frame_qps_;

  // State that is constant for the lifetime of this object once the ctor
  // returns.
  std::unique_ptr<Thread>
      codec_thread_;  // Thread on which to operate MediaCodec.
  ScopedGlobalRef<jclass> j_media_codec_video_decoder_class_;
  ScopedGlobalRef<jobject> j_media_codec_video_decoder_;
  jmethodID j_init_decode_method_;
  jmethodID j_reset_method_;
  jmethodID j_release_method_;
  jmethodID j_dequeue_input_buffer_method_;
  jmethodID j_queue_input_buffer_method_;
  jmethodID j_dequeue_byte_buffer_method_;
  jmethodID j_dequeue_texture_buffer_method_;
  jmethodID j_return_decoded_byte_buffer_method_;
  // MediaCodecVideoDecoder fields.
  jfieldID j_input_buffers_field_;
  jfieldID j_output_buffers_field_;
  jfieldID j_color_format_field_;
  jfieldID j_width_field_;
  jfieldID j_height_field_;
  jfieldID j_stride_field_;
  jfieldID j_slice_height_field_;
  // MediaCodecVideoDecoder.DecodedTextureBuffer fields.
  jfieldID j_texture_id_field_;
  jfieldID j_transform_matrix_field_;
  jfieldID j_texture_presentation_timestamp_ms_field_;
  jfieldID j_texture_timestamp_ms_field_;
  jfieldID j_texture_ntp_timestamp_ms_field_;
  jfieldID j_texture_decode_time_ms_field_;
  jfieldID j_texture_frame_delay_ms_field_;
  // MediaCodecVideoDecoder.DecodedOutputBuffer fields.
  jfieldID j_info_index_field_;
  jfieldID j_info_offset_field_;
  jfieldID j_info_size_field_;
  jfieldID j_presentation_timestamp_ms_field_;
  jfieldID j_timestamp_ms_field_;
  jfieldID j_ntp_timestamp_ms_field_;
  jfieldID j_byte_buffer_decode_time_ms_field_;

  // Global references; must be deleted in Release().
  std::vector<jobject> input_buffers_;
};

MediaCodecVideoDecoder::MediaCodecVideoDecoder(
    JNIEnv* jni, VideoCodecType codecType, jobject render_egl_context) :
    codecType_(codecType),
    render_egl_context_(render_egl_context),
    key_frame_required_(true),
    inited_(false),
    sw_fallback_required_(false),
    codec_thread_(new Thread()),
    j_media_codec_video_decoder_class_(
        jni,
        FindClass(jni, "org/webrtc/MediaCodecVideoDecoder")),
          j_media_codec_video_decoder_(
              jni,
              jni->NewObject(*j_media_codec_video_decoder_class_,
                   GetMethodID(jni,
                              *j_media_codec_video_decoder_class_,
                              "<init>",
                              "()V"))) {
  codec_thread_->SetName("MediaCodecVideoDecoder", NULL);
  RTC_CHECK(codec_thread_->Start()) << "Failed to start MediaCodecVideoDecoder";

  j_init_decode_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "initDecode",
      "(Lorg/webrtc/MediaCodecVideoDecoder$VideoCodecType;"
      "IILorg/webrtc/SurfaceTextureHelper;)Z");
  j_reset_method_ =
      GetMethodID(jni, *j_media_codec_video_decoder_class_, "reset", "(II)V");
  j_release_method_ =
      GetMethodID(jni, *j_media_codec_video_decoder_class_, "release", "()V");
  j_dequeue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "dequeueInputBuffer", "()I");
  j_queue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "queueInputBuffer", "(IIJJJ)Z");
  j_dequeue_byte_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "dequeueOutputBuffer",
      "(I)Lorg/webrtc/MediaCodecVideoDecoder$DecodedOutputBuffer;");
  j_dequeue_texture_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_decoder_class_, "dequeueTextureBuffer",
      "(I)Lorg/webrtc/MediaCodecVideoDecoder$DecodedTextureBuffer;");
  j_return_decoded_byte_buffer_method_ =
      GetMethodID(jni, *j_media_codec_video_decoder_class_,
                  "returnDecodedOutputBuffer", "(I)V");

  j_input_buffers_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_,
      "inputBuffers", "[Ljava/nio/ByteBuffer;");
  j_output_buffers_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_,
      "outputBuffers", "[Ljava/nio/ByteBuffer;");
  j_color_format_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "colorFormat", "I");
  j_width_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "width", "I");
  j_height_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "height", "I");
  j_stride_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "stride", "I");
  j_slice_height_field_ = GetFieldID(
      jni, *j_media_codec_video_decoder_class_, "sliceHeight", "I");

  jclass j_decoded_texture_buffer_class = FindClass(jni,
      "org/webrtc/MediaCodecVideoDecoder$DecodedTextureBuffer");
  j_texture_id_field_ = GetFieldID(
      jni, j_decoded_texture_buffer_class, "textureID", "I");
  j_transform_matrix_field_ = GetFieldID(
      jni, j_decoded_texture_buffer_class, "transformMatrix", "[F");
  j_texture_presentation_timestamp_ms_field_ = GetFieldID(
      jni, j_decoded_texture_buffer_class, "presentationTimeStampMs", "J");
  j_texture_timestamp_ms_field_ = GetFieldID(
      jni, j_decoded_texture_buffer_class, "timeStampMs", "J");
  j_texture_ntp_timestamp_ms_field_ = GetFieldID(
      jni, j_decoded_texture_buffer_class, "ntpTimeStampMs", "J");
  j_texture_decode_time_ms_field_ = GetFieldID(
      jni, j_decoded_texture_buffer_class, "decodeTimeMs", "J");
  j_texture_frame_delay_ms_field_ = GetFieldID(
      jni, j_decoded_texture_buffer_class, "frameDelayMs", "J");

  jclass j_decoded_output_buffer_class = FindClass(jni,
      "org/webrtc/MediaCodecVideoDecoder$DecodedOutputBuffer");
  j_info_index_field_ = GetFieldID(
      jni, j_decoded_output_buffer_class, "index", "I");
  j_info_offset_field_ = GetFieldID(
      jni, j_decoded_output_buffer_class, "offset", "I");
  j_info_size_field_ = GetFieldID(
      jni, j_decoded_output_buffer_class, "size", "I");
  j_presentation_timestamp_ms_field_ = GetFieldID(
      jni, j_decoded_output_buffer_class, "presentationTimeStampMs", "J");
  j_timestamp_ms_field_ = GetFieldID(
      jni, j_decoded_output_buffer_class, "timeStampMs", "J");
  j_ntp_timestamp_ms_field_ = GetFieldID(
      jni, j_decoded_output_buffer_class, "ntpTimeStampMs", "J");
  j_byte_buffer_decode_time_ms_field_ = GetFieldID(
      jni, j_decoded_output_buffer_class, "decodeTimeMs", "J");

  CHECK_EXCEPTION(jni) << "MediaCodecVideoDecoder ctor failed";
  use_surface_ = (render_egl_context_ != NULL);
  ALOGD << "MediaCodecVideoDecoder ctor. Use surface: " << use_surface_;
  memset(&codec_, 0, sizeof(codec_));
  AllowBlockingCalls();
}

MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
  // Call Release() to ensure no more callbacks to us after we are deleted.
  Release();
}

int32_t MediaCodecVideoDecoder::InitDecode(const VideoCodec* inst,
    int32_t numberOfCores) {
  ALOGD << "InitDecode.";
  if (inst == NULL) {
    ALOGE << "NULL VideoCodec instance";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Factory should guard against other codecs being used with us.
  RTC_CHECK(inst->codecType == codecType_)
      << "Unsupported codec " << inst->codecType << " for " << codecType_;

  if (sw_fallback_required_) {
    ALOGE << "InitDecode() - fallback to SW decoder";
    return WEBRTC_VIDEO_CODEC_OK;
  }
  // Save VideoCodec instance for later.
  if (&codec_ != inst) {
    codec_ = *inst;
  }
  // If maxFramerate is not set then assume 30 fps.
  codec_.maxFramerate = (codec_.maxFramerate >= 1) ? codec_.maxFramerate : 30;

  // Call Java init.
  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE,
      Bind(&MediaCodecVideoDecoder::InitDecodeOnCodecThread, this));
}

void MediaCodecVideoDecoder::ResetVariables() {
  CheckOnCodecThread();

  key_frame_required_ = true;
  frames_received_ = 0;
  frames_decoded_ = 0;
  frames_decoded_logged_ = kMaxDecodedLogFrames;
  start_time_ms_ = rtc::TimeMillis();
  current_frames_ = 0;
  current_bytes_ = 0;
  current_decoding_time_ms_ = 0;
  current_delay_time_ms_ = 0;
  pending_frame_qps_.clear();
}

int32_t MediaCodecVideoDecoder::InitDecodeOnCodecThread() {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  ALOGD << "InitDecodeOnCodecThread Type: " << (int)codecType_ << ". "
      << codec_.width << " x " << codec_.height << ". Fps: " <<
      (int)codec_.maxFramerate;

  // Release previous codec first if it was allocated before.
  int ret_val = ReleaseOnCodecThread();
  if (ret_val < 0) {
    ALOGE << "Release failure: " << ret_val << " - fallback to SW codec";
    sw_fallback_required_ = true;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ResetVariables();

  if (use_surface_) {
    surface_texture_helper_ = SurfaceTextureHelper::create(
        jni, "Decoder SurfaceTextureHelper", render_egl_context_);
    if (!surface_texture_helper_) {
      ALOGE << "Couldn't create SurfaceTextureHelper - fallback to SW codec";
      sw_fallback_required_ = true;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  jobject j_video_codec_enum = JavaEnumFromIndexAndClassName(
      jni, "MediaCodecVideoDecoder$VideoCodecType", codecType_);
  bool success = jni->CallBooleanMethod(
      *j_media_codec_video_decoder_,
      j_init_decode_method_,
      j_video_codec_enum,
      codec_.width,
      codec_.height,
      use_surface_ ? surface_texture_helper_->GetJavaSurfaceTextureHelper()
                   : nullptr);

  if (CheckException(jni) || !success) {
    ALOGE << "Codec initialization error - fallback to SW codec.";
    sw_fallback_required_ = true;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  inited_ = true;

  switch (codecType_) {
    case kVideoCodecVP8:
      max_pending_frames_ = kMaxPendingFramesVp8;
      break;
    case kVideoCodecVP9:
      max_pending_frames_ = kMaxPendingFramesVp9;
      break;
    case kVideoCodecH264:
      max_pending_frames_ = kMaxPendingFramesH264;
      break;
    default:
      max_pending_frames_ = 0;
  }
  ALOGD << "Maximum amount of pending frames: " << max_pending_frames_;

  jobjectArray input_buffers = (jobjectArray)GetObjectField(
      jni, *j_media_codec_video_decoder_, j_input_buffers_field_);
  size_t num_input_buffers = jni->GetArrayLength(input_buffers);
  input_buffers_.resize(num_input_buffers);
  for (size_t i = 0; i < num_input_buffers; ++i) {
    input_buffers_[i] =
        jni->NewGlobalRef(jni->GetObjectArrayElement(input_buffers, i));
    if (CheckException(jni)) {
      ALOGE << "NewGlobalRef error - fallback to SW codec.";
      sw_fallback_required_ = true;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  codec_thread_->PostDelayed(RTC_FROM_HERE, kMediaCodecPollMs, this);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::ResetDecodeOnCodecThread() {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  ALOGD << "ResetDecodeOnCodecThread Type: " << (int)codecType_ << ". "
      << codec_.width << " x " << codec_.height;
  ALOGD << "  Frames received: " << frames_received_ <<
      ". Frames decoded: " << frames_decoded_;

  inited_ = false;
  rtc::MessageQueueManager::Clear(this);
  ResetVariables();

  jni->CallVoidMethod(
      *j_media_codec_video_decoder_,
      j_reset_method_,
      codec_.width,
      codec_.height);

  if (CheckException(jni)) {
    ALOGE << "Soft reset error - fallback to SW codec.";
    sw_fallback_required_ = true;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  inited_ = true;

  codec_thread_->PostDelayed(RTC_FROM_HERE, kMediaCodecPollMs, this);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoDecoder::Release() {
  ALOGD << "DecoderRelease request";
  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE, Bind(&MediaCodecVideoDecoder::ReleaseOnCodecThread, this));
}

int32_t MediaCodecVideoDecoder::ReleaseOnCodecThread() {
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ALOGD << "DecoderReleaseOnCodecThread: Frames received: " <<
      frames_received_ << ". Frames decoded: " << frames_decoded_;
  ScopedLocalRefFrame local_ref_frame(jni);
  for (size_t i = 0; i < input_buffers_.size(); i++) {
    jni->DeleteGlobalRef(input_buffers_[i]);
  }
  input_buffers_.clear();
  jni->CallVoidMethod(*j_media_codec_video_decoder_, j_release_method_);
  surface_texture_helper_ = nullptr;
  inited_ = false;
  rtc::MessageQueueManager::Clear(this);
  if (CheckException(jni)) {
    ALOGE << "Decoder release exception";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  ALOGD << "DecoderReleaseOnCodecThread done";
  return WEBRTC_VIDEO_CODEC_OK;
}

void MediaCodecVideoDecoder::CheckOnCodecThread() {
  RTC_CHECK(codec_thread_.get() == ThreadManager::Instance()->CurrentThread())
      << "Running on wrong thread!";
}

void MediaCodecVideoDecoder::EnableFrameLogOnWarning() {
  // Log next 2 output frames.
  frames_decoded_logged_ = std::max(
      frames_decoded_logged_, frames_decoded_ + kMaxWarningLogFrames);
}

int32_t MediaCodecVideoDecoder::ProcessHWErrorOnCodecThread() {
  CheckOnCodecThread();
  int ret_val = ReleaseOnCodecThread();
  if (ret_val < 0) {
    ALOGE << "ProcessHWError: Release failure";
  }
  if (codecType_ == kVideoCodecH264) {
    // For now there is no SW H.264 which can be used as fallback codec.
    // So try to restart hw codec for now.
    ret_val = InitDecodeOnCodecThread();
    ALOGE << "Reset H.264 codec done. Status: " << ret_val;
    if (ret_val == WEBRTC_VIDEO_CODEC_OK) {
      // H.264 codec was succesfully reset - return regular error code.
      return WEBRTC_VIDEO_CODEC_ERROR;
    } else {
      // Fail to restart H.264 codec - return error code which should stop the
      // call.
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  } else {
    sw_fallback_required_ = true;
    ALOGE << "Return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }
}

int32_t MediaCodecVideoDecoder::Decode(
    const EncodedImage& inputImage,
    bool missingFrames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codecSpecificInfo,
    int64_t renderTimeMs) {
  if (sw_fallback_required_) {
    ALOGE << "Decode() - fallback to SW codec";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }
  if (callback_ == NULL) {
    ALOGE << "Decode() - callback_ is NULL";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (inputImage._buffer == NULL && inputImage._length > 0) {
    ALOGE << "Decode() - inputImage is incorrect";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (!inited_) {
    ALOGE << "Decode() - decoder is not initialized";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // Check if encoded frame dimension has changed.
  if ((inputImage._encodedWidth * inputImage._encodedHeight > 0) &&
      (inputImage._encodedWidth != codec_.width ||
      inputImage._encodedHeight != codec_.height)) {
    ALOGW << "Input resolution changed from " <<
        codec_.width << " x " << codec_.height << " to " <<
        inputImage._encodedWidth << " x " << inputImage._encodedHeight;
    codec_.width = inputImage._encodedWidth;
    codec_.height = inputImage._encodedHeight;
    int32_t ret;
    if (use_surface_ &&
        (codecType_ == kVideoCodecVP8 || codecType_ == kVideoCodecH264)) {
      // Soft codec reset - only for surface decoding.
      ret = codec_thread_->Invoke<int32_t>(
          RTC_FROM_HERE,
          Bind(&MediaCodecVideoDecoder::ResetDecodeOnCodecThread, this));
    } else {
      // Hard codec reset.
      ret = InitDecode(&codec_, 1);
    }
    if (ret < 0) {
      ALOGE << "InitDecode failure: " << ret << " - fallback to SW codec";
      sw_fallback_required_ = true;
      return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    }
  }

  // Always start with a complete key frame.
  if (key_frame_required_) {
    if (inputImage._frameType != webrtc::kVideoFrameKey) {
      ALOGE << "Decode() - key frame is required";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (!inputImage._completeFrame) {
      ALOGE << "Decode() - complete frame is required";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    key_frame_required_ = false;
  }
  if (inputImage._length == 0) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE,
      Bind(&MediaCodecVideoDecoder::DecodeOnCodecThread, this, inputImage));
}

int32_t MediaCodecVideoDecoder::DecodeOnCodecThread(
    const EncodedImage& inputImage) {
  CheckOnCodecThread();
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  // Try to drain the decoder and wait until output is not too
  // much behind the input.
  if (codecType_ == kVideoCodecH264 &&
      frames_received_ > frames_decoded_ + max_pending_frames_) {
    // Print warning for H.264 only - for VP8/VP9 one frame delay is ok.
    ALOGW << "Decoder is too far behind. Try to drain. Received: " <<
        frames_received_ << ". Decoded: " << frames_decoded_;
    EnableFrameLogOnWarning();
  }
  const int64 drain_start = rtc::TimeMillis();
  while ((frames_received_ > frames_decoded_ + max_pending_frames_) &&
         (rtc::TimeMillis() - drain_start) < kMediaCodecTimeoutMs) {
    if (!DeliverPendingOutputs(jni, kMediaCodecPollMs)) {
      ALOGE << "DeliverPendingOutputs error. Frames received: " <<
          frames_received_ << ". Frames decoded: " << frames_decoded_;
      return ProcessHWErrorOnCodecThread();
    }
  }
  if (frames_received_ > frames_decoded_ + max_pending_frames_) {
    ALOGE << "Output buffer dequeue timeout. Frames received: " <<
        frames_received_ << ". Frames decoded: " << frames_decoded_;
    return ProcessHWErrorOnCodecThread();
  }

  // Get input buffer.
  int j_input_buffer_index = jni->CallIntMethod(
      *j_media_codec_video_decoder_, j_dequeue_input_buffer_method_);
  if (CheckException(jni) || j_input_buffer_index < 0) {
    ALOGE << "dequeueInputBuffer error: " << j_input_buffer_index <<
        ". Retry DeliverPendingOutputs.";
    EnableFrameLogOnWarning();
    // Try to drain the decoder.
    if (!DeliverPendingOutputs(jni, kMediaCodecPollMs)) {
      ALOGE << "DeliverPendingOutputs error. Frames received: " <<
          frames_received_ << ". Frames decoded: " << frames_decoded_;
      return ProcessHWErrorOnCodecThread();
    }
    // Try dequeue input buffer one last time.
    j_input_buffer_index = jni->CallIntMethod(
        *j_media_codec_video_decoder_, j_dequeue_input_buffer_method_);
    if (CheckException(jni) || j_input_buffer_index < 0) {
      ALOGE << "dequeueInputBuffer critical error: " << j_input_buffer_index;
      return ProcessHWErrorOnCodecThread();
    }
  }

  // Copy encoded data to Java ByteBuffer.
  jobject j_input_buffer = input_buffers_[j_input_buffer_index];
  uint8_t* buffer =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_input_buffer));
  RTC_CHECK(buffer) << "Indirect buffer??";
  int64_t buffer_capacity = jni->GetDirectBufferCapacity(j_input_buffer);
  if (CheckException(jni) || buffer_capacity < inputImage._length) {
    ALOGE << "Input frame size "<<  inputImage._length <<
        " is bigger than buffer size " << buffer_capacity;
    return ProcessHWErrorOnCodecThread();
  }
  jlong presentation_timestamp_us = static_cast<jlong>(
      static_cast<int64_t>(frames_received_) * 1000000 / codec_.maxFramerate);
  memcpy(buffer, inputImage._buffer, inputImage._length);

  if (frames_decoded_ < frames_decoded_logged_) {
    ALOGD << "Decoder frame in # " << frames_received_ <<
        ". Type: " << inputImage._frameType <<
        ". Buffer # " << j_input_buffer_index <<
        ". TS: " << presentation_timestamp_us / 1000 <<
        ". Size: " << inputImage._length;
  }

  // Save input image timestamps for later output.
  frames_received_++;
  current_bytes_ += inputImage._length;
  rtc::Optional<uint8_t> qp;
  if (codecType_ == kVideoCodecVP8) {
    int qp_int;
    if (webrtc::vp8::GetQp(inputImage._buffer, inputImage._length, &qp_int)) {
      qp = rtc::Optional<uint8_t>(qp_int);
    }
  } else if (codecType_ == kVideoCodecH264) {
    h264_bitstream_parser_.ParseBitstream(inputImage._buffer,
                                          inputImage._length);
    int qp_int;
    if (h264_bitstream_parser_.GetLastSliceQp(&qp_int)) {
      qp = rtc::Optional<uint8_t>(qp_int);
    }
  }
  pending_frame_qps_.push_back(qp);

  // Feed input to decoder.
  bool success = jni->CallBooleanMethod(
      *j_media_codec_video_decoder_,
      j_queue_input_buffer_method_,
      j_input_buffer_index,
      inputImage._length,
      presentation_timestamp_us,
      static_cast<int64_t> (inputImage._timeStamp),
      inputImage.ntp_time_ms_);
  if (CheckException(jni) || !success) {
    ALOGE << "queueInputBuffer error";
    return ProcessHWErrorOnCodecThread();
  }

  // Try to drain the decoder
  if (!DeliverPendingOutputs(jni, 0)) {
    ALOGE << "DeliverPendingOutputs error";
    return ProcessHWErrorOnCodecThread();
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoDecoder::DeliverPendingOutputs(
    JNIEnv* jni, int dequeue_timeout_ms) {
  CheckOnCodecThread();
  if (frames_received_ <= frames_decoded_) {
    // No need to query for output buffers - decoder is drained.
    return true;
  }
  // Get decoder output.
  jobject j_decoder_output_buffer =
      jni->CallObjectMethod(*j_media_codec_video_decoder_,
          use_surface_ ? j_dequeue_texture_buffer_method_
                       : j_dequeue_byte_buffer_method_,
          dequeue_timeout_ms);

  if (CheckException(jni)) {
    ALOGE << "dequeueOutputBuffer() error";
    return false;
  }
  if (IsNull(jni, j_decoder_output_buffer)) {
    // No decoded frame ready.
    return true;
  }

  // Get decoded video frame properties.
  int color_format = GetIntField(jni, *j_media_codec_video_decoder_,
      j_color_format_field_);
  int width = GetIntField(jni, *j_media_codec_video_decoder_, j_width_field_);
  int height = GetIntField(jni, *j_media_codec_video_decoder_, j_height_field_);

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer;
  int64_t presentation_timestamps_ms = 0;
  int64_t output_timestamps_ms = 0;
  int64_t output_ntp_timestamps_ms = 0;
  int decode_time_ms = 0;
  int64_t frame_delayed_ms = 0;
  if (use_surface_) {
    // Extract data from Java DecodedTextureBuffer.
    presentation_timestamps_ms = GetLongField(
        jni, j_decoder_output_buffer,
        j_texture_presentation_timestamp_ms_field_);
    output_timestamps_ms = GetLongField(
        jni, j_decoder_output_buffer, j_texture_timestamp_ms_field_);
    output_ntp_timestamps_ms = GetLongField(
        jni, j_decoder_output_buffer, j_texture_ntp_timestamp_ms_field_);
    decode_time_ms = GetLongField(
        jni, j_decoder_output_buffer, j_texture_decode_time_ms_field_);

    const int texture_id =
        GetIntField(jni, j_decoder_output_buffer, j_texture_id_field_);
    if (texture_id != 0) {  // |texture_id| == 0 represents a dropped frame.
      const jfloatArray j_transform_matrix =
          reinterpret_cast<jfloatArray>(GetObjectField(
              jni, j_decoder_output_buffer, j_transform_matrix_field_));
      frame_delayed_ms = GetLongField(
          jni, j_decoder_output_buffer, j_texture_frame_delay_ms_field_);

      // Create webrtc::VideoFrameBuffer with native texture handle.
      frame_buffer = surface_texture_helper_->CreateTextureFrame(
          width, height, NativeHandleImpl(jni, texture_id, j_transform_matrix));
    } else {
      EnableFrameLogOnWarning();
    }
  } else {
    // Extract data from Java ByteBuffer and create output yuv420 frame -
    // for non surface decoding only.
    int stride =
        GetIntField(jni, *j_media_codec_video_decoder_, j_stride_field_);
    const int slice_height =
        GetIntField(jni, *j_media_codec_video_decoder_, j_slice_height_field_);
    const int output_buffer_index = GetIntField(
        jni, j_decoder_output_buffer, j_info_index_field_);
    const int output_buffer_offset = GetIntField(
        jni, j_decoder_output_buffer, j_info_offset_field_);
    const int output_buffer_size = GetIntField(
        jni, j_decoder_output_buffer, j_info_size_field_);
    presentation_timestamps_ms = GetLongField(
        jni, j_decoder_output_buffer, j_presentation_timestamp_ms_field_);
    output_timestamps_ms = GetLongField(
        jni, j_decoder_output_buffer, j_timestamp_ms_field_);
    output_ntp_timestamps_ms = GetLongField(
        jni, j_decoder_output_buffer, j_ntp_timestamp_ms_field_);

    decode_time_ms = GetLongField(jni, j_decoder_output_buffer,
                                  j_byte_buffer_decode_time_ms_field_);
    RTC_CHECK_GE(slice_height, height);

    if (output_buffer_size < width * height * 3 / 2) {
      ALOGE << "Insufficient output buffer size: " << output_buffer_size;
      return false;
    }
    if (output_buffer_size < stride * height * 3 / 2 &&
        slice_height == height && stride > width) {
      // Some codecs (Exynos) incorrectly report stride information for
      // output byte buffer, so actual stride value need to be corrected.
      stride = output_buffer_size * 2 / (height * 3);
    }
    jobjectArray output_buffers = reinterpret_cast<jobjectArray>(GetObjectField(
        jni, *j_media_codec_video_decoder_, j_output_buffers_field_));
    jobject output_buffer =
        jni->GetObjectArrayElement(output_buffers, output_buffer_index);
    uint8_t* payload = reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(
        output_buffer));
    if (CheckException(jni)) {
      return false;
    }
    payload += output_buffer_offset;

    // Create yuv420 frame.
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer =
        decoded_frame_pool_.CreateBuffer(width, height);
    if (color_format == COLOR_FormatYUV420Planar) {
      RTC_CHECK_EQ(0, stride % 2);
      const int uv_stride = stride / 2;
      const uint8_t* y_ptr = payload;
      const uint8_t* u_ptr = y_ptr + stride * slice_height;

      // Note that the case with odd |slice_height| is handled in a special way.
      // The chroma height contained in the payload is rounded down instead of
      // up, making it one row less than what we expect in WebRTC. Therefore, we
      // have to duplicate the last chroma rows for this case. Also, the offset
      // between the Y plane and the U plane is unintuitive for this case. See
      // http://bugs.webrtc.org/6651 for more info.
      const int chroma_width = (width + 1) / 2;
      const int chroma_height =
          (slice_height % 2 == 0) ? (height + 1) / 2 : height / 2;
      const int u_offset = uv_stride * slice_height / 2;
      const uint8_t* v_ptr = u_ptr + u_offset;
      libyuv::CopyPlane(y_ptr, stride,
                        i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                        width, height);
      libyuv::CopyPlane(u_ptr, uv_stride,
                        i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                        chroma_width, chroma_height);
      libyuv::CopyPlane(v_ptr, uv_stride,
                        i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                        chroma_width, chroma_height);
      if (slice_height % 2 == 1) {
        RTC_CHECK_EQ(height, slice_height);
        // Duplicate the last chroma rows.
        uint8_t* u_last_row_ptr = i420_buffer->MutableDataU() +
                                  chroma_height * i420_buffer->StrideU();
        memcpy(u_last_row_ptr, u_last_row_ptr - i420_buffer->StrideU(),
               i420_buffer->StrideU());
        uint8_t* v_last_row_ptr = i420_buffer->MutableDataV() +
                                  chroma_height * i420_buffer->StrideV();
        memcpy(v_last_row_ptr, v_last_row_ptr - i420_buffer->StrideV(),
               i420_buffer->StrideV());
      }
    } else {
      // All other supported formats are nv12.
      const uint8_t* y_ptr = payload;
      const uint8_t* uv_ptr = y_ptr + stride * slice_height;
      libyuv::NV12ToI420(y_ptr, stride, uv_ptr, stride,
                         i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                         i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                         i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                         width, height);
    }
    frame_buffer = i420_buffer;

    // Return output byte buffer back to codec.
    jni->CallVoidMethod(
        *j_media_codec_video_decoder_,
        j_return_decoded_byte_buffer_method_,
        output_buffer_index);
    if (CheckException(jni)) {
      ALOGE << "returnDecodedOutputBuffer error";
      return false;
    }
  }
  if (frames_decoded_ < frames_decoded_logged_) {
    ALOGD << "Decoder frame out # " << frames_decoded_ <<
        ". " << width << " x " << height <<
        ". Color: " << color_format <<
        ". TS: " << presentation_timestamps_ms <<
        ". DecTime: " << (int)decode_time_ms <<
        ". DelayTime: " << (int)frame_delayed_ms;
  }

  // Calculate and print decoding statistics - every 3 seconds.
  frames_decoded_++;
  current_frames_++;
  current_decoding_time_ms_ += decode_time_ms;
  current_delay_time_ms_ += frame_delayed_ms;
  int statistic_time_ms = rtc::TimeMillis() - start_time_ms_;
  if (statistic_time_ms >= kMediaCodecStatisticsIntervalMs &&
      current_frames_ > 0) {
    int current_bitrate = current_bytes_ * 8 / statistic_time_ms;
    int current_fps =
        (current_frames_ * 1000 + statistic_time_ms / 2) / statistic_time_ms;
    ALOGD << "Frames decoded: " << frames_decoded_ <<
        ". Received: " <<  frames_received_ <<
        ". Bitrate: " << current_bitrate << " kbps" <<
        ". Fps: " << current_fps <<
        ". DecTime: " << (current_decoding_time_ms_ / current_frames_) <<
        ". DelayTime: " << (current_delay_time_ms_ / current_frames_) <<
        " for last " << statistic_time_ms << " ms.";
    start_time_ms_ = rtc::TimeMillis();
    current_frames_ = 0;
    current_bytes_ = 0;
    current_decoding_time_ms_ = 0;
    current_delay_time_ms_ = 0;
  }

  // If the frame was dropped, frame_buffer is left as nullptr.
  if (frame_buffer) {
    VideoFrame decoded_frame(frame_buffer, 0, 0, webrtc::kVideoRotation_0);
    decoded_frame.set_timestamp(output_timestamps_ms);
    decoded_frame.set_ntp_time_ms(output_ntp_timestamps_ms);

    rtc::Optional<uint8_t> qp = pending_frame_qps_.front();
    pending_frame_qps_.pop_front();
    callback_->Decoded(decoded_frame, rtc::Optional<int32_t>(decode_time_ms),
                       qp);
  }
  return true;
}

int32_t MediaCodecVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

void MediaCodecVideoDecoder::OnMessage(rtc::Message* msg) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  if (!inited_) {
    return;
  }
  // We only ever send one message to |this| directly (not through a Bind()'d
  // functor), so expect no ID/data.
  RTC_CHECK(!msg->message_id) << "Unexpected message!";
  RTC_CHECK(!msg->pdata) << "Unexpected message!";
  CheckOnCodecThread();

  if (!DeliverPendingOutputs(jni, 0)) {
    ALOGE << "OnMessage: DeliverPendingOutputs error";
    ProcessHWErrorOnCodecThread();
    return;
  }
  codec_thread_->PostDelayed(RTC_FROM_HERE, kMediaCodecPollMs, this);
}

MediaCodecVideoDecoderFactory::MediaCodecVideoDecoderFactory()
    : egl_context_(nullptr) {
  ALOGD << "MediaCodecVideoDecoderFactory ctor";
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_decoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoDecoder");
  supported_codec_types_.clear();

  bool is_vp8_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isVp8HwSupported", "()Z"));
  if (CheckException(jni)) {
    is_vp8_hw_supported = false;
  }
  if (is_vp8_hw_supported) {
    ALOGD << "VP8 HW Decoder supported.";
    supported_codec_types_.push_back(kVideoCodecVP8);
  }

  bool is_vp9_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isVp9HwSupported", "()Z"));
  if (CheckException(jni)) {
    is_vp9_hw_supported = false;
  }
  if (is_vp9_hw_supported) {
    ALOGD << "VP9 HW Decoder supported.";
    supported_codec_types_.push_back(kVideoCodecVP9);
  }

  bool is_h264_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isH264HwSupported", "()Z"));
  if (CheckException(jni)) {
    is_h264_hw_supported = false;
  }
  if (is_h264_hw_supported) {
    ALOGD << "H264 HW Decoder supported.";
    supported_codec_types_.push_back(kVideoCodecH264);
  }
}

MediaCodecVideoDecoderFactory::~MediaCodecVideoDecoderFactory() {
  ALOGD << "MediaCodecVideoDecoderFactory dtor";
  if (egl_context_) {
    JNIEnv* jni = AttachCurrentThreadIfNeeded();
    jni->DeleteGlobalRef(egl_context_);
  }
}

void MediaCodecVideoDecoderFactory::SetEGLContext(
    JNIEnv* jni, jobject egl_context) {
  ALOGD << "MediaCodecVideoDecoderFactory::SetEGLContext";
  if (egl_context_) {
    jni->DeleteGlobalRef(egl_context_);
    egl_context_ = nullptr;
  }
  egl_context_ = jni->NewGlobalRef(egl_context);
  if (CheckException(jni)) {
    ALOGE << "error calling NewGlobalRef for EGL Context.";
  }
}

webrtc::VideoDecoder* MediaCodecVideoDecoderFactory::CreateVideoDecoder(
    VideoCodecType type) {
  if (supported_codec_types_.empty()) {
    ALOGW << "No HW video decoder for type " << (int)type;
    return nullptr;
  }
  for (VideoCodecType codec_type : supported_codec_types_) {
    if (codec_type == type) {
      ALOGD << "Create HW video decoder for type " << (int)type;
      JNIEnv* jni = AttachCurrentThreadIfNeeded();
      ScopedLocalRefFrame local_ref_frame(jni);
      return new MediaCodecVideoDecoder(jni, type, egl_context_);
    }
  }
  ALOGW << "Can not find HW video decoder for type " << (int)type;
  return nullptr;
}

void MediaCodecVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  ALOGD << "Destroy video decoder.";
  delete decoder;
}

const char* MediaCodecVideoDecoder::ImplementationName() const {
  return "MediaCodec";
}

}  // namespace webrtc_jni
