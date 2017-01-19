#!/bin/bash

# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# To set up in e.g. Eclipse, run a separate shell and pipe the output from the
# test into this script.
#
# In Eclipse, that amounts to creating a Run Configuration which starts
# "/bin/bash" with the arguments "-c [trunk_path]/out/Debug/modules_unittests
# --gtest_filter=*BweTest* | [trunk_path]/webrtc/modules/
# remote_bitrate_estimator/test/plot_dynamics.sh

# This script supports multiple figures (windows), the figure is specified as an
# identifier at the first argument after the PLOT command. Each figure has a
# single y axis and a dual y axis mode. If any line specifies an axis by ending
# with "#<axis number (1 or 2)>" two y axis will be used, the first will be
# assumed to represent bitrate (in kbps) and the second will be assumed to
# represent time deltas (in ms).

log=$(</dev/stdin)

# Plot dynamics.
function gen_gnuplot_input {
  colors=(a7001f 0a60c2 b2582b 21a66c d6604d 4393c3 f4a582 92c5de edcbb7 b1c5d0)
  plots=$(echo "$log" | grep "^PLOT" | grep "#")
  # Each figure corresponds to a separate plot window.
  figures=($(echo "$plots" | cut -f 2 | sort | uniq))

  for figure in "${figures[@]}" ; do
    # Each data set corresponds to a plot line.
    data_sets=$(echo "$plots" | grep "^PLOT.$figure" | cut -f 3 | sort | uniq)
    # Lines can be scaled on the left (1) or right (2) axis.
    linetypes=($(echo "$data_sets" | grep "#" | cut -d '#' -f 2 | \
                 cut -d '@' -f 1 | uniq))

    # Set plot configurations.
    echo "reset; "
    echo "set terminal wxt $figure size 1440,900 font \"Arial,9\"; "
    echo "set xlabel \"Seconds\"; "
    if (( "${#linetypes[@]}" > "1" )); then
      echo "set ylabel 'Bitrate (kbps)';"  # Left side.
      echo "set ytics nomirror;"
      echo "set y2label 'Time delta (ms)';"  # Right side.
      echo "set y2tics nomirror;"
    else
      # Single axis (left side), set its label according to data.
      y_label=$(echo "$data_sets" | grep "#" | cut -d '#' -f 1  | \
          cut -d ' ' -f 1 | cut -d '/' -f 3 | sed 's/[0-9]/#/g' | \
          cut -d '#' -f 3 | head -n 1 | sed 's/_/ /g')
      echo "set ylabel \"$y_label\";"
    fi

    i=0
    echo -n "plot "
    for set in $data_sets ; do
      (( i++ )) && echo -n ","
      echo -n "'-' with "
      echo -n "linespoints "
      echo -n "ps 0.5 "
      echo -n "lc rgbcolor \"#${colors[$(($i % 10))]}\" "
      if (( "${#linetypes[@]}" > "1" )); then
        # Multiple sets can have a same line plot.
        linetype=$(echo "$set" | grep "#" | cut -d '#' -f 2 | cut -d '@' -f 1)
        if (( "${#linetype}" > "0")); then
          echo -n "axes x1y$linetype "
        else
          # If no line type is specified, but line types are used, we will
          # default to scale on the left axis.
          echo -n "axes x1y1 "
        fi
      fi
      echo -n "title \"$set\" "
    done
    echo
    for set in $data_sets ; do
      echo "$log" | grep "^PLOT.$figure.$set" | cut -f 4,5
      echo "e"
    done
  done
}
gen_gnuplot_input | gnuplot -persist
