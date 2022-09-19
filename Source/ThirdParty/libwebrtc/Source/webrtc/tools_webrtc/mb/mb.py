#!/usr/bin/env vpython3

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""MB - the Meta-Build wrapper around GN.

MB is a wrapper script for GN that can be used to generate build files
for sets of canned configurations and analyze them.
"""

import os
import sys

_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
_SRC_DIR = os.path.dirname(os.path.dirname(_SCRIPT_DIR))
sys.path.insert(0, _SRC_DIR)

from tools.mb import mb


def _GetExecutable(target, platform):
  executable_prefix = '.\\' if platform == 'win32' else './'
  executable_suffix = '.exe' if platform == 'win32' else ''
  return executable_prefix + target + executable_suffix


def main(args):
  mbw = WebRTCMetaBuildWrapper()
  return mbw.Main(args)


class WebRTCMetaBuildWrapper(mb.MetaBuildWrapper):
  def __init__(self):
    super().__init__()
    # Make sure default_config and default_isolate_map are attributes of the
    # parent class before changing their values.
    # pylint: disable=access-member-before-definition
    assert self.default_config
    assert self.default_isolate_map
    self.default_config = os.path.join(_SCRIPT_DIR, 'mb_config.pyl')
    self.default_isolate_map = os.path.join(_SRC_DIR, 'infra', 'specs',
                                            'gn_isolate_map.pyl')

  def GetSwarmingCommand(self, target, vals):
    isolate_map = self.ReadIsolateMap()
    test_type = isolate_map[target]['type']

    is_android = 'target_os="android"' in vals['gn_args']
    is_linux = self.platform.startswith('linux') and not is_android
    is_ios = 'target_os="ios"' in vals['gn_args']

    if test_type == 'nontest':
      self.WriteFailureAndRaise('We should not be isolating %s.' % target,
                                output_path=None)
    if test_type not in ('console_test_launcher', 'windowed_test_launcher',
                         'non_parallel_console_test_launcher', 'raw',
                         'additional_compile_target', 'junit_test', 'script'):
      self.WriteFailureAndRaise('No command line for '
                                '%s found (test type %s).' %
                                (target, test_type),
                                output_path=None)

    cmdline = []
    extra_files = [
        '../../.vpython3',
        '../../testing/test_env.py',
    ]
    vpython_exe = 'vpython3'

    if isolate_map[target].get('script'):
      cmdline += [
          vpython_exe,
          '../../' + self.ToSrcRelPath(isolate_map[target]['script'])
      ]
    elif is_android:
      cmdline += [
          vpython_exe, '../../build/android/test_wrapper/logdog_wrapper.py',
          '--target', target, '--logdog-bin-cmd', '../../bin/logdog_butler',
          '--logcat-output-file', '${ISOLATED_OUTDIR}/logcats',
          '--store-tombstones'
      ]
    elif is_ios:
      cmdline += [
          vpython_exe, '../../tools_webrtc/flags_compatibility.py',
          'bin/run_%s' % target
      ]
      extra_files.append('../../tools_webrtc/flags_compatibility.py')
    elif test_type == 'raw':
      cmdline += [vpython_exe, '../../tools_webrtc/flags_compatibility.py']
      extra_files.append('../../tools_webrtc/flags_compatibility.py')
      cmdline.append(_GetExecutable(target, self.platform))
    else:
      if isolate_map[target].get('use_webcam', False):
        cmdline += [
            vpython_exe, '../../tools_webrtc/ensure_webcam_is_running.py'
        ]
        extra_files.append('../../tools_webrtc/ensure_webcam_is_running.py')

      # is_linux uses use_ozone and x11 by default.
      use_x11 = is_linux

      xvfb = use_x11 and test_type == 'windowed_test_launcher'
      if xvfb:
        cmdline += [vpython_exe, '../../testing/xvfb.py']
        extra_files.append('../../testing/xvfb.py')
      else:
        cmdline += [vpython_exe, '../../testing/test_env.py']

      extra_files += [
          '../../third_party/gtest-parallel/gtest-parallel',
          '../../third_party/gtest-parallel/gtest_parallel.py',
          '../../tools_webrtc/gtest-parallel-wrapper.py',
      ]
      output_dir = '${ISOLATED_OUTDIR}/test_logs'
      timeout = isolate_map[target].get('timeout', 900)
      cmdline += [
          '../../tools_webrtc/gtest-parallel-wrapper.py',
          '--output_dir=%s' % output_dir,
          '--gtest_color=no',
          # We tell gtest-parallel to interrupt the test after 900
          # seconds, so it can exit cleanly and report results,
          # instead of being interrupted by swarming and not
          # reporting anything.
          '--timeout=%s' % timeout,
      ]
      if test_type == 'non_parallel_console_test_launcher':
        # Still use the gtest-parallel-wrapper.py script since we
        # need it to run tests on swarming, but don't execute tests
        # in parallel.
        cmdline.append('--workers=1')

      asan = 'is_asan=true' in vals['gn_args']
      lsan = 'is_lsan=true' in vals['gn_args']
      msan = 'is_msan=true' in vals['gn_args']
      tsan = 'is_tsan=true' in vals['gn_args']
      sanitizer = asan or lsan or msan or tsan
      if not sanitizer:
        # Retry would hide most sanitizers detections.
        cmdline.append('--retry_failed=3')

      cmdline.append(_GetExecutable(target, self.platform))

      cmdline.extend([
          '--asan=%d' % asan,
          '--lsan=%d' % lsan,
          '--msan=%d' % msan,
          '--tsan=%d' % tsan,
      ])

    cmdline += isolate_map[target].get('args', [])

    return cmdline, extra_files

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
