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
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))

NO_TOOLS_ERROR_MESSAGE = (
  'Could not find PESQ or POLQA at %s.\n'
  '\n'
  'To fix this run:\n'
  '  python %s %s\n'
  '\n'
  'Note that these tools are Google-internal due to licensing, so in order to '
  'use them you will have to get your own license and manually put them in the '
  'right location.\n'
  'See https://cs.chromium.org/chromium/src/third_party/webrtc/tools_webrtc/'
  'download_tools.py?rcl=bbceb76f540159e2dba0701ac03c514f01624130&l=13')


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
  parser.add_argument('--num-retries', default='0',
                      help='Number of times to retry the test on Android.')
  parser.add_argument('--isolated-script-test-perf-output', default=None,
      help='Path to store perf results in histogram proto format.')
  parser.add_argument('--extra-test-args', default=[], action='append',
      help='Extra args to path to the test binary.')

  # Ignore Chromium-specific flags
  parser.add_argument('--test-launcher-summary-output',
                      type=str, default=None)
  args = parser.parse_args()

  return args


def _GetPlatform():
  if sys.platform == 'win32':
    return 'win'
  elif sys.platform == 'darwin':
    return 'mac'
  elif sys.platform.startswith('linux'):
    return 'linux'


def _GetExtension():
  return '.exe' if sys.platform == 'win32' else ''


def _GetPathToTools():
  tools_dir = os.path.join(SRC_DIR, 'tools_webrtc')
  toolchain_dir = os.path.join(tools_dir, 'audio_quality')

  platform = _GetPlatform()
  ext = _GetExtension()

  pesq_path = os.path.join(toolchain_dir, platform, 'pesq' + ext)
  if not os.path.isfile(pesq_path):
    pesq_path = None

  polqa_path = os.path.join(toolchain_dir, platform, 'PolqaOem64' + ext)
  if not os.path.isfile(polqa_path):
    polqa_path = None

  if (platform != 'mac' and not polqa_path) or not pesq_path:
    logging.error(NO_TOOLS_ERROR_MESSAGE,
                  toolchain_dir,
                  os.path.join(tools_dir, 'download_tools.py'),
                  toolchain_dir)

  return pesq_path, polqa_path


def ExtractTestRuns(lines, echo=False):
  """Extracts information about tests from the output of a test runner.

  Produces tuples
  (android_device, test_name, reference_file, degraded_file, cur_perf_results).
  """
  for line in lines:
    if echo:
      sys.stdout.write(line)

    # Output from Android has a prefix with the device name.
    android_prefix_re = r'(?:I\b.+\brun_tests_on_device\((.+?)\)\s*)?'
    test_re = r'^' + android_prefix_re + (r'TEST (\w+) ([^ ]+?) ([^\s]+)'
                                          r' ?([^\s]+)?\s*$')

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

    return {'pesq_mos': (raw_mos, 'unitless')}
  else:
    logging.error('PESQ: %s', out.splitlines()[-1])
    return {}


def _RunPolqa(executable_path, reference_file, degraded_file):
  # Analyze audio.
  command = [executable_path, '-q', '-LC', 'NB',
             '-Ref', reference_file, '-Test', degraded_file]
  process = subprocess.Popen(_LogCommand(command),
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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
  return {'polqa_mos_lqo': (mos_lqo, 'unitless')}


def _MergeInPerfResultsFromCcTests(histograms, run_perf_results_file):
  from tracing.value import histogram_set

  cc_histograms = histogram_set.HistogramSet()
  with open(run_perf_results_file, 'rb') as f:
    contents = f.read()
    if not contents:
      return

    cc_histograms.ImportProto(contents)

  histograms.Merge(cc_histograms)


Analyzer = collections.namedtuple('Analyzer', ['name', 'func', 'executable',
                                               'sample_rate_hz'])


def _ConfigurePythonPath(args):
  script_dir = os.path.dirname(os.path.realpath(__file__))
  checkout_root = os.path.abspath(
      os.path.join(script_dir, os.pardir, os.pardir))

  # TODO(https://crbug.com/1029452): Use a copy rule and add these from the out
  # dir like for the third_party/protobuf code.
  sys.path.insert(0, os.path.join(checkout_root, 'third_party', 'catapult',
                                  'tracing'))

  # The low_bandwidth_audio_perf_test gn rule will build the protobuf stub for
  # python, so put it in the path for this script before we attempt to import
  # it.
  histogram_proto_path = os.path.join(
      os.path.abspath(args.build_dir), 'pyproto', 'tracing', 'tracing', 'proto')
  sys.path.insert(0, histogram_proto_path)
  proto_stub_path = os.path.join(os.path.abspath(args.build_dir), 'pyproto')
  sys.path.insert(0, proto_stub_path)

  # Fail early in case the proto hasn't been built.
  try:
    import histogram_pb2
  except ImportError as e:
    logging.exception(e)
    raise ImportError('Could not import histogram_pb2. You need to build the '
                      'low_bandwidth_audio_perf_test target before invoking '
                      'this script. Expected to find '
                      'histogram_pb2.py in %s.' % histogram_proto_path)


def main():
  # pylint: disable=W0101
  logging.basicConfig(level=logging.INFO)
  logging.info('Invoked with %s', str(sys.argv))

  args = _ParseArgs()

  _ConfigurePythonPath(args)

  # Import catapult modules here after configuring the pythonpath.
  from tracing.value import histogram_set
  from tracing.value.diagnostics import reserved_infos
  from tracing.value.diagnostics import generic_set

  pesq_path, polqa_path = _GetPathToTools()
  if pesq_path is None:
    return 1

  out_dir = os.path.join(args.build_dir, '..')
  if args.android:
    test_command = [os.path.join(args.build_dir, 'bin',
                                 'run_low_bandwidth_audio_test'),
                    '-v', '--num-retries', args.num_retries]
  else:
    test_command = [os.path.join(args.build_dir, 'low_bandwidth_audio_test')]

  analyzers = [Analyzer('pesq', _RunPesq, pesq_path, 16000)]
  # Check if POLQA can run at all, or skip the 48 kHz tests entirely.
  example_path = os.path.join(SRC_DIR, 'resources',
                              'voice_engine', 'audio_tiny48.wav')
  if polqa_path and _RunPolqa(polqa_path, example_path, example_path):
    analyzers.append(Analyzer('polqa', _RunPolqa, polqa_path, 48000))

  histograms = histogram_set.HistogramSet()
  for analyzer in analyzers:
    # Start the test executable that produces audio files.
    test_process = subprocess.Popen(
        _LogCommand(test_command + [
            '--sample_rate_hz=%d' % analyzer.sample_rate_hz,
            '--test_case_prefix=%s' % analyzer.name,
          ] + args.extra_test_args),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    perf_results_file = None
    try:
      lines = iter(test_process.stdout.readline, '')
      for result in ExtractTestRuns(lines, echo=True):
        (android_device, test_name, reference_file, degraded_file,
         perf_results_file) = result

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
          hist = histograms.CreateHistogram(metric, units, [value])
          user_story = generic_set.GenericSet([test_name])
          hist.diagnostics[reserved_infos.STORIES.name] = user_story

          # Output human readable results.
          print 'RESULT %s: %s= %s %s' % (metric, test_name, value, units)

        if args.remove:
          os.remove(reference_file)
          os.remove(degraded_file)
    finally:
      test_process.terminate()
    if perf_results_file:
      perf_results_file = _GetFile(perf_results_file, out_dir, move=True,
                           android=args.android, adb_prefix=adb_prefix)
      _MergeInPerfResultsFromCcTests(histograms, perf_results_file)
      if args.remove:
        os.remove(perf_results_file)

  if args.isolated_script_test_perf_output:
    with open(args.isolated_script_test_perf_output, 'wb') as f:
      f.write(histograms.AsProto().SerializeToString())

  return test_process.wait()


if __name__ == '__main__':
  sys.exit(main())
