# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

""""Tests code paths not covered by the regular unit tests."""

import unittest

from webkitpy.layout_tests.layout_package.test_failures import *


class Test(unittest.TestCase):
    def assertResultHtml(self, failure_obj):
        self.assertNotEqual(failure_obj.result_html_output('foo'), None)

    def assert_loads(self, cls):
        failure_obj = cls()
        s = failure_obj.dumps()
        new_failure_obj = TestFailure.loads(s)
        self.assertTrue(isinstance(new_failure_obj, cls))

        self.assertEqual(failure_obj, new_failure_obj)

        # Also test that != is implemented.
        self.assertFalse(failure_obj != new_failure_obj)

    def test_crash(self):
        self.assertResultHtml(FailureCrash())

    def test_hash_incorrect(self):
        self.assertResultHtml(FailureImageHashIncorrect())

    def test_missing(self):
        self.assertResultHtml(FailureMissingResult())

    def test_missing_image(self):
        self.assertResultHtml(FailureMissingImage())

    def test_missing_image_hash(self):
        self.assertResultHtml(FailureMissingImageHash())

    def test_timeout(self):
        self.assertResultHtml(FailureTimeout())

    def test_unknown_failure_type(self):
        class UnknownFailure(TestFailure):
            pass

        failure_obj = UnknownFailure()
        self.assertRaises(ValueError, determine_result_type, [failure_obj])
        self.assertRaises(NotImplementedError, failure_obj.message)
        self.assertRaises(NotImplementedError, failure_obj.result_html_output,
                          "foo.txt")

    def test_loads(self):
        for c in ALL_FAILURE_CLASSES:
            self.assert_loads(c)

if __name__ == '__main__':
    unittest.main()
