#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""
This script is the wrapper that starts a loopback call with stubbed video in
and out. It then analyses the video quality of the output video against the
reference input video.

It expect to be given the webrtc output build directory as the first argument
all other arguments are optional.

It assumes you have a Android device plugged in.
"""

import argparse
import json
import logging
import os
import subprocess
import sys
import tempfile
import time


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
RTC_TOOLS_DIR = os.path.join(SRC_DIR, 'rtc_tools', 'testing')
TOOLCHAIN_DIR = os.path.join(SRC_DIR, 'tools_webrtc', 'video_quality_toolchain',
                             'linux')
BAD_DEVICES_JSON = os.path.join(SRC_DIR,
                                os.environ.get('CHROMIUM_OUT_DIR', 'out'),
                                'bad_devices.json')

sys.path.append(RTC_TOOLS_DIR)
import utils


class Error(Exception):
  pass


class VideoQualityTestError(Error):
  pass


def _RunCommand(argv, **kwargs):
  logging.info('Running %r', argv)
  subprocess.check_call(argv, **kwargs)


def _RunCommandWithOutput(argv, **kwargs):
  logging.info('Running %r', argv)
  return subprocess.check_output(argv, **kwargs)


def _RunBackgroundCommand(argv):
  logging.info('Running %r', argv)
  process = subprocess.Popen(argv)
  time.sleep(0.5)
  status = process.poll()
  if status:  # is not None or 0
    raise subprocess.CalledProcessError(status, argv)
  return process


def CreateEmptyDir(suggested_dir):
  if not suggested_dir:
    return tempfile.mkdtemp()
  utils.RemoveDirectory(suggested_dir)
  os.makedirs(suggested_dir)
  return suggested_dir


def _ParseArgs():
  parser = argparse.ArgumentParser(description='Start loopback video analysis.')
  parser.add_argument('build_dir_android',
      help='The path to the build directory for Android.')
  parser.add_argument('--temp_dir',
      help='A temporary directory to put the output.')
  parser.add_argument('--adb-path', help='Path to adb binary.', default='adb')
  parser.add_argument('--num-retries', default='0',
                      help='Number of times to retry the test on Android.')
  parser.add_argument('--isolated-script-test-perf-output',
      help='Where to store perf results in chartjson format.', default=None)
  parser.add_argument('--isolated-script-test-output', default=None,
      help='Path to output an empty JSON file which Chromium infra requires.')
  args, unknown_args = parser.parse_known_args()

  # Ignore Chromium-specific flags
  parser = argparse.ArgumentParser()
  parser.add_argument('--test-launcher-summary-output',
                      type=str, default=None)

  parser.parse_args(unknown_args)

  return args


def SelectAndroidDevice(adb_path):
  # Select an Android device in case multiple are connected.
  try:
    with open(BAD_DEVICES_JSON) as bad_devices_file:
      bad_devices = json.load(bad_devices_file)
  except IOError:
    if os.environ.get('CHROME_HEADLESS'):
      logging.warning('Cannot read %r', BAD_DEVICES_JSON)
    bad_devices = {}

  for line in _RunCommandWithOutput([adb_path, 'devices']).splitlines():
    if line.endswith('\tdevice'):
      android_device = line.split('\t')[0]
      if android_device not in bad_devices:
        return android_device
  raise VideoQualityTestError('Cannot find any connected Android device.')


def SetUpTools(android_device, temp_dir, processes):
  # Extract AppRTC.
  apprtc_archive = os.path.join(RTC_TOOLS_DIR, 'prebuilt_apprtc.zip')
  golang_archive = os.path.join(RTC_TOOLS_DIR, 'golang', 'linux', 'go.tar.gz')

  utils.UnpackArchiveTo(apprtc_archive, temp_dir)
  utils.UnpackArchiveTo(golang_archive, temp_dir)

  # Build AppRTC.
  build_apprtc_script = os.path.join(RTC_TOOLS_DIR, 'build_apprtc.py')
  apprtc_dir = os.path.join(temp_dir, 'apprtc')
  go_dir = os.path.join(temp_dir, 'go')
  collider_dir = os.path.join(temp_dir, 'collider')

  _RunCommand([sys.executable, build_apprtc_script, apprtc_dir, go_dir,
               collider_dir])

  # Start AppRTC Server.
  dev_appserver = os.path.join(temp_dir, 'apprtc', 'temp', 'google-cloud-sdk',
                              'bin', 'dev_appserver.py')
  appengine_dir = os.path.join(temp_dir, 'apprtc', 'out', 'app_engine')
  processes.append(_RunBackgroundCommand([
      sys.executable, dev_appserver, appengine_dir, '--port=9999',
      '--admin_port=9998', '--skip_sdk_update_check', '--clear_datastore=yes']))

  # Start Collider.
  collider_path = os.path.join(temp_dir, 'collider', 'collidermain')
  processes.append(_RunBackgroundCommand([
      collider_path, '-tls=false', '-port=8089',
      '-room-server=http://localhost:9999']))

  # Start adb reverse forwarder.
  reverseforwarder_path = os.path.join(
      SRC_DIR, 'build', 'android', 'adb_reverse_forwarder.py')
  processes.append(_RunBackgroundCommand([
      reverseforwarder_path, '--device', android_device, '9999', '9999', '8089',
      '8089']))


def RunTest(android_device, adb_path, build_dir, temp_dir, num_retries,
            chartjson_result_file):
  ffmpeg_path = os.path.join(TOOLCHAIN_DIR, 'ffmpeg')
  def ConvertVideo(input_video, output_video):
    _RunCommand([ffmpeg_path, '-y', '-i', input_video, output_video])

  # Start loopback call and record video.
  test_script = os.path.join(
      build_dir, 'bin', 'run_AppRTCMobile_stubbed_video_io_test_apk')
  _RunCommand([test_script, '--device', android_device,
               '--num-retries', num_retries])

  # Pull the recorded video.
  test_video = os.path.join(temp_dir, 'test_video.y4m')
  _RunCommand([adb_path, '-s', android_device,
              'pull', '/sdcard/output.y4m', test_video])

  # Convert the recorded and reference videos to YUV.
  reference_video = os.path.join(SRC_DIR,
      'resources', 'reference_video_640x360_30fps.y4m')

  test_video_yuv = os.path.join(temp_dir, 'test_video.yuv')
  reference_video_yuv = os.path.join(
      temp_dir, 'reference_video_640x360_30fps.yuv')

  ConvertVideo(test_video, test_video_yuv)
  ConvertVideo(reference_video, reference_video_yuv)

  # Run comparison script.
  compare_script = os.path.join(SRC_DIR, 'rtc_tools', 'compare_videos.py')
  frame_analyzer = os.path.join(build_dir, 'frame_analyzer_host')
  zxing_path = os.path.join(TOOLCHAIN_DIR, 'zxing')
  stats_file_ref = os.path.join(temp_dir, 'stats_ref.txt')
  stats_file_test = os.path.join(temp_dir, 'stats_test.txt')

  args = [
      '--ref_video', reference_video_yuv,
      '--test_video', test_video_yuv,
      '--yuv_frame_width', '640',
      '--yuv_frame_height', '360',
      '--stats_file_ref', stats_file_ref,
      '--stats_file_test', stats_file_test,
      '--frame_analyzer', frame_analyzer,
      '--ffmpeg_path', ffmpeg_path,
      '--zxing_path', zxing_path,
  ]
  if chartjson_result_file:
    args.extend(['--chartjson_result_file', chartjson_result_file])

  _RunCommand([sys.executable, compare_script] + args)


def main():
  logging.basicConfig(level=logging.INFO)

  args = _ParseArgs()

  temp_dir = args.temp_dir
  build_dir = args.build_dir_android
  adb_path = args.adb_path

  processes = []
  temp_dir = CreateEmptyDir(temp_dir)
  try:
    android_device = SelectAndroidDevice(adb_path)
    SetUpTools(android_device, temp_dir, processes)
    RunTest(android_device, adb_path, build_dir, temp_dir, args.num_retries,
            args.isolated_script_test_perf_output)
  finally:
    for process in processes:
      if process:
        process.terminate()
        process.wait()

    utils.RemoveDirectory(temp_dir)

  if args.isolated_script_test_output:
    with open(args.isolated_script_test_output, 'w') as f:
      f.write('{"version": 3}')


if __name__ == '__main__':
  sys.exit(main())
