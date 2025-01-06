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

import bisect
import calendar
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import time

from datetime import datetime, timedelta

from webkitcorepy import log, run, decorators
from webkitscmpy.local.scm import Scm
from webkitscmpy import remote, Commit, Contributor, Version


class Svn(Scm):
    class Cache(object):
        LOG_RE = re.compile(r'r(?P<revision>\d+) \| (?P<email>.*) \| (?P<date>.*)')
        VERSION = Version(1)

        def __init__(self, repo, guranteed_for=10):
            self.repo = repo
            self._last_populated = {}
            self._guranteed_for = guranteed_for

            if os.path.exists(self.path):
                try:
                    with open(self.path, 'r') as file:
                        self._data = json.load(file)
                except BaseException:
                    self._data = dict(version=str(self.VERSION))
            else:
                self._data = dict(version=str(self.VERSION))

        @property
        def path(self):
            return os.path.join(self.repo.common_directory, 'webkitscmpy-cache.json')

        def populate(self, branch=None):
            branch = branch or self.repo.default_branch
            if self._last_populated.get(branch, 0) + self._guranteed_for > time.time():
                return

            is_default_branch = branch == self.repo.default_branch
            if branch not in self._data:
                self._data[branch] = [0] if is_default_branch else []
            pos = len(self._data[branch])

            # If we aren't on the default branch, we will need the default branch to determine when
            # our  branch  intersects with the default branch.
            if not is_default_branch:
                self.populate(branch=self.repo.default_branch)

            try:
                did_warn = False
                count = 0
                log = None

                if is_default_branch or '/' in branch:
                    branch_arg = '^/{}'.format(branch)
                else:
                    branch_arg = '^/branches/{}'.format(branch)

                self._last_populated[branch] = time.time()
                log = subprocess.Popen(
                    [self.repo.executable(), 'log', '-q', branch_arg],
                    cwd=self.repo.root_path,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    encoding='utf-8',
                )
                if log.poll():
                    raise self.repo.Exception("Failed to construct branch history for '{}'".format(branch))

                default_count = 0
                line = log.stdout.readline()
                while line:
                    match = self.LOG_RE.match(line)
                    if match:
                        if not did_warn:
                            count += 1
                            if count > 1000:
                                self.repo.log('Caching commit data for {}, this will take a few minutes...'.format(branch))
                                did_warn = True

                        revision = int(match.group('revision'))
                        if pos > 0 and self._data[branch][pos - 1] == revision:
                            break
                        if not is_default_branch:
                            if revision in self._data[self.repo.default_branch]:
                                # Only handle 2 sequential cross-branch commits
                                if default_count > 2:
                                    break
                                default_count += 1
                            else:
                                default_count = 0
                        self._data[branch].insert(pos, revision)
                    line = log.stdout.readline()
            finally:
                if log and log.poll() is None:
                    log.kill()

            if default_count:
                self._data[branch] = self._data[branch][default_count - 1:]
            if self._data[self.repo.default_branch][0] == [0]:
                self._data['identifier'] = len(self._data[branch])

            try:
                with open(self.path, 'w') as file:
                    json.dump(self._data, file, indent=4)
            except (IOError, OSError):
                self.repo.log("Failed to write SVN cache to '{}'".format(self.path))

            return self._data[branch]

        def to_hash(self, **kwargs):
            return None

        def to_revision(self, hash=None, identifier=None, populate=True, branch=None):
            if hash:
                return None
            parts = Commit._parse_identifier(identifier, do_assert=False)
            if not parts:
                return None

            _, b_count, branch = parts
            if branch not in self._data and populate:
                self.populate(branch=branch)
            if branch not in self._data:
                return None

            if b_count < 0:
                bp_identifier = self.to_identifier(revision=self._data[branch][0])
                if not bp_identifier:
                    return None
                index = int(bp_identifier.split('@')[0]) + b_count
                if index <= 0:
                    return None
                return self._data[self.repo.default_branch][index]
            if b_count >= len(self._data[branch]) and populate:
                self.populate(branch=branch)
            if b_count >= len(self._data[branch]):
                return None
            return self._data[branch][b_count]

        def to_identifier(self, hash=None, revision=None, populate=True, branch=None):
            if hash:
                return None

            revision = Commit._parse_revision(revision, do_assert=False)
            if not revision:
                return None

            branch = branch or self.repo.branch
            if branch not in self._data and populate:
                self.populate(branch=branch)
            if branch not in self._data:
                return None

            index = bisect.bisect_left(self._data[branch], revision)
            exists = index < len(self._data[branch]) and self._data[branch][index] == revision
            if not exists:
                self.populate(branch=branch)
                index = bisect.bisect_left(self._data[branch], revision)
                exists = index < len(self._data[branch]) and self._data[branch] == revision

            if not exists or not index:
                if branch != self.repo.default_branch:
                    return self.to_identifier(branch=self.repo.default_branch, populate=populate)
                return None

            if branch == self.repo.default_branch:
                return '{}@trunk'.format(index, self.repo.default_branch)

            branch_point = bisect.bisect_left(self._data[self.repo.default_branch], self._data[branch][0])
            return '{}.{}@{}'.format(branch_point, index, branch)


    @classmethod
    @decorators.Memoize()
    def executable(cls):
        return Scm.executable('svn')

    @classmethod
    def is_checkout(cls, path):
        return run([cls.executable(), 'info'], cwd=path, capture_output=True).returncode == 0

    def __init__(self, path, dev_branches=None, prod_branches=None, contributors=None, id=None, cached=True, classifier=None,):
        self._root_path = path
        self._root_path = self.info(cached=False).get('Working Copy Root Path')
        if not self.root_path:
            raise OSError('Provided path {} is not a svn repository'.format(path))

        super(Svn, self).__init__(path, dev_branches=dev_branches, prod_branches=prod_branches, contributors=contributors, id=id, classifier=classifier,)
        self.cache = self.Cache(self) if cached else None

    @decorators.Memoize(cached=False)
    def info(self, branch=None, revision=None, tag=None):
        if tag and branch:
            raise ValueError('Cannot specify both branch and tag')
        if tag and revision:
            raise ValueError('Cannot specify both branch and tag')

        revision = Commit._parse_revision(revision)
        if branch and branch != self.default_branch and '/' not in branch:
            branch = 'branches/{}'.format(branch)
        additional_args = ['^/{}'.format(branch)] if branch and branch != self.default_branch else []
        additional_args += ['^/tags/{}'.format(tag)] if tag else []
        additional_args += ['-r', str(revision)] if revision else []

        info_result = run([self.executable(), 'info'] + additional_args, cwd=self.root_path, capture_output=True, encoding='utf-8')
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
    def common_directory(self):
        return os.path.join(self.root_path, '.svn')

    @property
    def default_branch(self):
        return 'trunk'

    @property
    def branch(self):
        local_path = self.path[len(self.root_path):]
        relative_url = self.info()['Relative URL']
        if local_path and relative_url.endswith(local_path):
            return relative_url[2:-len(local_path)]
        return relative_url[2:]

    def list(self, category):
        list_result = run([self.executable(), 'list', '^/{}'.format(category)], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if list_result.returncode:
            return []
        return [element.rstrip('/') for element in list_result.stdout.splitlines()]

    @property
    def branches(self):
        return ['trunk'] + self.list('branches')

    def tags(self):
        return self.list('tags')

    def _commit_count(self, revision=None, branch=None):
        branch = branch or self.default_branch
        if not self.cache:
            raise self.Exception('No available cache, cannot count commits')

        if revision:
            if revision not in self.cache._data[branch]:
                if 'branch' not in branch and branch != 'trunk':
                    sys.stderr.write("Check if 'r{}' is a tag\n".format(revision))
                raise self.Exception("Failed to find '{}' on '{}'".format(revision, branch))
            return bisect.bisect_left(self.cache._data[branch], int(revision))
        if branch == self.default_branch:
            return len(self.cache._data[branch])
        return self._commit_count(revision=self.cache._data[branch][0], branch=self.default_branch)

    def url(self, name=None):
        return self.info(cached=True)['Repository Root']

    @decorators.Memoize()
    def remote(self, name=None):
        return remote.Svn(self.url(name=name), contributors=self.contributors)

    def _branch_for(self, revision):
        if not self.cache:
            raise self.Exception('No available cache, cannot determine branch')
        candidates = [branch for branch, revisions in self.cache._data.items() if branch != 'version' and revision in revisions]
        candidate = self.prioritize_branches(candidates) if candidates else None

        # In the default branch case, we don't even need to ask the remote
        if candidate == self.default_branch:
            return candidate

        process = run(
            [self.executable(), 'log', '-v', '-q', self.url(), '-r', str(revision), '-l', '1'],
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
                partial = line.lstrip()[2:]
            elif partial:
                line = line.lstrip()
                while line.startswith(('A ', 'D ', 'M ')) and not line[2:].startswith(partial):
                    partial = partial[:-1]

        if len(partial) <= 3:
            raise self.Exception('Malformed set  of edited files')
        partial = partial.split(' ')[0]
        candidate = partial.split('/')[2 if partial.startswith('/branches') else 1]

        # Tags are a unique case for SVN, because they're treated as branches in native SVN
        if candidate == 'tags':
            return partial[1:].rstrip('/')
        return candidate

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None, include_log=True, include_identifier=True):
        if hash:
            raise ValueError('SVN does not support Git hashes')

        # Determine commit info, revision and branch for a given identifier
        parsed_branch_point = None
        if identifier is not None:
            if revision:
                raise ValueError('Cannot define both revision and identifier')
            if tag:
                raise ValueError('Cannot define both tag and identifier')
            if not self.cache:
                raise self.Exception('No available cache, cannot access by identifier')

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

            # Populate mapping of revisions on their respective branches if we don't have enough revisions on the
            # branch specified by the provided identifier
            if not self.cache._data.get(branch, []) or identifier >= len(self.cache._data.get(branch, [])):
                if branch != self.default_branch:
                    self.cache.populate(branch=self.default_branch)
                self.cache.populate(branch=branch)
            if identifier > len(self.cache._data.get(branch, [])):
                raise self.Exception('Identifier {} cannot be found on the specified branch in the current checkout'.format(identifier))

            if identifier <= 0:
                if branch == self.default_branch:
                    raise self.Exception('Illegal negative identifier on the default branch')
                identifier = self._commit_count(branch=branch) + identifier
                if identifier < 0:
                    raise self.Exception('Identifier does not exist on the specified branch')

                branch = self.default_branch

            revision = self.cache._data[branch][identifier]
            info = self.info(cached=True, branch=branch, revision=revision)
            branch = self._branch_for(revision)
            if not self.cache._data.get(branch, []) or identifier >= len(self.cache._data.get(branch, [])):
                self.cache.populate(branch=branch)

        # Determine the commit info and branch for a given revision
        elif revision:
            if branch:
                raise ValueError('Cannot define both branch and revision')
            if tag:
                raise ValueError('Cannot define both tag and revision')
            revision = Commit._parse_revision(revision, do_assert=True)
            branch = self._branch_for(revision)
            info = self.info(cached=True, revision=revision)

        # Determine the commit info, revision and branch for a tag or branch.
        else:
            if branch and tag:
                raise ValueError('Cannot define both branch and tag')

            branch = None if tag else branch or self.branch
            info = self.info(tag=tag) if tag else self.info(branch=branch)
            if not info:
                raise self.Exception("'{}' is not a recognized {}".format(
                    tag or branch,
                    'tag' if tag else 'branch',
                ))
            revision = int(info['Last Changed Rev'])
            if branch != self.default_branch:
                branch = self._branch_for(revision)

        # Extract the commit time from the commit info
        date = info['Last Changed Date'].split(' (')[0] if info.get('Last Changed Date') else None
        if date:
            tz_diff = date.split(' ')[-1]
            date = datetime.strptime(date[:-len(tz_diff)], '%Y-%m-%d %H:%M:%S ')
            date += timedelta(
                hours=int(tz_diff[1:3]),
                minutes=int(tz_diff[3:5]),
            ) * (1 if tz_diff[0] == '-' else -1)

        # Compute the identifier if the function did not receive one and we were asked to
        if self.cache and include_identifier and not identifier:
            if branch != self.default_branch and revision > self.cache._data.get(self.default_branch, [0])[-1]:
                self.cache.populate(branch=self.default_branch)
            if revision not in self.cache._data.get(branch, []):
                self.cache.populate(branch=branch)
            identifier = self._commit_count(revision=revision, branch=branch)

        branch_point = None if not include_identifier or branch == self.default_branch else self._commit_count(branch=branch)
        if branch_point and parsed_branch_point and branch_point != parsed_branch_point:
            raise ValueError("Provided 'branch_point' does not match branch point of specified branch")

        # Determine the commit message for a commit. Note that in Subversion, this will always result in a network call
        # and is one of the major reasons the WebKit project uses ChangeLogs.
        if branch == self.default_branch or '/' in branch:
            branch_arg = '^/{}'.format(branch)
        else:
            branch_arg = '^/branches/{}'.format(branch)
        log = run(
            [self.executable(), 'log', '-l', '1', '-r', str(revision), branch_arg], cwd=self.root_path,
            capture_output=True, encoding='utf-8',
        ) if include_log else None
        split_log = log.stdout.splitlines() if log else []
        if log and (not log.returncode or len(split_log) >= 3):
            author_line = split_log[1]
            for line in split_log[2:8]:
                if Contributor.SVN_PATCH_FROM_RE.match(line):
                    author_line = line
                    break

            author = Contributor.from_scm_log(author_line, self.contributors)
            message = '\n'.join(split_log[3:-1])
        else:
            if include_log:
                self.log('Failed to connect to remote, cannot compute commit message')
            email = info.get('Last Changed Author')
            author = self.contributors.create(email, email) if '@' in email else self.contributors.create(email)
            message = None

        return Commit(
            repository_id=self.id,
            revision=int(revision),
            branch=branch,
            identifier=identifier if include_identifier else None,
            branch_point=branch_point,
            timestamp=int(calendar.timegm(date.timetuple())) if date else None,
            author=author,
            message=message,
        )

    def _args_from_content(self, content, include_log=True):
        leading = content.splitlines()[0]
        match = Contributor.SVN_AUTHOR_RE.match(leading) or Contributor.SVN_AUTHOR_Q_RE.match(leading)
        if not match:
            return {}

        tz_diff = match.group('date').split(' ', 2)[-1]
        date = datetime.strptime(match.group('date')[:-len(tz_diff)], '%Y-%m-%d %H:%M:%S ')
        date += timedelta(
            hours=int(tz_diff[1:3]),
            minutes=int(tz_diff[3:5]),
        ) * (1 if tz_diff[0] == '-' else -1)

        return dict(
            revision=int(match.group('revision')),
            timestamp=int(calendar.timegm(date.timetuple())),
            author=Contributor.from_scm_log(leading, self.contributors),
            message='\n'.join(content.splitlines()[2:]).rstrip() if include_log else None,
        )


    def commits(self, begin=None, end=None, include_log=True, include_identifier=True):
        begin, end = self._commit_range(begin=begin, end=end, include_identifier=include_identifier)
        previous = end
        if end.branch == self.default_branch or '/' in end.branch:
            branch_arg = '^/{}'.format(end.branch)
        else:
            branch_arg = '^/branches/{}'.format(end.branch)

        try:
            log = None
            log = subprocess.Popen(
                [self.executable(), 'log', '-r', '{}:{}'.format(
                    end.revision, begin.revision,
                ), branch_arg] + ([] if include_log else ['-q']),
                cwd=self.root_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                encoding='utf-8',
            )
            if log.poll():
                raise self.Exception('Failed to find commits between {} and {} on {}'.format(begin, end, branch_arg))

            content = ''
            line = log.stdout.readline()
            divider = '-' * 72
            while True:
                if line and line.rstrip() != divider:
                    content += line
                    line = log.stdout.readline()
                    continue

                if not content:
                    line = log.stdout.readline()
                    continue

                branch_point = previous.branch_point if include_identifier else None
                identifier = previous.identifier if include_identifier else None

                args = self._args_from_content(content, include_log=include_log)
                if args['revision'] != previous.revision:
                    yield previous
                    identifier -= 1
                if not identifier:
                    identifier = branch_point
                    branch_point = None

                previous = Commit(
                    repository_id=self.id,
                    branch=end.branch if branch_point else self.default_branch,
                    identifier=identifier,
                    branch_point=branch_point,
                    **args
                )
                content = ''
                if not line:
                    break
                line = log.stdout.readline()

            yield previous

        finally:
            if log and log.poll() is None:
                log.kill()

    def checkout(self, argument):
        commit = self.find(argument)
        if not commit:
            return None

        command = [self.executable(), 'up', '-r', str(commit.revision)]
        if log.level > logging.WARNING:
            command.append('-q')

        return None if run(command, cwd=self.root_path).returncode else commit

    def pull(self):
        return run([self.executable(), 'up'], cwd=self.root_path).returncode

    def clean(self):
        result = run([
            self.executable(), 'revert', '-R', self.root_path,
        ], cwd=self.root_path).returncode
        if result:
            return result

        for line in reversed(run([self.executable(), 'status'], cwd=self.root_path, capture_output=True, encoding='utf-8').stdout.splitlines()):
            candidate = line.split('       ')
            if candidate[0] != '?':
                continue
            path = os.path.join(self.root_path, '       '.join(candidate[1:]))
            if os.path.isdir(path):
                shutil.rmtree(path, ignore_errors=True)
            elif os.path.exists(path):
                os.remove(path)
        return 0
