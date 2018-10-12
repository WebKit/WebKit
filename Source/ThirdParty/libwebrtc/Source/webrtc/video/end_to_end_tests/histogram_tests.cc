/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "absl/types/optional.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "system_wrappers/include/metrics.h"
#include "test/call_test.h"
#include "test/function_video_encoder_factory.h"
#include "test/gtest.h"

namespace webrtc {

class HistogramTest : public test::CallTest {
 protected:
  void VerifyHistogramStats(bool use_rtx, bool use_fec, bool screenshare);
};

void HistogramTest::VerifyHistogramStats(bool use_rtx,
                                         bool use_fec,
                                         bool screenshare) {
  class FrameObserver : public test::EndToEndTest,
                        public rtc::VideoSinkInterface<VideoFrame> {
   public:
    FrameObserver(bool use_rtx, bool use_fec, bool screenshare)
        : EndToEndTest(kLongTimeoutMs),
          use_rtx_(use_rtx),
          use_fec_(use_fec),
          screenshare_(screenshare),
          // This test uses NACK, so to send FEC we can't use a fake encoder.
          encoder_factory_([]() { return VP8Encoder::Create(); }),
          num_frames_received_(0) {}

   private:
    void OnFrame(const VideoFrame& video_frame) override {
      // The RTT is needed to estimate |ntp_time_ms| which is used by
      // end-to-end delay stats. Therefore, start counting received frames once
      // |ntp_time_ms| is valid.
      if (video_frame.ntp_time_ms() > 0 &&
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds() >=
              video_frame.ntp_time_ms()) {
        rtc::CritScope lock(&crit_);
        ++num_frames_received_;
      }
    }

    Action OnSendRtp(const uint8_t* packet, size_t length) override {
      if (MinMetricRunTimePassed() && MinNumberOfFramesReceived())
        observation_complete_.Set();

      return SEND_PACKET;
    }

    bool MinMetricRunTimePassed() {
      int64_t now_ms = Clock::GetRealTimeClock()->TimeInMilliseconds();
      if (!start_runtime_ms_)
        start_runtime_ms_ = now_ms;

      int64_t elapsed_sec = (now_ms - *start_runtime_ms_) / 1000;
      return elapsed_sec > metrics::kMinRunTimeInSeconds * 2;
    }

    bool MinNumberOfFramesReceived() const {
      const int kMinRequiredHistogramSamples = 200;
      rtc::CritScope lock(&crit_);
      return num_frames_received_ > kMinRequiredHistogramSamples;
    }

    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStream::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      // NACK
      send_config->rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
      (*receive_configs)[0].renderer = this;
      // FEC
      if (use_fec_) {
        send_config->rtp.ulpfec.ulpfec_payload_type = kUlpfecPayloadType;
        send_config->rtp.ulpfec.red_payload_type = kRedPayloadType;
        send_config->encoder_settings.encoder_factory = &encoder_factory_;
        send_config->rtp.payload_name = "VP8";
        encoder_config->codec_type = kVideoCodecVP8;
        (*receive_configs)[0].decoders[0].video_format = SdpVideoFormat("VP8");
        (*receive_configs)[0].rtp.red_payload_type = kRedPayloadType;
        (*receive_configs)[0].rtp.ulpfec_payload_type = kUlpfecPayloadType;
      }
      // RTX
      if (use_rtx_) {
        send_config->rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[0]);
        send_config->rtp.rtx.payload_type = kSendRtxPayloadType;
        (*receive_configs)[0].rtp.rtx_ssrc = kSendRtxSsrcs[0];
        (*receive_configs)[0]
            .rtp.rtx_associated_payload_types[kSendRtxPayloadType] =
            kFakeVideoSendPayloadType;
        if (use_fec_) {
          send_config->rtp.ulpfec.red_rtx_payload_type = kRtxRedPayloadType;
          (*receive_configs)[0]
              .rtp.rtx_associated_payload_types[kRtxRedPayloadType] =
              kSendRtxPayloadType;
        }
      }
      // RTT needed for RemoteNtpTimeEstimator for the receive stream.
      (*receive_configs)[0].rtp.rtcp_xr.receiver_reference_time_report = true;
      encoder_config->content_type =
          screenshare_ ? VideoEncoderConfig::ContentType::kScreen
                       : VideoEncoderConfig::ContentType::kRealtimeVideo;
    }

    void PerformTest() override {
      EXPECT_TRUE(Wait()) << "Timed out waiting for min frames to be received.";
    }

    rtc::CriticalSection crit_;
    const bool use_rtx_;
    const bool use_fec_;
    const bool screenshare_;
    test::FunctionVideoEncoderFactory encoder_factory_;
    absl::optional<int64_t> start_runtime_ms_;
    int num_frames_received_ RTC_GUARDED_BY(&crit_);
  } test(use_rtx, use_fec, screenshare);

  metrics::Reset();
  RunBaseTest(&test);

  const std::string video_prefix =
      screenshare ? "WebRTC.Video.Screenshare." : "WebRTC.Video.";
  // The content type extension is disabled in non screenshare test,
  // therefore no slicing on simulcast id should be present.
  const std::string video_suffix = screenshare ? ".S0" : "";

  // Verify that stats have been updated once.
  EXPECT_EQ(2, metrics::NumSamples("WebRTC.Call.LifetimeInSeconds"));
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Call.TimeReceivingVideoRtpPacketsInSeconds"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Call.VideoBitrateReceivedInKbps"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Call.RtcpBitrateReceivedInBps"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Call.BitrateReceivedInKbps"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Call.EstimatedSendBitrateInKbps"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Call.PacerBitrateInKbps"));

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SendStreamLifetimeInSeconds"));
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.ReceiveStreamLifetimeInSeconds"));

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.NackPacketsSentPerMinute"));
  EXPECT_EQ(1,
            metrics::NumSamples(video_prefix + "NackPacketsReceivedPerMinute"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.FirPacketsSentPerMinute"));
  EXPECT_EQ(1,
            metrics::NumSamples(video_prefix + "FirPacketsReceivedPerMinute"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.PliPacketsSentPerMinute"));
  EXPECT_EQ(1,
            metrics::NumSamples(video_prefix + "PliPacketsReceivedPerMinute"));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "KeyFramesSentInPermille"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.KeyFramesReceivedInPermille"));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "SentPacketsLostInPercent"));
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.ReceivedPacketsLostInPercent"));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "InputWidthInPixels"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "InputHeightInPixels"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "SentWidthInPixels"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "SentHeightInPixels"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "ReceivedWidthInPixels"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "ReceivedHeightInPixels"));

  EXPECT_EQ(1, metrics::NumEvents(video_prefix + "InputWidthInPixels",
                                  kDefaultWidth));
  EXPECT_EQ(1, metrics::NumEvents(video_prefix + "InputHeightInPixels",
                                  kDefaultHeight));
  EXPECT_EQ(
      1, metrics::NumEvents(video_prefix + "SentWidthInPixels", kDefaultWidth));
  EXPECT_EQ(1, metrics::NumEvents(video_prefix + "SentHeightInPixels",
                                  kDefaultHeight));
  EXPECT_EQ(1, metrics::NumEvents(video_prefix + "ReceivedWidthInPixels",
                                  kDefaultWidth));
  EXPECT_EQ(1, metrics::NumEvents(video_prefix + "ReceivedHeightInPixels",
                                  kDefaultHeight));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "InputFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "SentFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.DecodedFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.RenderFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.DelayedFramesToRenderer"));

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.JitterBufferDelayInMs"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.TargetDelayInMs"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.CurrentDelayInMs"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.OnewayDelayInMs"));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "EndToEndDelayInMs" +
                                   video_suffix));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "EndToEndDelayMaxInMs" +
                                   video_suffix));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "InterframeDelayInMs" +
                                   video_suffix));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "InterframeDelayMaxInMs" +
                                   video_suffix));

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.RenderSqrtPixelsPerSecond"));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "EncodeTimeInMs"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.DecodeTimeInMs"));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "NumberOfPauseEvents"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "PausedTimeInPercent"));

  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "BitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.BitrateReceivedInKbps"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "MediaBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.MediaBitrateReceivedInKbps"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "PaddingBitrateSentInKbps"));
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.PaddingBitrateReceivedInKbps"));
  EXPECT_EQ(
      1, metrics::NumSamples(video_prefix + "RetransmittedBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.RetransmittedBitrateReceivedInKbps"));

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SendDelayInMs"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "SendSideDelayInMs"));
  EXPECT_EQ(1, metrics::NumSamples(video_prefix + "SendSideDelayMaxInMs"));

  int num_rtx_samples = use_rtx ? 1 : 0;
  EXPECT_EQ(num_rtx_samples,
            metrics::NumSamples("WebRTC.Video.RtxBitrateSentInKbps"));
  EXPECT_EQ(num_rtx_samples,
            metrics::NumSamples("WebRTC.Video.RtxBitrateReceivedInKbps"));

  int num_red_samples = use_fec ? 1 : 0;
  EXPECT_EQ(num_red_samples,
            metrics::NumSamples("WebRTC.Video.FecBitrateSentInKbps"));
  EXPECT_EQ(num_red_samples,
            metrics::NumSamples("WebRTC.Video.FecBitrateReceivedInKbps"));
  EXPECT_EQ(num_red_samples,
            metrics::NumSamples("WebRTC.Video.ReceivedFecPacketsInPercent"));
}

TEST_F(HistogramTest, VerifyStatsWithRtx) {
  const bool kEnabledRtx = true;
  const bool kEnabledRed = false;
  const bool kScreenshare = false;
  VerifyHistogramStats(kEnabledRtx, kEnabledRed, kScreenshare);
}

TEST_F(HistogramTest, VerifyStatsWithRed) {
  const bool kEnabledRtx = false;
  const bool kEnabledRed = true;
  const bool kScreenshare = false;
  VerifyHistogramStats(kEnabledRtx, kEnabledRed, kScreenshare);
}

TEST_F(HistogramTest, VerifyStatsWithScreenshare) {
  const bool kEnabledRtx = false;
  const bool kEnabledRed = false;
  const bool kScreenshare = true;
  VerifyHistogramStats(kEnabledRtx, kEnabledRed, kScreenshare);
}

}  // namespace webrtc
