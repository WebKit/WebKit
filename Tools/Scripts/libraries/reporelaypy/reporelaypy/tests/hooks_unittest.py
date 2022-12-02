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

from reporelaypy import Checkout, Database, HookProcessor, HookReceiver
from webkitcorepy import testing, OutputCapture
from webkitflaskpy import mock_app
from webkitscmpy import mocks, Commit


class HooksUnittest(testing.PathTestCase):
    basepath = 'mock/repository'

    def setUp(self):
        super(HooksUnittest, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    @mock_app
    def test_receive(self, app=None, client=None):
        app.register_blueprint(HookReceiver(debug=True))
        response = client.get('/hooks')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [])

        response = client.post('/hooks', json=dict(ref='refs/heads/main'), headers={'X-GitHub-Event': 'push'})
        self.assertEqual(response.status_code, 202)
        self.assertEqual(response.json(), dict(
            status='Success',
            message='Hook queued for processing',
        ))

        response = client.get('/hooks')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [dict(data=dict(ref='refs/heads/main'), type='push')])

    @mock_app
    def test_invalid(self, app=None, client=None):
        app.register_blueprint(HookReceiver(debug=True))
        response = client.get('/hooks')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json(), [])

        response = client.post('/hooks', json=['invalid'], headers={'X-GitHub-Event': 'push'})
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json(), dict(
            status='Unrecognized Hook',
            message='Provided hook is in unrecognized format',
        ))

    @mock_app
    def test_process(self, app=None, client=None):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo:
            database = Database()
            app.register_blueprint(HookReceiver(database=database, debug=True))

            response = client.post('/hooks', json=dict(ref='refs/heads/main'), headers={'X-GitHub-Event': 'push'})
            self.assertEqual(response.status_code, 202)

            processor = HookProcessor(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                database=database,
            )
            processor.process_hooks()

            response = client.get('/hooks')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.json(), [])

        self.assertEqual(captured.stderr.getvalue(), '')

    @mock_app
    def test_process_branch(self, app=None, client=None):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo:
            database = Database()
            app.register_blueprint(HookReceiver(database=database, debug=True))

            response = client.post('/hooks', json=dict(ref='refs/heads/branch-a'), headers={'X-GitHub-Event': 'push'})
            self.assertEqual(response.status_code, 202)

            processor = HookProcessor(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                database=database,
            )
            processor.process_hooks()

            response = client.get('/hooks')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.json(), [])

        self.assertEqual(captured.stderr.getvalue(), "Failed to update 'branch-a' from 'origin'\n")

    @mock_app
    def test_hmac(self, app=None, client=None):
        app.register_blueprint(HookReceiver(debug=True, secret='secret'))

        response = client.post(
            '/hooks', json=dict(ref='refs/heads/main'),
            headers={
                'X-Hub-Signature-256': 'sha256=36a089f93b92e972b714e8c0f008873a206c690a8aee946a787eeb0f23e131b2',
                'X-GitHub-Event': 'push',
            },
        )
        self.assertEqual(response.status_code, 202)
        self.assertEqual(response.json(), dict(
            status='Success',
            message='Hook queued for processing',
        ))

    @mock_app
    def test_invalid_hmac(self, app=None, client=None):
        app.register_blueprint(HookReceiver(debug=True, secret='secret'))

        response = client.post('/hooks', json=dict(ref='refs/heads/main'), headers={'X-GitHub-Event': 'push'})
        self.assertEqual(response.status_code, 403)
        self.assertEqual(response.json(), dict(
            status='Unauthorized Hook',
            message='HMAC verification failed failed',
        ))

    @mock_app
    def test_pull_request(self, app=None, client=None):
        with OutputCapture() as captured, mocks.local.Git(self.path) as repo:
            database = Database()
            app.register_blueprint(HookReceiver(database=database, debug=True))

            response = client.post(
                '/hooks', headers={'X-GitHub-Event': 'pull_request'},
                json=dict(action='synchronize', number=58),
            )
            self.assertEqual(response.status_code, 202)

            def func(data):
                print('action: {}'.format(data.get('action')))
                print('number: {}'.format(data.get('number')))

            processor = HookProcessor(
                Checkout(path=self.path, url=repo.remote, sentinal=False),
                database=database,
                callbacks=dict(pull_request=func),
            )
            processor.process_hooks()

            response = client.get('/hooks')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.json(), [])

        self.assertEqual('', captured.stderr.getvalue())
        self.assertEqual(
            'action: synchronize\nnumber: 58\n',
            captured.stdout.getvalue(),
        )
