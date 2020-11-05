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

import unittest

from webkitcorepy import OutputCapture
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks, local


class TestCheckout(unittest.TestCase):
    path = '/mock/repository'

    def test_checkout_git(self):
        with mocks.local.Git(self.path), mocks.local.Svn(), MockTime, OutputCapture():
            self.assertEqual('bae5d1e90999d4f916a8a15810ccfa43f37a2fd6', local.Git(self.path).commit().hash)

            self.assertEqual(0, program.main(
                args=('checkout', '2@main'),
                path=self.path,
            ))

            self.assertEqual('fff83bb2d9171b4d9196e977eb0508fd57e7a08d', local.Git(self.path).commit().hash)

    def test_checkout_git_svn(self):
        with mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, OutputCapture():
            self.assertEqual('bae5d1e90999d4f916a8a15810ccfa43f37a2fd6', local.Git(self.path).commit().hash)

            self.assertEqual(0, program.main(
                args=('checkout', '2.2@branch-a'),
                path=self.path,
            ))

            self.assertEqual('621652add7fc416099bd2063366cc38ff61afe36', local.Git(self.path).commit().hash)

    def test_checkout_svn(self):
        with mocks.local.Git(), mocks.local.Svn(self.path), MockTime, OutputCapture():
            self.assertEqual(6, local.Svn(self.path).commit().revision)

            self.assertEqual(0, program.main(
                args=('checkout', '3@trunk'),
                path=self.path,
            ))

            self.assertEqual(4, local.Svn(self.path).commit().revision)
