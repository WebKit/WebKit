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
# remote_bitrate_estimator/test/plot_bars.sh

# This script supports multiple figures (windows), the figure is specified as an
# identifier at the first argument after the PLOT command. Each figure has a
# single y axis and a dual y axis mode. If any line specifies an axis by ending
# with "#<axis number (1 or 2)>" two y axis will be used, the first will be
# assumed to represent bitrate (in kbps) and the second will be assumed to
# represent time deltas (in ms).

log=$(</dev/stdin)

# Plot histograms.
function gen_gnuplot_bar_input {
  x_start=1
  x_end=3.75
  bars=$(echo "$log" | grep "BAR")

  labels=$(echo "$log" | grep "^LABEL")
  figures=($(echo "$bars" | cut -f 2 | sort | uniq))

  echo "reset"  # Clears previous settings.

  echo "set title font 'Verdana,22'"
  echo "set xtics font 'Verdana,24'"
  echo "set ytics font 'Verdana,14'"
  echo "set ylabel font 'Verdana,16'"

  echo "set xrange[$x_start:$x_end]"
  echo "set style fill solid 0.5"
  echo "set style fill solid border -1"

  declare -a ydist=(11.5 10.5 10.5)  # Used to correctly offset the y label.
  i=0
  for figure in "${figures[@]}" ; do

    echo "set terminal wxt $figure size 440,440 dashed"
    echo "set ylabel offset ${ydist[$i]}, -3"
    (( i++ ))

    title=$(echo "$labels" | grep "^LABEL.$figure" | cut -f 3 | \
                                         head -n 1 | sed 's/_/ /g')
    y_label=$(echo "$labels" | grep "^LABEL.$figure" | cut -f 4 | \
                                         head -n 1 | sed 's/_/ /g')

    # RMCAT flows.
    num_flows=$(echo "$labels" | grep "^LABEL.$figure" | cut -f 5 | \
                                         head -n 1)

    # RMCAT algorithm 1.
    x_label_1=$(echo "$log" | grep "BAR.$figure" | cut -f 3 | sed 's/_/\t/g' \
                                 | cut -f 1  | sort | uniq | head -n 1 )

    # RMCAT algorithm 2.
    x_label_2=$(echo "$log" | grep "BAR.$figure" | cut -f 3 | sed 's/_/\t/g' \
                                 | cut -f 1  | sort | uniq |  sed -n 2p)

    x_labels="('$x_label_1' 2, '$x_label_2' 3)"
    tcp_flow=false

    tcp_space=0.2  # Extra horizontal space between bars.

    # Parse labels if there are other flows in addition to RMCAT ones.
    IFS='x' read -ra split_label_1 <<< "$x_label_1"

    if (( ${#split_label_1[@]} > "1" )); then
      tcp_flow=true
      box_width=$(echo "(1.0-$tcp_space/2)/$num_flows" | bc -l)
      echo "set xtics font 'Verdana,16'"
      x_labels="("
      delimiter=""
      abscissa=$(echo $x_start + 0.5 + 0.5*$box_width | bc)
      for label in "${split_label_1[@]}" ; do
        x_labels+="$delimiter'$label' $abscissa"
        abscissa=$(echo $abscissa + $box_width | bc)
        delimiter=", "
      done
      abscissa=$(echo $abscissa + $tcp_space | bc)
      IFS='x' read -ra split_label_2 <<< "$x_label_2"
      for label in "${split_label_2[@]}" ; do
        x_labels+="$delimiter'$label' $abscissa"
        abscissa=$(echo $abscissa + $box_width | bc)
      done
      x_labels="$x_labels)"
    else
      box_width=$(echo 1.0/$num_flows | bc -l)
    fi

    echo "set boxwidth $box_width"

    # Plots can be directly exported to image files.
    file_name=$(echo "$labels" | grep "^LABEL.$figure" | cut -f 5 | head -n 1)

    y_max=0  # Used to scale the plot properly.

    # Scale all latency plots with the same vertical scale.
    delay_figure=5
    if (( $figure==$delay_figure )) ; then
      y_max=400
    else  # Take y_max = 1.1 * highest plot value.

      # Since only the optimal bitrate for the first flow is being ploted,
      # consider only this one for scalling purposes.
      data_sets=$(echo "$bars" | grep "LIMITERRORBAR.$figure" | cut -f 3 | \
                                     sed 's/_/\t/g' | cut -f 1 | sort | uniq)

      if (( ${#data_sets[@]} > "0" )); then
        for set in $data_sets ; do
          y=$(echo "$bars" | grep "LIMITERRORBAR.$figure.$set" | cut -f 8 | \
                                                                  head -n 1)
          if (( $(bc <<< "$y > $y_max") == 1 )); then
            y_max=$y
          fi
        done
      fi

      data_sets=$(echo "$bars" | grep "ERRORBAR.$figure" | cut -f 3 | \
                                                               sort | uniq)
      if (( ${#data_sets[@]} > "0" )); then
        for set in $data_sets ; do
          y=$(echo "$bars" | grep "ERRORBAR.$figure.$set" | cut -f 6 | \
                                                                 head -n 1)
          if (( $(bc <<< "$y > $y_max") == 1 )) ; then
            y_max=$y
          fi
        done
      fi

      data_sets=$(echo "$bars" | grep "BAR.$figure" | cut -f 3 | sort | uniq)

      for set in $data_sets ; do
        y=$(echo "$bars" | grep "BAR.$figure.$set" | cut -f 4 | head -n 1)
        if (( $(bc <<< "$y > $y_max") == 1 )) ; then
          y_max=$y
        fi
      done

      y_max=$(echo $y_max*1.1 | bc)
    fi


    echo "set ylabel \"$y_label\""
    echo "set yrange[0:$y_max]"

    echo "set multiplot"

    # Plot bars.
    data_sets=$(echo "$bars" | grep "BAR.$figure" | cut -f 3 | sort | uniq)

    echo "set xtics $x_labels"
    echo "plot '-' using 1:4:2 with boxes lc variable notitle"

    echo

    color=11  # Green.
    x_bar=$(echo $x_start + 0.5 + 0.5*$box_width | bc)
    for set in $data_sets ; do
      echo -n "$x_bar  $color  "
      echo "$bars" | grep "BAR.$figure.$set" | cut -f 3,4

      # Add extra space if TCP flows are being plotted.
      if $tcp_flow && \
          (( $(bc <<< "$x_bar < $x_start + 1.5 - 0.5*$tcp_space") == 1 )) && \
          (( $(bc <<< "$x_bar + $box_width > $x_start + 1.5 + 0.5*$tcp_space") \
           == 1 )); then
          x_bar=$(echo $x_bar + $tcp_space | bc)
      fi

      x_bar=$(echo $x_bar + $box_width | bc)

      if (( $(bc <<< "$x_bar > 2.5") == 1 )) ; then
        color=12  # Blue.
      fi
      # Different bar color for TCP flows:
      if $tcp_flow && \
         (( $(bc <<< "(100*$x_bar)%100 < 50") == 1 ))
      then
        color=18  # Gray.
      fi
    done
    echo "e"

    # Plot Baseline bars, e.g. one-way path delay on latency plots.
    data_sets=$(echo "$log" | grep "BASELINE.$figure" | cut -f 3 | sort | uniq)

    if (( ${#data_sets} > "0" )); then
      echo "set xtics $x_labels"
      echo "plot '-' using 1:4:2 with boxes lc variable notitle"

      echo

      color=18  # Gray.
      x_bar=$(echo $x_start + 0.5 + 0.5*$box_width | bc)
      for set in $data_sets ; do
        echo -n "$x_bar  $color  "
        echo "$log" | grep "BASELINE.$figure.$set" | cut -f 3,4

        # Add extra space if TCP flows are being plotted.
        if $tcp_flow && \
            (( $(bc <<< "$x_bar < $x_start + 1.5 - 0.5*$tcp_space") == 1 )) && \
            (( $(bc <<< "$x_bar + $box_width > $x_start + 1.5 \
            + 0.5*$tcp_space") == 1 )); then
            x_bar=$(echo $x_bar + $tcp_space | bc)
        fi

        x_bar=$(echo $x_bar + $box_width | bc)

      done
      echo "e"
    fi

    # Plot vertical error lines, e.g. y +- sigma.
    data_sets=$(echo "$bars" | grep "ERRORBAR.$figure" | cut -f 3 | sort | uniq)

    if (( ${#data_sets} > "0" )); then

      echo "set key left"
      error_title=$(echo "$bars" | grep "ERRORBAR.$figure" | cut -f 7 | \
                                                 head -n 1 | sed 's/_/ /g')

      echo "set xtics $x_labels"
      echo "plot '-' using 1:3:4:5 title '$error_title' with yerr"

      x_error_line=$(echo $x_start + 0.5 + 0.5*$box_width | bc)
      for set in $data_sets ; do
        echo -n "$x_error_line  "
        echo "$bars" | grep "ERRORBAR.$figure.$set" | cut -f 3,4,5,6

        # Add extra space if TCP flows are being plotted.
        if $tcp_flow && \
          (( $(bc <<< "$x_error_line < $x_start + 1.5 - 0.5*$tcp_space") == 1 \
          )) && (( $(bc <<< "$x_error_line + $box_width > $x_start + 1.5 \
          + 0.5*$tcp_space") == 1 )); then
          x_error_line=$(echo $x_error_line + $tcp_space | bc)
        fi

        x_error_line=$(echo $x_error_line + $box_width | bc)
      done
      echo "e"
    fi

    # Plot horizontal dashed lines, e.g. y = optimal bitrate.
    data_sets=$(echo "$bars" | grep "LIMITERRORBAR.$figure" | cut -f 3 \
                                                     | sort | uniq)
    if (( ${#data_sets} > "0" )); then

      echo "set style line 1 lt 1 lw 3 pt 3 ps 0 linecolor rgb 'black'"

      limit_titles=$(echo "$bars" | grep "LIMITERRORBAR.$figure" | cut -f 9 \
                                                         | sort | uniq)

      for title in $limit_titles ; do
        y_max=$(echo "$bars" | grep "LIMITERRORBAR.$figure" | grep "$title" \
                                               | cut -f 8 | head -n 1)

        retouched_title=$(echo "$title" | sed 's/#/\t/g' | cut -f 1 \
                                                         | sed 's/_/ /g')

        echo "set key right top"
        echo "set xtics $x_labels"
        echo "plot $y_max lt 7 lw 1 linecolor rgb 'black' \
                                  title '$retouched_title'"
      done

    fi

    echo "unset multiplot"
  done
}

gen_gnuplot_bar_input | gnuplot -persist
