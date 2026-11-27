# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import tempfile
import unittest
from unittest import mock

from webkitpy.safer_cpp.analyzer import find_scan_build


def touch_executable(path):
    with open(path, 'w') as f:
        f.write('#!/bin/sh\n')
    os.chmod(path, 0o755)


class FindScanBuildTest(unittest.TestCase):
    def test_prefers_the_newest_versioned_name_in_path(self):
        with tempfile.TemporaryDirectory() as directory:
            for name in ('scan-build', 'scan-build-22', 'scan-build-23'):
                touch_executable(os.path.join(directory, name))
            with mock.patch.dict(os.environ, {'PATH': directory}):
                self.assertEqual(find_scan_build(), os.path.join(directory, 'scan-build-23'))

    def test_prefers_the_one_next_to_the_analyzer(self):
        with tempfile.TemporaryDirectory() as directory:
            toolchain = os.path.join(directory, 'toolchain')
            os.mkdir(toolchain)
            touch_executable(os.path.join(toolchain, 'scan-build'))
            touch_executable(os.path.join(directory, 'scan-build-23'))
            with mock.patch.dict(os.environ, {'PATH': directory}):
                self.assertEqual(find_scan_build(os.path.join(toolchain, 'clang++')), os.path.join(toolchain, 'scan-build'))
                self.assertEqual(find_scan_build(os.path.join(directory, 'clang++')), os.path.join(directory, 'scan-build-23'))

    def test_none_when_absent(self):
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.dict(os.environ, {'PATH': directory}):
                self.assertIsNone(find_scan_build())
