/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/acm2/acm_receiver.h"

#include <algorithm>  // std::min
#include <memory>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "modules/audio_coding/acm2/rent_a_codec.h"
#include "modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/neteq/tools/rtp_generator.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {

namespace acm2 {

class AcmReceiverTestOldApi : public AudioPacketizationCallback,
                              public ::testing::Test {
 protected:
  AcmReceiverTestOldApi()
      : timestamp_(0),
        packet_sent_(false),
        last_packet_send_timestamp_(timestamp_),
        last_frame_type_(kEmptyFrame) {
    config_.decoder_factory = decoder_factory_;
  }

  ~AcmReceiverTestOldApi() {}

  void SetUp() override {
    acm_.reset(AudioCodingModule::Create(config_));
    receiver_.reset(new AcmReceiver(config_));
    ASSERT_TRUE(receiver_.get() != NULL);
    ASSERT_TRUE(acm_.get() != NULL);
    acm_->InitializeReceiver();
    acm_->RegisterTransportCallback(this);

    rtp_header_.header.sequenceNumber = 0;
    rtp_header_.header.timestamp = 0;
    rtp_header_.header.markerBit = false;
    rtp_header_.header.ssrc = 0x12345678;  // Arbitrary.
    rtp_header_.header.numCSRCs = 0;
    rtp_header_.header.payloadType = 0;
    rtp_header_.frameType = kAudioFrameSpeech;
  }

  void TearDown() override {}

  AudioCodecInfo SetEncoder(int payload_type,
                            const SdpAudioFormat& format,
                            const std::map<int, int> cng_payload_types = {}) {
    // Create the speech encoder.
    AudioCodecInfo info = encoder_factory_->QueryAudioEncoder(format).value();
    std::unique_ptr<AudioEncoder> enc =
        encoder_factory_->MakeAudioEncoder(payload_type, format, absl::nullopt);

    // If we have a compatible CN specification, stack a CNG on top.
    auto it = cng_payload_types.find(info.sample_rate_hz);
    if (it != cng_payload_types.end()) {
      AudioEncoderCng::Config config;
      config.speech_encoder = std::move(enc);
      config.num_channels = 1;
      config.payload_type = it->second;
      config.vad_mode = Vad::kVadNormal;
      enc = absl::make_unique<AudioEncoderCng>(std::move(config));
    }

    // Actually start using the new encoder.
    acm_->SetEncoder(std::move(enc));
    return info;
  }

  int InsertOnePacketOfSilence(const AudioCodecInfo& info) {
    // Frame setup according to the codec.
    AudioFrame frame;
    frame.sample_rate_hz_ = info.sample_rate_hz;
    frame.samples_per_channel_ = info.sample_rate_hz / 100;  // 10 ms.
    frame.num_channels_ = info.num_channels;
    frame.Mute();
    packet_sent_ = false;
    last_packet_send_timestamp_ = timestamp_;
    int num_10ms_frames = 0;
    while (!packet_sent_) {
      frame.timestamp_ = timestamp_;
      timestamp_ += rtc::checked_cast<uint32_t>(frame.samples_per_channel_);
      EXPECT_GE(acm_->Add10MsData(frame), 0);
      ++num_10ms_frames;
    }
    return num_10ms_frames;
  }

  template <size_t N>
  void AddSetOfCodecs(rtc::ArrayView<SdpAudioFormat> formats) {
    static int payload_type = 0;
    for (const auto& format : formats) {
      EXPECT_TRUE(receiver_->AddCodec(payload_type++, format));
    }
  }

  int SendData(FrameType frame_type,
               uint8_t payload_type,
               uint32_t timestamp,
               const uint8_t* payload_data,
               size_t payload_len_bytes,
               const RTPFragmentationHeader* fragmentation) override {
    if (frame_type == kEmptyFrame)
      return 0;

    rtp_header_.header.payloadType = payload_type;
    rtp_header_.frameType = frame_type;
    rtp_header_.header.timestamp = timestamp;

    int ret_val = receiver_->InsertPacket(
        rtp_header_,
        rtc::ArrayView<const uint8_t>(payload_data, payload_len_bytes));
    if (ret_val < 0) {
      assert(false);
      return -1;
    }
    rtp_header_.header.sequenceNumber++;
    packet_sent_ = true;
    last_frame_type_ = frame_type;
    return 0;
  }

  const rtc::scoped_refptr<AudioEncoderFactory> encoder_factory_ =
      CreateBuiltinAudioEncoderFactory();
  const rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_ =
      CreateBuiltinAudioDecoderFactory();
  AudioCodingModule::Config config_;
  std::unique_ptr<AcmReceiver> receiver_;
  std::unique_ptr<AudioCodingModule> acm_;
  WebRtcRTPHeader rtp_header_;
  uint32_t timestamp_;
  bool packet_sent_;  // Set when SendData is called reset when inserting audio.
  uint32_t last_packet_send_timestamp_;
  FrameType last_frame_type_;
};

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecGetCodec DISABLED_AddCodecGetCodec
#else
#define MAYBE_AddCodecGetCodec AddCodecGetCodec
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_AddCodecGetCodec) {
  const std::vector<AudioCodecSpec> codecs =
      decoder_factory_->GetSupportedDecoders();

  // Add codec.
  for (size_t n = 0; n < codecs.size(); ++n) {
    if (n & 0x1) {  // Just add codecs with odd index.
      const int payload_type = rtc::checked_cast<int>(n);
      EXPECT_TRUE(receiver_->AddCodec(payload_type, codecs[n].format));
    }
  }
  // Get codec and compare.
  for (size_t n = 0; n < codecs.size(); ++n) {
    const int payload_type = rtc::checked_cast<int>(n);
    if (n & 0x1) {
      // Codecs with odd index should match the reference.
      EXPECT_EQ(absl::make_optional(codecs[n].format),
                receiver_->DecoderByPayloadType(payload_type));
    } else {
      // Codecs with even index are not registered.
      EXPECT_EQ(absl::nullopt, receiver_->DecoderByPayloadType(payload_type));
    }
  }
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecChangePayloadType DISABLED_AddCodecChangePayloadType
#else
#define MAYBE_AddCodecChangePayloadType AddCodecChangePayloadType
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_AddCodecChangePayloadType) {
  const SdpAudioFormat format("giraffe", 8000, 1);

  // Register the same codec with different payloads.
  EXPECT_EQ(true, receiver_->AddCodec(17, format));
  EXPECT_EQ(true, receiver_->AddCodec(18, format));

  // Both payload types should exist.
  EXPECT_EQ(absl::make_optional(format), receiver_->DecoderByPayloadType(17));
  EXPECT_EQ(absl::make_optional(format), receiver_->DecoderByPayloadType(18));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecChangeCodecId DISABLED_AddCodecChangeCodecId
#else
#define MAYBE_AddCodecChangeCodecId AddCodecChangeCodecId
#endif
TEST_F(AcmReceiverTestOldApi, AddCodecChangeCodecId) {
  const SdpAudioFormat format1("giraffe", 8000, 1);
  const SdpAudioFormat format2("gnu", 16000, 1);

  // Register the same payload type with different codec ID.
  EXPECT_EQ(true, receiver_->AddCodec(17, format1));
  EXPECT_EQ(true, receiver_->AddCodec(17, format2));

  // Make sure that the last codec is used.
  EXPECT_EQ(absl::make_optional(format2), receiver_->DecoderByPayloadType(17));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecRemoveCodec DISABLED_AddCodecRemoveCodec
#else
#define MAYBE_AddCodecRemoveCodec AddCodecRemoveCodec
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_AddCodecRemoveCodec) {
  EXPECT_EQ(true, receiver_->AddCodec(17, SdpAudioFormat("giraffe", 8000, 1)));

  // Remove non-existing codec should not fail. ACM1 legacy.
  EXPECT_EQ(0, receiver_->RemoveCodec(18));

  // Remove an existing codec.
  EXPECT_EQ(0, receiver_->RemoveCodec(17));

  // Ask for the removed codec, must fail.
  EXPECT_EQ(absl::nullopt, receiver_->DecoderByPayloadType(17));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_SampleRate DISABLED_SampleRate
#else
#define MAYBE_SampleRate SampleRate
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_SampleRate) {
  const std::vector<SdpAudioFormat> codecs = {{"ISAC", 16000, 1},
                                              {"ISAC", 32000, 1}};
  for (size_t i = 0; i < codecs.size(); ++i) {
    const int payload_type = rtc::checked_cast<int>(i);
    EXPECT_EQ(true, receiver_->AddCodec(payload_type, codecs[i]));
  }

  constexpr int kOutSampleRateHz = 8000;  // Different than codec sample rate.
  for (size_t i = 0; i < codecs.size(); ++i) {
    const int payload_type = rtc::checked_cast<int>(i);
    const int num_10ms_frames =
        InsertOnePacketOfSilence(SetEncoder(payload_type, codecs[i]));
    for (int k = 0; k < num_10ms_frames; ++k) {
      AudioFrame frame;
      bool muted;
      EXPECT_EQ(0, receiver_->GetAudio(kOutSampleRateHz, &frame, &muted));
    }
    EXPECT_EQ(encoder_factory_->QueryAudioEncoder(codecs[i])->sample_rate_hz,
              receiver_->last_output_sample_rate_hz());
  }
}

class AcmReceiverTestFaxModeOldApi : public AcmReceiverTestOldApi {
 protected:
  AcmReceiverTestFaxModeOldApi() {
    config_.neteq_config.for_test_no_time_stretching = true;
  }

  void RunVerifyAudioFrame(const SdpAudioFormat& codec) {
    // Make sure "fax mode" is enabled. This will avoid delay changes unless the
    // packet-loss concealment is made. We do this in order to make the
    // timestamp increments predictable; in normal mode, NetEq may decide to do
    // accelerate or pre-emptive expand operations after some time, offsetting
    // the timestamp.
    EXPECT_TRUE(config_.neteq_config.for_test_no_time_stretching);

    constexpr int payload_type = 17;
    EXPECT_TRUE(receiver_->AddCodec(payload_type, codec));

    const AudioCodecInfo info = SetEncoder(payload_type, codec);
    const int output_sample_rate_hz = info.sample_rate_hz;
    const size_t output_channels = info.num_channels;
    const size_t samples_per_ms = rtc::checked_cast<size_t>(
        rtc::CheckedDivExact(output_sample_rate_hz, 1000));
    const AudioFrame::VADActivity expected_vad_activity =
        output_sample_rate_hz > 16000 ? AudioFrame::kVadActive
                                      : AudioFrame::kVadPassive;

    // Expect the first output timestamp to be 5*fs/8000 samples before the
    // first inserted timestamp (because of NetEq's look-ahead). (This value is
    // defined in Expand::overlap_length_.)
    uint32_t expected_output_ts =
        last_packet_send_timestamp_ -
        rtc::CheckedDivExact(5 * output_sample_rate_hz, 8000);

    AudioFrame frame;
    bool muted;
    EXPECT_EQ(0, receiver_->GetAudio(output_sample_rate_hz, &frame, &muted));
    // Expect timestamp = 0 before first packet is inserted.
    EXPECT_EQ(0u, frame.timestamp_);
    for (int i = 0; i < 5; ++i) {
      const int num_10ms_frames = InsertOnePacketOfSilence(info);
      for (int k = 0; k < num_10ms_frames; ++k) {
        EXPECT_EQ(0,
                  receiver_->GetAudio(output_sample_rate_hz, &frame, &muted));
        EXPECT_EQ(expected_output_ts, frame.timestamp_);
        expected_output_ts += rtc::checked_cast<uint32_t>(10 * samples_per_ms);
        EXPECT_EQ(10 * samples_per_ms, frame.samples_per_channel_);
        EXPECT_EQ(output_sample_rate_hz, frame.sample_rate_hz_);
        EXPECT_EQ(output_channels, frame.num_channels_);
        EXPECT_EQ(AudioFrame::kNormalSpeech, frame.speech_type_);
        EXPECT_EQ(expected_vad_activity, frame.vad_activity_);
        EXPECT_FALSE(muted);
      }
    }
  }
};

#if defined(WEBRTC_ANDROID)
#define MAYBE_VerifyAudioFramePCMU DISABLED_VerifyAudioFramePCMU
#else
#define MAYBE_VerifyAudioFramePCMU VerifyAudioFramePCMU
#endif
TEST_F(AcmReceiverTestFaxModeOldApi, MAYBE_VerifyAudioFramePCMU) {
  RunVerifyAudioFrame({"PCMU", 8000, 1});
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_VerifyAudioFrameISAC DISABLED_VerifyAudioFrameISAC
#else
#define MAYBE_VerifyAudioFrameISAC VerifyAudioFrameISAC
#endif
TEST_F(AcmReceiverTestFaxModeOldApi, MAYBE_VerifyAudioFrameISAC) {
  RunVerifyAudioFrame({"ISAC", 16000, 1});
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_VerifyAudioFrameOpus DISABLED_VerifyAudioFrameOpus
#else
#define MAYBE_VerifyAudioFrameOpus VerifyAudioFrameOpus
#endif
TEST_F(AcmReceiverTestFaxModeOldApi, MAYBE_VerifyAudioFrameOpus) {
  RunVerifyAudioFrame({"opus", 48000, 2});
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_PostdecodingVad DISABLED_PostdecodingVad
#else
#define MAYBE_PostdecodingVad PostdecodingVad
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_PostdecodingVad) {
  EXPECT_TRUE(config_.neteq_config.enable_post_decode_vad);
  constexpr int payload_type = 34;
  const SdpAudioFormat codec = {"L16", 16000, 1};
  const AudioCodecInfo info = SetEncoder(payload_type, codec);
  EXPECT_TRUE(receiver_->AddCodec(payload_type, codec));
  constexpr int kNumPackets = 5;
  AudioFrame frame;
  for (int n = 0; n < kNumPackets; ++n) {
    const int num_10ms_frames = InsertOnePacketOfSilence(info);
    for (int k = 0; k < num_10ms_frames; ++k) {
      bool muted;
      ASSERT_EQ(0, receiver_->GetAudio(info.sample_rate_hz, &frame, &muted));
    }
  }
  EXPECT_EQ(AudioFrame::kVadPassive, frame.vad_activity_);
}

class AcmReceiverTestPostDecodeVadPassiveOldApi : public AcmReceiverTestOldApi {
 protected:
  AcmReceiverTestPostDecodeVadPassiveOldApi() {
    config_.neteq_config.enable_post_decode_vad = false;
  }
};

#if defined(WEBRTC_ANDROID)
#define MAYBE_PostdecodingVad DISABLED_PostdecodingVad
#else
#define MAYBE_PostdecodingVad PostdecodingVad
#endif
TEST_F(AcmReceiverTestPostDecodeVadPassiveOldApi, MAYBE_PostdecodingVad) {
  EXPECT_FALSE(config_.neteq_config.enable_post_decode_vad);
  constexpr int payload_type = 34;
  const SdpAudioFormat codec = {"L16", 16000, 1};
  const AudioCodecInfo info = SetEncoder(payload_type, codec);
  encoder_factory_->QueryAudioEncoder(codec).value();
  EXPECT_TRUE(receiver_->AddCodec(payload_type, codec));
  const int kNumPackets = 5;
  AudioFrame frame;
  for (int n = 0; n < kNumPackets; ++n) {
    const int num_10ms_frames = InsertOnePacketOfSilence(info);
    for (int k = 0; k < num_10ms_frames; ++k) {
      bool muted;
      ASSERT_EQ(0, receiver_->GetAudio(info.sample_rate_hz, &frame, &muted));
    }
  }
  EXPECT_EQ(AudioFrame::kVadUnknown, frame.vad_activity_);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LastAudioCodec DISABLED_LastAudioCodec
#else
#define MAYBE_LastAudioCodec LastAudioCodec
#endif
#if defined(WEBRTC_CODEC_ISAC)
TEST_F(AcmReceiverTestOldApi, MAYBE_LastAudioCodec) {
  const std::vector<SdpAudioFormat> codecs = {{"ISAC", 16000, 1},
                                              {"PCMA", 8000, 1},
                                              {"ISAC", 32000, 1},
                                              {"L16", 32000, 1}};
  for (size_t i = 0; i < codecs.size(); ++i) {
    const int payload_type = rtc::checked_cast<int>(i);
    EXPECT_TRUE(receiver_->AddCodec(payload_type, codecs[i]));
  }

  const std::map<int, int> cng_payload_types = {
      {8000, 100}, {16000, 101}, {32000, 102}};
  for (const auto& x : cng_payload_types) {
    const int sample_rate_hz = x.first;
    const int payload_type = x.second;
    EXPECT_TRUE(receiver_->AddCodec(payload_type, {"CN", sample_rate_hz, 1}));
  }

  // No audio payload is received.
  EXPECT_EQ(absl::nullopt, receiver_->LastAudioFormat());

  // Start with sending DTX.
  packet_sent_ = false;
  InsertOnePacketOfSilence(
      SetEncoder(0, codecs[0], cng_payload_types));  // Enough to test
                                                     // with one codec.
  ASSERT_TRUE(packet_sent_);
  EXPECT_EQ(kAudioFrameCN, last_frame_type_);

  // Has received, only, DTX. Last Audio codec is undefined.
  EXPECT_EQ(absl::nullopt, receiver_->LastAudioFormat());
  EXPECT_FALSE(receiver_->last_packet_sample_rate_hz());

  for (size_t i = 0; i < codecs.size(); ++i) {
    // Set DTX off to send audio payload.
    packet_sent_ = false;
    const int payload_type = rtc::checked_cast<int>(i);
    const AudioCodecInfo info_without_cng = SetEncoder(payload_type, codecs[i]);
    InsertOnePacketOfSilence(info_without_cng);

    // Sanity check if Actually an audio payload received, and it should be
    // of type "speech."
    ASSERT_TRUE(packet_sent_);
    ASSERT_EQ(kAudioFrameSpeech, last_frame_type_);
    EXPECT_EQ(info_without_cng.sample_rate_hz,
              receiver_->last_packet_sample_rate_hz());

    // Set VAD on to send DTX. Then check if the "Last Audio codec" returns
    // the expected codec. Encode repeatedly until a DTX is sent.
    const AudioCodecInfo info_with_cng =
        SetEncoder(payload_type, codecs[i], cng_payload_types);
    while (last_frame_type_ != kAudioFrameCN) {
      packet_sent_ = false;
      InsertOnePacketOfSilence(info_with_cng);
      ASSERT_TRUE(packet_sent_);
    }
    EXPECT_EQ(info_with_cng.sample_rate_hz,
              receiver_->last_packet_sample_rate_hz());
    EXPECT_EQ(codecs[i], receiver_->LastAudioFormat());
  }
}
#endif

}  // namespace acm2

}  // namespace webrtc
