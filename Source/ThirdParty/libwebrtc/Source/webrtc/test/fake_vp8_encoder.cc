/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_vp8_encoder.h"

#include <algorithm>

#include "absl/types/optional.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "api/video_codecs/vp8_temporal_layers_factory.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/simulcast_utility.h"

namespace {

// Write width and height to the payload the same way as the real encoder does.
// It requires that |payload| has a size of at least kMinPayLoadHeaderLength.
void WriteFakeVp8(unsigned char* payload,
                  int width,
                  int height,
                  bool key_frame) {
  payload[0] = key_frame ? 0 : 0x01;

  if (key_frame) {
    payload[9] = (height & 0x3F00) >> 8;
    payload[8] = (height & 0x00FF);

    payload[7] = (width & 0x3F00) >> 8;
    payload[6] = (width & 0x00FF);
  }
}
}  // namespace

namespace webrtc {

namespace test {

FakeVp8Encoder::FakeVp8Encoder(Clock* clock) : FakeEncoder(clock) {
  sequence_checker_.Detach();
}

int32_t FakeVp8Encoder::InitEncode(const VideoCodec* config,
                                   const Settings& settings) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  auto result = FakeEncoder::InitEncode(config, settings);
  if (result != WEBRTC_VIDEO_CODEC_OK) {
    return result;
  }

  Vp8TemporalLayersFactory factory;
  frame_buffer_controller_ =
      factory.Create(*config, settings, &fec_controller_override_);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeVp8Encoder::Release() {
  auto result = FakeEncoder::Release();
  sequence_checker_.Detach();
  return result;
}

void FakeVp8Encoder::PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                                           size_t size_bytes,
                                           VideoFrameType frame_type,
                                           int stream_idx,
                                           uint32_t timestamp) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  codec_specific->codecType = kVideoCodecVP8;
  codec_specific->codecSpecific.VP8.keyIdx = kNoKeyIdx;
  codec_specific->codecSpecific.VP8.nonReference = false;
  if (size_bytes > 0) {
    frame_buffer_controller_->OnEncodeDone(
        stream_idx, timestamp, size_bytes,
        frame_type == VideoFrameType::kVideoFrameKey, -1, codec_specific);
  } else {
    frame_buffer_controller_->OnFrameDropped(stream_idx, timestamp);
  }
}

std::unique_ptr<RTPFragmentationHeader> FakeVp8Encoder::EncodeHook(
    EncodedImage* encoded_image,
    CodecSpecificInfo* codec_specific) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  uint8_t stream_idx = encoded_image->SpatialIndex().value_or(0);
  frame_buffer_controller_->NextFrameConfig(stream_idx,
                                            encoded_image->Timestamp());
  PopulateCodecSpecific(codec_specific, encoded_image->size(),
                        encoded_image->_frameType, stream_idx,
                        encoded_image->Timestamp());

  // Write width and height to the payload the same way as the real encoder
  // does.
  WriteFakeVp8(encoded_image->data(), encoded_image->_encodedWidth,
               encoded_image->_encodedHeight,
               encoded_image->_frameType == VideoFrameType::kVideoFrameKey);
  return nullptr;
}

VideoEncoder::EncoderInfo FakeVp8Encoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.implementation_name = "FakeVp8Encoder";
  return info;
}

}  // namespace test
}  // namespace webrtc
