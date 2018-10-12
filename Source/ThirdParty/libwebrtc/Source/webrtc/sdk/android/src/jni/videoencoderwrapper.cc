/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videoencoderwrapper.h"

#include <utility>

#include "common_video/h264/h264_common.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/timeutils.h"
#include "sdk/android/generated_video_jni/jni/VideoEncoderWrapper_jni.h"
#include "sdk/android/generated_video_jni/jni/VideoEncoder_jni.h"
#include "sdk/android/native_api/jni/class_loader.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/encodedimage.h"
#include "sdk/android/src/jni/videocodecstatus.h"

namespace webrtc {
namespace jni {

VideoEncoderWrapper::VideoEncoderWrapper(JNIEnv* jni,
                                         const JavaRef<jobject>& j_encoder)
    : encoder_(jni, j_encoder), int_array_class_(GetClass(jni, "[I")) {
  implementation_name_ = GetImplementationName(jni);

  initialized_ = false;
  num_resets_ = 0;
}
VideoEncoderWrapper::~VideoEncoderWrapper() = default;

int32_t VideoEncoderWrapper::InitEncode(const VideoCodec* codec_settings,
                                        int32_t number_of_cores,
                                        size_t max_payload_size) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();

  number_of_cores_ = number_of_cores;
  codec_settings_ = *codec_settings;
  num_resets_ = 0;
  encoder_queue_ = rtc::TaskQueue::Current();

  return InitEncodeInternal(jni);
}

int32_t VideoEncoderWrapper::InitEncodeInternal(JNIEnv* jni) {
  bool automatic_resize_on;
  switch (codec_settings_.codecType) {
    case kVideoCodecVP8:
      automatic_resize_on = codec_settings_.VP8()->automaticResizeOn;
      break;
    case kVideoCodecVP9:
      automatic_resize_on = codec_settings_.VP9()->automaticResizeOn;
      gof_.SetGofInfoVP9(TemporalStructureMode::kTemporalStructureMode1);
      gof_idx_ = 0;
      break;
    default:
      automatic_resize_on = true;
  }

  ScopedJavaLocalRef<jobject> settings = Java_Settings_Constructor(
      jni, number_of_cores_, codec_settings_.width, codec_settings_.height,
      static_cast<int>(codec_settings_.startBitrate),
      static_cast<int>(codec_settings_.maxFramerate),
      static_cast<int>(codec_settings_.numberOfSimulcastStreams),
      automatic_resize_on);

  ScopedJavaLocalRef<jobject> callback =
      Java_VideoEncoderWrapper_createEncoderCallback(jni,
                                                     jlongFromPointer(this));

  int32_t status = JavaToNativeVideoCodecStatus(
      jni, Java_VideoEncoder_initEncode(jni, encoder_, settings, callback));
  RTC_LOG(LS_INFO) << "initEncode: " << status;

  if (status == WEBRTC_VIDEO_CODEC_OK) {
    initialized_ = true;
  }
  return status;
}

int32_t VideoEncoderWrapper::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoEncoderWrapper::Release() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();

  int32_t status = JavaToNativeVideoCodecStatus(
      jni, Java_VideoEncoder_release(jni, encoder_));
  RTC_LOG(LS_INFO) << "release: " << status;
  frame_extra_infos_.clear();
  initialized_ = false;
  encoder_queue_ = nullptr;

  return status;
}

int32_t VideoEncoderWrapper::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* /* codec_specific_info */,
    const std::vector<FrameType>* frame_types) {
  if (!initialized_) {
    // Most likely initializing the codec failed.
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  JNIEnv* jni = AttachCurrentThreadIfNeeded();

  // Construct encode info.
  ScopedJavaLocalRef<jobjectArray> j_frame_types =
      NativeToJavaFrameTypeArray(jni, *frame_types);
  ScopedJavaLocalRef<jobject> encode_info =
      Java_EncodeInfo_Constructor(jni, j_frame_types);

  FrameExtraInfo info;
  info.capture_time_ns = frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec;
  info.timestamp_rtp = frame.timestamp();
  frame_extra_infos_.push_back(info);

  ScopedJavaLocalRef<jobject> j_frame = NativeToJavaVideoFrame(jni, frame);
  ScopedJavaLocalRef<jobject> ret =
      Java_VideoEncoder_encode(jni, encoder_, j_frame, encode_info);
  ReleaseJavaVideoFrame(jni, j_frame);
  return HandleReturnCode(jni, ret, "encode");
}

int32_t VideoEncoderWrapper::SetChannelParameters(uint32_t packet_loss,
                                                  int64_t rtt) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> ret = Java_VideoEncoder_setChannelParameters(
      jni, encoder_, (jshort)packet_loss, (jlong)rtt);
  return HandleReturnCode(jni, ret, "setChannelParameters");
}

int32_t VideoEncoderWrapper::SetRateAllocation(
    const VideoBitrateAllocation& allocation,
    uint32_t framerate) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();

  ScopedJavaLocalRef<jobject> j_bitrate_allocation =
      ToJavaBitrateAllocation(jni, allocation);
  ScopedJavaLocalRef<jobject> ret = Java_VideoEncoder_setRateAllocation(
      jni, encoder_, j_bitrate_allocation, (jint)framerate);
  return HandleReturnCode(jni, ret, "setRateAllocation");
}

VideoEncoderWrapper::ScalingSettings VideoEncoderWrapper::GetScalingSettings()
    const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedJavaLocalRef<jobject> j_scaling_settings =
      Java_VideoEncoder_getScalingSettings(jni, encoder_);
  bool isOn =
      Java_VideoEncoderWrapper_getScalingSettingsOn(jni, j_scaling_settings);

  if (!isOn)
    return ScalingSettings::kOff;

  absl::optional<int> low = JavaToNativeOptionalInt(
      jni,
      Java_VideoEncoderWrapper_getScalingSettingsLow(jni, j_scaling_settings));
  absl::optional<int> high = JavaToNativeOptionalInt(
      jni,
      Java_VideoEncoderWrapper_getScalingSettingsHigh(jni, j_scaling_settings));

  if (low && high)
    return ScalingSettings(*low, *high);

  switch (codec_settings_.codecType) {
    case kVideoCodecVP8: {
      // Same as in vp8_impl.cc.
      static const int kLowVp8QpThreshold = 29;
      static const int kHighVp8QpThreshold = 95;
      return ScalingSettings(low.value_or(kLowVp8QpThreshold),
                             high.value_or(kHighVp8QpThreshold));
    }
    case kVideoCodecVP9: {
      // QP is obtained from VP9-bitstream, so the QP corresponds to the
      // bitstream range of [0, 255] and not the user-level range of [0,63].
      static const int kLowVp9QpThreshold = 96;
      static const int kHighVp9QpThreshold = 185;

      return VideoEncoder::ScalingSettings(kLowVp9QpThreshold,
                                           kHighVp9QpThreshold);
    }
    case kVideoCodecH264: {
      // Same as in h264_encoder_impl.cc.
      static const int kLowH264QpThreshold = 24;
      static const int kHighH264QpThreshold = 37;
      return ScalingSettings(low.value_or(kLowH264QpThreshold),
                             high.value_or(kHighH264QpThreshold));
    }
    default:
      return ScalingSettings::kOff;
  }
}

bool VideoEncoderWrapper::SupportsNativeHandle() const {
  return true;
}

const char* VideoEncoderWrapper::ImplementationName() const {
  return implementation_name_.c_str();
}

void VideoEncoderWrapper::OnEncodedFrame(JNIEnv* jni,
                                         const JavaRef<jobject>& j_caller,
                                         const JavaRef<jobject>& j_buffer,
                                         jint encoded_width,
                                         jint encoded_height,
                                         jlong capture_time_ns,
                                         jint frame_type,
                                         jint rotation,
                                         jboolean complete_frame,
                                         const JavaRef<jobject>& j_qp) {
  const uint8_t* buffer =
      static_cast<uint8_t*>(jni->GetDirectBufferAddress(j_buffer.obj()));
  const size_t buffer_size = jni->GetDirectBufferCapacity(j_buffer.obj());

  std::vector<uint8_t> buffer_copy(buffer_size);
  memcpy(buffer_copy.data(), buffer, buffer_size);
  const int qp = JavaToNativeOptionalInt(jni, j_qp).value_or(-1);

  struct Lambda {
    VideoEncoderWrapper* video_encoder_wrapper;
    std::vector<uint8_t> task_buffer;
    int qp;
    jint encoded_width;
    jint encoded_height;
    jlong capture_time_ns;
    jint frame_type;
    jint rotation;
    jboolean complete_frame;
    std::deque<FrameExtraInfo>* frame_extra_infos;
    EncodedImageCallback* callback;

    void operator()() const {
      // Encoded frames are delivered in the order received, but some of them
      // may be dropped, so remove records of frames older than the current one.
      //
      // NOTE: if the current frame is associated with Encoder A, in the time
      // since this frame was received, Encoder A could have been Release()'ed,
      // Encoder B InitEncode()'ed (due to reuse of Encoder A), and frames
      // received by Encoder B. Thus there may be frame_extra_infos entries that
      // don't belong to us, and we need to be careful not to remove them.
      // Removing only those entries older than the current frame provides this
      // guarantee.
      while (!frame_extra_infos->empty() &&
             frame_extra_infos->front().capture_time_ns < capture_time_ns) {
        frame_extra_infos->pop_front();
      }
      if (frame_extra_infos->empty() ||
          frame_extra_infos->front().capture_time_ns != capture_time_ns) {
        RTC_LOG(LS_WARNING)
            << "Java encoder produced an unexpected frame with timestamp: "
            << capture_time_ns;
        return;
      }
      FrameExtraInfo frame_extra_info = std::move(frame_extra_infos->front());
      frame_extra_infos->pop_front();

      RTPFragmentationHeader header =
          video_encoder_wrapper->ParseFragmentationHeader(task_buffer);
      EncodedImage frame(const_cast<uint8_t*>(task_buffer.data()),
                         task_buffer.size(), task_buffer.size());
      frame._encodedWidth = encoded_width;
      frame._encodedHeight = encoded_height;
      frame.SetTimestamp(frame_extra_info.timestamp_rtp);
      frame.capture_time_ms_ = capture_time_ns / rtc::kNumNanosecsPerMillisec;
      frame._frameType = (FrameType)frame_type;
      frame.rotation_ = (VideoRotation)rotation;
      frame._completeFrame = complete_frame;
      if (qp == -1) {
        frame.qp_ = video_encoder_wrapper->ParseQp(task_buffer);
      } else {
        frame.qp_ = qp;
      }

      CodecSpecificInfo info(
          video_encoder_wrapper->ParseCodecSpecificInfo(frame));
      callback->OnEncodedImage(frame, &info, &header);
    }
  };

  encoder_queue_->PostTask(
      Lambda{this, std::move(buffer_copy), qp, encoded_width, encoded_height,
             capture_time_ns, frame_type, rotation, complete_frame,
             &frame_extra_infos_, callback_});
}

int32_t VideoEncoderWrapper::HandleReturnCode(JNIEnv* jni,
                                              const JavaRef<jobject>& j_value,
                                              const char* method_name) {
  int32_t value = JavaToNativeVideoCodecStatus(jni, j_value);
  if (value >= 0) {  // OK or NO_OUTPUT
    return value;
  }

  RTC_LOG(LS_WARNING) << method_name << ": " << value;
  if (value == WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE ||
      value == WEBRTC_VIDEO_CODEC_UNINITIALIZED) {  // Critical error.
    RTC_LOG(LS_WARNING) << "Java encoder requested software fallback.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  // Try resetting the codec.
  if (Release() == WEBRTC_VIDEO_CODEC_OK &&
      InitEncodeInternal(jni) == WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "Reset Java encoder.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  RTC_LOG(LS_WARNING) << "Unable to reset Java encoder.";
  return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
}

RTPFragmentationHeader VideoEncoderWrapper::ParseFragmentationHeader(
    const std::vector<uint8_t>& buffer) {
  RTPFragmentationHeader header;
  if (codec_settings_.codecType == kVideoCodecH264) {
    h264_bitstream_parser_.ParseBitstream(buffer.data(), buffer.size());

    // For H.264 search for start codes.
    const std::vector<H264::NaluIndex> nalu_idxs =
        H264::FindNaluIndices(buffer.data(), buffer.size());
    if (nalu_idxs.empty()) {
      RTC_LOG(LS_ERROR) << "Start code is not found!";
      RTC_LOG(LS_ERROR) << "Data:" << buffer[0] << " " << buffer[1] << " "
                        << buffer[2] << " " << buffer[3] << " " << buffer[4]
                        << " " << buffer[5];
    }
    header.VerifyAndAllocateFragmentationHeader(nalu_idxs.size());
    for (size_t i = 0; i < nalu_idxs.size(); i++) {
      header.fragmentationOffset[i] = nalu_idxs[i].payload_start_offset;
      header.fragmentationLength[i] = nalu_idxs[i].payload_size;
      header.fragmentationPlType[i] = 0;
      header.fragmentationTimeDiff[i] = 0;
    }
  } else {
    // Generate a header describing a single fragment.
    header.VerifyAndAllocateFragmentationHeader(1);
    header.fragmentationOffset[0] = 0;
    header.fragmentationLength[0] = buffer.size();
    header.fragmentationPlType[0] = 0;
    header.fragmentationTimeDiff[0] = 0;
  }
  return header;
}

int VideoEncoderWrapper::ParseQp(const std::vector<uint8_t>& buffer) {
  int qp;
  bool success;
  switch (codec_settings_.codecType) {
    case kVideoCodecVP8:
      success = vp8::GetQp(buffer.data(), buffer.size(), &qp);
      break;
    case kVideoCodecVP9:
      success = vp9::GetQp(buffer.data(), buffer.size(), &qp);
      break;
    case kVideoCodecH264:
      success = h264_bitstream_parser_.GetLastSliceQp(&qp);
      break;
    default:  // Default is to not provide QP.
      success = false;
      break;
  }
  return success ? qp : -1;  // -1 means unknown QP.
}

CodecSpecificInfo VideoEncoderWrapper::ParseCodecSpecificInfo(
    const EncodedImage& frame) {
  const bool key_frame = frame._frameType == kVideoFrameKey;

  CodecSpecificInfo info;
  memset(&info, 0, sizeof(info));
  info.codecType = codec_settings_.codecType;
  info.codec_name = implementation_name_.c_str();

  switch (codec_settings_.codecType) {
    case kVideoCodecVP8:
      info.codecSpecific.VP8.nonReference = false;
      info.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;
      info.codecSpecific.VP8.layerSync = false;
      info.codecSpecific.VP8.keyIdx = kNoKeyIdx;
      break;
    case kVideoCodecVP9:
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
        info.codecSpecific.VP9.width[0] = frame._encodedWidth;
        info.codecSpecific.VP9.height[0] = frame._encodedHeight;
        info.codecSpecific.VP9.gof.CopyGofInfoVP9(gof_);
      }
      break;
    default:
      break;
  }

  return info;
}

ScopedJavaLocalRef<jobject> VideoEncoderWrapper::ToJavaBitrateAllocation(
    JNIEnv* jni,
    const VideoBitrateAllocation& allocation) {
  ScopedJavaLocalRef<jobjectArray> j_allocation_array(
      jni, jni->NewObjectArray(kMaxSpatialLayers, int_array_class_.obj(),
                               nullptr /* initial */));
  for (int spatial_i = 0; spatial_i < kMaxSpatialLayers; ++spatial_i) {
    ScopedJavaLocalRef<jintArray> j_array_spatial_layer(
        jni, jni->NewIntArray(kMaxTemporalStreams));
    jint* array_spatial_layer = jni->GetIntArrayElements(
        j_array_spatial_layer.obj(), nullptr /* isCopy */);
    for (int temporal_i = 0; temporal_i < kMaxTemporalStreams; ++temporal_i) {
      array_spatial_layer[temporal_i] =
          allocation.GetBitrate(spatial_i, temporal_i);
    }
    jni->ReleaseIntArrayElements(j_array_spatial_layer.obj(),
                                 array_spatial_layer, JNI_COMMIT);

    jni->SetObjectArrayElement(j_allocation_array.obj(), spatial_i,
                               j_array_spatial_layer.obj());
  }
  return Java_BitrateAllocation_Constructor(jni, j_allocation_array);
}

std::string VideoEncoderWrapper::GetImplementationName(JNIEnv* jni) const {
  return JavaToStdString(
      jni, Java_VideoEncoder_getImplementationName(jni, encoder_));
}

std::unique_ptr<VideoEncoder> JavaToNativeVideoEncoder(
    JNIEnv* jni,
    const JavaRef<jobject>& j_encoder) {
  const jlong native_encoder =
      Java_VideoEncoder_createNativeVideoEncoder(jni, j_encoder);
  VideoEncoder* encoder;
  if (native_encoder == 0) {
    encoder = new VideoEncoderWrapper(jni, j_encoder);
  } else {
    encoder = reinterpret_cast<VideoEncoder*>(native_encoder);
  }
  return std::unique_ptr<VideoEncoder>(encoder);
}

bool IsHardwareVideoEncoder(JNIEnv* jni, const JavaRef<jobject>& j_encoder) {
  return Java_VideoEncoder_isHardwareEncoder(jni, j_encoder);
}

}  // namespace jni
}  // namespace webrtc
