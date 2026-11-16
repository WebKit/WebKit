#!/usr/bin/env python
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Checks for legacy root directory and instructs the user how to upgrade."""

import os
import sys

SOLUTION_ROOT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 os.pardir)
MESSAGE = """\
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
                      A C T I O N     R E Q I R E D
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

It looks like you have a legacy checkout where the solution's top-level
directory is named 'trunk'. From now on, it must be named 'src'.

What you need to do is to:

1. Edit your .gclient file and change the solution name from 'trunk' to 'src'
2. Rename your 'trunk' directory to 'src'
3. Re-run gclient sync (or gclient runhooks)"""


def main():
  gclient_filename = os.path.join(SOLUTION_ROOT_DIR, '.gclient')
  config_dict = {}
  try:
    with open(gclient_filename, 'rb') as gclient_file:
      exec(gclient_file, config_dict)
    for solution in config_dict.get('solutions', []):
      if solution['name'] == 'trunk':
        print MESSAGE
        if solution.get('custom_vars', {}).get('root_dir'):
          print ('4. Optional: Remove your "root_dir" entry from the '
                 'custom_vars dictionary of the solution.')

        # Remove the gclient cache file to avoid an error on the next sync.
        entries_file = os.path.join(SOLUTION_ROOT_DIR, '.gclient_entries')
        if os.path.exists(entries_file):
          os.unlink(entries_file)
        return 1
    return 0
  except Exception as e:
    print >> sys.stderr, "Error while parsing .gclient: ", e
    return 1


if __name__ == '__main__':
  sys.exit(main())
