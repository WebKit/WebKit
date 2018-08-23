/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/call_test.h"
#include "test/gtest.h"
#include "test/rtcp_packet_parser.h"

namespace webrtc {

class ExtendedReportsEndToEndTest : public test::CallTest {};

class RtcpXrObserver : public test::EndToEndTest {
 public:
  RtcpXrObserver(bool enable_rrtr,
                 bool enable_target_bitrate,
                 bool enable_zero_target_bitrate)
      : EndToEndTest(test::CallTest::kDefaultTimeoutMs),
        enable_rrtr_(enable_rrtr),
        enable_target_bitrate_(enable_target_bitrate),
        enable_zero_target_bitrate_(enable_zero_target_bitrate),
        sent_rtcp_sr_(0),
        sent_rtcp_rr_(0),
        sent_rtcp_rrtr_(0),
        sent_rtcp_target_bitrate_(false),
        sent_zero_rtcp_target_bitrate_(false),
        sent_rtcp_dlrr_(0) {}

 private:
  // Receive stream should send RR packets (and RRTR packets if enabled).
  Action OnReceiveRtcp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(packet, length));

    sent_rtcp_rr_ += parser.receiver_report()->num_packets();
    EXPECT_EQ(0, parser.sender_report()->num_packets());
    EXPECT_GE(1, parser.xr()->num_packets());
    if (parser.xr()->num_packets() > 0) {
      if (parser.xr()->rrtr())
        ++sent_rtcp_rrtr_;
      EXPECT_FALSE(parser.xr()->dlrr());
    }

    return SEND_PACKET;
  }
  // Send stream should send SR packets (and DLRR packets if enabled).
  Action OnSendRtcp(const uint8_t* packet, size_t length) override {
    rtc::CritScope lock(&crit_);
    test::RtcpPacketParser parser;
    EXPECT_TRUE(parser.Parse(packet, length));

    sent_rtcp_sr_ += parser.sender_report()->num_packets();
    EXPECT_LE(parser.xr()->num_packets(), 1);
    if (parser.xr()->num_packets() > 0) {
      EXPECT_FALSE(parser.xr()->rrtr());
      if (parser.xr()->dlrr())
        ++sent_rtcp_dlrr_;
      if (parser.xr()->target_bitrate()) {
        sent_rtcp_target_bitrate_ = true;
        for (const rtcp::TargetBitrate::BitrateItem& item :
             parser.xr()->target_bitrate()->GetTargetBitrates()) {
          if (item.target_bitrate_kbps == 0) {
            sent_zero_rtcp_target_bitrate_ = true;
            break;
          }
        }
      }
    }

    if (sent_rtcp_sr_ > kNumRtcpReportPacketsToObserve &&
        sent_rtcp_rr_ > kNumRtcpReportPacketsToObserve &&
        (sent_rtcp_target_bitrate_ || !enable_target_bitrate_) &&
        (sent_zero_rtcp_target_bitrate_ || !enable_zero_target_bitrate_)) {
      if (enable_rrtr_) {
        EXPECT_GT(sent_rtcp_rrtr_, 0);
        EXPECT_GT(sent_rtcp_dlrr_, 0);
      } else {
        EXPECT_EQ(sent_rtcp_rrtr_, 0);
        EXPECT_EQ(sent_rtcp_dlrr_, 0);
      }
      EXPECT_EQ(enable_target_bitrate_, sent_rtcp_target_bitrate_);
      EXPECT_EQ(enable_zero_target_bitrate_, sent_zero_rtcp_target_bitrate_);
      observation_complete_.Set();
    }
    return SEND_PACKET;
  }

  size_t GetNumVideoStreams() const override {
    // When sending a zero target bitrate, we use two spatial layers so that
    // we'll still have a layer with non-zero bitrate.
    return enable_zero_target_bitrate_ ? 2 : 1;
  }

  // This test uses VideoStream settings different from the the default one
  // implemented in DefaultVideoStreamFactory, so it implements its own
  // VideoEncoderConfig::VideoStreamFactoryInterface which is created
  // in ModifyVideoConfigs.
  class ZeroTargetVideoStreamFactory
      : public VideoEncoderConfig::VideoStreamFactoryInterface {
   public:
    ZeroTargetVideoStreamFactory() {}

   private:
    std::vector<VideoStream> CreateEncoderStreams(
        int width,
        int height,
        const VideoEncoderConfig& encoder_config) override {
      std::vector<VideoStream> streams =
          test::CreateVideoStreams(width, height, encoder_config);
      // Set one of the streams' target bitrates to zero to test that a
      // bitrate of 0 can be signalled.
      streams[encoder_config.number_of_streams - 1].min_bitrate_bps = 0;
      streams[encoder_config.number_of_streams - 1].target_bitrate_bps = 0;
      streams[encoder_config.number_of_streams - 1].max_bitrate_bps = 0;
      return streams;
    }
  };

  void ModifyVideoConfigs(
      VideoSendStream::Config* send_config,
      std::vector<VideoReceiveStream::Config>* receive_configs,
      VideoEncoderConfig* encoder_config) override {
    if (enable_zero_target_bitrate_) {
      encoder_config->video_stream_factory =
          new rtc::RefCountedObject<ZeroTargetVideoStreamFactory>();

      // Configure VP8 to be able to use simulcast.
      send_config->rtp.payload_name = "VP8";
      encoder_config->codec_type = kVideoCodecVP8;
      (*receive_configs)[0].decoders.resize(1);
      (*receive_configs)[0].decoders[0].payload_type =
          send_config->rtp.payload_type;
      (*receive_configs)[0].decoders[0].payload_name =
          send_config->rtp.payload_name;
    }
    if (enable_target_bitrate_) {
      // TargetBitrate only signaled for screensharing.
      encoder_config->content_type = VideoEncoderConfig::ContentType::kScreen;
    }
    (*receive_configs)[0].rtp.rtcp_mode = RtcpMode::kReducedSize;
    (*receive_configs)[0].rtp.rtcp_xr.receiver_reference_time_report =
        enable_rrtr_;
  }

  void PerformTest() override {
    EXPECT_TRUE(Wait())
        << "Timed out while waiting for RTCP SR/RR packets to be sent.";
  }

  static const int kNumRtcpReportPacketsToObserve = 5;

  rtc::CriticalSection crit_;
  const bool enable_rrtr_;
  const bool enable_target_bitrate_;
  const bool enable_zero_target_bitrate_;
  int sent_rtcp_sr_;
  int sent_rtcp_rr_ RTC_GUARDED_BY(&crit_);
  int sent_rtcp_rrtr_ RTC_GUARDED_BY(&crit_);
  bool sent_rtcp_target_bitrate_ RTC_GUARDED_BY(&crit_);
  bool sent_zero_rtcp_target_bitrate_ RTC_GUARDED_BY(&crit_);
  int sent_rtcp_dlrr_;
};

TEST_F(ExtendedReportsEndToEndTest,
       TestExtendedReportsWithRrtrWithoutTargetBitrate) {
  RtcpXrObserver test(/*enable_rrtr=*/true, /*enable_target_bitrate=*/false,
                      /*enable_zero_target_bitrate=*/false);
  RunBaseTest(&test);
}

TEST_F(ExtendedReportsEndToEndTest,
       TestExtendedReportsWithoutRrtrWithoutTargetBitrate) {
  RtcpXrObserver test(/*enable_rrtr=*/false, /*enable_target_bitrate=*/false,
                      /*enable_zero_target_bitrate=*/false);
  RunBaseTest(&test);
}

TEST_F(ExtendedReportsEndToEndTest,
       TestExtendedReportsWithRrtrWithTargetBitrate) {
  RtcpXrObserver test(/*enable_rrtr=*/true, /*enable_target_bitrate=*/true,
                      /*enable_zero_target_bitrate=*/false);
  RunBaseTest(&test);
}

TEST_F(ExtendedReportsEndToEndTest,
       TestExtendedReportsWithoutRrtrWithTargetBitrate) {
  RtcpXrObserver test(/*enable_rrtr=*/false, /*enable_target_bitrate=*/true,
                      /*enable_zero_target_bitrate=*/false);
  RunBaseTest(&test);
}

TEST_F(ExtendedReportsEndToEndTest,
       TestExtendedReportsCanSignalZeroTargetBitrate) {
  RtcpXrObserver test(/*enable_rrtr=*/false, /*enable_target_bitrate=*/true,
                      /*enable_zero_target_bitrate=*/true);
  RunBaseTest(&test);
}
}  // namespace webrtc
