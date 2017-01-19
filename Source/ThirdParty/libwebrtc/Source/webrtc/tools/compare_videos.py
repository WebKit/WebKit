#!/usr/bin/env python
# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

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
  parser.add_option('--barcode_decoder', type='string',
                    help=('Path to the barcode decoder script. By default, we '
                          'will assume we can find it in barcode_tools/'
                          'relative to this directory.'))
  parser.add_option('--ffmpeg_path', type='string',
                    help=('The path to where the ffmpeg executable is located. '
                          'If omitted, it will be assumed to be present in the '
                          'PATH with the name ffmpeg[.exe].'))
  parser.add_option('--zxing_path', type='string',
                    help=('The path to where the zxing executable is located. '
                          'If omitted, it will be assumed to be present in the '
                          'PATH with the name zxing[.exe].'))
  parser.add_option('--stats_file', type='string', default='stats.txt',
                    help=('Path to the temporary stats file to be created and '
                          'used. Default: %default'))
  parser.add_option('--yuv_frame_width', type='int', default=640,
                    help='Width of the YUV file\'s frames. Default: %default')
  parser.add_option('--yuv_frame_height', type='int', default=480,
                    help='Height of the YUV file\'s frames. Default: %default')
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
  return options


def main():
  """The main function.

  A simple invocation is:
  ./webrtc/tools/barcode_tools/compare_videos.py
  --ref_video=<path_and_name_of_reference_video>
  --test_video=<path_and_name_of_test_video>
  --frame_analyzer=<path_and_name_of_the_frame_analyzer_executable>

  Notice that the prerequisites for barcode_decoder.py also applies to this
  script. The means the following executables have to be available in the PATH:
  * zxing
  * ffmpeg
  """
  options = _ParseArgs()

  if options.barcode_decoder:
    path_to_decoder = options.barcode_decoder
  else:
    path_to_decoder = os.path.join(SCRIPT_DIR, 'barcode_tools',
                                   'barcode_decoder.py')

  # On Windows, sometimes the inherited stdin handle from the parent process
  # fails. Work around this by passing null to stdin to the subprocesses.
  null_filehandle = open(os.devnull, 'r')

  # Run barcode decoder on the test video to identify frame numbers.
  png_working_directory = tempfile.mkdtemp()
  cmd = [
    sys.executable,
    path_to_decoder,
    '--yuv_file=%s' % options.test_video,
    '--yuv_frame_width=%d' % options.yuv_frame_width,
    '--yuv_frame_height=%d' % options.yuv_frame_height,
    '--stats_file=%s' % options.stats_file,
    '--png_working_dir=%s' % png_working_directory,
  ]
  if options.zxing_path:
    cmd.append('--zxing_path=%s' % options.zxing_path)
  if options.ffmpeg_path:
    cmd.append('--ffmpeg_path=%s' % options.ffmpeg_path)
  barcode_decoder = subprocess.Popen(cmd, stdin=null_filehandle,
                                     stdout=sys.stdout, stderr=sys.stderr)
  barcode_decoder.wait()

  shutil.rmtree(png_working_directory)
  if barcode_decoder.returncode != 0:
    print 'Failed to run barcode decoder script.'
    return 1

  # Run frame analyzer to compare the videos and print output.
  cmd = [
    options.frame_analyzer,
    '--label=%s' % options.label,
    '--reference_file=%s' % options.ref_video,
    '--test_file=%s' % options.test_video,
    '--stats_file=%s' % options.stats_file,
    '--width=%d' % options.yuv_frame_width,
    '--height=%d' % options.yuv_frame_height,
  ]
  frame_analyzer = subprocess.Popen(cmd, stdin=null_filehandle,
                                    stdout=sys.stdout, stderr=sys.stderr)
  frame_analyzer.wait()
  if frame_analyzer.returncode != 0:
    print 'Failed to run frame analyzer.'
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main())
