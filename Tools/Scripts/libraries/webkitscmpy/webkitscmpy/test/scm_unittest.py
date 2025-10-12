# Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

from webkitcorepy import testing
from webkitscmpy import local, mocks


class TestScm(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestScm, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_detection(self):
        with mocks.local.Git(), mocks.local.Svn():
            with self.assertRaises(OSError):
                local.Scm.from_path(self.path)

    def test_root(self):
        with self.assertRaises(NotImplementedError):
            local.Scm(self.path).root_path

    def test_branch(self):
        with self.assertRaises(NotImplementedError):
            local.Scm(self.path).branch

    def test_remote(self):
        with self.assertRaises(NotImplementedError):
            local.Scm(self.path).remote()

    def test_dev_branches(self):
        self.assertTrue(local.Scm.DEV_BRANCHES.match('eng/1234'))
        self.assertTrue(local.Scm.DEV_BRANCHES.match('jbedard/eng/1234'))
        self.assertTrue(local.Scm.DEV_BRANCHES.match('dev/1234'))
        self.assertTrue(local.Scm.DEV_BRANCHES.match('jbedard/dev/1234'))
        self.assertTrue(local.Scm.DEV_BRANCHES.match('bug/1234'))
        self.assertTrue(local.Scm.DEV_BRANCHES.match('jbedard/bug/1234'))
        self.assertTrue(local.Scm.DEV_BRANCHES.match('integration/ci/stuff/safari-606-branch'))
        self.assertTrue(local.Scm.DEV_BRANCHES.match('clone/1234'))

        self.assertFalse(local.Scm.DEV_BRANCHES.match('main'))
        self.assertFalse(local.Scm.DEV_BRANCHES.match('random/1234'))
        self.assertFalse(local.Scm.DEV_BRANCHES.match('other/1234'))
        self.assertFalse(local.Scm.DEV_BRANCHES.match('safari-606-branch'))

    def test_prod_branches(self):
        self.assertTrue(local.Scm.PROD_BRANCHES.match('safari-610-branch'))
        self.assertTrue(local.Scm.PROD_BRANCHES.match('safari-610.1-branch'))

        self.assertFalse(local.Scm.PROD_BRANCHES.match('main'))
        self.assertFalse(local.Scm.PROD_BRANCHES.match('eng/1234'))
        self.assertFalse(local.Scm.PROD_BRANCHES.match('integration/ci/stuff/safari-606-branch'))
        self.assertFalse(local.Scm.PROD_BRANCHES.match('clone/1234'))
