#!/usr/bin/env python
#  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import re
import sys


def replace_double_quote(line):
    re_rtc_import = re.compile(
        r'(\s*)#import\s+"(\S+/|)(\w+\+|)RTC(\w+)\.h"(.*)', re.DOTALL)
    match = re_rtc_import.match(line)
    if not match:
        return line

    return '%s#import <WebRTC/%sRTC%s.h>%s' % (match.group(1), match.group(3),
                                               match.group(4), match.group(5))


def process(input_file, output_file):
    with open(input_file, 'rb') as fb, open(output_file, 'wb') as fw:
        for line in fb.read().decode('UTF-8').splitlines():
            fw.write(replace_double_quote(line).encode('UTF-8'))
            fw.write(b"\n")


def main():
    parser = argparse.ArgumentParser(
        description=
        "Copy headers of framework and replace double-quoted includes to" +
        " angle-bracketed respectively.")
    parser.add_argument('--input',
                        help='Input header files to copy.',
                        type=str)
    parser.add_argument('--output', help='Output file.', type=str)
    parsed_args = parser.parse_args()
    return process(parsed_args.input, parsed_args.output)


if __name__ == '__main__':
    sys.exit(main())
