#!/usr/bin/env python
# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Plots metrics from stdin.

Expected format:
PLOTTABLE_DATA: <json data>
Where json data has the following format:
{
  "graph_name": "<graph name>",
  "trace_name": "<test suite name>",
  "units": "<units>",
  "mean": <mean value>,
  "std": <standard deviation value>,
  "samples": [
    { "time": <sample time in us>, "value": <sample value> },
    ...
  ]
}
"""

import fileinput
import json
import matplotlib.pyplot as plt

LINE_PREFIX = 'PLOTTABLE_DATA: '

GRAPH_NAME = 'graph_name'
TRACE_NAME = 'trace_name'
UNITS = 'units'

MICROSECONDS_IN_SECOND = 1e6


def main():
  metrics = []
  for line in fileinput.input():
    line = line.strip()
    if line.startswith(LINE_PREFIX):
      line = line.replace(LINE_PREFIX, '')
      metrics.append(json.loads(line))
    else:
      print line

  for metric in metrics:
    figure = plt.figure()
    figure.canvas.set_window_title(metric[TRACE_NAME])

    x_values = []
    y_values = []
    start_x = None
    for sample in metric['samples']:
      if start_x is None:
        start_x = sample['time']
      # Time is us, we want to show it in seconds.
      x_values.append((sample['time'] - start_x) / MICROSECONDS_IN_SECOND)
      y_values.append(sample['value'])

    plt.ylabel('%s (%s)' % (metric[GRAPH_NAME], metric[UNITS]))
    plt.xlabel('time (s)')
    plt.title(metric[GRAPH_NAME])
    plt.plot(x_values, y_values)

  plt.show()


if __name__ == '__main__':
  main()
