/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_PYTHON_H_
#define WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_PYTHON_H_

#include "webrtc/tools/event_log_visualizer/plot_base.h"

namespace webrtc {
namespace plotting {

class PythonPlot final : public Plot {
 public:
  PythonPlot();
  ~PythonPlot() override;
  void Draw() override;
};

class PythonPlotCollection final : public PlotCollection {
 public:
  PythonPlotCollection();
  ~PythonPlotCollection() override;
  void Draw() override;
  Plot* AppendNewPlot() override;
};

}  // namespace plotting
}  // namespace webrtc

#endif  // WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_PYTHON_H_
