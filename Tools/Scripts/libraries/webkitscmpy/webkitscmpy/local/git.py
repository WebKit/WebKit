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

import logging
import re
import six

from webkitcorepy import run, decorators, TimeoutExpired
from webkitscmpy.local import Scm
from webkitscmpy import Commit, Contributor, log


class Git(Scm):
    GIT_COMMIT = re.compile(r'commit (?P<hash>[0-9a-f]+)')
    GIT_SVN_REVISION = re.compile(r'git-svn-id: \S+:\/\/.+@(?P<revision>\d+) .+-.+-.+-.+')

    @classmethod
    @decorators.Memoize()
    def executable(cls):
        return Scm.executable('git')

    @classmethod
    def is_checkout(cls, path):
        return run([cls.executable(), 'rev-parse', '--show-toplevel'], cwd=path, capture_output=True).returncode == 0

    def __init__(self, path, dev_branches=None, prod_branches=None):
        super(Git, self).__init__(path, dev_branches=dev_branches, prod_branches=prod_branches)
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
        try:
            return run(
                [self.executable(), 'svn', 'find-rev', 'r1'],
                cwd=self.root_path,
                capture_output=True,
                encoding='utf-8',
                timeout=1,
            ).returncode == 0
        except TimeoutExpired:
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

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None):
        if revision and not self.is_svn:
            raise self.Exception('This git checkout does not support SVN revisions')
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
                [self.executable(), 'log', '{}~{}'.format(branch or 'HEAD', base_count - identifier), '-1'],
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

        elif branch or tag:
            if hash:
                raise ValueError('Cannot define both tag/branch and hash')
            if branch and tag:
                raise ValueError('Cannot define both tag and branch')

            log = run([self.executable(), 'log', branch or tag, '-1'], cwd=self.root_path, capture_output=True, encoding='utf-8')
            if log.returncode:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(branch or tag))

        else:
            hash = Commit._parse_hash(hash, do_assert=True)
            log = run([self.executable(), 'log', hash or 'HEAD', '-1'], cwd=self.root_path, capture_output=True, encoding='utf-8')
            if log.returncode:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(hash or 'HEAD'))

        match = self.GIT_COMMIT.match(log.stdout.splitlines()[0])
        if not match:
            raise self.Exception('Invalid commit hash in git log')
        hash = match.group('hash')

        branch = self.prioritize_branches(self._branches_for(hash))

        if not identifier:
            identifier = self._commit_count(hash if branch == default_branch else '{}..{}'.format(default_branch, hash))
        branch_point = None if branch == default_branch else self._commit_count(hash) - identifier
        if branch_point and parsed_branch_point and branch_point != parsed_branch_point:
            raise ValueError("Provided 'branch_point' does not match branch point of specified branch")

        match = self.GIT_SVN_REVISION.search(log.stdout)
        revision = int(match.group('revision')) if match else None

        commit_time = run(
            [self.executable(), 'show', '-s', '--format=%ct', hash],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )
        if commit_time.returncode:
            raise self.Exception('Failed to retrieve commit time for {}'.format(hash))

        return Commit(
            hash=hash,
            revision=revision,
            identifier=identifier,
            branch_point=branch_point,
            branch=branch,
            timestamp=int(commit_time.stdout.lstrip()),
            author=Contributor.from_scm_log(log.stdout.splitlines()[1]),
            message='\n'.join(line[4:] for line in log.stdout.splitlines()[4:]),
        )

    def find(self, argument):
        if not isinstance(argument, six.string_types):
            raise ValueError("Expected 'argument' to be a string, not '{}'".format(type(argument)))

        parsed_commit = Commit.parse(argument, do_assert=False)
        if parsed_commit:
            return self.commit(
                hash=parsed_commit.hash,
                revision=parsed_commit.revision,
                identifier=parsed_commit.identifier,
                branch=parsed_commit.branch,
            )

        output = run(
            [self.executable(), 'rev-parse', argument],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )
        if output.returncode:
            raise ValueError("'{}' is not an argument recognized by git".format(argument))
        return self.commit(hash=output.stdout.rstrip())

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
