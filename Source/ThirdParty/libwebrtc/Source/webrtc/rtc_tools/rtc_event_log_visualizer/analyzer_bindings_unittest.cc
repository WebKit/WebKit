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

#include <memory>
#include <string>
#include <vector>

#include "rtc_base/protobuf_utils.h"
#include "rtc_base/system/file_wrapper.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/rtc_tools/rtc_event_log_visualizer/proto/chart.pb.h"
#else
#include "rtc_tools/rtc_event_log_visualizer/proto/chart.pb.h"
#endif

TEST(RtcEventLogAnalyzerBindingsTest, ProducesCharts) {
  constexpr int kInputBufferSize = 1'000'000;
  constexpr int kOutputBufferSize = 1'000'000;
  std::unique_ptr<char[]> input = std::make_unique<char[]>(kInputBufferSize);
  std::unique_ptr<char[]> output = std::make_unique<char[]>(kOutputBufferSize);

  // Read an RTC event log to a char buffer.
  std::string file_name = webrtc::test::ResourcePath(
      "rtc_event_log/rtc_event_log_500kbps", "binarypb");
  webrtc::FileWrapper file = webrtc::FileWrapper::OpenReadOnly(file_name);
  ASSERT_TRUE(file.is_open());
  int64_t file_size = file.FileSize();
  ASSERT_LE(file_size, kInputBufferSize);
  ASSERT_GT(file_size, 0);
  size_t input_size = file.Read(input.get(), static_cast<size_t>(file_size));
  ASSERT_EQ(static_cast<size_t>(file_size), input_size);

  // Call analyzer.
  uint32_t output_size = kOutputBufferSize;
  char selection[] = "outgoing_bitrate,network_delay_feedback";
  size_t selection_size = strlen(selection);
  analyze_rtc_event_log(input.get(), input_size, selection, selection_size,
                        output.get(), &output_size);
  ASSERT_GT(output_size, 0u);

  // Parse output as charts.
  webrtc::analytics::ChartCollection collection;
  bool success =
      collection.ParseFromArray(output.get(), static_cast<int>(output_size));
  ASSERT_TRUE(success);
  EXPECT_EQ(collection.charts().size(), 2);
  std::vector<std::string> chart_titles;
  for (const auto& chart : collection.charts()) {
    chart_titles.push_back(chart.title());
  }
  EXPECT_THAT(chart_titles,
              ::testing::UnorderedElementsAre(
                  "Outgoing RTP bitrate",
                  "Outgoing network delay (based on per-packet feedback)"));
  std::vector<std::string> chart_ids;
  for (const auto& chart : collection.charts()) {
    chart_ids.push_back(chart.id());
  }
  EXPECT_THAT(chart_ids, ::testing::UnorderedElementsAre(
                             "outgoing_bitrate", "network_delay_feedback"));
}
