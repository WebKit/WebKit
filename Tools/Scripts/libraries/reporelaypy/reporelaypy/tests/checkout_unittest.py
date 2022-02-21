# Copyright (C) 2021 Apple Inc. All rights reserved.
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

from reporelaypy import Checkout
from webkitcorepy import testing, OutputCapture
from webkitscmpy import mocks


class CheckoutUnittest(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(CheckoutUnittest, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    def test_constructor_sentinal(self):
        with mocks.local.Git(self.path) as repo, OutputCapture():
            with open(os.path.join(os.path.dirname(self.path), 'cloned'), 'w') as sentinal:
                sentinal.write('yes\n')
            Checkout(path=self.path, url=repo.remote, sentinal=True)

    def test_constructor_no_sentinal(self):
        with mocks.local.Git(self.path) as repo, OutputCapture():
            Checkout(path=self.path, url=repo.remote, sentinal=False)

            with self.assertRaises(Checkout.Exception):
                Checkout(path=self.path, url=None, sentinal=True)
