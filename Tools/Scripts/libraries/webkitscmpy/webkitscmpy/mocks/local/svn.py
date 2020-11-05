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

import os
import re

from datetime import datetime

from webkitcorepy import mocks
from webkitscmpy import local
from webkitscmpy import Commit, Contributor


class Svn(mocks.Subprocess):
    BRANCH_RE = re.compile(r'\^/(branches/)?(?P<branch>.+)')
    UTC_OFFSET = '-0100'

    @classmethod
    def log_line(cls, commit):
        email = '(no author)'
        if commit.author:
            email = commit.author.email or commit.author.name
        return 'r{revision} | {email} | {date}'.format(
            revision=commit.revision,
            email=email,
            date=datetime.fromtimestamp(commit.timestamp).strftime('%Y-%m-%d %H:%M:%S {} (%a, %d %b %Y)'.format(cls.UTC_OFFSET)),
        )

    def __init__(self, path='/.invalid-svn', remote=None, branches=None):
        self.path = path
        self.remote = remote or 'https://svn.mock.org/repository/{}'.format(os.path.basename(path))

        self.branches = branches or []
        self.connected = True

        try:
            self.executable = local.Svn.executable()
        except (OSError, AssertionError):
            self.executable = '/usr/bin/svn'

        # Provide a reasonable set of commits to test against
        contributor = Contributor(name='Jonathan Bedard', emails=['jbedard@apple.com'])
        self.commits = {
            'trunk': [
                Commit(
                    identifier='1@trunk',
                    revision=1,
                    author=contributor,
                    timestamp=1601660100,
                    message='1st commit\n',
                ), Commit(
                    identifier='2@trunk',
                    revision=2,
                    author=contributor,
                    timestamp=1601661100,
                    message='2nd commit\n',
                ), Commit(
                    identifier='3@trunk',
                    revision=4,
                    author=contributor,
                    timestamp=1601663100,
                    message='4th commit\n',
                ), Commit(
                    identifier='4@trunk',
                    revision=6,
                    author=contributor,
                    timestamp=1601665100,
                    message='6th commit\n',
                ),
            ], 'branch-a': [
                Commit(
                    identifier='2.1@branch-a',
                    revision=3,
                    author=contributor,
                    timestamp=1601662100,
                    message='3rd commit\n',
                ), Commit(
                    identifier='2.2@branch-a',
                    revision=7,
                    author=contributor,
                    timestamp=1601666100,
                    message='7th commit\n',
                ),
            ],
        }
        self.commits['branch-b'] = [
            self.commits['branch-a'][0], Commit(
                identifier='2.2@branch-b',
                revision=5,
                author=contributor,
                timestamp=1601664100,
                message='5th commit\n',
            ), Commit(
                identifier='2.3@branch-b',
                revision=8,
                author=contributor,
                timestamp=1601667100,
                message='8th commit\n',
            ),
        ]

        self.commits['tags/tag-1'] = [
            self.commits['branch-a'][0],
            self.commits['branch-a'][1], Commit(
                identifier='2.3@tags/tag-1',
                revision=9,
                author=contributor,
                timestamp=1601668100,
                message='9th commit\n',
            ),
        ]
        self.commits['tags/tag-2'] = [
            self.commits['branch-b'][0],
            self.commits['branch-b'][1],
            self.commits['branch-b'][2], Commit(
                identifier='2.4@tags/tag-2',
                revision=10,
                author=contributor,
                timestamp=1601669100,
                message='10th commit\n',
            ),
        ]
        self.head = self.commits['trunk'][3]

        super(Svn, self).__init__(
            mocks.Subprocess.Route(
                '/usr/bin/which', 'svn',
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.executable),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'info',
                cwd=self.path,
                generator=lambda *args, **kwargs: self._info(cwd=kwargs.get('cwd', ''))
            ), mocks.Subprocess.Route(
                self.executable, 'info', self.BRANCH_RE,
                cwd=self.path,
                generator=lambda *args, **kwargs: self._info(branch=self.BRANCH_RE.match(args[2]).group('branch'), cwd=kwargs.get('cwd', ''))
            ), mocks.Subprocess.Route(
                self.executable, 'list', '^/branches',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='/\n'.join(sorted(set(self.branches) | set(self.commits.keys()) - {'trunk'} - self.tags)) + '/\n',
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
                        '    M /{branch}/file.cpp\n'.format(
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
                self.executable, 'up', '-r', re.compile(r'\d+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.up(args[3]) else mocks.ProcessCompletion(returncode=1)
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
                date=datetime.fromtimestamp(commit.timestamp).strftime('%Y-%m-%d %H:%M:%S {} (%a, %d %b %Y)'.format(self.UTC_OFFSET)),
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
