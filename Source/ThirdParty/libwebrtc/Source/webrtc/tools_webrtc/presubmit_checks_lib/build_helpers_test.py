#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import unittest

#pylint: disable=relative-import
import build_helpers

TESTDATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            'testdata')


class GnCheckTest(unittest.TestCase):
    def testCircularDependencyError(self):
        test_dir = os.path.join(TESTDATA_DIR, 'circular_dependency')
        expected_errors = [
            'ERROR Dependency cycle:\n'
            '  //:bar ->\n  //:foo ->\n  //:bar'
        ]
        self.assertListEqual(expected_errors,
                             build_helpers.RunGnCheck(test_dir))


if __name__ == '__main__':
    unittest.main()
