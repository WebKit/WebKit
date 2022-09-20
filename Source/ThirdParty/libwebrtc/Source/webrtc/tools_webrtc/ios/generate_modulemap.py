#!/usr/bin/env vpython3

# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import sys


def GenerateModulemap():
  parser = argparse.ArgumentParser(description='Generate modulemap')
  parser.add_argument("-o", "--out", type=str, help="Output file.")
  parser.add_argument("-n", "--name", type=str, help="Name of binary.")

  args = parser.parse_args()

  with open(args.out, "w") as outfile:
    module_template = 'framework module %s {\n' \
                      '  umbrella header "%s.h"\n' \
                      '\n' \
                      '  export *\n' \
                      '  module * { export * }\n' \
                      '}\n' % (args.name, args.name)
    outfile.write(module_template)
  return 0


if __name__ == '__main__':
  sys.exit(GenerateModulemap())
