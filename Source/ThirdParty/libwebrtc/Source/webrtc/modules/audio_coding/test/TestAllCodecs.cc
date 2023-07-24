/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/test/TestAllCodecs.h"

#include <cstdio>
#include <limits>
#include <string>

#include "absl/strings/match.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/logging.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

// Description of the test:
// In this test we set up a one-way communication channel from a participant
// called "a" to a participant called "b".
// a -> channel_a_to_b -> b
//
// The test loops through all available mono codecs, encode at "a" sends over
// the channel, and decodes at "b".

#define CHECK_ERROR(f)                      \
  do {                                      \
    EXPECT_GE(f, 0) << "Error Calling API"; \
  } while (0)

namespace {
const size_t kVariableSize = std::numeric_limits<size_t>::max();
}

namespace webrtc {

// Class for simulating packet handling.
TestPack::TestPack()
    : receiver_acm_(NULL),
      sequence_number_(0),
      timestamp_diff_(0),
      last_in_timestamp_(0),
      total_bytes_(0),
      payload_size_(0) {}

TestPack::~TestPack() {}

void TestPack::RegisterReceiverACM(acm2::AcmReceiver* acm_receiver) {
  receiver_acm_ = acm_receiver;
  return;
}

int32_t TestPack::SendData(AudioFrameType frame_type,
                           uint8_t payload_type,
                           uint32_t timestamp,
                           const uint8_t* payload_data,
                           size_t payload_size,
                           int64_t absolute_capture_timestamp_ms) {
  RTPHeader rtp_header;
  int32_t status;

  rtp_header.markerBit = false;
  rtp_header.ssrc = 0;
  rtp_header.sequenceNumber = sequence_number_++;
  rtp_header.payloadType = payload_type;
  rtp_header.timestamp = timestamp;

  if (frame_type == AudioFrameType::kEmptyFrame) {
    // Skip this frame.
    return 0;
  }

  // Only run mono for all test cases.
  memcpy(payload_data_, payload_data, payload_size);

  status = receiver_acm_->InsertPacket(
      rtp_header, rtc::ArrayView<const uint8_t>(payload_data_, payload_size));

  payload_size_ = payload_size;
  timestamp_diff_ = timestamp - last_in_timestamp_;
  last_in_timestamp_ = timestamp;
  total_bytes_ += payload_size;
  return status;
}

size_t TestPack::payload_size() {
  return payload_size_;
}

uint32_t TestPack::timestamp_diff() {
  return timestamp_diff_;
}

void TestPack::reset_payload_size() {
  payload_size_ = 0;
}

TestAllCodecs::TestAllCodecs()
    : acm_a_(AudioCodingModule::Create()),
      acm_b_(std::make_unique<acm2::AcmReceiver>(
          acm2::AcmReceiver::Config(CreateBuiltinAudioDecoderFactory()))),
      channel_a_to_b_(NULL),
      test_count_(0),
      packet_size_samples_(0),
      packet_size_bytes_(0) {}

TestAllCodecs::~TestAllCodecs() {
  if (channel_a_to_b_ != NULL) {
    delete channel_a_to_b_;
    channel_a_to_b_ = NULL;
  }
}

void TestAllCodecs::Perform() {
  const std::string file_name =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  infile_a_.Open(file_name, 32000, "rb");

  acm_b_->SetCodecs({{107, {"L16", 8000, 1}},
                     {108, {"L16", 16000, 1}},
                     {109, {"L16", 32000, 1}},
                     {111, {"L16", 8000, 2}},
                     {112, {"L16", 16000, 2}},
                     {113, {"L16", 32000, 2}},
                     {0, {"PCMU", 8000, 1}},
                     {110, {"PCMU", 8000, 2}},
                     {8, {"PCMA", 8000, 1}},
                     {118, {"PCMA", 8000, 2}},
                     {102, {"ILBC", 8000, 1}},
                     {9, {"G722", 8000, 1}},
                     {119, {"G722", 8000, 2}},
                     {120, {"OPUS", 48000, 2, {{"stereo", "1"}}}},
                     {13, {"CN", 8000, 1}},
                     {98, {"CN", 16000, 1}},
                     {99, {"CN", 32000, 1}}});

  // Create and connect the channel
  channel_a_to_b_ = new TestPack;
  acm_a_->RegisterTransportCallback(channel_a_to_b_);
  channel_a_to_b_->RegisterReceiverACM(acm_b_.get());

  // All codecs are tested for all allowed sampling frequencies, rates and
  // packet sizes.
  test_count_++;
  OpenOutFile(test_count_);
  char codec_g722[] = "G722";
  RegisterSendCodec(codec_g722, 16000, 64000, 160, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_g722, 16000, 64000, 320, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_g722, 16000, 64000, 480, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_g722, 16000, 64000, 640, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_g722, 16000, 64000, 800, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_g722, 16000, 64000, 960, 0);
  Run(channel_a_to_b_);
  outfile_b_.Close();
#ifdef WEBRTC_CODEC_ILBC
  test_count_++;
  OpenOutFile(test_count_);
  char codec_ilbc[] = "ILBC";
  RegisterSendCodec(codec_ilbc, 8000, 13300, 240, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_ilbc, 8000, 13300, 480, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_ilbc, 8000, 15200, 160, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_ilbc, 8000, 15200, 320, 0);
  Run(channel_a_to_b_);
  outfile_b_.Close();
#endif
  test_count_++;
  OpenOutFile(test_count_);
  char codec_l16[] = "L16";
  RegisterSendCodec(codec_l16, 8000, 128000, 80, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_l16, 8000, 128000, 160, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_l16, 8000, 128000, 240, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_l16, 8000, 128000, 320, 0);
  Run(channel_a_to_b_);
  outfile_b_.Close();

  test_count_++;
  OpenOutFile(test_count_);
  RegisterSendCodec(codec_l16, 16000, 256000, 160, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_l16, 16000, 256000, 320, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_l16, 16000, 256000, 480, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_l16, 16000, 256000, 640, 0);
  Run(channel_a_to_b_);
  outfile_b_.Close();

  test_count_++;
  OpenOutFile(test_count_);
  RegisterSendCodec(codec_l16, 32000, 512000, 320, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_l16, 32000, 512000, 640, 0);
  Run(channel_a_to_b_);
  outfile_b_.Close();

  test_count_++;
  OpenOutFile(test_count_);
  char codec_pcma[] = "PCMA";
  RegisterSendCodec(codec_pcma, 8000, 64000, 80, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcma, 8000, 64000, 160, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcma, 8000, 64000, 240, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcma, 8000, 64000, 320, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcma, 8000, 64000, 400, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcma, 8000, 64000, 480, 0);
  Run(channel_a_to_b_);

  char codec_pcmu[] = "PCMU";
  RegisterSendCodec(codec_pcmu, 8000, 64000, 80, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcmu, 8000, 64000, 160, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcmu, 8000, 64000, 240, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcmu, 8000, 64000, 320, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcmu, 8000, 64000, 400, 0);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_pcmu, 8000, 64000, 480, 0);
  Run(channel_a_to_b_);
  outfile_b_.Close();
#ifdef WEBRTC_CODEC_OPUS
  test_count_++;
  OpenOutFile(test_count_);
  char codec_opus[] = "OPUS";
  RegisterSendCodec(codec_opus, 48000, 6000, 480, kVariableSize);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_opus, 48000, 20000, 480 * 2, kVariableSize);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_opus, 48000, 32000, 480 * 4, kVariableSize);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_opus, 48000, 48000, 480, kVariableSize);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_opus, 48000, 64000, 480 * 4, kVariableSize);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_opus, 48000, 96000, 480 * 6, kVariableSize);
  Run(channel_a_to_b_);
  RegisterSendCodec(codec_opus, 48000, 500000, 480 * 2, kVariableSize);
  Run(channel_a_to_b_);
  outfile_b_.Close();
#endif
}

// Register Codec to use in the test
//
// Input:  codec_name       - name to use when register the codec
//         sampling_freq_hz - sampling frequency in Herz
//         rate             - bitrate in bytes
//         packet_size      - packet size in samples
//         extra_byte       - if extra bytes needed compared to the bitrate
//                            used when registering, can be an internal header
//                            set to kVariableSize if the codec is a variable
//                            rate codec
void TestAllCodecs::RegisterSendCodec(char* codec_name,
                                      int32_t sampling_freq_hz,
                                      int rate,
                                      int packet_size,
                                      size_t extra_byte) {
  // Store packet-size in samples, used to validate the received packet.
  // If G.722, store half the size to compensate for the timestamp bug in the
  // RFC for G.722.
  int clockrate_hz = sampling_freq_hz;
  size_t num_channels = 1;
  if (absl::EqualsIgnoreCase(codec_name, "G722")) {
    packet_size_samples_ = packet_size / 2;
    clockrate_hz = sampling_freq_hz / 2;
  } else if (absl::EqualsIgnoreCase(codec_name, "OPUS")) {
    packet_size_samples_ = packet_size;
    num_channels = 2;
  } else {
    packet_size_samples_ = packet_size;
  }

  // Store the expected packet size in bytes, used to validate the received
  // packet. If variable rate codec (extra_byte == -1), set to -1.
  if (extra_byte != kVariableSize) {
    // Add 0.875 to always round up to a whole byte
    packet_size_bytes_ =
        static_cast<size_t>(static_cast<float>(packet_size * rate) /
                                static_cast<float>(sampling_freq_hz * 8) +
                            0.875) +
        extra_byte;
  } else {
    // Packets will have a variable size.
    packet_size_bytes_ = kVariableSize;
  }

  auto factory = CreateBuiltinAudioEncoderFactory();
  constexpr int payload_type = 17;
  SdpAudioFormat format = {codec_name, clockrate_hz, num_channels};
  format.parameters["ptime"] = rtc::ToString(rtc::CheckedDivExact(
      packet_size, rtc::CheckedDivExact(sampling_freq_hz, 1000)));
  acm_a_->SetEncoder(
      factory->MakeAudioEncoder(payload_type, format, absl::nullopt));
}

void TestAllCodecs::Run(TestPack* channel) {
  AudioFrame audio_frame;

  int32_t out_freq_hz = outfile_b_.SamplingFrequency();
  size_t receive_size;
  uint32_t timestamp_diff;
  channel->reset_payload_size();
  int error_count = 0;
  int counter = 0;
  // Set test length to 500 ms (50 blocks of 10 ms each).
  infile_a_.SetNum10MsBlocksToRead(50);
  // Fast-forward 1 second (100 blocks) since the file starts with silence.
  infile_a_.FastForward(100);

  while (!infile_a_.EndOfFile()) {
    // Add 10 msec to ACM.
    infile_a_.Read10MsData(audio_frame);
    CHECK_ERROR(acm_a_->Add10MsData(audio_frame));

    // Verify that the received packet size matches the settings.
    receive_size = channel->payload_size();
    if (receive_size) {
      if ((receive_size != packet_size_bytes_) &&
          (packet_size_bytes_ != kVariableSize)) {
        error_count++;
      }

      // Verify that the timestamp is updated with expected length. The counter
      // is used to avoid problems when switching codec or frame size in the
      // test.
      timestamp_diff = channel->timestamp_diff();
      if ((counter > 10) &&
          (static_cast<int>(timestamp_diff) != packet_size_samples_) &&
          (packet_size_samples_ > -1))
        error_count++;
    }

    // Run received side of ACM.
    bool muted;
    CHECK_ERROR(acm_b_->GetAudio(out_freq_hz, &audio_frame, &muted));
    ASSERT_FALSE(muted);

    // Write output speech to file.
    outfile_b_.Write10MsData(audio_frame.data(),
                             audio_frame.samples_per_channel_);

    // Update loop counter
    counter++;
  }

  EXPECT_EQ(0, error_count);

  if (infile_a_.EndOfFile()) {
    infile_a_.Rewind();
  }
}

void TestAllCodecs::OpenOutFile(int test_number) {
  std::string filename = webrtc::test::OutputPath();
  rtc::StringBuilder test_number_str;
  test_number_str << test_number;
  filename += "testallcodecs_out_";
  filename += test_number_str.str();
  filename += ".pcm";
  outfile_b_.Open(filename, 32000, "wb");
}

}  // namespace webrtc
