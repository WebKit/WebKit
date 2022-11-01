# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
import json
import re
import six
import subprocess
import sys
import time

from collections import defaultdict

from webkitcorepy import run, decorators, NestedFuzzyDict, string_utils
from webkitscmpy.local import Scm
from webkitscmpy import remote, Commit, Contributor, log


class Git(Scm):
    class Cache(object):
        def __init__(self, repo, guranteed_for=10):
            self.repo = repo
            self._ordered_commits = {}
            self._hash_to_identifiers = NestedFuzzyDict(primary_size=6)
            self._ordered_revisions = {}
            self._revisions_to_identifiers = {}
            self._last_populated = {}
            self._guranteed_for = guranteed_for

            self.load()

        def load(self):
            if not os.path.exists(self.path):
                return

            try:
                with open(self.path) as file:
                    content = json.load(file)
                    self._ordered_commits = content['hashes']
                    self._ordered_revisions = content['revisions']

                self._fill(self.repo.default_branch)
                for branch in self._ordered_commits.keys():
                    if branch == self.repo.default_branch:
                        continue
                    self._fill(branch)
            except BaseException:
                pass

        @property
        def path(self):
            return os.path.join(self.repo.common_directory, 'identifiers.json')

        def _fill(self, branch):
            default_branch = self.repo.default_branch
            if branch == default_branch:
                branch_point = None
            else:
                branch_point = int(self._hash_to_identifiers[self._ordered_commits[branch][0]].split('@')[0])

            index = len(self._ordered_commits[branch]) - 1
            while index:
                identifier = self._hash_to_identifiers.get(self._ordered_commits[branch][index])

                if identifier:
                    id_branch = identifier.split('@')[-1]
                    if branch in (default_branch, id_branch):
                        break
                    if branch != self.repo.prioritize_branches((branch, id_branch)):
                        break

                identifier = '{}@{}'.format('{}.{}'.format(branch_point, index) if branch_point else index, branch)
                self._hash_to_identifiers[self._ordered_commits[branch][index]] = identifier
                if self._ordered_revisions[branch][index]:
                    self._revisions_to_identifiers[self._ordered_revisions[branch][index]] = identifier
                index -= 1

        def populate(self, branch=None, remote=None):
            branch = branch or self.repo.branch
            if not branch:
                return
            if self._last_populated.get(branch, 0) + self._guranteed_for > time.time():
                return
            default_branch = self.repo.default_branch
            is_default_branch = branch == default_branch
            if branch not in self._ordered_commits:
                self._ordered_commits[branch] = [''] if is_default_branch else []
                self._ordered_revisions[branch] = [0] if is_default_branch else []

            # If we aren't on the default branch, we will need the default branch to determine when
            # our  branch  intersects with the default branch.
            if not is_default_branch:
                self.populate(branch=self.repo.default_branch, remote=self.repo.default_remote)
                self.populate(branch=self.repo.default_branch)
            hashes = []
            revisions = []

            def _append(branch, hash, revision=None):
                hashes.append(hash)
                revisions.append(revision)
                identifier = self._hash_to_identifiers.get(hash, '')
                return identifier.endswith(default_branch) or identifier.endswith(branch)

            intersected = False
            log = None
            try:
                kwargs = dict()
                if sys.version_info >= (3, 6):
                    kwargs = dict(encoding='utf-8')
                self._last_populated[branch] = time.time()
                log = subprocess.Popen(
                    [self.repo.executable(), 'log', '{}/{}'.format(remote, branch) if remote else branch, '--no-decorate', '--date=unix', '--'],
                    cwd=self.repo.root_path,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    **kwargs
                )
                if log.poll():
                    raise self.repo.Exception("Failed to construct branch history for '{}'".format(branch))

                hash = None
                revision = None

                line = log.stdout.readline()
                while line:
                    if line.startswith('    git-svn-id: '):
                        match = self.repo.GIT_SVN_REVISION.match(line.lstrip())
                        if match:
                            revision = int(match.group('revision'))
                    if not line.startswith('commit '):
                        line = log.stdout.readline()
                        continue

                    if hash and _append(branch, hash, revision=revision):
                        hash = None
                        intersected = True
                        break

                    hash = line.split(' ')[1].rstrip()
                    revision = None
                    line = log.stdout.readline()

                if hash:
                    intersected = _append(branch, hash, revision=revision)

            finally:
                # If our `git log` operation failed, we can't count on the validity of our cache
                if log and log.returncode:
                    return
                if log and log.poll() is None:
                    log.kill()

            if not hashes or intersected and len(hashes) <= 1:
                return

            hashes.reverse()
            revisions.reverse()

            order = len(self._ordered_commits[branch]) - 1
            while order > 0:
                if hashes[0] == self._ordered_commits[branch][order]:
                    order -= 1
                    break
                order -= 1

            self._ordered_commits[branch] = self._ordered_commits[branch][:order + 1] + hashes
            self._ordered_revisions[branch] = self._ordered_revisions[branch][:order + 1] + revisions
            self._fill(branch)

            try:
                with open(self.path, 'w') as file:
                    json.dump(dict(
                        hashes=self._ordered_commits,
                        revisions=self._ordered_revisions,
                    ), file, indent=4)
            except (IOError, OSError):
                self.repo.log("Failed to write identifier cache to '{}'".format(self.path))

        def clear(self, branch):
            for d in [self._ordered_commits, self._ordered_revisions, self._last_populated]:
                if branch in d:
                    del d[branch]

            self._hash_to_identifiers = NestedFuzzyDict(primary_size=6)
            self._revisions_to_identifiers = {}

            if self.repo.default_branch not in self._ordered_commits:
                return
            self._fill(self.repo.default_branch)
            for branch in self._ordered_commits.keys():
                if branch == self.repo.default_branch:
                    continue
                self._fill(branch)

            try:
                with open(self.path, 'w') as file:
                    json.dump(dict(
                        hashes=self._ordered_commits,
                        revisions=self._ordered_revisions,
                    ), file, indent=4)
            except (IOError, OSError):
                self.repo.log("Failed to write identifier cache to '{}'".format(self.path))

        def to_hash(self, revision=None, identifier=None, populate=True, branch=None):
            if revision:
                identifier = self.to_identifier(revision=revision, populate=populate, branch=branch)
            parts = Commit._parse_identifier(identifier, do_assert=False)
            if not parts:
                return None

            _, b_count, branch = parts
            if b_count < 0:
                return None
            if branch in self._ordered_commits and len(self._ordered_commits[branch]) > b_count:
                return self._ordered_commits[branch][b_count]

            self.load()
            if branch in self._ordered_commits and len(self._ordered_commits[branch]) > b_count:
                return self._ordered_commits[branch][b_count]
            if populate:
                self.populate(branch=branch)
                return self.to_hash(identifier=identifier, populate=False)
            return None

        def to_revision(self, hash=None, identifier=None, populate=True, branch=None):
            if hash:
                identifier = self.to_identifier(hash=hash, populate=populate, branch=branch)
            parts = Commit._parse_identifier(identifier, do_assert=False)
            if not parts:
                return None

            _, b_count, branch = parts
            if b_count < 0:
                return None
            if branch in self._ordered_revisions and len(self._ordered_revisions[branch]) > b_count:
                return self._ordered_revisions[branch][b_count]

            self.load()
            if branch in self._ordered_revisions and len(self._ordered_revisions[branch]) > b_count:
                return self._ordered_revisions[branch][b_count]
            if populate:
                self.populate(branch=branch)
                return self.to_revision(identifier=identifier, populate=False)
            return None

        def to_identifier(self, hash=None, revision=None, populate=True, branch=None):
            revision = Commit._parse_revision(revision, do_assert=False)
            if revision:
                if revision in self._revisions_to_identifiers:
                    return self._revisions_to_identifiers[revision]

                self.load()
                if revision in self._revisions_to_identifiers:
                    return self._revisions_to_identifiers[revision]
                if populate:
                    self.populate(branch=branch)
                    return self.to_identifier(revision=revision, populate=False)
                return None

            hash = Commit._parse_hash(hash, do_assert=False)
            if hash:
                try:
                    candidate = self._hash_to_identifiers.get(hash)
                except KeyError:  # Means the hash wasn't specific enough
                    return None
                if candidate:
                    return candidate

                self.load()
                candidate = self._hash_to_identifiers.get(hash)
                if candidate:
                    return candidate
                if populate:
                    self.populate(branch=branch)
                    return self.to_identifier(hash=hash, populate=False)
            return None


    GIT_COMMIT = re.compile(r'commit (?P<hash>[0-9a-f]+)')
    SSH_REMOTE = re.compile('(ssh://)?git@(?P<host>[^:/]+)[:/](?P<path>.+).git')
    HTTP_REMOTE = re.compile(r'(?P<protocol>https?)://(?P<host>[^\/]+)/(?P<path>.+).git')
    REMOTE_BRANCH = re.compile(r'remotes\/(?P<remote>[^\/]+)\/(?P<branch>.+)')
    USER_REMOTE = re.compile(r'(?P<name>[^:]+):(?P<branch>.+)')
    GIT_CONFIG_EXTENSION = 'git_config_extension'
    PROJECT_CONFIG_OPTIONS = {
        'pull.rebase': ['true', 'false'],
        'webkitscmpy.pull-request': ['overwrite', 'append'],
        'webkitscmpy.history': ['when-user-owned', 'disabled', 'always', 'never'],
        'webkitscmpy.update-fork': ['true', 'false'],
        'webkitscmpy.auto-check': ['true', 'false'],
        'webkitscmpy.auto-create-commit': ['true', 'false'],
    }
    CONFIG_LOCATIONS = ['global', 'repository', 'project']

    @classmethod
    @decorators.Memoize()
    def executable(cls):
        return Scm.executable('git')

    @classmethod
    def is_checkout(cls, path):
        return run([cls.executable(), 'rev-parse', '--show-toplevel'], cwd=path, capture_output=True).returncode == 0

    @decorators.hybridmethod
    @decorators.Memoize()
    def config(context, location=None):
        args = [context.executable(), 'config', '-l']
        kwargs = dict(capture_output=True, encoding='utf-8')
        if location and location not in context.CONFIG_LOCATIONS:
            raise TypeError("'{}' is not a valid git config location".format(location))

        if isinstance(context, type) and location in ['repository', 'project']:
            raise TypeError("Cannot find '{}' git config without local checkout".format(location))

        if isinstance(context, type) or location == 'global':
            args += ['--global']
        else:
            kwargs['cwd'] = context.root_path
            if location == 'project':
                # Without a project config, use the library defaults
                if not context.metadata or not os.path.isfile(os.path.join(context.metadata, context.GIT_CONFIG_EXTENSION)):
                    return {key: values[0] for key, values in context.PROJECT_CONFIG_OPTIONS.items()}
                args += ['--file', os.path.join(context.metadata, context.GIT_CONFIG_EXTENSION)]

        command = run(args, **kwargs)
        if command.returncode:
            sys.stderr.write("Failed to run '{}'{}\n".format(
                ' '.join(args),
                '' if isinstance(context, type) else ' in {}'.format(context.root_path),
            ))
            return {}

        result = {}
        for line in command.stdout.splitlines():
            parts = line.split('=')
            result[parts[0]] = '='.join(parts[1:])

        # When no location argument is provided, combine the project config and the repository config
        if not isinstance(context, type) and not location:
            default_config_values = context.config(location='project')
        else:
            default_config_values = {key: values[0] for key, values in Git.PROJECT_CONFIG_OPTIONS.items()}

        for key, value in default_config_values.items():
            if not result.get(key):
                result[key] = value
            elif not Git.PROJECT_CONFIG_OPTIONS.get(key):
                continue
            elif result.get(key) not in Git.PROJECT_CONFIG_OPTIONS[key]:
                sys.stderr.write("'{}' is not a valid value for '{}', using '{}' instead\n".format(result[key], key, value))
                result[key] = value

        return result

    def __init__(self, path, dev_branches=None, prod_branches=None, contributors=None, id=None, cached=sys.version_info > (3, 0)):
        super(Git, self).__init__(path, dev_branches=dev_branches, prod_branches=prod_branches, contributors=contributors, id=id)
        self._branch = None
        self.cache = self.Cache(self) if self.root_path and cached else None
        self.default_remote = 'origin'
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
        config = os.path.join(self.common_directory, 'config')
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
    @decorators.Memoize()
    def common_directory(self):
        result = run([self.executable(), 'rev-parse', '--git-common-dir'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if result.returncode:
            return os.path.join(self.root_path, '.git')
        return os.path.abspath(os.path.join(self.root_path, result.stdout.rstrip()))

    @property
    @decorators.Memoize()
    def default_branch(self):
        for name in ['HEAD', 'main', 'master']:
            result = run([self.executable(), 'rev-parse', '--symbolic-full-name', 'refs/remotes/{}/{}'.format(self.default_remote, name)],
                         cwd=self.path, capture_output=True, encoding='utf-8')
            s = result.stdout.strip()
            if result.returncode == 0 and s:
                assert s.startswith('refs/remotes/{}/'.format(self.default_remote))
                return s[len('refs/remotes/{}/'.format(self.default_remote)):]

        candidates = self.branches
        if 'main' in candidates:
            return 'main'
        if 'master' in candidates:
            return 'master'
        return None

    @property
    def branch(self):
        if self._branch:
            return self._branch

        status = run([self.executable(), 'status'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if status.returncode:
            raise self.Exception('Failed to run `git status` for {}'.format(self.root_path))
        if status.stdout.splitlines()[0].startswith('HEAD detached at'):
            return None

        result = run([self.executable(), 'rev-parse', '--abbrev-ref', 'HEAD'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if result.returncode:
            raise self.Exception('Failed to retrieve branch for {}'.format(self.root_path))
        self._branch = result.stdout.rstrip()
        return self._branch

    @property
    def branches(self):
        return self.branches_for()

    def tags(self, remote=None):
        if not remote:
            tags = run([self.executable(), 'tag'], cwd=self.root_path, capture_output=True, encoding='utf-8')
            if tags.returncode:
                raise self.Exception('Failed to retrieve tag list for {}'.format(self.root_path))
            return tags.stdout.splitlines()

        tags = run([self.executable(), 'ls-remote', '--tags', remote], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if tags.returncode:
            raise self.Exception('Failed to retrieve tag list for {} in {}'.format(remote, self.root_path))
        result = []
        for line in tags.stdout.splitlines():
            if line.endswith('^{}'):
                continue
            result.append('/'.join(line.split('/')[2:]))
        return result

    def url(self, name=None, cached=None):
        return self.config(cached=cached).get('remote.{}.url'.format(name or self.default_remote))

    @decorators.Memoize()
    def remote(self, name=None):
        url = self.url(name=name)
        if not url:
            return None
        ssh_match = self.SSH_REMOTE.match(url)
        http_match = self.HTTP_REMOTE.match(url)
        if ssh_match:
            url = 'https://{}/{}'.format(ssh_match.group('host'), ssh_match.group('path'))
        elif http_match:
            url = '{}://{}/{}'.format(http_match.group('protocol'), http_match.group('host'), http_match.group('path'))

        try:
            return remote.Scm.from_url(url, contributors=self.contributors)
        except OSError:
            pass

        return None

    @decorators.Memoize()
    def source_remotes(self, cached=True, personal=False):
        candidates = [self.default_remote]
        config = self.config(cached=cached)
        for candidate in config.keys():
            if not candidate.startswith('webkitscmpy.remotes'):
                continue
            candidate = candidate.split('.')[-1]
            if candidate in candidates:
                continue
            if config.get('remote.{}.url'.format(candidate)):
                candidates.append(candidate)

        personal_remotes = []
        if personal:
            all_remotes = list(self.branches_for(remote=None))
            for candidate in ['fork'] + ['{}-fork'.format(og) for og in candidates]:
                if candidate in all_remotes:
                    personal_remotes.append(candidate)
            usernames = []
            rmt = self.remote()
            for candidate in all_remotes:
                if not candidate or '-' in candidate:
                    continue
                if candidate in candidates or candidate in personal_remotes:
                    continue
                if isinstance(rmt, remote.GitHub) and candidate == rmt.credentials(required=False)[0]:
                    continue
                usernames.append(candidate)
            for username in sorted(usernames):
                for candidate in [username] + ['{}-{}'.format(username, og) for og in candidates]:
                    if candidate in all_remotes:
                        personal_remotes.append(candidate)
        return candidates + personal_remotes

    def _commit_count(self, native_parameter):
        revision_count = run(
            [self.executable(), 'rev-list', '--count', '--no-merges', native_parameter],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )
        if revision_count.returncode:
            raise self.Exception('Failed to retrieve revision count for {}'.format(native_parameter))
        return int(revision_count.stdout)

    @decorators.Memoize(cached=False)
    def branches_for(self, hash=None, remote=True):
        branch = run(
            [self.executable(), 'branch'] + (['--contains', hash, '-a'] if hash else ['-a']),
            cwd=self.root_path,
            capture_output=True,
            encoding='utf-8',
        )
        if branch.returncode:
            raise self.Exception('Failed to retrieve branch list for {}'.format(self.root_path))
        result = defaultdict(set)
        for branch in [branch.lstrip(' *') for branch in filter(lambda branch: '->' not in branch, branch.stdout.splitlines())]:
            match = self.REMOTE_BRANCH.match(branch)
            if match:
                result[match.group('remote')].add(match.group('branch'))
            else:
                result[None].add(branch)

        if remote is False:
            return sorted(result[None])
        if remote is True:
            return sorted(set.union(*result.values()))
        if isinstance(remote, string_utils.basestring):
            return sorted(result.get(remote, []))
        return result

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None, include_log=True, include_identifier=True):
        # Only git-svn checkouts can convert revisions to fully qualified commits, unless we happen to have a SVN cache built
        if revision:
            if hash:
                raise ValueError('Cannot define both hash and revision')
            hash = self.cache.to_hash(revision=revision, branch=branch) if self.cache else None

        # If we don't have an SVN cache built, and we're not git-svn, we can't reason about revisions
        if revision and not hash and not self.is_svn:
            raise self.Exception('This git checkout does not support SVN revisions')

        # Determine the hash for a provided Subversion revision
        elif revision and not hash:
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
        log_format = ['-1', '--no-decorate', '--date=unix'] if include_log else ['-1', '--no-decorate', '--date=unix', '--format=short']

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
                hash = self.cache.to_hash(identifier='{}@{}'.format(identifier, parsed_branch), branch=branch) if self.cache else None

            # If the cache managed to convert the identifier to a hash, we can skip some computation
            if hash:
                log = run(
                    [self.executable(), 'log', hash] + log_format + ['--'],
                    cwd=self.root_path,
                    capture_output=True,
                    encoding='utf-8',
                )
                if log.returncode:
                    raise self.Exception("Failed to retrieve commit information for '{}'".format(hash))

            # The cache has failed to convert the identifier, we need to do it the expensive way
            else:
                baseline = branch or 'HEAD'
                is_default = baseline == default_branch
                if baseline == 'HEAD':
                    is_default = default_branch in self.branches_for(baseline)

                if is_default and parsed_branch_point:
                    raise self.Exception('Cannot provide a branch point for a commit on the default branch')

                if is_default:
                    base_count = self._commit_count(baseline)
                else:
                    base_count = min(
                        self._commit_count('{}..{}'.format(default_branch, baseline)),
                        self._commit_count('{}/{}..{}'.format(self.default_remote, default_branch, baseline)),
                    )

                if identifier > base_count:
                    raise self.Exception('Identifier {} cannot be found on the specified branch in the current checkout'.format(identifier))
                log = run(
                    [self.executable(), 'log', '{}~{}'.format(branch or 'HEAD', base_count - identifier)] + log_format + ['--'],
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

            log = run([self.executable(), 'log', branch or tag] + log_format + ['--'], cwd=self.root_path, capture_output=True, encoding='utf-8')
            if log.returncode:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(branch or tag))

        # Determine the `git log` output for a given hash
        else:
            hash = Commit._parse_hash(hash, do_assert=True)
            log = run([self.executable(), 'log', hash or 'HEAD'] + log_format + ['--'], cwd=self.root_path, capture_output=True, encoding='utf-8')
            if log.returncode:
                raise self.Exception("Failed to retrieve commit information for '{}'".format(hash or 'HEAD'))

        # Fully define the hash from the `git log` output
        match = self.GIT_COMMIT.match(log.stdout.splitlines()[0])
        if not match:
            raise self.Exception('Invalid commit hash in git log')
        hash = match.group('hash')

        branch_point = None
        # A commit is often on multiple branches, the canonical branch is the one with the highest priority
        if branch != default_branch:
            branch = self.prioritize_branches(self.branches_for(hash), self.branch)

        if not identifier and include_identifier:
            cached_identifier = self.cache.to_identifier(hash=hash, branch=branch) if self.cache else None
            if cached_identifier:
                branch_point, identifier, branch = Commit._parse_identifier(cached_identifier)

        # Compute the identifier if the function did not receive one and we were asked to
        if not identifier and include_identifier:
            if branch == default_branch:
                identifier = self._commit_count(hash)
            else:
                identifier = min(
                    self._commit_count('{}..{}'.format(default_branch, hash)),
                    self._commit_count('{}/{}..{}'.format(self.default_remote, default_branch, hash)),
                )

        # Only compute the branch point we're on something other than the default branch
        if not branch_point and include_identifier and branch != default_branch:
            branch_point = self._commit_count(hash) - identifier
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
                timestamp = int(line.split(' ')[-1])

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

    def commits(self, begin=None, end=None, include_log=True, include_identifier=True, scopes=None):
        begin, end = self._commit_range(begin=begin, end=end, include_identifier=include_identifier)

        in_scope = set()
        for scope in scopes or []:
            ran = run(
                [self.executable(), 'log', '--pretty=%H', '{}..{}'.format(begin.hash, end.hash), '--', scope],
                cwd=self.root_path,
                capture_output=True,
                encoding='utf-8'
            )
            if not ran.returncode:
                for line in ran.stdout.splitlines():
                    in_scope.add(line)

        try:
            log = None
            log = subprocess.Popen(
                [self.executable(), 'log', '--format=fuller', '--no-decorate', '--date=unix',
                 '{}..{}'.format(begin.hash, end.hash), '--'],
                cwd=self.root_path,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                **(dict(encoding='utf-8') if sys.version_info > (3, 6) else dict())
            )
            if log.poll():
                raise self.Exception("Failed to construct history for '{}'".format(end.branch))

            line = log.stdout.readline()
            previous = [end]
            while line:
                if not line.startswith('commit '):
                    raise OSError('Failed to parse `git log` format')
                branch_point = previous[-1].branch_point
                identifier = previous[-1].identifier
                hash = line.split(' ')[-1].rstrip()
                if hash != previous[-1].hash:
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
                if commit.hash == previous[-1].hash:
                    previous[-1] = commit

                # If we share a timestamp with the previous commit, that means that this commit has an order
                # less than the set of commits cached in previous
                elif commit.timestamp == previous[-1].timestamp:
                    for cached in previous:
                        cached.order += 1
                    previous.append(commit)

                # If we don't share a timestamp with the previous set of commits, we should return all commits
                # cached in previous.
                else:
                    for cached in previous:
                        if scopes is None or cached.hash in in_scope:
                            yield cached
                    previous = [commit]

            for cached in previous:
                cached.order += begin.order
                if scopes is None or cached.hash in in_scope:
                    yield cached
        finally:
            if log and log.poll() is None:
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

    def _to_git_ref(self, argument):
        if not argument:
            return None
        if not isinstance(argument, six.string_types):
            raise ValueError("Expected 'argument' to be a string, not '{}'".format(type(argument)))
        parsed_commit = Commit.parse(argument, do_assert=False)
        try:
            if parsed_commit and not parsed_commit.hash:
                # It's possible that a branch can look like a hash (or revision), if this call fails,
                # we should return the unsanitized argument to our caller assumeing this is the case.
                return self.commit(
                    hash=parsed_commit.hash,
                    revision=parsed_commit.revision,
                    identifier=parsed_commit.identifier,
                    branch=parsed_commit.branch,
                    include_log=False,
                    include_identifier=False,
                ).hash
        except self.Exception:
            pass
        return argument

    def checkout(self, argument):
        self._branch = None

        if log.level > logging.WARNING:
            log_arg = ['-q']
        elif log.level < logging.WARNING:
            log_arg = ['--progress']
        else:
            log_arg = []

        match = self.USER_REMOTE.match(argument)
        rmt = self.remote()
        if match and isinstance(rmt, remote.GitHub):
            name = match.group('name')
            username = name.split('/')[0]
            repo_name = rmt.name if '/' not in name else name.split('/', 1)[-1]
            if username == rmt.credentials(required=False)[0]:
                name = 'fork' + repo_name[len(rmt.name):]
            else:
                name = username + repo_name[len(rmt.name):]

            if not self.url(name):
                url = self.url()
                if '://' in url:
                    rmt = '{}://{}/{}/{}.git'.format(url.split(':')[0], url.split('/')[2], username, repo_name)
                elif ':' in url:
                    rmt = '{}:{}/{}.git'.format(url.split(':')[0], username, repo_name)
                else:
                    sys.stderr.write("Failed to convert '{}' to '{}' remote\n".format(url, username))
                    return None
                if run(
                    [self.executable(), 'remote', 'add', name, rmt],
                    capture_output=True, cwd=self.root_path,
                ).returncode:
                    sys.stderr.write("Failed to add remote '{}' as '{}'\n".format(rmt, name))
                    return None
                self.config.clear()
            branch = match.group('branch')
            rc = run(
                [self.executable(), 'checkout'] + ['-B', branch, '{}/{}'.format(name, branch)] + log_arg,
                cwd=self.root_path,
            ).returncode
            if not rc:
                return self.commit()
            if rc == 128:
                run([self.executable(), 'fetch', name], cwd=self.root_path)
            return None if run(
                [self.executable(), 'checkout'] + ['-B', branch, '{}/{}'.format(name, branch)] + log_arg,
                cwd=self.root_path,
            ).returncode else self.commit()

        branch_remote = self.remote_for(argument)
        if branch_remote:
            result = run([
                self.executable(), 'branch',
                '--set-upstream-to' if argument in self.branches_for(remote=False) else '--track',
                argument, '{}/{}'.format(branch_remote, argument),
            ], capture_output=True, encoding='utf-8', cwd=self.root_path)
            if result.returncode:
                sys.stderr.write(result.stderr)

        return None if run(
            [self.executable(), 'checkout', self._to_git_ref(argument)] + log_arg + ['--'],
            cwd=self.root_path,
        ).returncode else self.commit()

    def rebase(self, target, base=None, head='HEAD', recommit=True):
        if head == self.default_branch or self.prod_branches.match(head):
            raise RuntimeError("Rebasing production branch '{}' banned in tooling!".format(head))

        target = self._to_git_ref(target)
        base = self._to_git_ref(base)
        head = self._to_git_ref(head)

        code = run([self.executable(), 'rebase', '--onto', target, base or target, head], cwd=self.root_path).returncode
        if self.cache:
            self.cache.clear(head if head != 'HEAD' else self.branch)
        if code or not recommit:
            return code
        return run([
            self.executable(), 'filter-branch', '-f',
            '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(
                date='{} -{}'.format(int(time.time()), self.gmtoffset())
            ), 'refs/heads/{}...{}'.format(target, head),
        ], cwd=self.root_path, env={'FILTER_BRANCH_SQUELCH_WARNING': '1'}, capture_output=True).returncode

    def fetch(self, branch, remote=None):
        return run(
            [self.executable(), 'fetch', remote or self.default_remote, '{}:{}'.format(branch, branch)],
            cwd=self.root_path,
        ).returncode

    def pull(self, rebase=None, branch=None, remote=None):
        remote = remote or self.default_remote
        commit = self.commit() if self.is_svn or branch else None

        code = 0
        if branch and self.branch != branch:
            code = self.fetch(branch=branch, remote=remote)
        if not code:
            command = [self.executable(), 'pull'] + ([remote, branch] if branch else [])
            if rebase is True:
                command += ['--rebase=True', '--autostash']
            elif rebase is False:
                command += ['--rebase=False']
            code = run(command, cwd=self.root_path).returncode
        if self.cache and rebase and branch != self.branch:
            self.cache.clear(self.branch)

        if not code and branch and rebase:
            result = run([self.executable(), 'rev-parse', 'HEAD'], cwd=self.root_path, capture_output=True, encoding='utf-8')
            if not result.returncode and result.stdout.rstrip() != commit.hash:
                code = run([
                    self.executable(),
                    'filter-branch', '-f',
                    '--env-filter', "GIT_AUTHOR_DATE='{date}';GIT_COMMITTER_DATE='{date}'".format(
                        date='{} -{}'.format(int(time.time()), self.gmtoffset())
                    ), 'HEAD...{}'.format('{}/{}'.format(remote, branch)),
                ], cwd=self.root_path, env={'FILTER_BRANCH_SQUELCH_WARNING': '1'}).returncode

        if not code and self.is_svn and commit.revision:
            return run([
                self.executable(), 'svn', 'fetch', '--log-window-size=5000', '-r', '{}:HEAD'.format(commit.revision),
            ], cwd=self.root_path).returncode
        return code

    def clean(self):
        return run([
            self.executable(), 'reset', 'HEAD', '--hard',
        ], cwd=self.root_path).returncode

    def modified(self, staged=None):
        if staged in [True, False]:
            command = run(
                [self.executable(), 'diff', '--name-only'] + (['--staged'] if staged else []),
                capture_output=True, encoding='utf-8', cwd=self.root_path,
            )
            if command.returncode:
                return []
            return command.stdout.splitlines()

        # When the user hasn't specified what they're looking for, we need to make some assumptions.
        # If all staged files are added, the user probably wants to include non-staged files too
        command = run(
            [self.executable(), 'diff', '--name-status', '--staged'],
            capture_output=True, encoding='utf-8', cwd=self.root_path,
        )
        if command.returncode:
            return []
        added = set()
        for line in command.stdout.splitlines():
            state, file = line.split(None, 1)
            if state == 'A':
                added.add(file)

        staged = self.modified(staged=True)
        if set(staged) - added:
            return staged
        return staged + self.modified(staged=False)

    def diff_lines(self, base, head=None):
        base = self._to_git_ref(base)
        head = self._to_git_ref(head)

        kwargs = dict()
        if sys.version_info >= (3, 6):
            kwargs = dict(encoding='utf-8')
        target = '{}..{}'.format(base, head) if head else base
        proc = subprocess.Popen(
            [self.executable(), 'diff', target],
            cwd=self.root_path,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            **kwargs
        )

        if proc.poll():
            sys.stderr.write("Failed to generate diff for '{}'\n".format(target))

        line = proc.stdout.readline()
        while line:
            yield line.rstrip()
            line = proc.stdout.readline()

    def files_changed(self, argument=None):
        if not argument:
            return self.modified()
        commit = self.find(argument, include_log=False, include_identifier=False)
        if not commit:
            raise ValueError("'{}' is not an argument recognized by git".format(argument))

        output = run(
            [self.executable(), 'show', commit.hash, '--pretty=', '--name-only'],
            cwd=self.root_path, capture_output=True, encoding='utf-8',
        )
        if output.returncode:
            raise ValueError("'{}' is not an argument recognized by git".format(argument))
        return output.stdout.rstrip().splitlines()

    def remote_for(self, argument):
        candidates = self.source_remotes()
        while candidates:
            if argument not in self.branches_for(remote=candidates[-1]):
                candidates.remove(candidates[-1])
                continue
            up_to_date = list(self.branches_for(hash='{}/{}'.format(candidates[-1], argument), remote=None).keys())
            for candidate in candidates:
                if candidate in up_to_date:
                    return candidate
            candidates.remove(candidates[-1])

        for remote in self.source_remotes(personal=True)[len(self.source_remotes()):]:
            if argument in self.branches_for(remote=remote):
                return remote
        return None

    def merge_base(self, ref_a, ref_b, include_log=True, include_identifier=True):
        a = self.find(ref_a, include_log=False, include_identifier=False)
        b = self.find(ref_b, include_log=False, include_identifier=False)
        if not a or not b:
            return None
        result = run(
            [self.executable(), 'merge-base', a.hash, b.hash],
            capture_output=True, encoding='utf-8', cwd=self.path,
        )
        if result.returncode:
            return None
        return self.commit(hash=result.stdout.rstrip(), include_log=include_log, include_identifier=include_identifier)
