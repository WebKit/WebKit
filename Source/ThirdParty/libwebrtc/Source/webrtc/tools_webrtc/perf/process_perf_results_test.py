#!/usr/bin/env vpython3

# Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import sys

import unittest
from unittest import mock

_SCRIPT_DIR = os.path.dirname(__file__)
_SRC_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, '..', '..'))

sys.path.insert(0, os.path.join(_SRC_DIR, 'third_party', 'protobuf', 'python'))
import process_perf_results


class ProcessPerfResultsTest(unittest.TestCase):
  def testConfigurePythonPath(self):
    # pylint: disable=protected-access
    self.assertEqual(
        0,
        process_perf_results._ConfigurePythonPath(
            os.path.join(_SRC_DIR, 'out/Default')))

  def testUploadToDasboard(self):
    outdir = os.path.join(_SRC_DIR, 'out/Default')
    args = mock.Mock(
        build_properties='{' + '"outdir":"' + outdir + '", ' +
        '"perf_dashboard_machine_group":"mock_machine_group", ' +
        '"bot":"mock_bot", ' + '"webrtc_git_hash":"mock_webrtc_git_hash", ' +
        '"commit_position":"123456", ' +
        '"build_page_url":"mock_build_page_url", ' +
        '"dashboard_url":"mock_dashboard_url"' + '}',
        summary_json='mock_sumary_json',
        task_output_dir='mock_task_output_dir',
        test_suite='mock_test_suite',
    )
    perftest_output = mock.Mock(
        absolute=lambda: 'dummy_path/perftest-output.pb',
        is_file=lambda: True,
    )
    with mock.patch('pathlib.Path.rglob') as mocked_rglob:
      with mock.patch('catapult_uploader.UploadToDashboard') as mocked_upload:
        mocked_rglob.return_value = [perftest_output]
        mocked_upload.return_value = 0
        # pylint: disable=protected-access
        self.assertEqual(0, process_perf_results._UploadToDasboard(args))

        import catapult_uploader
        mocked_upload.assert_called_once_with(
            catapult_uploader.UploaderOptions(
                perf_dashboard_machine_group='mock_machine_group',
                bot='mock_bot',
                test_suite='mock_test_suite',
                webrtc_git_hash='mock_webrtc_git_hash',
                commit_position='123456',
                build_page_url='mock_build_page_url',
                dashboard_url='mock_dashboard_url',
                input_results_file=perftest_output.absolute()))


if (__name__) == '__main__':
  unittest.main()
