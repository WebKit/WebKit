/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/analyzer_bindings.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "rtc_base/system/file_wrapper.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/rtc_tools/rtc_event_log_visualizer/proto/chart.pb.h"
#else
#include "rtc_tools/rtc_event_log_visualizer/proto/chart.pb.h"
#endif

class RtcEventLogAnalyzerBindingsTest : public ::testing::Test {
  void SetUp() override {
    // Read an RTC event log to a char buffer.
    std::string file_name = webrtc::test::ResourcePath(
        "rtc_event_log/rtc_event_log_500kbps", "binarypb");
    webrtc::FileWrapper file = webrtc::FileWrapper::OpenReadOnly(file_name);
    ASSERT_TRUE(file.is_open());

    std::optional<size_t> file_size = file.FileSize();
    ASSERT_TRUE(file_size.has_value());
    constexpr size_t kMaxFileSize = 1'000'000;
    ASSERT_GT(*file_size, 0u);
    ASSERT_LE(*file_size, kMaxFileSize);

    event_log_contents_.resize(*file_size);
    size_t read_size =
        file.Read(event_log_contents_.data(), event_log_contents_.size());
    ASSERT_EQ(*file_size, read_size);
  }

 protected:
  std::vector<char> event_log_contents_;
};

TEST_F(RtcEventLogAnalyzerBindingsTest, OutgoingBitrateChart) {
  uint32_t kMaxOutputSize = 1'000'000;
  std::vector<char> output(kMaxOutputSize);

  // Call analyzer.
  char selection[] = "outgoing_bitrate";
  size_t selection_size = strlen(selection);
  uint32_t output_size = output.size();
  analyze_rtc_event_log(event_log_contents_.data(), event_log_contents_.size(),
                        selection, selection_size, output.data(), &output_size);
  ASSERT_GT(output_size, 0u);

  // Parse output as charts.
  webrtc::analytics::ChartCollection collection;
  bool success = collection.ParseFromString(
      absl::string_view(output.data(), static_cast<int>(output_size)));
  ASSERT_TRUE(success);
  ASSERT_EQ(collection.charts().size(), 1);
  EXPECT_EQ(collection.charts(0).title(), "Outgoing RTP bitrate");
  EXPECT_EQ(collection.charts(0).id(), "outgoing_bitrate");
}

TEST_F(RtcEventLogAnalyzerBindingsTest, NetWorkDelayFeedbackChart) {
  uint32_t kMaxOutputSize = 1'000'000;
  std::vector<char> output(kMaxOutputSize);

  // Call analyzer.
  char selection[] = "network_delay_feedback";
  size_t selection_size = strlen(selection);
  uint32_t output_size = output.size();
  analyze_rtc_event_log(event_log_contents_.data(), event_log_contents_.size(),
                        selection, selection_size, output.data(), &output_size);
  ASSERT_GT(output_size, 0u);

  // Parse output as charts.
  webrtc::analytics::ChartCollection collection;
  bool success = collection.ParseFromString(
      absl::string_view(output.data(), static_cast<int>(output_size)));
  ASSERT_TRUE(success);
  ASSERT_EQ(collection.charts().size(), 1);
  EXPECT_EQ(collection.charts(0).title(),
            "Outgoing network delay (based on per-packet feedback)");
  EXPECT_EQ(collection.charts(0).id(), "network_delay_feedback");
}

TEST_F(RtcEventLogAnalyzerBindingsTest, OutputbufferTooSmall) {
  uint32_t kMaxOutputSize = 100;
  std::vector<char> output(kMaxOutputSize);

  // Call analyzer.
  char selection[] = "outgoing_bitrate";
  size_t selection_size = strlen(selection);
  uint32_t output_size = output.size();
  analyze_rtc_event_log(event_log_contents_.data(), event_log_contents_.size(),
                        selection, selection_size, output.data(), &output_size);

  // No output since the buffer is too small.
  ASSERT_EQ(output_size, 0u);
}
