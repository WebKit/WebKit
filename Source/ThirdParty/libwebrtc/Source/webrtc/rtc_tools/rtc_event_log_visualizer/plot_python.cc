/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/plot_python.h"

#include <stdio.h>

#include <memory>
#include <string>
#include <vector>

#include "rtc_base/checks.h"

namespace webrtc {

PythonPlot::PythonPlot() {}

PythonPlot::~PythonPlot() {}

void PythonPlot::Draw() {
  PrintPythonCode();
}

PythonPlotCollection::PythonPlotCollection(bool shared_xaxis)
    : shared_xaxis_(shared_xaxis) {}

PythonPlotCollection::~PythonPlotCollection() {}

void PythonPlotCollection::Draw() {
  PrintPythonCode(shared_xaxis_);
}

Plot* PythonPlotCollection::AppendNewPlot() {
  Plot* plot = new PythonPlot();
  plots_.push_back(std::unique_ptr<Plot>(plot));
  return plot;
}

}  // namespace webrtc
