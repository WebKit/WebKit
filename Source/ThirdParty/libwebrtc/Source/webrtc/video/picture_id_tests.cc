/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "media/engine/internalencoderfactory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "test/call_test.h"
#include "test/field_trial.h"

namespace webrtc {
namespace {
const int kFrameMaxWidth = 1280;
const int kFrameMaxHeight = 720;
const int kFrameRate = 30;
const int kMaxSecondsLost = 5;
const int kMaxFramesLost = kFrameRate * kMaxSecondsLost;
const int kMinPacketsToObserve = 10;
const int kEncoderBitrateBps = 100000;
const uint32_t kPictureIdWraparound = (1 << 15);

const char kVp8ForcedFallbackEncoderEnabled[] =
    "WebRTC-VP8-Forced-Fallback-Encoder-v2/Enabled/";
}  // namespace

class PictureIdObserver : public test::RtpRtcpObserver {
 public:
  PictureIdObserver()
      : test::RtpRtcpObserver(test::CallTest::kDefaultTimeoutMs),
        max_expected_picture_id_gap_(0),
        num_ssrcs_to_observe_(1) {}

  void SetExpectedSsrcs(size_t num_expected_ssrcs) {
    rtc::CritScope lock(&crit_);
    num_ssrcs_to_observe_ = num_expected_ssrcs;
  }

  void ResetObservedSsrcs() {
    rtc::CritScope lock(&crit_);
    // Do not clear the timestamp and picture_id, to ensure that we check
    // consistency between reinits and recreations.
    num_packets_sent_.clear();
    observed_ssrcs_.clear();
  }

  void SetMaxExpectedPictureIdGap(int max_expected_picture_id_gap) {
    rtc::CritScope lock(&crit_);
    max_expected_picture_id_gap_ = max_expected_picture_id_gap;
  }

 private:
  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);

    // RTP header.
    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, length, &header));
    const uint32_t timestamp = header.timestamp;
    const uint32_t ssrc = header.ssrc;

    const bool known_ssrc = (ssrc == test::CallTest::kVideoSendSsrcs[0] ||
                             ssrc == test::CallTest::kVideoSendSsrcs[1] ||
                             ssrc == test::CallTest::kVideoSendSsrcs[2]);
    EXPECT_TRUE(known_ssrc) << "Unknown SSRC sent.";

    const bool is_padding =
        (length == header.headerLength + header.paddingLength);
    if (is_padding) {
      return SEND_PACKET;
    }

    // VP8 header.
    std::unique_ptr<RtpDepacketizer> depacketizer(
        RtpDepacketizer::Create(kRtpVideoVp8));
    RtpDepacketizer::ParsedPayload parsed_payload;
    EXPECT_TRUE(depacketizer->Parse(
        &parsed_payload, &packet[header.headerLength],
        length - header.headerLength - header.paddingLength));
    const uint16_t picture_id =
        parsed_payload.type.Video.codecHeader.VP8.pictureId;

    // If this is the first packet, we have nothing to compare to.
    if (last_observed_timestamp_.find(ssrc) == last_observed_timestamp_.end()) {
      last_observed_timestamp_[ssrc] = timestamp;
      last_observed_picture_id_[ssrc] = picture_id;
      ++num_packets_sent_[ssrc];

      return SEND_PACKET;
    }

    // Verify continuity and monotonicity of picture_id sequence.
    if (last_observed_timestamp_[ssrc] == timestamp) {
      // Packet belongs to same frame as before.
      EXPECT_EQ(last_observed_picture_id_[ssrc], picture_id);
    } else {
      // Packet is a new frame.

      // Picture id should be increasing.
      const bool picture_id_is_increasing =
          AheadOf<uint16_t, kPictureIdWraparound>(
              picture_id, last_observed_picture_id_[ssrc]);
      EXPECT_TRUE(picture_id_is_increasing);

      // Picture id should not increase more than expected.
      const int picture_id_diff = ForwardDiff<uint16_t, kPictureIdWraparound>(
          last_observed_picture_id_[ssrc], picture_id);

      // For delta frames, expect continuously increasing picture id.
      if (parsed_payload.frame_type != kVideoFrameKey) {
        EXPECT_EQ(picture_id_diff, 1);
      }
      // Any frames still in queue is lost when a VideoSendStream is destroyed.
      // The first frame after recreation should be a key frame.
      if (picture_id_diff > 1) {
        EXPECT_EQ(kVideoFrameKey, parsed_payload.frame_type);
        EXPECT_LE(picture_id_diff - 1, max_expected_picture_id_gap_);
      }
    }
    last_observed_timestamp_[ssrc] = timestamp;
    last_observed_picture_id_[ssrc] = picture_id;

    // Pass the test when enough media packets have been received
    // on all streams.
    if (++num_packets_sent_[ssrc] >= kMinPacketsToObserve &&
        observed_ssrcs_.find(ssrc) == observed_ssrcs_.end()) {
      observed_ssrcs_.insert(ssrc);
      if (observed_ssrcs_.size() == num_ssrcs_to_observe_) {
        observation_complete_.Set();
      }
    }

    return SEND_PACKET;
  }

  rtc::CriticalSection crit_;
  std::map<uint32_t, uint32_t> last_observed_timestamp_ RTC_GUARDED_BY(crit_);
  std::map<uint32_t, uint16_t> last_observed_picture_id_ RTC_GUARDED_BY(crit_);
  std::map<uint32_t, size_t> num_packets_sent_ RTC_GUARDED_BY(crit_);
  int max_expected_picture_id_gap_ RTC_GUARDED_BY(crit_);
  size_t num_ssrcs_to_observe_ RTC_GUARDED_BY(crit_);
  std::set<uint32_t> observed_ssrcs_ RTC_GUARDED_BY(crit_);
};

class PictureIdTest : public test::CallTest,
                      public ::testing::WithParamInterface<std::string> {
 public:
  PictureIdTest() : scoped_field_trial_(GetParam()) {}

  virtual ~PictureIdTest() {
    EXPECT_EQ(nullptr, video_send_stream_);
    EXPECT_TRUE(video_receive_streams_.empty());

    task_queue_.SendTask([this]() {
      Stop();
      DestroyStreams();
      send_transport_.reset();
      receive_transport_.reset();
      DestroyCalls();
    });
  }

  void SetupEncoder(VideoEncoder* encoder);
  void TestPictureIdContinuousAfterReconfigure(
      const std::vector<int>& ssrc_counts);
  void TestPictureIdIncreaseAfterRecreateStreams(
      const std::vector<int>& ssrc_counts);

 private:
  test::ScopedFieldTrials scoped_field_trial_;
  PictureIdObserver observer;
};

INSTANTIATE_TEST_CASE_P(TestWithForcedFallbackEncoderEnabled,
                        PictureIdTest,
                        ::testing::Values(kVp8ForcedFallbackEncoderEnabled,
                                          ""));

// Use a special stream factory to ensure that all simulcast streams are being
// sent.
class VideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  VideoStreamFactory() = default;

 private:
  std::vector<VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const VideoEncoderConfig& encoder_config) override {
    std::vector<VideoStream> streams =
        test::CreateVideoStreams(width, height, encoder_config);

    if (encoder_config.number_of_streams > 1) {
      RTC_DCHECK_EQ(3, encoder_config.number_of_streams);

      for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
        streams[i].min_bitrate_bps = kEncoderBitrateBps;
        streams[i].target_bitrate_bps = kEncoderBitrateBps;
        streams[i].max_bitrate_bps = kEncoderBitrateBps;
      }

      // test::CreateVideoStreams does not return frame sizes for the lower
      // streams that are accepted by VP8Impl::InitEncode.
      // TODO(brandtr): Fix the problem in test::CreateVideoStreams, rather
      // than overriding the values here.
      streams[1].width = streams[2].width / 2;
      streams[1].height = streams[2].height / 2;
      streams[0].width = streams[1].width / 2;
      streams[0].height = streams[1].height / 2;
    } else {
      // Use the same total bitrates when sending a single stream to avoid
      // lowering the bitrate estimate and requiring a subsequent rampup.
      streams[0].min_bitrate_bps = 3 * kEncoderBitrateBps;
      streams[0].target_bitrate_bps = 3 * kEncoderBitrateBps;
      streams[0].max_bitrate_bps = 3 * kEncoderBitrateBps;
    }

    return streams;
  }
};

void PictureIdTest::SetupEncoder(VideoEncoder* encoder) {
  task_queue_.SendTask([this, &encoder]() {
    Call::Config config(event_log_.get());
    CreateCalls(config, config);

    send_transport_.reset(new test::PacketTransport(
        &task_queue_, sender_call_.get(), &observer,
        test::PacketTransport::kSender, payload_type_map_,
        FakeNetworkPipe::Config()));

    CreateSendConfig(kNumSsrcs, 0, 0, send_transport_.get());
    video_send_config_.encoder_settings.encoder = encoder;
    video_send_config_.encoder_settings.payload_name = "VP8";
    video_encoder_config_.video_stream_factory =
        new rtc::RefCountedObject<VideoStreamFactory>();
    video_encoder_config_.number_of_streams = 1;
  });
}

void PictureIdTest::TestPictureIdContinuousAfterReconfigure(
    const std::vector<int>& ssrc_counts) {
  task_queue_.SendTask([this]() {
    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kFrameRate, kFrameMaxWidth, kFrameMaxHeight);

    // Initial test with a single stream.
    Start();
  });

  EXPECT_TRUE(observer.Wait()) << "Timed out waiting for packets.";

  // Reconfigure VideoEncoder and test picture id increase.
  // Expect continuously increasing picture id, equivalent to no gaps.
  observer.SetMaxExpectedPictureIdGap(0);
  for (int ssrc_count : ssrc_counts) {
    video_encoder_config_.number_of_streams = ssrc_count;
    observer.SetExpectedSsrcs(ssrc_count);
    observer.ResetObservedSsrcs();
    // Make sure the picture_id sequence is continuous on reinit and recreate.
    task_queue_.SendTask([this]() {
      video_send_stream_->ReconfigureVideoEncoder(video_encoder_config_.Copy());
    });
    EXPECT_TRUE(observer.Wait()) << "Timed out waiting for packets.";
  }

  task_queue_.SendTask([this]() {
    Stop();
    DestroyStreams();
    send_transport_.reset();
    receive_transport_.reset();
    DestroyCalls();
  });
}

void PictureIdTest::TestPictureIdIncreaseAfterRecreateStreams(
    const std::vector<int>& ssrc_counts) {
  task_queue_.SendTask([this]() {
    CreateVideoStreams();
    CreateFrameGeneratorCapturer(kFrameRate, kFrameMaxWidth, kFrameMaxHeight);

    // Initial test with a single stream.
    Start();
  });

  EXPECT_TRUE(observer.Wait()) << "Timed out waiting for packets.";

  // Recreate VideoSendStream and test picture id increase.
  // When the VideoSendStream is destroyed, any frames still in queue is lost
  // with it, therefore it is expected that some frames might be lost.
  observer.SetMaxExpectedPictureIdGap(kMaxFramesLost);
  for (int ssrc_count : ssrc_counts) {
    task_queue_.SendTask([this, &ssrc_count]() {
      video_encoder_config_.number_of_streams = ssrc_count;

      frame_generator_capturer_->Stop();
      sender_call_->DestroyVideoSendStream(video_send_stream_);

      observer.SetExpectedSsrcs(ssrc_count);
      observer.ResetObservedSsrcs();

      video_send_stream_ = sender_call_->CreateVideoSendStream(
          video_send_config_.Copy(), video_encoder_config_.Copy());
      video_send_stream_->Start();
      CreateFrameGeneratorCapturer(kFrameRate, kFrameMaxWidth, kFrameMaxHeight);
      frame_generator_capturer_->Start();
    });

    EXPECT_TRUE(observer.Wait()) << "Timed out waiting for packets.";
  }

  task_queue_.SendTask([this]() {
    Stop();
    DestroyStreams();
    send_transport_.reset();
    receive_transport_.reset();
  });
}

TEST_P(PictureIdTest, PictureIdContinuousAfterReconfigureVp8) {
  std::unique_ptr<VideoEncoder> encoder(VP8Encoder::Create());
  SetupEncoder(encoder.get());
  TestPictureIdContinuousAfterReconfigure({1, 3, 3, 1, 1});
}

TEST_P(PictureIdTest, PictureIdIncreasingAfterRecreateStreamVp8) {
  std::unique_ptr<VideoEncoder> encoder(VP8Encoder::Create());
  SetupEncoder(encoder.get());
  TestPictureIdIncreaseAfterRecreateStreams({1, 3, 3, 1, 1});
}

TEST_P(PictureIdTest, PictureIdIncreasingAfterStreamCountChangeVp8) {
  std::unique_ptr<VideoEncoder> encoder(VP8Encoder::Create());
  // Make sure that that the picture id is not reset if the stream count goes
  // down and then up.
  std::vector<int> ssrc_counts = {3, 1, 3};
  SetupEncoder(encoder.get());
  TestPictureIdContinuousAfterReconfigure(ssrc_counts);
}

TEST_P(PictureIdTest,
       PictureIdContinuousAfterReconfigureSimulcastEncoderAdapter) {
  InternalEncoderFactory internal_encoder_factory;
  SimulcastEncoderAdapter simulcast_encoder_adapter(&internal_encoder_factory);
  SetupEncoder(&simulcast_encoder_adapter);
  TestPictureIdContinuousAfterReconfigure({1, 3, 3, 1, 1});
}

TEST_P(PictureIdTest,
       PictureIdIncreasingAfterRecreateStreamSimulcastEncoderAdapter) {
  InternalEncoderFactory internal_encoder_factory;
  SimulcastEncoderAdapter simulcast_encoder_adapter(&internal_encoder_factory);
  SetupEncoder(&simulcast_encoder_adapter);
  TestPictureIdIncreaseAfterRecreateStreams({1, 3, 3, 1, 1});
}

// When using the simulcast encoder adapter, the picture id is randomly set
// when the ssrc count is reduced and then increased. This means that we are
// not spec compliant in that particular case.
TEST_P(PictureIdTest,
       PictureIdIncreasingAfterStreamCountChangeSimulcastEncoderAdapter) {
  // If forced fallback is enabled, the picture id is set in the PayloadRouter
  // and the sequence should be continuous.
  if (GetParam() == kVp8ForcedFallbackEncoderEnabled) {
    InternalEncoderFactory internal_encoder_factory;
    SimulcastEncoderAdapter simulcast_encoder_adapter(
        &internal_encoder_factory);
    // Make sure that that the picture id is not reset if the stream count goes
    // down and then up.
    std::vector<int> ssrc_counts = {3, 1, 3};
    SetupEncoder(&simulcast_encoder_adapter);
    TestPictureIdContinuousAfterReconfigure(ssrc_counts);
  }
}

}  // namespace webrtc
