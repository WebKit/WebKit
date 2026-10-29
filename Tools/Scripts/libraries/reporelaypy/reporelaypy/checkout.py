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
import subprocess
import sys
import time

from webkitcorepy import run
from webkitscmpy import local, remote

NOTHING_TO_PUSH = 'nothing-to-push'


class Checkout(object):
    REMOTE = 'origin'
    REMOTE_QUERY_TIMEOUT = 60

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

            result = dict(
                path=obj.path,
                url=obj.url,
                sentinal=obj.sentinal,
                remotes=obj.remotes,
                forwarding=obj.forwarding,
                credentials=obj.credentials,
            )
            if obj.fallback_repository:
                result['fallback_url'] = obj.fallback_repository.url
            return result

    @classmethod
    def from_json(cls, data):
        data = data if isinstance(data, dict) else json.loads(data)
        if isinstance(data, list):
            return [cls(**node, primary=False) for node in data]
        return cls(**data, primary=False)

    @staticmethod
    def clone(url, path, remotes, credentials, sentinal_file=None, checkout_data=None, disable_origin=True, post_clone_commands=None):
        run([local.Git.executable(), 'clone', url, path], cwd=os.path.dirname(path))
        run([local.Git.executable(), 'config', 'pull.ff', 'only'], cwd=path)
        if disable_origin:
            run([local.Git.executable(), 'remote', 'set-url', '--push', 'origin', 'INVALID'], cwd=os.path.dirname(path))

        Checkout.add_remotes(local.Git(path), remotes)
        Checkout.add_credentials(local.Git(path), credentials)

        for command in post_clone_commands or []:
            run(**command)

        if sentinal_file:
            with open(sentinal_file, 'w') as cloned:
                cloned.write('yes\n')

        if checkout_data:
            Checkout.from_json(checkout_data).update_all(remote=key)

        return 0

    @staticmethod
    def add_remotes(repository, remotes):
        for name, url in (remotes or {}).items():
            run([repository.executable(), 'remote', 'add', name, url], cwd=repository.root_path)

    @staticmethod
    def add_credentials(repository, credentials):
        git_credentials_content = ''

        for url, creds in (credentials or {}).items():
            username = creds.get('username')
            password = creds.get('password')
            if username:
                run(
                    [repository.executable(), 'config', '--local', 'credential.{}.username'.format(url), username],
                    cwd=repository.root_path,
                )
            protocol, host = url.split('://')
            if username and password and protocol and host:
                git_credentials_content += '{}://{}:{}@{}\n'.format(protocol, username, password, host)

        if not git_credentials_content:
            return

        run([repository.executable(), 'config', '--local', 'credential.helper', 'store'], cwd=repository.root_path)
        with open(os.path.expanduser('~/.git-credentials'), 'w') as f:
            f.write(git_credentials_content)

    def __init__(
        self, path, url=None, http_proxy=None,
        sentinal=True, fallback_url=None, primary=True,
        remotes=None, credentials=None,
        forwarding=None, disable_origin=True,
        post_clone_commands=None,
    ):
        self.sentinal = sentinal
        self.path = path
        self.url = url
        self.remotes = remotes or dict()
        self.forwarding = forwarding or [('origin', list(self.remotes.keys()))]
        self.credentials = credentials or dict()
        self._repository = None
        self._child_process = None
        self.fallback_repository = remote.Scm.from_url(fallback_url) if fallback_url else None
        if self.fallback_repository:
            self.fallback_repository.commit()

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
                    self.url = self.repository.url(name=self.REMOTE)
                if self.url and self.repository.url(name=self.REMOTE) != self.url:
                    sys.stderr.write("Specified '{}' as the URL, but the specified path is from '{}'\n".format(
                        self.url, self.repository.url(name=self.REMOTE),
                    ))
                if primary:
                    Checkout.add_remotes(self.repository, remotes)
                    Checkout.add_credentials(self.repository, self.credentials)
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

        checkout_data = Checkout.Encoder().default(self)
        if self.sentinal:
            self._child_process = multiprocessing.Process(
                target=self.clone,
                args=(self.url, path, self.remotes, self.credentials, self.sentinal_file, checkout_data, disable_origin, post_clone_commands),
            )
            self._child_process.start()
        else:
            self.clone(self.url, path, self.remotes, self.credentials, checkout_data=checkout_data, disable_origin=disable_origin, post_clone_commands=post_clone_commands)

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

    def is_updated(self, branch, remote=REMOTE):
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

    def push_update(self, branch=None, tag=None, remote=None, track=False, dest_branch=None):
        if not remote or remote in ('fork',):
            return NOTHING_TO_PUSH

        if tag:
            return not run(
                [self.repository.executable(), 'push', remote, tag],
                cwd=self.repository.root_path,
            ).returncode

        branch = branch or self.repository.default_branch
        if not track and self.is_updated(branch, remote=remote):
            return NOTHING_TO_PUSH

        # Make an attempt to push commits one-at-a-time to ensure we get CI jobs for each commit
        # Only push commits one-at-a-time if we are a) on main or b) have less than 50 individual commits.
        # This prevents branch-management from spinning up thousands of CI jobs or causing commits.webkit.org to hang.
        sentinel = run(
            [self.repository.executable(), 'rev-list', '--count', '{}/{}..{}'.format(remote, dest_branch or branch, branch)],
            cwd=self.repository.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        num_commits = 1 if sentinel.returncode else int(sentinel.stdout.rstrip())
        while num_commits > 1 and (num_commits < 50 or branch == self.repository.default_branch):
            num_commits -= 1
            run(
                [self.repository.executable(), 'push', remote, '{}~{}:{}'.format(branch, num_commits, dest_branch or branch)],
                cwd=self.repository.root_path,
            )

        return not run(
            [self.repository.executable(), 'push', remote, '{}:{}'.format(branch, dest_branch or branch)],
            cwd=self.repository.root_path,
        ).returncode

    def forwarding_targets(self, remote=None, branch=None, tag=None):
        """[(target_remote, dest_branch), ...] for a ref arriving from `remote`.

        Encodes the routing rules in self.forwarding, including their two subtleties:
        only the *first* matching rule applies, and a 'target:name' entry renames the
        ref on the way out. Callers must not iterate self.remotes directly -- the
        rules deliberately do not send every ref to every mirror.

        dest_branch is None when the ref keeps its name.
        """
        for match, tos in self.forwarding:
            if match not in (remote, '{}:{}'.format(remote, branch or tag)):
                continue
            targets = []
            for to in tos:
                split = to.split(':', 1)
                targets.append((split[0], split[1] if len(split) > 1 else None))
            return targets
        return []

    def forward_update(self, branch=None, tag=None, remote=None, track=False):
        did_succeed = True

        for target, dest_branch in self.forwarding_targets(remote=remote, branch=branch, tag=tag):
            pushed = self.push_update(
                branch=branch,
                tag=tag,
                remote=target,
                track=track,
                dest_branch=dest_branch,
            )
            did_succeed = (pushed is NOTHING_TO_PUSH or bool(pushed)) and did_succeed

        return did_succeed

    def fetch(self, remote=REMOTE):
        return not run(
            [self.repository.executable(), 'fetch', remote, '--prune'],
            cwd=self.repository.root_path,
        ).returncode

    def remote_sha(self, branch, remote):
        """Tip of `branch` on `remote`, read with ls-remote.

        Returns None if the ref is absent, and raises Checkout.Exception if the
        mirror could not be queried.
        """
        try:
            result = run(
                [self.repository.executable(), 'ls-remote', remote, 'refs/heads/{}'.format(branch)],
                cwd=self.repository.root_path,
                capture_output=True,
                encoding='utf-8',
                timeout=self.REMOTE_QUERY_TIMEOUT,
            )
        except subprocess.TimeoutExpired:
            raise self.Exception("Timed out querying '{}' for '{}'".format(remote, branch))
        if result.returncode:
            raise self.Exception("Failed to query '{}' for '{}'".format(remote, branch))
        for line in (result.stdout or '').splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[1] == 'refs/heads/{}'.format(branch):
                return parts[0]
        return None

    def local_sha(self, branch):
        result = run(
            [self.repository.executable(), 'rev-parse', 'refs/heads/{}'.format(branch)],
            cwd=self.repository.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if result.returncode:
            return None
        return (result.stdout or '').strip() or None

    def reconcile(self, branches=None, remote=REMOTE):
        """Push any mirror whose copy of one of `branches` disagrees with `remote`.

        Returns {branch: 'source-unknown' | 'no-targets'} for branches that could not
        be handled, and {(target, dest_branch): 'current' | 'pushed' | 'failed'}
        otherwise.
        """
        if not self.repository:
            sys.stderr.write('Cannot reconcile, clone still pending...\n')
            return {}

        branches = branches or [self.repository.default_branch]
        results = {}

        self.fetch(remote)
        for branch in branches:
            self.update_for(branch=branch, remote=remote)

            expected = self.local_sha(branch)
            if not expected:
                results[branch] = 'source-unknown'
                continue

            targets = self.forwarding_targets(remote=remote, branch=branch)
            if not targets:
                results[branch] = 'no-targets'
                continue

            for target, dest_branch in targets:
                dest = dest_branch or branch
                try:
                    actual = self.remote_sha(dest, target)
                except self.Exception as e:
                    sys.stderr.write('{}\n'.format(e))
                    results[(target, dest)] = 'failed'
                    continue
                if actual == expected:
                    results[(target, dest)] = 'current'
                    continue
                pushed = self.push_update(
                    branch=branch, remote=target, track=True, dest_branch=dest_branch,
                )
                results[(target, dest)] = 'failed' if pushed is False else 'pushed'

        return results

    def update_for(self, branch=None, remote=REMOTE, track=False):
        if not self.repository:
            sys.stderr.write("Cannot update '{}', clone still pending...\n".format(branch))
            return False

        branch = branch or self.repository.default_branch
        if branch == self.repository.default_branch:
            if self.repository.pull(remote=remote):
                return False
            self.repository.cache.populate(branch=branch)
            return True
        elif not self.repository.prod_branches.match(branch):
            return False
        elif track and branch not in self.repository.branches_for(remote=remote):
            run([self.repository.executable(), 'fetch', '--prune'], cwd=self.repository.root_path)
            run(
                [self.repository.executable(), 'branch', '--track', branch, 'remotes/{}/{}'.format(remote, branch)],
                cwd=self.repository.root_path,
            )
            self.repository.cache.populate(branch=branch)
        elif not track and self.is_updated(branch, remote=remote):
            return True

        run(
            [self.repository.executable(), 'fetch', remote, '{}:{}'.format(branch, branch)],
            cwd=self.repository.root_path,
        )
        self.repository.cache.populate(branch=branch)
        return True

    def update_all_todos(self, remote=None):
        if not self.repository:
            sys.stderr.write("Cannot update checkout, clone still pending...\n")
            return []

        if remote is None:
            updated = set()
            for key, _ in reversed(self.forwarding):
                key = key.split(':')[0]
                if key in updated:
                    continue
                for action in self.update_all_todos(remote=key):
                    yield action
                updated.add(key)
            return

        print('Forwarding updates from {}'.format(remote))

        self.fetch(remote)
        if self.repository.pull(remote=remote):
            print("    Failed to pull from '{}'".format(remote))
        else:
            self.repository.cache.populate(branch=self.repository.default_branch)

        def action(remote, branch=None, tag=None, track=False):
            if tag:
                print(f'    {tag}')
                self.forward_update(remote=remote, tag=tag)
                return

            if track:
                run(
                    [self.repository.executable(), 'branch', '--track', branch, 'remotes/{}/{}'.format(remote, branch)],
                    cwd=self.repository.root_path,
                )
                self.repository.cache.populate(branch=branch)
            else:
                if not self.update_for(branch=branch, remote=remote):
                    print("    Failed to update '{}' from '{}'".format(branch, remote or self.repository.default_remote))

            self.forward_update(branch=branch, remote=remote)

        # First, update all branches we're already tracking
        print('Updating branches...')
        all_branches = set(self.repository.branches_for(remote=remote))
        for branch in self.repository.branches_for(remote=False):
            if branch not in all_branches:
                continue
            all_branches.remove(branch)

            yield lambda: action(remote=remote, branch=branch)

        # Then, track all untracked branches
        print('Tracking new branches...')
        for branch in all_branches:
            yield lambda: action(remote=remote, branch=branch, track=True)

        # Sync all tags
        print('Syncing tags...')
        origin_tags = set(self.repository.tags(remote=remote))
        for target_remote in self.remotes.keys():
            print('Syncing tags for {}...'.format(target_remote))
            if target_remote == remote:
                continue
            remote_tags = set(self.repository.tags(remote=target_remote))
            for tag in origin_tags - remote_tags:
                yield lambda: action(remote=remote, tag=tag)

    def update_all(self, remote=None, throttle=None):
        is_first = True
        for todo in self.update_all_todos(remote=remote):
            if throttle and is_first:
                time.sleep(throttle)

            todo()
            is_first = False
