#!/usr/bin/env vpython3

# Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import unittest
from unittest import mock

import apply_include_cleaner

_GTEST_KEY = '"gtest/gtest.h"'
_GTEST_VALUE = '"test/gtest.h"'


class ApplyIncludeCleanerTest(unittest.TestCase):

    _OUTPUT = 'cleaner output'

    @mock.patch('subprocess.run',
                return_value=mock.Mock(stdout=_OUTPUT, returncode=0))
    def test_no_modification(self, mock_subprocess):
        file = mock.Mock()
        file.read_text.return_value = '#include stuff'
        output = apply_include_cleaner.apply_include_cleaner_to_file(
            file, should_modify=True, cmd=[])
        self.assertEqual(output, self._OUTPUT)
        mock_subprocess.assert_called_once()
        file.write_text.assert_not_called()

    @mock.patch('subprocess.run',
                return_value=mock.Mock(stdout=_OUTPUT, returncode=0))
    def test_content_modification(self, mock_subprocess):
        file = mock.Mock()
        file.read_text.return_value = '#include "libyuv/something.h"'
        output = apply_include_cleaner.apply_include_cleaner_to_file(
            file, should_modify=True, cmd=[])
        self.assertEqual(output, self._OUTPUT)
        mock_subprocess.assert_called_once()
        file.write_text.assert_called_once_with(
            '#include "third_party/libyuv/include/libyuv/something.h"')

    @mock.patch('subprocess.run',
                return_value=mock.Mock(stdout=_OUTPUT, returncode=0))
    def test_pragma_no_modification(self, mock_subprocess):
        file = mock.Mock()
        file.read_text.return_value = '#include <sys/socket.h>  // IWYU'
        output = apply_include_cleaner.apply_include_cleaner_to_file(
            file, should_modify=True, cmd=[])
        self.assertEqual(output, self._OUTPUT)
        mock_subprocess.assert_called_once()
        file.write_text.assert_not_called()

    @mock.patch('subprocess.run',
                return_value=mock.Mock(stdout=f'+ {_GTEST_KEY}\n',
                                       returncode=0))
    def test_gtest_output_modification(self, mock_subprocess):
        file = mock.Mock()
        file.read_text.return_value = f'#include {_GTEST_VALUE}'
        output = apply_include_cleaner.apply_include_cleaner_to_file(
            file, should_modify=True, cmd=[])
        self.assertEqual(output, '')
        mock_subprocess.assert_called_once()
        file.write_text.assert_not_called()

    @mock.patch('subprocess.run',
                return_value=mock.Mock(stdout=f'+ {_GTEST_KEY}\n',
                                       returncode=0))
    def test_gtest_output_no_modification(self, mock_subprocess):
        file = mock.Mock()
        file.read_text.return_value = '#include stuff'
        output = apply_include_cleaner.apply_include_cleaner_to_file(
            file, should_modify=True, cmd=[])
        self.assertEqual(output, f'+ {_GTEST_KEY}')
        mock_subprocess.assert_called_once()
        file.write_text.assert_not_called()


if (__name__) == '__main__':
    unittest.main()
