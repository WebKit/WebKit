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
import json
import unittest

from reporelaypy import Checkout, CheckoutRoute, Redirector
from webkitcorepy import testing, OutputCapture
from webkitflaskpy import mock_app
from webkitscmpy import mocks, Commit


class RedirectorUnittest(unittest.TestCase):
    def test_empty(self):
        self.assertEqual(
            [],
            Redirector.from_json(json.dumps([], cls=Redirector.Encoder)),
        )

    def test_bitbucket(self):
        redir = Redirector('https://bitbucket.webkit.org/projects/WEBKIT/repos/webkit')
        self.assertEqual(redir.type, 'bitbucket')
        self.assertEqual(
            redir(None).headers.get('location'),
            'https://bitbucket.webkit.org/projects/WEBKIT/repos/webkit/commits',
        )
        self.assertEqual(
            redir(Commit(hash='deadbeef')).headers.get('location'),
            'https://bitbucket.webkit.org/projects/WEBKIT/repos/webkit/commits/deadbeef',
        )

        transfered = Redirector.from_json(json.dumps([redir], cls=Redirector.Encoder))[0]
        self.assertEqual(redir.type, transfered.type)
        self.assertEqual(redir(None).headers.get('location'), transfered(None).headers.get('location'))
        self.assertEqual(
            redir(Commit(hash='deadbeef')).headers.get('location'),
            transfered(Commit(hash='deadbeef')).headers.get('location'),
        )

    def test_trac(self):
        redir = Redirector('https://trac.webkit.org')
        self.assertEqual(redir.type, 'trac')
        self.assertEqual(
            redir(None).headers.get('location'),
            'https://trac.webkit.org',
        )
        self.assertEqual(
            redir(Commit(revision=12345)).headers.get('location'),
            'https://trac.webkit.org/changeset/12345/webkit',
        )

        transfered = Redirector.from_json(json.dumps([redir], cls=Redirector.Encoder))[0]
        self.assertEqual(redir.type, transfered.type)
        self.assertEqual(redir(None).headers.get('location'), transfered(None).headers.get('location'))
        self.assertEqual(
            redir(Commit(revision=12345)).headers.get('location'),
            transfered(Commit(revision=12345)).headers.get('location'),
        )

    def test_github(self):
        redir = Redirector('https://github.com/WebKit/WebKit')
        self.assertEqual(redir.type, 'github')
        self.assertEqual(
            redir(None).headers.get('location'),
            'https://github.com/WebKit/WebKit/commits',
        )
        self.assertEqual(
            redir(Commit(hash='deadbeef')).headers.get('location'),
            'https://github.com/WebKit/WebKit/commit/deadbeef',
        )

        transfered = Redirector.from_json(json.dumps([redir], cls=Redirector.Encoder))[0]
        self.assertEqual(redir.type, transfered.type)
        self.assertEqual(redir(None).headers.get('location'), transfered(None).headers.get('location'))
        self.assertEqual(
            redir(Commit(hash='deadbeef')).headers.get('location'),
            transfered(Commit(hash='deadbeef')).headers.get('location'),
        )

    def test_fallback(self):
        redir = Redirector('https://github.com/WebKit/WebKit', fallback=Redirector('https://github.com/WebKit/WebKit-security'))
        self.assertEqual(
            redir(None).headers.get('location'),
            'https://github.com/WebKit/WebKit/commits',
        )
        self.assertEqual(
            redir(Commit(hash='deadbeef', message='defined')).headers.get('location'),
            'https://github.com/WebKit/WebKit/commit/deadbeef',
        )
        self.assertEqual(
            redir(Commit(hash='deadbeef')).headers.get('location'),
            'https://github.com/WebKit/WebKit-security/commit/deadbeef',
        )

    def test_fallback_compare(self):
        redir = Redirector('https://github.com/WebKit/WebKit', fallback=Redirector('https://github.com/WebKit/WebKit-security'))
        self.assertEqual(
            redir(None).headers.get('location'),
            'https://github.com/WebKit/WebKit/commits',
        )
        self.assertEqual(
            redir.compare(Commit(hash='deadbeef', message='defined'), Commit(hash='beefdead', message='defined')).headers.get('location'),
            'https://github.com/WebKit/WebKit/compare/beefdead...deadbeef',
        )
        self.assertEqual(
            redir.compare(Commit(hash='deadbeef'), Commit(hash='beefdead')).headers.get('location'),
            'https://github.com/WebKit/WebKit-security/compare/beefdead...deadbeef',
        )


class CheckoutRouteUnittest(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(CheckoutRouteUnittest, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    @mock_app
    def test_landing(self, app=None, client=None):
        with mocks.local.Git(self.path) as repo, OutputCapture():
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://trac.webkit.org')],
            ))
            response = client.get('/')
            self.assertEqual(response.status_code, 302)
            self.assertEqual(response.headers.get('location'), 'https://trac.webkit.org')

    @mock_app
    def test_json(self, app=None, client=None):
        with mocks.local.Git(self.path) as repo:
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://trac.webkit.org')],
            ))
            reference = Commit.Encoder().default(repo.commits['main'][3])
            del reference['author']
            del reference['message']

            response = client.get('4@main/json')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.json(), reference)

    @mock_app
    def test_json_invalid(self, app=None, client=None):
        with mocks.local.Git(self.path) as repo:
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://trac.webkit.org')],
            ))

            response = client.get('bae5d1e90999~1/json')
            self.assertEqual(response.status_code, 404)
            self.assertEqual(response.json(), dict(
                status='Not Found',
                error='Object of type NoneType is not JSON serializable',
                message='No commit with the specified reference could be found',
            ))

    @mock_app
    def test_redirect(self, app=None, client=None):
        with mocks.local.Git(self.path) as repo:
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://github.com/WebKit/WebKit')],
            ))

            response = client.get('1@branch-a')
            self.assertEqual(response.status_code, 302)
            self.assertEqual(
                response.headers.get('location'),
                'https://github.com/WebKit/WebKit/commit/a30ce8494bf1ac2807a69844f726be4a9843ca55',
            )

    @mock_app
    def test_invoked_redirect(self, app=None, client=None):
        with mocks.local.Git(self.path, git_svn=True) as repo, OutputCapture():
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://github.com/WebKit/WebKit'), Redirector('https://trac.webkit.org')],
            ))

            response = client.get('a30ce849/trac')
            self.assertEqual(response.status_code, 302)
            self.assertEqual(
                response.headers.get('location'),
                'https://trac.webkit.org/changeset/3/webkit',
            )

    @mock_app
    def test_trac(self, app=None, client=None):
        with mocks.local.Git(self.path, git_svn=True) as repo, OutputCapture():
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://github.com/WebKit/WebKit')],
            ))

            response = client.get('/changeset/6/webkit')
            self.assertEqual(response.status_code, 302)
            self.assertEqual(
                response.headers.get('location'),
                'https://github.com/WebKit/WebKit/commit/621652add7fc416099bd2063366cc38ff61afe36',
            )

    @mock_app
    def test_compare(self, app=None, client=None):
        with mocks.local.Git(self.path) as repo:
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://github.com/WebKit/WebKit')],
            ))

            response = client.get('compare/2@main...4@main')
            self.assertEqual(response.status_code, 302)
            self.assertEqual(
                response.headers.get('location'),
                'https://github.com/WebKit/WebKit/compare/fff83bb2d9171b4d9196e977eb0508fd57e7a08d...bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
            )

    @mock_app
    def test_compare_trac(self, app=None, client=None):
        with mocks.local.Git(self.path) as repo:
            app.register_blueprint(CheckoutRoute(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                redirectors=[Redirector('https://github.com/WebKit/WebKit'), Redirector('https://trac.webkit.org')],
            ))

            response = client.get('compare/2@main...4@main')
            self.assertEqual(response.status_code, 302)
            self.assertEqual(
                response.headers.get('location'),
                'https://github.com/WebKit/WebKit/compare/fff83bb2d9171b4d9196e977eb0508fd57e7a08d...bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
            )
