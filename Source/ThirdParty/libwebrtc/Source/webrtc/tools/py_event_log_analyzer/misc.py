#  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Utility functions for calculating statistics.
"""

from __future__ import division
import collections
import sys


def count_reordered(sequence_numbers):
  """Returns number of reordered indices.

  A reordered index is an index `i` for which sequence_numbers[i] >=
  sequence_numbers[i + 1]
  """
  return sum(1 for (s1, s2) in zip(sequence_numbers,
                                   sequence_numbers[1:]) if
             s1 >= s2)


def ssrc_normalized_size_table(data_points):
  """Counts proportion of data for every SSRC.

  Args:
     data_points: list of pb_parse.DataPoint

  Returns:
     A dictionary mapping from every SSRC in the data points. The
     value of an SSRC `s` is the proportion of sizes of packets with
     SSRC `s` to the total size of all packets.

  """
  mapping = collections.defaultdict(int)
  for point in data_points:
    mapping[point.ssrc] += point.size
  return normalize_counter(mapping)


def normalize_counter(counter):
  """Returns a normalized version of the dictionary `counter`.

  Does not modify `counter`.

  Returns:
    A new dictionary, in which every value in `counter`
    has been divided by the total to sum up to 1.
  """
  total = sum(counter.values())
  return {key: counter[key] / total for key in counter}


def unwrap(data, mod):
  """Returns `data` unwrapped modulo `mod`. Does not modify data.

  Adds integer multiples of mod to all elements of data except the
  first, such that all pairs of consecutive elements (a, b) satisfy
  -mod / 2 <= b - a < mod / 2.

  E.g. unwrap([0, 1, 2, 0, 1, 2, 7, 8], 3) -> [0, 1, 2, 3,
  4, 5, 4, 5]
  """
  lst = data[:]
  for i in range(1, len(data)):
    lst[i] = lst[i - 1] + (lst[i] - lst[i - 1] +
                           mod // 2) % mod - (mod // 2)
  return lst


def ssrc_directions(data_points):
  ssrc_is_incoming = {}
  for point in data_points:
    ssrc_is_incoming[point.ssrc] = point.incoming
  return ssrc_is_incoming


# Python 2/3-compatible input function
if sys.version_info[0] <= 2:
  get_input = raw_input
else:
  get_input = input
