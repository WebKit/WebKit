/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_
#define SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_

#include <jni.h>
#include <deque>

#include "api/video_codecs/video_decoder.h"
#include "common_video/h264/h264_bitstream_parser.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

// Wraps a Java decoder and delegates all calls to it.
class VideoDecoderWrapper : public VideoDecoder {
 public:
  VideoDecoderWrapper(JNIEnv* jni, jobject decoder);

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;

  // Returns true if the decoder prefer to decode frames late.
  // That is, it can not decode infinite number of frames before the decoded
  // frame is consumed.
  bool PrefersLateDecoding() const override;

  const char* ImplementationName() const override;

  // Wraps the frame to a AndroidVideoBuffer and passes it to the callback.
  void OnDecodedFrame(JNIEnv* env,
                      jobject j_caller,
                      jobject j_frame,
                      jobject j_decode_time_ms,
                      jobject j_qp);

 private:
  struct FrameExtraInfo {
    int64_t timestamp_ns;  // Used as an identifier of the frame.

    uint32_t timestamp_rtp;
    int64_t timestamp_ntp;
    rtc::Optional<uint8_t> qp;
  };

  int32_t InitDecodeInternal(JNIEnv* jni);

  // Takes Java VideoCodecStatus, handles it and returns WEBRTC_VIDEO_CODEC_*
  // status code.
  int32_t HandleReturnCode(JNIEnv* jni, jobject code);

  rtc::Optional<uint8_t> ParseQP(const EncodedImage& input_image);

  VideoCodec codec_settings_;
  int32_t number_of_cores_;

  bool initialized_;
  std::deque<FrameExtraInfo> frame_extra_infos_;
  bool qp_parsing_enabled_;
  H264BitstreamParser h264_bitstream_parser_;
  std::string implementation_name_;

  DecodedImageCallback* callback_;

  const ScopedGlobalRef<jobject> decoder_;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_VIDEODECODERWRAPPER_H_
