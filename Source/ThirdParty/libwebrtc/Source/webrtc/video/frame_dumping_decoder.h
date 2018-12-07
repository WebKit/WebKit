/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_FRAME_DUMPING_DECODER_H_
#define VIDEO_FRAME_DUMPING_DECODER_H_

#include <memory>

#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/platform_file.h"

namespace webrtc {

// A decoder wrapper that writes the encoded frames to a file.
class FrameDumpingDecoder : public VideoDecoder {
 public:
  FrameDumpingDecoder(std::unique_ptr<VideoDecoder> decoder,
                      rtc::PlatformFile file);
  ~FrameDumpingDecoder() override;

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;
  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;
  int32_t Release() override;
  bool PrefersLateDecoding() const override;
  const char* ImplementationName() const override;

 private:
  std::unique_ptr<VideoDecoder> decoder_;
  std::unique_ptr<IvfFileWriter> writer_;
};

}  // namespace webrtc

#endif  // VIDEO_FRAME_DUMPING_DECODER_H_
