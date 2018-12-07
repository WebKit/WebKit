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
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/h264_common.h"
#include "common_video/h264/profile_level_id.h"
#include "media/base/codec.h"
#include "media/base/mediaconstants.h"
#include "media/engine/internalencoderfactory.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/bind.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/weak_ptr.h"
#include "sdk/android/generated_video_jni/jni/MediaCodecVideoEncoder_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/androidmediacodeccommon.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/videocodecinfo.h"
#include "sdk/android/src/jni/videoframe.h"
#include "system_wrappers/include/field_trial.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/video_common.h"

using rtc::Bind;
using rtc::ThreadManager;

namespace webrtc {
namespace jni {

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
#define ALOGD RTC_LOG_TAG(rtc::LS_INFO, TAG_ENCODER)
#define ALOGW RTC_LOG_TAG(rtc::LS_WARNING, TAG_ENCODER)
#define ALOGE RTC_LOG_TAG(rtc::LS_ERROR, TAG_ENCODER)

    namespace {
  // Maximum time limit between incoming frames before requesting a key frame.
  const int64_t kFrameDiffThresholdMs = 350;
  const int kMinKeyFrameInterval = 6;
  const char kCustomQPThresholdsFieldTrial[] = "WebRTC-CustomQPThresholds";
}  // namespace

// MediaCodecVideoEncoder is a VideoEncoder implementation that uses
// Android's MediaCodec SDK API behind the scenes to implement (hopefully)
// HW-backed video encode.  This C++ class is implemented as a very thin shim,
// delegating all of the interesting work to org.webrtc.MediaCodecVideoEncoder.
// MediaCodecVideoEncoder must be operated on a single task queue, currently
// this is the encoder queue from ViE encoder.
class MediaCodecVideoEncoder : public VideoEncoder {
 public:
  ~MediaCodecVideoEncoder() override;
  MediaCodecVideoEncoder(JNIEnv* jni,
                         const SdpVideoFormat& format,
                         bool has_egl_context);

  // VideoEncoder implementation.
  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t /* number_of_cores */,
                     size_t /* max_payload_size */) override;
  int32_t Encode(const VideoFrame& input_image,
                 const CodecSpecificInfo* /* codec_specific_info */,
                 const std::vector<FrameType>* frame_types) override;
  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t SetRateAllocation(const VideoBitrateAllocation& rate_allocation,
                            uint32_t frame_rate) override;
  EncoderInfo GetEncoderInfo() const override;

  // Fills the input buffer with data from the buffers passed as parameters.
  bool FillInputBuffer(JNIEnv* jni,
                       int input_buffer_index,
                       uint8_t const* buffer_y,
                       int stride_y,
                       uint8_t const* buffer_u,
                       int stride_u,
                       uint8_t const* buffer_v,
                       int stride_v);

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
  bool MaybeReconfigureEncoder(JNIEnv* jni, const VideoFrame& frame);

  // Returns true if the frame is a texture frame and we should use surface
  // based encoding.
  bool IsTextureFrame(JNIEnv* jni, const VideoFrame& frame);

  bool EncodeByteBuffer(JNIEnv* jni,
                        bool key_frame,
                        const VideoFrame& frame,
                        int input_buffer_index);
  // Encodes a new style org.webrtc.VideoFrame. Might be a I420 or a texture
  // frame.
  bool EncodeJavaFrame(JNIEnv* jni,
                       bool key_frame,
                       const JavaRef<jobject>& frame,
                       int input_buffer_index);

  // Deliver any outputs pending in the MediaCodec to our |callback_| and return
  // true on success.
  bool DeliverPendingOutputs(JNIEnv* jni);

  VideoEncoder::ScalingSettings GetScalingSettingsInternal() const;

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
  const SdpVideoFormat format_;

  EncodedImageCallback* callback_;

  // State that is constant for the lifetime of this object once the ctor
  // returns.
  rtc::SequencedTaskChecker encoder_queue_checker_;
  ScopedJavaGlobalRef<jobject> j_media_codec_video_encoder_;

  // State that is valid only between InitEncode() and the next Release().
  int width_;   // Frame width in pixels.
  int height_;  // Frame height in pixels.
  bool inited_;
  bool use_surface_;
  enum libyuv::FourCC encoder_fourcc_;  // Encoder color space format.
  uint32_t last_set_bitrate_kbps_;      // Last-requested bitrate in kbps.
  uint32_t last_set_fps_;               // Last-requested frame rate.
  int64_t current_timestamp_us_;        // Current frame timestamps in us.
  int frames_received_;                 // Number of frames received by encoder.
  int frames_encoded_;                  // Number of frames encoded by encoder.
  int frames_dropped_media_encoder_;    // Number of frames dropped by encoder.
  // Number of dropped frames caused by full queue.
  int consecutive_full_queue_frame_drops_;
  int64_t stat_start_time_ms_;  // Start time for statistics.
  int current_frames_;  // Number of frames in the current statistics interval.
  int current_bytes_;   // Encoded bytes in the current statistics interval.
  int current_acc_qp_;  // Accumulated QP in the current statistics interval.
  int current_encoding_time_ms_;  // Overall encoding time in the current second
  int64_t last_input_timestamp_ms_;   // Timestamp of last received yuv frame.
  int64_t last_output_timestamp_ms_;  // Timestamp of last encoded frame.
  // Holds the task while the polling loop is paused.
  std::unique_ptr<rtc::QueuedTask> encode_task_;

  struct InputFrameInfo {
    InputFrameInfo(int64_t encode_start_time,
                   int32_t frame_timestamp,
                   int64_t frame_render_time_ms,
                   VideoRotation rotation)
        : encode_start_time(encode_start_time),
          frame_timestamp(frame_timestamp),
          frame_render_time_ms(frame_render_time_ms),
          rotation(rotation) {}
    // Time when video frame is sent to encoder input.
    const int64_t encode_start_time;

    // Input frame information.
    const int32_t frame_timestamp;
    const int64_t frame_render_time_ms;
    const VideoRotation rotation;
  };
  std::list<InputFrameInfo> input_frame_infos_;
  int32_t output_timestamp_;       // Last output frame timestamp from
                                   // |input_frame_infos_|.
  int64_t output_render_time_ms_;  // Last output frame render time from
                                   // |input_frame_infos_|.
  VideoRotation output_rotation_;  // Last output frame rotation from
                                   // |input_frame_infos_|.

  // Frame size in bytes fed to MediaCodec.
  int yuv_size_;
  // True only when between a callback_->OnEncodedImage() call return a positive
  // value and the next Encode() call being ignored.
  bool drop_next_input_frame_;
  bool scale_;
  H264::Profile profile_;
  // Global references; must be deleted in Release().
  std::vector<ScopedJavaGlobalRef<jobject>> input_buffers_;
  H264BitstreamParser h264_bitstream_parser_;

  // VP9 variables to populate codec specific structure.
  GofInfoVP9 gof_;  // Contains each frame's temporal information for
                    // non-flexible VP9 mode.
  size_t gof_idx_;

  const bool has_egl_context_;
  EncoderInfo encoder_info_;

  // Temporary fix for VP8.
  // Sends a key frame if frames are largely spaced apart (possibly
  // corresponding to a large image change).
  int64_t last_frame_received_ms_;
  int frames_received_since_last_key_;
  VideoCodecMode codec_mode_;

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
                                               const SdpVideoFormat& format,
                                               bool has_egl_context)
    : format_(format),
      callback_(NULL),
      j_media_codec_video_encoder_(
          jni,
          Java_MediaCodecVideoEncoder_Constructor(jni)),
      inited_(false),
      use_surface_(false),
      has_egl_context_(has_egl_context),
      sw_fallback_required_(false) {
  encoder_queue_checker_.Detach();
}

int32_t MediaCodecVideoEncoder::InitEncode(const VideoCodec* codec_settings,
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

  if (codec_settings->numberOfSimulcastStreams > 1) {
    ALOGD << "Number of simulcast layers requested: "
          << codec_settings->numberOfSimulcastStreams
          << ". Requesting software fallback.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  // Check allowed H.264 profile
  profile_ = H264::Profile::kProfileBaseline;
  if (codec_type == kVideoCodecH264) {
    const absl::optional<H264::ProfileLevelId> profile_level_id =
        H264::ParseSdpProfileLevelId(format_.parameters);
    RTC_DCHECK(profile_level_id);
    profile_ = profile_level_id->profile;
    ALOGD << "H.264 profile: " << profile_;
  }

  encoder_info_.supports_native_handle = has_egl_context_;
  encoder_info_.implementation_name = "MediaCodec";
  encoder_info_.scaling_settings = GetScalingSettingsInternal();

  return InitEncodeInternal(
      init_width, init_height, codec_settings->startBitrate,
      codec_settings->maxFramerate,
      codec_settings->expect_encode_from_texture && has_egl_context_);
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

bool IsFormatSupported(const std::vector<SdpVideoFormat>& supported_formats,
                       const SdpVideoFormat& format) {
  for (const SdpVideoFormat& supported_format : supported_formats) {
    if (cricket::IsSameCodec(format.name, format.parameters,
                             supported_format.name,
                             supported_format.parameters)) {
      return true;
    }
  }
  return false;
}

bool MediaCodecVideoEncoder::ProcessHWError(
    bool reset_if_fallback_unavailable) {
  ALOGE << "ProcessHWError";
  if (IsFormatSupported(InternalEncoderFactory().GetSupportedFormats(),
                        format_)) {
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
  return PayloadStringToCodecType(format_.name);
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
  RTC_CHECK(!use_surface || has_egl_context_) << "EGL context not set.";
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
  gof_.SetGofInfoVP9(TemporalStructureMode::kTemporalStructureMode1);
  gof_idx_ = 0;
  last_frame_received_ms_ = -1;
  frames_received_since_last_key_ = kMinKeyFrameInterval;

  // We enforce no extra stride/padding in the format creation step.
  ScopedJavaLocalRef<jobject> j_video_codec_enum =
      Java_VideoCodecType_fromNativeIndex(jni, codec_type);
  const bool encode_status = Java_MediaCodecVideoEncoder_initEncode(
      jni, j_media_codec_video_encoder_, j_video_codec_enum, profile_, width,
      height, kbps, fps, use_surface);

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
    ScopedJavaLocalRef<jobjectArray> input_buffers =
        Java_MediaCodecVideoEncoder_getInputBuffers(
            jni, j_media_codec_video_encoder_);
    if (CheckException(jni)) {
      ALOGE << "Exception in get input buffers.";
      ProcessHWError(false /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (IsNull(jni, input_buffers)) {
      ProcessHWError(false /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    switch (Java_MediaCodecVideoEncoder_getColorFormat(
        jni, j_media_codec_video_encoder_)) {
      case COLOR_FormatYUV420Planar:
        encoder_fourcc_ = libyuv::FOURCC_YU12;
        break;
      case COLOR_FormatYUV420SemiPlanar:
      case COLOR_QCOM_FormatYUV420SemiPlanar:
      case COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m:
        encoder_fourcc_ = libyuv::FOURCC_NV12;
        break;
      default:
        RTC_LOG(LS_ERROR) << "Wrong color format.";
        ProcessHWError(false /* reset_if_fallback_unavailable */);
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    RTC_CHECK(input_buffers_.empty())
        << "Unexpected double InitEncode without Release";
    input_buffers_ = JavaToNativeVector<ScopedJavaGlobalRef<jobject>>(
        jni, input_buffers, [](JNIEnv* env, const JavaRef<jobject>& o) {
          return ScopedJavaGlobalRef<jobject>(env, o);
        });
    for (const ScopedJavaGlobalRef<jobject>& buffer : input_buffers_) {
      int64_t yuv_buffer_capacity = jni->GetDirectBufferCapacity(buffer.obj());
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
    const VideoFrame& frame,
    const CodecSpecificInfo* /* codec_specific_info */,
    const std::vector<FrameType>* frame_types) {
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
  if (codec_mode_ == VideoCodecMode::kRealtimeVideo) {
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

  rtc::scoped_refptr<VideoFrameBuffer> input_buffer(frame.video_frame_buffer());

  VideoFrame input_frame(input_buffer, frame.timestamp(),
                         frame.render_time_ms(), frame.rotation());

  if (!MaybeReconfigureEncoder(jni, input_frame)) {
    ALOGE << "Failed to reconfigure encoder.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  const bool key_frame =
      frame_types->front() != kVideoFrameDelta || send_key_frame;
  bool encode_status = true;

  int j_input_buffer_index = -1;
  if (!use_surface_) {
    j_input_buffer_index = Java_MediaCodecVideoEncoder_dequeueInputBuffer(
        jni, j_media_codec_video_encoder_);
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
  }

  if (input_frame.video_frame_buffer()->type() !=
      VideoFrameBuffer::Type::kNative) {
    encode_status =
        EncodeByteBuffer(jni, key_frame, input_frame, j_input_buffer_index);
  } else {
    ScopedJavaLocalRef<jobject> j_frame = NativeToJavaVideoFrame(jni, frame);
    encode_status =
        EncodeJavaFrame(jni, key_frame, j_frame, j_input_buffer_index);
    ReleaseJavaVideoFrame(jni, j_frame);
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

bool MediaCodecVideoEncoder::MaybeReconfigureEncoder(JNIEnv* jni,
                                                     const VideoFrame& frame) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);

  bool is_texture = IsTextureFrame(jni, frame);
  const bool reconfigure_due_to_format = is_texture != use_surface_;
  const bool reconfigure_due_to_size =
      frame.width() != width_ || frame.height() != height_;

  if (reconfigure_due_to_format) {
    ALOGD << "Reconfigure encoder due to format change. "
          << (use_surface_ ? "Reconfiguring to encode from byte buffer."
                           : "Reconfiguring to encode from texture.");
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

  return InitEncodeInternal(width_, height_, 0, 0, is_texture) ==
         WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoEncoder::IsTextureFrame(JNIEnv* jni,
                                            const VideoFrame& frame) {
  if (frame.video_frame_buffer()->type() != VideoFrameBuffer::Type::kNative) {
    return false;
  }
  return Java_MediaCodecVideoEncoder_isTextureBuffer(
      jni, static_cast<AndroidVideoBuffer*>(frame.video_frame_buffer().get())
               ->video_frame_buffer());
}

bool MediaCodecVideoEncoder::EncodeByteBuffer(JNIEnv* jni,
                                              bool key_frame,
                                              const VideoFrame& frame,
                                              int input_buffer_index) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);
  RTC_CHECK(!use_surface_);

  rtc::scoped_refptr<I420BufferInterface> i420_buffer =
      frame.video_frame_buffer()->ToI420();
  if (!FillInputBuffer(jni, input_buffer_index, i420_buffer->DataY(),
                       i420_buffer->StrideY(), i420_buffer->DataU(),
                       i420_buffer->StrideU(), i420_buffer->DataV(),
                       i420_buffer->StrideV())) {
    return false;
  }
  bool encode_status = Java_MediaCodecVideoEncoder_encodeBuffer(
      jni, j_media_codec_video_encoder_, key_frame, input_buffer_index,
      yuv_size_, current_timestamp_us_);
  if (CheckException(jni)) {
    ALOGE << "Exception in encode buffer.";
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return false;
  }
  return encode_status;
}

bool MediaCodecVideoEncoder::FillInputBuffer(JNIEnv* jni,
                                             int input_buffer_index,
                                             uint8_t const* buffer_y,
                                             int stride_y,
                                             uint8_t const* buffer_u,
                                             int stride_u,
                                             uint8_t const* buffer_v,
                                             int stride_v) {
  uint8_t* yuv_buffer = reinterpret_cast<uint8_t*>(
      jni->GetDirectBufferAddress(input_buffers_[input_buffer_index].obj()));
  if (CheckException(jni)) {
    ALOGE << "Exception in get direct buffer address.";
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return false;
  }
  RTC_CHECK(yuv_buffer) << "Indirect buffer??";

  RTC_CHECK(!libyuv::ConvertFromI420(buffer_y, stride_y, buffer_u, stride_u,
                                     buffer_v, stride_v, yuv_buffer, width_,
                                     width_, height_, encoder_fourcc_))
      << "ConvertFromI420 failed";
  return true;
}

bool MediaCodecVideoEncoder::EncodeJavaFrame(JNIEnv* jni,
                                             bool key_frame,
                                             const JavaRef<jobject>& frame,
                                             int input_buffer_index) {
  bool encode_status = Java_MediaCodecVideoEncoder_encodeFrame(
      jni, j_media_codec_video_encoder_, jlongFromPointer(this), key_frame,
      frame, input_buffer_index, current_timestamp_us_);
  if (CheckException(jni)) {
    ALOGE << "Exception in encode frame.";
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return false;
  }
  return encode_status;
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
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
  input_buffers_.clear();
  Java_MediaCodecVideoEncoder_release(jni, j_media_codec_video_encoder_);
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
  // It's legal to move the encoder to another queue now.
  encoder_queue_checker_.Detach();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRateAllocation(
    const VideoBitrateAllocation& rate_allocation,
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
  bool ret = Java_MediaCodecVideoEncoder_setRates(
      jni, j_media_codec_video_encoder_,
      rtc::dchecked_cast<int>(last_set_bitrate_kbps_),
      rtc::dchecked_cast<int>(last_set_fps_));
  if (CheckException(jni) || !ret) {
    ProcessHWError(true /* reset_if_fallback_unavailable */);
    return sw_fallback_required_ ? WEBRTC_VIDEO_CODEC_OK
                                 : WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

VideoEncoder::EncoderInfo MediaCodecVideoEncoder::GetEncoderInfo() const {
  return encoder_info_;
}

bool MediaCodecVideoEncoder::DeliverPendingOutputs(JNIEnv* jni) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&encoder_queue_checker_);

  while (true) {
    ScopedJavaLocalRef<jobject> j_output_buffer_info =
        Java_MediaCodecVideoEncoder_dequeueOutputBuffer(
            jni, j_media_codec_video_encoder_);
    if (CheckException(jni)) {
      ALOGE << "Exception in set dequeue output buffer.";
      ProcessHWError(true /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (IsNull(jni, j_output_buffer_info)) {
      break;
    }

    int output_buffer_index =
        Java_OutputBufferInfo_getIndex(jni, j_output_buffer_info);
    if (output_buffer_index == -1) {
      ProcessHWError(true /* reset_if_fallback_unavailable */);
      return false;
    }

    // Get key and config frame flags.
    ScopedJavaLocalRef<jobject> j_output_buffer =
        Java_OutputBufferInfo_getBuffer(jni, j_output_buffer_info);
    bool key_frame =
        Java_OutputBufferInfo_isKeyFrame(jni, j_output_buffer_info);

    // Get frame timestamps from a queue - for non config frames only.
    int64_t encoding_start_time_ms = 0;
    int64_t frame_encoding_time_ms = 0;
    last_output_timestamp_ms_ =
        Java_OutputBufferInfo_getPresentationTimestampUs(jni,
                                                         j_output_buffer_info) /
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
    size_t payload_size = jni->GetDirectBufferCapacity(j_output_buffer.obj());
    uint8_t* payload = reinterpret_cast<uint8_t*>(
        jni->GetDirectBufferAddress(j_output_buffer.obj()));
    if (CheckException(jni)) {
      ALOGE << "Exception in get direct buffer address.";
      ProcessHWError(true /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Callback - return encoded frame.
    const VideoCodecType codec_type = GetCodecType();
    EncodedImageCallback::Result callback_result(
        EncodedImageCallback::Result::OK);
    if (callback_) {
      std::unique_ptr<EncodedImage> image(
          new EncodedImage(payload, payload_size, payload_size));
      image->_encodedWidth = width_;
      image->_encodedHeight = height_;
      image->SetTimestamp(output_timestamp_);
      image->capture_time_ms_ = output_render_time_ms_;
      image->rotation_ = output_rotation_;
      image->content_type_ = (codec_mode_ == VideoCodecMode::kScreensharing)
                                 ? VideoContentType::SCREENSHARE
                                 : VideoContentType::UNSPECIFIED;
      image->timing_.flags = VideoSendTiming::kInvalid;
      image->_frameType = (key_frame ? kVideoFrameKey : kVideoFrameDelta);
      image->_completeFrame = true;
      CodecSpecificInfo info;
      memset(&info, 0, sizeof(info));
      info.codecType = codec_type;
      if (codec_type == kVideoCodecVP8) {
        info.codecSpecific.VP8.nonReference = false;
        info.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;
        info.codecSpecific.VP8.layerSync = false;
        info.codecSpecific.VP8.keyIdx = kNoKeyIdx;
      } else if (codec_type == kVideoCodecVP9) {
        if (key_frame) {
          gof_idx_ = 0;
        }
        info.codecSpecific.VP9.inter_pic_predicted = key_frame ? false : true;
        info.codecSpecific.VP9.flexible_mode = false;
        info.codecSpecific.VP9.ss_data_available = key_frame ? true : false;
        info.codecSpecific.VP9.temporal_idx = kNoTemporalIdx;
        info.codecSpecific.VP9.temporal_up_switch = true;
        info.codecSpecific.VP9.inter_layer_predicted = false;
        info.codecSpecific.VP9.gof_idx =
            static_cast<uint8_t>(gof_idx_++ % gof_.num_frames_in_gof);
        info.codecSpecific.VP9.num_spatial_layers = 1;
        info.codecSpecific.VP9.first_frame_in_picture = true;
        info.codecSpecific.VP9.end_of_picture = true;
        info.codecSpecific.VP9.spatial_layer_resolution_present = false;
        if (info.codecSpecific.VP9.ss_data_available) {
          info.codecSpecific.VP9.spatial_layer_resolution_present = true;
          info.codecSpecific.VP9.width[0] = width_;
          info.codecSpecific.VP9.height[0] = height_;
          info.codecSpecific.VP9.gof.CopyGofInfoVP9(gof_);
        }
      }

      // Generate a header describing a single fragment.
      RTPFragmentationHeader header;
      memset(&header, 0, sizeof(header));
      if (codec_type == kVideoCodecVP8 || codec_type == kVideoCodecVP9) {
        header.VerifyAndAllocateFragmentationHeader(1);
        header.fragmentationOffset[0] = 0;
        header.fragmentationLength[0] = image->_length;
        header.fragmentationPlType[0] = 0;
        header.fragmentationTimeDiff[0] = 0;
        if (codec_type == kVideoCodecVP8) {
          int qp;
          if (vp8::GetQp(payload, payload_size, &qp)) {
            current_acc_qp_ += qp;
            image->qp_ = qp;
          }
        } else if (codec_type == kVideoCodecVP9) {
          int qp;
          if (vp9::GetQp(payload, payload_size, &qp)) {
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
        const std::vector<H264::NaluIndex> nalu_idxs =
            H264::FindNaluIndices(payload, payload_size);
        if (nalu_idxs.empty()) {
          ALOGE << "Start code is not found!";
          ALOGE << "Data:" << image->_buffer[0] << " " << image->_buffer[1]
                << " " << image->_buffer[2] << " " << image->_buffer[3] << " "
                << image->_buffer[4] << " " << image->_buffer[5];
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
    bool success = Java_MediaCodecVideoEncoder_releaseOutputBuffer(
        jni, j_media_codec_video_encoder_, output_buffer_index);
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
  if ((statistic_time_ms >= kMediaCodecStatisticsIntervalMs || force_log) &&
      statistic_time_ms > 0) {
    // Prevent division by zero.
    int current_frames_divider = current_frames_ != 0 ? current_frames_ : 1;

    int current_bitrate = current_bytes_ * 8 / statistic_time_ms;
    int current_fps =
        (current_frames_ * 1000 + statistic_time_ms / 2) / statistic_time_ms;
    ALOGD << "Encoded frames: " << frames_encoded_
          << ". Bitrate: " << current_bitrate
          << ", target: " << last_set_bitrate_kbps_ << " kbps"
          << ", fps: " << current_fps << ", encTime: "
          << (current_encoding_time_ms_ / current_frames_divider)
          << ". QP: " << (current_acc_qp_ / current_frames_divider)
          << " for last " << statistic_time_ms << " ms.";
    stat_start_time_ms_ = rtc::TimeMillis();
    current_frames_ = 0;
    current_bytes_ = 0;
    current_acc_qp_ = 0;
    current_encoding_time_ms_ = 0;
  }
}

VideoEncoder::ScalingSettings
MediaCodecVideoEncoder::GetScalingSettingsInternal() const {
  if (!scale_)
    return VideoEncoder::ScalingSettings::kOff;

  const VideoCodecType codec_type = GetCodecType();
  if (field_trial::IsEnabled(kCustomQPThresholdsFieldTrial)) {
    std::string experiment_string =
        field_trial::FindFullName(kCustomQPThresholdsFieldTrial);
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
        return VideoEncoder::ScalingSettings(low_vp8_qp_threshold,
                                             high_vp8_qp_threshold);
      } else if (codec_type == kVideoCodecH264) {
        return VideoEncoder::ScalingSettings(low_h264_qp_threshold,
                                             high_h264_qp_threshold);
      }
    }
  }
  if (codec_type == kVideoCodecVP8) {
    // Same as in vp8_impl.cc.
    static const int kLowVp8QpThreshold = 29;
    static const int kHighVp8QpThreshold = 95;

    return VideoEncoder::ScalingSettings(kLowVp8QpThreshold,
                                         kHighVp8QpThreshold);
  } else if (codec_type == kVideoCodecVP9) {
    // QP is obtained from VP9-bitstream, so the QP corresponds to the bitstream
    // range of [0, 255] and not the user-level range of [0,63].
    static const int kLowVp9QpThreshold = 96;
    static const int kHighVp9QpThreshold = 185;

    return VideoEncoder::ScalingSettings(kLowVp9QpThreshold,
                                         kHighVp9QpThreshold);
  } else if (codec_type == kVideoCodecH264) {
    // Same as in h264_encoder_impl.cc.
    static const int kLowH264QpThreshold = 24;
    static const int kHighH264QpThreshold = 37;

    return VideoEncoder::ScalingSettings(kLowH264QpThreshold,
                                         kHighH264QpThreshold);
  }
  return VideoEncoder::ScalingSettings::kOff;
}

static void JNI_MediaCodecVideoEncoder_FillInputBuffer(
    JNIEnv* jni,
    const JavaParamRef<jclass>&,
    jlong native_encoder,
    jint input_buffer,
    const JavaParamRef<jobject>& j_buffer_y,
    jint stride_y,
    const JavaParamRef<jobject>& j_buffer_u,
    jint stride_u,
    const JavaParamRef<jobject>& j_buffer_v,
    jint stride_v) {
  uint8_t* buffer_y =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_buffer_y.obj()));
  uint8_t* buffer_u =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_buffer_u.obj()));
  uint8_t* buffer_v =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_buffer_v.obj()));

  RTC_DCHECK(buffer_y) << "GetDirectBufferAddress returned null. Ensure that "
                          "getDataY returns a direct ByteBuffer.";
  RTC_DCHECK(buffer_u) << "GetDirectBufferAddress returned null. Ensure that "
                          "getDataU returns a direct ByteBuffer.";
  RTC_DCHECK(buffer_v) << "GetDirectBufferAddress returned null. Ensure that "
                          "getDataV returns a direct ByteBuffer.";

  reinterpret_cast<MediaCodecVideoEncoder*>(native_encoder)
      ->FillInputBuffer(jni, input_buffer, buffer_y, stride_y, buffer_u,
                        stride_u, buffer_v, stride_v);
}

static jlong JNI_MediaCodecVideoEncoder_CreateEncoder(
    JNIEnv* env,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& format,
    jboolean has_egl_context) {
  ScopedLocalRefFrame local_ref_frame(env);
  return jlongFromPointer(new MediaCodecVideoEncoder(
      env, VideoCodecInfoToSdpVideoFormat(env, format), has_egl_context));
}

}  // namespace jni
}  // namespace webrtc
