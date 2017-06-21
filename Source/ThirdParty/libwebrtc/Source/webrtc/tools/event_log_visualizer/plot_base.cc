/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/event_log_visualizer/plot_base.h"

#include <algorithm>

#include "webrtc/base/checks.h"

namespace webrtc {
namespace plotting {

void Plot::SetXAxis(float min_value,
                    float max_value,
                    std::string label,
                    float left_margin,
                    float right_margin) {
  RTC_DCHECK_LE(min_value, max_value);
  xaxis_min_ = min_value - left_margin * (max_value - min_value);
  xaxis_max_ = max_value + right_margin * (max_value - min_value);
  xaxis_label_ = label;
}

void Plot::SetSuggestedXAxis(float min_value,
                             float max_value,
                             std::string label,
                             float left_margin,
                             float right_margin) {
  for (const auto& series : series_list_) {
    for (const auto& point : series.points) {
      min_value = std::min(min_value, point.x);
      max_value = std::max(max_value, point.x);
    }
  }
  SetXAxis(min_value, max_value, label, left_margin, right_margin);
}

void Plot::SetYAxis(float min_value,
                    float max_value,
                    std::string label,
                    float bottom_margin,
                    float top_margin) {
  RTC_DCHECK_LE(min_value, max_value);
  yaxis_min_ = min_value - bottom_margin * (max_value - min_value);
  yaxis_max_ = max_value + top_margin * (max_value - min_value);
  yaxis_label_ = label;
}

void Plot::SetSuggestedYAxis(float min_value,
                             float max_value,
                             std::string label,
                             float bottom_margin,
                             float top_margin) {
  for (const auto& series : series_list_) {
    for (const auto& point : series.points) {
      min_value = std::min(min_value, point.y);
      max_value = std::max(max_value, point.y);
    }
  }
  SetYAxis(min_value, max_value, label, bottom_margin, top_margin);
}

void Plot::SetTitle(std::string title) {
  title_ = title;
}

void Plot::AppendTimeSeries(TimeSeries&& time_series) {
  series_list_.emplace_back(std::move(time_series));
}

void Plot::AppendTimeSeriesIfNotEmpty(TimeSeries&& time_series) {
  if (time_series.points.size() > 0) {
    series_list_.emplace_back(std::move(time_series));
  }
}

}  // namespace plotting
}  // namespace webrtc
