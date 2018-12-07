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

#include "api/video_codecs/create_vp8_temporal_layers.h"
#include "api/video_codecs/vp8_temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"

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

FakeVP8Encoder::FakeVP8Encoder(Clock* clock)
    : FakeEncoder(clock), callback_(nullptr) {
  FakeEncoder::RegisterEncodeCompleteCallback(this);
  sequence_checker_.Detach();
}

int32_t FakeVP8Encoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  callback_ = callback;
  return 0;
}

int32_t FakeVP8Encoder::InitEncode(const VideoCodec* config,
                                   int32_t number_of_cores,
                                   size_t max_payload_size) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  auto result =
      FakeEncoder::InitEncode(config, number_of_cores, max_payload_size);
  if (result != WEBRTC_VIDEO_CODEC_OK) {
    return result;
  }

  SetupTemporalLayers(*config);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeVP8Encoder::Release() {
  auto result = FakeEncoder::Release();
  sequence_checker_.Detach();
  return result;
}

void FakeVP8Encoder::SetupTemporalLayers(const VideoCodec& codec) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);

  int num_streams = SimulcastUtility::NumberOfSimulcastStreams(codec);
  for (int i = 0; i < num_streams; ++i) {
    Vp8TemporalLayersType type;
    int num_temporal_layers =
        SimulcastUtility::NumberOfTemporalLayers(codec, i);
    if (SimulcastUtility::IsConferenceModeScreenshare(codec) && i == 0) {
      type = Vp8TemporalLayersType::kBitrateDynamic;
      // Legacy screenshare layers supports max 2 layers.
      num_temporal_layers = std::max<int>(2, num_temporal_layers);
    } else {
      type = Vp8TemporalLayersType::kFixedPattern;
    }
    temporal_layers_.emplace_back(
        CreateVp8TemporalLayers(type, num_temporal_layers));
  }
}

void FakeVP8Encoder::PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                                           size_t size_bytes,
                                           FrameType frame_type,
                                           int stream_idx,
                                           uint32_t timestamp) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  codec_specific->codecType = kVideoCodecVP8;
  CodecSpecificInfoVP8* vp8Info = &(codec_specific->codecSpecific.VP8);
  vp8Info->keyIdx = kNoKeyIdx;
  vp8Info->nonReference = false;
  temporal_layers_[stream_idx]->OnEncodeDone(
      timestamp, size_bytes, frame_type == kVideoFrameKey, -1, vp8Info);
}

EncodedImageCallback::Result FakeVP8Encoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragments) {
  RTC_DCHECK_CALLED_SEQUENTIALLY(&sequence_checker_);
  uint8_t stream_idx = encoded_image.SpatialIndex().value_or(0);
  CodecSpecificInfo overrided_specific_info;
  temporal_layers_[stream_idx]->UpdateLayerConfig(encoded_image.Timestamp());
  PopulateCodecSpecific(&overrided_specific_info, encoded_image._length,
                        encoded_image._frameType, stream_idx,
                        encoded_image.Timestamp());

  // Write width and height to the payload the same way as the real encoder
  // does.
  WriteFakeVp8(encoded_image._buffer, encoded_image._encodedWidth,
               encoded_image._encodedHeight,
               encoded_image._frameType == kVideoFrameKey);
  return callback_->OnEncodedImage(encoded_image, &overrided_specific_info,
                                   fragments);
}

VideoEncoder::EncoderInfo FakeVP8Encoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.implementation_name = "FakeVp8Encoder";
  return info;
}

}  // namespace test
}  // namespace webrtc
