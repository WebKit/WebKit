#!/usr/bin/env python
# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import json
import optparse
import os
import shutil
import subprocess
import sys
import tempfile


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Chrome browsertests will throw away stderr; avoid that output gets lost.
sys.stderr = sys.stdout


def _ParseArgs():
  """Registers the command-line options."""
  usage = 'usage: %prog [options]'
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--label', type='string', default='MY_TEST',
                    help=('Label of the test, used to identify different '
                          'tests. Default: %default'))
  parser.add_option('--ref_video', type='string',
                    help='Reference video to compare with (YUV).')
  parser.add_option('--test_video', type='string',
                    help=('Test video to be compared with the reference '
                          'video (YUV).'))
  parser.add_option('--frame_analyzer', type='string',
                    help='Path to the frame analyzer executable.')
  parser.add_option('--aligned_output_file', type='string',
                    help='Path for output aligned YUV or Y4M file.')
  parser.add_option('--vmaf', type='string',
                    help='Path to VMAF executable.')
  parser.add_option('--vmaf_model', type='string',
                    help='Path to VMAF model.')
  parser.add_option('--vmaf_phone_model', action='store_true',
                    help='Whether to use phone model in VMAF.')
  parser.add_option('--barcode_decoder', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--ffmpeg_path', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--zxing_path', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--stats_file_ref', type='string', default='stats_ref.txt',
                    help=('DEPRECATED'))
  parser.add_option('--stats_file_test', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--stats_file', type='string',
                    help=('DEPRECATED'))
  parser.add_option('--yuv_frame_width', type='int', default=640,
                    help='Width of the YUV file\'s frames. Default: %default')
  parser.add_option('--yuv_frame_height', type='int', default=480,
                    help='Height of the YUV file\'s frames. Default: %default')
  parser.add_option('--chartjson_result_file', type='str', default=None,
                    help='Where to store perf results in chartjson format.')
  options, _ = parser.parse_args()

  if not options.ref_video:
    parser.error('You must provide a path to the reference video!')
  if not os.path.exists(options.ref_video):
    parser.error('Cannot find the reference video at %s' % options.ref_video)

  if not options.test_video:
    parser.error('You must provide a path to the test video!')
  if not os.path.exists(options.test_video):
    parser.error('Cannot find the test video at %s' % options.test_video)

  if not options.frame_analyzer:
    parser.error('You must provide the path to the frame analyzer executable!')
  if not os.path.exists(options.frame_analyzer):
    parser.error('Cannot find frame analyzer executable at %s!' %
                 options.frame_analyzer)

  if options.vmaf and not options.vmaf_model:
    parser.error('You must provide a path to a VMAF model to use VMAF.')

  return options

def _DevNull():
  """On Windows, sometimes the inherited stdin handle from the parent process
  fails. Workaround this by passing null to stdin to the subprocesses commands.
  This function can be used to create the null file handler.
  """
  return open(os.devnull, 'r')


def _RunFrameAnalyzer(options, yuv_directory=None):
  """Run frame analyzer to compare the videos and print output."""
  cmd = [
    options.frame_analyzer,
    '--label=%s' % options.label,
    '--reference_file=%s' % options.ref_video,
    '--test_file=%s' % options.test_video,
    '--stats_file_ref=%s' % options.stats_file_ref,
    '--stats_file_test=%s' % options.stats_file_test,
    '--width=%d' % options.yuv_frame_width,
    '--height=%d' % options.yuv_frame_height,
  ]
  if options.chartjson_result_file:
    cmd.append('--chartjson_result_file=%s' % options.chartjson_result_file)
  if options.aligned_output_file:
    cmd.append('--aligned_output_file=%s' % options.aligned_output_file)
  if yuv_directory:
    cmd.append('--yuv_directory=%s' % yuv_directory)
  frame_analyzer = subprocess.Popen(cmd, stdin=_DevNull(),
                                    stdout=sys.stdout, stderr=sys.stderr)
  frame_analyzer.wait()
  if frame_analyzer.returncode != 0:
    print 'Failed to run frame analyzer.'
  return frame_analyzer.returncode


def _RunVmaf(options, yuv_directory, logfile):
  """ Run VMAF to compare videos and print output.

  The yuv_directory is assumed to have been populated with a reference and test
  video in .yuv format, with names according to the label.
  """
  cmd = [
    options.vmaf,
    'yuv420p',
    str(options.yuv_frame_width),
    str(options.yuv_frame_height),
    os.path.join(yuv_directory, "ref.yuv"),
    os.path.join(yuv_directory, "test.yuv"),
    options.vmaf_model,
    '--log',
    logfile,
    '--log-fmt',
    'json',
  ]
  if options.vmaf_phone_model:
    cmd.append('--phone-model')

  vmaf = subprocess.Popen(cmd, stdin=_DevNull(),
                          stdout=sys.stdout, stderr=sys.stderr)
  vmaf.wait()
  if vmaf.returncode != 0:
    print 'Failed to run VMAF.'
    return 1

  # Read per-frame scores from VMAF output and print.
  with open(logfile) as f:
    vmaf_data = json.load(f)
    vmaf_scores = []
    for frame in vmaf_data['frames']:
      vmaf_scores.append(frame['metrics']['vmaf'])
    print 'RESULT VMAF: %s=' % options.label, vmaf_scores

  return 0


def main():
  """The main function.

  A simple invocation is:
  ./webrtc/rtc_tools/compare_videos.py
  --ref_video=<path_and_name_of_reference_video>
  --test_video=<path_and_name_of_test_video>
  --frame_analyzer=<path_and_name_of_the_frame_analyzer_executable>

  Running vmaf requires the following arguments:
  --vmaf, --vmaf_model, --yuv_frame_width, --yuv_frame_height
  """
  options = _ParseArgs()

  if options.vmaf:
    try:
      # Directory to save temporary YUV files for VMAF in frame_analyzer.
      yuv_directory = tempfile.mkdtemp()
      _, vmaf_logfile = tempfile.mkstemp()

      # Run frame analyzer to compare the videos and print output.
      if _RunFrameAnalyzer(options, yuv_directory=yuv_directory) != 0:
        return 1

      # Run VMAF for further video comparison and print output.
      return _RunVmaf(options, yuv_directory, vmaf_logfile)
    finally:
      shutil.rmtree(yuv_directory)
      os.remove(vmaf_logfile)
  else:
    return _RunFrameAnalyzer(options)

  return 0

if __name__ == '__main__':
  sys.exit(main())
