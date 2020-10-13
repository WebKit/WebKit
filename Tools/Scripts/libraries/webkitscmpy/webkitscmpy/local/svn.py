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

import bisect
import calendar
import json
import os
import re
import subprocess
import sys

from datetime import datetime, timedelta
from dateutil import tz

from webkitcorepy import run, decorators
from webkitscmpy.local.scm import Scm
from webkitscmpy import Commit, Contributor, Version


class Svn(Scm):
    executable = '/usr/bin/svn' if os.path.exists('/usr/bin/svn') else '/usr/local/bin/svn'
    LOG_RE = re.compile(r'r(?P<revision>\d+) \| (?P<email>.*) \| (?P<date>.*)')
    CACHE_VERSION = Version(1)

    @classmethod
    def is_checkout(cls, path):
        return run([cls.executable, 'info'], cwd=path, capture_output=True).returncode == 0

    def __init__(self, path, dev_branches=None, prod_branches=None):
        super(Svn, self).__init__(path, dev_branches=dev_branches, prod_branches=prod_branches)

        self._root_path = self.path
        self._root_path = self.info(cached=False).get('Working Copy Root Path')

        if not self.root_path:
            raise OSError('Provided path {} is not a svn repository'.format(path))

        if os.path.exists(self._cache_path):
            try:
                with open(self._cache_path) as file:
                    self._metadata_cache = json.load(file)
            except BaseException:
                self._metadata_cache = dict(version=str(self.CACHE_VERSION))
                raise
        else:
            self._metadata_cache = dict(version=str(self.CACHE_VERSION))

    @decorators.Memoize(cached=False)
    def info(self, branch=None, revision=None):
        revision = Commit._parse_revision(revision)
        additional_args = ['^/branches/{}'.format(branch)] if branch and branch != self.default_branch else []
        additional_args += ['-r', str(revision)] if revision else []

        info_result = run([self.executable, 'info'] + additional_args, cwd=self.root_path, capture_output=True, encoding='utf-8')
        if info_result.returncode:
            return {}

        result = {}
        for line in info_result.stdout.splitlines():
            split = line.split(': ')
            result[split[0]] = ': '.join(split[1:])
        return result

    @property
    def is_svn(self):
        return True

    @property
    def root_path(self):
        return self._root_path

    @property
    def default_branch(self):
        return 'trunk'

    @property
    def branch(self):
        local_path = self.path[len(self.root_path):]
        if local_path:
            return self.info()['Relative URL'][2:-len(local_path)]
        return self.info()['Relative URL'][2:]

    def list(self, category):
        list_result = run([self.executable, 'list', '^/{}'.format(category)], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if list_result.returncode:
            return []
        return [element.rstrip('/') for element in list_result.stdout.splitlines()]

    @property
    def branches(self):
        return ['trunk'] + self.list('branches')

    @property
    def tags(self):
        return self.list('tags')

    @property
    def _cache_path(self):
        return os.path.join(self.root_path, '.svn', 'webkitscmpy-cache.json')

    def _cache_revisions(self, branch=None):
        branch = branch or self.default_branch
        is_default_branch = branch == self.default_branch
        if branch not in self._metadata_cache:
            self._metadata_cache[branch] = [0] if is_default_branch else []
        pos = len(self._metadata_cache[branch])

        # If we aren't on the default branch, we will need the default branch to determine when
        # our  branch  intersects with the default branch.
        if not is_default_branch:
            self._cache_revisions(branch=self.default_branch)

        try:
            did_warn = False
            count = 0
            log = None
            candidate_index = -1 if is_default_branch else len(self._metadata_cache[self.default_branch])

            branch_arg = '^/{}{}'.format('' if is_default_branch else 'branches/', branch)
            kwargs = dict()
            if sys.version_info >= (3, 0):
                kwargs = dict(encoding='utf-8')

            log = subprocess.Popen(
                [self.executable, 'log', '-q', branch_arg],
                cwd=self.root_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                **kwargs
            )
            if log.poll():
                raise self.Exception("Failed to construct branch history for '{}'".format(branch))

            line = log.stdout.readline()
            while line:
                match = self.LOG_RE.match(line)
                if match:
                    if not did_warn:
                        count += 1
                        if count > 1000:
                            self.log('Caching commit data for {}, this will take a few minutes...'.format(branch))
                            did_warn = True

                    revision = int(match.group('revision'))
                    if pos > 0 and self._metadata_cache[branch][pos - 1] == revision:
                        break
                    if not is_default_branch:
                        if revision in self._metadata_cache[self.default_branch]:
                            self._metadata_cache[branch].insert(pos, revision)
                            break
                    self._metadata_cache[branch].insert(pos, revision)
                line = log.stdout.readline()
        finally:
            if log:
                log.kill()

        if self._metadata_cache[self.default_branch][0] == [0]:
            self._metadata_cache['identifier'] = len(self._metadata_cache[branch])

        try:
            with open(self._cache_path, 'w') as file:
                json.dump(self._metadata_cache, file, indent=4)
        except (IOError, OSError):
            self.log("Failed to write SVN cache to '{}'".format(self._cache_path))

        return self._metadata_cache[branch]

    def _commit_count(self, revision=None, branch=None):
        branch = branch or self.default_branch

        if revision:
            if revision not in self._metadata_cache[branch]:
                raise self.Exception("Failed to find '{}' on '{}'".format(revision, branch))
            return bisect.bisect_left(self._metadata_cache[branch], int(revision))
        if branch == self.default_branch:
            return len(self._metadata_cache[branch])
        return self._commit_count(revision=self._metadata_cache[branch][0], branch=self.default_branch)

    def remote(self, name=None):
        return self.info(cached=True)['Repository Root']

    def _branch_for(self, revision):
        candidates = [branch for branch, revisions in self._metadata_cache.items() if branch != 'version' and revision in revisions]
        candidate = self.prioritize_branches(candidates) if candidates else None

        # In the default branch case, we don't even need to ask the remote
        if candidate == self.default_branch:
            return candidate

        process = run(
            [self.executable, 'log', '-v', '-q', self.remote(), '-r', str(revision), '-l', '1'],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )

        # If we didn't get a valid answer from the remote, but we found a matching candidate, we return that.
        # This is a bit risky because there is a chance the branch we have cached is not the canonical branch
        # for a revision, but this is pretty unlikely because it would require the n + 1 level branch to be cached
        # but not the n level branch.
        if process.returncode or not process.stdout:
            if candidate:
                return candidate
            raise self.Exception("Failed to retrieve branch for '{}'".format(revision))

        partial = None
        for line in process.stdout.splitlines():
            if partial is None and line == 'Changed paths:':
                partial = ''
            elif partial == '':
                partial = line.lstrip()
            elif partial:
                line = line.lstrip()
                while line.startswith('M ') and not line.startswith(partial):
                    partial = partial[:-1]

        if len(partial) <= 3:
            raise self.Exception('Malformed set  of edited files')
        partial = partial[2:].split(' ')[0]
        return partial.split('/')[2 if partial.startswith('/branches') else 1]

    def commit(self, hash=None, revision=None, identifier=None, branch=None):
        if hash:
            raise ValueError('SVN does not support Git hashes')

        parsed_branch_point = None
        if identifier is not None:
            if revision:
                raise ValueError('Cannot define both revision and identifier')

            parsed_branch_point, identifier, parsed_branch = Commit._parse_identifier(identifier, do_assert=True)
            if parsed_branch:
                if branch and branch != parsed_branch:
                    raise ValueError(
                        "Caller passed both 'branch' and 'identifier', but specified different branches ({} and {})".format(
                            branch, parsed_branch,
                        ),
                    )
                branch = parsed_branch
            branch = branch or self.branch

            if branch == self.default_branch and parsed_branch_point:
                raise self.Exception('Cannot provide a branch point for a commit on the default branch')

            if not self._metadata_cache.get(branch, []) or identifier >= len(self._metadata_cache.get(branch, [])):
                if branch != self.default_branch:
                    self._cache_revisions(branch=self.default_branch)
                self._cache_revisions(branch=branch)
            if identifier > len(self._metadata_cache.get(branch, [])):
                raise self.Exception('Identifier {} cannot be found on the specified branch in the current checkout'.format(identifier))

            if identifier <= 0:
                if branch == self.default_branch:
                    raise self.Exception('Illegal negative identifier on the default branch')
                identifier = self._commit_count(branch=branch) + identifier
                if identifier < 0:
                    raise self.Exception('Identifier does not exist on the specified branch')

                branch = self.default_branch

            revision = self._metadata_cache[branch][identifier]
            info = self.info(cached=True, branch=branch, revision=revision)
            branch = self._branch_for(revision)

        elif revision:
            if branch:
                raise ValueError('Cannot define both branch and revision')
            revision = Commit._parse_revision(revision, do_assert=True)
            branch = self._branch_for(revision)
            info = self.info(cached=True, branch=branch, revision=revision)

        else:
            branch = branch or self.branch
            info = self.info(branch=branch)
            if not info:
                raise self.Exception("'{}' is not a recognized branch".format(branch))
            revision = int(info['Last Changed Rev'])
            if branch != self.default_branch:
                branch = self._branch_for(revision)

        date = info['Last Changed Date'].split(' (')[0]
        tz_diff = date.split(' ')[-1]
        date = datetime.strptime(date[:-len(tz_diff)], '%Y-%m-%d %H:%M:%S ')
        delta = timedelta(
            hours=int(tz_diff[1:3]),
            minutes=int(tz_diff[3:5]),
        )
        date = date.replace(
            tzinfo=tz.tzoffset(
                name='UTC{}'.format(tz_diff),
                offset=delta.seconds * (-1 if tz_diff[0] == '-' else 1)
            ),
        )

        if not identifier:
            if branch != self.default_branch and revision > self._metadata_cache.get(self.default_branch, [0])[-1]:
                self._cache_revisions(branch=self.default_branch)
            if revision not in self._metadata_cache.get(branch, []):
                self._cache_revisions(branch=branch)
            identifier = self._commit_count(revision=revision, branch=branch)

        branch_point = None if branch == self.default_branch else self._commit_count(branch=branch)
        if branch_point and parsed_branch_point and branch_point != parsed_branch_point:
            raise ValueError("Provided 'branch_point' does not match branch point of specified branch")

        branch_arg = '^/{}{}'.format('' if branch == self.default_branch else 'branches/', branch)
        log = run(
            [self.executable, 'log', '-l', '1', '-r', str(revision), branch_arg], cwd=self.root_path,
            capture_output=True, encoding='utf-8',
        )
        split_log = log.stdout.splitlines()
        if not log.returncode or len(split_log) >= 3:
            author_line = split_log[1]
            for line in split_log[2:8]:
                if Contributor.SVN_PATCH_FROM_RE.match(line):
                    author_line = line
                    break

            author = Contributor.from_scm_log(author_line)
            message = '\n'.join(split_log[3:-1])
        else:
            self.log('Failed to connect to remote, cannot compute commit message')
            email = info['Last Changed Author']
            author = Contributor.by_email.get(
                email,
                Contributor.by_name.get(
                    email,
                    Contributor(name=email, emails=[email]),
                ),
            )
            message = None

        return Commit(
            revision=int(revision),
            branch=branch,
            identifier=identifier,
            branch_point=branch_point,
            timestamp=int(calendar.timegm(date.timetuple())),
            author=author,
            message=message,
        )
