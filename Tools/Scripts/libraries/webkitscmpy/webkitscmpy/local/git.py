# Copyright (C) 2020, 2021 Apple Inc. All rights reserved.
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

import calendar
import logging
import os
import re
import six
import subprocess
import sys
import time

from datetime import datetime, timedelta

from webkitcorepy import run, decorators
from webkitscmpy.local import Scm
from webkitscmpy import Commit, Contributor, log


class Git(Scm):
    GIT_COMMIT = re.compile(r'commit (?P<hash>[0-9a-f]+)')

    @classmethod
    @decorators.Memoize()
    def executable(cls):
        return Scm.executable('git')

    @classmethod
    def is_checkout(cls, path):
        return run([cls.executable(), 'rev-parse', '--show-toplevel'], cwd=path, capture_output=True).returncode == 0

    def __init__(self, path, dev_branches=None, prod_branches=None, contributors=None, id=None):
        super(Git, self).__init__(path, dev_branches=dev_branches, prod_branches=prod_branches, contributors=contributors, id=id)
        if not self.root_path:
            raise OSError('Provided path {} is not a git repository'.format(path))

    @decorators.Memoize(cached=False)
    def info(self):
        if not self.is_svn:
            raise self.Exception('Cannot run SVN info on a git checkout which is not git-svn')

        info_result = run([self.executable(), 'svn', 'info'], cwd=self.path, capture_output=True, encoding='utf-8')
        if info_result.returncode:
            return {}

        result = {}
        for line in info_result.stdout.splitlines():
            split = line.split(': ')
            result[split[0]] = ': '.join(split[1:])
        return result

    @property
    @decorators.Memoize()
    def is_svn(self):
        config = os.path.join(self.root_path, '.git/config')
        if not os.path.isfile(config):
            return False

        with open(config, 'r') as config:
            for line in config.readlines():
                if line.startswith('[svn-remote "svn"]'):
                    return True
            return False

    @property
    def is_git(self):
        return True

    @property
    @decorators.Memoize()
    def root_path(self):
        result = run([self.executable(), 'rev-parse', '--show-toplevel'], cwd=self.path, capture_output=True, encoding='utf-8')
        if result.returncode:
            return None
        return result.stdout.rstrip()

    @property
    def default_branch(self):
        result = run([self.executable(), 'rev-parse', '--abbrev-ref', 'origin/HEAD'], cwd=self.path, capture_output=True, encoding='utf-8')
        if result.returncode:
            candidates = self.branches
            if 'master' in candidates:
                return 'master'
            if 'main' in candidates:
                return 'main'
            return None
        return '/'.join(result.stdout.rstrip().split('/')[1:])

    @property
    def branch(self):
        status = run([self.executable(), 'status'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if status.returncode:
            raise self.Exception('Failed to run `git status` for {}'.format(self.root_path))
        if status.stdout.splitlines()[0].startswith('HEAD detached at'):
            return None

        result = run([self.executable(), 'rev-parse', '--abbrev-ref', 'HEAD'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if result.returncode:
            raise self.Exception('Failed to retrieve branch for {}'.format(self.root_path))
        return result.stdout.rstrip()

    @property
    def branches(self):
        return self._branches_for()

    @property
    def tags(self):
        tags = run([self.executable(), 'tag'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if tags.returncode:
            raise self.Exception('Failed to retrieve tag list for {}'.format(self.root_path))
        return tags.stdout.splitlines()

    def remote(self, name=None):
        result = run([self.executable(), 'remote', 'get-url', name or 'origin'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if result.returncode:
            raise self.Exception('Failed to retrieve remote for {}'.format(self.root_path))
        return result.stdout.rstrip()

    def _commit_count(self, native_parameter):
        revision_count = run(
            [self.executable(), 'rev-list', '--count', '--no-merges', native_parameter],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )
        if revision_count.returncode:
            raise self.Exception('Failed to retrieve revision count for {}'.format(native_parameter))
        return int(revision_count.stdout)

    def _branches_for(self, hash=None):
        branch = run(
            [self.executable(), 'branch', '-a'] + (['--contains', hash] if hash else []),
            cwd=self.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if branch.returncode:
            raise self.Exception('Failed to retrieve branch list for {}'.format(self.root_path))
        result = [branch.lstrip(' *') for branch in filter(lambda branch: '->' not in branch, branch.stdout.splitlines())]
        return sorted(set(['/'.join(branch.split('/')[2:]) if branch.startswith('remotes/origin/') else branch for branch in result]))

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None, include_log=True, include_identifier=True):
        # Only git-svn checkouts can convert revisions to fully qualified commits
        if revision and not self.is_svn:
            raise self.Exception('This git checkout does not support SVN revisions')

        # Determine the hash for a provided Subversion revision
        elif revision:
            if hash:
                raise ValueError('Cannot define both hash and revision')

            revision = Commit._parse_revision(revision, do_assert=True)
            revision_log = run(
                [self.executable(), 'svn', 'find-rev', 'r{}'.format(revision)],
                cwd=self.root_path, capture_output=True, encoding='utf-8',
                timeout=3,
            )
            if revision_log.returncode:
                raise self.Exception("Failed to retrieve commit information for 'r{}'".format(revision))
            hash = revision_log.stdout.rstrip()
            if not hash:
                raise self.Exception("Failed to find 'r{}'".format(revision))

        default_branch = self.default_branch
        parsed_branch_point = None
        log_format = ['-1'] if include_log else ['-1', '--format=short']

        # Determine the `git log` output and branch for a given identifier
        if identifier is not None:
            if revision:
                raise ValueError('Cannot define both revision and identifier')
            if hash:
                raise ValueError('Cannot define both hash and identifier')
            if tag:
                raise ValueError('Cannot define both tag and identifier')

            parsed_branch_point, identifier, parsed_branch = Commit._parse_identifier(identifier, do_assert=True)
            if parsed_branch:
                if branch and branch != parsed_branch:
                    raise ValueError(
                        "Caller passed both 'branch' and 'identifier', but specified different branches ({} and {})".format(
                            branch, parsed_branch,
                        ),
                    )
                branch = parsed_branch

            baseline = branch or 'HEAD'
            is_default = baseline == default_branch
            if baseline == 'HEAD':
                is_default = default_branch in self._branches_for(baseline)

            if is_default and parsed_branch_point:
                raise self.Exception('Cannot provide a branch point for a commit on the default branch')

            base_count = self._commit_count(baseline if is_default else '{}..{}'.format(default_branch, baseline))

            if identifier > base_count:
                raise self.Exception('Identifier {} cannot be found on the specified branch in the current checkout'.format(identifier))
            log = run(
                [self.executable(), 'log', '{}~{}'.format(branch or 'HEAD', base_count - identifier)] + log_format,
                cwd=self.root_path,
                capture_output=True,
                encoding='utf-8',
            )
            if log.returncode:
                raise self.Exception("Failed to retrieve commit information for 'i{}@{}'".format(identifier, branch or 'HEAD'))

            # Negative identifiers are actually commits on the default branch, we will need to re-compute the identifier
            if identifier < 0 and is_default:
                raise self.Exception('Illegal negative identifier on the default branch')
            if identifier < 0:
                identifier = None

        # Determine the `git log` output for a given branch or tag
        elif branch or tag:
            if hash:
                raise ValueError('Cannot define both tag/branch and hash')
            if branch and tag:
                raise ValueError('Cannot define both tag and branch')

            log = run([self.executable(), 'log', branch or tag] + log_format, cwd=self.root_path, capture_output=True, encoding='utf-8')
            if log.returncode:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(branch or tag))

        # Determine the `git log` output for a given hash
        else:
            hash = Commit._parse_hash(hash, do_assert=True)
            log = run([self.executable(), 'log', hash or 'HEAD'] + log_format, cwd=self.root_path, capture_output=True, encoding='utf-8')
            if log.returncode:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(hash or 'HEAD'))

        # Fully define the hash from the `git log` output
        match = self.GIT_COMMIT.match(log.stdout.splitlines()[0])
        if not match:
            raise self.Exception('Invalid commit hash in git log')
        hash = match.group('hash')

        # A commit is often on multiple branches, the canonical branch is the one with the highest priority
        branch = self.prioritize_branches(self._branches_for(hash))

        # Compute the identifier if the function did not receive one and we were asked to
        if not identifier and include_identifier:
            identifier = self._commit_count(hash if branch == default_branch else '{}..{}'.format(default_branch, hash))

        # Only compute the branch point we're on something other than the default branch
        branch_point = None if not include_identifier or branch == default_branch else self._commit_count(hash) - identifier
        if branch_point and parsed_branch_point and branch_point != parsed_branch_point:
            raise ValueError("Provided 'branch_point' does not match branch point of specified branch")

        # Check the commit log for a git-svn revision
        logcontent = '\n'.join(line[4:] for line in log.stdout.splitlines()[4:])
        matches = self.GIT_SVN_REVISION.findall(logcontent)
        revision = int(matches[-1].split('@')[0]) if matches else None

        # We only care about when a commit was commited
        commit_time = run(
            [self.executable(), 'show', '-s', '--format=%ct', hash],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )
        if commit_time.returncode:
            raise self.Exception('Failed to retrieve commit time for {}'.format(hash))
        timestamp = int(commit_time.stdout.lstrip())

        # Comparing commits in different repositories involves comparing timestamps. This is problematic because it git,
        # it's possible for a series of commits to share a commit time. To handle this case, we assign each commit a
        # zero-indexed "order" within it's timestamp.
        order = 0
        while not identifier or order + 1 < identifier + (branch_point or 0):
            commit_time = run(
                [self.executable(), 'show', '-s', '--format=%ct', '{}~{}'.format(hash, order + 1)],
                cwd=self.root_path, capture_output=True, encoding='utf-8',
            )
            if commit_time.returncode:
                break
            if int(commit_time.stdout.lstrip()) != timestamp:
                break
            order += 1

        return Commit(
            repository_id=self.id,
            hash=hash,
            revision=revision,
            identifier=identifier if include_identifier else None,
            branch_point=branch_point,
            branch=branch,
            timestamp=timestamp,
            order=order,
            author=Contributor.from_scm_log(log.stdout.splitlines()[1], self.contributors),
            message=logcontent if include_log else None,
        )

    def _args_from_content(self, content, include_log=True):
        author = None
        timestamp = None

        for line in content.splitlines()[:4]:
            split = line.split(': ')
            if split[0] == 'Author':
                author = Contributor.from_scm_log(line.lstrip(), self.contributors)
            elif split[0] == 'CommitDate':
                tz_diff = line.split(' ')[-1]
                date = datetime.strptime(split[1].lstrip()[:-len(tz_diff)], '%a %b %d %H:%M:%S %Y ')
                date += timedelta(
                    hours=int(tz_diff[1:3]),
                    minutes=int(tz_diff[3:5]),
                ) * (1 if tz_diff[0] == '-' else -1)
                timestamp = int(calendar.timegm(date.timetuple())) - time.timezone

        message = ''
        for line in content.splitlines()[5:]:
            message += line[4:] + '\n'
        matches = self.GIT_SVN_REVISION.findall(message)

        return dict(
            revision=int(matches[-1].split('@')[0]) if matches else None,
            author=author,
            timestamp=timestamp,
            message=message.rstrip() if include_log else None,
        )

    def commits(self, begin=None, end=None, include_log=True, include_identifier=True):
        begin, end = self._commit_range(begin=begin, end=end, include_identifier=include_identifier)

        try:
            log = None
            log = subprocess.Popen(
                [self.executable(), 'log', '--format=fuller', '{}...{}'.format(end.hash, begin.hash)],
                cwd=self.root_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                **(dict(encoding='utf-8') if sys.version_info > (3, 0) else dict())
            )
            if log.poll():
                raise self.Exception("Failed to construct history for '{}'".format(end.branch))

            line = log.stdout.readline()
            previous = [end]
            while line:
                if not line.startswith('commit '):
                    raise OSError('Failed to parse `git log` format')
                branch_point = previous[0].branch_point
                identifier = previous[0].identifier
                hash = line.split(' ')[-1].rstrip()
                if hash != previous[0].hash:
                    identifier -= 1

                if not identifier:
                    identifier = branch_point
                    branch_point = None

                content = ''
                line = log.stdout.readline()
                while line and not line.startswith('commit '):
                    content += line
                    line = log.stdout.readline()

                commit = Commit(
                    repository_id=self.id,
                    hash=hash,
                    branch=end.branch if identifier and branch_point else self.default_branch,
                    identifier=identifier if include_identifier else None,
                    branch_point=branch_point if include_identifier else None,
                    order=0,
                    **self._args_from_content(content, include_log=include_log)
                )

                # Ensure that we don't duplicate the first and last commits
                if commit.hash == previous[0].hash:
                    previous[0] = commit

                # If we share a timestamp with the previous commit, that means that this commit has an order
                # less than the set of commits cached in previous
                elif commit.timestamp == previous[0].timestamp:
                    for cached in previous:
                        cached.order += 1
                    previous.append(commit)

                # If we don't share a timestamp with the previous set of commits, we should return all commits
                # cached in previous.
                else:
                    for cached in previous:
                        yield cached
                    previous = [commit]

            for cached in previous:
                cached.order += begin.order
                yield cached
        finally:
            if log:
                log.kill()

    def find(self, argument, include_log=True, include_identifier=True):
        if not isinstance(argument, six.string_types):
            raise ValueError("Expected 'argument' to be a string, not '{}'".format(type(argument)))

        # Map any candidate default branch to the one used by this repository
        if argument in self.DEFAULT_BRANCHES:
            argument = self.default_branch

        # See if the argument the user specified is a recognized commit format
        parsed_commit = Commit.parse(argument, do_assert=False)
        if parsed_commit:
            if parsed_commit.branch in self.DEFAULT_BRANCHES:
                parsed_commit.branch = self.default_branch

            return self.commit(
                hash=parsed_commit.hash,
                revision=parsed_commit.revision,
                identifier=parsed_commit.identifier,
                branch=parsed_commit.branch,
                include_log=include_log,
                include_identifier=include_identifier,
            )

        # The argument isn't a recognized commit format, hopefully it is a valid git ref of some form
        output = run(
            [self.executable(), 'rev-parse', argument],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )
        if output.returncode:
            raise ValueError("'{}' is not an argument recognized by git".format(argument))
        return self.commit(hash=output.stdout.rstrip(), include_log=include_log, include_identifier=include_identifier)

    def checkout(self, argument):
        if not isinstance(argument, six.string_types):
            raise ValueError("Expected 'argument' to be a string, not '{}'".format(type(argument)))

        if log.level > logging.WARNING:
            log_arg = ['-q']
        elif log.level < logging.WARNING:
            log_arg = ['--progress']
        else:
            log_arg = []

        parsed_commit = Commit.parse(argument, do_assert=False)
        if parsed_commit:
            commit = self.commit(
                hash=parsed_commit.hash,
                revision=parsed_commit.revision,
                identifier=parsed_commit.identifier,
                branch=parsed_commit.branch,
            )
            return None if run(
                [self.executable(), 'checkout'] + [commit.hash] + log_arg,
                cwd=self.root_path,
            ).returncode else commit

        return None if run(
            [self.executable(), 'checkout'] + [argument] + log_arg,
            cwd=self.root_path,
        ).returncode else self.commit()

    def pull(self):
        commit = self.commit()
        code = run([self.executable(), 'pull'], cwd=self.root_path).returncode
        if not code and self.is_svn:
            return run([
                self.executable(), 'svn', 'fetch', '--log-window-size=5000', '-r', '{}:HEAD'.format(commit.revision),
            ], cwd=self.root_path).returncode
        return code

    def clean(self):
        return run([
            self.executable(), 'reset', 'HEAD', '--hard',
        ], cwd=self.root_path).returncode
