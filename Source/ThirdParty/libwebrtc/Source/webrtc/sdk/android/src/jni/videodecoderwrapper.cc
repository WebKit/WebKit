/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/videodecoderwrapper.h"

#include "api/video/video_frame.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"
#include "rtc_base/logging.h"
#include "sdk/android/generated_video_jni/jni/VideoDecoderWrapper_jni.h"
#include "sdk/android/generated_video_jni/jni/VideoDecoder_jni.h"
#include "sdk/android/src/jni/classreferenceholder.h"
#include "sdk/android/src/jni/encodedimage.h"
#include "sdk/android/src/jni/videocodecstatus.h"
#include "sdk/android/src/jni/videoframe.h"

namespace webrtc {
namespace jni {

namespace {
// RTP timestamps are 90 kHz.
const int64_t kNumRtpTicksPerMillisec = 90000 / rtc::kNumMillisecsPerSec;

template <typename Dst, typename Src>
inline rtc::Optional<Dst> cast_optional(const rtc::Optional<Src>& value) {
  return value ? rtc::Optional<Dst>(rtc::dchecked_cast<Dst, Src>(*value))
               : rtc::nullopt;
}
}  // namespace

VideoDecoderWrapper::VideoDecoderWrapper(JNIEnv* jni, jobject decoder)
    : decoder_(jni, decoder) {
  initialized_ = false;
  // QP parsing starts enabled and we disable it if the decoder provides frames.
  qp_parsing_enabled_ = true;

  implementation_name_ = JavaToStdString(
      jni, Java_VideoDecoder_getImplementationName(jni, decoder));
}

int32_t VideoDecoderWrapper::InitDecode(const VideoCodec* codec_settings,
                                        int32_t number_of_cores) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  return InitDecodeInternal(jni);
}

int32_t VideoDecoderWrapper::InitDecodeInternal(JNIEnv* jni) {
  jobject settings = Java_Settings_Constructor(
      jni, number_of_cores_, codec_settings_.width, codec_settings_.height);

  jobject callback = Java_VideoDecoderWrapper_createDecoderCallback(
      jni, jlongFromPointer(this));

  jobject ret =
      Java_VideoDecoder_initDecode(jni, *decoder_, settings, callback);
  if (JavaToNativeVideoCodecStatus(jni, ret) == WEBRTC_VIDEO_CODEC_OK) {
    initialized_ = true;
  }

  // The decoder was reinitialized so re-enable the QP parsing in case it stops
  // providing QP values.
  qp_parsing_enabled_ = true;

  return HandleReturnCode(jni, ret);
}

int32_t VideoDecoderWrapper::Decode(
    const EncodedImage& image_param,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  if (!initialized_) {
    // Most likely initializing the codec failed.
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  }

  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);

  // Make a mutable copy so we can modify the timestamp.
  EncodedImage input_image(image_param);
  // We use RTP timestamp for capture time because capture_time_ms_ is always 0.
  input_image.capture_time_ms_ =
      input_image._timeStamp / kNumRtpTicksPerMillisec;

  FrameExtraInfo frame_extra_info;
  frame_extra_info.timestamp_ns =
      input_image.capture_time_ms_ * rtc::kNumNanosecsPerMillisec;
  frame_extra_info.timestamp_rtp = input_image._timeStamp;
  frame_extra_info.timestamp_ntp = input_image.ntp_time_ms_;
  frame_extra_info.qp =
      qp_parsing_enabled_ ? ParseQP(input_image) : rtc::nullopt;
  frame_extra_infos_.push_back(frame_extra_info);

  jobject jinput_image = NativeToJavaEncodedImage(jni, input_image);
  jobject ret = Java_VideoDecoder_decode(jni, *decoder_, jinput_image, nullptr);
  return HandleReturnCode(jni, ret);
}

int32_t VideoDecoderWrapper::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoDecoderWrapper::Release() {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  ScopedLocalRefFrame local_ref_frame(jni);
  jobject ret = Java_VideoDecoder_release(jni, *decoder_);
  frame_extra_infos_.clear();
  initialized_ = false;
  return HandleReturnCode(jni, ret);
}

bool VideoDecoderWrapper::PrefersLateDecoding() const {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  return Java_VideoDecoder_getPrefersLateDecoding(jni, *decoder_);
}

const char* VideoDecoderWrapper::ImplementationName() const {
  return implementation_name_.c_str();
}

void VideoDecoderWrapper::OnDecodedFrame(JNIEnv* env,
                                         jobject j_caller,
                                         jobject j_frame,
                                         jobject j_decode_time_ms,
                                         jobject j_qp) {
  const uint64_t timestamp_ns = GetJavaVideoFrameTimestampNs(env, j_frame);

  FrameExtraInfo frame_extra_info;
  do {
    if (frame_extra_infos_.empty()) {
      RTC_LOG(LS_WARNING) << "Java decoder produced an unexpected frame: "
                          << timestamp_ns;
      return;
    }

    frame_extra_info = frame_extra_infos_.front();
    frame_extra_infos_.pop_front();
    // If the decoder might drop frames so iterate through the queue until we
    // find a matching timestamp.
  } while (frame_extra_info.timestamp_ns != timestamp_ns);

  VideoFrame frame =
      JavaToNativeFrame(env, j_frame, frame_extra_info.timestamp_rtp);
  frame.set_ntp_time_ms(frame_extra_info.timestamp_ntp);

  rtc::Optional<int32_t> decoding_time_ms =
      JavaToNativeOptionalInt(env, j_decode_time_ms);

  rtc::Optional<uint8_t> decoder_qp =
      cast_optional<uint8_t, int32_t>(JavaToNativeOptionalInt(env, j_qp));
  // If the decoder provides QP values itself, no need to parse the bitstream.
  // Enable QP parsing if decoder does not provide QP values itself.
  qp_parsing_enabled_ = !decoder_qp.has_value();
  callback_->Decoded(frame, decoding_time_ms,
                     decoder_qp ? decoder_qp : frame_extra_info.qp);
}

int32_t VideoDecoderWrapper::HandleReturnCode(JNIEnv* jni, jobject code) {
  int32_t value = JavaToNativeVideoCodecStatus(jni, code);
  if (value < 0) {  // Any errors are represented by negative values.
    // Reset the codec.
    if (Release() == WEBRTC_VIDEO_CODEC_OK) {
      InitDecodeInternal(jni);
    }

    RTC_LOG(LS_WARNING) << "Falling back to software decoder.";
    return WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
  } else {
    return value;
  }
}

rtc::Optional<uint8_t> VideoDecoderWrapper::ParseQP(
    const EncodedImage& input_image) {
  if (input_image.qp_ != -1) {
    return input_image.qp_;
  }

  rtc::Optional<uint8_t> qp;
  switch (codec_settings_.codecType) {
    case kVideoCodecVP8: {
      int qp_int;
      if (vp8::GetQp(input_image._buffer, input_image._length, &qp_int)) {
        qp = qp_int;
      }
      break;
    }
    case kVideoCodecVP9: {
      int qp_int;
      if (vp9::GetQp(input_image._buffer, input_image._length, &qp_int)) {
        qp = qp_int;
      }
      break;
    }
    case kVideoCodecH264: {
      h264_bitstream_parser_.ParseBitstream(input_image._buffer,
                                            input_image._length);
      int qp_int;
      if (h264_bitstream_parser_.GetLastSliceQp(&qp_int)) {
        qp = qp_int;
      }
      break;
    }
    default:
      break;  // Default is to not provide QP.
  }
  return qp;
}

}  // namespace jni
}  // namespace webrtc
