# Copyright (C) 2022 Apple Inc. All rights reserved.
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

import logging
import os
import sys
import time

from mock import patch
from unittest import TestCase
from webkitbugspy import radar, mocks as bmocks
from webkitcorepy import OutputCapture, Terminal, testing
from webkitscmpy import local, mocks, Commit, program
from webkitscmpy.program.trace import Relationship, CommitsStory


class TestRelationship(TestCase):
    def test_cherry_pick(self):
        self.assertEqual(
            ('original', ['0123456789ab']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Cherry-pick 0123456789ab <rdar://54321>',
            ))
        )
        self.assertEqual(
            ('original', ['123@main', '0123456789ab']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Cherry-pick 123@main (0123456789ab). <rdar://54321>',
            ))
        )
        self.assertEqual(
            ('original', ['123@main', 'r120']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Cherry-pick 123@main (r120). <rdar://54321>',
            ))
        )
        self.assertEqual(
            ('original', ['123@main']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='(partial) Cherry-pick of 123@main. <rdar://54321>',
            ))
        )
        self.assertEqual(
            ('original', ['123@main']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Cherry-picked 123@main. <rdar://54321>',
            ))
        )

    def test_revert(self):
        self.assertEqual(
            ('reverts', ['1230@main', '0123456789ab']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Reverts 1230@main (0123456789ab)',
            ))
        )
        self.assertEqual(
            ('reverts', ['1230@main']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Revert 1230@main, it broke the build',
            ))
        )

    def test_follow_up(self):
        self.assertEqual(
            ('follow-up to', ['1230@main', '0123456789ab']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Fix following 1230@main (0123456789ab)',
            ))
        )
        self.assertEqual(
            ('follow-up to', ['1230@main']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Follow-up 1230@main, it broke the build',
            ))
        )
        self.assertEqual(
            ('follow-up to', ['1230@main']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='REGRESSION 1230@main',
            ))
        )
        self.assertEqual(
            ('follow-up to', ['1230@main']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Test-addition (1230@main)',
            ))
        )

    def test_double_revert(self):
        self.assertEqual(
            ('original', ['1230@main', '0123456789ab']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Reverts "Revert 1230@main (0123456789ab)"',
            ))
        )
        self.assertEqual(
            ('original', ['1230@main']), Relationship.parse(Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Revert "Revert 1230@main, it broke the build"',
            ))
        )


class TestCommitsStory(TestCase):
    def test_cherry_pick(self):
        with bmocks.Radar(issues=bmocks.ISSUES), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            commit = Commit(
                hash='deadbeef1234', revision=1234, identifier='1234@main',
                message='Cherry-pick 123@main (0123456789ab). <rdar://54321>',
            )
            story = CommitsStory([commit])
            self.assertEqual(story.by_issue.get('rdar://54321', []), [commit])
            self.assertEqual(
                [str(rel) for rel in story.relations.get('123@main', [])],
                ['1234@main cherry-picked'],
            )
            self.assertEqual(
                [str(rel) for rel in story.relations.get('0123456789ab', [])],
                ['1234@main cherry-picked'],
            )

    def test_revert(self):
        story = CommitsStory([Commit(
            hash='deadbeef1234', revision=1234, identifier='1234@main',
            message='Reverts 1230@main (0123456789ab)',
        )])
        self.assertEqual(story.by_issue, {})
        self.assertEqual(
            [str(rel) for rel in story.relations.get('1230@main', [])],
            ['1234@main reverted by'],
        )
        self.assertEqual(
            [str(rel) for rel in story.relations.get('0123456789ab', [])],
            ['1234@main reverted by'],
        )

    def test_follow_up(self):
        story = CommitsStory([Commit(
            hash='deadbeef1234', revision=1234, identifier='1234@main',
            message='Fix following 1230@main (0123456789ab)',
        )])
        self.assertEqual(story.by_issue, {})
        self.assertEqual(
            [str(rel) for rel in story.relations.get('1230@main', [])],
            ['1234@main followed-up by'],
        )
        self.assertEqual(
            [str(rel) for rel in story.relations.get('0123456789ab', [])],
            ['1234@main followed-up by'],
        )


class TestTrace(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(TestTrace, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_none(self):
        with OutputCapture() as captured, mocks.local.Git(), mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            self.assertEqual(1, program.main(
                args=('trace', '6@main'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'No repository provided\n')

    def test_revert_a(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Reverts 5@main', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | Reverts 5@main\n    reverts 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_revert_b(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Revert [5@main]', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | Revert [5@main]\n    reverts 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_revert_c(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Revert "5@main"', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | Revert "5@main"\n    reverts 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_revert_cherry_pick(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.commits['main'].append(Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Cherry-pick a30ce8494bf1. rdar://problem/1234', timestamp=int(time.time()),
                author=repo.head.author,
            ))
            repo.commits['main'].append(Commit(
                hash='feebdaed4321', revision=11, identifier='7@main',
                message='Revert "Cherry-pick a30ce8494bf1. rdar://problem/1234"', timestamp=int(time.time()),
                author=repo.head.author,
            ))

            repo.head = repo.commits['main'][-1]

            self.assertEqual(0, program.main(
                args=('trace', '7@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '7@main | feebdaed4321 | Revert "Cherry-pick a30ce8494bf1. rdar://problem/1234"\n    reverts 2.1@branch-a | a30ce8494bf1 | 3rd commit\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_follow_up_a(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Follow-up 5@main', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | Follow-up 5@main\n    follow-up to 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_follow_up_b(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Follow up to 5@main', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | Follow up to 5@main\n    follow-up to 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_follow_up_c(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Followup fix to 5@main', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | Followup fix to 5@main\n    follow-up to 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_regression(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='REGRESSION (5@main) Fix the build', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | REGRESSION (5@main) Fix the build\n    follow-up to 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_gardening(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='[Gardening]: REGRESSION 5@main', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | [Gardening]: REGRESSION 5@main\n    follow-up to 5@main | d8bce26fa65c | Patch Series\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_invalid_revert(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Reverts <rdar://1234>', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(1, program.main(
                args=('trace', '6@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '6@main | deadbeef1234 | Reverts <rdar://1234>\n',
        )
        self.assertEqual(captured.stderr.getvalue(), 'No relationships found\n')

    def test_reverted_by(self):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), Terminal.override_atty(
                sys.stdin, isatty=False):
            repo.head = Commit(
                hash='deadbeef1234', revision=10, identifier='6@main',
                message='Reverts 5@main', timestamp=int(time.time()),
                author=repo.head.author,
            )
            repo.commits['main'].append(repo.head)

            self.assertEqual(0, program.main(
                args=('trace', '5@main', '--limit', '1'),
                path=self.path,
            ))
        self.assertEqual(
            captured.stdout.getvalue(),
            '5@main | d8bce26fa65c | Patch Series\n    reverted by 6@main | deadbeef1234 | Reverts 5@main\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
