# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import unittest

from datetime import datetime
from webkitscmpy import Commit, Contributor


class TestCommit(unittest.TestCase):
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

        self.assertEqual(None, Commit._parse_revision('i1234'))
        self.assertEqual(None, Commit._parse_revision('c3bd784f8b88bd03'))
        self.assertEqual(None, Commit._parse_revision('0'))
        self.assertEqual(None, Commit._parse_revision('-1'))
        self.assertEqual(None, Commit._parse_revision('3.141592'))
        self.assertEqual(None, Commit._parse_revision(3.141592))

    def test_parse_identifier(self):
        self.assertEqual((None, 1234, None), Commit._parse_identifier('i1234'))
        self.assertEqual((None, 1234, None), Commit._parse_identifier('I1234'))
        self.assertEqual((None, 1234, None), Commit._parse_identifier('1234'))
        self.assertEqual((None, 1234, None), Commit._parse_identifier(1234))

        self.assertEqual((None, 1234, 'main'), Commit._parse_identifier('i1234@main'))
        self.assertEqual((None, 1234, 'main'), Commit._parse_identifier('I1234@main'))
        self.assertEqual((None, 1234, 'main'), Commit._parse_identifier('1234@main'))

        self.assertEqual((None, 1234, 'eng/bug'), Commit._parse_identifier('i1234@eng/bug'))
        self.assertEqual((None, 1234, 'eng/bug'), Commit._parse_identifier('I1234@eng/bug'))
        self.assertEqual((None, 1234, 'eng/bug'), Commit._parse_identifier('1234@eng/bug'))

        self.assertEqual((1234, 1, 'eng/bug'), Commit._parse_identifier('i1234.1@eng/bug'))
        self.assertEqual((1234, 1, 'eng/bug'), Commit._parse_identifier('I1234.1@eng/bug'))
        self.assertEqual((1234, 1, 'eng/bug'), Commit._parse_identifier('1234.1@eng/bug'))

        self.assertEqual((None, 0, None), Commit._parse_identifier('0'))
        self.assertEqual((None, -1, None), Commit._parse_identifier('-1'))

        self.assertEqual((None, 0, 'eng/bug'), Commit._parse_identifier('i0@eng/bug'))
        self.assertEqual((None, 0, 'eng/bug'), Commit._parse_identifier('I0@eng/bug'))
        self.assertEqual((None, 0, 'eng/bug'), Commit._parse_identifier('0@eng/bug'))

        self.assertEqual((None, -1, 'eng/bug'), Commit._parse_identifier('i-1@eng/bug'))
        self.assertEqual((None, -1, 'eng/bug'), Commit._parse_identifier('I-1@eng/bug'))
        self.assertEqual((None, -1, 'eng/bug'), Commit._parse_identifier('-1@eng/bug'))

        self.assertEqual(None, Commit._parse_identifier('i1234-invalid'))
        self.assertEqual(None, Commit._parse_identifier('r266896'))
        self.assertEqual(None, Commit._parse_identifier('c3bd784f8b88bd03'))
        self.assertEqual(None, Commit._parse_identifier(3.141592))

    def test_parse(self):
        self.assertEqual(Commit.parse('i123@main'), Commit(identifier=123, branch='main'))
        self.assertEqual(Commit.parse('i123'), Commit(identifier=123))
        self.assertEqual(Commit.parse('123'), Commit(identifier=123))

        self.assertEqual(Commit.parse('r123'), Commit(revision=123))

        self.assertEqual(
            Commit.parse('c3bd784f8b88bd03f64467ddd3304ed8be28acbe'),
            Commit(hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe'),
        )

    def test_pretty_print(self):
        self.assertEqual(
            Commit(
                identifier='i123@master',
                hash='c3bd784f8b88bd03f64467ddd3304ed8be28acbe',
                author=Contributor('Jonathan Bedard', ['jbedard@apple.com']),
                message='NOT PRINTED',
            ).pretty_print(),
            '''i123@master
    git hash: c3bd784f8b88 on master
    identifier: i123 on master
    by Jonathan Bedard <jbedard@apple.com>
''',
        )

        self.assertEqual(
            Commit(
                identifier='i123@trunk',
                revision='r123',
                author=Contributor('Jonathan Bedard', ['jbedard@apple.com']),
                timestamp=1000,
                message='NOT PRINTED',
            ).pretty_print(),
            '''i123@trunk
    SVN revision: r123 on trunk
    identifier: i123 on trunk
    by Jonathan Bedard <jbedard@apple.com> @ {}
'''.format(datetime.utcfromtimestamp(1000)),
        )

        self.assertEqual(
            Commit(
                identifier='i123@trunk',
                revision='r123',
                author=Contributor('Jonathan Bedard', ['jbedard@apple.com']),
                timestamp=1000,
                message='PRINTED\n',
            ).pretty_print(message=True),
            '''i123@trunk
    SVN revision: r123 on trunk
    identifier: i123 on trunk
    by Jonathan Bedard <jbedard@apple.com> @ {}

PRINTED
'''.format(datetime.utcfromtimestamp(1000)),
        )

    def test_repr(self):
        self.assertEqual(str(Commit(identifier=123, branch='main')), 'i123@main')
        self.assertEqual(str(Commit(branch_point=1234, identifier=1, branch='eng/1234')), 'i1234.1@eng/1234')
        self.assertEqual(str(Commit(identifier=123)), 'i123')

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
        Contributor.clear()
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')

        commit = Commit(revision=1, identifier=1, author=contributor)
        self.assertEqual(commit.author, contributor)

        commit = Commit(revision=1, identifier=1, author='Jonathan Bedard')
        self.assertEqual(commit.author, contributor)

        commit = Commit(revision=1, identifier=1, author='jbedard@apple.com')
        self.assertEqual(commit.author, contributor)

    def test_invalid_contributor(self):
        Contributor.clear()
        with self.assertRaises(ValueError):
            Commit(revision=1, identifier=1, author='Jonathan Bedard')
