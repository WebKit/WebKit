#!/usr/bin/env python
#
# Copyright 2014 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


# This script is a modified copy of the src/build/gyp_chromium.py file. 
# It is needed for parallel processing.

# This file is (possibly, depending on python version) imported by
# gyp_libyuv when GYP_PARALLEL=1 and it creates sub-processes
# through the multiprocessing library.

# Importing in Python 2.6 (fixed in 2.7) on Windows doesn't search for
# imports that don't end in .py (and aren't directories with an
# __init__.py). This wrapper makes "import gyp_libyuv" work with
# those old versions and makes it possible to execute gyp_libyuv.py
# directly on Windows where the extension is useful.

import os

path = os.path.abspath(os.path.split(__file__)[0])
execfile(os.path.join(path, 'gyp_libyuv'))
