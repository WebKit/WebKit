/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "test/call_test.h"
#include "test/field_trial.h"
#include "test/gtest.h"

#include "modules/rtp_rtcp/source/byte_io.h"
#include "test/rtcp_packet_parser.h"
#include "video/end_to_end_tests/multi_stream_tester.h"

namespace webrtc {

class TransportFeedbackEndToEndTest
    : public test::CallTest,
      public testing::WithParamInterface<std::string> {
 public:
  TransportFeedbackEndToEndTest() : field_trial_(GetParam()) {}

  virtual ~TransportFeedbackEndToEndTest() {
  }

 private:
  test::ScopedFieldTrials field_trial_;
};

INSTANTIATE_TEST_CASE_P(
    FieldTrials,
    TransportFeedbackEndToEndTest,
    ::testing::Values("WebRTC-TaskQueueCongestionControl/Enabled/",
                      "WebRTC-TaskQueueCongestionControl/Disabled/"));

TEST_P(TransportFeedbackEndToEndTest, AssignsTransportSequenceNumbers) {
  static const int kExtensionId = 5;

  class RtpExtensionHeaderObserver : public test::DirectTransport {
   public:
    RtpExtensionHeaderObserver(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        Call* sender_call,
        const uint32_t& first_media_ssrc,
        const std::map<uint32_t, uint32_t>& ssrc_map,
        const std::map<uint8_t, MediaType>& payload_type_map)
        : DirectTransport(task_queue,
                          absl::make_unique<FakeNetworkPipe>(
                              Clock::GetRealTimeClock(),
                              absl::make_unique<SimulatedNetwork>(
                                  BuiltInNetworkBehaviorConfig())),
                          sender_call,
                          payload_type_map),
          parser_(RtpHeaderParser::Create()),
          first_media_ssrc_(first_media_ssrc),
          rtx_to_media_ssrcs_(ssrc_map),
          padding_observed_(false),
          rtx_padding_observed_(false),
          retransmit_observed_(false),
          started_(false) {
      parser_->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                          kExtensionId);
    }
    virtual ~RtpExtensionHeaderObserver() {}

    bool SendRtp(const uint8_t* data,
                 size_t length,
                 const PacketOptions& options) override {
      {
        rtc::CritScope cs(&lock_);

        if (IsDone())
          return false;

        if (started_) {
          RTPHeader header;
          EXPECT_TRUE(parser_->Parse(data, length, &header));
          bool drop_packet = false;

          EXPECT_TRUE(header.extension.hasTransportSequenceNumber);
          EXPECT_EQ(options.packet_id,
                    header.extension.transportSequenceNumber);
          if (!streams_observed_.empty()) {
            // Unwrap packet id and verify uniqueness.
            int64_t packet_id = unwrapper_.Unwrap(options.packet_id);
            EXPECT_TRUE(received_packed_ids_.insert(packet_id).second);
          }

          // Drop (up to) every 17th packet, so we get retransmits.
          // Only drop media, and not on the first stream (otherwise it will be
          // hard to distinguish from padding, which is always sent on the first
          // stream).
          if (header.payloadType != kSendRtxPayloadType &&
              header.ssrc != first_media_ssrc_ &&
              header.extension.transportSequenceNumber % 17 == 0) {
            dropped_seq_[header.ssrc].insert(header.sequenceNumber);
            drop_packet = true;
          }

          if (header.payloadType == kSendRtxPayloadType) {
            uint16_t original_sequence_number =
                ByteReader<uint16_t>::ReadBigEndian(&data[header.headerLength]);
            uint32_t original_ssrc =
                rtx_to_media_ssrcs_.find(header.ssrc)->second;
            std::set<uint16_t>* seq_no_map = &dropped_seq_[original_ssrc];
            auto it = seq_no_map->find(original_sequence_number);
            if (it != seq_no_map->end()) {
              retransmit_observed_ = true;
              seq_no_map->erase(it);
            } else {
              rtx_padding_observed_ = true;
            }
          } else {
            streams_observed_.insert(header.ssrc);
          }

          if (IsDone())
            done_.Set();

          if (drop_packet)
            return true;
        }
      }

      return test::DirectTransport::SendRtp(data, length, options);
    }

    bool IsDone() {
      bool observed_types_ok =
          streams_observed_.size() == MultiStreamTester::kNumStreams &&
          retransmit_observed_ && rtx_padding_observed_;
      if (!observed_types_ok)
        return false;
      // We should not have any gaps in the sequence number range.
      size_t seqno_range =
          *received_packed_ids_.rbegin() - *received_packed_ids_.begin() + 1;
      return seqno_range == received_packed_ids_.size();
    }

    bool Wait() {
      {
        // Can't be sure until this point that rtx_to_media_ssrcs_ etc have
        // been initialized and are OK to read.
        rtc::CritScope cs(&lock_);
        started_ = true;
      }
      return done_.Wait(kDefaultTimeoutMs);
    }

    rtc::CriticalSection lock_;
    rtc::Event done_;
    std::unique_ptr<RtpHeaderParser> parser_;
    SequenceNumberUnwrapper unwrapper_;
    std::set<int64_t> received_packed_ids_;
    std::set<uint32_t> streams_observed_;
    std::map<uint32_t, std::set<uint16_t>> dropped_seq_;
    const uint32_t& first_media_ssrc_;
    const std::map<uint32_t, uint32_t>& rtx_to_media_ssrcs_;
    bool padding_observed_;
    bool rtx_padding_observed_;
    bool retransmit_observed_;
    bool started_;
  };

  class TransportSequenceNumberTester : public MultiStreamTester {
   public:
    explicit TransportSequenceNumberTester(
        test::SingleThreadedTaskQueueForTesting* task_queue)
        : MultiStreamTester(task_queue),
          first_media_ssrc_(0),
          observer_(nullptr) {}
    virtual ~TransportSequenceNumberTester() {}

   protected:
    void Wait() override {
      RTC_DCHECK(observer_);
      EXPECT_TRUE(observer_->Wait());
    }

    void UpdateSendConfig(
        size_t stream_index,
        VideoSendStream::Config* send_config,
        VideoEncoderConfig* encoder_config,
        test::FrameGeneratorCapturer** frame_generator) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTransportSequenceNumberUri, kExtensionId));

      // Force some padding to be sent. Note that since we do send media
      // packets we can not guarantee that a padding only packet is sent.
      // Instead, padding will most likely be send as an RTX packet.
      const int kPaddingBitrateBps = 50000;
      encoder_config->max_bitrate_bps = 200000;
      encoder_config->min_transmit_bitrate_bps =
          encoder_config->max_bitrate_bps + kPaddingBitrateBps;

      // Configure RTX for redundant payload padding.
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[stream_index]);
      send_config->rtp.rtx.payload_type = kSendRtxPayloadType;
      rtx_to_media_ssrcs_[kSendRtxSsrcs[stream_index]] =
          send_config->rtp.ssrcs[0];

      if (stream_index == 0)
        first_media_ssrc_ = send_config->rtp.ssrcs[0];
    }

    void UpdateReceiveConfig(
        size_t stream_index,
        VideoReceiveStream::Config* receive_config) override {
      receive_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      receive_config->rtp.extensions.clear();
      receive_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTransportSequenceNumberUri, kExtensionId));
      receive_config->renderer = &fake_renderer_;
    }

    test::DirectTransport* CreateSendTransport(
        test::SingleThreadedTaskQueueForTesting* task_queue,
        Call* sender_call) override {
      std::map<uint8_t, MediaType> payload_type_map =
          MultiStreamTester::payload_type_map_;
      RTC_DCHECK(payload_type_map.find(kSendRtxPayloadType) ==
                 payload_type_map.end());
      payload_type_map[kSendRtxPayloadType] = MediaType::VIDEO;
      observer_ = new RtpExtensionHeaderObserver(
          task_queue, sender_call, first_media_ssrc_, rtx_to_media_ssrcs_,
          payload_type_map);
      return observer_;
    }

   private:
    test::FakeVideoRenderer fake_renderer_;
    uint32_t first_media_ssrc_;
    std::map<uint32_t, uint32_t> rtx_to_media_ssrcs_;
    RtpExtensionHeaderObserver* observer_;
  } tester(&task_queue_);

  tester.RunTest();
}

class TransportFeedbackTester : public test::EndToEndTest {
 public:
  TransportFeedbackTester(bool feedback_enabled,
                          size_t num_video_streams,
                          size_t num_audio_streams)
      : EndToEndTest(
            ::webrtc::TransportFeedbackEndToEndTest::kDefaultTimeoutMs),
        feedback_enabled_(feedback_enabled),
        num_video_streams_(num_video_streams),
        num_audio_streams_(num_audio_streams),
        receiver_call_(nullptr) {
    // Only one stream of each supported for now.
    EXPECT_LE(num_video_streams, 1u);
    EXPECT_LE(num_audio_streams, 1u);
  }

 protected:
  Action OnSendRtcp(const uint8_t* data, size_t length) override {
    EXPECT_FALSE(HasTransportFeedback(data, length));
    return SEND_PACKET;
  }

  Action OnReceiveRtcp(const uint8_t* data, size_t length) override {
    if (HasTransportFeedback(data, length))
      observation_complete_.Set();
    return SEND_PACKET;
  }

  bool HasTransportFeedback(const uint8_t* data, size_t length) const {
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(data, length));
    return parser.transport_feedback()->num_packets() > 0;
  }

  void PerformTest() override {
    const int64_t kDisabledFeedbackTimeoutMs = 5000;
    EXPECT_EQ(feedback_enabled_,
              observation_complete_.Wait(feedback_enabled_
                                             ? test::CallTest::kDefaultTimeoutMs
                                             : kDisabledFeedbackTimeoutMs));
  }

  void OnCallsCreated(Call* sender_call, Call* receiver_call) override {
    receiver_call_ = receiver_call;
  }

  size_t GetNumVideoStreams() const override { return num_video_streams_; }
  size_t GetNumAudioStreams() const override { return num_audio_streams_; }

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    (*receive_configs)[0].rtp.transport_cc = feedback_enabled_;
  }

  void ModifyAudioConfigs(
      AudioSendStream::Config* send_config,
      std::vector<AudioReceiveStream::Config>* receive_configs) override {
    send_config->rtp.extensions.clear();
    send_config->rtp.extensions.push_back(
        RtpExtension(RtpExtension::kTransportSequenceNumberUri, kExtensionId));
    (*receive_configs)[0].rtp.extensions.clear();
    (*receive_configs)[0].rtp.extensions = send_config->rtp.extensions;
    (*receive_configs)[0].rtp.transport_cc = feedback_enabled_;
  }

 private:
  static const int kExtensionId = 5;
  const bool feedback_enabled_;
  const size_t num_video_streams_;
  const size_t num_audio_streams_;
  Call* receiver_call_;
};

TEST_P(TransportFeedbackEndToEndTest, VideoReceivesTransportFeedback) {
  TransportFeedbackTester test(true, 1, 0);
  RunBaseTest(&test);
}

TEST_P(TransportFeedbackEndToEndTest, VideoTransportFeedbackNotConfigured) {
  TransportFeedbackTester test(false, 1, 0);
  RunBaseTest(&test);
}

TEST_P(TransportFeedbackEndToEndTest, AudioReceivesTransportFeedback) {
  TransportFeedbackTester test(true, 0, 1);
  RunBaseTest(&test);
}

TEST_P(TransportFeedbackEndToEndTest, AudioTransportFeedbackNotConfigured) {
  TransportFeedbackTester test(false, 0, 1);
  RunBaseTest(&test);
}

TEST_P(TransportFeedbackEndToEndTest, AudioVideoReceivesTransportFeedback) {
  TransportFeedbackTester test(true, 1, 1);
  RunBaseTest(&test);
}

TEST_P(TransportFeedbackEndToEndTest,
       StopsAndResumesMediaWhenCongestionWindowFull) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-CwndExperiment/Enabled-250/");

  class TransportFeedbackTester : public test::EndToEndTest {
   public:
    TransportFeedbackTester(size_t num_video_streams, size_t num_audio_streams)
        : EndToEndTest(
              ::webrtc::TransportFeedbackEndToEndTest::kDefaultTimeoutMs),
          num_video_streams_(num_video_streams),
          num_audio_streams_(num_audio_streams),
          media_sent_(0),
          media_sent_before_(0),
          padding_sent_(0) {
      // Only one stream of each supported for now.
      EXPECT_LE(num_video_streams, 1u);
      EXPECT_LE(num_audio_streams, 1u);
    }

   protected:
    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      const bool only_padding =
          header.headerLength + header.paddingLength == length;
      rtc::CritScope lock(&crit_);
      // Padding is expected in congested state to probe for connectivity when
      // packets has been dropped.
      if (only_padding) {
        media_sent_before_ = media_sent_;
        ++padding_sent_;
      } else {
        ++media_sent_;
        if (padding_sent_ == 0) {
          ++media_sent_before_;
          EXPECT_LT(media_sent_, 40)
              << "Media sent without feedback when congestion window is full.";
        } else if (media_sent_ > media_sent_before_) {
          observation_complete_.Set();
        }
      }
      return SEND_PACKET;
    }

    Action OnReceiveRtcp(const uint8_t* data, size_t length) override {
      rtc::CritScope lock(&crit_);
      // To fill up the congestion window we drop feedback on packets after 20
      // packets have been sent. This means that any packets that has not yet
      // received feedback after that will be considered as oustanding data and
      // therefore filling up the congestion window. In the congested state, the
      // pacer should send padding packets to trigger feedback in case all
      // feedback of previous traffic was lost. This test listens for the
      // padding packets and when 2 padding packets have been received, feedback
      // will be let trough again. This should cause the pacer to continue
      // sending meadia yet again.
      if (media_sent_ > 20 && HasTransportFeedback(data, length) &&
          padding_sent_ < 2) {
        return DROP_PACKET;
      }
      return SEND_PACKET;
    }

    bool HasTransportFeedback(const uint8_t* data, size_t length) const {
      test::RtcpPacketParser parser;
      EXPECT_TRUE(parser.Parse(data, length));
      return parser.transport_feedback()->num_packets() > 0;
    }
    void ModifySenderBitrateConfig(
        BitrateConstraints* bitrate_config) override {
      bitrate_config->max_bitrate_bps = 300000;
    }

    void PerformTest() override {
      const int64_t kFailureTimeoutMs = 10000;
      EXPECT_TRUE(observation_complete_.Wait(kFailureTimeoutMs))
          << "Stream not continued after congestion window full.";
    }

    size_t GetNumVideoStreams() const override { return num_video_streams_; }
    size_t GetNumAudioStreams() const override { return num_audio_streams_; }

   private:
    const size_t num_video_streams_;
    const size_t num_audio_streams_;
    rtc::CriticalSection crit_;
    int media_sent_ RTC_GUARDED_BY(crit_);
    int media_sent_before_ RTC_GUARDED_BY(crit_);
    int padding_sent_ RTC_GUARDED_BY(crit_);
  } test(1, 0);
  RunBaseTest(&test);
}

TEST_P(TransportFeedbackEndToEndTest, TransportSeqNumOnAudioAndVideo) {
  static constexpr int kExtensionId = 8;
  static constexpr size_t kMinPacketsToWaitFor = 50;
  class TransportSequenceNumberTest : public test::EndToEndTest {
   public:
    TransportSequenceNumberTest()
        : EndToEndTest(kDefaultTimeoutMs),
          video_observed_(false),
          audio_observed_(false) {
      parser_->RegisterRtpHeaderExtension(kRtpExtensionTransportSequenceNumber,
                                          kExtensionId);
    }

    size_t GetNumVideoStreams() const override { return 1; }
    size_t GetNumAudioStreams() const override { return 1; }

    void ModifyAudioConfigs(
        AudioSendStream::Config* send_config,
        std::vector<AudioReceiveStream::Config>* receive_configs) override {
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTransportSequenceNumberUri, kExtensionId));
      (*receive_configs)[0].rtp.extensions.clear();
      (*receive_configs)[0].rtp.extensions = send_config->rtp.extensions;
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      RTPHeader header;
      EXPECT_TRUE(parser_->Parse(packet, length, &header));
      EXPECT_TRUE(header.extension.hasTransportSequenceNumber);
      // Unwrap packet id and verify uniqueness.
      int64_t packet_id =
          unwrapper_.Unwrap(header.extension.transportSequenceNumber);
      EXPECT_TRUE(received_packet_ids_.insert(packet_id).second);

      if (header.ssrc == kVideoSendSsrcs[0])
        video_observed_ = true;
      if (header.ssrc == kAudioSendSsrc)
        audio_observed_ = true;
      if (audio_observed_ && video_observed_ &&
          received_packet_ids_.size() >= kMinPacketsToWaitFor) {
        size_t packet_id_range =
            *received_packet_ids_.rbegin() - *received_packet_ids_.begin() + 1;
        EXPECT_EQ(received_packet_ids_.size(), packet_id_range);
        observation_complete_.Set();
      }
      return SEND_PACKET;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out while waiting for audio and video "
                             "packets with transport sequence number.";
    }

    void ExpectSuccessful() {
      EXPECT_TRUE(video_observed_);
      EXPECT_TRUE(audio_observed_);
      EXPECT_GE(received_packet_ids_.size(), kMinPacketsToWaitFor);
    }

   private:
    bool video_observed_;
    bool audio_observed_;
    SequenceNumberUnwrapper unwrapper_;
    std::set<int64_t> received_packet_ids_;
  } test;

  RunBaseTest(&test);
  // Double check conditions for successful test to produce better error
  // message when the test fail.
  test.ExpectSuccessful();
}
}  // namespace webrtc
