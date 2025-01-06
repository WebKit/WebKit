
/*
 *  Copyright 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "api/environment/environment.h"
#include "api/rtp_parameters.h"
#include "api/task_queue/task_queue_base.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "rtc_base/checks.h"
#include "rtc_base/task_queue_for_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/gtest.h"
#include "test/video_test_constants.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {
namespace {
RtpExtension GetCorruptionExtension() {
  return RtpExtension(RtpExtension::kCorruptionDetectionUri,
                      /*extension_id=*/1,
                      /*encrypted=*/true);
}
}  // namespace

class CorruptionDetectionTest : public test::CallTest {
 public:
  CorruptionDetectionTest() { RegisterRtpExtension(GetCorruptionExtension()); }
};

TEST_F(
    CorruptionDetectionTest,
    ReportsCorruptionStatsIfSendStreamIsConfiguredToEnableCorruptionDetection) {
  class StatsObserver : public test::EndToEndTest {
   public:
    StatsObserver()
        : EndToEndTest(test::VideoTestConstants::kLongTimeout),
          encoder_factory_(
              [](const Environment& env, const SdpVideoFormat& format) {
                return CreateVp8Encoder(env);
              }),
          decoder_factory_(
              [](const Environment& env, const SdpVideoFormat& format) {
                return CreateVp8Decoder(env);
              }) {}

   private:
    void ModifyVideoConfigs(
        VideoSendStream::Config* send_config,
        std::vector<VideoReceiveStreamInterface::Config>* receive_configs,
        VideoEncoderConfig* encoder_config) override {
      encoder_config->codec_type = kVideoCodecVP8;
      send_config->encoder_settings.enable_frame_instrumentation_generator =
          true;
      send_config->encoder_settings.encoder_factory = &encoder_factory_;
      send_config->rtp.payload_name = "VP8";
      send_config->rtp.extensions.clear();
      send_config->rtp.extensions.push_back(GetCorruptionExtension());

      for (auto& receive_config : *receive_configs) {
        receive_config.decoder_factory = &decoder_factory_;
        RTC_CHECK(!receive_config.decoders.empty());
        receive_config.decoders[0].video_format =
            SdpVideoFormat(send_config->rtp.payload_name);
      }
    }

    void OnVideoStreamsCreated(VideoSendStream* send_stream,
                               const std::vector<VideoReceiveStreamInterface*>&
                                   receive_streams) override {
      receive_streams_ = receive_streams;
      task_queue_ = TaskQueueBase::Current();
    }

    void PerformTest() override {
      constexpr int kMaxIterations = 200;
      bool corruption_score_reported = false;
      for (int i = 0; i < kMaxIterations; ++i) {
        SleepMs(10);
        VideoReceiveStreamInterface::Stats stats;
        SendTask(task_queue_, [&]() {
          ASSERT_EQ(receive_streams_.size(), 1u);
          stats = receive_streams_[0]->GetStats();
        });
        if (stats.corruption_score_count > 0) {
          corruption_score_reported = true;
          ASSERT_TRUE(stats.corruption_score_sum.has_value());
          EXPECT_TRUE(stats.corruption_score_squared_sum.has_value());
          double average_score =
              *stats.corruption_score_sum / stats.corruption_score_count;
          EXPECT_GE(average_score, 0);
          EXPECT_LE(average_score, 1);
          break;
        }
      }
      EXPECT_TRUE(corruption_score_reported);
    }

    std::vector<VideoReceiveStreamInterface*> receive_streams_;
    TaskQueueBase* task_queue_ = nullptr;
    test::FunctionVideoEncoderFactory encoder_factory_;
    test::FunctionVideoDecoderFactory decoder_factory_;
  } test;

  RunBaseTest(&test);
}

}  // namespace webrtc
