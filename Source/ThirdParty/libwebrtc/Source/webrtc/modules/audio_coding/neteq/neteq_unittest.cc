/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/include/neteq.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>  // memset

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/audio_coding/codecs/pcm16b/pcm16b.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "modules/audio_coding/neteq/tools/neteq_packet_source_input.h"
#include "modules/audio_coding/neteq/tools/neteq_test.h"
#include "modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "modules/rtp_rtcp/include/rtcp_statistics.h"
#include "rtc_base/ignore_wundef.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/protobuf_utils.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/arch.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

// This must come after test/gtest.h
#include "rtc_base/flags.h"  // NOLINT(build/include)

#ifdef WEBRTC_NETEQ_UNITTEST_BITEXACT
RTC_PUSH_IGNORING_WUNDEF()
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_coding/neteq/neteq_unittest.pb.h"
#else
#include "modules/audio_coding/neteq/neteq_unittest.pb.h"
#endif
RTC_POP_IGNORING_WUNDEF()
#endif

WEBRTC_DEFINE_bool(gen_ref, false, "Generate reference files.");

namespace webrtc {

namespace {

const std::string& PlatformChecksum(const std::string& checksum_general,
                                    const std::string& checksum_android_32,
                                    const std::string& checksum_android_64,
                                    const std::string& checksum_win_32,
                                    const std::string& checksum_win_64) {
#if defined(WEBRTC_ANDROID)
#ifdef WEBRTC_ARCH_64_BITS
  return checksum_android_64;
#else
  return checksum_android_32;
#endif  // WEBRTC_ARCH_64_BITS
#elif defined(WEBRTC_WIN)
#ifdef WEBRTC_ARCH_64_BITS
  return checksum_win_64;
#else
  return checksum_win_32;
#endif  // WEBRTC_ARCH_64_BITS
#else
  return checksum_general;
#endif  // WEBRTC_WIN
}

#ifdef WEBRTC_NETEQ_UNITTEST_BITEXACT
void Convert(const webrtc::NetEqNetworkStatistics& stats_raw,
             webrtc::neteq_unittest::NetEqNetworkStatistics* stats) {
  stats->set_current_buffer_size_ms(stats_raw.current_buffer_size_ms);
  stats->set_preferred_buffer_size_ms(stats_raw.preferred_buffer_size_ms);
  stats->set_jitter_peaks_found(stats_raw.jitter_peaks_found);
  stats->set_packet_loss_rate(stats_raw.packet_loss_rate);
  stats->set_expand_rate(stats_raw.expand_rate);
  stats->set_speech_expand_rate(stats_raw.speech_expand_rate);
  stats->set_preemptive_rate(stats_raw.preemptive_rate);
  stats->set_accelerate_rate(stats_raw.accelerate_rate);
  stats->set_secondary_decoded_rate(stats_raw.secondary_decoded_rate);
  stats->set_secondary_discarded_rate(stats_raw.secondary_discarded_rate);
  stats->set_clockdrift_ppm(stats_raw.clockdrift_ppm);
  stats->set_added_zero_samples(stats_raw.added_zero_samples);
  stats->set_mean_waiting_time_ms(stats_raw.mean_waiting_time_ms);
  stats->set_median_waiting_time_ms(stats_raw.median_waiting_time_ms);
  stats->set_min_waiting_time_ms(stats_raw.min_waiting_time_ms);
  stats->set_max_waiting_time_ms(stats_raw.max_waiting_time_ms);
}

void Convert(const webrtc::RtcpStatistics& stats_raw,
             webrtc::neteq_unittest::RtcpStatistics* stats) {
  stats->set_fraction_lost(stats_raw.fraction_lost);
  stats->set_cumulative_lost(stats_raw.packets_lost);
  stats->set_extended_max_sequence_number(
      stats_raw.extended_highest_sequence_number);
  stats->set_jitter(stats_raw.jitter);
}

void AddMessage(FILE* file,
                rtc::MessageDigest* digest,
                const std::string& message) {
  int32_t size = message.length();
  if (file)
    ASSERT_EQ(1u, fwrite(&size, sizeof(size), 1, file));
  digest->Update(&size, sizeof(size));

  if (file)
    ASSERT_EQ(static_cast<size_t>(size),
              fwrite(message.data(), sizeof(char), size, file));
  digest->Update(message.data(), sizeof(char) * size);
}

#endif  // WEBRTC_NETEQ_UNITTEST_BITEXACT

void LoadDecoders(webrtc::NetEq* neteq) {
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(0, SdpAudioFormat("pcmu", 8000, 1)));
  // Use non-SdpAudioFormat argument when registering PCMa, so that we get test
  // coverage for that as well.
  ASSERT_EQ(0, neteq->RegisterPayloadType(webrtc::NetEqDecoder::kDecoderPCMa,
                                          "pcma", 8));
#ifdef WEBRTC_CODEC_ILBC
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(102, SdpAudioFormat("ilbc", 8000, 1)));
#endif
#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(103, SdpAudioFormat("isac", 16000, 1)));
#endif
#ifdef WEBRTC_CODEC_ISAC
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(104, SdpAudioFormat("isac", 32000, 1)));
#endif
#ifdef WEBRTC_CODEC_OPUS
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(
                111, SdpAudioFormat("opus", 48000, 2, {{"stereo", "0"}})));
#endif
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(93, SdpAudioFormat("L16", 8000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(94, SdpAudioFormat("L16", 16000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(95, SdpAudioFormat("L16", 32000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(13, SdpAudioFormat("cn", 8000, 1)));
  ASSERT_EQ(true,
            neteq->RegisterPayloadType(98, SdpAudioFormat("cn", 16000, 1)));
}
}  // namespace

class ResultSink {
 public:
  explicit ResultSink(const std::string& output_file);
  ~ResultSink();

  template <typename T>
  void AddResult(const T* test_results, size_t length);

  void AddResult(const NetEqNetworkStatistics& stats);
  void AddResult(const RtcpStatistics& stats);

  void VerifyChecksum(const std::string& ref_check_sum);

 private:
  FILE* output_fp_;
  std::unique_ptr<rtc::MessageDigest> digest_;
};

ResultSink::ResultSink(const std::string& output_file)
    : output_fp_(nullptr),
      digest_(rtc::MessageDigestFactory::Create(rtc::DIGEST_SHA_1)) {
  if (!output_file.empty()) {
    output_fp_ = fopen(output_file.c_str(), "wb");
    EXPECT_TRUE(output_fp_ != NULL);
  }
}

ResultSink::~ResultSink() {
  if (output_fp_)
    fclose(output_fp_);
}

template <typename T>
void ResultSink::AddResult(const T* test_results, size_t length) {
  if (output_fp_) {
    ASSERT_EQ(length, fwrite(test_results, sizeof(T), length, output_fp_));
  }
  digest_->Update(test_results, sizeof(T) * length);
}

void ResultSink::AddResult(const NetEqNetworkStatistics& stats_raw) {
#ifdef WEBRTC_NETEQ_UNITTEST_BITEXACT
  neteq_unittest::NetEqNetworkStatistics stats;
  Convert(stats_raw, &stats);

  ProtoString stats_string;
  ASSERT_TRUE(stats.SerializeToString(&stats_string));
  AddMessage(output_fp_, digest_.get(), stats_string);
#else
  FAIL() << "Writing to reference file requires Proto Buffer.";
#endif  // WEBRTC_NETEQ_UNITTEST_BITEXACT
}

void ResultSink::AddResult(const RtcpStatistics& stats_raw) {
#ifdef WEBRTC_NETEQ_UNITTEST_BITEXACT
  neteq_unittest::RtcpStatistics stats;
  Convert(stats_raw, &stats);

  ProtoString stats_string;
  ASSERT_TRUE(stats.SerializeToString(&stats_string));
  AddMessage(output_fp_, digest_.get(), stats_string);
#else
  FAIL() << "Writing to reference file requires Proto Buffer.";
#endif  // WEBRTC_NETEQ_UNITTEST_BITEXACT
}

void ResultSink::VerifyChecksum(const std::string& checksum) {
  std::vector<char> buffer;
  buffer.resize(digest_->Size());
  digest_->Finish(&buffer[0], buffer.size());
  const std::string result = rtc::hex_encode(&buffer[0], digest_->Size());
  if (checksum.size() == result.size()) {
    EXPECT_EQ(checksum, result);
  } else {
    // Check result is one the '|'-separated checksums.
    EXPECT_NE(checksum.find(result), std::string::npos)
        << result << " should be one of these:\n"
        << checksum;
  }
}

class NetEqDecodingTest : public ::testing::Test {
 protected:
  // NetEQ must be polled for data once every 10 ms. Thus, neither of the
  // constants below can be changed.
  static const int kTimeStepMs = 10;
  static const size_t kBlockSize8kHz = kTimeStepMs * 8;
  static const size_t kBlockSize16kHz = kTimeStepMs * 16;
  static const size_t kBlockSize32kHz = kTimeStepMs * 32;
  static const size_t kBlockSize48kHz = kTimeStepMs * 48;
  static const int kInitSampleRateHz = 8000;

  NetEqDecodingTest();
  virtual void SetUp();
  virtual void TearDown();
  void SelectDecoders(NetEqDecoder* used_codec);
  void OpenInputFile(const std::string& rtp_file);
  void Process();

  void DecodeAndCompare(const std::string& rtp_file,
                        const std::string& output_checksum,
                        const std::string& network_stats_checksum,
                        bool gen_ref);

  static void PopulateRtpInfo(int frame_index,
                              int timestamp,
                              RTPHeader* rtp_info);
  static void PopulateCng(int frame_index,
                          int timestamp,
                          RTPHeader* rtp_info,
                          uint8_t* payload,
                          size_t* payload_len);

  void WrapTest(uint16_t start_seq_no,
                uint32_t start_timestamp,
                const std::set<uint16_t>& drop_seq_numbers,
                bool expect_seq_no_wrap,
                bool expect_timestamp_wrap);

  void LongCngWithClockDrift(double drift_factor,
                             double network_freeze_ms,
                             bool pull_audio_during_freeze,
                             int delay_tolerance_ms,
                             int max_time_to_speech_ms);

  void DuplicateCng();

  NetEq* neteq_;
  NetEq::Config config_;
  std::unique_ptr<test::RtpFileSource> rtp_source_;
  std::unique_ptr<test::Packet> packet_;
  unsigned int sim_clock_;
  AudioFrame out_frame_;
  int output_sample_rate_;
  int algorithmic_delay_ms_;
};

// Allocating the static const so that it can be passed by reference.
const int NetEqDecodingTest::kTimeStepMs;
const size_t NetEqDecodingTest::kBlockSize8kHz;
const size_t NetEqDecodingTest::kBlockSize16kHz;
const size_t NetEqDecodingTest::kBlockSize32kHz;
const int NetEqDecodingTest::kInitSampleRateHz;

NetEqDecodingTest::NetEqDecodingTest()
    : neteq_(NULL),
      config_(),
      sim_clock_(0),
      output_sample_rate_(kInitSampleRateHz),
      algorithmic_delay_ms_(0) {
  config_.sample_rate_hz = kInitSampleRateHz;
}

void NetEqDecodingTest::SetUp() {
  neteq_ = NetEq::Create(config_, CreateBuiltinAudioDecoderFactory());
  NetEqNetworkStatistics stat;
  ASSERT_EQ(0, neteq_->NetworkStatistics(&stat));
  algorithmic_delay_ms_ = stat.current_buffer_size_ms;
  ASSERT_TRUE(neteq_);
  LoadDecoders(neteq_);
}

void NetEqDecodingTest::TearDown() {
  delete neteq_;
}

void NetEqDecodingTest::OpenInputFile(const std::string& rtp_file) {
  rtp_source_.reset(test::RtpFileSource::Create(rtp_file));
}

void NetEqDecodingTest::Process() {
  // Check if time to receive.
  while (packet_ && sim_clock_ >= packet_->time_ms()) {
    if (packet_->payload_length_bytes() > 0) {
#ifndef WEBRTC_CODEC_ISAC
      // Ignore payload type 104 (iSAC-swb) if ISAC is not supported.
      if (packet_->header().payloadType != 104)
#endif
        ASSERT_EQ(0,
                  neteq_->InsertPacket(
                      packet_->header(),
                      rtc::ArrayView<const uint8_t>(
                          packet_->payload(), packet_->payload_length_bytes()),
                      static_cast<uint32_t>(packet_->time_ms() *
                                            (output_sample_rate_ / 1000))));
    }
    // Get next packet.
    packet_ = rtp_source_->NextPacket();
  }

  // Get audio from NetEq.
  bool muted;
  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_FALSE(muted);
  ASSERT_TRUE((out_frame_.samples_per_channel_ == kBlockSize8kHz) ||
              (out_frame_.samples_per_channel_ == kBlockSize16kHz) ||
              (out_frame_.samples_per_channel_ == kBlockSize32kHz) ||
              (out_frame_.samples_per_channel_ == kBlockSize48kHz));
  output_sample_rate_ = out_frame_.sample_rate_hz_;
  EXPECT_EQ(output_sample_rate_, neteq_->last_output_sample_rate_hz());

  // Increase time.
  sim_clock_ += kTimeStepMs;
}

void NetEqDecodingTest::DecodeAndCompare(
    const std::string& rtp_file,
    const std::string& output_checksum,
    const std::string& network_stats_checksum,
    bool gen_ref) {
  OpenInputFile(rtp_file);

  std::string ref_out_file =
      gen_ref ? webrtc::test::OutputPath() + "neteq_universal_ref.pcm" : "";
  ResultSink output(ref_out_file);

  std::string stat_out_file =
      gen_ref ? webrtc::test::OutputPath() + "neteq_network_stats.dat" : "";
  ResultSink network_stats(stat_out_file);

  packet_ = rtp_source_->NextPacket();
  int i = 0;
  uint64_t last_concealed_samples = 0;
  uint64_t last_total_samples_received = 0;
  while (packet_) {
    rtc::StringBuilder ss;
    ss << "Lap number " << i++ << " in DecodeAndCompare while loop";
    SCOPED_TRACE(ss.str());  // Print out the parameter values on failure.
    ASSERT_NO_FATAL_FAILURE(Process());
    ASSERT_NO_FATAL_FAILURE(
        output.AddResult(out_frame_.data(), out_frame_.samples_per_channel_));

    // Query the network statistics API once per second
    if (sim_clock_ % 1000 == 0) {
      // Process NetworkStatistics.
      NetEqNetworkStatistics current_network_stats;
      ASSERT_EQ(0, neteq_->NetworkStatistics(&current_network_stats));
      ASSERT_NO_FATAL_FAILURE(network_stats.AddResult(current_network_stats));

      // Compare with CurrentDelay, which should be identical.
      EXPECT_EQ(current_network_stats.current_buffer_size_ms,
                neteq_->CurrentDelayMs());

      // Verify that liftime stats and network stats report similar loss
      // concealment rates.
      auto lifetime_stats = neteq_->GetLifetimeStatistics();
      const uint64_t delta_concealed_samples =
          lifetime_stats.concealed_samples - last_concealed_samples;
      last_concealed_samples = lifetime_stats.concealed_samples;
      const uint64_t delta_total_samples_received =
          lifetime_stats.total_samples_received - last_total_samples_received;
      last_total_samples_received = lifetime_stats.total_samples_received;
      // The tolerance is 1% but expressed in Q14.
      EXPECT_NEAR(
          (delta_concealed_samples << 14) / delta_total_samples_received,
          current_network_stats.expand_rate, (2 << 14) / 100.0);
    }
  }

  SCOPED_TRACE("Check output audio.");
  output.VerifyChecksum(output_checksum);
  SCOPED_TRACE("Check network stats.");
  network_stats.VerifyChecksum(network_stats_checksum);
}

void NetEqDecodingTest::PopulateRtpInfo(int frame_index,
                                        int timestamp,
                                        RTPHeader* rtp_info) {
  rtp_info->sequenceNumber = frame_index;
  rtp_info->timestamp = timestamp;
  rtp_info->ssrc = 0x1234;     // Just an arbitrary SSRC.
  rtp_info->payloadType = 94;  // PCM16b WB codec.
  rtp_info->markerBit = 0;
}

void NetEqDecodingTest::PopulateCng(int frame_index,
                                    int timestamp,
                                    RTPHeader* rtp_info,
                                    uint8_t* payload,
                                    size_t* payload_len) {
  rtp_info->sequenceNumber = frame_index;
  rtp_info->timestamp = timestamp;
  rtp_info->ssrc = 0x1234;     // Just an arbitrary SSRC.
  rtp_info->payloadType = 98;  // WB CNG.
  rtp_info->markerBit = 0;
  payload[0] = 64;   // Noise level -64 dBov, quite arbitrarily chosen.
  *payload_len = 1;  // Only noise level, no spectral parameters.
}

#if !defined(WEBRTC_IOS) && defined(WEBRTC_NETEQ_UNITTEST_BITEXACT) && \
    (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)) &&    \
    defined(WEBRTC_CODEC_ILBC) && !defined(WEBRTC_ARCH_ARM64)
#define MAYBE_TestBitExactness TestBitExactness
#else
#define MAYBE_TestBitExactness DISABLED_TestBitExactness
#endif
TEST_F(NetEqDecodingTest, MAYBE_TestBitExactness) {
  const std::string input_rtp_file =
      webrtc::test::ResourcePath("audio_coding/neteq_universal_new", "rtp");

  const std::string output_checksum =
      PlatformChecksum("0c6dc227f781c81a229970f8fceda1a012498cba",
                       "15c4a2202877a414515e218bdb7992f0ad53e5af", "not used",
                       "0c6dc227f781c81a229970f8fceda1a012498cba",
                       "25fc4c863caa499aa447a5b8d059f5452cbcc500");

  const std::string network_stats_checksum =
      PlatformChecksum("4b2370f5c794741d2a46be5c7935c66ef3fb53e9",
                       "e339cb2adf5ab3dfc21cb7205d670a34751e8336", "not used",
                       "4b2370f5c794741d2a46be5c7935c66ef3fb53e9",
                       "4b2370f5c794741d2a46be5c7935c66ef3fb53e9");

  DecodeAndCompare(input_rtp_file, output_checksum, network_stats_checksum,
                   FLAG_gen_ref);
}

#if !defined(WEBRTC_IOS) && defined(WEBRTC_NETEQ_UNITTEST_BITEXACT) && \
    defined(WEBRTC_CODEC_OPUS)
#define MAYBE_TestOpusBitExactness TestOpusBitExactness
#else
#define MAYBE_TestOpusBitExactness DISABLED_TestOpusBitExactness
#endif
TEST_F(NetEqDecodingTest, MAYBE_TestOpusBitExactness) {
  const std::string input_rtp_file =
      webrtc::test::ResourcePath("audio_coding/neteq_opus", "rtp");

  // Checksum depends on libopus being compiled with or without SSE.
  const std::string maybe_sse =
      "14a63b3c7b925c82296be4bafc71bec85f2915c2|"
      "2c05677daa968d6c68b92adf4affb7cd9bb4d363";
  const std::string output_checksum = PlatformChecksum(
      maybe_sse, "b7b7ed802b0e18ee416973bf3b9ae98599b0181d",
      "5876e52dda90d5ca433c3726555b907b97c86374", maybe_sse, maybe_sse);

  const std::string network_stats_checksum =
      PlatformChecksum("adb3272498e436d1c019cbfd71610e9510c54497",
                       "fa935a91abc7291db47428a2d7c5361b98713a92",
                       "42106aa5267300f709f63737707ef07afd9dac61",
                       "adb3272498e436d1c019cbfd71610e9510c54497",
                       "adb3272498e436d1c019cbfd71610e9510c54497");

  DecodeAndCompare(input_rtp_file, output_checksum, network_stats_checksum,
                   FLAG_gen_ref);
}

#if !defined(WEBRTC_IOS) && defined(WEBRTC_NETEQ_UNITTEST_BITEXACT) && \
    defined(WEBRTC_CODEC_OPUS)
#define MAYBE_TestOpusDtxBitExactness TestOpusDtxBitExactness
#else
#define MAYBE_TestOpusDtxBitExactness DISABLED_TestOpusDtxBitExactness
#endif
TEST_F(NetEqDecodingTest, MAYBE_TestOpusDtxBitExactness) {
  const std::string input_rtp_file =
      webrtc::test::ResourcePath("audio_coding/neteq_opus_dtx", "rtp");

  const std::string maybe_sse =
      "713af6c92881f5aab1285765ee6680da9d1c06ce|"
      "2ac10c4e79aeedd0df2863b079da5848b40f00b5";
  const std::string output_checksum = PlatformChecksum(
      maybe_sse, "3ec991b96872123f1554c03c543ca5d518431e46",
      "da9f9a2d94e0c2d67342fad4965d7b91cda50b25", maybe_sse, maybe_sse);

  const std::string network_stats_checksum =
      "bab58dc587d956f326056d7340c96eb9d2d3cc21";

  DecodeAndCompare(input_rtp_file, output_checksum, network_stats_checksum,
                   FLAG_gen_ref);
}

// Use fax mode to avoid time-scaling. This is to simplify the testing of
// packet waiting times in the packet buffer.
class NetEqDecodingTestFaxMode : public NetEqDecodingTest {
 protected:
  NetEqDecodingTestFaxMode() : NetEqDecodingTest() {
    config_.for_test_no_time_stretching = true;
  }
  void TestJitterBufferDelay(bool apply_packet_loss);
};

TEST_F(NetEqDecodingTestFaxMode, TestFrameWaitingTimeStatistics) {
  // Insert 30 dummy packets at once. Each packet contains 10 ms 16 kHz audio.
  size_t num_frames = 30;
  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  for (size_t i = 0; i < num_frames; ++i) {
    const uint8_t payload[kPayloadBytes] = {0};
    RTPHeader rtp_info;
    rtp_info.sequenceNumber = rtc::checked_cast<uint16_t>(i);
    rtp_info.timestamp = rtc::checked_cast<uint32_t>(i * kSamples);
    rtp_info.ssrc = 0x1234;     // Just an arbitrary SSRC.
    rtp_info.payloadType = 94;  // PCM16b WB codec.
    rtp_info.markerBit = 0;
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
  }
  // Pull out all data.
  for (size_t i = 0; i < num_frames; ++i) {
    bool muted;
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  NetEqNetworkStatistics stats;
  EXPECT_EQ(0, neteq_->NetworkStatistics(&stats));
  // Since all frames are dumped into NetEQ at once, but pulled out with 10 ms
  // spacing (per definition), we expect the delay to increase with 10 ms for
  // each packet. Thus, we are calculating the statistics for a series from 10
  // to 300, in steps of 10 ms.
  EXPECT_EQ(155, stats.mean_waiting_time_ms);
  EXPECT_EQ(155, stats.median_waiting_time_ms);
  EXPECT_EQ(10, stats.min_waiting_time_ms);
  EXPECT_EQ(300, stats.max_waiting_time_ms);

  // Check statistics again and make sure it's been reset.
  EXPECT_EQ(0, neteq_->NetworkStatistics(&stats));
  EXPECT_EQ(-1, stats.mean_waiting_time_ms);
  EXPECT_EQ(-1, stats.median_waiting_time_ms);
  EXPECT_EQ(-1, stats.min_waiting_time_ms);
  EXPECT_EQ(-1, stats.max_waiting_time_ms);
}

TEST_F(NetEqDecodingTest, TestAverageInterArrivalTimeNegative) {
  const int kNumFrames = 3000;  // Needed for convergence.
  int frame_index = 0;
  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  while (frame_index < kNumFrames) {
    // Insert one packet each time, except every 10th time where we insert two
    // packets at once. This will create a negative clock-drift of approx. 10%.
    int num_packets = (frame_index % 10 == 0 ? 2 : 1);
    for (int n = 0; n < num_packets; ++n) {
      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(frame_index, frame_index * kSamples, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
      ++frame_index;
    }

    // Pull out data once.
    bool muted;
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  NetEqNetworkStatistics network_stats;
  ASSERT_EQ(0, neteq_->NetworkStatistics(&network_stats));
  EXPECT_EQ(-103192, network_stats.clockdrift_ppm);
}

TEST_F(NetEqDecodingTest, TestAverageInterArrivalTimePositive) {
  const int kNumFrames = 5000;  // Needed for convergence.
  int frame_index = 0;
  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  for (int i = 0; i < kNumFrames; ++i) {
    // Insert one packet each time, except every 10th time where we don't insert
    // any packet. This will create a positive clock-drift of approx. 11%.
    int num_packets = (i % 10 == 9 ? 0 : 1);
    for (int n = 0; n < num_packets; ++n) {
      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(frame_index, frame_index * kSamples, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
      ++frame_index;
    }

    // Pull out data once.
    bool muted;
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  NetEqNetworkStatistics network_stats;
  ASSERT_EQ(0, neteq_->NetworkStatistics(&network_stats));
  EXPECT_EQ(110953, network_stats.clockdrift_ppm);
}

void NetEqDecodingTest::LongCngWithClockDrift(double drift_factor,
                                              double network_freeze_ms,
                                              bool pull_audio_during_freeze,
                                              int delay_tolerance_ms,
                                              int max_time_to_speech_ms) {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 30;
  const size_t kSamples = kFrameSizeMs * 16;
  const size_t kPayloadBytes = kSamples * 2;
  double next_input_time_ms = 0.0;
  double t_ms;
  bool muted;

  // Insert speech for 5 seconds.
  const int kSpeechDurationMs = 5000;
  for (t_ms = 0; t_ms < kSpeechDurationMs; t_ms += 10) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one 30 ms speech frame.
      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs) * drift_factor;
    }
    // Pull out data once.
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
  absl::optional<uint32_t> playout_timestamp = neteq_->GetPlayoutTimestamp();
  ASSERT_TRUE(playout_timestamp);
  int32_t delay_before = timestamp - *playout_timestamp;

  // Insert CNG for 1 minute (= 60000 ms).
  const int kCngPeriodMs = 100;
  const int kCngPeriodSamples = kCngPeriodMs * 16;  // Period in 16 kHz samples.
  const int kCngDurationMs = 60000;
  for (; t_ms < kSpeechDurationMs + kCngDurationMs; t_ms += 10) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one CNG frame each 100 ms.
      uint8_t payload[kPayloadBytes];
      size_t payload_len;
      RTPHeader rtp_info;
      PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
      ASSERT_EQ(0, neteq_->InsertPacket(
                       rtp_info,
                       rtc::ArrayView<const uint8_t>(payload, payload_len), 0));
      ++seq_no;
      timestamp += kCngPeriodSamples;
      next_input_time_ms += static_cast<double>(kCngPeriodMs) * drift_factor;
    }
    // Pull out data once.
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }

  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);

  if (network_freeze_ms > 0) {
    // First keep pulling audio for |network_freeze_ms| without inserting
    // any data, then insert CNG data corresponding to |network_freeze_ms|
    // without pulling any output audio.
    const double loop_end_time = t_ms + network_freeze_ms;
    for (; t_ms < loop_end_time; t_ms += 10) {
      // Pull out data once.
      ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
      ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
      EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
    }
    bool pull_once = pull_audio_during_freeze;
    // If |pull_once| is true, GetAudio will be called once half-way through
    // the network recovery period.
    double pull_time_ms = (t_ms + next_input_time_ms) / 2;
    while (next_input_time_ms <= t_ms) {
      if (pull_once && next_input_time_ms >= pull_time_ms) {
        pull_once = false;
        // Pull out data once.
        ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
        ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
        EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
        t_ms += 10;
      }
      // Insert one CNG frame each 100 ms.
      uint8_t payload[kPayloadBytes];
      size_t payload_len;
      RTPHeader rtp_info;
      PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
      ASSERT_EQ(0, neteq_->InsertPacket(
                       rtp_info,
                       rtc::ArrayView<const uint8_t>(payload, payload_len), 0));
      ++seq_no;
      timestamp += kCngPeriodSamples;
      next_input_time_ms += kCngPeriodMs * drift_factor;
    }
  }

  // Insert speech again until output type is speech.
  double speech_restart_time_ms = t_ms;
  while (out_frame_.speech_type_ != AudioFrame::kNormalSpeech) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one 30 ms speech frame.
      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
      ++seq_no;
      timestamp += kSamples;
      next_input_time_ms += kFrameSizeMs * drift_factor;
    }
    // Pull out data once.
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
    // Increase clock.
    t_ms += 10;
  }

  // Check that the speech starts again within reasonable time.
  double time_until_speech_returns_ms = t_ms - speech_restart_time_ms;
  EXPECT_LT(time_until_speech_returns_ms, max_time_to_speech_ms);
  playout_timestamp = neteq_->GetPlayoutTimestamp();
  ASSERT_TRUE(playout_timestamp);
  int32_t delay_after = timestamp - *playout_timestamp;
  // Compare delay before and after, and make sure it differs less than 20 ms.
  EXPECT_LE(delay_after, delay_before + delay_tolerance_ms * 16);
  EXPECT_GE(delay_after, delay_before - delay_tolerance_ms * 16);
}

TEST_F(NetEqDecodingTest, LongCngWithNegativeClockDrift) {
  // Apply a clock drift of -25 ms / s (sender faster than receiver).
  const double kDriftFactor = 1000.0 / (1000.0 + 25.0);
  const double kNetworkFreezeTimeMs = 0.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 20;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithPositiveClockDrift) {
  // Apply a clock drift of +25 ms / s (sender slower than receiver).
  const double kDriftFactor = 1000.0 / (1000.0 - 25.0);
  const double kNetworkFreezeTimeMs = 0.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 20;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithNegativeClockDriftNetworkFreeze) {
  // Apply a clock drift of -25 ms / s (sender faster than receiver).
  const double kDriftFactor = 1000.0 / (1000.0 + 25.0);
  const double kNetworkFreezeTimeMs = 5000.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 50;
  const int kMaxTimeToSpeechMs = 200;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithPositiveClockDriftNetworkFreeze) {
  // Apply a clock drift of +25 ms / s (sender slower than receiver).
  const double kDriftFactor = 1000.0 / (1000.0 - 25.0);
  const double kNetworkFreezeTimeMs = 5000.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 20;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithPositiveClockDriftNetworkFreezeExtraPull) {
  // Apply a clock drift of +25 ms / s (sender slower than receiver).
  const double kDriftFactor = 1000.0 / (1000.0 - 25.0);
  const double kNetworkFreezeTimeMs = 5000.0;
  const bool kGetAudioDuringFreezeRecovery = true;
  const int kDelayToleranceMs = 20;
  const int kMaxTimeToSpeechMs = 100;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, LongCngWithoutClockDrift) {
  const double kDriftFactor = 1.0;  // No drift.
  const double kNetworkFreezeTimeMs = 0.0;
  const bool kGetAudioDuringFreezeRecovery = false;
  const int kDelayToleranceMs = 10;
  const int kMaxTimeToSpeechMs = 50;
  LongCngWithClockDrift(kDriftFactor, kNetworkFreezeTimeMs,
                        kGetAudioDuringFreezeRecovery, kDelayToleranceMs,
                        kMaxTimeToSpeechMs);
}

TEST_F(NetEqDecodingTest, UnknownPayloadType) {
  const size_t kPayloadBytes = 100;
  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  rtp_info.payloadType = 1;  // Not registered as a decoder.
  EXPECT_EQ(NetEq::kFail, neteq_->InsertPacket(rtp_info, payload, 0));
}

#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
#define MAYBE_DecoderError DecoderError
#else
#define MAYBE_DecoderError DISABLED_DecoderError
#endif

TEST_F(NetEqDecodingTest, MAYBE_DecoderError) {
  const size_t kPayloadBytes = 100;
  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  rtp_info.payloadType = 103;  // iSAC, but the payload is invalid.
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
  // Set all of |out_data_| to 1, and verify that it was set to 0 by the call
  // to GetAudio.
  int16_t* out_frame_data = out_frame_.mutable_data();
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; ++i) {
    out_frame_data[i] = 1;
  }
  bool muted;
  EXPECT_EQ(NetEq::kFail, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_FALSE(muted);

  // Verify that the first 160 samples are set to 0.
  static const int kExpectedOutputLength = 160;  // 10 ms at 16 kHz sample rate.
  const int16_t* const_out_frame_data = out_frame_.data();
  for (int i = 0; i < kExpectedOutputLength; ++i) {
    rtc::StringBuilder ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());  // Print out the parameter values on failure.
    EXPECT_EQ(0, const_out_frame_data[i]);
  }
}

TEST_F(NetEqDecodingTest, GetAudioBeforeInsertPacket) {
  // Set all of |out_data_| to 1, and verify that it was set to 0 by the call
  // to GetAudio.
  int16_t* out_frame_data = out_frame_.mutable_data();
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; ++i) {
    out_frame_data[i] = 1;
  }
  bool muted;
  EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_FALSE(muted);
  // Verify that the first block of samples is set to 0.
  static const int kExpectedOutputLength =
      kInitSampleRateHz / 100;  // 10 ms at initial sample rate.
  const int16_t* const_out_frame_data = out_frame_.data();
  for (int i = 0; i < kExpectedOutputLength; ++i) {
    rtc::StringBuilder ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());  // Print out the parameter values on failure.
    EXPECT_EQ(0, const_out_frame_data[i]);
  }
  // Verify that the sample rate did not change from the initial configuration.
  EXPECT_EQ(config_.sample_rate_hz, neteq_->last_output_sample_rate_hz());
}

class NetEqBgnTest : public NetEqDecodingTest {
 protected:
  void CheckBgn(int sampling_rate_hz) {
    size_t expected_samples_per_channel = 0;
    uint8_t payload_type = 0xFF;  // Invalid.
    if (sampling_rate_hz == 8000) {
      expected_samples_per_channel = kBlockSize8kHz;
      payload_type = 93;  // PCM 16, 8 kHz.
    } else if (sampling_rate_hz == 16000) {
      expected_samples_per_channel = kBlockSize16kHz;
      payload_type = 94;  // PCM 16, 16 kHZ.
    } else if (sampling_rate_hz == 32000) {
      expected_samples_per_channel = kBlockSize32kHz;
      payload_type = 95;  // PCM 16, 32 kHz.
    } else {
      ASSERT_TRUE(false);  // Unsupported test case.
    }

    AudioFrame output;
    test::AudioLoop input;
    // We are using the same 32 kHz input file for all tests, regardless of
    // |sampling_rate_hz|. The output may sound weird, but the test is still
    // valid.
    ASSERT_TRUE(input.Init(
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm"),
        10 * sampling_rate_hz,  // Max 10 seconds loop length.
        expected_samples_per_channel));

    // Payload of 10 ms of PCM16 32 kHz.
    uint8_t payload[kBlockSize32kHz * sizeof(int16_t)];
    RTPHeader rtp_info;
    PopulateRtpInfo(0, 0, &rtp_info);
    rtp_info.payloadType = payload_type;

    uint32_t receive_timestamp = 0;
    bool muted;
    for (int n = 0; n < 10; ++n) {  // Insert few packets and get audio.
      auto block = input.GetNextBlock();
      ASSERT_EQ(expected_samples_per_channel, block.size());
      size_t enc_len_bytes =
          WebRtcPcm16b_Encode(block.data(), block.size(), payload);
      ASSERT_EQ(enc_len_bytes, expected_samples_per_channel * 2);

      ASSERT_EQ(0, neteq_->InsertPacket(
                       rtp_info,
                       rtc::ArrayView<const uint8_t>(payload, enc_len_bytes),
                       receive_timestamp));
      output.Reset();
      ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
      ASSERT_EQ(1u, output.num_channels_);
      ASSERT_EQ(expected_samples_per_channel, output.samples_per_channel_);
      ASSERT_EQ(AudioFrame::kNormalSpeech, output.speech_type_);

      // Next packet.
      rtp_info.timestamp +=
          rtc::checked_cast<uint32_t>(expected_samples_per_channel);
      rtp_info.sequenceNumber++;
      receive_timestamp +=
          rtc::checked_cast<uint32_t>(expected_samples_per_channel);
    }

    output.Reset();

    // Get audio without inserting packets, expecting PLC and PLC-to-CNG. Pull
    // one frame without checking speech-type. This is the first frame pulled
    // without inserting any packet, and might not be labeled as PLC.
    ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
    ASSERT_EQ(1u, output.num_channels_);
    ASSERT_EQ(expected_samples_per_channel, output.samples_per_channel_);

    // To be able to test the fading of background noise we need at lease to
    // pull 611 frames.
    const int kFadingThreshold = 611;

    // Test several CNG-to-PLC packet for the expected behavior. The number 20
    // is arbitrary, but sufficiently large to test enough number of frames.
    const int kNumPlcToCngTestFrames = 20;
    bool plc_to_cng = false;
    for (int n = 0; n < kFadingThreshold + kNumPlcToCngTestFrames; ++n) {
      output.Reset();
      // Set to non-zero.
      memset(output.mutable_data(), 1, AudioFrame::kMaxDataSizeBytes);
      ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
      ASSERT_FALSE(muted);
      ASSERT_EQ(1u, output.num_channels_);
      ASSERT_EQ(expected_samples_per_channel, output.samples_per_channel_);
      if (output.speech_type_ == AudioFrame::kPLCCNG) {
        plc_to_cng = true;
        double sum_squared = 0;
        const int16_t* output_data = output.data();
        for (size_t k = 0;
             k < output.num_channels_ * output.samples_per_channel_; ++k)
          sum_squared += output_data[k] * output_data[k];
        EXPECT_EQ(0, sum_squared);
      } else {
        EXPECT_EQ(AudioFrame::kPLC, output.speech_type_);
      }
    }
    EXPECT_TRUE(plc_to_cng);  // Just to be sure that PLC-to-CNG has occurred.
  }
};

TEST_F(NetEqBgnTest, RunTest) {
  CheckBgn(8000);
  CheckBgn(16000);
  CheckBgn(32000);
}

void NetEqDecodingTest::WrapTest(uint16_t start_seq_no,
                                 uint32_t start_timestamp,
                                 const std::set<uint16_t>& drop_seq_numbers,
                                 bool expect_seq_no_wrap,
                                 bool expect_timestamp_wrap) {
  uint16_t seq_no = start_seq_no;
  uint32_t timestamp = start_timestamp;
  const int kBlocksPerFrame = 3;  // Number of 10 ms blocks per frame.
  const int kFrameSizeMs = kBlocksPerFrame * kTimeStepMs;
  const int kSamples = kBlockSize16kHz * kBlocksPerFrame;
  const size_t kPayloadBytes = kSamples * sizeof(int16_t);
  double next_input_time_ms = 0.0;
  uint32_t receive_timestamp = 0;

  // Insert speech for 2 seconds.
  const int kSpeechDurationMs = 2000;
  int packets_inserted = 0;
  uint16_t last_seq_no;
  uint32_t last_timestamp;
  bool timestamp_wrapped = false;
  bool seq_no_wrapped = false;
  for (double t_ms = 0; t_ms < kSpeechDurationMs; t_ms += 10) {
    // Each turn in this for loop is 10 ms.
    while (next_input_time_ms <= t_ms) {
      // Insert one 30 ms speech frame.
      uint8_t payload[kPayloadBytes] = {0};
      RTPHeader rtp_info;
      PopulateRtpInfo(seq_no, timestamp, &rtp_info);
      if (drop_seq_numbers.find(seq_no) == drop_seq_numbers.end()) {
        // This sequence number was not in the set to drop. Insert it.
        ASSERT_EQ(0,
                  neteq_->InsertPacket(rtp_info, payload, receive_timestamp));
        ++packets_inserted;
      }
      NetEqNetworkStatistics network_stats;
      ASSERT_EQ(0, neteq_->NetworkStatistics(&network_stats));

      // Due to internal NetEq logic, preferred buffer-size is about 4 times the
      // packet size for first few packets. Therefore we refrain from checking
      // the criteria.
      if (packets_inserted > 4) {
        // Expect preferred and actual buffer size to be no more than 2 frames.
        EXPECT_LE(network_stats.preferred_buffer_size_ms, kFrameSizeMs * 2);
        EXPECT_LE(network_stats.current_buffer_size_ms,
                  kFrameSizeMs * 2 + algorithmic_delay_ms_);
      }
      last_seq_no = seq_no;
      last_timestamp = timestamp;

      ++seq_no;
      timestamp += kSamples;
      receive_timestamp += kSamples;
      next_input_time_ms += static_cast<double>(kFrameSizeMs);

      seq_no_wrapped |= seq_no < last_seq_no;
      timestamp_wrapped |= timestamp < last_timestamp;
    }
    // Pull out data once.
    AudioFrame output;
    bool muted;
    ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
    ASSERT_EQ(kBlockSize16kHz, output.samples_per_channel_);
    ASSERT_EQ(1u, output.num_channels_);

    // Expect delay (in samples) to be less than 2 packets.
    absl::optional<uint32_t> playout_timestamp = neteq_->GetPlayoutTimestamp();
    ASSERT_TRUE(playout_timestamp);
    EXPECT_LE(timestamp - *playout_timestamp,
              static_cast<uint32_t>(kSamples * 2));
  }
  // Make sure we have actually tested wrap-around.
  ASSERT_EQ(expect_seq_no_wrap, seq_no_wrapped);
  ASSERT_EQ(expect_timestamp_wrap, timestamp_wrapped);
}

TEST_F(NetEqDecodingTest, SequenceNumberWrap) {
  // Start with a sequence number that will soon wrap.
  std::set<uint16_t> drop_seq_numbers;  // Don't drop any packets.
  WrapTest(0xFFFF - 10, 0, drop_seq_numbers, true, false);
}

TEST_F(NetEqDecodingTest, SequenceNumberWrapAndDrop) {
  // Start with a sequence number that will soon wrap.
  std::set<uint16_t> drop_seq_numbers;
  drop_seq_numbers.insert(0xFFFF);
  drop_seq_numbers.insert(0x0);
  WrapTest(0xFFFF - 10, 0, drop_seq_numbers, true, false);
}

TEST_F(NetEqDecodingTest, TimestampWrap) {
  // Start with a timestamp that will soon wrap.
  std::set<uint16_t> drop_seq_numbers;
  WrapTest(0, 0xFFFFFFFF - 3000, drop_seq_numbers, false, true);
}

TEST_F(NetEqDecodingTest, TimestampAndSequenceNumberWrap) {
  // Start with a timestamp and a sequence number that will wrap at the same
  // time.
  std::set<uint16_t> drop_seq_numbers;
  WrapTest(0xFFFF - 10, 0xFFFFFFFF - 5000, drop_seq_numbers, true, true);
}

void NetEqDecodingTest::DuplicateCng() {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 10;
  const int kSampleRateKhz = 16;
  const int kSamples = kFrameSizeMs * kSampleRateKhz;
  const size_t kPayloadBytes = kSamples * 2;

  const int algorithmic_delay_samples =
      std::max(algorithmic_delay_ms_ * kSampleRateKhz, 5 * kSampleRateKhz / 8);
  // Insert three speech packets. Three are needed to get the frame length
  // correct.
  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  bool muted;
  for (int i = 0; i < 3; ++i) {
    PopulateRtpInfo(seq_no, timestamp, &rtp_info);
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
    ++seq_no;
    timestamp += kSamples;

    // Pull audio once.
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  }
  // Verify speech output.
  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);

  // Insert same CNG packet twice.
  const int kCngPeriodMs = 100;
  const int kCngPeriodSamples = kCngPeriodMs * kSampleRateKhz;
  size_t payload_len;
  PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
  // This is the first time this CNG packet is inserted.
  ASSERT_EQ(
      0, neteq_->InsertPacket(
             rtp_info, rtc::ArrayView<const uint8_t>(payload, payload_len), 0));

  // Pull audio once and make sure CNG is played.
  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
  EXPECT_FALSE(
      neteq_->GetPlayoutTimestamp());  // Returns empty value during CNG.
  EXPECT_EQ(timestamp - algorithmic_delay_samples,
            out_frame_.timestamp_ + out_frame_.samples_per_channel_);

  // Insert the same CNG packet again. Note that at this point it is old, since
  // we have already decoded the first copy of it.
  ASSERT_EQ(
      0, neteq_->InsertPacket(
             rtp_info, rtc::ArrayView<const uint8_t>(payload, payload_len), 0));

  // Pull audio until we have played |kCngPeriodMs| of CNG. Start at 10 ms since
  // we have already pulled out CNG once.
  for (int cng_time_ms = 10; cng_time_ms < kCngPeriodMs; cng_time_ms += 10) {
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
    EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
    EXPECT_FALSE(
        neteq_->GetPlayoutTimestamp());  // Returns empty value during CNG.
    EXPECT_EQ(timestamp - algorithmic_delay_samples,
              out_frame_.timestamp_ + out_frame_.samples_per_channel_);
  }

  // Insert speech again.
  ++seq_no;
  timestamp += kCngPeriodSamples;
  PopulateRtpInfo(seq_no, timestamp, &rtp_info);
  ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));

  // Pull audio once and verify that the output is speech again.
  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
  absl::optional<uint32_t> playout_timestamp = neteq_->GetPlayoutTimestamp();
  ASSERT_TRUE(playout_timestamp);
  EXPECT_EQ(timestamp + kSamples - algorithmic_delay_samples,
            *playout_timestamp);
}

TEST_F(NetEqDecodingTest, DiscardDuplicateCng) {
  DuplicateCng();
}

TEST_F(NetEqDecodingTest, CngFirst) {
  uint16_t seq_no = 0;
  uint32_t timestamp = 0;
  const int kFrameSizeMs = 10;
  const int kSampleRateKhz = 16;
  const int kSamples = kFrameSizeMs * kSampleRateKhz;
  const int kPayloadBytes = kSamples * 2;
  const int kCngPeriodMs = 100;
  const int kCngPeriodSamples = kCngPeriodMs * kSampleRateKhz;
  size_t payload_len;

  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;

  PopulateCng(seq_no, timestamp, &rtp_info, payload, &payload_len);
  ASSERT_EQ(
      NetEq::kOK,
      neteq_->InsertPacket(
          rtp_info, rtc::ArrayView<const uint8_t>(payload, payload_len), 0));
  ++seq_no;
  timestamp += kCngPeriodSamples;

  // Pull audio once and make sure CNG is played.
  bool muted;
  ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);

  // Insert some speech packets.
  const uint32_t first_speech_timestamp = timestamp;
  int timeout_counter = 0;
  do {
    ASSERT_LT(timeout_counter++, 20) << "Test timed out";
    PopulateRtpInfo(seq_no, timestamp, &rtp_info);
    ASSERT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
    ++seq_no;
    timestamp += kSamples;

    // Pull audio once.
    ASSERT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_EQ(kBlockSize16kHz, out_frame_.samples_per_channel_);
  } while (!IsNewerTimestamp(out_frame_.timestamp_, first_speech_timestamp));
  // Verify speech output.
  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
}

class NetEqDecodingTestWithMutedState : public NetEqDecodingTest {
 public:
  NetEqDecodingTestWithMutedState() : NetEqDecodingTest() {
    config_.enable_muted_state = true;
  }

 protected:
  static constexpr size_t kSamples = 10 * 16;
  static constexpr size_t kPayloadBytes = kSamples * 2;

  void InsertPacket(uint32_t rtp_timestamp) {
    uint8_t payload[kPayloadBytes] = {0};
    RTPHeader rtp_info;
    PopulateRtpInfo(0, rtp_timestamp, &rtp_info);
    EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
  }

  void InsertCngPacket(uint32_t rtp_timestamp) {
    uint8_t payload[kPayloadBytes] = {0};
    RTPHeader rtp_info;
    size_t payload_len;
    PopulateCng(0, rtp_timestamp, &rtp_info, payload, &payload_len);
    EXPECT_EQ(
        NetEq::kOK,
        neteq_->InsertPacket(
            rtp_info, rtc::ArrayView<const uint8_t>(payload, payload_len), 0));
  }

  bool GetAudioReturnMuted() {
    bool muted;
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    return muted;
  }

  void GetAudioUntilMuted() {
    while (!GetAudioReturnMuted()) {
      ASSERT_LT(counter_++, 1000) << "Test timed out";
    }
  }

  void GetAudioUntilNormal() {
    bool muted = false;
    while (out_frame_.speech_type_ != AudioFrame::kNormalSpeech) {
      EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
      ASSERT_LT(counter_++, 1000) << "Test timed out";
    }
    EXPECT_FALSE(muted);
  }

  int counter_ = 0;
};

// Verifies that NetEq goes in and out of muted state as expected.
TEST_F(NetEqDecodingTestWithMutedState, MutedState) {
  // Insert one speech packet.
  InsertPacket(0);
  // Pull out audio once and expect it not to be muted.
  EXPECT_FALSE(GetAudioReturnMuted());
  // Pull data until faded out.
  GetAudioUntilMuted();
  EXPECT_TRUE(out_frame_.muted());

  // Verify that output audio is not written during muted mode. Other parameters
  // should be correct, though.
  AudioFrame new_frame;
  int16_t* frame_data = new_frame.mutable_data();
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; i++) {
    frame_data[i] = 17;
  }
  bool muted;
  EXPECT_EQ(0, neteq_->GetAudio(&new_frame, &muted));
  EXPECT_TRUE(muted);
  EXPECT_TRUE(out_frame_.muted());
  for (size_t i = 0; i < AudioFrame::kMaxDataSizeSamples; i++) {
    EXPECT_EQ(17, frame_data[i]);
  }
  EXPECT_EQ(out_frame_.timestamp_ + out_frame_.samples_per_channel_,
            new_frame.timestamp_);
  EXPECT_EQ(out_frame_.samples_per_channel_, new_frame.samples_per_channel_);
  EXPECT_EQ(out_frame_.sample_rate_hz_, new_frame.sample_rate_hz_);
  EXPECT_EQ(out_frame_.num_channels_, new_frame.num_channels_);
  EXPECT_EQ(out_frame_.speech_type_, new_frame.speech_type_);
  EXPECT_EQ(out_frame_.vad_activity_, new_frame.vad_activity_);

  // Insert new data. Timestamp is corrected for the time elapsed since the last
  // packet. Verify that normal operation resumes.
  InsertPacket(kSamples * counter_);
  GetAudioUntilNormal();
  EXPECT_FALSE(out_frame_.muted());

  NetEqNetworkStatistics stats;
  EXPECT_EQ(0, neteq_->NetworkStatistics(&stats));
  // NetEqNetworkStatistics::expand_rate tells the fraction of samples that were
  // concealment samples, in Q14 (16384 = 100%) .The vast majority should be
  // concealment samples in this test.
  EXPECT_GT(stats.expand_rate, 14000);
  // And, it should be greater than the speech_expand_rate.
  EXPECT_GT(stats.expand_rate, stats.speech_expand_rate);
}

// Verifies that NetEq goes out of muted state when given a delayed packet.
TEST_F(NetEqDecodingTestWithMutedState, MutedStateDelayedPacket) {
  // Insert one speech packet.
  InsertPacket(0);
  // Pull out audio once and expect it not to be muted.
  EXPECT_FALSE(GetAudioReturnMuted());
  // Pull data until faded out.
  GetAudioUntilMuted();
  // Insert new data. Timestamp is only corrected for the half of the time
  // elapsed since the last packet. That is, the new packet is delayed. Verify
  // that normal operation resumes.
  InsertPacket(kSamples * counter_ / 2);
  GetAudioUntilNormal();
}

// Verifies that NetEq goes out of muted state when given a future packet.
TEST_F(NetEqDecodingTestWithMutedState, MutedStateFuturePacket) {
  // Insert one speech packet.
  InsertPacket(0);
  // Pull out audio once and expect it not to be muted.
  EXPECT_FALSE(GetAudioReturnMuted());
  // Pull data until faded out.
  GetAudioUntilMuted();
  // Insert new data. Timestamp is over-corrected for the time elapsed since the
  // last packet. That is, the new packet is too early. Verify that normal
  // operation resumes.
  InsertPacket(kSamples * counter_ * 2);
  GetAudioUntilNormal();
}

// Verifies that NetEq goes out of muted state when given an old packet.
TEST_F(NetEqDecodingTestWithMutedState, MutedStateOldPacket) {
  // Insert one speech packet.
  InsertPacket(0);
  // Pull out audio once and expect it not to be muted.
  EXPECT_FALSE(GetAudioReturnMuted());
  // Pull data until faded out.
  GetAudioUntilMuted();

  EXPECT_NE(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
  // Insert packet which is older than the first packet.
  InsertPacket(kSamples * (counter_ - 1000));
  EXPECT_FALSE(GetAudioReturnMuted());
  EXPECT_EQ(AudioFrame::kNormalSpeech, out_frame_.speech_type_);
}

// Verifies that NetEq doesn't enter muted state when CNG mode is active and the
// packet stream is suspended for a long time.
TEST_F(NetEqDecodingTestWithMutedState, DoNotMuteExtendedCngWithoutPackets) {
  // Insert one CNG packet.
  InsertCngPacket(0);

  // Pull 10 seconds of audio (10 ms audio generated per lap).
  for (int i = 0; i < 1000; ++i) {
    bool muted;
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
    ASSERT_FALSE(muted);
  }
  EXPECT_EQ(AudioFrame::kCNG, out_frame_.speech_type_);
}

// Verifies that NetEq goes back to normal after a long CNG period with the
// packet stream suspended.
TEST_F(NetEqDecodingTestWithMutedState, RecoverAfterExtendedCngWithoutPackets) {
  // Insert one CNG packet.
  InsertCngPacket(0);

  // Pull 10 seconds of audio (10 ms audio generated per lap).
  for (int i = 0; i < 1000; ++i) {
    bool muted;
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame_, &muted));
  }

  // Insert new data. Timestamp is corrected for the time elapsed since the last
  // packet. Verify that normal operation resumes.
  InsertPacket(kSamples * counter_);
  GetAudioUntilNormal();
}

class NetEqDecodingTestTwoInstances : public NetEqDecodingTest {
 public:
  NetEqDecodingTestTwoInstances() : NetEqDecodingTest() {}

  void SetUp() override {
    NetEqDecodingTest::SetUp();
    config2_ = config_;
  }

  void CreateSecondInstance() {
    neteq2_.reset(NetEq::Create(config2_, CreateBuiltinAudioDecoderFactory()));
    ASSERT_TRUE(neteq2_);
    LoadDecoders(neteq2_.get());
  }

 protected:
  std::unique_ptr<NetEq> neteq2_;
  NetEq::Config config2_;
};

namespace {
::testing::AssertionResult AudioFramesEqualExceptData(const AudioFrame& a,
                                                      const AudioFrame& b) {
  if (a.timestamp_ != b.timestamp_)
    return ::testing::AssertionFailure() << "timestamp_ diff (" << a.timestamp_
                                         << " != " << b.timestamp_ << ")";
  if (a.sample_rate_hz_ != b.sample_rate_hz_)
    return ::testing::AssertionFailure()
           << "sample_rate_hz_ diff (" << a.sample_rate_hz_
           << " != " << b.sample_rate_hz_ << ")";
  if (a.samples_per_channel_ != b.samples_per_channel_)
    return ::testing::AssertionFailure()
           << "samples_per_channel_ diff (" << a.samples_per_channel_
           << " != " << b.samples_per_channel_ << ")";
  if (a.num_channels_ != b.num_channels_)
    return ::testing::AssertionFailure()
           << "num_channels_ diff (" << a.num_channels_
           << " != " << b.num_channels_ << ")";
  if (a.speech_type_ != b.speech_type_)
    return ::testing::AssertionFailure()
           << "speech_type_ diff (" << a.speech_type_
           << " != " << b.speech_type_ << ")";
  if (a.vad_activity_ != b.vad_activity_)
    return ::testing::AssertionFailure()
           << "vad_activity_ diff (" << a.vad_activity_
           << " != " << b.vad_activity_ << ")";
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult AudioFramesEqual(const AudioFrame& a,
                                            const AudioFrame& b) {
  ::testing::AssertionResult res = AudioFramesEqualExceptData(a, b);
  if (!res)
    return res;
  if (memcmp(a.data(), b.data(),
             a.samples_per_channel_ * a.num_channels_ * sizeof(*a.data())) !=
      0) {
    return ::testing::AssertionFailure() << "data_ diff";
  }
  return ::testing::AssertionSuccess();
}

}  // namespace

TEST_F(NetEqDecodingTestTwoInstances, CompareMutedStateOnOff) {
  ASSERT_FALSE(config_.enable_muted_state);
  config2_.enable_muted_state = true;
  CreateSecondInstance();

  // Insert one speech packet into both NetEqs.
  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  uint8_t payload[kPayloadBytes] = {0};
  RTPHeader rtp_info;
  PopulateRtpInfo(0, 0, &rtp_info);
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
  EXPECT_EQ(0, neteq2_->InsertPacket(rtp_info, payload, 0));

  AudioFrame out_frame1, out_frame2;
  bool muted;
  for (int i = 0; i < 1000; ++i) {
    rtc::StringBuilder ss;
    ss << "i = " << i;
    SCOPED_TRACE(ss.str());  // Print out the loop iterator on failure.
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame1, &muted));
    EXPECT_FALSE(muted);
    EXPECT_EQ(0, neteq2_->GetAudio(&out_frame2, &muted));
    if (muted) {
      EXPECT_TRUE(AudioFramesEqualExceptData(out_frame1, out_frame2));
    } else {
      EXPECT_TRUE(AudioFramesEqual(out_frame1, out_frame2));
    }
  }
  EXPECT_TRUE(muted);

  // Insert new data. Timestamp is corrected for the time elapsed since the last
  // packet.
  PopulateRtpInfo(0, kSamples * 1000, &rtp_info);
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
  EXPECT_EQ(0, neteq2_->InsertPacket(rtp_info, payload, 0));

  int counter = 0;
  while (out_frame1.speech_type_ != AudioFrame::kNormalSpeech) {
    ASSERT_LT(counter++, 1000) << "Test timed out";
    rtc::StringBuilder ss;
    ss << "counter = " << counter;
    SCOPED_TRACE(ss.str());  // Print out the loop iterator on failure.
    EXPECT_EQ(0, neteq_->GetAudio(&out_frame1, &muted));
    EXPECT_FALSE(muted);
    EXPECT_EQ(0, neteq2_->GetAudio(&out_frame2, &muted));
    if (muted) {
      EXPECT_TRUE(AudioFramesEqualExceptData(out_frame1, out_frame2));
    } else {
      EXPECT_TRUE(AudioFramesEqual(out_frame1, out_frame2));
    }
  }
  EXPECT_FALSE(muted);
}

TEST_F(NetEqDecodingTest, LastDecodedTimestampsEmpty) {
  EXPECT_TRUE(neteq_->LastDecodedTimestamps().empty());

  // Pull out data once.
  AudioFrame output;
  bool muted;
  ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));

  EXPECT_TRUE(neteq_->LastDecodedTimestamps().empty());
}

TEST_F(NetEqDecodingTest, LastDecodedTimestampsOneDecoded) {
  // Insert one packet with PCM16b WB data (this is what PopulateRtpInfo does by
  // default). Make the length 10 ms.
  constexpr size_t kPayloadSamples = 16 * 10;
  constexpr size_t kPayloadBytes = 2 * kPayloadSamples;
  uint8_t payload[kPayloadBytes] = {0};

  RTPHeader rtp_info;
  constexpr uint32_t kRtpTimestamp = 0x1234;
  PopulateRtpInfo(0, kRtpTimestamp, &rtp_info);
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));

  // Pull out data once.
  AudioFrame output;
  bool muted;
  ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));

  EXPECT_EQ(std::vector<uint32_t>({kRtpTimestamp}),
            neteq_->LastDecodedTimestamps());

  // Nothing decoded on the second call.
  ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));
  EXPECT_TRUE(neteq_->LastDecodedTimestamps().empty());
}

TEST_F(NetEqDecodingTest, LastDecodedTimestampsTwoDecoded) {
  // Insert two packets with PCM16b WB data (this is what PopulateRtpInfo does
  // by default). Make the length 5 ms so that NetEq must decode them both in
  // the same GetAudio call.
  constexpr size_t kPayloadSamples = 16 * 5;
  constexpr size_t kPayloadBytes = 2 * kPayloadSamples;
  uint8_t payload[kPayloadBytes] = {0};

  RTPHeader rtp_info;
  constexpr uint32_t kRtpTimestamp1 = 0x1234;
  PopulateRtpInfo(0, kRtpTimestamp1, &rtp_info);
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));
  constexpr uint32_t kRtpTimestamp2 = kRtpTimestamp1 + kPayloadSamples;
  PopulateRtpInfo(1, kRtpTimestamp2, &rtp_info);
  EXPECT_EQ(0, neteq_->InsertPacket(rtp_info, payload, 0));

  // Pull out data once.
  AudioFrame output;
  bool muted;
  ASSERT_EQ(0, neteq_->GetAudio(&output, &muted));

  EXPECT_EQ(std::vector<uint32_t>({kRtpTimestamp1, kRtpTimestamp2}),
            neteq_->LastDecodedTimestamps());
}

TEST_F(NetEqDecodingTest, TestConcealmentEvents) {
  const int kNumConcealmentEvents = 19;
  const size_t kSamples = 10 * 16;
  const size_t kPayloadBytes = kSamples * 2;
  int seq_no = 0;
  RTPHeader rtp_info;
  rtp_info.ssrc = 0x1234;     // Just an arbitrary SSRC.
  rtp_info.payloadType = 94;  // PCM16b WB codec.
  rtp_info.markerBit = 0;
  const uint8_t payload[kPayloadBytes] = {0};
  bool muted;

  for (int i = 0; i < kNumConcealmentEvents; i++) {
    // Insert some packets of 10 ms size.
    for (int j = 0; j < 10; j++) {
      rtp_info.sequenceNumber = seq_no++;
      rtp_info.timestamp = rtp_info.sequenceNumber * kSamples;
      neteq_->InsertPacket(rtp_info, payload, 0);
      neteq_->GetAudio(&out_frame_, &muted);
    }

    // Lose a number of packets.
    int num_lost = 1 + i;
    for (int j = 0; j < num_lost; j++) {
      seq_no++;
      neteq_->GetAudio(&out_frame_, &muted);
    }
  }

  // Check number of concealment events.
  NetEqLifetimeStatistics stats = neteq_->GetLifetimeStatistics();
  EXPECT_EQ(kNumConcealmentEvents, static_cast<int>(stats.concealment_events));
}

// Test that the jitter buffer delay stat is computed correctly.
void NetEqDecodingTestFaxMode::TestJitterBufferDelay(bool apply_packet_loss) {
  const int kNumPackets = 10;
  const int kDelayInNumPackets = 2;
  const int kPacketLenMs = 10;  // All packets are of 10 ms size.
  const size_t kSamples = kPacketLenMs * 16;
  const size_t kPayloadBytes = kSamples * 2;
  RTPHeader rtp_info;
  rtp_info.ssrc = 0x1234;     // Just an arbitrary SSRC.
  rtp_info.payloadType = 94;  // PCM16b WB codec.
  rtp_info.markerBit = 0;
  const uint8_t payload[kPayloadBytes] = {0};
  bool muted;
  int packets_sent = 0;
  int packets_received = 0;
  int expected_delay = 0;
  while (packets_received < kNumPackets) {
    // Insert packet.
    if (packets_sent < kNumPackets) {
      rtp_info.sequenceNumber = packets_sent++;
      rtp_info.timestamp = rtp_info.sequenceNumber * kSamples;
      neteq_->InsertPacket(rtp_info, payload, 0);
    }

    // Get packet.
    if (packets_sent > kDelayInNumPackets) {
      neteq_->GetAudio(&out_frame_, &muted);
      packets_received++;

      // The delay reported by the jitter buffer never exceeds
      // the number of samples previously fetched with GetAudio
      // (hence the min()).
      int packets_delay = std::min(packets_received, kDelayInNumPackets + 1);

      // The increase of the expected delay is the product of
      // the current delay of the jitter buffer in ms * the
      // number of samples that are sent for play out.
      int current_delay_ms = packets_delay * kPacketLenMs;
      expected_delay += current_delay_ms * kSamples;
    }
  }

  if (apply_packet_loss) {
    // Extra call to GetAudio to cause concealment.
    neteq_->GetAudio(&out_frame_, &muted);
  }

  // Check jitter buffer delay.
  NetEqLifetimeStatistics stats = neteq_->GetLifetimeStatistics();
  EXPECT_EQ(expected_delay, static_cast<int>(stats.jitter_buffer_delay_ms));
}

TEST_F(NetEqDecodingTestFaxMode, TestJitterBufferDelayWithoutLoss) {
  TestJitterBufferDelay(false);
}

TEST_F(NetEqDecodingTestFaxMode, TestJitterBufferDelayWithLoss) {
  TestJitterBufferDelay(true);
}

namespace test {
TEST(NetEqNoTimeStretchingMode, RunTest) {
  NetEq::Config config;
  config.for_test_no_time_stretching = true;
  auto codecs = NetEqTest::StandardDecoderMap();
  NetEqTest::ExtDecoderMap ext_codecs;
  NetEqPacketSourceInput::RtpHeaderExtensionMap rtp_ext_map = {
      {1, kRtpExtensionAudioLevel},
      {3, kRtpExtensionAbsoluteSendTime},
      {5, kRtpExtensionTransportSequenceNumber},
      {7, kRtpExtensionVideoContentType},
      {8, kRtpExtensionVideoTiming}};
  std::unique_ptr<NetEqInput> input(new NetEqRtpDumpInput(
      webrtc::test::ResourcePath("audio_coding/neteq_universal_new", "rtp"),
      rtp_ext_map, absl::nullopt /*No SSRC filter*/));
  std::unique_ptr<TimeLimitedNetEqInput> input_time_limit(
      new TimeLimitedNetEqInput(std::move(input), 20000));
  std::unique_ptr<AudioSink> output(new VoidAudioSink);
  NetEqTest::Callbacks callbacks;
  NetEqTest test(config, codecs, ext_codecs, std::move(input_time_limit),
                 std::move(output), callbacks);
  test.Run();
  const auto stats = test.SimulationStats();
  EXPECT_EQ(0, stats.accelerate_rate);
  EXPECT_EQ(0, stats.preemptive_rate);
}

}  // namespace test
}  // namespace webrtc
