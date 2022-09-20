/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/rtc_event_log_visualizer/plot_base.h"

#include <algorithm>
#include <memory>

#include "rtc_base/checks.h"

namespace webrtc {

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

void Plot::SetYAxisTickLabels(
    const std::vector<std::pair<float, std::string>>& labels) {
  yaxis_tick_labels_ = labels;
}

void Plot::SetTitle(const std::string& title) {
  title_ = title;
}

void Plot::SetId(const std::string& id) {
  id_ = id;
}

void Plot::AppendTimeSeries(TimeSeries&& time_series) {
  series_list_.emplace_back(std::move(time_series));
}

void Plot::AppendIntervalSeries(IntervalSeries&& interval_series) {
  interval_list_.emplace_back(std::move(interval_series));
}

void Plot::AppendTimeSeriesIfNotEmpty(TimeSeries&& time_series) {
  if (!time_series.points.empty()) {
    series_list_.emplace_back(std::move(time_series));
  }
}

void Plot::PrintPythonCode() const {
  // Write python commands to stdout. Intended program usage is
  // ./event_log_visualizer event_log160330.dump | python

  if (!series_list_.empty()) {
    printf("color_count = %zu\n", series_list_.size());
    printf(
        "hls_colors = [(i*1.0/color_count, 0.25+i*0.5/color_count, 0.8) for i "
        "in range(color_count)]\n");
    printf("colors = [colorsys.hls_to_rgb(*hls) for hls in hls_colors]\n");

    for (size_t i = 0; i < series_list_.size(); i++) {
      printf("\n# === Series: %s ===\n", series_list_[i].label.c_str());
      // List x coordinates
      printf("x%zu = [", i);
      if (!series_list_[i].points.empty())
        printf("%.3f", series_list_[i].points[0].x);
      for (size_t j = 1; j < series_list_[i].points.size(); j++)
        printf(", %.3f", series_list_[i].points[j].x);
      printf("]\n");

      // List y coordinates
      printf("y%zu = [", i);
      if (!series_list_[i].points.empty())
        printf("%G", series_list_[i].points[0].y);
      for (size_t j = 1; j < series_list_[i].points.size(); j++)
        printf(", %G", series_list_[i].points[j].y);
      printf("]\n");

      if (series_list_[i].line_style == LineStyle::kBar) {
        // There is a plt.bar function that draws bar plots,
        // but it is *way* too slow to be useful.
        printf(
            "plt.vlines(x%zu, [min(t,0) for t in y%zu], [max(t,0) for t in "
            "y%zu], color=colors[%zu], label=\'%s\')\n",
            i, i, i, i, series_list_[i].label.c_str());
        if (series_list_[i].point_style == PointStyle::kHighlight) {
          printf(
              "plt.plot(x%zu, y%zu, color=colors[%zu], "
              "marker='.', ls=' ')\n",
              i, i, i);
        }
      } else if (series_list_[i].line_style == LineStyle::kLine) {
        if (series_list_[i].point_style == PointStyle::kHighlight) {
          printf(
              "plt.plot(x%zu, y%zu, color=colors[%zu], label=\'%s\', "
              "marker='.')\n",
              i, i, i, series_list_[i].label.c_str());
        } else {
          printf("plt.plot(x%zu, y%zu, color=colors[%zu], label=\'%s\')\n", i,
                 i, i, series_list_[i].label.c_str());
        }
      } else if (series_list_[i].line_style == LineStyle::kStep) {
        // Draw lines from (x[0],y[0]) to (x[1],y[0]) to (x[1],y[1]) and so on
        // to illustrate the "steps". This can be expressed by duplicating all
        // elements except the first in x and the last in y.
        printf("xd%zu = [dup for v in x%zu for dup in [v, v]]\n", i, i);
        printf("yd%zu = [dup for v in y%zu for dup in [v, v]]\n", i, i);
        printf(
            "plt.plot(xd%zu[1:], yd%zu[:-1], color=colors[%zu], "
            "label=\'%s\')\n",
            i, i, i, series_list_[i].label.c_str());
        if (series_list_[i].point_style == PointStyle::kHighlight) {
          printf(
              "plt.plot(x%zu, y%zu, color=colors[%zu], "
              "marker='.', ls=' ')\n",
              i, i, i);
        }
      } else if (series_list_[i].line_style == LineStyle::kNone) {
        printf(
            "plt.plot(x%zu, y%zu, color=colors[%zu], label=\'%s\', "
            "marker='o', ls=' ')\n",
            i, i, i, series_list_[i].label.c_str());
      } else {
        printf("raise Exception(\"Unknown graph type\")\n");
      }
    }

    // IntervalSeries
    printf("interval_colors = ['#ff8e82','#5092fc','#c4ffc4','#aaaaaa']\n");
    RTC_CHECK_LE(interval_list_.size(), 4);
    // To get the intervals to show up in the legend we have to create patches
    // for them.
    printf("legend_patches = []\n");
    for (size_t i = 0; i < interval_list_.size(); i++) {
      // List intervals
      printf("\n# === IntervalSeries: %s ===\n",
             interval_list_[i].label.c_str());
      printf("ival%zu = [", i);
      if (!interval_list_[i].intervals.empty()) {
        printf("(%G, %G)", interval_list_[i].intervals[0].begin,
               interval_list_[i].intervals[0].end);
      }
      for (size_t j = 1; j < interval_list_[i].intervals.size(); j++) {
        printf(", (%G, %G)", interval_list_[i].intervals[j].begin,
               interval_list_[i].intervals[j].end);
      }
      printf("]\n");

      printf("for i in range(0, %zu):\n", interval_list_[i].intervals.size());
      if (interval_list_[i].orientation == IntervalSeries::kVertical) {
        printf(
            "  plt.axhspan(ival%zu[i][0], ival%zu[i][1], "
            "facecolor=interval_colors[%zu], "
            "alpha=0.3)\n",
            i, i, i);
      } else {
        printf(
            "  plt.axvspan(ival%zu[i][0], ival%zu[i][1], "
            "facecolor=interval_colors[%zu], "
            "alpha=0.3)\n",
            i, i, i);
      }
      printf(
          "legend_patches.append(mpatches.Patch(ec=\'black\', "
          "fc=interval_colors[%zu], label='%s'))\n",
          i, interval_list_[i].label.c_str());
    }
  }

  printf("plt.xlim(%f, %f)\n", xaxis_min_, xaxis_max_);
  printf("plt.ylim(%f, %f)\n", yaxis_min_, yaxis_max_);
  printf("plt.xlabel(\'%s\')\n", xaxis_label_.c_str());
  printf("plt.ylabel(\'%s\')\n", yaxis_label_.c_str());
  printf("plt.title(\'%s\')\n", title_.c_str());
  printf("fig = plt.gcf()\n");
  printf("fig.canvas.manager.set_window_title(\'%s\')\n", id_.c_str());
  if (!yaxis_tick_labels_.empty()) {
    printf("yaxis_tick_labels = [");
    for (const auto& kv : yaxis_tick_labels_) {
      printf("(%f,\"%s\"),", kv.first, kv.second.c_str());
    }
    printf("]\n");
    printf("yaxis_tick_labels = list(zip(*yaxis_tick_labels))\n");
    printf("plt.yticks(*yaxis_tick_labels)\n");
  }
  if (!series_list_.empty() || !interval_list_.empty()) {
    printf("handles, labels = plt.gca().get_legend_handles_labels()\n");
    printf("for lp in legend_patches:\n");
    printf("   handles.append(lp)\n");
    printf("   labels.append(lp.get_label())\n");
    printf("plt.legend(handles, labels, loc=\'best\', fontsize=\'small\')\n");
  }
}

void Plot::ExportProtobuf(webrtc::analytics::Chart* chart) const {
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
  chart->set_id(id_);

  for (const auto& kv : yaxis_tick_labels_) {
    webrtc::analytics::TickLabel* tick = chart->add_yaxis_tick_labels();
    tick->set_value(kv.first);
    tick->set_label(kv.second);
  }
}

void PlotCollection::PrintPythonCode(bool shared_xaxis) const {
  printf("import matplotlib.pyplot as plt\n");
  printf("plt.rcParams.update({'figure.max_open_warning': 0})\n");
  printf("import matplotlib.patches as mpatches\n");
  printf("import matplotlib.patheffects as pe\n");
  printf("import colorsys\n");
  for (size_t i = 0; i < plots_.size(); i++) {
    printf("plt.figure(%zu)\n", i);
    if (shared_xaxis) {
      // Link x-axes across all figures for synchronized zooming.
      if (i == 0) {
        printf("axis0 = plt.subplot(111)\n");
      } else {
        printf("plt.subplot(111, sharex=axis0)\n");
      }
    }
    plots_[i]->PrintPythonCode();
  }
  printf("plt.show()\n");
}

void PlotCollection::ExportProtobuf(
    webrtc::analytics::ChartCollection* collection) const {
  for (const auto& plot : plots_) {
    webrtc::analytics::Chart* protobuf_representation =
        collection->add_charts();
    plot->ExportProtobuf(protobuf_representation);
  }
  if (calltime_to_utc_ms_) {
    collection->set_calltime_to_utc_ms(*calltime_to_utc_ms_);
  }
}

Plot* PlotCollection::AppendNewPlot() {
  plots_.push_back(std::make_unique<Plot>());
  return plots_.back().get();
}

}  // namespace webrtc
