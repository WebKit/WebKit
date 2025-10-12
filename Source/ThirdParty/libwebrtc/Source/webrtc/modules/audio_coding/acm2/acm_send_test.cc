/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_send_test.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/environment/environment_factory.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

AcmSendTestOldApi::AcmSendTestOldApi(InputAudioFile* audio_source,
                                     int source_rate_hz,
                                     int test_duration_ms)
    : clock_(0),
      env_(CreateEnvironment(&clock_)),
      acm_(AudioCodingModule::Create()),
      audio_source_(audio_source),
      source_rate_hz_(source_rate_hz),
      input_block_size_samples_(
          static_cast<size_t>(source_rate_hz_ * kBlockSizeMs / 1000)),
      codec_registered_(false),
      test_duration_ms_(test_duration_ms),
      frame_type_(AudioFrameType::kAudioFrameSpeech),
      payload_type_(0),
      timestamp_(0),
      sequence_number_(0) {
  input_frame_.sample_rate_hz_ = source_rate_hz_;
  input_frame_.num_channels_ = 1;
  input_frame_.samples_per_channel_ = input_block_size_samples_;
  RTC_DCHECK_LE(input_block_size_samples_ * input_frame_.num_channels_,
                AudioFrame::kMaxDataSizeSamples);
  acm_->RegisterTransportCallback(this);
}

AcmSendTestOldApi::~AcmSendTestOldApi() = default;

bool AcmSendTestOldApi::RegisterCodec(absl::string_view payload_name,
                                      int clockrate_hz,
                                      int num_channels,
                                      int payload_type,
                                      int frame_size_samples) {
  SdpAudioFormat format(payload_name, clockrate_hz, num_channels);
  if (absl::EqualsIgnoreCase(payload_name, "g722")) {
    RTC_CHECK_EQ(16000, clockrate_hz);
    format.clockrate_hz = 8000;
  } else if (absl::EqualsIgnoreCase(payload_name, "opus")) {
    RTC_CHECK(num_channels == 1 || num_channels == 2);
    if (num_channels == 2) {
      format.parameters["stereo"] = "1";
    }
    format.num_channels = 2;
  }
  format.parameters["ptime"] = absl::StrCat(
      CheckedDivExact(frame_size_samples, CheckedDivExact(clockrate_hz, 1000)));
  auto factory = CreateBuiltinAudioEncoderFactory();
  acm_->SetEncoder(
      factory->Create(env_, format, {.payload_type = payload_type}));
  codec_registered_ = true;
  input_frame_.num_channels_ = num_channels;
  RTC_DCHECK_LE(input_block_size_samples_ * input_frame_.num_channels_,
                AudioFrame::kMaxDataSizeSamples);
  return codec_registered_;
}

void AcmSendTestOldApi::RegisterExternalCodec(
    std::unique_ptr<AudioEncoder> external_speech_encoder) {
  input_frame_.num_channels_ = external_speech_encoder->NumChannels();
  acm_->SetEncoder(std::move(external_speech_encoder));
  RTC_DCHECK_LE(input_block_size_samples_ * input_frame_.num_channels_,
                AudioFrame::kMaxDataSizeSamples);
  codec_registered_ = true;
}

std::unique_ptr<RtpPacketReceived> AcmSendTestOldApi::NextPacket() {
  RTC_DCHECK(codec_registered_);
  if (filter_.test(static_cast<size_t>(payload_type_))) {
    // This payload type should be filtered out. Since the payload type is the
    // same throughout the whole test run, no packet at all will be delivered.
    // We can just as well signal that the test is over by returning NULL.
    return nullptr;
  }
  // Insert audio and process until one packet is produced.
  while (clock_.TimeInMilliseconds() < test_duration_ms_) {
    clock_.AdvanceTimeMilliseconds(kBlockSizeMs);
    RTC_CHECK(audio_source_->Read(
        input_block_size_samples_ * input_frame_.num_channels_,
        input_frame_.mutable_data()));
    data_to_send_ = false;
    RTC_CHECK_GE(acm_->Add10MsData(input_frame_), 0);
    input_frame_.timestamp_ += static_cast<uint32_t>(input_block_size_samples_);
    if (data_to_send_) {
      // Encoded packet received.
      return CreatePacket();
    }
  }
  // Test ended.
  return nullptr;
}

// This method receives the callback from ACM when a new packet is produced.
int32_t AcmSendTestOldApi::SendData(
    AudioFrameType frame_type,
    uint8_t payload_type,
    uint32_t timestamp,
    const uint8_t* payload_data,
    size_t payload_len_bytes,
    int64_t /* absolute_capture_timestamp_ms */) {
  // Store the packet locally.
  frame_type_ = frame_type;
  payload_type_ = payload_type;
  timestamp_ = timestamp;
  last_payload_vec_.assign(payload_data, payload_data + payload_len_bytes);
  RTC_DCHECK_EQ(last_payload_vec_.size(), payload_len_bytes);
  data_to_send_ = true;
  return 0;
}

std::unique_ptr<RtpPacketReceived> AcmSendTestOldApi::CreatePacket() {
  auto rtp_packet = std::make_unique<RtpPacketReceived>();

  // Populate the header.
  rtp_packet->SetPayloadType(payload_type_);
  rtp_packet->SetSequenceNumber(sequence_number_);
  rtp_packet->SetTimestamp(timestamp_);
  rtp_packet->SetSsrc(0x12345678);
  ++sequence_number_;

  rtp_packet->SetPayload(last_payload_vec_);
  rtp_packet->set_arrival_time(clock_.CurrentTime());
  return rtp_packet;
}

}  // namespace test
}  // namespace webrtc
