#!/usr/bin/env python
# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Generates a command-line for coverage.py. Useful for manual coverage runs.

Before running the generated command line, do this:

gn gen out/coverage --args='use_clang_coverage=true is_component_build=false'
"""

import sys

TESTS = [
  'video_capture_tests',
  'webrtc_nonparallel_tests',
  'video_engine_tests',
  'tools_unittests',
  'test_support_unittests',
  'slow_tests',
  'system_wrappers_unittests',
  'rtc_unittests',
  'rtc_stats_unittests',
  'rtc_pc_unittests',
  'rtc_media_unittests',
  'peerconnection_unittests',
  'modules_unittests',
  'modules_tests',
  'low_bandwidth_audio_test',
  'common_video_unittests',
  'common_audio_unittests',
  'audio_decoder_unittests'
]

def main():
  cmd = ([sys.executable, 'tools/code_coverage/coverage.py'] + TESTS +
         ['-b out/coverage', '-o out/report'] +
         ['-i=\'.*/out/.*|.*/third_party/.*|.*test.*\''] +
         ['-c \'out/coverage/%s\'' % t for t in TESTS])

  def WithXvfb(binary):
    return '-c \'%s testing/xvfb.py %s\'' % (sys.executable, binary)
  modules_unittests = 'out/coverage/modules_unittests'
  cmd[cmd.index('-c \'%s\'' % modules_unittests)] = WithXvfb(modules_unittests)

  print ' '.join(cmd)
  return 0

if __name__ == '__main__':
  sys.exit(main())
