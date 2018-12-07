/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_dumping_decoder.h"

#include <utility>

#include "modules/video_coding/include/video_codec_interface.h"

namespace webrtc {

FrameDumpingDecoder::FrameDumpingDecoder(std::unique_ptr<VideoDecoder> decoder,
                                         rtc::PlatformFile file)
    : decoder_(std::move(decoder)),
      writer_(IvfFileWriter::Wrap(rtc::File(file),
                                  /* byte_limit= */ 100000000)) {}

FrameDumpingDecoder::~FrameDumpingDecoder() = default;

int32_t FrameDumpingDecoder::InitDecode(const VideoCodec* codec_settings,
                                        int32_t number_of_cores) {
  return decoder_->InitDecode(codec_settings, number_of_cores);
}

int32_t FrameDumpingDecoder::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  int32_t ret = decoder_->Decode(input_image, missing_frames,
                                 codec_specific_info, render_time_ms);
  writer_->WriteFrame(input_image, codec_specific_info->codecType);

  return ret;
}

int32_t FrameDumpingDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  return decoder_->RegisterDecodeCompleteCallback(callback);
}

int32_t FrameDumpingDecoder::Release() {
  return decoder_->Release();
}

bool FrameDumpingDecoder::PrefersLateDecoding() const {
  return decoder_->PrefersLateDecoding();
}

const char* FrameDumpingDecoder::ImplementationName() const {
  return decoder_->ImplementationName();
}

}  // namespace webrtc
