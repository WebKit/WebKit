/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/bandwidth_quality_scaler.h"

#include <memory>
#include <string>

#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/experiments/encoder_info_settings.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {

namespace {
constexpr int kFramerateFps = 30;
constexpr TimeDelta kDefaultEncodeTime = TimeDelta::Seconds(1) / kFramerateFps;
constexpr TimeDelta kWaitTime = TimeDelta::Millis(200);

}  // namespace

class FakeBandwidthQualityScalerHandler
    : public BandwidthQualityScalerUsageHandlerInterface {
 public:
  ~FakeBandwidthQualityScalerHandler() override = default;
  void OnReportUsageBandwidthHigh() override {
    adapt_down_event_count_++;
    event_.Set();
  }

  void OnReportUsageBandwidthLow() override {
    adapt_up_event_count_++;
    event_.Set();
  }

  rtc::Event event_;
  int adapt_up_event_count_ = 0;
  int adapt_down_event_count_ = 0;
};

class BandwidthQualityScalerTest : public ::testing::Test {
 protected:
  enum ScaleDirection {
    kKeepScaleNormalBandwidth,
    kKeepScaleAboveMaxBandwidth,
    kKeepScaleUnderMinBandwidth,
  };

  enum FrameType {
    kKeyFrame,
    kNormalFrame,
    kNormalFrame_Overuse,
    kNormalFrame_Underuse,
  };
  struct FrameConfig {
    FrameConfig(int frame_num,
                FrameType frame_type,
                int actual_width,
                int actual_height)
        : frame_num(frame_num),
          frame_type(frame_type),
          actual_width(actual_width),
          actual_height(actual_height) {}

    int frame_num;
    FrameType frame_type;
    int actual_width;
    int actual_height;
  };

  BandwidthQualityScalerTest()
      : task_queue_(time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
            "BandwidthQualityScalerTestQueue",
            TaskQueueFactory::Priority::NORMAL)),
        handler_(std::make_unique<FakeBandwidthQualityScalerHandler>()) {
    task_queue_.SendTask([this] {
      bandwidth_quality_scaler_ =
          std::make_unique<BandwidthQualityScaler>(handler_.get());
      bandwidth_quality_scaler_->SetResolutionBitrateLimits(
          EncoderInfoSettings::
              GetDefaultSinglecastBitrateLimitsWhenQpIsUntrusted());
      // Only for testing. Set first_timestamp_ in RateStatistics to 0.
      bandwidth_quality_scaler_->ReportEncodeInfo(0, 0, 0, 0);
    });
  }

  ~BandwidthQualityScalerTest() {
    task_queue_.SendTask([this] { bandwidth_quality_scaler_ = nullptr; });
  }

  int GetFrameSizeBytes(
      const FrameConfig& config,
      const VideoEncoder::ResolutionBitrateLimits& bitrate_limits) {
    int scale = 8 * kFramerateFps;
    switch (config.frame_type) {
      case FrameType::kKeyFrame: {
        // 4 is experimental value. Based on the test, the number of bytes of
        // the key frame is about four times of the normal frame
        return bitrate_limits.max_bitrate_bps * 4 / scale;
      }
      case FrameType::kNormalFrame_Overuse: {
        return bitrate_limits.max_bitrate_bps * 3 / 2 / scale;
      }
      case FrameType::kNormalFrame_Underuse: {
        return bitrate_limits.min_start_bitrate_bps * 3 / 4 / scale;
      }
      case FrameType::kNormalFrame: {
        return (bitrate_limits.max_bitrate_bps +
                bitrate_limits.min_start_bitrate_bps) /
               2 / scale;
      }
    }
    return -1;
  }

  std::optional<VideoEncoder::ResolutionBitrateLimits>
  GetDefaultSuitableBitrateLimit(int frame_size_pixels) {
    return EncoderInfoSettings::
        GetSinglecastBitrateLimitForResolutionWhenQpIsUntrusted(
            frame_size_pixels,
            EncoderInfoSettings::
                GetDefaultSinglecastBitrateLimitsWhenQpIsUntrusted());
  }

  void TriggerBandwidthQualityScalerTest(
      const std::vector<FrameConfig>& frame_configs) {
    RTC_CHECK(!frame_configs.empty());

    int total_frame_nums = 0;
    for (const FrameConfig& frame_config : frame_configs) {
      total_frame_nums += frame_config.frame_num;
    }

    EXPECT_EQ(kFramerateFps *
                  BandwidthQualityScaler::kBitrateStateUpdateInterval.seconds(),
              total_frame_nums);

    TimeDelta delay = TimeDelta::Zero();
    int num_delayed_tasks = 0;
    for (const FrameConfig& config : frame_configs) {
      std::optional<VideoEncoder::ResolutionBitrateLimits> suitable_bitrate =
          GetDefaultSuitableBitrateLimit(config.actual_width *
                                         config.actual_height);
      EXPECT_TRUE(suitable_bitrate);
      for (int j = 0; j <= config.frame_num; ++j) {
        delay += kDefaultEncodeTime;
        int frame_size_bytes = GetFrameSizeBytes(config, *suitable_bitrate);
        RTC_CHECK_GT(frame_size_bytes, 0);
        ++num_delayed_tasks;
        task_queue_.PostDelayedTask(
            [frame_size_bytes, config, &num_delayed_tasks, this] {
              bandwidth_quality_scaler_->ReportEncodeInfo(
                  frame_size_bytes,
                  time_controller_.GetClock()->CurrentTime().ms(),
                  config.actual_width, config.actual_height);
              --num_delayed_tasks;
            },
            delay);
      }
    }
    time_controller_.AdvanceTime(delay);
    ASSERT_TRUE(time_controller_.Wait([&] { return num_delayed_tasks == 0; }));
  }

  GlobalSimulatedTimeController time_controller_{Timestamp::Seconds(1234)};
  TaskQueueForTest task_queue_;
  std::unique_ptr<BandwidthQualityScaler> bandwidth_quality_scaler_;
  std::unique_ptr<FakeBandwidthQualityScalerHandler> handler_;
};

TEST_F(BandwidthQualityScalerTest, AllNormalFrame_640x360) {
  const std::vector<FrameConfig> frame_configs{
      FrameConfig(150, FrameType::kNormalFrame, 640, 360)};
  TriggerBandwidthQualityScalerTest(frame_configs);

  // When resolution is 640*360, experimental working bitrate range is
  // [500000,800000] bps. Encoded bitrate is 654253, so it falls in the range
  // without any operation(up/down).
  EXPECT_FALSE(handler_->event_.Wait(kWaitTime));
  EXPECT_EQ(0, handler_->adapt_down_event_count_);
  EXPECT_EQ(0, handler_->adapt_up_event_count_);
}

TEST_F(BandwidthQualityScalerTest, AllNormalFrame_AboveMaxBandwidth_640x360) {
  const std::vector<FrameConfig> frame_configs{
      FrameConfig(150, FrameType::kNormalFrame_Overuse, 640, 360)};
  TriggerBandwidthQualityScalerTest(frame_configs);

  // When resolution is 640*360, experimental working bitrate range is
  // [500000,800000] bps. Encoded bitrate is 1208000 > 800000 * 0.95, so it
  // triggers adapt_up_event_count_.
  EXPECT_TRUE(handler_->event_.Wait(kWaitTime));
  EXPECT_EQ(0, handler_->adapt_down_event_count_);
  EXPECT_EQ(1, handler_->adapt_up_event_count_);
}

TEST_F(BandwidthQualityScalerTest, AllNormalFrame_Underuse_640x360) {
  const std::vector<FrameConfig> frame_configs{
      FrameConfig(150, FrameType::kNormalFrame_Underuse, 640, 360)};
  TriggerBandwidthQualityScalerTest(frame_configs);

  // When resolution is 640*360, experimental working bitrate range is
  // [500000,800000] bps. Encoded bitrate is 377379 < 500000 * 0.8, so it
  // triggers adapt_down_event_count_.
  EXPECT_TRUE(handler_->event_.Wait(kWaitTime));
  EXPECT_EQ(1, handler_->adapt_down_event_count_);
  EXPECT_EQ(0, handler_->adapt_up_event_count_);
}

TEST_F(BandwidthQualityScalerTest, FixedFrameTypeTest1_640x360) {
  const std::vector<FrameConfig> frame_configs{
      FrameConfig(5, FrameType::kNormalFrame_Underuse, 640, 360),
      FrameConfig(110, FrameType::kNormalFrame, 640, 360),
      FrameConfig(20, FrameType::kNormalFrame_Overuse, 640, 360),
      FrameConfig(15, FrameType::kKeyFrame, 640, 360),
  };
  TriggerBandwidthQualityScalerTest(frame_configs);

  // When resolution is 640*360, experimental working bitrate range is
  // [500000,800000] bps. Encoded bitrate is 1059462 > 800000 * 0.95, so it
  // triggers adapt_up_event_count_.
  EXPECT_TRUE(handler_->event_.Wait(kWaitTime));
  EXPECT_EQ(0, handler_->adapt_down_event_count_);
  EXPECT_EQ(1, handler_->adapt_up_event_count_);
}

TEST_F(BandwidthQualityScalerTest, FixedFrameTypeTest2_640x360) {
  const std::vector<FrameConfig> frame_configs{
      FrameConfig(10, FrameType::kNormalFrame_Underuse, 640, 360),
      FrameConfig(50, FrameType::kNormalFrame, 640, 360),
      FrameConfig(5, FrameType::kKeyFrame, 640, 360),
      FrameConfig(85, FrameType::kNormalFrame_Overuse, 640, 360),
  };
  TriggerBandwidthQualityScalerTest(frame_configs);

  // When resolution is 640*360, experimental working bitrate range is
  // [500000,800000] bps. Encoded bitrate is 1059462 > 800000 * 0.95, so it
  // triggers adapt_up_event_count_.
  EXPECT_TRUE(handler_->event_.Wait(kWaitTime));
  EXPECT_EQ(0, handler_->adapt_down_event_count_);
  EXPECT_EQ(1, handler_->adapt_up_event_count_);
}

}  // namespace webrtc
