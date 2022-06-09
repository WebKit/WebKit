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
import time
import sys

from datetime import datetime

from webkitcorepy import OutputCapture, Terminal, testing
from webkitcorepy.mocks import Time as MockTime
from webkitscmpy import program, mocks


class TestLog(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestLog, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_git(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(-1, program.main(
                args=('log', 'main'),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '''commit 5@main (d8bce26fa65c6fc8f39c17927abb77f69fab82fc)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    Patch Series

commit 4@main (bae5d1e90999d4f916a8a15810ccfa43f37a2fd6)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    8th commit

commit 3@main (1abe25b443e985f93b90d830e4a7e3731336af4d)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    4th commit

commit 2@main (fff83bb2d9171b4d9196e977eb0508fd57e7a08d)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    2nd commit

commit 1@main (9b8311f25a77ba14923d9d5a6532103f54abefcb)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    1st commit
'''.format(*reversed([
                datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000')
                for commit in mocks.local.Git(self.path).commits['main']
            ])),
        )

    def test_git_svn(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(-1, program.main(
                args=('log', 'main'),
                path=self.path,
            ))

            self.assertEqual(
                captured.stdout.getvalue(),
                '''commit 5@main (d8bce26fa65c6fc8f39c17927abb77f69fab82fc, r9)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    Patch Series
    git-svn-id: https://svn.example.org/repository/repository/trunk@9 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit 4@main (bae5d1e90999d4f916a8a15810ccfa43f37a2fd6, r8)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    8th commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@8 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit 3@main (1abe25b443e985f93b90d830e4a7e3731336af4d, r4)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    4th commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@4 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit 2@main (fff83bb2d9171b4d9196e977eb0508fd57e7a08d, r2)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    2nd commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@2 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit 1@main (9b8311f25a77ba14923d9d5a6532103f54abefcb, r1)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    1st commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc
'''.format(*reversed([
                    datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000')
                    for commit in mocks.local.Git(self.path).commits['main']
                ])),
            )

    def test_git_svn_revision(self):
        with OutputCapture() as captured, mocks.local.Git(self.path, git_svn=True), mocks.local.Svn(), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(-1, program.main(
                args=('log', 'main', '--revision'),
                path=self.path,
            ))

            self.assertEqual(
                captured.stdout.getvalue(),
                '''commit r9 (d8bce26fa65c6fc8f39c17927abb77f69fab82fc, 5@main)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    Patch Series
    git-svn-id: https://svn.example.org/repository/repository/trunk@9 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit r8 (bae5d1e90999d4f916a8a15810ccfa43f37a2fd6, 4@main)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    8th commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@8 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit r4 (1abe25b443e985f93b90d830e4a7e3731336af4d, 3@main)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    4th commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@4 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit r2 (fff83bb2d9171b4d9196e977eb0508fd57e7a08d, 2@main)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    2nd commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@2 268f45cc-cd09-0410-ab3c-d52691b4dbfc

commit r1 (9b8311f25a77ba14923d9d5a6532103f54abefcb, 1@main)
Author: Jonathan Bedard <jbedard@apple.com>
Date:   {}

    1st commit
    git-svn-id: https://svn.example.org/repository/repository/trunk@1 268f45cc-cd09-0410-ab3c-d52691b4dbfc
'''.format(*reversed([
                    datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000')
                    for commit in mocks.local.Git(self.path).commits['main']
                ])),
            )

    def test_svn(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path), MockTime, Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(-1, program.main(
                args=('log',),
                path=self.path,
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            '''------------------------------------------------------------------------
r6 (4@trunk) | jbedard@apple.com | 2020-10-02 18:58:20 0000 (Fri, 02 Oct 2020) | 1 lines

6th commit

------------------------------------------------------------------------
r4 (3@trunk) | jbedard@apple.com | 2020-10-02 18:25:00 0000 (Fri, 02 Oct 2020) | 1 lines

4th commit

------------------------------------------------------------------------
r2 (2@trunk) | jbedard@apple.com | 2020-10-02 17:51:40 0000 (Fri, 02 Oct 2020) | 1 lines

2nd commit

------------------------------------------------------------------------
r1 (1@trunk) | jbedard@apple.com | 2020-10-02 17:35:00 0000 (Fri, 02 Oct 2020) | 1 lines

1st commit

''',
        )
