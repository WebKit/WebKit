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
#include "webrtc/api/android/jni/androidmediaencoder_jni.h"

#include <algorithm>
#include <memory>
#include <list>

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "webrtc/api/android/jni/androidmediacodeccommon.h"
#include "webrtc/api/android/jni/classreferenceholder.h"
#include "webrtc/api/android/jni/native_handle_impl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/h264/h264_bitstream_parser.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"
#include "webrtc/modules/video_coding/utility/vp8_header_parser.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/system_wrappers/include/logcat_trace_context.h"

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

// H.264 start code length.
#define H264_SC_LENGTH 4
// Maximum allowed NALUs in one output frame.
#define MAX_NALUS_PERFRAME 32
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
}  // namespace

// MediaCodecVideoEncoder is a webrtc::VideoEncoder implementation that uses
// Android's MediaCodec SDK API behind the scenes to implement (hopefully)
// HW-backed video encode.  This C++ class is implemented as a very thin shim,
// delegating all of the interesting work to org.webrtc.MediaCodecVideoEncoder.
// MediaCodecVideoEncoder is created, operated, and destroyed on a single
// thread, currently the libjingle Worker thread.
class MediaCodecVideoEncoder : public webrtc::VideoEncoder,
                               public rtc::MessageHandler {
 public:
  virtual ~MediaCodecVideoEncoder();
  MediaCodecVideoEncoder(JNIEnv* jni,
                         VideoCodecType codecType,
                         jobject egl_context);

  // webrtc::VideoEncoder implementation.  Everything trampolines to
  // |codec_thread_| for execution.
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
  int32_t SetRates(uint32_t new_bit_rate, uint32_t frame_rate) override;

  // rtc::MessageHandler implementation.
  void OnMessage(rtc::Message* msg) override;

  void OnDroppedFrame() override;

  bool SupportsNativeHandle() const override { return egl_context_ != nullptr; }
  const char* ImplementationName() const override;

 private:
  // ResetCodecOnCodecThread() calls ReleaseOnCodecThread() and
  // InitEncodeOnCodecThread() in an attempt to restore the codec to an
  // operable state.  Necessary after all manner of OMX-layer errors.
  // Returns true if the codec was reset successfully.
  bool ResetCodecOnCodecThread();

  // Fallback to a software encoder if one is supported else try to reset the
  // encoder. Called with |reset_if_fallback_unavailable| equal to false from
  // init/release encoder so that we don't go into infinite recursion.
  // Returns true if the codec was reset successfully.
  bool ProcessHWErrorOnCodecThread(bool reset_if_fallback_unavailable);

  // Calls ProcessHWErrorOnCodecThread(true). Returns
  // WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE if sw_fallback_required_ was set or
  // WEBRTC_VIDEO_CODEC_ERROR otherwise.
  int32_t ProcessHWErrorOnEncodeOnCodecThread();

  // Implementation of webrtc::VideoEncoder methods above, all running on the
  // codec thread exclusively.
  //
  // If width==0 then this is assumed to be a re-initialization and the
  // previously-current values are reused instead of the passed parameters
  // (makes it easier to reason about thread-safety).
  int32_t InitEncodeOnCodecThread(int width, int height, int kbps, int fps,
      bool use_surface);
  // Reconfigure to match |frame| in width, height. Also reconfigures the
  // encoder if |frame| is a texture/byte buffer and the encoder is initialized
  // for byte buffer/texture. Returns false if reconfiguring fails.
  bool MaybeReconfigureEncoderOnCodecThread(const webrtc::VideoFrame& frame);
  int32_t EncodeOnCodecThread(
      const webrtc::VideoFrame& input_image,
      const std::vector<webrtc::FrameType>* frame_types,
      const int64_t frame_input_time_ms);
  bool EncodeByteBufferOnCodecThread(JNIEnv* jni,
      bool key_frame, const webrtc::VideoFrame& frame, int input_buffer_index);
  bool EncodeTextureOnCodecThread(JNIEnv* jni,
      bool key_frame, const webrtc::VideoFrame& frame);

  int32_t RegisterEncodeCompleteCallbackOnCodecThread(
      webrtc::EncodedImageCallback* callback);
  int32_t ReleaseOnCodecThread();
  int32_t SetRatesOnCodecThread(uint32_t new_bit_rate, uint32_t frame_rate);
  void OnDroppedFrameOnCodecThread();

  // Helper accessors for MediaCodecVideoEncoder$OutputBufferInfo members.
  int GetOutputBufferInfoIndex(JNIEnv* jni, jobject j_output_buffer_info);
  jobject GetOutputBufferInfoBuffer(JNIEnv* jni, jobject j_output_buffer_info);
  bool GetOutputBufferInfoIsKeyFrame(JNIEnv* jni, jobject j_output_buffer_info);
  jlong GetOutputBufferInfoPresentationTimestampUs(
      JNIEnv* jni, jobject j_output_buffer_info);

  // Deliver any outputs pending in the MediaCodec to our |callback_| and return
  // true on success.
  bool DeliverPendingOutputs(JNIEnv* jni);

  // Search for H.264 start codes.
  int32_t NextNaluPosition(uint8_t *buffer, size_t buffer_size);

  // Displays encoder statistics.
  void LogStatistics(bool force_log);

  // Type of video codec.
  VideoCodecType codecType_;

  // Valid all the time since RegisterEncodeCompleteCallback() Invoke()s to
  // |codec_thread_| synchronously.
  webrtc::EncodedImageCallback* callback_;

  // State that is constant for the lifetime of this object once the ctor
  // returns.
  std::unique_ptr<Thread>
      codec_thread_;  // Thread on which to operate MediaCodec.
  rtc::ThreadChecker codec_thread_checker_;
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
  // Touched only on codec_thread_ so no explicit synchronization necessary.
  int width_;   // Frame width in pixels.
  int height_;  // Frame height in pixels.
  bool inited_;
  bool use_surface_;
  uint16_t picture_id_;
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
  int current_acc_qp_; // Accumulated QP in the current statistics interval.
  int current_encoding_time_ms_;  // Overall encoding time in the current second
  int64_t last_input_timestamp_ms_;  // Timestamp of last received yuv frame.
  int64_t last_output_timestamp_ms_;  // Timestamp of last encoded frame.

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
  int32_t output_timestamp_;      // Last output frame timestamp from
                                  // |input_frame_infos_|.
  int64_t output_render_time_ms_; // Last output frame render time from
                                  // |input_frame_infos_|.
  webrtc::VideoRotation output_rotation_;  // Last output frame rotation from
                                           // |input_frame_infos_|.
  // Frame size in bytes fed to MediaCodec.
  int yuv_size_;
  // True only when between a callback_->OnEncodedImage() call return a positive
  // value and the next Encode() call being ignored.
  bool drop_next_input_frame_;
  // Global references; must be deleted in Release().
  std::vector<jobject> input_buffers_;
  QualityScaler quality_scaler_;
  // Dynamic resolution change, off by default.
  bool scale_;

  // H264 bitstream parser, used to extract QP from encoded bitstreams.
  webrtc::H264BitstreamParser h264_bitstream_parser_;

  // VP9 variables to populate codec specific structure.
  webrtc::GofInfoVP9 gof_; // Contains each frame's temporal information for
                           // non-flexible VP9 mode.
  uint8_t tl0_pic_idx_;
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

  bool sw_fallback_required_;
};

MediaCodecVideoEncoder::~MediaCodecVideoEncoder() {
  // Call Release() to ensure no more callbacks to us after we are deleted.
  Release();
}

MediaCodecVideoEncoder::MediaCodecVideoEncoder(JNIEnv* jni,
                                               VideoCodecType codecType,
                                               jobject egl_context)
    : codecType_(codecType),
      callback_(NULL),
      codec_thread_(new Thread()),
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
      picture_id_(0),
      egl_context_(egl_context),
      sw_fallback_required_(false) {
  ScopedLocalRefFrame local_ref_frame(jni);
  // It would be nice to avoid spinning up a new thread per MediaCodec, and
  // instead re-use e.g. the PeerConnectionFactory's |worker_thread_|, but bug
  // 2732 means that deadlocks abound.  This class synchronously trampolines
  // to |codec_thread_|, so if anything else can be coming to _us_ from
  // |codec_thread_|, or from any thread holding the |_sendCritSect| described
  // in the bug, we have a problem.  For now work around that with a dedicated
  // thread.
  codec_thread_->SetName("MediaCodecVideoEncoder", NULL);
  RTC_CHECK(codec_thread_->Start()) << "Failed to start MediaCodecVideoEncoder";
  codec_thread_checker_.DetachFromThread();
  jclass j_output_buffer_info_class =
      FindClass(jni, "org/webrtc/MediaCodecVideoEncoder$OutputBufferInfo");
  j_init_encode_method_ = GetMethodID(
      jni,
      *j_media_codec_video_encoder_class_,
      "initEncode",
      "(Lorg/webrtc/MediaCodecVideoEncoder$VideoCodecType;"
      "IIIILorg/webrtc/EglBase14$Context;)Z");
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
    ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
  }
  srand(time(NULL));
  AllowBlockingCalls();
}

int32_t MediaCodecVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    int32_t /* number_of_cores */,
    size_t /* max_payload_size */) {
  if (codec_settings == NULL) {
    ALOGE << "NULL VideoCodec instance";
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  // Factory should guard against other codecs being used with us.
  RTC_CHECK(codec_settings->codecType == codecType_)
      << "Unsupported codec " << codec_settings->codecType << " for "
      << codecType_;
  if (sw_fallback_required_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  codec_mode_ = codec_settings->mode;
  int init_width = codec_settings->width;
  int init_height = codec_settings->height;
  // Scaling is disabled for VP9, but optionally enabled for VP8.
  // TODO(pbos): Extract automaticResizeOn out of VP8 settings.
  scale_ = false;
  if (codecType_ == kVideoCodecVP8) {
    scale_ = codec_settings->codecSpecific.VP8.automaticResizeOn;
  } else if (codecType_ != kVideoCodecVP9) {
    scale_ = true;
  }

  ALOGD << "InitEncode request: " << init_width << " x " << init_height;
  ALOGD << "Encoder automatic resize " << (scale_ ? "enabled" : "disabled");

  if (scale_) {
    if (codecType_ == kVideoCodecVP8 || codecType_ == kVideoCodecH264) {
      quality_scaler_.Init(codecType_, codec_settings->startBitrate,
                           codec_settings->width, codec_settings->height,
                           codec_settings->maxFramerate);
    } else {
      // When adding codec support to additional hardware codecs, also configure
      // their QP thresholds for scaling.
      RTC_NOTREACHED() << "Unsupported codec without configured QP thresholds.";
      scale_ = false;
    }
    QualityScaler::Resolution res = quality_scaler_.GetScaledResolution();
    init_width = res.width;
    init_height = res.height;
    ALOGD << "Scaled resolution: " << init_width << " x " << init_height;
  }

  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE,
      Bind(&MediaCodecVideoEncoder::InitEncodeOnCodecThread, this, init_width,
           init_height, codec_settings->startBitrate,
           codec_settings->maxFramerate,
           codec_settings->expect_encode_from_texture));
}

int32_t MediaCodecVideoEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const webrtc::CodecSpecificInfo* /* codec_specific_info */,
    const std::vector<webrtc::FrameType>* frame_types) {
  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE, Bind(&MediaCodecVideoEncoder::EncodeOnCodecThread, this,
                          frame, frame_types, rtc::TimeMillis()));
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE,
      Bind(&MediaCodecVideoEncoder::RegisterEncodeCompleteCallbackOnCodecThread,
           this, callback));
}

int32_t MediaCodecVideoEncoder::Release() {
  ALOGD << "EncoderRelease request";
  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE, Bind(&MediaCodecVideoEncoder::ReleaseOnCodecThread, this));
}

int32_t MediaCodecVideoEncoder::SetChannelParameters(uint32_t /* packet_loss */,
                                                     int64_t /* rtt */) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRates(uint32_t new_bit_rate,
                                         uint32_t frame_rate) {
  return codec_thread_->Invoke<int32_t>(
      RTC_FROM_HERE, Bind(&MediaCodecVideoEncoder::SetRatesOnCodecThread, this,
                          new_bit_rate, frame_rate));
}

void MediaCodecVideoEncoder::OnMessage(rtc::Message* msg) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  // We only ever send one message to |this| directly (not through a Bind()'d
  // functor), so expect no ID/data.
  RTC_CHECK(!msg->message_id) << "Unexpected message!";
  RTC_CHECK(!msg->pdata) << "Unexpected message!";
  if (!inited_) {
    return;
  }

  // It would be nice to recover from a failure here if one happened, but it's
  // unclear how to signal such a failure to the app, so instead we stay silent
  // about it and let the next app-called API method reveal the borkedness.
  DeliverPendingOutputs(jni);

  // If there aren't more frames to deliver, we can start polling at lower rate.
  if (input_frame_infos_.empty()) {
    codec_thread_->PostDelayed(RTC_FROM_HERE, kMediaCodecPollNoFramesMs, this);
  } else {
    codec_thread_->PostDelayed(RTC_FROM_HERE, kMediaCodecPollMs, this);
  }

  // Call log statistics here so it's called even if no frames are being
  // delivered.
  LogStatistics(false);
}

bool MediaCodecVideoEncoder::ResetCodecOnCodecThread() {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  ALOGE << "ResetOnCodecThread";
  if (ReleaseOnCodecThread() != WEBRTC_VIDEO_CODEC_OK) {
    ALOGE << "Releasing codec failed during reset.";
    return false;
  }
  if (InitEncodeOnCodecThread(width_, height_, 0, 0, false) !=
      WEBRTC_VIDEO_CODEC_OK) {
    ALOGE << "Initializing encoder failed during reset.";
    return false;
  }
  return true;
}

bool MediaCodecVideoEncoder::ProcessHWErrorOnCodecThread(
    bool reset_if_fallback_unavailable) {
  ALOGE << "ProcessHWErrorOnCodecThread";
  if (VideoEncoder::IsSupportedSoftware(
          VideoEncoder::CodecToEncoderType(codecType_))) {
    ALOGE << "Fallback to SW encoder.";
    sw_fallback_required_ = true;
    return false;
  } else if (reset_if_fallback_unavailable) {
    ALOGE << "Reset encoder.";
    return ResetCodecOnCodecThread();
  }
  return false;
}

int32_t MediaCodecVideoEncoder::ProcessHWErrorOnEncodeOnCodecThread() {
  ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
  return sw_fallback_required_ ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                               : WEBRTC_VIDEO_CODEC_ERROR;
}

int32_t MediaCodecVideoEncoder::InitEncodeOnCodecThread(
    int width, int height, int kbps, int fps, bool use_surface) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  if (sw_fallback_required_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  RTC_CHECK(!use_surface || egl_context_ != nullptr) << "EGL context not set.";
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  ALOGD << "InitEncodeOnCodecThread Type: " <<  (int)codecType_ << ", " <<
      width << " x " << height << ". Bitrate: " << kbps <<
      " kbps. Fps: " << fps;
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
  picture_id_ = static_cast<uint16_t>(rand()) & 0x7FFF;
  gof_.SetGofInfoVP9(webrtc::TemporalStructureMode::kTemporalStructureMode1);
  tl0_pic_idx_ = static_cast<uint8_t>(rand());
  gof_idx_ = 0;
  last_frame_received_ms_ = -1;
  frames_received_since_last_key_ = kMinKeyFrameInterval;

  // We enforce no extra stride/padding in the format creation step.
  jobject j_video_codec_enum = JavaEnumFromIndexAndClassName(
      jni, "MediaCodecVideoEncoder$VideoCodecType", codecType_);
  const bool encode_status = jni->CallBooleanMethod(
      *j_media_codec_video_encoder_, j_init_encode_method_,
      j_video_codec_enum, width, height, kbps, fps,
      (use_surface ? egl_context_ : nullptr));
  if (!encode_status) {
    ALOGE << "Failed to configure encoder.";
    ProcessHWErrorOnCodecThread(false /* reset_if_fallback_unavailable */);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (CheckException(jni)) {
    ALOGE << "Exception in init encode.";
    ProcessHWErrorOnCodecThread(false /* reset_if_fallback_unavailable */);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (!use_surface) {
    jobjectArray input_buffers = reinterpret_cast<jobjectArray>(
        jni->CallObjectMethod(*j_media_codec_video_encoder_,
            j_get_input_buffers_method_));
    if (CheckException(jni)) {
      ALOGE << "Exception in get input buffers.";
      ProcessHWErrorOnCodecThread(false /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    if (IsNull(jni, input_buffers)) {
      ProcessHWErrorOnCodecThread(false /* reset_if_fallback_unavailable */);
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
        ProcessHWErrorOnCodecThread(false /* reset_if_fallback_unavailable */);
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
        ProcessHWErrorOnCodecThread(false /* reset_if_fallback_unavailable */);
        return WEBRTC_VIDEO_CODEC_ERROR;
      }
      RTC_CHECK(yuv_buffer_capacity >= yuv_size_) << "Insufficient capacity";
    }
  }

  inited_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::EncodeOnCodecThread(
    const webrtc::VideoFrame& frame,
    const std::vector<webrtc::FrameType>* frame_types,
    const int64_t frame_input_time_ms) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  if (sw_fallback_required_)
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

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
    if (!ProcessHWErrorOnCodecThread(
            true /* reset_if_fallback_unavailable */)) {
      return sw_fallback_required_ ? WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE
                                   : WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  if (frames_encoded_ < kMaxEncodedLogFrames) {
    ALOGD << "Encoder frame in # " << (frames_received_ - 1)
          << ". TS: " << (int)(current_timestamp_us_ / 1000)
          << ". Q: " << input_frame_infos_.size() << ". Fps: " << last_set_fps_
          << ". Kbps: " << last_set_bitrate_kbps_;
  }

  if (drop_next_input_frame_) {
    ALOGW << "Encoder drop frame - failed callback.";
    drop_next_input_frame_ = false;
    current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;
    frames_dropped_media_encoder_++;
    OnDroppedFrameOnCodecThread();
    return WEBRTC_VIDEO_CODEC_OK;
  }

  RTC_CHECK(frame_types->size() == 1) << "Unexpected stream count";

  // Check if we accumulated too many frames in encoder input buffers and drop
  // frame if so.
  if (input_frame_infos_.size() > MAX_ENCODER_Q_SIZE) {
    ALOGD << "Already " << input_frame_infos_.size()
          << " frames in the queue, dropping"
          << ". TS: " << (int)(current_timestamp_us_ / 1000)
          << ". Fps: " << last_set_fps_
          << ". Consecutive drops: " << consecutive_full_queue_frame_drops_;
    current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;
    consecutive_full_queue_frame_drops_++;
    if (consecutive_full_queue_frame_drops_ >=
        ENCODER_STALL_FRAMEDROP_THRESHOLD) {
      ALOGE << "Encoder got stuck.";
      return ProcessHWErrorOnEncodeOnCodecThread();
    }
    frames_dropped_media_encoder_++;
    OnDroppedFrameOnCodecThread();
    return WEBRTC_VIDEO_CODEC_OK;
  }
  consecutive_full_queue_frame_drops_ = 0;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> input_buffer(
      frame.video_frame_buffer());
  if (scale_) {
    // Check framerate before spatial resolution change.
    quality_scaler_.OnEncodeFrame(frame.width(), frame.height());
    const webrtc::QualityScaler::Resolution scaled_resolution =
        quality_scaler_.GetScaledResolution();
    if (scaled_resolution.width != frame.width() ||
        scaled_resolution.height != frame.height()) {
      if (input_buffer->native_handle() != nullptr) {
        input_buffer = static_cast<AndroidTextureBuffer*>(input_buffer.get())
                           ->CropScaleAndRotate(frame.width(), frame.height(),
                                                0, 0,
                                                scaled_resolution.width,
                                                scaled_resolution.height,
                                                webrtc::kVideoRotation_0);
      } else {
        input_buffer = quality_scaler_.GetScaledBuffer(input_buffer);
      }
    }
  }

  VideoFrame input_frame(input_buffer, frame.timestamp(),
                         frame.render_time_ms(), frame.rotation());

  if (!MaybeReconfigureEncoderOnCodecThread(input_frame)) {
    ALOGE << "Failed to reconfigure encoder.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  const bool key_frame =
      frame_types->front() != webrtc::kVideoFrameDelta || send_key_frame;
  bool encode_status = true;
  if (!input_frame.video_frame_buffer()->native_handle()) {
    int j_input_buffer_index = jni->CallIntMethod(*j_media_codec_video_encoder_,
        j_dequeue_input_buffer_method_);
    if (CheckException(jni)) {
      ALOGE << "Exception in dequeu input buffer.";
      return ProcessHWErrorOnEncodeOnCodecThread();
    }
    if (j_input_buffer_index == -1) {
      // Video codec falls behind - no input buffer available.
      ALOGW << "Encoder drop frame - no input buffers available";
      if (frames_received_ > 1) {
        current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;
        frames_dropped_media_encoder_++;
        OnDroppedFrameOnCodecThread();
      } else {
        // Input buffers are not ready after codec initialization, HW is still
        // allocating thme - this is expected and should not result in drop
        // frame report.
        frames_received_ = 0;
      }
      return WEBRTC_VIDEO_CODEC_OK;  // TODO(fischman): see webrtc bug 2887.
    } else if (j_input_buffer_index == -2) {
      return ProcessHWErrorOnEncodeOnCodecThread();
    }
    encode_status = EncodeByteBufferOnCodecThread(jni, key_frame, input_frame,
        j_input_buffer_index);
  } else {
    encode_status = EncodeTextureOnCodecThread(jni, key_frame, input_frame);
  }

  if (!encode_status) {
    ALOGE << "Failed encode frame with timestamp: " << input_frame.timestamp();
    return ProcessHWErrorOnEncodeOnCodecThread();
  }

  // Save input image timestamps for later output.
  input_frame_infos_.emplace_back(
      frame_input_time_ms, input_frame.timestamp(),
      input_frame.render_time_ms(), input_frame.rotation());

  last_input_timestamp_ms_ =
      current_timestamp_us_ / rtc::kNumMicrosecsPerMillisec;

  current_timestamp_us_ += rtc::kNumMicrosecsPerSec / last_set_fps_;

  codec_thread_->Clear(this);
  codec_thread_->PostDelayed(RTC_FROM_HERE, kMediaCodecPollMs, this);

  if (!DeliverPendingOutputs(jni)) {
    return ProcessHWErrorOnEncodeOnCodecThread();
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoEncoder::MaybeReconfigureEncoderOnCodecThread(
    const webrtc::VideoFrame& frame) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());

  const bool is_texture_frame =
      frame.video_frame_buffer()->native_handle() != nullptr;
  const bool reconfigure_due_to_format = is_texture_frame != use_surface_;
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

  ReleaseOnCodecThread();

  return InitEncodeOnCodecThread(width_, height_, 0, 0 , is_texture_frame) ==
      WEBRTC_VIDEO_CODEC_OK;
}

bool MediaCodecVideoEncoder::EncodeByteBufferOnCodecThread(JNIEnv* jni,
    bool key_frame, const webrtc::VideoFrame& frame, int input_buffer_index) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  RTC_CHECK(!use_surface_);

  jobject j_input_buffer = input_buffers_[input_buffer_index];
  uint8_t* yuv_buffer =
      reinterpret_cast<uint8_t*>(jni->GetDirectBufferAddress(j_input_buffer));
  if (CheckException(jni)) {
    ALOGE << "Exception in get direct buffer address.";
    ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
    return false;
  }
  RTC_CHECK(yuv_buffer) << "Indirect buffer??";
  RTC_CHECK(!libyuv::ConvertFromI420(
      frame.video_frame_buffer()->DataY(),
      frame.video_frame_buffer()->StrideY(),
      frame.video_frame_buffer()->DataU(),
      frame.video_frame_buffer()->StrideU(),
      frame.video_frame_buffer()->DataV(),
      frame.video_frame_buffer()->StrideV(),
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
    ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
    return false;
  }
  return encode_status;
}

bool MediaCodecVideoEncoder::EncodeTextureOnCodecThread(JNIEnv* jni,
    bool key_frame, const webrtc::VideoFrame& frame) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  RTC_CHECK(use_surface_);
  NativeHandleImpl* handle = static_cast<NativeHandleImpl*>(
      frame.video_frame_buffer()->native_handle());
  jfloatArray sampling_matrix = handle->sampling_matrix.ToJava(jni);
  bool encode_status = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                              j_encode_texture_method_,
                                              key_frame,
                                              handle->oes_texture_id,
                                              sampling_matrix,
                                              current_timestamp_us_);
  if (CheckException(jni)) {
    ALOGE << "Exception in encode texture.";
    ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
    return false;
  }
  return encode_status;
}

int32_t MediaCodecVideoEncoder::RegisterEncodeCompleteCallbackOnCodecThread(
    webrtc::EncodedImageCallback* callback) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::ReleaseOnCodecThread() {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  if (!inited_) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ALOGD << "EncoderReleaseOnCodecThread: Frames received: " <<
      frames_received_ << ". Encoded: " << frames_encoded_ <<
      ". Dropped: " << frames_dropped_media_encoder_;
  ScopedLocalRefFrame local_ref_frame(jni);
  for (size_t i = 0; i < input_buffers_.size(); ++i)
    jni->DeleteGlobalRef(input_buffers_[i]);
  input_buffers_.clear();
  jni->CallVoidMethod(*j_media_codec_video_encoder_, j_release_method_);
  if (CheckException(jni)) {
    ALOGE << "Exception in release.";
    ProcessHWErrorOnCodecThread(false /* reset_if_fallback_unavailable */);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  rtc::MessageQueueManager::Clear(this);
  inited_ = false;
  use_surface_ = false;
  ALOGD << "EncoderReleaseOnCodecThread done.";
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t MediaCodecVideoEncoder::SetRatesOnCodecThread(uint32_t new_bit_rate,
                                                      uint32_t frame_rate) {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  if (sw_fallback_required_)
    return WEBRTC_VIDEO_CODEC_OK;
  frame_rate = (frame_rate < MAX_ALLOWED_VIDEO_FPS) ?
      frame_rate : MAX_ALLOWED_VIDEO_FPS;
  if (last_set_bitrate_kbps_ == new_bit_rate &&
      last_set_fps_ == frame_rate) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  if (scale_) {
    quality_scaler_.ReportFramerate(frame_rate);
  }
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  if (new_bit_rate > 0) {
    last_set_bitrate_kbps_ = new_bit_rate;
  }
  if (frame_rate > 0) {
    last_set_fps_ = frame_rate;
  }
  bool ret = jni->CallBooleanMethod(*j_media_codec_video_encoder_,
                                       j_set_rates_method_,
                                       last_set_bitrate_kbps_,
                                       last_set_fps_);
  if (CheckException(jni) || !ret) {
    ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
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
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());

  while (true) {
    jobject j_output_buffer_info = jni->CallObjectMethod(
        *j_media_codec_video_encoder_, j_dequeue_output_buffer_method_);
    if (CheckException(jni)) {
      ALOGE << "Exception in set dequeue output buffer.";
      ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    if (IsNull(jni, j_output_buffer_info)) {
      break;
    }

    int output_buffer_index =
        GetOutputBufferInfoIndex(jni, j_output_buffer_info);
    if (output_buffer_index == -1) {
      ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
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
      ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Callback - return encoded frame.
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
      image->_frameType =
          (key_frame ? webrtc::kVideoFrameKey : webrtc::kVideoFrameDelta);
      image->_completeFrame = true;
      image->adapt_reason_.quality_resolution_downscales =
          scale_ ? quality_scaler_.downscale_shift() : -1;

      webrtc::CodecSpecificInfo info;
      memset(&info, 0, sizeof(info));
      info.codecType = codecType_;
      if (codecType_ == kVideoCodecVP8) {
        info.codecSpecific.VP8.pictureId = picture_id_;
        info.codecSpecific.VP8.nonReference = false;
        info.codecSpecific.VP8.simulcastIdx = 0;
        info.codecSpecific.VP8.temporalIdx = webrtc::kNoTemporalIdx;
        info.codecSpecific.VP8.layerSync = false;
        info.codecSpecific.VP8.tl0PicIdx = webrtc::kNoTl0PicIdx;
        info.codecSpecific.VP8.keyIdx = webrtc::kNoKeyIdx;
      } else if (codecType_ == kVideoCodecVP9) {
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
      if (codecType_ == kVideoCodecVP8 || codecType_ == kVideoCodecVP9) {
        header.VerifyAndAllocateFragmentationHeader(1);
        header.fragmentationOffset[0] = 0;
        header.fragmentationLength[0] = image->_length;
        header.fragmentationPlType[0] = 0;
        header.fragmentationTimeDiff[0] = 0;
        if (codecType_ == kVideoCodecVP8 && scale_) {
          int qp;
          if (webrtc::vp8::GetQp(payload, payload_size, &qp)) {
            current_acc_qp_ += qp;
            quality_scaler_.ReportQP(qp);
            image->qp_ = qp;
          }
        }
      } else if (codecType_ == kVideoCodecH264) {
        if (scale_) {
          h264_bitstream_parser_.ParseBitstream(payload, payload_size);
          int qp;
          if (h264_bitstream_parser_.GetLastSliceQp(&qp)) {
            current_acc_qp_ += qp;
            quality_scaler_.ReportQP(qp);
          }
        }
        // For H.264 search for start codes.
        int32_t scPositions[MAX_NALUS_PERFRAME + 1] = {};
        int32_t scPositionsLength = 0;
        int32_t scPosition = 0;
        while (scPositionsLength < MAX_NALUS_PERFRAME) {
          int32_t naluPosition = NextNaluPosition(
              payload + scPosition, payload_size - scPosition);
          if (naluPosition < 0) {
            break;
          }
          scPosition += naluPosition;
          scPositions[scPositionsLength++] = scPosition;
          scPosition += H264_SC_LENGTH;
        }
        if (scPositionsLength == 0) {
          ALOGE << "Start code is not found!";
          ALOGE << "Data:" <<  image->_buffer[0] << " " << image->_buffer[1]
              << " " << image->_buffer[2] << " " << image->_buffer[3]
              << " " << image->_buffer[4] << " " << image->_buffer[5];
          ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
          return false;
        }
        scPositions[scPositionsLength] = payload_size;
        header.VerifyAndAllocateFragmentationHeader(scPositionsLength);
        for (size_t i = 0; i < scPositionsLength; i++) {
          header.fragmentationOffset[i] = scPositions[i] + H264_SC_LENGTH;
          header.fragmentationLength[i] =
              scPositions[i + 1] - header.fragmentationOffset[i];
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
      ProcessHWErrorOnCodecThread(true /* reset_if_fallback_unavailable */);
      return false;
    }

    // Print per frame statistics.
    if (encoding_start_time_ms > 0) {
      frame_encoding_time_ms = rtc::TimeMillis() - encoding_start_time_ms;
    }
    if (frames_encoded_ < kMaxEncodedLogFrames) {
      int current_latency =
          (int)(last_input_timestamp_ms_ - last_output_timestamp_ms_);
      ALOGD << "Encoder frame out # " << frames_encoded_ <<
          ". Key: " << key_frame <<
          ". Size: " << payload_size <<
          ". TS: " << (int)last_output_timestamp_ms_ <<
          ". Latency: " << current_latency <<
          ". EncTime: " << frame_encoding_time_ms;
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

int32_t MediaCodecVideoEncoder::NextNaluPosition(
    uint8_t *buffer, size_t buffer_size) {
  if (buffer_size < H264_SC_LENGTH) {
    return -1;
  }
  uint8_t *head = buffer;
  // Set end buffer pointer to 4 bytes before actual buffer end so we can
  // access head[1], head[2] and head[3] in a loop without buffer overrun.
  uint8_t *end = buffer + buffer_size - H264_SC_LENGTH;

  while (head < end) {
    if (head[0]) {
      head++;
      continue;
    }
    if (head[1]) { // got 00xx
      head += 2;
      continue;
    }
    if (head[2]) { // got 0000xx
      head += 3;
      continue;
    }
    if (head[3] != 0x01) { // got 000000xx
      head++; // xx != 1, continue searching.
      continue;
    }
    return (int32_t)(head - buffer);
  }
  return -1;
}

void MediaCodecVideoEncoder::OnDroppedFrame() {
  // Methods running on the codec thread should call OnDroppedFrameOnCodecThread
  // directly.
  RTC_DCHECK(!codec_thread_checker_.CalledOnValidThread());
  codec_thread_->Invoke<void>(
      RTC_FROM_HERE,
      Bind(&MediaCodecVideoEncoder::OnDroppedFrameOnCodecThread, this));
}

void MediaCodecVideoEncoder::OnDroppedFrameOnCodecThread() {
  RTC_DCHECK(codec_thread_checker_.CalledOnValidThread());
  // Report dropped frame to quality_scaler_.
  if (scale_)
    quality_scaler_.ReportDroppedFrame();
}

const char* MediaCodecVideoEncoder::ImplementationName() const {
  return "MediaCodec";
}

MediaCodecVideoEncoderFactory::MediaCodecVideoEncoderFactory()
    : egl_context_(nullptr) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jclass j_encoder_class = FindClass(jni, "org/webrtc/MediaCodecVideoEncoder");
  supported_codecs_.clear();

  bool is_vp8_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isVp8HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_vp8_hw_supported) {
    ALOGD << "VP8 HW Encoder supported.";
    supported_codecs_.push_back(cricket::VideoCodec("VP8"));
  }

  bool is_vp9_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isVp9HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_vp9_hw_supported) {
    ALOGD << "VP9 HW Encoder supported.";
    supported_codecs_.push_back(cricket::VideoCodec("VP9"));
  }

  bool is_h264_hw_supported = jni->CallStaticBooleanMethod(
      j_encoder_class,
      GetStaticMethodID(jni, j_encoder_class, "isH264HwSupported", "()Z"));
  CHECK_EXCEPTION(jni);
  if (is_h264_hw_supported) {
    ALOGD << "H.264 HW Encoder supported.";
    supported_codecs_.push_back(cricket::VideoCodec("H264"));
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
  if (supported_codecs_.empty()) {
    ALOGW << "No HW video encoder for codec " << codec.name;
    return nullptr;
  }
  if (FindMatchingCodec(supported_codecs_, codec)) {
    ALOGD << "Create HW video encoder for " << codec.name;
    const VideoCodecType type = cricket::CodecTypeFromName(codec.name);
    return new MediaCodecVideoEncoder(AttachCurrentThreadIfNeeded(), type,
                                      egl_context_);
  }
  ALOGW << "Can not find HW video encoder for type " << codec.name;
  return nullptr;
}

const std::vector<cricket::VideoCodec>&
MediaCodecVideoEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

void MediaCodecVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  ALOGD << "Destroy video encoder.";
  delete encoder;
}

}  // namespace webrtc_jni
