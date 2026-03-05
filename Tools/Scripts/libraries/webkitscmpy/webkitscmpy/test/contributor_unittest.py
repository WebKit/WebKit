# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

import json
import unittest

from webkitcorepy import string_utils, StringIO, unicode
from webkitscmpy import Contributor


class TestContributor(unittest.TestCase):
    def test_git_log(self):
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_double_git_log(self):
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com, jbedard@webkit.org>')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com', 'jbedard@webkit.org'])

    def test_triple_git_log(self):
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com, jbedard@webkit.org, admin@webkit.org>')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com', 'jbedard@webkit.org', 'admin@webkit.org'])

    def test_git_svn_log(self):
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com@268f45cc-cd09-0410-ab3c-d52691b4dbfc>')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_git_username_only(self):
        contributor = Contributor.from_scm_log('Author:     Jonathan Bedard <jonathanbedard>')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jonathanbedard'])

    def test_git_no_author(self):
        contributor = Contributor.from_scm_log('Author: Automated Checkin <devnull>')
        self.assertIsNone(contributor)

    def test_git_svn_no_author(self):
        contributor = Contributor.from_scm_log('Author: (no author) <(no author)@268f45cc-cd09-0410-ab3c-d52691b4dbfc>')
        self.assertIsNone(contributor)

    def test_svn_log(self):
        contributor = Contributor.from_scm_log('r266751 | jbedard@apple.com | 2020-09-08 14:33:42 -0700 (Tue, 08 Sep 2020) | 10 lines')

        self.assertEqual(contributor.name, 'jbedard@apple.com')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_short_svn_log(self):
        contributor = Contributor.from_scm_log('r266751 | jbedard@apple.com | 2020-09-08 14:33:42 -0700 (Tue, 08 Sep 2020) | 1 line')

        self.assertEqual(contributor.name, 'jbedard@apple.com')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_svn_patch_by_log(self):
        contributor = Contributor.from_scm_log('Patch by Jonathan Bedard <jbedard@apple.com> on 2020-09-10')

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_author_mapping(self):
        contributors = Contributor.Mapping()
        Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>', contributors)
        contributor = Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@webkit.org>', contributors)

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com', 'jbedard@webkit.org'])

    def test_email_mapping(self):
        contributors = Contributor.Mapping()
        Contributor.from_scm_log('Author: Jonathan Bedard <jbedard@apple.com>', contributors)
        contributor = Contributor.from_scm_log('r266751 | jbedard@apple.com | 2020-09-08 14:33:42 -0700 (Tue, 08 Sep 2020) | 10 lines', contributors)

        self.assertEqual(contributor.name, 'Jonathan Bedard')
        self.assertEqual(contributor.emails, ['jbedard@apple.com'])

    def test_invalid_log(self):
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

        self.assertNotEqual(Contributor('Jonathan Bedard', ['jbedard@apple.com']), None)

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

    def test_unicode(self):
        self.assertEqual(
            unicode(Contributor(u'Michael Br\u00fcning', ['michael.bruning@digia.com'])),
            string_utils.encode(u'Michael Br\u00fcning <michael.bruning@digia.com>', target_type=unicode),
        )

    def test_json_encode(self):
        self.assertDictEqual(
            dict(
                name='Jonathan Bedard',
                emails=['jbedard@apple.com', 'jbedard@webkit.org'],
            ), json.loads(json.dumps(Contributor(
                name='Jonathan Bedard',
                emails=['jbedard@apple.com', 'jbedard@webkit.org'],
            ), cls=Contributor.Encoder))
        )

    def test_json_decode(self):
        contributor_a = Contributor(
            name='Jonathan Bedard',
            emails=['jbedard@apple.com', 'jbedard@webkit.org'],
        )

        dictionary = json.loads(json.dumps(contributor_a, cls=Contributor.Encoder))
        contributor_b = Contributor(**dictionary)

        self.assertEqual(contributor_a, contributor_b)

    def test_upper_case_email(self):
        mapping = Contributor.Mapping()
        mapping.create('Fujii Hironori', 'Hironori.Fujii@sony.com')
        self.assertEqual(mapping['Hironori.Fujii@sony.com'].name, 'Fujii Hironori')
        self.assertEqual(mapping['hironori.fujii@sony.com'].name, 'Fujii Hironori')

    def test_saving(self):
        mapping_a = Contributor.Mapping()
        mapping_a.create('Jonathan Bedard', 'jbedard@apple.com')
        mapping_a.create('Stephanie Lewis', 'slewis@apple.com')
        mapping_a['JonWBedard'] = mapping_a['jbedard@apple.com']

        file = StringIO()
        mapping_a.save(file)
        file.seek(0)

        mapping_b = Contributor.Mapping.load(file)
        self.assertEqual(mapping_a['jbedard@apple.com'], mapping_b['jbedard@apple.com'])
        self.assertEqual(mapping_a['jbedard@apple.com'], mapping_b['JonWBedard'])
        self.assertEqual(mapping_a['slewis@apple.com'], mapping_b['slewis@apple.com'])

    def test_iteration(self):
        mapping = Contributor.Mapping()
        mapping.create('Jonathan Bedard', 'jbedard@apple.com')
        mapping.create('Stephanie Lewis', 'slewis@apple.com')

        self.assertEqual(
            sorted(['jbedard@apple.com', 'slewis@apple.com']),
            sorted([contributor.email for contributor in mapping]),
        )

    def test_github(self):
        mapping = Contributor.Mapping()
        mapping.create(
            'Jonathan Bedard', 'jbedard@apple.com',
            github='JonWBedard',
        )
        mapping.create(
            'Jonathan Bedard', 'jbedard@apple.com',
            github='JonWBedard',
        )
        mapping.create(
            'Kocsen Chung', 'kocsen_chung@apple.com',
            github='kocsenc',
        )

        self.assertEqual(mapping['JonWBedard'], mapping['jbedard@apple.com'])
        self.assertEqual(mapping['kocsenc'], mapping['kocsen_chung@apple.com'])
        self.assertEqual(mapping['jbedard@apple.com'].github, 'JonWBedard')
        self.assertEqual(mapping['kocsen_chung@apple.com'].github, 'kocsenc')

    def test_bitbucket(self):
        mapping = Contributor.Mapping()
        mapping.create(
            'Jonathan Bedard', 'jbedard@apple.com',
            bitbucket='jonathan_bedard',
        )
        mapping.create(
            'Kocsen Chung', 'kocsen_chung@apple.com',
            bitbucket='kocsen_chung',
        )

        self.assertEqual(mapping['jonathan_bedard'], mapping['jbedard@apple.com'])
        self.assertEqual(mapping['kocsen_chung'], mapping['kocsen_chung@apple.com'])
        self.assertEqual(mapping['jbedard@apple.com'].bitbucket, 'jonathan_bedard')
        self.assertEqual(mapping['kocsen_chung@apple.com'].bitbucket, 'kocsen_chung')
