#!/usr/bin/python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
import argparse
import numpy as np


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', required=True)
    parser.add_argument('-o', required=True)

    args = parser.parse_args()

    array = np.arange(100, dtype=np.float32)
    array.tofile(open(args.o, 'w'))


if __name__ == '__main__':
    main()
