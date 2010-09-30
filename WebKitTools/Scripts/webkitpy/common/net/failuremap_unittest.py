# Copyright (c) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

import unittest

from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.failuremap import *
from webkitpy.common.net.regressionwindow import RegressionWindow
from webkitpy.tool.mocktool import MockBuilder


class FailureMapTest(unittest.TestCase):
    builder1 = MockBuilder("Builder1")
    builder2 = MockBuilder("Builder2")

    build1a = Build(builder1, build_number=22, revision=1233, is_green=True)
    build1b = Build(builder1, build_number=23, revision=1234, is_green=False)
    build2a = Build(builder2, build_number=89, revision=1233, is_green=True)
    build2b = Build(builder2, build_number=90, revision=1235, is_green=False)

    regression_window1 = RegressionWindow(build1a, build1b)
    regression_window2 = RegressionWindow(build2a, build2b)

    def _make_failure_map(self):
        failure_map = FailureMap()
        failure_map.add_regression_window(self.builder1, self.regression_window1)
        failure_map.add_regression_window(self.builder2, self.regression_window2)
        return failure_map

    def test_failing_revisions(self):
        failure_map = self._make_failure_map()
        self.assertEquals(failure_map.failing_revisions(), [1234, 1235])

    def test_new_failures(self):
        failure_map = self._make_failure_map()
        failure_map.filter_out_old_failures(lambda revision: False)
        self.assertEquals(failure_map.failing_revisions(), [1234, 1235])

    def test_new_failures_with_old_revisions(self):
        failure_map = self._make_failure_map()
        failure_map.filter_out_old_failures(lambda revision: revision == 1234)
        self.assertEquals(failure_map.failing_revisions(), [])

    def test_new_failures_with_more_old_revisions(self):
        failure_map = self._make_failure_map()
        failure_map.filter_out_old_failures(lambda revision: revision == 1235)
        self.assertEquals(failure_map.failing_revisions(), [1234])
