/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_PROTOBUF_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_PROTOBUF_H_

#include "rtc_base/ignore_wundef.h"
RTC_PUSH_IGNORING_WUNDEF()
#include "rtc_tools/event_log_visualizer/chart.pb.h"
RTC_POP_IGNORING_WUNDEF()
#include "rtc_tools/event_log_visualizer/plot_base.h"

namespace webrtc {
namespace plotting {

class ProtobufPlot final : public Plot {
 public:
  ProtobufPlot();
  ~ProtobufPlot() override;
  void Draw() override;
  void ExportProtobuf(webrtc::analytics::Chart* chart);
};

class ProtobufPlotCollection final : public PlotCollection {
 public:
  ProtobufPlotCollection();
  ~ProtobufPlotCollection() override;
  void Draw() override;
  Plot* AppendNewPlot() override;
  void ExportProtobuf(webrtc::analytics::ChartCollection* collection);
};

}  // namespace plotting
}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_PROTOBUF_H_
