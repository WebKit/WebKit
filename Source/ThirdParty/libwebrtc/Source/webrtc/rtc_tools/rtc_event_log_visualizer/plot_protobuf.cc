/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/plot_protobuf.h"

#include <stddef.h>

#include <iostream>
#include <memory>
#include <vector>

namespace webrtc {

ProtobufPlot::ProtobufPlot() {}

ProtobufPlot::~ProtobufPlot() {}

void ProtobufPlot::Draw() {}

ProtobufPlotCollection::ProtobufPlotCollection() {}

ProtobufPlotCollection::~ProtobufPlotCollection() {}

void ProtobufPlotCollection::Draw() {
  webrtc::analytics::ChartCollection collection;
  ExportProtobuf(&collection);
  std::cout << collection.SerializeAsString();
}

Plot* ProtobufPlotCollection::AppendNewPlot() {
  Plot* plot = new ProtobufPlot();
  plots_.push_back(std::unique_ptr<Plot>(plot));
  return plot;
}

}  // namespace webrtc
