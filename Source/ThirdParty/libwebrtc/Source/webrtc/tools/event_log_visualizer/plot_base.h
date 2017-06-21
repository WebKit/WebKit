/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_BASE_H_
#define WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_BASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace webrtc {
namespace plotting {

enum PlotStyle {
  LINE_GRAPH,
  LINE_DOT_GRAPH,
  BAR_GRAPH,
  LINE_STEP_GRAPH,
  DOT_GRAPH
};

struct TimeSeriesPoint {
  TimeSeriesPoint(float x, float y) : x(x), y(y) {}
  float x;
  float y;
};

struct TimeSeries {
  TimeSeries() = default;
  TimeSeries(const char* label, PlotStyle style) : label(label), style(style) {}
  TimeSeries(const std::string& label, PlotStyle style)
      : label(label), style(style) {}
  TimeSeries(TimeSeries&& other)
      : label(std::move(other.label)),
        style(other.style),
        points(std::move(other.points)) {}
  TimeSeries& operator=(TimeSeries&& other) {
    label = std::move(other.label);
    style = other.style;
    points = std::move(other.points);
    return *this;
  }

  std::string label;
  PlotStyle style;
  std::vector<TimeSeriesPoint> points;
};

// A container that represents a general graph, with axes, title and one or
// more data series. A subclass should define the output format by overriding
// the Draw() method.
class Plot {
 public:
  virtual ~Plot() {}

  // Overloaded to draw the plot.
  virtual void Draw() = 0;

  // Sets the lower x-axis limit to min_value (if left_margin == 0).
  // Sets the upper x-axis limit to max_value (if right_margin == 0).
  // The margins are measured as fractions of the interval
  // (max_value - min_value) and are added to either side of the plot.
  void SetXAxis(float min_value,
                float max_value,
                std::string label,
                float left_margin = 0,
                float right_margin = 0);

  // Sets the lower and upper x-axis limits based on min_value and max_value,
  // but modified such that all points in the data series can be represented
  // on the x-axis. The margins are measured as fractions of the range of
  // x-values and are added to either side of the plot.
  void SetSuggestedXAxis(float min_value,
                         float max_value,
                         std::string label,
                         float left_margin = 0,
                         float right_margin = 0);

  // Sets the lower y-axis limit to min_value (if bottom_margin == 0).
  // Sets the upper y-axis limit to max_value (if top_margin == 0).
  // The margins are measured as fractions of the interval
  // (max_value - min_value) and are added to either side of the plot.
  void SetYAxis(float min_value,
                float max_value,
                std::string label,
                float bottom_margin = 0,
                float top_margin = 0);

  // Sets the lower and upper y-axis limits based on min_value and max_value,
  // but modified such that all points in the data series can be represented
  // on the y-axis. The margins are measured as fractions of the range of
  // y-values and are added to either side of the plot.
  void SetSuggestedYAxis(float min_value,
                         float max_value,
                         std::string label,
                         float bottom_margin = 0,
                         float top_margin = 0);

  // Sets the title of the plot.
  void SetTitle(std::string title);

  // Add a new TimeSeries to the plot.
  void AppendTimeSeries(TimeSeries&& time_series);

  // Add a new TimeSeries to the plot if the series contains contains data.
  // Otherwise, the call has no effect and the timeseries is destroyed.
  void AppendTimeSeriesIfNotEmpty(TimeSeries&& time_series);

 protected:
  float xaxis_min_;
  float xaxis_max_;
  std::string xaxis_label_;
  float yaxis_min_;
  float yaxis_max_;
  std::string yaxis_label_;
  std::string title_;
  std::vector<TimeSeries> series_list_;
};

class PlotCollection {
 public:
  virtual ~PlotCollection() {}
  virtual void Draw() = 0;
  virtual Plot* AppendNewPlot() = 0;

 protected:
  std::vector<std::unique_ptr<Plot> > plots_;
};

}  // namespace plotting
}  // namespace webrtc

#endif  // WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_PLOT_BASE_H_
