/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// NOTICE: androidmediaencoder_jni.h must be included before
// androidmediacodeccommon.h to avoid build errors.
#include "webrtc/sdk/android/src/jni/androidmediaencoder_jni.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "webrtc/api/video_codecs/video_encoder.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/random.h"
#include "webrtc/base/sequenced_task_checker.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/weak_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/h264/h264_bitstream_parser.h"
#include "webrtc/common_video/h264/h264_common.h"
#include "webrtc/common_video/h264/profile_level_id.h"
#include "webrtc/media/engine/internalencoderfactory.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"
#include "webrtc/modules/video_coding/utility/vp8_header_parser.h"
#include "webrtc/modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "webrtc/sdk/android/src/jni/androidmediacodeccommon.h"
#include "webrtc/sdk/android/src/jni/classreferenceholder.h"
#include "webrtc/sdk/android/src/jni/native_handle_impl.h"
#include "webrtc/system_wrappers/include/field_trial.h"

using rtc::Bind;
using rtc::Thread;
using rtc::ThreadManager;

using webrtc::CodecSpecificInfo;
using webrtc::EncodedImage;
using webrtc::VideoFrame;
using webrtc::RTPFragmentationHeader;
using webrtc::VideoCodec;
using webrtc::VideoCodecType;
using webrtc::kVideoCodecH264;
using webrtc::kVideoCodecVP8;
using webrtc::kVideoCodecVP9;
using webrtc::QualityScaler;

namespace webrtc_jni {

// Maximum supported HW video encoder fps.
#define MAX_VIDEO_FPS 30
// Maximum allowed fps value in SetRates() call.
#define MAX_ALLOWED_VIDEO_FPS 60
// Maximum allowed frames in encoder input queue.
#define MAX_ENCODER_Q_SIZE 2
// Maximum amount of dropped frames caused by full encoder queue - exceeding
// this threshold means that encoder probably got stuck and need to be reset.
#define ENCODER_STALL_FRAMEDROP_THRESHOLD 60

// Logging macros.
#define TAG_ENCODER "MediaCodecVideoEncoder"
#ifdef TRACK_BUFFER_TIMING
#define ALOGV(...)
  __android_log_print(ANDROID_LOG_VERBOSE, TAG_ENCODER, __VA_ARGS__)
#else
#define ALOGV(...)
#endif
#define ALOGD LOG_TAG(rtc::LS_INFO, TAG_ENCODER)
#define ALOGW LOG_TAG(rtc::LS_WARNING, TAG_ENCODER)
#define ALOGE LOG_TAG(rtc::LS_ERROR, TAG_ENCODER)

namespace {
// Maximum time limit between incoming frames before requesting a key frame.
const size_t kFrameDiffThresholdMs = 350;
const int kMinKeyFrameInterval = 6;
const char kH264HighProfileFieldTrial[] = "WebRTC-H264HighProfile";
const char kCustomQPThresholdsFieldTrial[] = "WebRTC-CustomQPThresholds";
}  // namespace

// MediaCodecVideoEncoder is a webrtc::VideoEncoder implementation that uses
// Android's MediaCodec SDK API behind the scenes to implement (hopefully)
// HW-backed video encode.  This C++ class is implemented as a very thin shim,
// delegating all of the interesting work to org.webrtc.MediaCodecVideoEncoder.
// MediaCodecVideoEncoder must be operated on a single task queue, currently
// this is the encoder queue from ViE encoder.
class MediaCodecVideoEncoder : public webrtc::VideoEncoder {
 public:
  virtual ~MediaCodecVideoEncoder();
  MediaCodecVideoEncoder(JNIEnv* jni,
                         const cricket::VideoCodec& codec,
                         jobject egl_context);

  // webrtc::VideoEncoder implementation.
  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     int32_t /* number_of_cores */,
                     size_t /* max_payload_size */) override;
  int32_t Encode(const webrtc::VideoFrame& input_image,
                 const webrtc::CodecSpecificInfo* /* codec_specific_info */,
                 const std::vector<webrtc::FrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t SetChannelParameters(uint32_t /* packet_loss */,
                               int64_t /* rtt */) override;
  int32_t SetRateAllocation(const webrtc::BitrateAllocation& rate_allocation,
                            uint32_t frame_rate) override;

  bool SupportsNativeHandle() const override { return egl_context_ != nullptr; }
  const char* ImplementationName() const override;

 private:
  class EncodeTask : public rtc::QueuedTask {
   public:
    explicit EncodeTask(rtc::WeakPtr<MediaCodecVideoEncoder> encoder);
    bool Run() override;

   private:
    rtc::WeakPtr<MediaCodecVideoEncoder> encoder_;
  };

  // ResetCodec() calls Release() and InitEncodeInternal() in an attempt to
  // restore the codec to an operable state. Necessary after all manner of
  // OMX-layer errors. Returns true if the codec was reset successfully.
  bool ResetCodec();

  // Fallback to a software encoder if one is supported else try to reset the
  // encoder. Called with |reset_if_fallback_unavailable| equal to false from
  // init/release encoder so that we don't go into infinite recursion.
  // Returns true if the codec was reset successfully.
  bool ProcessHWError(bool reset_if_fallback_unavailable);

  // Calls ProcessHWError(true). Returns WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE if
  // sw_fallback_required_ was set or WEBRTC_VIDEO_CODEC_ERROR otherwise.
  int32_t ProcessHWErrorOnEncode();

  // If width==0 then this is assumed to be a re-initialization and the
  // previously-current values are reused instead of the passed parameters
  // (makes it easier to reason about thread-safety).
  int32_t InitEncodeInternal(int width,
                             int height,
                             int kbps,
                             int fps,
                             bool use_surface);
  // Reconfigure to match |frame| in width, height. Also reconfigures the
  // encoder if |frame| is a texture/byte buffer and the encoder is initialized
  // for byte buffer/texture. Returns false if reconfiguring fails.
  bool MaybeReconfigureEncoder(const webrtc::VideoFrame& frame);
  bool EncodeByteBuffer(JNIEnv* jni,
                        bool key_frame,
                        const webrtc::VideoFrame& frame,
                        int input_buffer_index);
  bool EncodeTexture(JNIEnv* jni,
                     bool key_frame,
                     const webrtc::VideoFrame& frame);

  // Helper accessors for MediaCodecVideoEncoder$OutputBufferInfo members.
  int GetOutputBufferInfoIndex(JNIEnv* jni, jobject j_output_buffer_info);
  jobject GetOutputBufferInfoBuffer(JNIEnv* jni, jobject j_output_buffer_info);
  bool GetOutputBufferInfoIsKeyFrame(JNIEnv* jni, jobject j_output_buffer_info);
  jlong GetOutputBufferInfoPresentationTimestampUs(
      JNIEnv* jni, jobject j_output_buffer_info);

  // Deliver any outputs pending in the MediaCodec to our |callback_| and return
  // true on success.
  bool DeliverPendingOutputs(JNIEnv* jni);

  VideoEncoder::ScalingSettings GetScalingSettings() const override;

  // Displays encoder statistics.
  void LogStatistics(bool force_log);

  VideoCodecType GetCodecType() const;

#if RTC_DCHECK_IS_ON
  // Mutex for protecting inited_. It is only used for correctness checking on
  // debug build. It is used for checking that encoder has been released in the
  // destructor. Because this might happen on a different thread, we need a
  // mutex.
  rtc::CriticalSection inited_crit_;
#endif

  // Type of video codec.
  const cricket::VideoCodec codec_;

  webrtc::EncodedImageCallback* callback_;

  // State that is constant for the lifetime of this object once the ctor
  // returns.
  rtc::SequencedTaskChecker encoder_queue_checker_;
  ScopedGlobalRef<jclass> j_media_codec_video_encoder_class_;
  ScopedGlobalRef<jobject> j_media_codec_video_encoder_;
  jmethodID j_init_encode_method_;
  jmethodID j_get_input_buffers_method_;
  jmethodID j_dequeue_input_buffer_method_;
  jmethodID j_encode_buffer_method_;
  jmethodID j_encode_texture_method_;
  jmethodID j_release_method_;
  jmethodID j_set_rates_method_;
  jmethodID j_dequeue_output_buffer_method_;
  jmethodID j_release_output_buffer_method_;
  jfieldID j_color_format_field_;
  jfieldID j_info_index_field_;
  jfieldID j_info_buffer_field_;
  jfieldID j_info_is_key_frame_field_;
  jfieldID j_info_presentation_timestamp_us_field_;

  // State that is valid only between InitEncode() and the next Release().
  int width_;   // Frame width in pixels.
  int height_;  // Frame height in pixels.
  bool inited_;
  bool use_surface_;
  enum libyuv::FourCC encoder_fourcc_;  // Encoder color space format.
  int last_set_bitrate_kbps_;  // Last-requested bitrate in kbps.
  int last_set_fps_;  // Last-requested frame rate.
  int64_t current_timestamp_us_;  // Current frame timestamps in us.
  int frames_received_;  // Number of frames received by encoder.
  int frames_encoded_;  // Number of frames encoded by encoder.
  int frames_dropped_media_encoder_;  // Number of frames dropped by encoder.
  // Number of dropped frames caused by full queue.
  int consecutive_full_queue_frame_drops_;
  int64_t stat_start_time_ms_;  // Start time for statistics.
  int current_frames_;  // Number of frames in the current statistics interval.
  int current_bytes_;  // Encoded bytes in the current statistics interval.
  int current_acc_qp_;  // Accumulated QP in the current statistics interval.
  int current_encoding_time_ms_;  // Overall encoding time in the current second
  int64_t last_input_timestamp_ms_;  // Timestamp of last received yuv frame.
  int64_t last_output_timestamp_ms_;  // Timestamp of last encoded frame.
  // Holds the task while the polling loop is paused.
  std::unique_ptr<rtc::QueuedTask> encode_task_;

  struct InputFrameInfo {
    InputFrameInfo(int64_t encode_start_time,
                   int32_t frame_timestamp,
                   int64_t frame_render_time_ms,
                   webrtc::VideoRotation rotation)
        : encode_start_time(encode_start_time),
          frame_timestamp(frame_timestamp),
          frame_render_time_ms(frame_render_time_ms),
          rotation(rotation) {}
    // Time when video frame is sent to encoder input.
    const int64_t encode_start_time;

    // Input frame information.
    const int32_t frame_timestamp;
    const int64_t frame_render_time_ms;
    const webrtc::VideoRotation rotation;
  };
  std::list<InputFrameInfo> input_frame_infos_;
  int32_t output_timestamp_;       // Last output frame timestamp from
                                   // |input_frame_infos_|.
  int64_t output_render_time_ms_;  // Last output frame render time from
                                   // |input_frame_infos_|.
  webrtc::VideoRotation output_rotation_;  // Last output frame rotation from
                                           // |input_frame_infos_|.

  // Frame size in bytes fed to MediaCodec.
  int yuv_size_;
  // True only when between a callback_->OnEncodedImage() call return a positive
  // value and the next Encode() call being ignored.
  bool drop_next_input_frame_;
  bool scale_;
  webrtc::H264::Profile profile_;
  // Global references; must be deleted in Release().
  std::vector<jobject> input_buffers_;
  webrtc::H264BitstreamParser h264_bitstream_parser_;

  // VP9 variables to populate codec specific structure.
  webrtc::GofInfoVP9 gof_;  // Contains each frame's temporal information for
                            // non-flexible VP9 mode.
  size_t gof_idx_;

  // EGL context - owned by factory, should not be allocated/destroyed
  // by MediaCodecVideoEncoder.
  jobject egl_context_;

  // Temporary fix for VP8.
  // Sends a key frame if frames are largely spaced apart (possibly
  // corresponding to a large image change).
  int64_t last_frame_received_ms_;
  int frames_received_since_last_key_;
  webrtc::VideoCodecMode codec_mode_;

  // RTP state.
  uint16_t picture_id_;
  uint8_t tl0_pic_idx_;

  bool sw_fallback_required_;

  // All other member variables should be before WeakPtrFactory. Valid only from
  // InitEncode to Release.
  std::unique_ptr<rtc::WeakPtrFactory<MediaCodecVideoEncoder>> weak_factory_;
};

MediaCodecVideoEncoder::~MediaCodecVideoEncoder() {
#if RTC_DCHECK_IS_ON
  rtc::CritScope lock(&inited_crit_);
  RTC_DCHECK(!inited_);
#endif
}

MediaCodecVideoEncoder::MediaCodecVideoEncoder(JNIEnv* jni,
                                               const cricket::VideoCodec& codec,
                                               jobject egl_context)
    : codec_(codec),
      callback_(NULL),
      j_media_codec_video_encoder_class_(
          jni,
          FindClass(jni, "org/webrtc/MediaCodecVideoEncoder")),
      j_media_codec_video_encoder_(
          jni,
          jni->NewObject(*j_media_codec_video_encoder_class_,
                         GetMethodID(jni,
                                     *j_media_codec_video_encoder_class_,
                                     "<init>",
                                     "()V"))),
      inited_(false),
      use_surface_(false),
      egl_context_(egl_context),
      sw_fallback_required_(false) {
  encoder_queue_checker_.Detach();

  jclass j_output_buffer_info_class =
      FindClass(jni, "org/webrtc/MediaCodecVideoEncoder$OutputBufferInfo");
  j_init_encode_method_ =
      GetMethodID(jni, *j_media_codec_video_encoder_class_, "initEncode",
                  "(Lorg/webrtc/MediaCodecVideoEncoder$VideoCodecType;"
                  "IIIIILorg/webrtc/EglBase14$Context;)Z");
  j_get_input_buffers_method_ = GetMethodID(
      jni,
      *j_media_codec_video_encoder_class_,
      "getInputBuffers",
      "()[Ljava/nio/ByteBuffer;");
  j_dequeue_input_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "dequeueInputBuffer", "()I");
  j_encode_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "encodeBuffer", "(ZIIJ)Z");
  j_encode_texture_method_ = GetMethodID(
        jni, *j_media_codec_video_encoder_class_, "encodeTexture",
        "(ZI[FJ)Z");
  j_release_method_ =
      GetMethodID(jni, *j_media_codec_video_encoder_class_, "release", "()V");
  j_set_rates_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "setRates", "(II)Z");
  j_dequeue_output_buffer_method_ = GetMethodID(
      jni,
      *j_media_codec_video_encoder_class_,
      "dequeueOutputBuffer",
      "()Lorg/webrtc/MediaCodecVideoEncoder$OutputBufferInfo;");
  j_release_output_buffer_method_ = GetMethodID(
      jni, *j_media_codec_video_encoder_class_, "releaseOutputBuffer", "(I)Z");

  j_color_format_field_ =
      GetFieldID(jni, *j_media_codec_video_encoder_class_, "colorFormat", "I");
  j_info_index_field_ =
      GetFieldID(jni, j_output_buffer_info_class, "index", "I");
  j_info_buffer_field_ = GetFieldID(
      jni, j_output_buffer_info_class, "buffer", "Ljava/nio/ByteBuffer;");
  j_info_is_key_frame_field_ =
      GetFieldID(jni, j_output_buffer_info_class, "isKeyFrame", "Z");
  j_info_presentation_timestamp_us_field_ = GetFieldID(
      jni, j_output_buffer_info_class, "presentationTimestampUs", "J");
  if (CheckException(jni)) {
    ALOGW << "MediaCodecVideoEncoder ctor failed.";
    ProcessHWError(true /* reset_if_fallback_unavailable */);
  }

  webrtc::Random random(rtc::TimeMicros());
  picture_id_ = random.Rand<uint16_t>() & 0x7FFF;
  tl0_pic_idx_ = random.Rand<uint8_t>();
}

int32_t MediaCodecVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    int32_t /* number_of_cores */,
    size_t /* max_payload_size */) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  if (codec_settings == NULL) {
    ALOGE << "NULL VideoCodec instance";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Factory should guard against other codecs being used with us.
  const VideoCodecType codec_type = GetCodecType();
  RTC_CHECK(codec_settings->codecType == codec_type)
      << "Unsupported codec " << codec_settings->codecType << " for "
      << codec_type;
  if (sw_fallback_required_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  codec_mode_ = codec_settings->mode;
  int init_width = codec_settings->width;
  int init_height = codec_settings->height;
  // Scaling is optionally enabled for VP8 and VP9.
  // TODO(pbos): Extract automaticResizeOn out of VP8 settings.
  scale_ = false;
  if (codec_type == kVideoCodecVP8) {
    scale_ = codec_settings->VP8().automaticResizeOn;
  } else if (codec_type == kVideoCodecVP9) {
    scale_ = codec_settings->VP9().automaticResizeOn;
  } else {
    scale_ = true;
  }

  ALOGD << "InitEncode request: " << init_width << " x " << init_height;
  ALOGD << "Encoder automatic resize " << (scale_ ? "enabled" : "disabled");

  // Check allowed H.264 profile
  profile_ = webrtc::H264::Profile::kProfileBaseline;
  if (codec_type == kVideoCodecH264) {
    const rtc::Optional<webrtc::H264::ProfileLevelId> profile_level_id =
        webrtc::H264::ParseSdpProfileLevelId(codec_.params);
    RTC_DCHECK(profile_level_id);
    profile_ = profile_level_id->profile;
    ALOGD << "H.264 profile: " << profile_;
  }

  return InitEncodeInternal(
      init_width, init_height, codec_settings->startBitrate,
      codec_settings->maxFramerate, codec_settings->expect_encode_from_texture);
}

int32_t MediaCodecVideoEncoder::SetChannelParameters(uint32_t /* packet_loss */,
                                                     int64_t /* rtt */) {
  return WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoEncoder::ResetCodec() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  ALOGE << "Reset";
  if (Release() != WEBRTC_VIDEO_CODEC_OK) {
    ALOGE << "Releasing codec failed during reset.";
    return false;
  }
  if (InitEncodeInternal(width_, height_, 0, 0, false) !=
      WEBRTC_VIDEO_CODEC_OK) {
    ALOGE << "Initializing encoder failed during reset.";
    return false;
  }
  return true;
}

MediaCodecVideoEncoder::EncodeTask::EncodeTask(
    rtc::WeakPtr<MediaCodecVideoEncoder> encoder)
    : encoder_(encoder) {}

bool MediaCodecVideoEncoder::EncodeTask::Run() {
  if (!encoder_) {
    // Encoder was destroyed.
    return true;
  }

  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_->encoder_queue_checker_);
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  if (!encoder_->inited_) {
    encoder_->encode_task_ = std::unique_ptr<rtc::QueuedTask>(this);
    return false;
  }

  // It would be nice to recover from a failure here if one happened, but it's
  // unclear how to signal such a failure to the app, so instead we stay silent
  // about it and let the next app-called API method reveal the borkedness.
  encoder_->DeliverPendingOutputs(jni);

  if (!encoder_) {
    // Encoder can be destroyed in DeliverPendingOutputs.
    return true;
  }

  // Call log statistics here so it's called even if no frames are being
  // delivered.
  encoder_->LogStatistics(false);

  // If there aren't more frames to deliver, we can start polling at lower rate.
  if (encoder_->input_frame_infos_.empty()) {
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kMediaCodecPollNoFramesMs);
  } else {
    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kMediaCodecPollMs);
  }

  return false;
}

bool MediaCodecVideoEncoder::ProcessHWError(
    bool reset_if_fallback_unavailable) {
  ALOGE << "ProcessHWError";
  if (FindMatchingCodec(cricket::InternalEncoderFactory().supported_codecs(),
                        codec_)) {
    ALOGE << "Fallback to SW encoder.";
    sw_fallback_required_ = true;
    return false;
  } else if (reset_if_fallback_unavailable) {
    ALOGE << "Reset encoder.";
    return ResetCodec();
  }
  return false;
}

int32_t MediaCodecVideoEncoder::ProcessHWErrorOnEncode() {
  ProcessHWError(true /* reset_if_fallback_unavailable */);
  return sw_fallback_required_ ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                               : WEBRTC_VIDEO_CODEC_ERROR;
}

VideoCodecType MediaCodecVideoEncoder::GetCodecType() const {
  return webrtc::PayloadNameToCodecType(codec_.name)
      .value_or(webrtc::kVideoCodecUnknown);
}

int32_t MediaCodecVideoEncoder::InitEncodeInternal(int width,
                                                   int height,
                                                   int kbps,
                                                   int fps,
                                                   bool use_surface) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  if (sw_fallback_required_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  RTC_CHECK(!use_surface || egl_context_ != nullptr) << "EGL context not set.";
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  const VideoCodecType codec_type = GetCodecType();
  ALOGD << "InitEncodeInternal Type: " << static_cast<int>(codec_type) << ", "
        << width << " x " << height << ". Bitrate: " << kbps
        << " kbps. Fps: " << fps << ". Profile: " << profile_ << ".";
  if (kbps == 0) {
    kbps = last_set_bitrate_kbps_;
  }
  if (fps == 0) {
    fps = MAX_VIDEO_FPS;
  }

  width_ = width;
  height_ = height;
  last_set_bitrate_kbps_ = kbps;
  last_set_fps_ = (fps < MAX_VIDEO_FPS) ? fps : MAX_VIDEO_FPS;
  yuv_size_ = width_ * height_ * 3 / 2;
  frames_received_ = 0;
  frames_encoded_ = 0;
  frames_dropped_media_encoder_ = 0;
  consecutive_full_queue_frame_drops_ = 0;
  current_timestamp_us_ = 0;
  stat_start_time_ms_ = rtc::TimeMillis();
  current_frames_ = 0;
  current_bytes_ = 0;
  current_acc_qp_ = 0;
  current_encoding_time_ms_ = 0;
  last_input_timestamp_ms_ = -1;
  last_output_timestamp_ms_ = -1;
  output_timestamp_ = 0;
  output_render_time_ms_ = 0;
  input_frame_infos_.clear();
  drop_next_input_frame_ = false;
  use_surface_ = use_surface;
  gof_.SetGofInfoVP9(webrtc::TemporalStructureMode::kTemporalStructureMode1);
  gof_idx_ = 0;
  last_frame_received_ms_ = -1;
  frames_received_since_last_key_ = kMinKeyFrameInterval;

  // We enforce no extra stride/padding in the format creation step.
  jobject j_video_codec_enum = JavaEnumFromIndexAndClassName(
      jni, "MediaCodecVideoEncoder$VideoCodecType", codec_type);
  const bool encode_status = jni->CallBooleanMethod(
      *j_media_codec_video_encoder_, j_init_encode_method_, j_video_codec_enum,
      profile_, width, height, kbps, fps,
      (use_surface ? egl_context_ : nullptr));
  if (!encode_status) {
    ALOGE << "Failed to configure encoder.";
    ProcessHWError(false /* reset_if_fallback_unavailable */);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (CheckException(jni)) {
    ALOGE << "Exception in init encode.";
    ProcessHWError(false /* reset_if_fallback_unavailable */);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (!use_surface) {
    jobjectArray input_buffers = reinterpret_cast<jobjectArray>(
        jni->CallObjectMethod(*j_media_codec_video_encoder_,
            j_get_input_buffers_method_));
    if (CheckException(jni)) {
      ALOGE << "Exception in get input buffers.";
      ProcessHWError(false /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (IsNull(jni, input_buffers)) {
      ProcessHWError(false /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    switch (GetIntField(jni, *j_media_codec_video_encoder_,
        j_color_format_field_)) {
      case COLOR_FormatYUV420Planar:
        encoder_fourcc_ = libyuv::FOURCC_YU12;
        break;
      case COLOR_FormatYUV420SemiPlanar:
      case COLOR_QCOM_FormatYUV420SemiPlanar:
      case COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m:
        encoder_fourcc_ = libyuv::FOURCC_NV12;
        break;
      default:
        LOG(LS_ERROR) << "Wrong color format.";
        ProcessHWError(false /* reset_if_fallback_unavailable */);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }
    size_t num_input_buffers = jni->GetArrayLength(input_buffers);
    RTC_CHECK(input_buffers_.empty())
        << "Unexpected double InitEncode without Release";
    input_buffers_.resize(num_input_buffers);
    for (size_t i = 0; i < num_input_buffers; ++i) {
      input_buffers_[i] =
          jni->NewGlobalRef(jni->GetObjectArrayElement(input_buffers, i));
      int64_t yuv_buffer_capacity =
          jni->GetDirectBufferCapacity(input_buffers_[i]);
      if (CheckException(jni)) {
        ALOGE << "Exception in get direct buffer capacity.";
        ProcessHWError(false /* reset_if_fallback_unavailable */);
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      RTC_CHECK(yuv_buffer_capacity >= yuv_size_) << "Insufficient capacity";
    }
  }

  {
#if RTC_DCHECK_IS_ON
    rtc::CritScope lock(&inited_crit_);
#endif
    inited_ = true;
  }
  weak_factory_.reset(new rtc::WeakPtrFactory<MediaCodecVideoEncoder>(this));
  encode_task_.reset(new EncodeTask(weak_factory_->GetWeakPtr()));

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const webrtc::CodecSpecificInfo* /* codec_specific_info */,
    const std::vector<webrtc::FrameType>* frame_types) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  if (sw_fallback_required_)
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  const int64_t frame_input_time_ms = rtc::TimeMillis();

  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  bool send_key_frame = false;
  if (codec_mode_ == webrtc::kRealtimeVideo) {
    ++frames_received_since_last_key_;
    int64_t now_ms = rtc::TimeMillis();
    if (last_frame_received_ms_ != -1 &&
        (now_ms - last_frame_received_ms_) > kFrameDiffThresholdMs) {
      // Add limit to prevent triggering a key for every frame for very low
      // framerates (e.g. if frame diff > kFrameDiffThresholdMs).
      if (frames_received_since_last_key_ > kMinKeyFrameInterval) {
        ALOGD << "Send key, frame diff: " << (now_ms - last_frame_received_ms_);
        send_key_frame = true;
      }
      frames_received_since_last_key_ = 0;
    }
    last_frame_received_ms_ = now_ms;
  }

  frames_received_++;
  if (!DeliverPendingOutputs(jni)) {
    if (!ProcessHWError(true /* reset_if_fallback_unavailable */)) {
      return sw_fallback_required_ ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                                   : WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  if (frames_encoded_ < kMaxEncodedLogFrames) {
    ALOGD << "Encoder frame in # " << (frames_received_ - 1)
          << ". TS: " << static_cast<int>(current_timestamp_us_ / 1000)
          << ". Q: " << input_frame_infos_.size() << ". Fps: " << last_set_fps_
          << ". Kbps: " << last_set_bitrate_kbps_;
  }

  if (drop_next_input_frame_) {
    ALOGW << "Encoder drop frame - failed callback.";
    drop_next_input_frame_ = false;
    current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;
    frames_dropped_media_encoder_++;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  RTC_CHECK(frame_types->size() == 1) << "Unexpected stream count";

  // Check if we accumulated too many frames in encoder input buffers and drop
  // frame if so.
  if (input_frame_infos_.size() > MAX_ENCODER_Q_SIZE) {
    ALOGD << "Already " << input_frame_infos_.size()
          << " frames in the queue, dropping"
          << ". TS: " << static_cast<int>(current_timestamp_us_ / 1000)
          << ". Fps: " << last_set_fps_
          << ". Consecutive drops: " << consecutive_full_queue_frame_drops_;
    current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;
    consecutive_full_queue_frame_drops_++;
    if (consecutive_full_queue_frame_drops_ >=
        ENCODER_STALL_FRAMEDROP_THRESHOLD) {
      ALOGE << "Encoder got stuck.";
      return ProcessHWErrorOnEncode();
    }
    frames_dropped_media_encoder_++;
    return WEBRTC_VIDEO_CODEC_OK;
  }
  consecutive_full_queue_frame_drops_ = 0;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> input_buffer(
      frame.video_frame_buffer());

  VideoFrame input_frame(input_buffer, frame.timestamp(),
                         frame.render_time_ms(), frame.rotation());

  if (!MaybeReconfigureEncoder(input_frame)) {
    ALOGE << "Failed to reconfigure encoder.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  const bool key_frame =
      frame_types->front() != webrtc::kVideoFrameDelta || send_key_frame;
  bool encode_status = true;
  if (input_frame.video_frame_buffer()->type() !=
      webrtc::VideoFrameBuffer::Type::kNative) {
    int j_input_buffer_index = jni->CallIntMethod(
        *j_media_codec_video_encoder_, j_dequeue_input_buffer_method_);
    if (CheckException(jni)) {
      ALOGE << "Exception in dequeu input buffer.";
      return ProcessHWErrorOnEncode();
    }
    if (j_input_buffer_index == -1) {
      // Video codec falls behind - no input buffer available.
      ALOGW << "Encoder drop frame - no input buffers available";
      if (frames_received_ > 1) {
        current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;
        frames_dropped_media_encoder_++;
      } else {
        // Input buffers are not ready after codec initialization, HW is still
        // allocating thme - this is expected and should not result in drop
        // frame report.
        frames_received_ = 0;
      }
      return WEBRTC_VIDEO_CODEC_OK;  // TODO(fischman): see webrtc bug 2887.
    } else if (j_input_buffer_index == -2) {
      return ProcessHWErrorOnEncode();
    }
    encode_status =
        EncodeByteBuffer(jni, key_frame, input_frame, j_input_buffer_index);
  } else {
    encode_status = EncodeTexture(jni, key_frame, input_frame);
  }

  if (!encode_status) {
    ALOGE << "Failed encode frame with timestamp: " << input_frame.timestamp();
    return ProcessHWErrorOnEncode();
  }

  // Save input image timestamps for later output.
  input_frame_infos_.emplace_back(frame_input_time_ms, input_frame.timestamp(),
                                  input_frame.render_time_ms(),
                                  input_frame.rotation());

  last_input_timestamp_ms_ =
      current_timestamp_us_ / rtc::kNumMicrosecsPerMillisec;

  current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;

  // Start the polling loop if it is not started.
  if (encode_task_) {
    rtc::TaskQueue::Current()->PostDelayedTask(std::move(encode_task_),
                                               kMediaCodecPollMs);
  }

  if (!DeliverPendingOutputs(jni)) {
    return ProcessHWErrorOnEncode();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoEncoder::MaybeReconfigureEncoder(
    const webrtc::VideoFrame& frame) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);

  const bool reconfigure_due_to_format = frame.is_texture() != use_surface_;
  const bool reconfigure_due_to_size =
      frame.width() != width_ || frame.height() != height_;

  if (reconfigure_due_to_format) {
      ALOGD << "Reconfigure encoder due to format change. "
            << (use_surface_ ?
                "Reconfiguring to encode from byte buffer." :
                "Reconfiguring to encode from texture.");
      LogStatistics(true);
  }
  if (reconfigure_due_to_size) {
    ALOGW << "Reconfigure encoder due to frame resolution change from "
        << width_ << " x " << height_ << " to " << frame.width() << " x "
        << frame.height();
    LogStatistics(true);
    width_ = frame.width();
    height_ = frame.height();
  }

  if (!reconfigure_due_to_format && !reconfigure_due_to_size)
    return true;

  Release();

  return InitEncodeInternal(width_, height_, 0, 0, frame.is_texture()) ==
         WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoEncoder::EncodeByteBuffer(JNIEnv* jni,
                                              bool key_frame,
                                              const webrtc::VideoFrame& frame,
                                              int input_buffer_index) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  RTC_CHECK(!use_surface_);

  jobject j_input_buffer = input_buffers_[input_buffer_index];
  uint8_t* yuv_buffer =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_input_buffer));
  if (CheckException(jni)) {
    ALOGE << "Exception in get direct buffer address.";
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return false;
  }
  RTC_CHECK(yuv_buffer) << "Indirect buffer??";
  rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer =
      frame.video_frame_buffer()->ToI420();
  RTC_CHECK(!libyuv::ConvertFromI420(
      i420_buffer->DataY(), i420_buffer->StrideY(), i420_buffer->DataU(),
      i420_buffer->StrideU(), i420_buffer->DataV(), i420_buffer->StrideV(),
      yuv_buffer, width_, width_, height_, encoder_fourcc_))
      << "ConvertFromI420 failed";

  bool encode_status = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                              j_encode_buffer_method_,
                                              key_frame,
                                              input_buffer_index,
                                              yuv_size_,
                                              current_timestamp_us_);
  if (CheckException(jni)) {
    ALOGE << "Exception in encode buffer.";
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return false;
  }
  return encode_status;
}

bool MediaCodecVideoEncoder::EncodeTexture(JNIEnv* jni,
                                           bool key_frame,
                                           const webrtc::VideoFrame& frame) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  RTC_CHECK(use_surface_);
  NativeHandleImpl handle =
      static_cast<AndroidTextureBuffer*>(frame.video_frame_buffer().get())
          ->native_handle_impl();

  jfloatArray sampling_matrix = handle.sampling_matrix.ToJava(jni);
  bool encode_status = jni->CallBooleanMethod(
      *j_media_codec_video_encoder_, j_encode_texture_method_, key_frame,
      handle.oes_texture_id, sampling_matrix, current_timestamp_us_);
  if (CheckException(jni)) {
    ALOGE << "Exception in encode texture.";
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return false;
  }
  return encode_status;
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::Release() {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ALOGD << "EncoderRelease: Frames received: " << frames_received_
        << ". Encoded: " << frames_encoded_
        << ". Dropped: " << frames_dropped_media_encoder_;
  encode_task_.reset(nullptr);
  weak_factory_.reset(nullptr);
  ScopedLocalRefFrame local_ref_frame(jni);
  for (size_t i = 0; i < input_buffers_.size(); ++i)
    jni->DeleteGlobalRef(input_buffers_[i]);
  input_buffers_.clear();
  jni->CallVoidMethod(*j_media_codec_video_encoder_, j_release_method_);
  if (CheckException(jni)) {
    ALOGE << "Exception in release.";
    ProcessHWError(false /* reset_if_fallback_unavailable */);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  {
#if RTC_DCHECK_IS_ON
    rtc::CritScope lock(&inited_crit_);
#endif
    inited_ = false;
  }
  use_surface_ = false;
  ALOGD << "EncoderRelease done.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRateAllocation(
    const webrtc::BitrateAllocation& rate_allocation,
    uint32_t frame_rate) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  const uint32_t new_bit_rate = rate_allocation.get_sum_kbps();
  if (sw_fallback_required_)
    return WEBRTC_VIDEO_CODEC_OK;
  frame_rate =
      (frame_rate < MAX_ALLOWED_VIDEO_FPS) ? frame_rate : MAX_ALLOWED_VIDEO_FPS;
  if (last_set_bitrate_kbps_ == new_bit_rate && last_set_fps_ == frame_rate) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  if (new_bit_rate > 0) {
    last_set_bitrate_kbps_ = new_bit_rate;
  }
  if (frame_rate > 0) {
    last_set_fps_ = frame_rate;
  }
  bool ret =
      jni->CallBooleanMethod(*j_media_codec_video_encoder_, j_set_rates_method_,
                             last_set_bitrate_kbps_, last_set_fps_);
  if (CheckException(jni) || !ret) {
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return sw_fallback_required_ ? WEBRTC_VIDEO_CODEC_OK
                                 : WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

int MediaCodecVideoEncoder::GetOutputBufferInfoIndex(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetIntField(jni, j_output_buffer_info, j_info_index_field_);
}

jobject MediaCodecVideoEncoder::GetOutputBufferInfoBuffer(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetObjectField(jni, j_output_buffer_info, j_info_buffer_field_);
}

bool MediaCodecVideoEncoder::GetOutputBufferInfoIsKeyFrame(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetBooleanField(jni, j_output_buffer_info, j_info_is_key_frame_field_);
}

jlong MediaCodecVideoEncoder::GetOutputBufferInfoPresentationTimestampUs(
    JNIEnv* jni,
    jobject j_output_buffer_info) {
  return GetLongField(
      jni, j_output_buffer_info, j_info_presentation_timestamp_us_field_);
}

bool MediaCodecVideoEncoder::DeliverPendingOutputs(JNIEnv* jni) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);

  while (true) {
    jobject j_output_buffer_info = jni->CallObjectMethod(
        *j_media_codec_video_encoder_, j_dequeue_output_buffer_method_);
    if (CheckException(jni)) {
      ALOGE << "Exception in set dequeue output buffer.";
      ProcessHWError(true /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (IsNull(jni, j_output_buffer_info)) {
      break;
    }

    int output_buffer_index =
        GetOutputBufferInfoIndex(jni, j_output_buffer_info);
    if (output_buffer_index == -1) {
      ProcessHWError(true /* reset_if_fallback_unavailable */);
      return false;
    }

    // Get key and config frame flags.
    jobject j_output_buffer =
        GetOutputBufferInfoBuffer(jni, j_output_buffer_info);
    bool key_frame = GetOutputBufferInfoIsKeyFrame(jni, j_output_buffer_info);

    // Get frame timestamps from a queue - for non config frames only.
    int64_t encoding_start_time_ms = 0;
    int64_t frame_encoding_time_ms = 0;
    last_output_timestamp_ms_ =
        GetOutputBufferInfoPresentationTimestampUs(jni, j_output_buffer_info) /
        rtc::kNumMicrosecsPerMillisec;
    if (!input_frame_infos_.empty()) {
      const InputFrameInfo& frame_info = input_frame_infos_.front();
      output_timestamp_ = frame_info.frame_timestamp;
      output_render_time_ms_ = frame_info.frame_render_time_ms;
      output_rotation_ = frame_info.rotation;
      encoding_start_time_ms = frame_info.encode_start_time;
      input_frame_infos_.pop_front();
    }

    // Extract payload.
    size_t payload_size = jni->GetDirectBufferCapacity(j_output_buffer);
    uint8_t* payload = reinterpret_cast<uint8_t*>(
        jni->GetDirectBufferAddress(j_output_buffer));
    if (CheckException(jni)) {
      ALOGE << "Exception in get direct buffer address.";
      ProcessHWError(true /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Callback - return encoded frame.
    const VideoCodecType codec_type = GetCodecType();
    webrtc::EncodedImageCallback::Result callback_result(
        webrtc::EncodedImageCallback::Result::OK);
    if (callback_) {
      std::unique_ptr<webrtc::EncodedImage> image(
          new webrtc::EncodedImage(payload, payload_size, payload_size));
      image->_encodedWidth = width_;
      image->_encodedHeight = height_;
      image->_timeStamp = output_timestamp_;
      image->capture_time_ms_ = output_render_time_ms_;
      image->rotation_ = output_rotation_;
      image->content_type_ =
          (codec_mode_ == webrtc::VideoCodecMode::kScreensharing)
              ? webrtc::VideoContentType::SCREENSHARE
              : webrtc::VideoContentType::UNSPECIFIED;
      image->timing_.is_timing_frame = false;
      image->_frameType =
          (key_frame ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta);
      image->_completeFrame = true;
      webrtc::CodecSpecificInfo info;
      memset(&info, 0, sizeof(info));
      info.codecType = codec_type;
      if (codec_type == kVideoCodecVP8) {
        info.codecSpecific.VP8.pictureId = picture_id_;
        info.codecSpecific.VP8.nonReference = false;
        info.codecSpecific.VP8.simulcastIdx = 0;
        info.codecSpecific.VP8.temporalIdx = webrtc::kNoTemporalIdx;
        info.codecSpecific.VP8.layerSync = false;
        info.codecSpecific.VP8.tl0PicIdx = webrtc::kNoTl0PicIdx;
        info.codecSpecific.VP8.keyIdx = webrtc::kNoKeyIdx;
      } else if (codec_type == kVideoCodecVP9) {
        if (key_frame) {
          gof_idx_ = 0;
        }
        info.codecSpecific.VP9.picture_id = picture_id_;
        info.codecSpecific.VP9.inter_pic_predicted = key_frame ? false : true;
        info.codecSpecific.VP9.flexible_mode = false;
        info.codecSpecific.VP9.ss_data_available = key_frame ? true : false;
        info.codecSpecific.VP9.tl0_pic_idx = tl0_pic_idx_++;
        info.codecSpecific.VP9.temporal_idx = webrtc::kNoTemporalIdx;
        info.codecSpecific.VP9.spatial_idx = webrtc::kNoSpatialIdx;
        info.codecSpecific.VP9.temporal_up_switch = true;
        info.codecSpecific.VP9.inter_layer_predicted = false;
        info.codecSpecific.VP9.gof_idx =
            static_cast<uint8_t>(gof_idx_++ % gof_.num_frames_in_gof);
        info.codecSpecific.VP9.num_spatial_layers = 1;
        info.codecSpecific.VP9.spatial_layer_resolution_present = false;
        if (info.codecSpecific.VP9.ss_data_available) {
          info.codecSpecific.VP9.spatial_layer_resolution_present = true;
          info.codecSpecific.VP9.width[0] = width_;
          info.codecSpecific.VP9.height[0] = height_;
          info.codecSpecific.VP9.gof.CopyGofInfoVP9(gof_);
        }
      }
      picture_id_ = (picture_id_ + 1) & 0x7FFF;

      // Generate a header describing a single fragment.
      webrtc::RTPFragmentationHeader header;
      memset(&header, 0, sizeof(header));
      if (codec_type == kVideoCodecVP8 || codec_type == kVideoCodecVP9) {
        header.VerifyAndAllocateFragmentationHeader(1);
        header.fragmentationOffset[0] = 0;
        header.fragmentationLength[0] = image->_length;
        header.fragmentationPlType[0] = 0;
        header.fragmentationTimeDiff[0] = 0;
        if (codec_type == kVideoCodecVP8) {
          int qp;
          if (webrtc::vp8::GetQp(payload, payload_size, &qp)) {
            current_acc_qp_ += qp;
            image->qp_ = qp;
          }
        } else if (codec_type == kVideoCodecVP9) {
          int qp;
          if (webrtc::vp9::GetQp(payload, payload_size, &qp)) {
            current_acc_qp_ += qp;
            image->qp_ = qp;
          }
        }
      } else if (codec_type == kVideoCodecH264) {
        h264_bitstream_parser_.ParseBitstream(payload, payload_size);
        int qp;
        if (h264_bitstream_parser_.GetLastSliceQp(&qp)) {
          current_acc_qp_ += qp;
          image->qp_ = qp;
        }
        // For H.264 search for start codes.
        const std::vector<webrtc::H264::NaluIndex> nalu_idxs =
            webrtc::H264::FindNaluIndices(payload, payload_size);
        if (nalu_idxs.empty()) {
          ALOGE << "Start code is not found!";
          ALOGE << "Data:" <<  image->_buffer[0] << " " << image->_buffer[1]
              << " " << image->_buffer[2] << " " << image->_buffer[3]
              << " " << image->_buffer[4] << " " << image->_buffer[5];
          ProcessHWError(true /* reset_if_fallback_unavailable */);
          return false;
        }
        header.VerifyAndAllocateFragmentationHeader(nalu_idxs.size());
        for (size_t i = 0; i < nalu_idxs.size(); i++) {
          header.fragmentationOffset[i] = nalu_idxs[i].payload_start_offset;
          header.fragmentationLength[i] = nalu_idxs[i].payload_size;
          header.fragmentationPlType[i] = 0;
          header.fragmentationTimeDiff[i] = 0;
        }
      }

      callback_result = callback_->OnEncodedImage(*image, &info, &header);
    }

    // Return output buffer back to the encoder.
    bool success = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                          j_release_output_buffer_method_,
                                          output_buffer_index);
    if (CheckException(jni) || !success) {
      ProcessHWError(true /* reset_if_fallback_unavailable */);
      return false;
    }

    // Print per frame statistics.
    if (encoding_start_time_ms > 0) {
      frame_encoding_time_ms = rtc::TimeMillis() - encoding_start_time_ms;
    }
    if (frames_encoded_ < kMaxEncodedLogFrames) {
      int current_latency = static_cast<int>(last_input_timestamp_ms_ -
                                             last_output_timestamp_ms_);
      ALOGD << "Encoder frame out # " << frames_encoded_
            << ". Key: " << key_frame << ". Size: " << payload_size
            << ". TS: " << static_cast<int>(last_output_timestamp_ms_)
            << ". Latency: " << current_latency
            << ". EncTime: " << frame_encoding_time_ms;
    }

    // Calculate and print encoding statistics - every 3 seconds.
    frames_encoded_++;
    current_frames_++;
    current_bytes_ += payload_size;
    current_encoding_time_ms_ += frame_encoding_time_ms;
    LogStatistics(false);

    // Errors in callback_result are currently ignored.
    if (callback_result.drop_next_frame)
      drop_next_input_frame_ = true;
  }
  return true;
}

void MediaCodecVideoEncoder::LogStatistics(bool force_log) {
  int statistic_time_ms = rtc::TimeMillis() - stat_start_time_ms_;
  if ((statistic_time_ms >= kMediaCodecStatisticsIntervalMs || force_log)
      && statistic_time_ms > 0) {
    // Prevent division by zero.
    int current_frames_divider = current_frames_ != 0 ? current_frames_ : 1;

    int current_bitrate = current_bytes_ * 8 / statistic_time_ms;
    int current_fps =
        (current_frames_ * 1000 + statistic_time_ms / 2) / statistic_time_ms;
    ALOGD << "Encoded frames: " << frames_encoded_ <<
        ". Bitrate: " << current_bitrate <<
        ", target: " << last_set_bitrate_kbps_ << " kbps" <<
        ", fps: " << current_fps <<
        ", encTime: " << (current_encoding_time_ms_ / current_frames_divider) <<
        ". QP: " << (current_acc_qp_ / current_frames_divider) <<
        " for last " << statistic_time_ms << " ms.";
    stat_start_time_ms_ = rtc::TimeMillis();
    current_frames_ = 0;
    current_bytes_ = 0;
    current_acc_qp_ = 0;
    current_encoding_time_ms_ = 0;
  }
}

webrtc::VideoEncoder::ScalingSettings
MediaCodecVideoEncoder::GetScalingSettings() const {
  if (webrtc::field_trial::IsEnabled(kCustomQPThresholdsFieldTrial)) {
    const VideoCodecType codec_type = GetCodecType();
    std::string experiment_string =
        webrtc::field_trial::FindFullName(kCustomQPThresholdsFieldTrial);
    ALOGD << "QP custom thresholds: " << experiment_string << " for codec "
          << codec_type;
    int low_vp8_qp_threshold;
    int high_vp8_qp_threshold;
    int low_h264_qp_threshold;
    int high_h264_qp_threshold;
    int parsed_values = sscanf(experiment_string.c_str(), "Enabled-%u,%u,%u,%u",
                               &low_vp8_qp_threshold, &high_vp8_qp_threshold,
                               &low_h264_qp_threshold, &high_h264_qp_threshold);
    if (parsed_values == 4) {
      RTC_CHECK_GT(high_vp8_qp_threshold, low_vp8_qp_threshold);
      RTC_CHECK_GT(low_vp8_qp_threshold, 0);
      RTC_CHECK_GT(high_h264_qp_threshold, low_h264_qp_threshold);
      RTC_CHECK_GT(low_h264_qp_threshold, 0);
      if (codec_type == kVideoCodecVP8) {
        return VideoEncoder::ScalingSettings(scale_, low_vp8_qp_threshold,
                                             high_vp8_qp_threshold);
      } else if (codec_type == kVideoCodecH264) {
        return VideoEncoder::ScalingSettings(scale_, low_h264_qp_threshold,
                                             high_h264_qp_threshold);
      }
    }
  }
  return VideoEncoder::ScalingSettings(scale_);
}

const char* MediaCodecVideoEncoder::ImplementationName() const {
  return "MediaCodec";
}

MediaCodecVideoEncoderFactory::MediaCodecVideoEncoderFactory()
    : egl_context_(nullptr) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_encoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoEncoder");
  jclass j_decoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoDecoder");
  supported_codecs_.clear();

  bool is_vp8_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isVp8HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_vp8_hw_supported) {
    ALOGD << "VP8 HW Encoder supported.";
    supported_codecs_.push_back(cricket::VideoCodec(cricket::kVp8CodecName));
  }

  bool is_vp9_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isVp9HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_vp9_hw_supported) {
    ALOGD << "VP9 HW Encoder supported.";
    supported_codecs_.push_back(cricket::VideoCodec(cricket::kVp9CodecName));
  }
  supported_codecs_with_h264_hp_ = supported_codecs_;

  // Check if high profile is supported by decoder. If yes, encoder can always
  // fall back to baseline profile as a subset as high profile.
  bool is_h264_high_profile_hw_supported = jni->CallStaticBooleanMethod(
      j_decoder_class,
      GetStaticMethodID(jni, j_decoder_class, "isH264HighProfileHwSupported",
                        "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_h264_high_profile_hw_supported) {
    ALOGD << "H.264 High Profile HW Encoder supported.";
    // TODO(magjed): Enumerate actual level instead of using hardcoded level
    // 3.1. Level 3.1 is 1280x720@30fps which is enough for now.
    cricket::VideoCodec constrained_high(cricket::kH264CodecName);
    const webrtc::H264::ProfileLevelId constrained_high_profile(
        webrtc::H264::kProfileConstrainedHigh, webrtc::H264::kLevel3_1);
    constrained_high.SetParam(
        cricket::kH264FmtpProfileLevelId,
        *webrtc::H264::ProfileLevelIdToString(constrained_high_profile));
    constrained_high.SetParam(cricket::kH264FmtpLevelAsymmetryAllowed, "1");
    constrained_high.SetParam(cricket::kH264FmtpPacketizationMode, "1");
    supported_codecs_with_h264_hp_.push_back(constrained_high);
  }

  bool is_h264_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isH264HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_h264_hw_supported) {
    ALOGD << "H.264 HW Encoder supported.";
    // TODO(magjed): Push Constrained High profile as well when negotiation is
    // ready, http://crbug/webrtc/6337. We can negotiate Constrained High
    // profile as long as we have decode support for it and still send Baseline
    // since Baseline is a subset of the High profile.
    cricket::VideoCodec constrained_baseline(cricket::kH264CodecName);
    const webrtc::H264::ProfileLevelId constrained_baseline_profile(
        webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1);
    constrained_baseline.SetParam(
        cricket::kH264FmtpProfileLevelId,
        *webrtc::H264::ProfileLevelIdToString(constrained_baseline_profile));
    constrained_baseline.SetParam(cricket::kH264FmtpLevelAsymmetryAllowed, "1");
    constrained_baseline.SetParam(cricket::kH264FmtpPacketizationMode, "1");
    supported_codecs_.push_back(constrained_baseline);
    supported_codecs_with_h264_hp_.push_back(constrained_baseline);
  }
}

MediaCodecVideoEncoderFactory::~MediaCodecVideoEncoderFactory() {
  ALOGD << "MediaCodecVideoEncoderFactory dtor";
  if (egl_context_) {
    JNIEnv* jni = AttachCurrentThreadIfNeeded();
    jni->DeleteGlobalRef(egl_context_);
  }
}

void MediaCodecVideoEncoderFactory::SetEGLContext(
    JNIEnv* jni, jobject egl_context) {
  ALOGD << "MediaCodecVideoEncoderFactory::SetEGLContext";
  if (egl_context_) {
    jni->DeleteGlobalRef(egl_context_);
    egl_context_ = nullptr;
  }
  egl_context_ = jni->NewGlobalRef(egl_context);
  if (CheckException(jni)) {
    ALOGE << "error calling NewGlobalRef for EGL Context.";
  }
}

webrtc::VideoEncoder* MediaCodecVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  if (supported_codecs().empty()) {
    ALOGW << "No HW video encoder for codec " << codec.name;
    return nullptr;
  }
  if (FindMatchingCodec(supported_codecs(), codec)) {
    ALOGD << "Create HW video encoder for " << codec.name;
    JNIEnv* jni = AttachCurrentThreadIfNeeded();
    ScopedLocalRefFrame local_ref_frame(jni);
    return new MediaCodecVideoEncoder(jni, codec, egl_context_);
  }
  ALOGW << "Can not find HW video encoder for type " << codec.name;
  return nullptr;
}

const std::vector<cricket::VideoCodec>&
MediaCodecVideoEncoderFactory::supported_codecs() const {
  if (webrtc::field_trial::IsEnabled(kH264HighProfileFieldTrial)) {
    return supported_codecs_with_h264_hp_;
  } else {
    return supported_codecs_;
  }
}

void MediaCodecVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  ALOGD << "Destroy video encoder.";
  delete encoder;
}

}  // namespace webrtc_jni
