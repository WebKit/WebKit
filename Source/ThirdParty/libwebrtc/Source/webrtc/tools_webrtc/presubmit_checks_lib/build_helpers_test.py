#!/usr/bin/env vpython3

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import re
import os
import unittest

import build_helpers

TESTDATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            'testdata')


class GnCheckTest(unittest.TestCase):

    def test_circular_dependency_error(self):
        test_dir = os.path.join(TESTDATA_DIR, 'circular_dependency')
        expected_error = re.compile('ERROR Dependency cycle')
        gn_output = build_helpers.run_gn_check(test_dir)
        self.assertEqual(1, len(gn_output))
        self.assertRegex(gn_output[0], expected_error)


if __name__ == '__main__':
    unittest.main()
