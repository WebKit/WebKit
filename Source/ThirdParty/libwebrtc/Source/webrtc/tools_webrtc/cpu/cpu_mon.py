#!/usr/bin/env vpython3
#
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import sys

import psutil
import numpy
from matplotlib import pyplot


class CpuSnapshot:
  def __init__(self, label):
    self.label = label
    self.samples = []

  def Capture(self, sample_count):
    print(('Capturing %d CPU samples for %s...' %
           ((sample_count - len(self.samples)), self.label)))
    while len(self.samples) < sample_count:
      self.samples.append(psutil.cpu_percent(1.0, False))

  def Text(self):
    return (
        '%s: avg=%s, median=%s, min=%s, max=%s' %
        (self.label, numpy.average(self.samples), numpy.median(
            self.samples), numpy.min(self.samples), numpy.max(self.samples)))

  def Max(self):
    return numpy.max(self.samples)


def GrabCpuSamples(sample_count):
  print('Label for snapshot (enter to quit): ')
  label = eval(input().strip())
  if len(label) == 0:
    return None

  snapshot = CpuSnapshot(label)
  snapshot.Capture(sample_count)

  return snapshot


def main():
  print('How many seconds to capture per snapshot (enter for 60)?')
  sample_count = eval(input().strip())
  if len(sample_count) > 0 and int(sample_count) > 0:
    sample_count = int(sample_count)
  else:
    print('Defaulting to 60 samples.')
    sample_count = 60

  snapshots = []
  while True:
    snapshot = GrabCpuSamples(sample_count)
    if snapshot is None:
      break
    snapshots.append(snapshot)

  if len(snapshots) == 0:
    print('no samples captured')
    return -1

  pyplot.title('CPU usage')

  for s in snapshots:
    pyplot.plot(s.samples, label=s.Text(), linewidth=2)

  pyplot.legend()

  pyplot.show()
  return 0


if __name__ == '__main__':
  sys.exit(main())
