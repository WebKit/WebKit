/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_tools/event_log_visualizer/plot_python.h"

#include <stdio.h>

#include <memory>

#include "rtc_base/checks.h"

namespace webrtc {

PythonPlot::PythonPlot() {}

PythonPlot::~PythonPlot() {}

void PythonPlot::Draw() {
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
      if (series_list_[i].points.size() > 0)
        printf("%.3f", series_list_[i].points[0].x);
      for (size_t j = 1; j < series_list_[i].points.size(); j++)
        printf(", %.3f", series_list_[i].points[j].x);
      printf("]\n");

      // List y coordinates
      printf("y%zu = [", i);
      if (series_list_[i].points.size() > 0)
        printf("%G", series_list_[i].points[0].y);
      for (size_t j = 1; j < series_list_[i].points.size(); j++)
        printf(", %G", series_list_[i].points[j].y);
      printf("]\n");

      if (series_list_[i].line_style == LineStyle::kBar) {
        // There is a plt.bar function that draws bar plots,
        // but it is *way* too slow to be useful.
        printf(
            "plt.vlines(x%zu, map(lambda t: min(t,0), y%zu), map(lambda t: "
            "max(t,0), y%zu), color=colors[%zu], "
            "label=\'%s\')\n",
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
      if (interval_list_[i].intervals.size() > 0) {
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
  if (!series_list_.empty() || !interval_list_.empty()) {
    printf("handles, labels = plt.gca().get_legend_handles_labels()\n");
    printf("for lp in legend_patches:\n");
    printf("   handles.append(lp)\n");
    printf("   labels.append(lp.get_label())\n");
    printf("plt.legend(handles, labels, loc=\'best\', fontsize=\'small\')\n");
  }
}

PythonPlotCollection::PythonPlotCollection() {}

PythonPlotCollection::~PythonPlotCollection() {}

void PythonPlotCollection::Draw() {
  printf("import matplotlib.pyplot as plt\n");
  printf("import matplotlib.patches as mpatches\n");
  printf("import matplotlib.patheffects as pe\n");
  printf("import colorsys\n");
  for (size_t i = 0; i < plots_.size(); i++) {
    printf("plt.figure(%zu)\n", i);
    plots_[i]->Draw();
  }
  printf("plt.show()\n");
}

Plot* PythonPlotCollection::AppendNewPlot() {
  Plot* plot = new PythonPlot();
  plots_.push_back(std::unique_ptr<Plot>(plot));
  return plot;
}

}  // namespace webrtc
