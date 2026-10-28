# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import datetime
import json
import os
import re

from unittest.mock import patch

from webkitcorepy import OutputCapture, Terminal, testing
from webkitscmpy import mocks
from webkitscmpy.commit_graph import CommitGraph


class TestCommitGraph(testing.PathTestCase):
    basepath = 'mock/repository'

    # Every mock commit lands during the week of Monday, September 28th, 2020
    WINDOW = ('--since', '2020-09-21', '--until', '2020-10-12')

    def setUp(self):
        super(TestCommitGraph, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    def test_weeks(self):
        self.assertEqual(
            CommitGraph.weeks(datetime.date(2020, 9, 23), datetime.date(2020, 10, 12)),
            [datetime.date(2020, 9, 28), datetime.date(2020, 10, 5)],
        )
        self.assertEqual(
            CommitGraph.weeks(datetime.date(2020, 9, 21), datetime.date(2020, 10, 11)),
            [datetime.date(2020, 9, 21), datetime.date(2020, 9, 28)],
        )

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn():
            self.assertEqual(1, CommitGraph.main(['--csv', os.path.dirname(self.path)] + list(self.WINDOW)))
        self.assertEqual(captured.stderr.getvalue(), "No repository found at '{}'\n".format(os.path.dirname(self.path)))

    def test_svn(self):
        os.mkdir(os.path.join(self.path, '.svn'))
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(self.path):
            self.assertEqual(1, CommitGraph.main([self.path, '--csv'] + list(self.WINDOW)))
        self.assertEqual(captured.stderr.getvalue(), "Can only chart git repositories, '{}' is not one\n".format(self.path))

    def test_csv(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(0, CommitGraph.main([self.path, '--csv'] + list(self.WINDOW)))
        self.assertEqual(
            captured.stdout.getvalue(),
            'week,repository\n'
            '2020-09-21,0\n'
            '2020-09-28,5\n'
            '2020-10-05,0\n',
        )

    def test_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(0, CommitGraph.main([self.path, '--csv', '--branch', 'branch-a'] + list(self.WINDOW)))
        self.assertEqual(
            captured.stdout.getvalue(),
            'week,repository\n'
            '2020-09-21,0\n'
            '2020-09-28,4\n'
            '2020-10-05,0\n',
        )

    def test_invalid_branch(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(1, CommitGraph.main([self.path, '--csv', '--branch', 'does-not-exist'] + list(self.WINDOW)))
        self.assertEqual(captured.stderr.getvalue(), "Failed to retrieve commits on 'does-not-exist' in 'repository'\n")

    def test_no_weeks(self):
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(1, CommitGraph.main([self.path, '--csv', '--since', '2020-09-29', '--until', '2020-10-04']))
        self.assertEqual(captured.stderr.getvalue(), 'No full weeks between 2020-09-29 and 2020-10-04\n')

    def test_multiple_repositories(self):
        # Our git mock can only stand in for one repository at a time
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn():
            self.assertEqual(0, CommitGraph.main(['--csv', '--branch', 'branch-a', self.path, self.path] + list(self.WINDOW)))
        self.assertEqual(
            captured.stdout.getvalue(),
            'week,repository,repository\n'
            '2020-09-21,0,0\n'
            '2020-09-28,4,4\n'
            '2020-10-05,0,0\n',
        )

    def test_html(self):
        output = os.path.join(self.path, 'chart.html')
        with OutputCapture() as captured, mocks.local.Git(self.path), mocks.local.Svn(), patch.object(Terminal, 'open_url') as open_url:
            self.assertEqual(0, CommitGraph.main([self.path, '--output', output, '--average', '2'] + list(self.WINDOW)))
        open_url.assert_called_once_with('file://{}'.format(output))
        self.assertEqual(
            captured.stdout.getvalue(),
            'repository: 5 commits over 3 weeks\n'
            'Wrote chart to {}\n'.format(output),
        )

        with open(output) as file:
            contents = file.read()
        self.assertIn('<title>Weekly commit volume: repository</title>', contents)
        self.assertIn('Commits landed on main (first-parent) per week, Sep 21, 2020 to Oct 11, 2020.', contents)
        self.assertIn('2-week rolling average', contents)
        self.assertNotIn('__', re.sub(r'<script>.*</script>', '', contents, flags=re.DOTALL))

        data = json.loads(re.search(r'const DATA = (.+);\n', contents).group(1))
        self.assertEqual(data, dict(
            weeks=['2020-09-21', '2020-09-28', '2020-10-05'],
            series=[dict(name='repository', counts=[0, 5, 0])],
            average=2,
        ))

    def test_no_open(self):
        output = os.path.join(self.path, 'chart.html')
        with OutputCapture(), mocks.local.Git(self.path), mocks.local.Svn(), patch.object(Terminal, 'open_url') as open_url:
            self.assertEqual(0, CommitGraph.main([self.path, '--output', output, '--no-open'] + list(self.WINDOW)))
        open_url.assert_not_called()
        self.assertTrue(os.path.isfile(output))

    def test_csv_does_not_open(self):
        with OutputCapture(), mocks.local.Git(self.path), mocks.local.Svn(), patch.object(Terminal, 'open_url') as open_url:
            self.assertEqual(0, CommitGraph.main([self.path, '--csv'] + list(self.WINDOW)))
        open_url.assert_not_called()
