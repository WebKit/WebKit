# Copyright (C) 2020 Apple Inc. All rights reserved.
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
import unittest

from webkitscmpy import local, mocks


class TestSvn(unittest.TestCase):
    path = '/mock/repository'

    def test_detection(self):
        with mocks.local.Svn(self.path), mocks.local.Git():
            detect = local.Scm.from_path(self.path)
            self.assertEqual(detect.executable, local.Svn.executable)

    def test_root(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(local.Svn(self.path).root_path, self.path)

            with self.assertRaises(OSError):
                local.Svn(os.path.dirname(self.path)).root_path

    def test_branch(self):
        with mocks.local.Svn(self.path):
            self.assertEqual(local.Svn(self.path).branch, 'trunk')

    def test_remote(self):
        with mocks.local.Svn(self.path) as repo:
            self.assertEqual(local.Svn(self.path).remote(), repo.remote)
