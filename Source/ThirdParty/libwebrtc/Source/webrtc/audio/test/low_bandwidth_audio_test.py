#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
This script is the wrapper that runs the low-bandwidth audio test.

After running the test, post-process steps for calculating audio quality of the
output files will be performed.
"""

import argparse
import collections
import logging
import os
import re
import shutil
import subprocess
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir,
                                        os.pardir))


def _LogCommand(command):
  logging.info('Running %r', command)
  return command


def _ParseArgs():
  parser = argparse.ArgumentParser(description='Run low-bandwidth audio tests.')
  parser.add_argument('build_dir',
      help='Path to the build directory (e.g. out/Release).')
  parser.add_argument('--remove', action='store_true',
      help='Remove output audio files after testing.')
  parser.add_argument('--android', action='store_true',
      help='Perform the test on a connected Android device instead.')
  parser.add_argument('--adb-path', help='Path to adb binary.', default='adb')
  args = parser.parse_args()
  return args


def _GetPlatform():
  if sys.platform == 'win32':
    return 'win'
  elif sys.platform == 'darwin':
    return 'mac'
  elif sys.platform.startswith('linux'):
    return 'linux'


def _DownloadTools():
  tools_dir = os.path.join(SRC_DIR, 'tools_webrtc')
  toolchain_dir = os.path.join(tools_dir, 'audio_quality')

  # Download PESQ and POLQA.
  download_script = os.path.join(tools_dir, 'download_tools.py')
  command = [sys.executable, download_script, toolchain_dir]
  subprocess.check_call(_LogCommand(command))

  pesq_path = os.path.join(toolchain_dir, _GetPlatform(), 'pesq')
  polqa_path = os.path.join(toolchain_dir, _GetPlatform(), 'PolqaOem64')
  return pesq_path, polqa_path


def ExtractTestRuns(lines, echo=False):
  """Extracts information about tests from the output of a test runner.

  Produces tuples (android_device, test_name, reference_file, degraded_file).
  """
  for line in lines:
    if echo:
      sys.stdout.write(line)

    # Output from Android has a prefix with the device name.
    android_prefix_re = r'(?:I\b.+\brun_tests_on_device\((.+?)\)\s*)?'
    test_re = r'^' + android_prefix_re + r'TEST (\w+) ([^ ]+?) ([^ ]+?)\s*$'

    match = re.search(test_re, line)
    if match:
      yield match.groups()


def _GetFile(file_path, out_dir, move=False,
             android=False, adb_prefix=('adb',)):
  out_file_name = os.path.basename(file_path)
  out_file_path = os.path.join(out_dir, out_file_name)

  if android:
    # Pull the file from the connected Android device.
    adb_command = adb_prefix + ('pull', file_path, out_dir)
    subprocess.check_call(_LogCommand(adb_command))
    if move:
      # Remove that file.
      adb_command = adb_prefix + ('shell', 'rm', file_path)
      subprocess.check_call(_LogCommand(adb_command))
  elif os.path.abspath(file_path) != os.path.abspath(out_file_path):
    if move:
      shutil.move(file_path, out_file_path)
    else:
      shutil.copy(file_path, out_file_path)

  return out_file_path


def _RunPesq(executable_path, reference_file, degraded_file,
             sample_rate_hz=16000):
  directory = os.path.dirname(reference_file)
  assert os.path.dirname(degraded_file) == directory

  # Analyze audio.
  command = [executable_path, '+%d' % sample_rate_hz,
             os.path.basename(reference_file),
             os.path.basename(degraded_file)]
  # Need to provide paths in the current directory due to a bug in PESQ:
  # On Mac, for some 'path/to/file.wav', if 'file.wav' is longer than
  # 'path/to', PESQ crashes.
  out = subprocess.check_output(_LogCommand(command),
                                cwd=directory, stderr=subprocess.STDOUT)

  # Find the scores in stdout of PESQ.
  match = re.search(
      r'Prediction \(Raw MOS, MOS-LQO\):\s+=\s+([\d.]+)\s+([\d.]+)', out)
  if match:
    raw_mos, _ = match.groups()

    return {'pesq_mos': (raw_mos, 'score')}
  else:
    logging.error('PESQ: %s', out.splitlines()[-1])
    return {}


def _RunPolqa(executable_path, reference_file, degraded_file):
  # Analyze audio.
  command = [executable_path, '-q', '-LC', 'NB',
             '-Ref', reference_file, '-Test', degraded_file]
  try:
    process = subprocess.Popen(_LogCommand(command),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  except OSError as e:
    if e.errno == os.errno.ENOENT:
      logging.warning('POLQA executable missing, skipping test.')
      return {}
    else:
      raise
  out, err = process.communicate()

  # Find the scores in stdout of POLQA.
  match = re.search(r'\bMOS-LQO:\s+([\d.]+)', out)

  if process.returncode != 0 or not match:
    if process.returncode == 2:
      logging.warning('%s (2)', err.strip())
      logging.warning('POLQA license error, skipping test.')
    else:
      logging.error('%s (%d)', err.strip(), process.returncode)
    return {}

  mos_lqo, = match.groups()
  return {'polqa_mos_lqo': (mos_lqo, 'score')}


Analyzer = collections.namedtuple('Analyzer', ['func', 'executable',
                                               'sample_rate_hz'])


def main():
  # pylint: disable=W0101
  logging.basicConfig(level=logging.INFO)

  args = _ParseArgs()

  pesq_path, polqa_path = _DownloadTools()

  out_dir = os.path.join(args.build_dir, '..')
  if args.android:
    test_command = [os.path.join(args.build_dir, 'bin',
                                 'run_low_bandwidth_audio_test'), '-v']
  else:
    test_command = [os.path.join(args.build_dir, 'low_bandwidth_audio_test')]

  analyzers = [Analyzer(_RunPesq, pesq_path, 16000)]
  # Check if POLQA can run at all, or skip the 48 kHz tests entirely.
  example_path = os.path.join(SRC_DIR, 'resources',
                              'voice_engine', 'audio_tiny48.wav')
  if _RunPolqa(polqa_path, example_path, example_path):
    analyzers.append(Analyzer(_RunPolqa, polqa_path, 48000))

  for analyzer in analyzers:
    # Start the test executable that produces audio files.
    test_process = subprocess.Popen(
        _LogCommand(test_command + ['--sample_rate_hz=%d' %
                                    analyzer.sample_rate_hz]),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
      lines = iter(test_process.stdout.readline, '')
      for result in ExtractTestRuns(lines, echo=True):
        (android_device, test_name, reference_file, degraded_file) = result

        adb_prefix = (args.adb_path,)
        if android_device:
          adb_prefix += ('-s', android_device)

        reference_file = _GetFile(reference_file, out_dir,
                                  android=args.android, adb_prefix=adb_prefix)
        degraded_file = _GetFile(degraded_file, out_dir, move=True,
                                 android=args.android, adb_prefix=adb_prefix)

        analyzer_results = analyzer.func(analyzer.executable,
                                         reference_file, degraded_file)
        for metric, (value, units) in analyzer_results.items():
          # Output a result for the perf dashboard.
          print 'RESULT %s: %s= %s %s' % (metric, test_name, value, units)

        if args.remove:
          os.remove(reference_file)
          os.remove(degraded_file)
    finally:
      test_process.terminate()

  return test_process.wait()


if __name__ == '__main__':
  sys.exit(main())
