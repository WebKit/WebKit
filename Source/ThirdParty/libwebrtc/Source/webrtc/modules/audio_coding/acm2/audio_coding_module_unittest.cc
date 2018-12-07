/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <memory>
#include <vector>

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "modules/audio_coding/acm2/acm_receive_test.h"
#include "modules/audio_coding/acm2/acm_send_test.h"
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "modules/audio_coding/codecs/g711/audio_decoder_pcm.h"
#include "modules/audio_coding/codecs/g711/audio_encoder_pcm.h"
#include "modules/audio_coding/codecs/isac/main/include/audio_encoder_isac.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/audio_coding/neteq/tools/audio_checksum.h"
#include "modules/audio_coding/neteq/tools/audio_loop.h"
#include "modules/audio_coding/neteq/tools/constant_pcm_packet_source.h"
#include "modules/audio_coding/neteq/tools/input_audio_file.h"
#include "modules/audio_coding/neteq/tools/output_audio_file.h"
#include "modules/audio_coding/neteq/tools/output_wav_file.h"
#include "modules/audio_coding/neteq/tools/packet.h"
#include "modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/event.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/system/arch.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder.h"
#include "test/mock_audio_encoder.h"
#include "test/testsupport/fileutils.h"

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::_;

namespace webrtc {

namespace {
const int kSampleRateHz = 16000;
const int kNumSamples10ms = kSampleRateHz / 100;
const int kFrameSizeMs = 10;  // Multiple of 10.
const int kFrameSizeSamples = kFrameSizeMs / 10 * kNumSamples10ms;
const int kPayloadSizeBytes = kFrameSizeSamples * sizeof(int16_t);
const uint8_t kPayloadType = 111;
}  // namespace

class RtpUtility {
 public:
  RtpUtility(int samples_per_packet, uint8_t payload_type)
      : samples_per_packet_(samples_per_packet), payload_type_(payload_type) {}

  virtual ~RtpUtility() {}

  void Populate(WebRtcRTPHeader* rtp_header) {
    rtp_header->header.sequenceNumber = 0xABCD;
    rtp_header->header.timestamp = 0xABCDEF01;
    rtp_header->header.payloadType = payload_type_;
    rtp_header->header.markerBit = false;
    rtp_header->header.ssrc = 0x1234;
    rtp_header->header.numCSRCs = 0;
    rtp_header->frameType = kAudioFrameSpeech;

    rtp_header->header.payload_type_frequency = kSampleRateHz;
  }

  void Forward(WebRtcRTPHeader* rtp_header) {
    ++rtp_header->header.sequenceNumber;
    rtp_header->header.timestamp += samples_per_packet_;
  }

 private:
  int samples_per_packet_;
  uint8_t payload_type_;
};

class PacketizationCallbackStubOldApi : public AudioPacketizationCallback {
 public:
  PacketizationCallbackStubOldApi()
      : num_calls_(0),
        last_frame_type_(kEmptyFrame),
        last_payload_type_(-1),
        last_timestamp_(0) {}

  int32_t SendData(FrameType frame_type,
                   uint8_t payload_type,
                   uint32_t timestamp,
                   const uint8_t* payload_data,
                   size_t payload_len_bytes,
                   const RTPFragmentationHeader* fragmentation) override {
    rtc::CritScope lock(&crit_sect_);
    ++num_calls_;
    last_frame_type_ = frame_type;
    last_payload_type_ = payload_type;
    last_timestamp_ = timestamp;
    last_payload_vec_.assign(payload_data, payload_data + payload_len_bytes);
    return 0;
  }

  int num_calls() const {
    rtc::CritScope lock(&crit_sect_);
    return num_calls_;
  }

  int last_payload_len_bytes() const {
    rtc::CritScope lock(&crit_sect_);
    return rtc::checked_cast<int>(last_payload_vec_.size());
  }

  FrameType last_frame_type() const {
    rtc::CritScope lock(&crit_sect_);
    return last_frame_type_;
  }

  int last_payload_type() const {
    rtc::CritScope lock(&crit_sect_);
    return last_payload_type_;
  }

  uint32_t last_timestamp() const {
    rtc::CritScope lock(&crit_sect_);
    return last_timestamp_;
  }

  void SwapBuffers(std::vector<uint8_t>* payload) {
    rtc::CritScope lock(&crit_sect_);
    last_payload_vec_.swap(*payload);
  }

 private:
  int num_calls_ RTC_GUARDED_BY(crit_sect_);
  FrameType last_frame_type_ RTC_GUARDED_BY(crit_sect_);
  int last_payload_type_ RTC_GUARDED_BY(crit_sect_);
  uint32_t last_timestamp_ RTC_GUARDED_BY(crit_sect_);
  std::vector<uint8_t> last_payload_vec_ RTC_GUARDED_BY(crit_sect_);
  rtc::CriticalSection crit_sect_;
};

class AudioCodingModuleTestOldApi : public ::testing::Test {
 protected:
  AudioCodingModuleTestOldApi()
      : rtp_utility_(new RtpUtility(kFrameSizeSamples, kPayloadType)),
        clock_(Clock::GetRealTimeClock()) {}

  ~AudioCodingModuleTestOldApi() {}

  void TearDown() {}

  void SetUp() {
    acm_.reset(AudioCodingModule::Create([this] {
      AudioCodingModule::Config config;
      config.clock = clock_;
      config.decoder_factory = CreateBuiltinAudioDecoderFactory();
      return config;
    }()));

    rtp_utility_->Populate(&rtp_header_);

    input_frame_.sample_rate_hz_ = kSampleRateHz;
    input_frame_.num_channels_ = 1;
    input_frame_.samples_per_channel_ = kSampleRateHz * 10 / 1000;  // 10 ms.
    static_assert(kSampleRateHz * 10 / 1000 <= AudioFrame::kMaxDataSizeSamples,
                  "audio frame too small");
    input_frame_.Mute();

    ASSERT_EQ(0, acm_->RegisterTransportCallback(&packet_cb_));

    SetUpL16Codec();
  }

  // Set up L16 codec.
  virtual void SetUpL16Codec() {
    audio_format_ = SdpAudioFormat("L16", kSampleRateHz, 1);
    ASSERT_EQ(0, AudioCodingModule::Codec("L16", &codec_, kSampleRateHz, 1));
    codec_.pltype = kPayloadType;
  }

  virtual void RegisterCodec() {
    EXPECT_EQ(true, acm_->RegisterReceiveCodec(kPayloadType, *audio_format_));
    acm_->SetEncoder(CreateBuiltinAudioEncoderFactory()->MakeAudioEncoder(
        kPayloadType, *audio_format_, absl::nullopt));
  }

  virtual void InsertPacketAndPullAudio() {
    InsertPacket();
    PullAudio();
  }

  virtual void InsertPacket() {
    const uint8_t kPayload[kPayloadSizeBytes] = {0};
    ASSERT_EQ(0,
              acm_->IncomingPacket(kPayload, kPayloadSizeBytes, rtp_header_));
    rtp_utility_->Forward(&rtp_header_);
  }

  virtual void PullAudio() {
    AudioFrame audio_frame;
    bool muted;
    ASSERT_EQ(0, acm_->PlayoutData10Ms(-1, &audio_frame, &muted));
    ASSERT_FALSE(muted);
  }

  virtual void InsertAudio() {
    ASSERT_GE(acm_->Add10MsData(input_frame_), 0);
    input_frame_.timestamp_ += kNumSamples10ms;
  }

  virtual void VerifyEncoding() {
    int last_length = packet_cb_.last_payload_len_bytes();
    EXPECT_TRUE(last_length == 2 * codec_.pacsize || last_length == 0)
        << "Last encoded packet was " << last_length << " bytes.";
  }

  virtual void InsertAudioAndVerifyEncoding() {
    InsertAudio();
    VerifyEncoding();
  }

  std::unique_ptr<RtpUtility> rtp_utility_;
  std::unique_ptr<AudioCodingModule> acm_;
  PacketizationCallbackStubOldApi packet_cb_;
  WebRtcRTPHeader rtp_header_;
  AudioFrame input_frame_;

  // These two have to be kept in sync for now. In the future, we'll be able to
  // eliminate the CodecInst and keep only the SdpAudioFormat.
  absl::optional<SdpAudioFormat> audio_format_;
  CodecInst codec_;

  Clock* clock_;
};

// Check if the statistics are initialized correctly. Before any call to ACM
// all fields have to be zero.
#if defined(WEBRTC_ANDROID)
#define MAYBE_InitializedToZero DISABLED_InitializedToZero
#else
#define MAYBE_InitializedToZero InitializedToZero
#endif
TEST_F(AudioCodingModuleTestOldApi, MAYBE_InitializedToZero) {
  RegisterCodec();
  AudioDecodingCallStats stats;
  acm_->GetDecodingCallStatistics(&stats);
  EXPECT_EQ(0, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(0, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);
  EXPECT_EQ(0, stats.decoded_muted_output);
}

// Insert some packets and pull audio. Check statistics are valid. Then,
// simulate packet loss and check if PLC and PLC-to-CNG statistics are
// correctly updated.
#if defined(WEBRTC_ANDROID)
#define MAYBE_NetEqCalls DISABLED_NetEqCalls
#else
#define MAYBE_NetEqCalls NetEqCalls
#endif
TEST_F(AudioCodingModuleTestOldApi, MAYBE_NetEqCalls) {
  RegisterCodec();
  AudioDecodingCallStats stats;
  const int kNumNormalCalls = 10;

  for (int num_calls = 0; num_calls < kNumNormalCalls; ++num_calls) {
    InsertPacketAndPullAudio();
  }
  acm_->GetDecodingCallStatistics(&stats);
  EXPECT_EQ(kNumNormalCalls, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);
  EXPECT_EQ(0, stats.decoded_muted_output);

  const int kNumPlc = 3;
  const int kNumPlcCng = 5;

  // Simulate packet-loss. NetEq first performs PLC then PLC fades to CNG.
  for (int n = 0; n < kNumPlc + kNumPlcCng; ++n) {
    PullAudio();
  }
  acm_->GetDecodingCallStatistics(&stats);
  EXPECT_EQ(kNumNormalCalls + kNumPlc + kNumPlcCng, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(kNumPlc, stats.decoded_plc);
  EXPECT_EQ(kNumPlcCng, stats.decoded_plc_cng);
  EXPECT_EQ(0, stats.decoded_muted_output);
  // TODO(henrik.lundin) Add a test with muted state enabled.
}

TEST_F(AudioCodingModuleTestOldApi, VerifyOutputFrame) {
  AudioFrame audio_frame;
  const int kSampleRateHz = 32000;
  bool muted;
  EXPECT_EQ(0, acm_->PlayoutData10Ms(kSampleRateHz, &audio_frame, &muted));
  ASSERT_FALSE(muted);
  EXPECT_EQ(0u, audio_frame.timestamp_);
  EXPECT_GT(audio_frame.num_channels_, 0u);
  EXPECT_EQ(static_cast<size_t>(kSampleRateHz / 100),
            audio_frame.samples_per_channel_);
  EXPECT_EQ(kSampleRateHz, audio_frame.sample_rate_hz_);
}

// The below test is temporarily disabled on Windows due to problems
// with clang debug builds.
// TODO(tommi): Re-enable when we've figured out what the problem is.
// http://crbug.com/615050
#if !defined(WEBRTC_WIN) && defined(__clang__) && RTC_DCHECK_IS_ON && \
    GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST_F(AudioCodingModuleTestOldApi, FailOnZeroDesiredFrequency) {
  AudioFrame audio_frame;
  bool muted;
  EXPECT_DEATH(acm_->PlayoutData10Ms(0, &audio_frame, &muted),
               "dst_sample_rate_hz");
}
#endif

// Checks that the transport callback is invoked once for each speech packet.
// Also checks that the frame type is kAudioFrameSpeech.
TEST_F(AudioCodingModuleTestOldApi, TransportCallbackIsInvokedForEachPacket) {
  const int k10MsBlocksPerPacket = 3;
  codec_.pacsize = k10MsBlocksPerPacket * kSampleRateHz / 100;
  audio_format_->parameters["ptime"] = "30";
  RegisterCodec();
  const int kLoops = 10;
  for (int i = 0; i < kLoops; ++i) {
    EXPECT_EQ(i / k10MsBlocksPerPacket, packet_cb_.num_calls());
    if (packet_cb_.num_calls() > 0)
      EXPECT_EQ(kAudioFrameSpeech, packet_cb_.last_frame_type());
    InsertAudioAndVerifyEncoding();
  }
  EXPECT_EQ(kLoops / k10MsBlocksPerPacket, packet_cb_.num_calls());
  EXPECT_EQ(kAudioFrameSpeech, packet_cb_.last_frame_type());
}

#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
// Verifies that the RTP timestamp series is not reset when the codec is
// changed.
TEST_F(AudioCodingModuleTestOldApi, TimestampSeriesContinuesWhenCodecChanges) {
  RegisterCodec();  // This registers the default codec.
  uint32_t expected_ts = input_frame_.timestamp_;
  int blocks_per_packet = codec_.pacsize / (kSampleRateHz / 100);
  // Encode 5 packets of the first codec type.
  const int kNumPackets1 = 5;
  for (int j = 0; j < kNumPackets1; ++j) {
    for (int i = 0; i < blocks_per_packet; ++i) {
      EXPECT_EQ(j, packet_cb_.num_calls());
      InsertAudio();
    }
    EXPECT_EQ(j + 1, packet_cb_.num_calls());
    EXPECT_EQ(expected_ts, packet_cb_.last_timestamp());
    expected_ts += codec_.pacsize;
  }

  // Change codec.
  ASSERT_EQ(0, AudioCodingModule::Codec("ISAC", &codec_, kSampleRateHz, 1));
  audio_format_ = SdpAudioFormat("ISAC", kSampleRateHz, 1);
  RegisterCodec();
  blocks_per_packet = codec_.pacsize / (kSampleRateHz / 100);
  // Encode another 5 packets.
  const int kNumPackets2 = 5;
  for (int j = 0; j < kNumPackets2; ++j) {
    for (int i = 0; i < blocks_per_packet; ++i) {
      EXPECT_EQ(kNumPackets1 + j, packet_cb_.num_calls());
      InsertAudio();
    }
    EXPECT_EQ(kNumPackets1 + j + 1, packet_cb_.num_calls());
    EXPECT_EQ(expected_ts, packet_cb_.last_timestamp());
    expected_ts += codec_.pacsize;
  }
}
#endif

// Introduce this class to set different expectations on the number of encoded
// bytes. This class expects all encoded packets to be 9 bytes (matching one
// CNG SID frame) or 0 bytes. This test depends on |input_frame_| containing
// (near-)zero values. It also introduces a way to register comfort noise with
// a custom payload type.
class AudioCodingModuleTestWithComfortNoiseOldApi
    : public AudioCodingModuleTestOldApi {
 protected:
  void RegisterCngCodec(int rtp_payload_type) {
    EXPECT_EQ(true,
              acm_->RegisterReceiveCodec(
                  rtp_payload_type, SdpAudioFormat("cn", kSampleRateHz, 1)));
    acm_->ModifyEncoder([&](std::unique_ptr<AudioEncoder>* enc) {
      AudioEncoderCngConfig config;
      config.speech_encoder = std::move(*enc);
      config.num_channels = 1;
      config.payload_type = rtp_payload_type;
      config.vad_mode = Vad::kVadNormal;
      *enc = CreateComfortNoiseEncoder(std::move(config));
    });
  }

  void VerifyEncoding() override {
    int last_length = packet_cb_.last_payload_len_bytes();
    EXPECT_TRUE(last_length == 9 || last_length == 0)
        << "Last encoded packet was " << last_length << " bytes.";
  }

  void DoTest(int blocks_per_packet, int cng_pt) {
    const int kLoops = 40;
    // This array defines the expected frame types, and when they should arrive.
    // We expect a frame to arrive each time the speech encoder would have
    // produced a packet, and once every 100 ms the frame should be non-empty,
    // that is contain comfort noise.
    const struct {
      int ix;
      FrameType type;
    } expectation[] = {
        {2, kAudioFrameCN},  {5, kEmptyFrame},    {8, kEmptyFrame},
        {11, kAudioFrameCN}, {14, kEmptyFrame},   {17, kEmptyFrame},
        {20, kAudioFrameCN}, {23, kEmptyFrame},   {26, kEmptyFrame},
        {29, kEmptyFrame},   {32, kAudioFrameCN}, {35, kEmptyFrame},
        {38, kEmptyFrame}};
    for (int i = 0; i < kLoops; ++i) {
      int num_calls_before = packet_cb_.num_calls();
      EXPECT_EQ(i / blocks_per_packet, num_calls_before);
      InsertAudioAndVerifyEncoding();
      int num_calls = packet_cb_.num_calls();
      if (num_calls == num_calls_before + 1) {
        EXPECT_EQ(expectation[num_calls - 1].ix, i);
        EXPECT_EQ(expectation[num_calls - 1].type, packet_cb_.last_frame_type())
            << "Wrong frame type for lap " << i;
        EXPECT_EQ(cng_pt, packet_cb_.last_payload_type());
      } else {
        EXPECT_EQ(num_calls, num_calls_before);
      }
    }
  }
};

// Checks that the transport callback is invoked once per frame period of the
// underlying speech encoder, even when comfort noise is produced.
// Also checks that the frame type is kAudioFrameCN or kEmptyFrame.
TEST_F(AudioCodingModuleTestWithComfortNoiseOldApi,
       TransportCallbackTestForComfortNoiseRegisterCngLast) {
  const int k10MsBlocksPerPacket = 3;
  codec_.pacsize = k10MsBlocksPerPacket * kSampleRateHz / 100;
  audio_format_->parameters["ptime"] = "30";
  RegisterCodec();
  const int kCngPayloadType = 105;
  RegisterCngCodec(kCngPayloadType);
  DoTest(k10MsBlocksPerPacket, kCngPayloadType);
}

// A multi-threaded test for ACM. This base class is using the PCM16b 16 kHz
// codec, while the derive class AcmIsacMtTest is using iSAC.
class AudioCodingModuleMtTestOldApi : public AudioCodingModuleTestOldApi {
 protected:
  static const int kNumPackets = 500;
  static const int kNumPullCalls = 500;

  AudioCodingModuleMtTestOldApi()
      : AudioCodingModuleTestOldApi(),
        send_thread_(CbSendThread, this, "send"),
        insert_packet_thread_(CbInsertPacketThread, this, "insert_packet"),
        pull_audio_thread_(CbPullAudioThread, this, "pull_audio"),
        send_count_(0),
        insert_packet_count_(0),
        pull_audio_count_(0),
        next_insert_packet_time_ms_(0),
        fake_clock_(new SimulatedClock(0)) {
    clock_ = fake_clock_.get();
  }

  void SetUp() {
    AudioCodingModuleTestOldApi::SetUp();
    RegisterCodec();  // Must be called before the threads start below.
    StartThreads();
  }

  void StartThreads() {
    send_thread_.Start();
    send_thread_.SetPriority(rtc::kRealtimePriority);
    insert_packet_thread_.Start();
    insert_packet_thread_.SetPriority(rtc::kRealtimePriority);
    pull_audio_thread_.Start();
    pull_audio_thread_.SetPriority(rtc::kRealtimePriority);
  }

  void TearDown() {
    AudioCodingModuleTestOldApi::TearDown();
    pull_audio_thread_.Stop();
    send_thread_.Stop();
    insert_packet_thread_.Stop();
  }

  bool RunTest() {
    return test_complete_.Wait(10 * 60 * 1000);  // 10 minutes' timeout.
  }

  virtual bool TestDone() {
    if (packet_cb_.num_calls() > kNumPackets) {
      rtc::CritScope lock(&crit_sect_);
      if (pull_audio_count_ > kNumPullCalls) {
        // Both conditions for completion are met. End the test.
        return true;
      }
    }
    return false;
  }

  static bool CbSendThread(void* context) {
    return reinterpret_cast<AudioCodingModuleMtTestOldApi*>(context)
        ->CbSendImpl();
  }

  // The send thread doesn't have to care about the current simulated time,
  // since only the AcmReceiver is using the clock.
  bool CbSendImpl() {
    SleepMs(1);
    if (HasFatalFailure()) {
      // End the test early if a fatal failure (ASSERT_*) has occurred.
      test_complete_.Set();
    }
    ++send_count_;
    InsertAudioAndVerifyEncoding();
    if (TestDone()) {
      test_complete_.Set();
    }
    return true;
  }

  static bool CbInsertPacketThread(void* context) {
    return reinterpret_cast<AudioCodingModuleMtTestOldApi*>(context)
        ->CbInsertPacketImpl();
  }

  bool CbInsertPacketImpl() {
    SleepMs(1);
    {
      rtc::CritScope lock(&crit_sect_);
      if (clock_->TimeInMilliseconds() < next_insert_packet_time_ms_) {
        return true;
      }
      next_insert_packet_time_ms_ += 10;
    }
    // Now we're not holding the crit sect when calling ACM.
    ++insert_packet_count_;
    InsertPacket();
    return true;
  }

  static bool CbPullAudioThread(void* context) {
    return reinterpret_cast<AudioCodingModuleMtTestOldApi*>(context)
        ->CbPullAudioImpl();
  }

  bool CbPullAudioImpl() {
    SleepMs(1);
    {
      rtc::CritScope lock(&crit_sect_);
      // Don't let the insert thread fall behind.
      if (next_insert_packet_time_ms_ < clock_->TimeInMilliseconds()) {
        return true;
      }
      ++pull_audio_count_;
    }
    // Now we're not holding the crit sect when calling ACM.
    PullAudio();
    fake_clock_->AdvanceTimeMilliseconds(10);
    return true;
  }

  rtc::PlatformThread send_thread_;
  rtc::PlatformThread insert_packet_thread_;
  rtc::PlatformThread pull_audio_thread_;
  rtc::Event test_complete_;
  int send_count_;
  int insert_packet_count_;
  int pull_audio_count_ RTC_GUARDED_BY(crit_sect_);
  rtc::CriticalSection crit_sect_;
  int64_t next_insert_packet_time_ms_ RTC_GUARDED_BY(crit_sect_);
  std::unique_ptr<SimulatedClock> fake_clock_;
};

#if defined(WEBRTC_IOS)
#define MAYBE_DoTest DISABLED_DoTest
#else
#define MAYBE_DoTest DoTest
#endif
TEST_F(AudioCodingModuleMtTestOldApi, MAYBE_DoTest) {
  EXPECT_TRUE(RunTest());
}

// This is a multi-threaded ACM test using iSAC. The test encodes audio
// from a PCM file. The most recent encoded frame is used as input to the
// receiving part. Depending on timing, it may happen that the same RTP packet
// is inserted into the receiver multiple times, but this is a valid use-case,
// and simplifies the test code a lot.
class AcmIsacMtTestOldApi : public AudioCodingModuleMtTestOldApi {
 protected:
  static const int kNumPackets = 500;
  static const int kNumPullCalls = 500;

  AcmIsacMtTestOldApi()
      : AudioCodingModuleMtTestOldApi(), last_packet_number_(0) {}

  ~AcmIsacMtTestOldApi() {}

  void SetUp() override {
    AudioCodingModuleTestOldApi::SetUp();
    RegisterCodec();  // Must be called before the threads start below.

    // Set up input audio source to read from specified file, loop after 5
    // seconds, and deliver blocks of 10 ms.
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/speech_mono_16kHz", "pcm");
    audio_loop_.Init(input_file_name, 5 * kSampleRateHz, kNumSamples10ms);

    // Generate one packet to have something to insert.
    int loop_counter = 0;
    while (packet_cb_.last_payload_len_bytes() == 0) {
      InsertAudio();
      ASSERT_LT(loop_counter++, 10);
    }
    // Set |last_packet_number_| to one less that |num_calls| so that the packet
    // will be fetched in the next InsertPacket() call.
    last_packet_number_ = packet_cb_.num_calls() - 1;

    StartThreads();
  }

  void RegisterCodec() override {
    static_assert(kSampleRateHz == 16000, "test designed for iSAC 16 kHz");
    audio_format_ = SdpAudioFormat("isac", kSampleRateHz, 1);
    AudioCodingModule::Codec("ISAC", &codec_, kSampleRateHz, 1);
    codec_.pltype = kPayloadType;

    // Register iSAC codec in ACM, effectively unregistering the PCM16B codec
    // registered in AudioCodingModuleTestOldApi::SetUp();
    EXPECT_EQ(true, acm_->RegisterReceiveCodec(kPayloadType, *audio_format_));
    acm_->SetEncoder(CreateBuiltinAudioEncoderFactory()->MakeAudioEncoder(
        kPayloadType, *audio_format_, absl::nullopt));
  }

  void InsertPacket() override {
    int num_calls = packet_cb_.num_calls();  // Store locally for thread safety.
    if (num_calls > last_packet_number_) {
      // Get the new payload out from the callback handler.
      // Note that since we swap buffers here instead of directly inserting
      // a pointer to the data in |packet_cb_|, we avoid locking the callback
      // for the duration of the IncomingPacket() call.
      packet_cb_.SwapBuffers(&last_payload_vec_);
      ASSERT_GT(last_payload_vec_.size(), 0u);
      rtp_utility_->Forward(&rtp_header_);
      last_packet_number_ = num_calls;
    }
    ASSERT_GT(last_payload_vec_.size(), 0u);
    ASSERT_EQ(0, acm_->IncomingPacket(&last_payload_vec_[0],
                                      last_payload_vec_.size(), rtp_header_));
  }

  void InsertAudio() override {
    // TODO(kwiberg): Use std::copy here. Might be complications because AFAICS
    // this call confuses the number of samples with the number of bytes, and
    // ends up copying only half of what it should.
    memcpy(input_frame_.mutable_data(), audio_loop_.GetNextBlock().data(),
           kNumSamples10ms);
    AudioCodingModuleTestOldApi::InsertAudio();
  }

  // Override the verification function with no-op, since iSAC produces variable
  // payload sizes.
  void VerifyEncoding() override {}

  // This method is the same as AudioCodingModuleMtTestOldApi::TestDone(), but
  // here it is using the constants defined in this class (i.e., shorter test
  // run).
  bool TestDone() override {
    if (packet_cb_.num_calls() > kNumPackets) {
      rtc::CritScope lock(&crit_sect_);
      if (pull_audio_count_ > kNumPullCalls) {
        // Both conditions for completion are met. End the test.
        return true;
      }
    }
    return false;
  }

  int last_packet_number_;
  std::vector<uint8_t> last_payload_vec_;
  test::AudioLoop audio_loop_;
};

#if defined(WEBRTC_IOS)
#define MAYBE_DoTest DISABLED_DoTest
#else
#define MAYBE_DoTest DoTest
#endif
#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
TEST_F(AcmIsacMtTestOldApi, MAYBE_DoTest) {
  EXPECT_TRUE(RunTest());
}
#endif

class AcmReRegisterIsacMtTestOldApi : public AudioCodingModuleTestOldApi {
 protected:
  static const int kRegisterAfterNumPackets = 5;
  static const int kNumPackets = 10;
  static const int kPacketSizeMs = 30;
  static const int kPacketSizeSamples = kPacketSizeMs * 16;

  AcmReRegisterIsacMtTestOldApi()
      : AudioCodingModuleTestOldApi(),
        receive_thread_(CbReceiveThread, this, "receive"),
        codec_registration_thread_(CbCodecRegistrationThread,
                                   this,
                                   "codec_registration"),
        codec_registered_(false),
        receive_packet_count_(0),
        next_insert_packet_time_ms_(0),
        fake_clock_(new SimulatedClock(0)) {
    AudioEncoderIsacFloatImpl::Config config;
    config.payload_type = kPayloadType;
    isac_encoder_.reset(new AudioEncoderIsacFloatImpl(config));
    clock_ = fake_clock_.get();
  }

  void SetUp() override {
    AudioCodingModuleTestOldApi::SetUp();
    // Set up input audio source to read from specified file, loop after 5
    // seconds, and deliver blocks of 10 ms.
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/speech_mono_16kHz", "pcm");
    audio_loop_.Init(input_file_name, 5 * kSampleRateHz, kNumSamples10ms);
    RegisterCodec();  // Must be called before the threads start below.
    StartThreads();
  }

  void RegisterCodec() override {
    static_assert(kSampleRateHz == 16000, "test designed for iSAC 16 kHz");
    AudioCodingModule::Codec("ISAC", &codec_, kSampleRateHz, 1);
    codec_.pltype = kPayloadType;

    // Register iSAC codec in ACM, effectively unregistering the PCM16B codec
    // registered in AudioCodingModuleTestOldApi::SetUp();
    // Only register the decoder for now. The encoder is registered later.
    ASSERT_EQ(true, acm_->RegisterReceiveCodec(codec_.pltype,
                                               CodecInstToSdp(codec_)));
  }

  void StartThreads() {
    receive_thread_.Start();
    receive_thread_.SetPriority(rtc::kRealtimePriority);
    codec_registration_thread_.Start();
    codec_registration_thread_.SetPriority(rtc::kRealtimePriority);
  }

  void TearDown() override {
    AudioCodingModuleTestOldApi::TearDown();
    receive_thread_.Stop();
    codec_registration_thread_.Stop();
  }

  bool RunTest() {
    return test_complete_.Wait(10 * 60 * 1000);  // 10 minutes' timeout.
  }

  static bool CbReceiveThread(void* context) {
    return reinterpret_cast<AcmReRegisterIsacMtTestOldApi*>(context)
        ->CbReceiveImpl();
  }

  bool CbReceiveImpl() {
    SleepMs(1);
    rtc::Buffer encoded;
    AudioEncoder::EncodedInfo info;
    {
      rtc::CritScope lock(&crit_sect_);
      if (clock_->TimeInMilliseconds() < next_insert_packet_time_ms_) {
        return true;
      }
      next_insert_packet_time_ms_ += kPacketSizeMs;
      ++receive_packet_count_;

      // Encode new frame.
      uint32_t input_timestamp = rtp_header_.header.timestamp;
      while (info.encoded_bytes == 0) {
        info = isac_encoder_->Encode(input_timestamp,
                                     audio_loop_.GetNextBlock(), &encoded);
        input_timestamp += 160;  // 10 ms at 16 kHz.
      }
      EXPECT_EQ(rtp_header_.header.timestamp + kPacketSizeSamples,
                input_timestamp);
      EXPECT_EQ(rtp_header_.header.timestamp, info.encoded_timestamp);
      EXPECT_EQ(rtp_header_.header.payloadType, info.payload_type);
    }
    // Now we're not holding the crit sect when calling ACM.

    // Insert into ACM.
    EXPECT_EQ(0, acm_->IncomingPacket(encoded.data(), info.encoded_bytes,
                                      rtp_header_));

    // Pull audio.
    for (int i = 0; i < rtc::CheckedDivExact(kPacketSizeMs, 10); ++i) {
      AudioFrame audio_frame;
      bool muted;
      EXPECT_EQ(0, acm_->PlayoutData10Ms(-1 /* default output frequency */,
                                         &audio_frame, &muted));
      if (muted) {
        ADD_FAILURE();
        return false;
      }
      fake_clock_->AdvanceTimeMilliseconds(10);
    }
    rtp_utility_->Forward(&rtp_header_);
    return true;
  }

  static bool CbCodecRegistrationThread(void* context) {
    return reinterpret_cast<AcmReRegisterIsacMtTestOldApi*>(context)
        ->CbCodecRegistrationImpl();
  }

  bool CbCodecRegistrationImpl() {
    SleepMs(1);
    if (HasFatalFailure()) {
      // End the test early if a fatal failure (ASSERT_*) has occurred.
      test_complete_.Set();
    }
    rtc::CritScope lock(&crit_sect_);
    if (!codec_registered_ &&
        receive_packet_count_ > kRegisterAfterNumPackets) {
      // Register the iSAC encoder.
      acm_->SetEncoder(CreateBuiltinAudioEncoderFactory()->MakeAudioEncoder(
          kPayloadType, *audio_format_, absl::nullopt));
      codec_registered_ = true;
    }
    if (codec_registered_ && receive_packet_count_ > kNumPackets) {
      test_complete_.Set();
    }
    return true;
  }

  rtc::PlatformThread receive_thread_;
  rtc::PlatformThread codec_registration_thread_;
  rtc::Event test_complete_;
  rtc::CriticalSection crit_sect_;
  bool codec_registered_ RTC_GUARDED_BY(crit_sect_);
  int receive_packet_count_ RTC_GUARDED_BY(crit_sect_);
  int64_t next_insert_packet_time_ms_ RTC_GUARDED_BY(crit_sect_);
  std::unique_ptr<AudioEncoderIsacFloatImpl> isac_encoder_;
  std::unique_ptr<SimulatedClock> fake_clock_;
  test::AudioLoop audio_loop_;
};

#if defined(WEBRTC_IOS)
#define MAYBE_DoTest DISABLED_DoTest
#else
#define MAYBE_DoTest DoTest
#endif
#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
TEST_F(AcmReRegisterIsacMtTestOldApi, MAYBE_DoTest) {
  EXPECT_TRUE(RunTest());
}
#endif

// Disabling all of these tests on iOS until file support has been added.
// See https://code.google.com/p/webrtc/issues/detail?id=4752 for details.
#if !defined(WEBRTC_IOS)

class AcmReceiverBitExactnessOldApi : public ::testing::Test {
 public:
  static std::string PlatformChecksum(std::string others,
                                      std::string win64,
                                      std::string android_arm32,
                                      std::string android_arm64,
                                      std::string android_arm64_clang) {
#if defined(_WIN32) && defined(WEBRTC_ARCH_64_BITS)
    return win64;
#elif defined(WEBRTC_ANDROID) && defined(WEBRTC_ARCH_ARM)
    return android_arm32;
#elif defined(WEBRTC_ANDROID) && defined(WEBRTC_ARCH_ARM64)
#if defined(__clang__)
    // Android ARM64 with Clang compiler
    return android_arm64_clang;
#else
    // Android ARM64 with non-Clang compiler
    return android_arm64;
#endif  // __clang__
#else
    return others;
#endif
  }

 protected:
  struct ExternalDecoder {
    int rtp_payload_type;
    AudioDecoder* external_decoder;
    int sample_rate_hz;
    int num_channels;
    std::string name;
  };

  void Run(int output_freq_hz, const std::string& checksum_ref) {
    Run(output_freq_hz, checksum_ref, CreateBuiltinAudioDecoderFactory(),
        [](AudioCodingModule*) {});
  }

  void Run(int output_freq_hz,
           const std::string& checksum_ref,
           rtc::scoped_refptr<AudioDecoderFactory> decoder_factory,
           rtc::FunctionView<void(AudioCodingModule*)> decoder_reg) {
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/neteq_universal_new", "rtp");
    std::unique_ptr<test::RtpFileSource> packet_source(
        test::RtpFileSource::Create(input_file_name));
#ifdef WEBRTC_ANDROID
    // Filter out iLBC and iSAC-swb since they are not supported on Android.
    packet_source->FilterOutPayloadType(102);  // iLBC.
    packet_source->FilterOutPayloadType(104);  // iSAC-swb.
#endif

    test::AudioChecksum checksum;
    const std::string output_file_name =
        webrtc::test::OutputPath() +
        ::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->test_case_name() +
        "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
        "_output.wav";
    test::OutputWavFile output_file(output_file_name, output_freq_hz);
    test::AudioSinkFork output(&checksum, &output_file);

    test::AcmReceiveTestOldApi test(
        packet_source.get(), &output, output_freq_hz,
        test::AcmReceiveTestOldApi::kArbitraryChannels,
        std::move(decoder_factory));
    ASSERT_NO_FATAL_FAILURE(test.RegisterNetEqTestCodecs());
    decoder_reg(test.get_acm());
    test.Run();

    std::string checksum_string = checksum.Finish();
    EXPECT_EQ(checksum_ref, checksum_string);

    // Delete the output file.
    remove(output_file_name.c_str());
  }
};

#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)) && \
    defined(WEBRTC_CODEC_ILBC)
TEST_F(AcmReceiverBitExactnessOldApi, 8kHzOutput) {
  Run(8000, PlatformChecksum("7294941b62293e143d6d6c84955923fd",
                             "f26b8c9aa8257c7185925fa5b102f46a",
                             "65e5ef5c3de9c2abf3c8d0989772e9fc",
                             "4598140b5e4f7ee66c5adad609e65a3e",
                             "04a1d3e735730b6d7dbd5df10f717d27"));
}

TEST_F(AcmReceiverBitExactnessOldApi, 16kHzOutput) {
  Run(16000, PlatformChecksum("de8143dd3cc23241f1e1d5cf14e04b8a",
                              "eada3f321120d8d5afffbe4170a55d2f",
                              "135d8c3c7b92aa13d45cad7c91068e3e",
                              "f2aad418af974a3b1694d5ae5cc2c3c7",
                              "728b69068332efade35b1a9c32029e1b"));
}

TEST_F(AcmReceiverBitExactnessOldApi, 32kHzOutput) {
  Run(32000, PlatformChecksum("521d336237bdcc9ab44050e9da8917fc",
                              "73d44a7bedb6dfa7c70cf997223d8c10",
                              "f3ee2f14b03fb8e98f526f82583f84d9",
                              "100869c8dcde51346c2073e52a272d98",
                              "5f338b4bc38707d0a14d75a357e1546e"));
}

TEST_F(AcmReceiverBitExactnessOldApi, 48kHzOutput) {
  Run(48000, PlatformChecksum("5955e31373828969de7fb308fb58a84e",
                              "83c0eca235b1a806426ff6ca8655cdf7",
                              "1126a8c03d1ebc6aa7348b9c541e2082",
                              "bd44bf97e7899186532f91235cef444d",
                              "9d092dbc96e7ef6870b78c1056e87315"));
}

TEST_F(AcmReceiverBitExactnessOldApi, 48kHzOutputExternalDecoder) {
  class ADFactory : public AudioDecoderFactory {
   public:
    ADFactory()
        : mock_decoder_(new MockAudioDecoder()),
          pcmu_decoder_(1),
          decode_forwarder_(&pcmu_decoder_),
          fact_(CreateBuiltinAudioDecoderFactory()) {
      // Set expectations on the mock decoder and also delegate the calls to
      // the real decoder.
      EXPECT_CALL(*mock_decoder_, IncomingPacket(_, _, _, _, _))
          .Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&pcmu_decoder_, &AudioDecoderPcmU::IncomingPacket));
      EXPECT_CALL(*mock_decoder_, SampleRateHz())
          .Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&pcmu_decoder_, &AudioDecoderPcmU::SampleRateHz));
      EXPECT_CALL(*mock_decoder_, Channels())
          .Times(AtLeast(1))
          .WillRepeatedly(Invoke(&pcmu_decoder_, &AudioDecoderPcmU::Channels));
      EXPECT_CALL(*mock_decoder_, DecodeInternal(_, _, _, _, _))
          .Times(AtLeast(1))
          .WillRepeatedly(Invoke(&decode_forwarder_, &DecodeForwarder::Decode));
      EXPECT_CALL(*mock_decoder_, HasDecodePlc())
          .Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&pcmu_decoder_, &AudioDecoderPcmU::HasDecodePlc));
      EXPECT_CALL(*mock_decoder_, PacketDuration(_, _))
          .Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&pcmu_decoder_, &AudioDecoderPcmU::PacketDuration));
      EXPECT_CALL(*mock_decoder_, Die());
    }
    std::vector<AudioCodecSpec> GetSupportedDecoders() override {
      return fact_->GetSupportedDecoders();
    }
    bool IsSupportedDecoder(const SdpAudioFormat& format) override {
      return format.name == "MockPCMu" ? true
                                       : fact_->IsSupportedDecoder(format);
    }
    std::unique_ptr<AudioDecoder> MakeAudioDecoder(
        const SdpAudioFormat& format,
        absl::optional<AudioCodecPairId> codec_pair_id) override {
      return format.name == "MockPCMu"
                 ? std::move(mock_decoder_)
                 : fact_->MakeAudioDecoder(format, codec_pair_id);
    }

   private:
    // Class intended to forward a call from a mock DecodeInternal to Decode on
    // the real decoder's Decode. DecodeInternal for the real decoder isn't
    // public.
    class DecodeForwarder {
     public:
      explicit DecodeForwarder(AudioDecoder* decoder) : decoder_(decoder) {}
      int Decode(const uint8_t* encoded,
                 size_t encoded_len,
                 int sample_rate_hz,
                 int16_t* decoded,
                 AudioDecoder::SpeechType* speech_type) {
        return decoder_->Decode(encoded, encoded_len, sample_rate_hz,
                                decoder_->PacketDuration(encoded, encoded_len) *
                                    decoder_->Channels() * sizeof(int16_t),
                                decoded, speech_type);
      }

     private:
      AudioDecoder* const decoder_;
    };

    std::unique_ptr<MockAudioDecoder> mock_decoder_;
    AudioDecoderPcmU pcmu_decoder_;
    DecodeForwarder decode_forwarder_;
    rtc::scoped_refptr<AudioDecoderFactory> fact_;  // Fallback factory.
  };

  rtc::scoped_refptr<rtc::RefCountedObject<ADFactory>> factory(
      new rtc::RefCountedObject<ADFactory>);
  Run(48000,
      PlatformChecksum("5955e31373828969de7fb308fb58a84e",
                       "83c0eca235b1a806426ff6ca8655cdf7",
                       "1126a8c03d1ebc6aa7348b9c541e2082",
                       "bd44bf97e7899186532f91235cef444d",
                       "9d092dbc96e7ef6870b78c1056e87315"),
      factory, [](AudioCodingModule* acm) {
        acm->RegisterReceiveCodec(0, {"MockPCMu", 8000, 1});
      });
}
#endif

// This test verifies bit exactness for the send-side of ACM. The test setup is
// a chain of three different test classes:
//
// test::AcmSendTest -> AcmSenderBitExactness -> test::AcmReceiveTest
//
// The receiver side is driving the test by requesting new packets from
// AcmSenderBitExactness::NextPacket(). This method, in turn, asks for the
// packet from test::AcmSendTest::NextPacket, which inserts audio from the
// input file until one packet is produced. (The input file loops indefinitely.)
// Before passing the packet to the receiver, this test class verifies the
// packet header and updates a payload checksum with the new payload. The
// decoded output from the receiver is also verified with a (separate) checksum.
class AcmSenderBitExactnessOldApi : public ::testing::Test,
                                    public test::PacketSource {
 protected:
  static const int kTestDurationMs = 1000;

  AcmSenderBitExactnessOldApi()
      : frame_size_rtp_timestamps_(0),
        packet_count_(0),
        payload_type_(0),
        last_sequence_number_(0),
        last_timestamp_(0),
        payload_checksum_(rtc::MessageDigestFactory::Create(rtc::DIGEST_MD5)) {}

  // Sets up the test::AcmSendTest object. Returns true on success, otherwise
  // false.
  bool SetUpSender() {
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    // Note that |audio_source_| will loop forever. The test duration is set
    // explicitly by |kTestDurationMs|.
    audio_source_.reset(new test::InputAudioFile(input_file_name));
    static const int kSourceRateHz = 32000;
    send_test_.reset(new test::AcmSendTestOldApi(
        audio_source_.get(), kSourceRateHz, kTestDurationMs));
    return send_test_.get() != NULL;
  }

  // Registers a send codec in the test::AcmSendTest object. Returns true on
  // success, false on failure.
  bool RegisterSendCodec(const char* payload_name,
                         int sampling_freq_hz,
                         int channels,
                         int payload_type,
                         int frame_size_samples,
                         int frame_size_rtp_timestamps) {
    payload_type_ = payload_type;
    frame_size_rtp_timestamps_ = frame_size_rtp_timestamps;
    return send_test_->RegisterCodec(payload_name, sampling_freq_hz, channels,
                                     payload_type, frame_size_samples);
  }

  void RegisterExternalSendCodec(
      std::unique_ptr<AudioEncoder> external_speech_encoder,
      int payload_type) {
    payload_type_ = payload_type;
    frame_size_rtp_timestamps_ = rtc::checked_cast<uint32_t>(
        external_speech_encoder->Num10MsFramesInNextPacket() *
        external_speech_encoder->RtpTimestampRateHz() / 100);
    send_test_->RegisterExternalCodec(std::move(external_speech_encoder));
  }

  // Runs the test. SetUpSender() and RegisterSendCodec() must have been called
  // before calling this method.
  void Run(const std::string& audio_checksum_ref,
           const std::string& payload_checksum_ref,
           int expected_packets,
           test::AcmReceiveTestOldApi::NumOutputChannels expected_channels) {
    // Set up the receiver used to decode the packets and verify the decoded
    // output.
    test::AudioChecksum audio_checksum;
    const std::string output_file_name =
        webrtc::test::OutputPath() +
        ::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->test_case_name() +
        "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
        "_output.wav";
    const int kOutputFreqHz = 8000;
    test::OutputWavFile output_file(output_file_name, kOutputFreqHz);
    // Have the output audio sent both to file and to the checksum calculator.
    test::AudioSinkFork output(&audio_checksum, &output_file);
    test::AcmReceiveTestOldApi receive_test(this, &output, kOutputFreqHz,
                                            expected_channels,
                                            CreateBuiltinAudioDecoderFactory());
    ASSERT_NO_FATAL_FAILURE(receive_test.RegisterDefaultCodecs());

    // This is where the actual test is executed.
    receive_test.Run();

    // Extract and verify the audio checksum.
    std::string checksum_string = audio_checksum.Finish();
    ExpectChecksumEq(audio_checksum_ref, checksum_string);

    // Extract and verify the payload checksum.
    rtc::Buffer checksum_result(payload_checksum_->Size());
    payload_checksum_->Finish(checksum_result.data(), checksum_result.size());
    checksum_string =
        rtc::hex_encode(checksum_result.data<char>(), checksum_result.size());
    ExpectChecksumEq(payload_checksum_ref, checksum_string);

    // Verify number of packets produced.
    EXPECT_EQ(expected_packets, packet_count_);

    // Delete the output file.
    remove(output_file_name.c_str());
  }

  // Helper: result must be one the "|"-separated checksums.
  void ExpectChecksumEq(std::string ref, std::string result) {
    if (ref.size() == result.size()) {
      // Only one checksum: clearer message.
      EXPECT_EQ(ref, result);
    } else {
      EXPECT_NE(ref.find(result), std::string::npos)
          << result << " must be one of these:\n"
          << ref;
    }
  }

  // Inherited from test::PacketSource.
  std::unique_ptr<test::Packet> NextPacket() override {
    auto packet = send_test_->NextPacket();
    if (!packet)
      return NULL;

    VerifyPacket(packet.get());
    // TODO(henrik.lundin) Save the packet to file as well.

    // Pass it on to the caller. The caller becomes the owner of |packet|.
    return packet;
  }

  // Verifies the packet.
  void VerifyPacket(const test::Packet* packet) {
    EXPECT_TRUE(packet->valid_header());
    // (We can check the header fields even if valid_header() is false.)
    EXPECT_EQ(payload_type_, packet->header().payloadType);
    if (packet_count_ > 0) {
      // This is not the first packet.
      uint16_t sequence_number_diff =
          packet->header().sequenceNumber - last_sequence_number_;
      EXPECT_EQ(1, sequence_number_diff);
      uint32_t timestamp_diff = packet->header().timestamp - last_timestamp_;
      EXPECT_EQ(frame_size_rtp_timestamps_, timestamp_diff);
    }
    ++packet_count_;
    last_sequence_number_ = packet->header().sequenceNumber;
    last_timestamp_ = packet->header().timestamp;
    // Update the checksum.
    payload_checksum_->Update(packet->payload(),
                              packet->payload_length_bytes());
  }

  void SetUpTest(const char* codec_name,
                 int codec_sample_rate_hz,
                 int channels,
                 int payload_type,
                 int codec_frame_size_samples,
                 int codec_frame_size_rtp_timestamps) {
    ASSERT_TRUE(SetUpSender());
    ASSERT_TRUE(RegisterSendCodec(codec_name, codec_sample_rate_hz, channels,
                                  payload_type, codec_frame_size_samples,
                                  codec_frame_size_rtp_timestamps));
  }

  void SetUpTestExternalEncoder(
      std::unique_ptr<AudioEncoder> external_speech_encoder,
      int payload_type) {
    ASSERT_TRUE(SetUpSender());
    RegisterExternalSendCodec(std::move(external_speech_encoder), payload_type);
  }

  std::unique_ptr<test::AcmSendTestOldApi> send_test_;
  std::unique_ptr<test::InputAudioFile> audio_source_;
  uint32_t frame_size_rtp_timestamps_;
  int packet_count_;
  uint8_t payload_type_;
  uint16_t last_sequence_number_;
  uint32_t last_timestamp_;
  std::unique_ptr<rtc::MessageDigest> payload_checksum_;
};

class AcmSenderBitExactnessNewApi : public AcmSenderBitExactnessOldApi {};

#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
TEST_F(AcmSenderBitExactnessOldApi, IsacWb30ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ISAC", 16000, 1, 103, 480, 480));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "2c9cb15d4ed55b5a0cadd04883bc73b0",
          "9336a9b993cbd8a751f0e8958e66c89c",
          "bd4682225f7c4ad5f2049f6769713ac2",
          "343f1f42be0607c61e6516aece424609",
          "2c9cb15d4ed55b5a0cadd04883bc73b0"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "3c79f16f34218271f3dca4e2b1dfe1bb",
          "d42cb5195463da26c8129bbfe73a22e6",
          "83de248aea9c3c2bd680b6952401b4ca",
          "3c79f16f34218271f3dca4e2b1dfe1bb",
          "3c79f16f34218271f3dca4e2b1dfe1bb"),
      33, test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, IsacWb60ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ISAC", 16000, 1, 103, 960, 960));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "1ad29139a04782a33daad8c2b9b35875",
          "14d63c5f08127d280e722e3191b73bdd",
          "edcf26694c289e3d9691faf79b74f09f",
          "ef75e900e6f375e3061163c53fd09a63",
          "1ad29139a04782a33daad8c2b9b35875"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "9e0a0ab743ad987b55b8e14802769c56",
          "ebe04a819d3a9d83a83a17f271e1139a",
          "97aeef98553b5a4b5a68f8b716e8eaf0",
          "9e0a0ab743ad987b55b8e14802769c56",
          "9e0a0ab743ad987b55b8e14802769c56"),
      16, test::AcmReceiveTestOldApi::kMonoOutput);
}
#endif

#if defined(WEBRTC_ANDROID)
#define MAYBE_IsacSwb30ms DISABLED_IsacSwb30ms
#else
#define MAYBE_IsacSwb30ms IsacSwb30ms
#endif
#if defined(WEBRTC_CODEC_ISAC)
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_IsacSwb30ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ISAC", 32000, 1, 104, 960, 960));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "5683b58da0fbf2063c7adc2e6bfb3fb8",
          "2b3c387d06f00b7b7aad4c9be56fb83d", "android_arm32_audio",
          "android_arm64_audio", "android_arm64_clang_audio"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "ce86106a93419aefb063097108ec94ab",
          "bcc2041e7744c7ebd9f701866856849c", "android_arm32_payload",
          "android_arm64_payload", "android_arm64_clang_payload"),
      33, test::AcmReceiveTestOldApi::kMonoOutput);
}
#endif

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_8000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 1, 107, 80, 80));
  Run("de4a98e1406f8b798d99cd0704e862e2", "c1edd36339ce0326cc4550041ad719a0",
      100, test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_16000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 16000, 1, 108, 160, 160));
  Run("ae646d7b68384a1269cc080dd4501916", "ad786526383178b08d80d6eee06e9bad",
      100, test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_32000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 32000, 1, 109, 320, 320));
  Run("7fe325e8fbaf755e3c5df0b11a4774fb", "5ef82ea885e922263606c6fdbc49f651",
      100, test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_stereo_8000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 2, 111, 80, 80));
  Run("fb263b74e7ac3de915474d77e4744ceb", "62ce5adb0d4965d0a52ec98ae7f98974",
      100, test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_stereo_16000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 16000, 2, 112, 160, 160));
  Run("d09e9239553649d7ac93e19d304281fd", "41ca8edac4b8c71cd54fd9f25ec14870",
      100, test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcm16_stereo_32000khz_10ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 32000, 2, 113, 320, 320));
  Run("5f025d4f390982cc26b3d92fe02e3044", "50e58502fb04421bf5b857dda4c96879",
      100, test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcmu_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMU", 8000, 1, 0, 160, 160));
  Run("81a9d4c0bb72e9becc43aef124c981e9", "8f9b8750bd80fe26b6cbf6659b89f0f9",
      50, test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcma_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMA", 8000, 1, 8, 160, 160));
  Run("39611f798969053925a49dc06d08de29", "6ad745e55aa48981bfc790d0eeef2dd1",
      50, test::AcmReceiveTestOldApi::kMonoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcmu_stereo_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMU", 8000, 2, 110, 160, 160));
  Run("437bec032fdc5cbaa0d5175430af7b18", "60b6f25e8d1e74cb679cfe756dd9bca5",
      50, test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessOldApi, Pcma_stereo_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("PCMA", 8000, 2, 118, 160, 160));
  Run("a5c6d83c5b7cedbeff734238220a4b0c", "92b282c83efd20e7eeef52ba40842cf7",
      50, test::AcmReceiveTestOldApi::kStereoOutput);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_Ilbc_30ms DISABLED_Ilbc_30ms
#else
#define MAYBE_Ilbc_30ms Ilbc_30ms
#endif
#if defined(WEBRTC_CODEC_ILBC)
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_Ilbc_30ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("ILBC", 8000, 1, 102, 240, 240));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "7b6ec10910debd9af08011d3ed5249f7",
          "7b6ec10910debd9af08011d3ed5249f7", "android_arm32_audio",
          "android_arm64_audio", "android_arm64_clang_audio"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "cfae2e9f6aba96e145f2bcdd5050ce78",
          "cfae2e9f6aba96e145f2bcdd5050ce78", "android_arm32_payload",
          "android_arm64_payload", "android_arm64_clang_payload"),
      33, test::AcmReceiveTestOldApi::kMonoOutput);
}
#endif

#if defined(WEBRTC_ANDROID)
#define MAYBE_G722_20ms DISABLED_G722_20ms
#else
#define MAYBE_G722_20ms G722_20ms
#endif
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_G722_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("G722", 16000, 1, 9, 320, 160));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "e99c89be49a46325d03c0d990c292d68",
          "e99c89be49a46325d03c0d990c292d68", "android_arm32_audio",
          "android_arm64_audio", "android_arm64_clang_audio"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "fc68a87e1380614e658087cb35d5ca10",
          "fc68a87e1380614e658087cb35d5ca10", "android_arm32_payload",
          "android_arm64_payload", "android_arm64_clang_payload"),
      50, test::AcmReceiveTestOldApi::kMonoOutput);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_G722_stereo_20ms DISABLED_G722_stereo_20ms
#else
#define MAYBE_G722_stereo_20ms G722_stereo_20ms
#endif
TEST_F(AcmSenderBitExactnessOldApi, MAYBE_G722_stereo_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("G722", 16000, 2, 119, 320, 160));
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "e280aed283e499d37091b481ca094807",
          "e280aed283e499d37091b481ca094807", "android_arm32_audio",
          "android_arm64_audio", "android_arm64_clang_audio"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          "66516152eeaa1e650ad94ff85f668dac",
          "66516152eeaa1e650ad94ff85f668dac", "android_arm32_payload",
          "android_arm64_payload", "android_arm64_clang_payload"),
      50, test::AcmReceiveTestOldApi::kStereoOutput);
}

namespace {
// Checksum depends on libopus being compiled with or without SSE.
const std::string audio_maybe_sse =
    "3e285b74510e62062fbd8142dacd16e9|"
    "fd5d57d6d766908e6a7211e2a5c7f78a";
const std::string payload_maybe_sse =
    "78cf8f03157358acdc69f6835caa0d9b|"
    "b693bd95c2ee2354f92340dd09e9da68";
// Common checksums.
const std::string audio_checksum =
    AcmReceiverBitExactnessOldApi::PlatformChecksum(
        audio_maybe_sse,
        audio_maybe_sse,
        "439e97ad1932c49923b5da029c17dd5e",
        "038ec90f5f3fc2320f3090f8ecef6bb7",
        "038ec90f5f3fc2320f3090f8ecef6bb7");
const std::string payload_checksum =
    AcmReceiverBitExactnessOldApi::PlatformChecksum(
        payload_maybe_sse,
        payload_maybe_sse,
        "ab88b1a049c36bdfeb7e8b057ef6982a",
        "27fef7b799393347ec3b5694369a1c36",
        "27fef7b799393347ec3b5694369a1c36");
};  // namespace

TEST_F(AcmSenderBitExactnessOldApi, Opus_stereo_20ms) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 2, 120, 960, 960));
  Run(audio_checksum, payload_checksum, 50,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessNewApi, MAYBE_OpusFromFormat_stereo_20ms) {
  const auto config = AudioEncoderOpus::SdpToConfig(
      SdpAudioFormat("opus", 48000, 2, {{"stereo", "1"}}));
  ASSERT_NO_FATAL_FAILURE(SetUpTestExternalEncoder(
      AudioEncoderOpus::MakeAudioEncoder(*config, 120), 120));
  Run(audio_checksum, payload_checksum, 50,
      test::AcmReceiveTestOldApi::kStereoOutput);
}

TEST_F(AcmSenderBitExactnessNewApi, OpusFromFormat_stereo_20ms_voip) {
  auto config = AudioEncoderOpus::SdpToConfig(
      SdpAudioFormat("opus", 48000, 2, {{"stereo", "1"}}));
  // If not set, default will be kAudio in case of stereo.
  config->application = AudioEncoderOpusConfig::ApplicationMode::kVoip;
  ASSERT_NO_FATAL_FAILURE(SetUpTestExternalEncoder(
      AudioEncoderOpus::MakeAudioEncoder(*config, 120), 120));
  // Checksum depends on libopus being compiled with or without SSE.
  const std::string audio_maybe_sse =
      "b0325df4e8104f04e03af23c0b75800e|"
      "3cd4e1bc2acd9440bb9e97af34080ffc";
  const std::string payload_maybe_sse =
      "4eab2259b6fe24c22dd242a113e0b3d9|"
      "4fc0af0aa06c26454af09832d3ec1b4e";
  Run(AcmReceiverBitExactnessOldApi::PlatformChecksum(
          audio_maybe_sse, audio_maybe_sse, "1c81121f5d9286a5a865d01dbab22ce8",
          "11d547f89142e9ef03f37d7ca7f32379",
          "11d547f89142e9ef03f37d7ca7f32379"),
      AcmReceiverBitExactnessOldApi::PlatformChecksum(
          payload_maybe_sse, payload_maybe_sse,
          "839ea60399447268ee0f0262a50b75fd",
          "1815fd5589cad0c6f6cf946c76b81aeb",
          "1815fd5589cad0c6f6cf946c76b81aeb"),
      50, test::AcmReceiveTestOldApi::kStereoOutput);
}

// This test is for verifying the SetBitRate function. The bitrate is changed at
// the beginning, and the number of generated bytes are checked.
class AcmSetBitRateTest : public ::testing::Test {
 protected:
  static const int kTestDurationMs = 1000;

  // Sets up the test::AcmSendTest object. Returns true on success, otherwise
  // false.
  bool SetUpSender() {
    const std::string input_file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    // Note that |audio_source_| will loop forever. The test duration is set
    // explicitly by |kTestDurationMs|.
    audio_source_.reset(new test::InputAudioFile(input_file_name));
    static const int kSourceRateHz = 32000;
    send_test_.reset(new test::AcmSendTestOldApi(
        audio_source_.get(), kSourceRateHz, kTestDurationMs));
    return send_test_.get();
  }

  // Registers a send codec in the test::AcmSendTest object. Returns true on
  // success, false on failure.
  virtual bool RegisterSendCodec(const char* payload_name,
                                 int sampling_freq_hz,
                                 int channels,
                                 int payload_type,
                                 int frame_size_samples,
                                 int frame_size_rtp_timestamps) {
    return send_test_->RegisterCodec(payload_name, sampling_freq_hz, channels,
                                     payload_type, frame_size_samples);
  }

  void RegisterExternalSendCodec(
      std::unique_ptr<AudioEncoder> external_speech_encoder,
      int payload_type) {
    send_test_->RegisterExternalCodec(std::move(external_speech_encoder));
  }

  void RunInner(int min_expected_total_bits, int max_expected_total_bits) {
    int nr_bytes = 0;
    while (std::unique_ptr<test::Packet> next_packet =
               send_test_->NextPacket()) {
      nr_bytes += rtc::checked_cast<int>(next_packet->payload_length_bytes());
    }
    EXPECT_LE(min_expected_total_bits, nr_bytes * 8);
    EXPECT_GE(max_expected_total_bits, nr_bytes * 8);
  }

  void SetUpTest(const char* codec_name,
                 int codec_sample_rate_hz,
                 int channels,
                 int payload_type,
                 int codec_frame_size_samples,
                 int codec_frame_size_rtp_timestamps) {
    ASSERT_TRUE(SetUpSender());
    ASSERT_TRUE(RegisterSendCodec(codec_name, codec_sample_rate_hz, channels,
                                  payload_type, codec_frame_size_samples,
                                  codec_frame_size_rtp_timestamps));
  }

  std::unique_ptr<test::AcmSendTestOldApi> send_test_;
  std::unique_ptr<test::InputAudioFile> audio_source_;
};

class AcmSetBitRateOldApi : public AcmSetBitRateTest {
 protected:
  // Runs the test. SetUpSender() must have been called and a codec must be set
  // up before calling this method.
  void Run(int target_bitrate_bps,
           int min_expected_total_bits,
           int max_expected_total_bits) {
    ASSERT_TRUE(send_test_->acm());
    send_test_->acm()->SetBitRate(target_bitrate_bps);
    RunInner(min_expected_total_bits, max_expected_total_bits);
  }
};

class AcmSetBitRateNewApi : public AcmSetBitRateTest {
 protected:
  // Runs the test. SetUpSender() must have been called and a codec must be set
  // up before calling this method.
  void Run(int min_expected_total_bits, int max_expected_total_bits) {
    RunInner(min_expected_total_bits, max_expected_total_bits);
  }
};

TEST_F(AcmSetBitRateOldApi, Opus_48khz_20ms_10kbps) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 1, 107, 960, 960));
  Run(10000, 8000, 12000);
}

TEST_F(AcmSetBitRateNewApi, OpusFromFormat_48khz_20ms_10kbps) {
  const auto config = AudioEncoderOpus::SdpToConfig(
      SdpAudioFormat("opus", 48000, 2, {{"maxaveragebitrate", "10000"}}));
  ASSERT_TRUE(SetUpSender());
  RegisterExternalSendCodec(AudioEncoderOpus::MakeAudioEncoder(*config, 107),
                            107);
  RunInner(8000, 12000);
}

TEST_F(AcmSetBitRateOldApi, Opus_48khz_20ms_50kbps) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 1, 107, 960, 960));
  Run(50000, 40000, 60000);
}

TEST_F(AcmSetBitRateNewApi, OpusFromFormat_48khz_20ms_50kbps) {
  const auto config = AudioEncoderOpus::SdpToConfig(
      SdpAudioFormat("opus", 48000, 2, {{"maxaveragebitrate", "50000"}}));
  ASSERT_TRUE(SetUpSender());
  RegisterExternalSendCodec(AudioEncoderOpus::MakeAudioEncoder(*config, 107),
                            107);
  RunInner(40000, 60000);
}

// The result on the Android platforms is inconsistent for this test case.
// On android_rel the result is different from android and android arm64 rel.
#if defined(WEBRTC_ANDROID)
#define MAYBE_Opus_48khz_20ms_100kbps DISABLED_Opus_48khz_20ms_100kbps
#define MAYBE_OpusFromFormat_48khz_20ms_100kbps \
  DISABLED_OpusFromFormat_48khz_20ms_100kbps
#else
#define MAYBE_Opus_48khz_20ms_100kbps Opus_48khz_20ms_100kbps
#define MAYBE_OpusFromFormat_48khz_20ms_100kbps \
  OpusFromFormat_48khz_20ms_100kbps
#endif
TEST_F(AcmSetBitRateOldApi, MAYBE_Opus_48khz_20ms_100kbps) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 1, 107, 960, 960));
  Run(100000, 80000, 120000);
}

TEST_F(AcmSetBitRateNewApi, MAYBE_OpusFromFormat_48khz_20ms_100kbps) {
  const auto config = AudioEncoderOpus::SdpToConfig(
      SdpAudioFormat("opus", 48000, 2, {{"maxaveragebitrate", "100000"}}));
  ASSERT_TRUE(SetUpSender());
  RegisterExternalSendCodec(AudioEncoderOpus::MakeAudioEncoder(*config, 107),
                            107);
  RunInner(80000, 120000);
}

// These next 2 tests ensure that the SetBitRate function has no effect on PCM
TEST_F(AcmSetBitRateOldApi, Pcm16_8khz_10ms_8kbps) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 1, 107, 80, 80));
  Run(8000, 128000, 128000);
}

TEST_F(AcmSetBitRateOldApi, Pcm16_8khz_10ms_32kbps) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 1, 107, 80, 80));
  Run(32000, 128000, 128000);
}

// This test is for verifying the SetBitRate function. The bitrate is changed
// in the middle, and the number of generated bytes are before and after the
// change are checked.
class AcmChangeBitRateOldApi : public AcmSetBitRateOldApi {
 protected:
  AcmChangeBitRateOldApi() : sampling_freq_hz_(0), frame_size_samples_(0) {}

  // Registers a send codec in the test::AcmSendTest object. Returns true on
  // success, false on failure.
  bool RegisterSendCodec(const char* payload_name,
                         int sampling_freq_hz,
                         int channels,
                         int payload_type,
                         int frame_size_samples,
                         int frame_size_rtp_timestamps) override {
    frame_size_samples_ = frame_size_samples;
    sampling_freq_hz_ = sampling_freq_hz;
    return AcmSetBitRateOldApi::RegisterSendCodec(
        payload_name, sampling_freq_hz, channels, payload_type,
        frame_size_samples, frame_size_rtp_timestamps);
  }

  // Runs the test. SetUpSender() and RegisterSendCodec() must have been called
  // before calling this method.
  void Run(int target_bitrate_bps,
           int expected_before_switch_bits,
           int expected_after_switch_bits) {
    ASSERT_TRUE(send_test_->acm());
    int nr_packets =
        sampling_freq_hz_ * kTestDurationMs / (frame_size_samples_ * 1000);
    int nr_bytes_before = 0, nr_bytes_after = 0;
    int packet_counter = 0;
    while (std::unique_ptr<test::Packet> next_packet =
               send_test_->NextPacket()) {
      if (packet_counter == nr_packets / 2)
        send_test_->acm()->SetBitRate(target_bitrate_bps);
      if (packet_counter < nr_packets / 2)
        nr_bytes_before +=
            rtc::checked_cast<int>(next_packet->payload_length_bytes());
      else
        nr_bytes_after +=
            rtc::checked_cast<int>(next_packet->payload_length_bytes());
      packet_counter++;
    }
    // Check that bitrate is 80-120 percent of expected value.
    EXPECT_GE(expected_before_switch_bits, nr_bytes_before * 8 * 8 / 10);
    EXPECT_LE(expected_before_switch_bits, nr_bytes_before * 8 * 12 / 10);
    EXPECT_GE(expected_after_switch_bits, nr_bytes_after * 8 * 8 / 10);
    EXPECT_LE(expected_after_switch_bits, nr_bytes_after * 8 * 12 / 10);
  }

  uint32_t sampling_freq_hz_;
  uint32_t frame_size_samples_;
};

TEST_F(AcmChangeBitRateOldApi, Opus_48khz_20ms_10kbps_2) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 1, 107, 960, 960));
  Run(10000, 14096, 4232);
}

TEST_F(AcmChangeBitRateOldApi, Opus_48khz_20ms_50kbps_2) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 1, 107, 960, 960));
  Run(50000, 14096, 22552);
}

TEST_F(AcmChangeBitRateOldApi, Opus_48khz_20ms_100kbps_2) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("opus", 48000, 1, 107, 960, 960));
  Run(100000, 14096, 49472);
}

// These next 2 tests ensure that the SetBitRate function has no effect on PCM
TEST_F(AcmChangeBitRateOldApi, Pcm16_8khz_10ms_8kbps) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 1, 107, 80, 80));
  Run(8000, 64000, 64000);
}

TEST_F(AcmChangeBitRateOldApi, Pcm16_8khz_10ms_32kbps) {
  ASSERT_NO_FATAL_FAILURE(SetUpTest("L16", 8000, 1, 107, 80, 80));
  Run(32000, 64000, 64000);
}

TEST_F(AcmSenderBitExactnessOldApi, External_Pcmu_20ms) {
  CodecInst codec_inst;
  codec_inst.channels = 1;
  codec_inst.pacsize = 160;
  codec_inst.pltype = 0;
  AudioEncoderPcmU encoder(codec_inst);
  auto mock_encoder = absl::make_unique<MockAudioEncoder>();
  // Set expectations on the mock encoder and also delegate the calls to the
  // real encoder.
  EXPECT_CALL(*mock_encoder, SampleRateHz())
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(&encoder, &AudioEncoderPcmU::SampleRateHz));
  EXPECT_CALL(*mock_encoder, NumChannels())
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(&encoder, &AudioEncoderPcmU::NumChannels));
  EXPECT_CALL(*mock_encoder, RtpTimestampRateHz())
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(&encoder, &AudioEncoderPcmU::RtpTimestampRateHz));
  EXPECT_CALL(*mock_encoder, Num10MsFramesInNextPacket())
      .Times(AtLeast(1))
      .WillRepeatedly(
          Invoke(&encoder, &AudioEncoderPcmU::Num10MsFramesInNextPacket));
  EXPECT_CALL(*mock_encoder, GetTargetBitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(&encoder, &AudioEncoderPcmU::GetTargetBitrate));
  EXPECT_CALL(*mock_encoder, EncodeImpl(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(
          &encoder, static_cast<AudioEncoder::EncodedInfo (AudioEncoder::*)(
                        uint32_t, rtc::ArrayView<const int16_t>, rtc::Buffer*)>(
                        &AudioEncoderPcmU::Encode)));
  ASSERT_NO_FATAL_FAILURE(
      SetUpTestExternalEncoder(std::move(mock_encoder), codec_inst.pltype));
  Run("81a9d4c0bb72e9becc43aef124c981e9", "8f9b8750bd80fe26b6cbf6659b89f0f9",
      50, test::AcmReceiveTestOldApi::kMonoOutput);
}

// This test fixture is implemented to run ACM and change the desired output
// frequency during the call. The input packets are simply PCM16b-wb encoded
// payloads with a constant value of |kSampleValue|. The test fixture itself
// acts as PacketSource in between the receive test class and the constant-
// payload packet source class. The output is both written to file, and analyzed
// in this test fixture.
class AcmSwitchingOutputFrequencyOldApi : public ::testing::Test,
                                          public test::PacketSource,
                                          public test::AudioSink {
 protected:
  static const size_t kTestNumPackets = 50;
  static const int kEncodedSampleRateHz = 16000;
  static const size_t kPayloadLenSamples = 30 * kEncodedSampleRateHz / 1000;
  static const int kPayloadType = 108;  // Default payload type for PCM16b-wb.

  AcmSwitchingOutputFrequencyOldApi()
      : first_output_(true),
        num_packets_(0),
        packet_source_(kPayloadLenSamples,
                       kSampleValue,
                       kEncodedSampleRateHz,
                       kPayloadType),
        output_freq_2_(0),
        has_toggled_(false) {}

  void Run(int output_freq_1, int output_freq_2, int toggle_period_ms) {
    // Set up the receiver used to decode the packets and verify the decoded
    // output.
    const std::string output_file_name =
        webrtc::test::OutputPath() +
        ::testing::UnitTest::GetInstance()
            ->current_test_info()
            ->test_case_name() +
        "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name() +
        "_output.pcm";
    test::OutputAudioFile output_file(output_file_name);
    // Have the output audio sent both to file and to the WriteArray method in
    // this class.
    test::AudioSinkFork output(this, &output_file);
    test::AcmReceiveTestToggleOutputFreqOldApi receive_test(
        this, &output, output_freq_1, output_freq_2, toggle_period_ms,
        test::AcmReceiveTestOldApi::kMonoOutput);
    ASSERT_NO_FATAL_FAILURE(receive_test.RegisterDefaultCodecs());
    output_freq_2_ = output_freq_2;

    // This is where the actual test is executed.
    receive_test.Run();

    // Delete output file.
    remove(output_file_name.c_str());
  }

  // Inherited from test::PacketSource.
  std::unique_ptr<test::Packet> NextPacket() override {
    // Check if it is time to terminate the test. The packet source is of type
    // ConstantPcmPacketSource, which is infinite, so we must end the test
    // "manually".
    if (num_packets_++ > kTestNumPackets) {
      EXPECT_TRUE(has_toggled_);
      return NULL;  // Test ended.
    }

    // Get the next packet from the source.
    return packet_source_.NextPacket();
  }

  // Inherited from test::AudioSink.
  bool WriteArray(const int16_t* audio, size_t num_samples) override {
    // Skip checking the first output frame, since it has a number of zeros
    // due to how NetEq is initialized.
    if (first_output_) {
      first_output_ = false;
      return true;
    }
    for (size_t i = 0; i < num_samples; ++i) {
      EXPECT_EQ(kSampleValue, audio[i]);
    }
    if (num_samples ==
        static_cast<size_t>(output_freq_2_ / 100))  // Size of 10 ms frame.
      has_toggled_ = true;
    // The return value does not say if the values match the expectation, just
    // that the method could process the samples.
    return true;
  }

  const int16_t kSampleValue = 1000;
  bool first_output_;
  size_t num_packets_;
  test::ConstantPcmPacketSource packet_source_;
  int output_freq_2_;
  bool has_toggled_;
};

TEST_F(AcmSwitchingOutputFrequencyOldApi, TestWithoutToggling) {
  Run(16000, 16000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle16KhzTo32Khz) {
  Run(16000, 32000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle32KhzTo16Khz) {
  Run(32000, 16000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle16KhzTo8Khz) {
  Run(16000, 8000, 1000);
}

TEST_F(AcmSwitchingOutputFrequencyOldApi, Toggle8KhzTo16Khz) {
  Run(8000, 16000, 1000);
}

#endif

}  // namespace webrtc
