#!/usr/bin/python2
#
# Copyright 2019 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# remove_files.py:
#   This special action is used to cleanup old files from the build directory.
#   Otherwise ANGLE will pick up the old file(s), causing build or runtime errors.
#

import glob
import os
import sys

if len(sys.argv) < 3:
    print("Usage: " + sys.argv[0] + " <stamp_file> <remove_patterns>")

stamp_file = sys.argv[1]

for i in range(2, len(sys.argv)):
    remove_pattern = sys.argv[i]
    remove_files = glob.glob(remove_pattern)
    for f in remove_files:
        if os.path.isfile(f):
            os.remove(f)

# touch an unused file to keep a timestamp
with open(stamp_file, "w") as f:
    f.write("blah")
    f.close()
