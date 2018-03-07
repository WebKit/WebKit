/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/plot_protobuf.h"

#include <memory>

namespace webrtc {
namespace plotting {

ProtobufPlot::ProtobufPlot() {}

ProtobufPlot::~ProtobufPlot() {}

void ProtobufPlot::Draw() {}

void ProtobufPlot::ExportProtobuf(webrtc::analytics::Chart* chart) {
  for (size_t i = 0; i < series_list_.size(); i++) {
    webrtc::analytics::DataSet* data_set = chart->add_data_sets();
    for (const auto& point : series_list_[i].points) {
      data_set->add_x_values(point.x);
    }
    for (const auto& point : series_list_[i].points) {
      data_set->add_y_values(point.y);
    }

    if (series_list_[i].line_style == LineStyle::kBar) {
      data_set->set_style(webrtc::analytics::ChartStyle::BAR_CHART);
    } else if (series_list_[i].line_style == LineStyle::kLine) {
      data_set->set_style(webrtc::analytics::ChartStyle::LINE_CHART);
    } else if (series_list_[i].line_style == LineStyle::kStep) {
      data_set->set_style(webrtc::analytics::ChartStyle::LINE_STEP_CHART);
    } else if (series_list_[i].line_style == LineStyle::kNone) {
      data_set->set_style(webrtc::analytics::ChartStyle::SCATTER_CHART);
    } else {
      data_set->set_style(webrtc::analytics::ChartStyle::UNDEFINED);
    }

    if (series_list_[i].point_style == PointStyle::kHighlight)
      data_set->set_highlight_points(true);

    data_set->set_label(series_list_[i].label);
  }

  chart->set_xaxis_min(xaxis_min_);
  chart->set_xaxis_max(xaxis_max_);
  chart->set_yaxis_min(yaxis_min_);
  chart->set_yaxis_max(yaxis_max_);
  chart->set_xaxis_label(xaxis_label_);
  chart->set_yaxis_label(yaxis_label_);
  chart->set_title(title_);
}

ProtobufPlotCollection::ProtobufPlotCollection() {}

ProtobufPlotCollection::~ProtobufPlotCollection() {}

void ProtobufPlotCollection::Draw() {}

void ProtobufPlotCollection::ExportProtobuf(
    webrtc::analytics::ChartCollection* collection) {
  for (const auto& plot : plots_) {
    // TODO(terelius): Ensure that there is no way to insert plots other than
    // ProtobufPlots in a ProtobufPlotCollection. Needed to safely static_cast
    // here.
    webrtc::analytics::Chart* protobuf_representation
        = collection->add_charts();
    static_cast<ProtobufPlot*>(plot.get())
        ->ExportProtobuf(protobuf_representation);
  }
}

Plot* ProtobufPlotCollection::AppendNewPlot() {
  Plot* plot = new ProtobufPlot();
  plots_.push_back(std::unique_ptr<Plot>(plot));
  return plot;
}

}  // namespace plotting
}  // namespace webrtc
