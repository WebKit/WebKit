/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/simulated_network.h"
#include "call/fake_network_pipe.h"
#include "call/simulated_network.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "test/call_test.h"
#include "test/function_video_encoder_factory.h"

namespace webrtc {
namespace {
const int kFrameMaxWidth = 1280;
const int kFrameMaxHeight = 720;
const int kFrameRate = 30;
const int kMaxSecondsLost = 5;
const int kMaxFramesLost = kFrameRate * kMaxSecondsLost;
const int kMinPacketsToObserve = 10;
const int kEncoderBitrateBps = 300000;
const uint32_t kPictureIdWraparound = (1 << 15);
const size_t kNumTemporalLayers[] = {1, 2, 3};

}  // namespace

class PictureIdObserver : public test::RtpRtcpObserver {
 public:
  explicit PictureIdObserver(VideoCodecType codec_type)
      : test::RtpRtcpObserver(test::CallTest::kDefaultTimeoutMs),
        codec_type_(codec_type),
        max_expected_picture_id_gap_(0),
        max_expected_tl0_idx_gap_(0),
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
    // Expect smaller gap for |tl0_pic_idx| (running index for temporal_idx 0).
    max_expected_tl0_idx_gap_ = max_expected_picture_id_gap_ / 2;
  }

 private:
  struct ParsedPacket {
    uint32_t timestamp;
    uint32_t ssrc;
    int16_t picture_id;
    int16_t tl0_pic_idx;
    uint8_t temporal_idx;
    FrameType frame_type;
  };

  bool ParsePayload(const uint8_t* packet,
                    size_t length,
                    ParsedPacket* parsed) const {
    RTPHeader header;
    EXPECT_TRUE(parser_->Parse(packet, length, &header));
    EXPECT_TRUE(header.ssrc == test::CallTest::kVideoSendSsrcs[0] ||
                header.ssrc == test::CallTest::kVideoSendSsrcs[1] ||
                header.ssrc == test::CallTest::kVideoSendSsrcs[2])
        << "Unknown SSRC sent.";

    EXPECT_GE(length, header.headerLength + header.paddingLength);
    size_t payload_length = length - header.headerLength - header.paddingLength;
    if (payload_length == 0) {
      return false;  // Padding packet.
    }

    parsed->timestamp = header.timestamp;
    parsed->ssrc = header.ssrc;

    std::unique_ptr<RtpDepacketizer> depacketizer(
        RtpDepacketizer::Create(codec_type_));
    RtpDepacketizer::ParsedPayload parsed_payload;
    EXPECT_TRUE(depacketizer->Parse(
        &parsed_payload, &packet[header.headerLength], payload_length));

    switch (codec_type_) {
      case kVideoCodecVP8: {
        const auto& vp8_header = absl::get<RTPVideoHeaderVP8>(
            parsed_payload.video_header().video_type_header);
        parsed->picture_id = vp8_header.pictureId;
        parsed->tl0_pic_idx = vp8_header.tl0PicIdx;
        parsed->temporal_idx = vp8_header.temporalIdx;
        break;
      }
      case kVideoCodecVP9: {
        const auto& vp9_header = absl::get<RTPVideoHeaderVP9>(
            parsed_payload.video_header().video_type_header);
        parsed->picture_id = vp9_header.picture_id;
        parsed->tl0_pic_idx = vp9_header.tl0_pic_idx;
        parsed->temporal_idx = vp9_header.temporal_idx;
        break;
      }
      default:
        RTC_NOTREACHED();
        break;
    }

    parsed->frame_type = parsed_payload.frame_type;
    return true;
  }

  // Verify continuity and monotonicity of picture_id sequence.
  void VerifyPictureId(const ParsedPacket& current,
                       const ParsedPacket& last) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(&crit_) {
    if (current.timestamp == last.timestamp) {
      EXPECT_EQ(last.picture_id, current.picture_id);
      return;  // Same frame.
    }

    // Packet belongs to a new frame.
    // Picture id should be increasing.
    EXPECT_TRUE((AheadOf<uint16_t, kPictureIdWraparound>(current.picture_id,
                                                         last.picture_id)));

    // Expect continuously increasing picture id.
    int diff = ForwardDiff<uint16_t, kPictureIdWraparound>(last.picture_id,
                                                           current.picture_id);
    if (diff > 1) {
      // If the VideoSendStream is destroyed, any frames still in queue is lost.
      // Gaps only possible for first frame after a recreation, i.e. key frames.
      EXPECT_EQ(kVideoFrameKey, current.frame_type);
      EXPECT_LE(diff - 1, max_expected_picture_id_gap_);
    }
  }

  void VerifyTl0Idx(const ParsedPacket& current, const ParsedPacket& last) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(&crit_) {
    if (current.tl0_pic_idx == kNoTl0PicIdx ||
        current.temporal_idx == kNoTemporalIdx) {
      return;  // No temporal layers.
    }

    if (current.timestamp == last.timestamp || current.temporal_idx != 0) {
      EXPECT_EQ(last.tl0_pic_idx, current.tl0_pic_idx);
      return;
    }

    // New frame with |temporal_idx| 0.
    // |tl0_pic_idx| should be increasing.
    EXPECT_TRUE(AheadOf<uint8_t>(current.tl0_pic_idx, last.tl0_pic_idx));

    // Expect continuously increasing idx.
    int diff = ForwardDiff<uint8_t>(last.tl0_pic_idx, current.tl0_pic_idx);
    if (diff > 1) {
      // If the VideoSendStream is destroyed, any frames still in queue is lost.
      // Gaps only possible for first frame after a recreation, i.e. key frames.
      EXPECT_EQ(kVideoFrameKey, current.frame_type);
      EXPECT_LE(diff - 1, max_expected_tl0_idx_gap_);
    }
  }

  Action OnSendRtp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);

    ParsedPacket parsed;
    if (!ParsePayload(packet, length, &parsed))
      return SEND_PACKET;

    uint32_t ssrc = parsed.ssrc;
    if (last_observed_packet_.find(ssrc) != last_observed_packet_.end()) {
      // Compare to last packet.
      VerifyPictureId(parsed, last_observed_packet_[ssrc]);
      VerifyTl0Idx(parsed, last_observed_packet_[ssrc]);
    }

    last_observed_packet_[ssrc] = parsed;

    // Pass the test when enough media packets have been received on all
    // streams.
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
  const VideoCodecType codec_type_;
  std::map<uint32_t, ParsedPacket> last_observed_packet_ RTC_GUARDED_BY(crit_);
  std::map<uint32_t, size_t> num_packets_sent_ RTC_GUARDED_BY(crit_);
  int max_expected_picture_id_gap_ RTC_GUARDED_BY(crit_);
  int max_expected_tl0_idx_gap_ RTC_GUARDED_BY(crit_);
  size_t num_ssrcs_to_observe_ RTC_GUARDED_BY(crit_);
  std::set<uint32_t> observed_ssrcs_ RTC_GUARDED_BY(crit_);
};

class PictureIdTest : public test::CallTest,
                      public ::testing::WithParamInterface<size_t> {
 public:
  PictureIdTest() : num_temporal_layers_(GetParam()) {}

  virtual ~PictureIdTest() {
    task_queue_.SendTask([this]() {
      send_transport_.reset();
      receive_transport_.reset();
      DestroyCalls();
    });
  }

  void SetupEncoder(VideoEncoderFactory* encoder_factory,
                    const std::string& payload_name);
  void TestPictureIdContinuousAfterReconfigure(
      const std::vector<int>& ssrc_counts);
  void TestPictureIdIncreaseAfterRecreateStreams(
      const std::vector<int>& ssrc_counts);

 private:
  const size_t num_temporal_layers_;
  std::unique_ptr<PictureIdObserver> observer_;
};

INSTANTIATE_TEST_CASE_P(TemporalLayers,
                        PictureIdTest,
                        ::testing::ValuesIn(kNumTemporalLayers));

// Use a special stream factory to ensure that all simulcast streams are being
// sent.
class VideoStreamFactory
    : public VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  explicit VideoStreamFactory(size_t num_temporal_layers)
      : num_of_temporal_layers_(num_temporal_layers) {}

 private:
  std::vector<VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const VideoEncoderConfig& encoder_config) override {
    std::vector<VideoStream> streams =
        test::CreateVideoStreams(width, height, encoder_config);

    // Use the same total bitrates when sending a single stream to avoid
    // lowering the bitrate estimate and requiring a subsequent rampup.
    const int encoder_stream_bps =
        kEncoderBitrateBps /
        rtc::checked_cast<int>(encoder_config.number_of_streams);

    for (size_t i = 0; i < encoder_config.number_of_streams; ++i) {
      streams[i].min_bitrate_bps = encoder_stream_bps;
      streams[i].target_bitrate_bps = encoder_stream_bps;
      streams[i].max_bitrate_bps = encoder_stream_bps;
      streams[i].num_temporal_layers = num_of_temporal_layers_;
      // test::CreateVideoStreams does not return frame sizes for the lower
      // streams that are accepted by VP8Impl::InitEncode.
      // TODO(brandtr): Fix the problem in test::CreateVideoStreams, rather
      // than overriding the values here.
      streams[i].width =
          width / (1 << (encoder_config.number_of_streams - 1 - i));
      streams[i].height =
          height / (1 << (encoder_config.number_of_streams - 1 - i));
    }

    return streams;
  }

  const size_t num_of_temporal_layers_;
};

void PictureIdTest::SetupEncoder(VideoEncoderFactory* encoder_factory,
                                 const std::string& payload_name) {
  observer_.reset(
      new PictureIdObserver(PayloadStringToCodecType(payload_name)));

  task_queue_.SendTask([this, encoder_factory, payload_name]() {
    CreateCalls();

    send_transport_.reset(new test::PacketTransport(
        &task_queue_, sender_call_.get(), observer_.get(),
        test::PacketTransport::kSender, payload_type_map_,
        absl::make_unique<FakeNetworkPipe>(
            Clock::GetRealTimeClock(), absl::make_unique<SimulatedNetwork>(
                                           DefaultNetworkSimulationConfig()))));

    CreateSendConfig(kNumSimulcastStreams, 0, 0, send_transport_.get());
    GetVideoSendConfig()->encoder_settings.encoder_factory = encoder_factory;
    GetVideoSendConfig()->rtp.payload_name = payload_name;
    GetVideoEncoderConfig()->codec_type =
        PayloadStringToCodecType(payload_name);
    GetVideoEncoderConfig()->video_stream_factory =
        new rtc::RefCountedObject<VideoStreamFactory>(num_temporal_layers_);
    GetVideoEncoderConfig()->number_of_streams = 1;
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

  EXPECT_TRUE(observer_->Wait()) << "Timed out waiting for packets.";

  // Reconfigure VideoEncoder and test picture id increase.
  // Expect continuously increasing picture id, equivalent to no gaps.
  observer_->SetMaxExpectedPictureIdGap(0);
  for (int ssrc_count : ssrc_counts) {
    GetVideoEncoderConfig()->number_of_streams = ssrc_count;
    observer_->SetExpectedSsrcs(ssrc_count);
    observer_->ResetObservedSsrcs();
    // Make sure the picture_id sequence is continuous on reinit and recreate.
    task_queue_.SendTask([this]() {
      GetVideoSendStream()->ReconfigureVideoEncoder(
          GetVideoEncoderConfig()->Copy());
    });
    EXPECT_TRUE(observer_->Wait()) << "Timed out waiting for packets.";
  }

  task_queue_.SendTask([this]() {
    Stop();
    DestroyStreams();
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

  EXPECT_TRUE(observer_->Wait()) << "Timed out waiting for packets.";

  // Recreate VideoSendStream and test picture id increase.
  // When the VideoSendStream is destroyed, any frames still in queue is lost
  // with it, therefore it is expected that some frames might be lost.
  observer_->SetMaxExpectedPictureIdGap(kMaxFramesLost);
  for (int ssrc_count : ssrc_counts) {
    task_queue_.SendTask([this, &ssrc_count]() {
      frame_generator_capturer_->Stop();
      DestroyVideoSendStreams();

      GetVideoEncoderConfig()->number_of_streams = ssrc_count;
      observer_->SetExpectedSsrcs(ssrc_count);
      observer_->ResetObservedSsrcs();

      CreateVideoSendStreams();
      GetVideoSendStream()->Start();
      CreateFrameGeneratorCapturer(kFrameRate, kFrameMaxWidth, kFrameMaxHeight);
      frame_generator_capturer_->Start();
    });

    EXPECT_TRUE(observer_->Wait()) << "Timed out waiting for packets.";
  }

  task_queue_.SendTask([this]() {
    Stop();
    DestroyStreams();
  });
}

TEST_P(PictureIdTest, ContinuousAfterReconfigureVp8) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  SetupEncoder(&encoder_factory, "VP8");
  TestPictureIdContinuousAfterReconfigure({1, 3, 3, 1, 1});
}

TEST_P(PictureIdTest, IncreasingAfterRecreateStreamVp8) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  SetupEncoder(&encoder_factory, "VP8");
  TestPictureIdIncreaseAfterRecreateStreams({1, 3, 3, 1, 1});
}

TEST_P(PictureIdTest, ContinuousAfterStreamCountChangeVp8) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP8Encoder::Create(); });
  // Make sure that the picture id is not reset if the stream count goes
  // down and then up.
  SetupEncoder(&encoder_factory, "VP8");
  TestPictureIdContinuousAfterReconfigure({3, 1, 3});
}

TEST_P(PictureIdTest, ContinuousAfterReconfigureSimulcastEncoderAdapter) {
  InternalEncoderFactory internal_encoder_factory;
  test::FunctionVideoEncoderFactory encoder_factory(
      [&internal_encoder_factory]() {
        return absl::make_unique<SimulcastEncoderAdapter>(
            &internal_encoder_factory, SdpVideoFormat("VP8"));
      });
  SetupEncoder(&encoder_factory, "VP8");
  TestPictureIdContinuousAfterReconfigure({1, 3, 3, 1, 1});
}

TEST_P(PictureIdTest, IncreasingAfterRecreateStreamSimulcastEncoderAdapter) {
  InternalEncoderFactory internal_encoder_factory;
  test::FunctionVideoEncoderFactory encoder_factory(
      [&internal_encoder_factory]() {
        return absl::make_unique<SimulcastEncoderAdapter>(
            &internal_encoder_factory, SdpVideoFormat("VP8"));
      });
  SetupEncoder(&encoder_factory, "VP8");
  TestPictureIdIncreaseAfterRecreateStreams({1, 3, 3, 1, 1});
}

TEST_P(PictureIdTest, ContinuousAfterStreamCountChangeSimulcastEncoderAdapter) {
  InternalEncoderFactory internal_encoder_factory;
  test::FunctionVideoEncoderFactory encoder_factory(
      [&internal_encoder_factory]() {
        return absl::make_unique<SimulcastEncoderAdapter>(
            &internal_encoder_factory, SdpVideoFormat("VP8"));
      });
  // Make sure that the picture id is not reset if the stream count goes
  // down and then up.
  SetupEncoder(&encoder_factory, "VP8");
  TestPictureIdContinuousAfterReconfigure({3, 1, 3});
}

TEST_P(PictureIdTest, IncreasingAfterRecreateStreamVp9) {
  test::FunctionVideoEncoderFactory encoder_factory(
      []() { return VP9Encoder::Create(); });
  SetupEncoder(&encoder_factory, "VP9");
  TestPictureIdIncreaseAfterRecreateStreams({1, 1});
}

}  // namespace webrtc
