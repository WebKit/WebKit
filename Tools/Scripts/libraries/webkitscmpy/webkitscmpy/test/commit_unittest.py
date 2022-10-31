# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import logging
import os
import unittest

from mock import patch
from datetime import datetime
from webkitbugspy import bugzilla, mocks as bmocks, Tracker, radar
from webkitcorepy import OutputCapture, testing
from webkitcorepy.mocks import Environment
from webkitscmpy import Contributor, Commit, program, mocks


class TestCommit(unittest.TestCase):
    BUGZILLA = 'https://bugs.example.com'

    def test_parse_hash(self):
        self.assertEqual(
            '1a2e41e3f7cdf51b1e1d02880cfb65eab9327ef2',
            Commit._parse_hash('1a2e41e3f7cdf51b1e1d02880cfb65eab9327ef2')
        )
        self.assertEqual(
            'c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
            Commit._parse_hash('C3BD784F8B88BD03F64467DDD3304ED8BE28ACBE')
        )

        self.assertEqual(None, Commit._parse_hash('invalid hash'))
        self.assertEqual(None, Commit._parse_hash('c3bd784f8b88bd03f64467ddd3304ed8be28acbe1'))

    def test_parse_revision(self):
        self.assertEqual(266896, Commit._parse_revision('r266896'))
        self.assertEqual(266896, Commit._parse_revision('R266896'))
        self.assertEqual(266896, Commit._parse_revision('266896'))
        self.assertEqual(266896, Commit._parse_revision(266896))

        self.assertEqual(None, Commit._parse_revision('c3bd784f8b88bd03'))
        self.assertEqual(None, Commit._parse_revision('0'))
        self.assertEqual(None, Commit._parse_revision('-1'))
        self.assertEqual(None, Commit._parse_revision('3.141592'))
        self.assertEqual(None, Commit._parse_revision(3.141592))

    def test_parse_identifier(self):
        self.assertEqual((None, 1234, None), Commit._parse_identifier('1234'))
        self.assertEqual((None, 1234, None), Commit._parse_identifier(1234))
        self.assertEqual((None, 1234, None), Commit._parse_identifier('1234@'))

        self.assertEqual((None, 1234, 'main'), Commit._parse_identifier('1234@main'))
        self.assertEqual((None, 1234, 'eng/bug'), Commit._parse_identifier('1234@eng/bug'))
        self.assertEqual((1234, 1, 'eng/bug'), Commit._parse_identifier('1234.1@eng/bug'))

        self.assertEqual((None, 0, None), Commit._parse_identifier('0'))
        self.assertEqual((None, -1, None), Commit._parse_identifier('-1'))

        self.assertEqual((None, 0, 'eng/bug'), Commit._parse_identifier('0@eng/bug'))
        self.assertEqual((None, -1, 'eng/bug'), Commit._parse_identifier('-1@eng/bug'))

        self.assertEqual(None, Commit._parse_identifier('1234-invalid'))
        self.assertEqual(None, Commit._parse_identifier('r266896'))
        self.assertEqual(None, Commit._parse_identifier('c3bd784f8b88bd03'))
        self.assertEqual(None, Commit._parse_identifier(3.141592))

    def test_parse(self):
        self.assertEqual(Commit.parse('123@main'), Commit(identifier=123, branch='main'))
        self.assertEqual(Commit.parse('123'), Commit(identifier=123))

        self.assertEqual(Commit.parse('r123'), Commit(revision=123))

        self.assertEqual(
            Commit.parse('c3bd784f8b88bd03f64467ddd3304ed8be28acbe'),
            Commit(hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe'),
        )

    def test_pretty_print(self):
        self.assertEqual(
            Commit(
                identifier='123@master',
                hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
                author=Contributor('Jonathan Bedard', ['jbedard@apple.com']),
                message='NOT PRINTED',
            ).pretty_print(),
            '''123@master
    git hash: c3bd784f8b88 on master
    identifier: 123 on master
    by Jonathan Bedard <jbedard@apple.com>
''',
        )

        self.assertEqual(
            Commit(
                identifier='123@trunk',
                revision='r123',
                author=Contributor('Jonathan Bedard', ['jbedard@apple.com']),
                timestamp=1000,
                message='NOT PRINTED',
            ).pretty_print(),
            '''123@trunk
    SVN revision: r123 on trunk
    identifier: 123 on trunk
    by Jonathan Bedard <jbedard@apple.com> @ {}
'''.format(datetime.utcfromtimestamp(1000)),
        )

        self.assertEqual(
            Commit(
                identifier='123.1@branch-a',
                revision='r124',
                author=Contributor('Jonathan Bedard', ['jbedard@apple.com']),
                timestamp=1000,
                message='NOT PRINTED',
            ).pretty_print(),
            '''123.1@branch-a
    SVN revision: r124 on branch-a
    identifier: 1 on branch-a branched from 123
    by Jonathan Bedard <jbedard@apple.com> @ {}
'''.format(datetime.utcfromtimestamp(1000)),
        )

        self.assertEqual(
            Commit(
                identifier='123@trunk',
                revision='r123',
                author=Contributor('Jonathan Bedard', ['jbedard@apple.com']),
                timestamp=1000,
                message='PRINTED\n',
            ).pretty_print(message=True),
            '''123@trunk
    SVN revision: r123 on trunk
    identifier: 123 on trunk
    by Jonathan Bedard <jbedard@apple.com> @ {}

PRINTED
'''.format(datetime.utcfromtimestamp(1000)),
        )

    def test_repr(self):
        self.assertEqual(str(Commit(identifier=123, branch='main')), '123@main')
        self.assertEqual(str(Commit(branch_point=1234, identifier=1, branch='eng/1234')), '1234.1@eng/1234')
        self.assertEqual(str(Commit(identifier=123)), '123')

        self.assertEqual(str(Commit(revision=123)), 'r123')

        self.assertEqual(str(Commit(hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe')), 'c3bd784f8b88')

    def test_timestamp_compare(self):
        self.assertLess(
            Commit(revision=1, identifier=1, timestamp=100),
            Commit(revision=2, identifier=2, timestamp=200),
        )
        self.assertGreater(
            Commit(revision=2, identifier=2, timestamp=200),
            Commit(revision=1, identifier=1, timestamp=100),
        )
        self.assertLess(
            Commit(revision=1, identifier=1, timestamp=100),
            Commit(revision=2, identifier=2, timestamp=100),
        )
        self.assertGreater(
            Commit(revision=2, identifier=2, timestamp=100),
            Commit(revision=1, identifier=1, timestamp=100),
        )
        self.assertEqual(
            Commit(revision=1, identifier=1, timestamp=100),
            Commit(revision=1, identifier=1, timestamp=100),
        )

    def test_identifier_compare(self):
        self.assertLess(Commit(identifier=1), Commit(identifier=2))
        self.assertGreater(Commit(identifier=2), Commit(identifier=1))
        self.assertEqual(Commit(identifier=1), Commit(identifier=1))

    def test_revision_compare(self):
        self.assertLess(Commit(revision=1), Commit(revision=2))
        self.assertGreater(Commit(revision=2), Commit(revision=1))
        self.assertEqual(Commit(revision=1), Commit(revision=1))

    def test_invalid_compare(self):
        with self.assertRaises(ValueError):
            Commit(revision=1).__cmp__(Commit(identifier=2))

        with self.assertRaises(ValueError):
            Commit(revision=1, timestamp=100).__cmp__(Commit(identifier=2, timestamp=100))

    def test_contributor(self):
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')

        commit = Commit(revision=1, identifier=1, author=contributor)
        self.assertEqual(commit.author, contributor)

        commit = Commit(revision=1, identifier=1, author=Contributor.Encoder().default(contributor))
        self.assertEqual(commit.author, contributor)

    def test_invalid_contributor(self):
        with self.assertRaises(TypeError):
            Commit(revision=1, identifier=1, author='Jonathan Bedard')

    def test_branch_point(self):
        self.assertEqual('0', str(Commit(identifier=0)))
        self.assertEqual('0@branch-a', str(Commit(identifier=0, branch='branch-a')))
        self.assertEqual('1234.0@branch-a', str(Commit(branch_point=1234, identifier=0, branch='branch-a')))

    def test_json_encode(self):
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')

        self.assertDictEqual(
            dict(
                revision=1,
                hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
                branch='main',
                identifier='1@main',
                timestamp=1000,
                order=0,
                author=dict(
                    name='Jonathan Bedard',
                    emails=['jbedard@apple.com'],
                ), message='Message',
            ), json.loads(json.dumps(Commit(
                revision=1,
                hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
                identifier='1@main',
                timestamp=1000,
                author=contributor,
                message='Message',
            ), cls=Commit.Encoder))
        )

    def test_json_decode(self):
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')

        commit_a = Commit(
            revision=1,
            hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
            identifier='1@main',
            timestamp=1000,
            author=Contributor.Encoder().default(contributor),
            message='Message'
        )

        dictionary = json.loads(json.dumps(commit_a, cls=Commit.Encoder))
        commit_b = Commit(**dictionary)

        self.assertEqual(commit_a, commit_b)

    def test_from_json(self):
        self.assertEqual(
            Commit.from_json(dict(
                repository_id='webkit',
                id=1234,
                timestamp=1000,
            )), Commit(
                repository_id='webkit',
                revision=1234,
                timestamp=1000,
            ),
        )

        self.assertEqual(
            Commit.from_json(dict(
                repository_id='webkit',
                id='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
                timestamp=2000,
            )), Commit(
                repository_id='webkit',
                hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
                timestamp=2000,
            ),
        )

    def test_from_json_str(self):
        contributor = Contributor.from_scm_log('Author: jbedard@apple.com <jbedard@apple.com>')
        self.assertEqual(
            Commit.from_json('''{
    "revision": 1,
    "hash": "c3bd784f8b88bd03f64467ddd3304ed8be28acbe",
    "identifier": "1@main",
    "timestamp": 1000,
    "author": "jbedard@apple.com",
    "message": "Message"
}'''), Commit(
                revision=1,
                hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
                identifier='1@main',
                timestamp=1000,
                author=Contributor.Encoder().default(contributor),
                message='Message'
            ),
        )

    def test_parse_issues(self):
        contributor = Contributor.from_scm_log('Author: jbedard@apple.com <jbedard@apple.com>')
        commit = Commit(
            revision=1,
            hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
            identifier='1@main',
            timestamp=1000,
            author=Contributor.Encoder().default(contributor),
            message='Commit title\n'
                    'https://bugs.example.com/show_bug.cgi?id=1\n'
                    '<rdar://problem/2>\n\n'
                    'Reviewed by NOBODY (OOPS!)\n\n'
                    'Will fix this in https://bugs.example.com/show_bug.cgi?id=3 and <rdar://problem/4>\n',
        )

        with patch('webkitbugspy.Tracker._trackers', []):
            self.assertEqual([], commit.issues)

        with bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)]):
            self.assertEqual([
                Tracker.from_string('https://bugs.example.com/show_bug.cgi?id=1'),
            ], commit.issues)

        with bmocks.Radar(), patch('webkitbugspy.Tracker._trackers', [radar.Tracker()]):
            self.assertEqual([
                Tracker.from_string('<rdar://problem/2>'),
            ], commit.issues)

        with bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
        ), bmocks.Radar(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA), radar.Tracker()]):
            self.assertEqual([
                Tracker.from_string('https://bugs.example.com/show_bug.cgi?id=1'),
                Tracker.from_string('<rdar://problem/2>'),
            ], commit.issues)

    def test_parse_issue_ignore_reference(self):
        contributor = Contributor.from_scm_log('Author: jbedard@apple.com <jbedard@apple.com>')
        commit = Commit(
            revision=1,
            hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
            identifier='1@main',
            timestamp=1000,
            author=Contributor.Encoder().default(contributor),
            message='Remove something added in rdar://1\n'
                    'https://bugs.example.com/show_bug.cgi?id=1\n'
                    '<rdar://problem/2>\n\n'
                    'Reviewed by NOBODY (OOPS!)\n\n'
                    'Will fix this in https://bugs.example.com/show_bug.cgi?id=3 and <rdar://problem/4>\n',
        )

        with bmocks.Bugzilla(
            self.BUGZILLA.split('://')[-1],
            projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
        ), bmocks.Radar(), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA), radar.Tracker()]):
            self.assertEqual([
                Tracker.from_string('https://bugs.example.com/show_bug.cgi?id=1'),
                Tracker.from_string('<rdar://problem/2>'),
                Tracker.from_string('<rdar://problem/1>'),
            ], commit.issues)


class TestDoCommit(testing.PathTestCase):
    basepath = 'mock/repository'
    BUGZILLA = 'https://bugs.example.com'

    def setUp(self):
        super(TestDoCommit, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_svn(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn(self.path), patch(
                'webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('commit',),
                path=self.path,
            ))
        self.assertEqual(captured.root.log.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "Can only 'commit' on a native Git repository\n")

    def test_none(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn(), patch(
                'webkitbugspy.Tracker._trackers', []):
            self.assertEqual(1, program.main(
                args=('commit',),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), "Can only 'commit' on a native Git repository\n")

    def test_commit(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path) as repo, mocks.local.Svn(), patch('webkitbugspy.Tracker._trackers', []):
            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('commit',),
                path=self.path,
            ))
            self.assertDictEqual(repo.staged, {})
            self.assertEqual(repo.head.hash, 'c28f53f7fabd7bd9535af890cb7dc473cb342999')
            self.assertEqual(
                '[Testing] Creating commits\n'
                'Reviewed by Jonathan Bedard\n\n'
                ' * added.txt\n',
                repo.head.message,
            )

        self.assertEqual(
            '\n'.join([line for line in captured.root.log.getvalue().splitlines() if 'Mock process' not in line]),
            '')
        self.assertEqual(captured.stdout.getvalue(), "")
        self.assertEqual(captured.stderr.getvalue(), "")

    def test_commit_with_bug(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(
                projects=bmocks.PROJECTS) as remote, bmocks.Bugzilla(
                self.BUGZILLA.split('://')[-1],
                projects=bmocks.PROJECTS, issues=bmocks.ISSUES,
                environment=Environment(
                    BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                    BUGS_EXAMPLE_COM_PASSWORD='password',
                )), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA)],
        ), mocks.local.Git(
            self.path, remote='https://{}'.format(remote.remote),
            remotes=dict(fork='https://{}/Contributor/WebKit'.format(remote.hosts[0])),
        ) as repo, mocks.local.Svn():
            repo.staged['added.txt'] = 'added'
            self.assertEqual(0, program.main(
                args=('commit', '-i', '3', ),
                path=self.path,
            ))
            self.assertDictEqual(repo.staged, {})
            self.assertEqual(repo.head.hash, '0cc822a8ca16698e13363f917e3d9dad387141a4')
            self.assertEqual(
                'Example issue 2\n'
                'https://bugs.example.com/show_bug.cgi?id=3\n'
                'Reviewed by Jonathan Bedard\n\n'
                ' * added.txt\n',
                repo.head.message,
            )

        self.assertEqual(
            '\n'.join([line for line in captured.root.log.getvalue().splitlines() if 'Mock process' not in line]),
            '')
        self.assertEqual(captured.stdout.getvalue(), "")
        self.assertEqual(captured.stderr.getvalue(), "")
