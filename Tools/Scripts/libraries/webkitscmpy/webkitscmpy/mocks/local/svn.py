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

import json
import os
import re

from datetime import datetime

from webkitcorepy import mocks
from webkitscmpy import local
from webkitscmpy import Commit, Contributor


class Svn(mocks.Subprocess):
    BRANCH_RE = re.compile(r'\^/(branches/)?(?P<branch>.+)')

    def log_line(self, commit):
        email = '(no author)'
        if commit.author:
            email = commit.author.email or commit.author.name
        return 'r{revision} | {email} | {date}'.format(
            revision=commit.revision,
            email=email,
            date=datetime.utcfromtimestamp(commit.timestamp).strftime('%Y-%m-%d %H:%M:%S {} (%a, %d %b %Y)'.format(self.utc_offset)),
        )

    def __init__(self, path='/.invalid-svn', datafile=None, remote=None, utc_offset=None):
        self.path = path
        self.remote = remote or 'https://svn.mock.org/repository/{}'.format(os.path.basename(path))
        self.utc_offset = utc_offset or '0000'

        self.connected = True

        try:
            self.executable = local.Svn.executable()
        except (OSError, AssertionError):
            self.executable = '/usr/bin/svn'

        with open(datafile or os.path.join(os.path.dirname(os.path.dirname(__file__)), 'svn-repo.json')) as file:
            self.commits = json.load(file)
        for key, commits in self.commits.items():
            self.commits[key] = [Commit(**kwargs) for kwargs in commits]

        self.head = self.commits['trunk'][-1]

        super(Svn, self).__init__(
            mocks.Subprocess.Route(
                self.executable, 'info', self.BRANCH_RE,
                cwd=self.path,
                generator=lambda *args, **kwargs: self._info(branch=self.BRANCH_RE.match(args[2]).group('branch'), cwd=kwargs.get('cwd', ''))
            ), mocks.Subprocess.Route(
                self.executable, 'info', '-r', re.compile(r'\d+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self._info(revision=int(args[3]), cwd=kwargs.get('cwd', ''))
            ), mocks.Subprocess.Route(
                self.executable, 'info',
                cwd=self.path,
                generator=lambda *args, **kwargs: self._info(cwd=kwargs.get('cwd', ''))
            ), mocks.Subprocess.Route(
                self.executable, 'list', '^/branches',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='/\n'.join(sorted(set(self.commits.keys()) - {'trunk'} - self.tags)) + '/\n',
                ) if self.connected else mocks.ProcessCompletion(returncode=1),
            ), mocks.Subprocess.Route(
                self.executable, 'list', '^/tags',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='/\n'.join([tag[len('tags/'):] for tag in sorted(self.tags)]) + '/\n',
                ) if self.connected else mocks.ProcessCompletion(returncode=1),
            ), mocks.Subprocess.Route(
                self.executable, 'log', '-v', '-q', self.remote, '-r', re.compile(r'\d+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout=
                        '{line}\n'
                        'Changed paths:\n'
                        '    M /{branch}/ChangeLog\n'
                        '    M /{branch}/file.cpp\n'
                        '    D /{branch}/deleted.cpp\n'
                        '    A /{branch}/added.cpp\n'.format(
                            line=self.log_line(self.find(revision=args[6])),
                            branch=self.find(revision=args[6]).branch if self.find(revision=args[6]).branch.split('/')[0] in ['trunk', 'tags'] else 'branches/{}'.format(self.find(revision=args[6]).branch)
                        ),
                ) if self.connected and self.find(revision=args[6]) else mocks.ProcessCompletion(returncode=1),
            ), mocks.Subprocess.Route(
                self.executable, 'log', '-q', self.BRANCH_RE,
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n{}\n'.format('-' * 72).join([
                        self.log_line(commit) for commit in self._commits_for(branch=self.BRANCH_RE.match(args[3]).group('branch'))
                    ]),
                ) if self.connected and self.BRANCH_RE.match(args[3]).group('branch') in self.commits else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'log', '-l', '1', '-r', re.compile(r'\d+'), self.BRANCH_RE,
                cwd=self.path,
                generator=lambda *args, **kwargs: self._log_for(
                    branch=self.BRANCH_RE.match(args[6]).group('branch'),
                    revision=args[5],
                ) if self.connected else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'log', '-r', re.compile(r'\d+:\d+'), self.BRANCH_RE,
                cwd=self.path,
                generator=lambda *args, **kwargs: self._log_range(
                    branch=self.BRANCH_RE.match(args[4]).group('branch'),
                    end=int(args[3].split(':')[0]),
                    begin=int(args[3].split(':')[-1]),
                ) if self.connected else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'log',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    self._log_range(
                        branch='trunk',
                        begin=self.commits['trunk'][0].revision,
                        end=self.commits['trunk'][-1].revision,
                    ) if self.connected else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'up', '-r', re.compile(r'\d+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.up(args[3]) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'up',
                cwd=self.path,
                completion=mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable, 'revert', '-R',
                cwd=self.path,
                completion=mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable,
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=1,
                    stderr="Type 'svn help' for usage.\n",
                )
            ), mocks.Subprocess.Route(
                self.executable,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=1,
                    stderr="svn: E155007: '{}' is not a working copy\n".format(kwargs.get('cwd')),
                )
            ),
        )

    def __enter__(self):
        from mock import patch

        # TODO: Use shutil directly when Python 2.7 is removed
        from whichcraft import which
        self.patches.append(patch('whichcraft.which', lambda cmd: dict(svn=self.executable).get(cmd, which(cmd))))
        return super(Svn, self).__enter__()

    @property
    def branch(self):
        return self.head.branch

    @property
    def tags(self):
        return set(branch for branch in self.commits.keys() if branch.startswith('tags'))

    def _info(self, branch=None, revision=None, cwd=''):
        commit = self.find(branch=branch, revision=revision)
        if not commit:
            return mocks.ProcessCompletion(returncode=1, stderr='svn: E160006: No such revision {}\n'.format(revision))

        return mocks.ProcessCompletion(
            returncode=0,
            stdout=
            'Path: .\n'
            'Working Copy Root Path: {path}\n'
            'URL: {remote}/{branch}{directory}\n'
            'Relative URL: ^/{branch}{directory}\n'
            'Repository Root: {remote}\n'
            'Revision: {revision}\n'
            'NodeKind: directory\n'
            'Schedule: normal\n'
            'Last Changed Author: {author}\n'
            'Last Changed Rev: {revision}\n'
            'Last Changed Date: {date}'.format(
                directory=cwd[len(self.path):],
                path=self.path,
                remote=self.remote,
                branch=self.branch,
                revision=commit.revision,
                author=commit.author.email,
                date=datetime.utcfromtimestamp(commit.timestamp).strftime('%Y-%m-%d %H:%M:%S {} (%a, %d %b %Y)'.format(self.utc_offset)),
            ),
        )

    def _commits_for(self, branch='trunk'):
        if branch not in self.commits:
            return []
        result = [commit for commit in reversed(self.commits[branch])]
        if self.commits[branch][0].branch_point:
            result += [commit for commit in reversed(self.commits['trunk'][:self.commits[branch][0].branch_point])]
        return result

    def _log_for(self, branch=None, revision=None):
        commit = self.find(branch=branch, revision=revision)
        if not commit:
            return mocks.ProcessCompletion(returncode=1)
        return mocks.ProcessCompletion(
            returncode=0,
            stdout=
                '------------------------------------------------------------------------\n'
                '{line} | {lines} lines\n\n'
                '{log}\n'.format(
                    line=self.log_line(commit),
                    lines=len(commit.message.splitlines()),
                    log=commit.message
                ),
        )

    def _log_range(self, branch=None, end=None, begin=None):
        if end < begin:
            return mocks.ProcessCompletion(returncode=1)

        output = ''
        previous = None
        for b in [branch, 'trunk']:
            for candidate in reversed(self.commits.get(b, [])):
                if candidate.revision > end or candidate.revision < begin:
                    continue
                if previous and previous.revision <= candidate.revision:
                    continue
                previous = candidate
                output += ('------------------------------------------------------------------------\n'
                    '{line} | {lines} lines\n\n'
                    '{log}\n').format(
                        line=self.log_line(candidate),
                        lines=len(candidate.message.splitlines()),
                        log=candidate.message
                )
        return mocks.ProcessCompletion(returncode=0, stdout=output)

    def find(self, branch=None, revision=None):
        if not branch and not revision:
            return self.head
        for candidate in [branch] if branch else sorted(self.commits.keys()):
            if not revision:
                if self.head.branch == candidate:
                    return self.head
                return self.commits[candidate][-1]
            for commit in self.commits[candidate]:
                if str(commit.revision) == str(revision):
                    return commit
        return None

    def up(self, revision):
        commit = self.find(revision=revision)
        if commit:
            self.head = commit
        return True if commit else False
