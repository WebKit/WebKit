#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

""" Adds extra patterns to the root .gitignore file.

Reads the contents of the filename given as the first argument and appends
them to the root .gitignore file. The new entires are intended to be additional
ignoring patterns, or negating patterns to override existing entries (man
gitignore for more details).
"""

import os
import sys

MODIFY_STRING = '# The following added by %s\n'


def main(argv):
  if not argv[1]:
    # Special case; do nothing.
    return 0

  modify_string = MODIFY_STRING % argv[0]
  gitignore_file = os.path.dirname(argv[0]) + '/../../.gitignore'
  lines = open(gitignore_file, 'r').readlines()
  for i, line in enumerate(lines):
    # Look for modify_string in the file to ensure we don't append the extra
    # patterns more than once.
    if line == modify_string:
      lines = lines[:i]
      break
  lines.append(modify_string)

  f = open(gitignore_file, 'w')
  f.write(''.join(lines))
  f.write(open(argv[1], 'r').read())
  f.close()

if __name__ == '__main__':
  sys.exit(main(sys.argv))
