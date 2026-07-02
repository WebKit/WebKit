#!/usr/bin/env python3
## Copyright (c) 2017, Alliance for Open Media. All rights reserved.
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
"""Aggregate multiple entropy stats output which is written in 32-bit int.

python ./aggregate_entropy_stats.py [dir of stats files] [keyword of filenames]
 [filename of final stats]
"""

__author__ = "yuec@google.com"

import os
import sys
import numpy as np

def main():
    dir = sys.argv[1]
    sum = []
    for fn in os.listdir(dir):
        if sys.argv[2] in fn:
            stats = np.fromfile(dir + fn, dtype=np.int32)
            if len(sum) == 0:
                sum = stats
            else:
                sum = np.add(sum, stats)
    if len(sum) == 0:
        print("No stats file is found. Double-check directory and keyword?")
    else:
        sum.tofile(dir+sys.argv[3])

if __name__ == '__main__':
    main()
