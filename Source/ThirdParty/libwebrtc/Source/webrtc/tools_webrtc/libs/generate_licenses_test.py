#!/usr/bin/env python
# pylint: disable=relative-import,protected-access,unused-argument

#  Copyright 2017 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import os
import sys

SRC = os.path.abspath(os.path.join(
                      os.path.dirname((__file__)), os.pardir, os.pardir))
sys.path.append(os.path.join(SRC, 'third_party', 'pymock'))

import unittest
import mock

from generate_licenses import LicenseBuilder


class TestLicenseBuilder(unittest.TestCase):
  @staticmethod
  def _FakeRunGN(buildfile_dir, target):
    return """
    {
      "target1": {
        "deps": [
          "//a/b/third_party/libname1:c",
          "//a/b/third_party/libname2:c(//d/e/f:g)",
          "//a/b/third_party/libname3/c:d(//e/f/g:h)",
          "//a/b/not_third_party/c"
        ]
      }
    }
    """

  def testParseLibrary(self):
    self.assertEquals(LicenseBuilder._ParseLibrary(
            '//a/b/third_party/libname1:c').group(1),
        'libname1')
    self.assertEquals(LicenseBuilder._ParseLibrary(
            '//a/b/third_party/libname2:c(d)').group(1),
        'libname2')
    self.assertEquals(LicenseBuilder._ParseLibrary(
            '//a/b/third_party/libname3/c:d(e)').group(1),
        'libname3')
    self.assertEquals(LicenseBuilder._ParseLibrary('//a/b/not_third_party/c'),
        None)

  @mock.patch('generate_licenses.LicenseBuilder._RunGN', _FakeRunGN)
  def testGetThirdPartyLibraries(self):
    self.assertEquals(LicenseBuilder._GetThirdPartyLibraries(
            'out/arm', 'target1'),
        set(['libname1', 'libname2', 'libname3']))


if __name__ == '__main__':
  unittest.main()
