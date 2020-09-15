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

from webkitscmpy import Contributor


class TestContributor(unittest.TestCase):
    def test_git_log(self):
        Contributor.clear()
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_svn_log(self):
        Contributor.clear()
        contributor = Contributor.from_scm_log('r266751 | jbedard@apple.com | 2020-09-08 14:33:42 -0700 (Tue, 08 Sep 2020) | 10 lines')

        self.assertEqual(contributor.name, 'jbedard@apple.com')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_svn_patch_by_log(self):
        Contributor.clear()
        contributor = Contributor.from_scm_log('Patch by Jonathan Bedard <jbedard@apple.com> on 2020-09-10')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_author_mapping(self):
        Contributor.clear()
        Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@webkit.org>')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com', 'jbedard@webkit.org'])

    def test_email_mapping(self):
        Contributor.clear()
        Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')
        contributor = Contributor.from_scm_log('r266751 | jbedard@apple.com | 2020-09-08 14:33:42 -0700 (Tue, 08 Sep 2020) | 10 lines')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_invalid_log(self):
        Contributor.clear()
        with self.assertRaises(ValueError):
            Contributor.from_scm_log('Jonathan Bedard <jbedard@apple.com>')

    def test_comparison(self):
        self.assertEqual(
            Contributor('Jonathan Bedard', ['jbedard@apple.com']),
            Contributor('Jonathan Bedard', ['jbedard@webkit.org']),
        )

        self.assertNotEqual(
            Contributor('Jonathan Bedard', ['jbedard@apple.com']),
            Contributor('Aakash Jain', ['aakashjain@apple.com']),
        )

    def test_string_comparison(self):
        self.assertEqual(
            Contributor('Jonathan Bedard', ['jbedard@apple.com']),
            'Jonathan Bedard',
        )

        self.assertNotEqual(
            Contributor('Jonathan Bedard', ['jbedard@apple.com']),
            'Aakash Jain',
        )

    def test_hash(self):
        self.assertEqual(
            hash(Contributor('Jonathan Bedard', ['jbedard@apple.com'])),
            hash(Contributor('Jonathan Bedard', ['jbedard@webkit.org'])),
        )

        self.assertNotEqual(
            hash(Contributor('Jonathan Bedard', ['jbedard@apple.com'])),
            hash(Contributor('Aakash Jain', ['aakashjain@apple.com'])),
        )

    def test_sorting(self):
        self.assertEqual(
            sorted([Contributor('Jonathan Bedard'), Contributor('Aakash Jain')]),
            [Contributor('Aakash Jain'), Contributor('Jonathan Bedard')],
        )
