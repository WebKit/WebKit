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

import json
import multiprocessing
import os
import shutil
import sys

from webkitcorepy import run
from webkitscmpy import local


class Checkout(object):
    class Exception(RuntimeError):
        pass

    class Encoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, dict):
                return {key: self.default(value) for key, value in obj.items()}
            if isinstance(obj, list):
                return [self.default(value) for value in obj]
            if not isinstance(obj, Checkout):
                return super(Checkout.Encoder, self).default(obj)

            return dict(
                path=obj.path,
                url=obj.url,
                sentinal=obj.sentinal,
            )

    @classmethod
    def from_json(cls, data):
        data = data if isinstance(data, dict) else json.loads(data)
        if isinstance(data, list):
            return [cls(**node, primary=False) for node in data]
        return cls(**data, primary=False)

    @staticmethod
    def clone(url, path, sentinal_file=None):
        run([local.Git.executable(), 'clone', url, path], cwd=os.path.dirname(path))
        run([local.Git.executable(), 'config', 'pull.ff', 'only'], cwd=path)

        if sentinal_file:
            with open(sentinal_file, 'w') as cloned:
                cloned.write('yes\n')
        return 0

    def __init__(self, path, url=None, http_proxy=None, sentinal=True, primary=True):
        self.sentinal = sentinal
        self.path = path
        self.url = url
        self._repository = None
        self._child_process = None

        containing_path = os.path.dirname(path)
        if not os.path.isdir(containing_path):
            raise self.Exception("Containing path '{}' does not exist".format(containing_path))

        if http_proxy and run([
            local.Git.executable(), 'config', '--global', 'http.proxy', 'http://{}'.format(http_proxy),
        ]).returncode:
            split_proxy = http_proxy.split('@')
            raise self.Exception("Failed to set https proxy to '{}'".format(
                '{}@{}'.format('*' * len(split_proxy[0]), split_proxy[1]) if '@' in http_proxy else http_proxy,
            ))

        try:
            if self.repository:
                if not self.url:
                    self.url = self.repository.url(name='origin')
                if self.url and self.repository.url(name='origin') != self.url:
                    sys.stderr.write("Specified '{}' as the URL, but the specified path is from '{}'\n".format(
                        self.url, self.repository.url(name='origin'),
                    ))
                return
        except FileNotFoundError:
            pass

        if not primary:
            return

        if not self.url:
            raise self.Exception('No url provided to clone from')

        if os.path.isfile(self.sentinal_file):
            os.remove(self.sentinal_file)
        if os.path.isdir(path):
            shutil.rmtree(path, ignore_errors=True)

        if self.sentinal:
            self._child_process = multiprocessing.Process(
                target=self.clone,
                args=(self.url, path, self.sentinal_file),
            )
            self._child_process.start()
        else:
            self.clone(self.url, path)

    @property
    def sentinal_file(self):
        return os.path.join(os.path.dirname(self.path), 'cloned')

    @property
    def repository(self):
        if self._repository:
            return self._repository
        if os.path.isfile(self.sentinal_file) or not self.sentinal:
            if self._child_process:
                self._child_process.join()
                self._child_process = None
            self._repository = local.Git(self.path)
            return self._repository
        return None

    def is_updated(self, branch, remote='origin'):
        if not self.repository:
            sys.stderr.write('Cannot query checkout, clone still pending...\n')
            return None

        result = run(
            [self.repository.executable(), 'show-ref', branch],
            cwd=self.repository.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if result.returncode:
            return False
        ref = None
        for line in result.stdout.splitlines():
            if line.split()[-1].startswith('refs/heads'):
                ref = line.split()[0]
                break
        if not ref:
            return False
        for line in result.stdout.splitlines():
            if line.split()[-1].startswith('refs/remotes/{}'.format(remote)):
                return ref == line.split()[0]
        return False

    def update_for(self, branch=None, remote='origin'):
        if not self.repository:
            sys.stderr.write("Cannot update '{}', clone still pending...\n".format(branch))
            return None

        branch = branch or self.repository.default_branch
        if branch == self.repository.default_branch:
            self.repository.pull(remote=remote)
            self.repository.cache.populate(branch=branch)
            return True
        if not self.repository.prod_branches.match(branch):
            return False
        if self.is_updated(branch, remote=remote):
            return True

        run(
            [self.repository.executable(), 'fetch', remote, '{}:{}'.format(branch, branch)],
            cwd=self.repository.root_path,
        )
        self.repository.cache.populate(branch=branch)
        return True

    def update_all(self, remote='origin'):
        if not self.repository:
            sys.stderr.write("Cannot update checkout, clone still pending...\n")
            return None

        self.repository.pull(remote=remote)
        self.repository.cache.populate(branch=self.repository.default_branch)

        # First, update all branches we're already tracking
        all_branches = set(self.repository.branches_for(remote=remote))
        for branch in self.repository.branches_for(remote=False):
            if branch in all_branches:
                all_branches.remove(branch)
            self.update_for(branch=branch, remote=remote)

        # Then, track all untracked branches
        for branch in all_branches:
            run(
                [self.repository.executable(), 'branch', '--track', branch, 'remotes/{}/{}'.format(remote, branch)],
                cwd=self.repository.root_path,
            )
            self.repository.cache.populate(branch=branch)
