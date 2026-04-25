#!/usr/bin/env vpython3

# Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import re
import subprocess
import sys

WEBRTC_VERSION_RE = re.compile(
    r'WebRTC source stamp [0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}'
)


if __name__ == '__main__':
  args = sys.argv
  if len(args) != 2:
    print('Usage: binary_version_test.py <FILE_NAME>')
    sys.exit(1)
  filename = sys.argv[1]
  output = subprocess.check_output(['strings', filename])
  strings_in_binary = output.decode('utf-8').splitlines()
  for symbol in strings_in_binary:
    if WEBRTC_VERSION_RE.match(symbol):
      with open('webrtc_binary_version_check', 'w') as f:
        f.write(symbol)
      sys.exit(0)
  print('WebRTC source timestamp not found in "%s"' % filename)
  print('Check why "kSourceTimestamp" from call/version.cc is not linked '
        '(or why it has been optimized away by the compiler/linker)')
  sys.exit(1)
