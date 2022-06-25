#!/usr/bin/env python
# Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Generates command-line instructions to produce one-time iOS coverage using
coverage.py.

This script is usable for both real devices and simulator.
But for real devices actual execution should be done manually from Xcode
and coverage.profraw files should be also copied manually from the device.

Additional prerequisites:

1. Xcode 10+ with iPhone Simulator SDK. Can be installed by command:
   $ mac_toolchain install -kind ios -xcode-version 10l232m \
     -output-dir build/mac_files/Xcode.app

2. For computing coverage on real device you probably also need to apply
following patch to code_coverage/coverage.py script:

========== BEGINNING OF PATCH ==========
--- a/code_coverage/coverage.py
+++ b/code_coverage/coverage.py
@@ -693,8 +693,7 @@ def _AddArchArgumentForIOSIfNeeded(cmd_list, num_archs):
   to use, and one architecture needs to be specified for each binary.
   "" "
if _IsIOS():
-    cmd_list.extend(['-arch=x86_64'] * num_archs)
+    cmd_list.extend(['-arch=arm64'] * num_archs)


def _GetBinaryPath(command):
@@ -836,8 +835,8 @@ def _GetBinaryPathsFromTargets(targets, build_dir):
  binary_path = os.path.join(build_dir, target)
  if coverage_utils.GetHostPlatform() == 'win':
    binary_path += '.exe'
+    elif coverage_utils.GetHostPlatform() == 'mac':
+      binary_path += '.app/%s' % target

if os.path.exists(binary_path):
  binary_paths.append(binary_path)
========== ENDING OF PATCH ==========

"""

import sys

DIRECTORY = 'out/coverage'

TESTS = [
    'audio_decoder_unittests',
    'common_audio_unittests',
    'common_video_unittests',
    'modules_tests',
    'modules_unittests',
    'rtc_media_unittests',
    'rtc_pc_unittests',
    'rtc_stats_unittests',
    'rtc_unittests',
    'slow_tests',
    'system_wrappers_unittests',
    'test_support_unittests',
    'tools_unittests',
    'video_capture_tests',
    'video_engine_tests',
    'webrtc_nonparallel_tests',
]

XC_TESTS = [
    'apprtcmobile_tests',
    'sdk_framework_unittests',
    'sdk_unittests',
]


def FormatIossimTest(test_name, is_xctest=False):
    args = ['%s/%s.app' % (DIRECTORY, test_name)]
    if is_xctest:
        args += ['%s/%s_module.xctest' % (DIRECTORY, test_name)]

    return '-c \'%s/iossim %s\'' % (DIRECTORY, ' '.join(args))


def GetGNArgs(is_simulator):
    target_cpu = 'x64' if is_simulator else 'arm64'
    return ([] + ['target_os="ios"'] + ['target_cpu="%s"' % target_cpu] +
            ['use_clang_coverage=true'] + ['is_component_build=false'] +
            ['dcheck_always_on=true'])


def GenerateIOSSimulatorCommand():
    gn_args_string = ' '.join(GetGNArgs(is_simulator=True))
    gn_cmd = ['gn', 'gen', DIRECTORY, '--args=\'%s\'' % gn_args_string]

    coverage_cmd = ([sys.executable, 'tools/code_coverage/coverage.py'] +
                    ["%s.app" % t for t in XC_TESTS + TESTS] +
                    ['-b %s' % DIRECTORY, '-o out/report'] +
                    ['-i=\'.*/out/.*|.*/third_party/.*|.*test.*\''] +
                    [FormatIossimTest(t, is_xctest=True) for t in XC_TESTS] +
                    [FormatIossimTest(t, is_xctest=False) for t in TESTS])

    print 'To get code coverage using iOS sim just run following commands:'
    print ''
    print ' '.join(gn_cmd)
    print ''
    print ' '.join(coverage_cmd)
    return 0


def GenerateIOSDeviceCommand():
    gn_args_string = ' '.join(GetGNArgs(is_simulator=False))

    coverage_report_cmd = (
        [sys.executable, 'tools/code_coverage/coverage.py'] +
        ['%s.app' % t for t in TESTS] + ['-b %s' % DIRECTORY] +
        ['-o out/report'] + ['-p %s/merged.profdata' % DIRECTORY] +
        ['-i=\'.*/out/.*|.*/third_party/.*|.*test.*\''])

    print 'Computing code coverage for real iOS device is a little bit tedious.'
    print ''
    print 'You will need:'
    print ''
    print '1. Generate xcode project and open it with Xcode 10+:'
    print '  gn gen %s --ide=xcode --args=\'%s\'' % (DIRECTORY, gn_args_string)
    print '  open %s/all.xcworkspace' % DIRECTORY
    print ''
    print '2. Execute these Run targets manually with Xcode Run button and '
    print 'manually save generated coverage.profraw file to %s:' % DIRECTORY
    print '\n'.join('- %s' % t for t in TESTS)
    print ''
    print '3. Execute these Test targets manually with Xcode Test button and '
    print 'manually save generated coverage.profraw file to %s:' % DIRECTORY
    print '\n'.join('- %s' % t for t in XC_TESTS)
    print ''
    print '4. Merge *.profraw files to *.profdata using llvm-profdata tool:'
    print('  build/mac_files/Xcode.app/Contents/Developer/Toolchains/' +
          'XcodeDefault.xctoolchain/usr/bin/llvm-profdata merge ' +
          '-o %s/merged.profdata ' % DIRECTORY +
          '-sparse=true %s/*.profraw' % DIRECTORY)
    print ''
    print '5. Generate coverage report:'
    print '  ' + ' '.join(coverage_report_cmd)
    return 0


def Main():
    if len(sys.argv) < 2:
        print 'Please specify type of coverage:'
        print '  %s simulator' % sys.argv[0]
        print '  %s device' % sys.argv[0]
    elif sys.argv[1] == 'simulator':
        GenerateIOSSimulatorCommand()
    elif sys.argv[1] == 'device':
        GenerateIOSDeviceCommand()
    else:
        print 'Unsupported type of coverage'

    return 0


if __name__ == '__main__':
    sys.exit(Main())
