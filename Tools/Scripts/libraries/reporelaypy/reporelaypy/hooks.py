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

import hashlib
import hmac
import json
import re
import sys

from flask import current_app, json as fjson, request
from reporelaypy import Database
from webkitflaskpy import AuthedBlueprint
from webkitcorepy import Environment, string_utils

TIMEOUT = 30 * 60


class HookProcessor(object):
    INBOUND_KEY = 'inbound-hooks'
    WORKER_HOOKS = 'worker-hooks'
    TYPES = ('pull_request', 'push')
    BRANCH_PREFIX = 'refs/heads/'
    TAG_PREFIX = 'refs/tags/'
    REMOTES_RE = re.compile(r'remote\.(?P<remote>\S+)\.url')

    @classmethod
    def is_valid(cls, type, data):
        if type not in cls.TYPES or not isinstance(data, dict):
            return False
        if type == 'push':
            return bool(data.get('ref'))
        if type == 'pull_request':
            return bool(data.get('number')) and data.get('action') in ('edited', 'synchronize')
        return True

    def __init__(self, checkout, database=None, num_workers=1, worker_index=0, callbacks=None):
        self.checkout = checkout
        self.database = database or Database()
        self.num_workers = num_workers
        self.worker_index = worker_index
        self.callbacks = callbacks or dict()

    def process_hook(self, type, data):
        if not self.is_valid(type, data):
            return None

        self.callbacks.get(type, lambda _: _)(data)

    def process_worker_hook(self, type, data):
        if not self.is_valid(type, data):
            return None

        branch = data.get('ref')
        remote = ''
        name = data.get('repository', {}).get('url', '').split('://')[-1]
        if name and self.checkout.repository:
            for config_arg, url in self.checkout.repository.config().items():
                match = self.REMOTES_RE.match(config_arg)
                if not match or '{}.git'.format(name) not in [url.split('@')[-1], url.split('://')[-1]]:
                    continue
                remote = match.group('remote')
                break

        if type == 'push' and branch:
            try:
                if branch.startswith(self.BRANCH_PREFIX):
                    branch = branch[len(self.BRANCH_PREFIX):]
                    if self.checkout.update_for(branch, track=True, remote=remote):
                        self.checkout.forward_update(branch=branch, remote=remote, track=True)
                    else:
                        sys.stderr.write("Failed to update '{}' from '{}'\n".format(branch, remote or self.checkout.repository.default_remote))

                if branch.startswith(self.TAG_PREFIX):
                    tag = branch[len(self.TAG_PREFIX):]
                    self.checkout.fetch(remote=remote)
                    self.checkout.forward_update(tag=tag, remote=remote)

            except BaseException as e:
                sys.stderr.write('{}\n'.format(e))

    def process_hooks(self):
        with self.database.lock(name='lock_{}'.format(self.INBOUND_KEY), timeout=60):
            for key in self.database.scan_iter('{}:*'.format(self.INBOUND_KEY)):
                data = json.loads(self.database.get(string_utils.decode(key)))
                self.process_hook(type=data.get('type'), data=data.get('data'))

                encoded = json.dumps(data)
                digest = hashlib.md5()
                digest.update(string_utils.encode(encoded))
                digest = digest.hexdigest()
                for i in range(self.num_workers):
                    self.database.set('{}-{}:{}'.format(HookProcessor.WORKER_HOOKS, i, digest), encoded, ex=TIMEOUT)
                self.database.delete(string_utils.decode(key))

        for key in self.database.scan_iter('{}-{}:*'.format(self.WORKER_HOOKS, self.worker_index)):
            data = json.loads(self.database.get(string_utils.decode(key)))
            self.process_worker_hook(type=data.get('type'), data=data.get('data'))
            self.database.delete(string_utils.decode(key))


class HookReceiver(AuthedBlueprint):
    SECRET_ENV = 'HOOK_SECRET'

    def __init__(self, import_name=__name__, auth_decorator=None, database=None, debug=False, secret=None):
        super(HookReceiver, self).__init__('hooks', import_name, url_prefix='/hooks', auth_decorator=auth_decorator)

        self.database = database or Database()
        self.secret = secret

        self.add_url_rule('', 'inbound', self.inbound, methods=('POST',))
        if debug:
            self.add_url_rule('', 'received', self.received, methods=('GET',))

    def inbound(self):
        if self.secret and request.headers.get('X-Hub-Signature-256', '') != 'sha256={}'.format(hmac.new(
            string_utils.encode(self.secret), request.data, hashlib.sha256,
        ).hexdigest()):
            return current_app.response_class(
                fjson.dumps(dict(
                    status='Unauthorized Hook',
                    message='HMAC verification failed failed',
                ), indent=4),
                mimetype='application/json',
                status=403,
            )

        type = request.headers.get('X-GitHub-Event', '')
        data = request.get_json(silent=True, force=False)
        if HookProcessor.is_valid(type, data):
            encoded = json.dumps(dict(type=type, data=data))
            digest = hashlib.md5()
            digest.update(string_utils.encode(encoded))

            self.database.set('{}:{}'.format(HookProcessor.INBOUND_KEY, digest.hexdigest()), encoded, ex=TIMEOUT)

            return current_app.response_class(
                fjson.dumps(dict(
                    status='Success',
                    message='Hook queued for processing',
                ), indent=4),
                mimetype='application/json',
                status=202,
            )

        return current_app.response_class(
            fjson.dumps(dict(
                status='Unrecognized Hook',
                message='Provided hook is in unrecognized format',
            ), indent=4),
            mimetype='application/json',
            status=400,
        )

    def received(self):
        result = [
            json.loads(self.database.get(string_utils.decode(key)))
            for key in self.database.scan_iter('{}:*'.format(HookProcessor.INBOUND_KEY))
        ]
        return current_app.response_class(
            fjson.dumps(result, indent=4),
            mimetype='application/json',
            status=200,
        )
