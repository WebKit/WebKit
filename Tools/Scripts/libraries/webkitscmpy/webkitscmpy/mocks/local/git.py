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
from webkitscmpy import local, Commit, Contributor


class Git(mocks.Subprocess):

    def __init__(
        self, path='/.invalid-git',
        remote=None, branches=None, tags=None,
        detached=None, default_branch='main',
        git_svn=False,
    ):
        self.path = path
        self.default_branch = default_branch
        self.remote = remote or 'git@webkit.org:/mock/{}'.format(os.path.basename(path))
        self.detached = detached or False

        self.branches = branches or []
        self.tags = tags or {}

        try:
            self.executable = local.Git.executable()
        except (OSError, AssertionError):
            self.executable = '/usr/bin/git'

        # Provide a reasonable set of commits to test against
        contributor = Contributor(name='Jonathan Bedard', emails=['jbedard@apple.com'])
        self.commits = {
            default_branch: [
                Commit(
                    identifier='1@{}'.format(default_branch),
                    hash='9b8311f25a77ba14923d9d5a6532103f54abefcb',
                    revision=1 if git_svn else None,
                    author=contributor,
                    timestamp=1601660000,
                    message='1st commit\n',
                ), Commit(
                    identifier='2@{}'.format(default_branch),
                    hash='fff83bb2d9171b4d9196e977eb0508fd57e7a08d',
                    revision=2 if git_svn else None,
                    author=contributor,
                    timestamp=1601661000,
                    message='2nd commit\n',
                ), Commit(
                    identifier='3@{}'.format(default_branch),
                    hash='1abe25b443e985f93b90d830e4a7e3731336af4d',
                    revision=4 if git_svn else None,
                    author=contributor,
                    timestamp=1601663000,
                    message='4th commit\n',
                ), Commit(
                    identifier='4@{}'.format(default_branch),
                    hash='bae5d1e90999d4f916a8a15810ccfa43f37a2fd6',
                    revision=6 if git_svn else None,
                    author=contributor,
                    timestamp=1601665000,
                    message='6th commit\n',
                ),
            ], 'branch-a': [
                Commit(
                    identifier='2.1@branch-a',
                    hash='a30ce8494bf1ac2807a69844f726be4a9843ca55',
                    revision=3 if git_svn else None,
                    author=contributor,
                    timestamp=1601662000,
                    message='3rd commit\n',
                ), Commit(
                    identifier='2.2@branch-a',
                    hash='621652add7fc416099bd2063366cc38ff61afe36',
                    revision=7 if git_svn else None,
                    author=contributor,
                    timestamp=1601666000,
                    message='7th commit\n',
                ),
            ],
        }
        self.commits['branch-b'] = [
            self.commits['branch-a'][0], Commit(
                identifier='2.2@branch-b',
                hash='3cd32e352410565bb543821fbf856a6d3caad1c4',
                revision=5 if git_svn else None,
                author=contributor,
                timestamp=1601664000,
                message='5th commit\n',
            ), Commit(
                identifier='2.3@branch-b',
                hash='790725a6d79e28db2ecdde29548d2262c0bd059d',
                revision=8 if git_svn else None,
                author=contributor,
                timestamp=1601667000,
                message='8th commit\n',
            ),
        ]
        self.head = self.commits[self.default_branch][3]

        self.tags['tag-1'] = self.commits['branch-a'][-1]
        self.tags['tag-2'] = self.commits['branch-b'][-1]

        if git_svn:
            git_svn_routes = [
                mocks.Subprocess.Route(
                    self.executable, 'svn', 'find-rev', re.compile(r'r\d+'),
                    cwd=self.path,
                    generator=lambda *args, **kwargs:
                        mocks.ProcessCompletion(
                            returncode=0,
                            stdout=getattr(self.find(args[3][1:]), 'hash', '\n'),
                        )
                ), mocks.Subprocess.Route(
                    self.executable, 'svn', 'info',
                    cwd=self.path,
                    generator=lambda *args, **kwargs:
                        mocks.ProcessCompletion(
                            returncode=0,
                            stdout=
                                'Path: .\n'
                                'URL: {remote}/{branch}\n'
                                'Repository Root: {remote}\n'
                                'Revision: {revision}\n'
                                'Node Kind: directory\n'
                                'Schedule: normal\n'
                                'Last Changed Author: {author}\n'
                                'Last Changed Rev: {revision}\n'
                                'Last Changed Date: {date}'.format(
                                    path=self.path,
                                    remote=self.remote,
                                    branch=self.head.branch,
                                    revision=self.head.revision,
                                    author=self.head.author.email,
                                    date=datetime.fromtimestamp(self.head.timestamp).strftime('%Y-%m-%d %H:%M:%S'),
                                ),
                        ),
                ),
            ]

        else:
            git_svn_routes = [mocks.Subprocess.Route(
                self.executable, 'svn',
                cwd=self.path,
                completion=mocks.ProcessCompletion(returncode=1, elapsed=2),
            )]

        super(Git, self).__init__(
            mocks.Subprocess.Route(
                '/usr/bin/which', 'git',
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.executable),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'status',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='''HEAD detached at b8a315ed93c
nothing to commit, working tree clean
''' if self.detached else ''''On branch {branch}
Your branch is up to date with 'origin/{branch}'.

nothing to commit, working tree clean
'''.format(branch=self.branch),
                    ),
            ),
            mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--show-toplevel',
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.path),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--abbrev-ref', 'HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format('HEAD' if self.detached else self.branch),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'remote', 'get-url', '.*',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.remote),
                ) if args[3] == 'origin' else mocks.ProcessCompletion(
                    returncode=128,
                    stderr="fatal: No such remote '{}'\n".format(args[3]),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-a',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(['* ' + self.branch] + list(({default_branch} | set(self.commits.keys()) | set(self.branches)) - {self.branch}))) +
                           '\nremotes/origin/HEAD -> origin/{}\n'.format(default_branch),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'tag',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(self.tags.keys())) + '\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '--abbrev-ref', 'origin/HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='origin/{}\n'.format(default_branch),
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-parse', '.*',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.find(args[2]).hash),
                ) if self.find(args[2]) else mocks.ProcessCompletion(returncode=128)
            ), mocks.Subprocess.Route(
                self.executable, 'log', re.compile(r'.+'), '-1',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout=
                        'commit {hash} (HEAD -> {branch}, origin/{branch}, origin/HEAD)\n'
                        'Author: {author} <{email}>\n'
                        'Date:   {date}\n'
                        '\n{log}'.format(
                            hash=self.find(args[2]).hash,
                            branch=self.branch,
                            author=self.find(args[2]).author.name,
                            email=self.find(args[2]).author.email,
                            date=datetime.fromtimestamp(self.find(args[2]).timestamp).strftime('%a %b %d %H:%M:%S %Y'),
                            log='\n'.join([
                                    ('    ' + line) if line else '' for line in self.find(args[2]).message.splitlines()
                                ] + ['git-svn-id: https://svn.{}repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                    self.find(args[2]).revision,
                                )] if git_svn else [],
                            )
                        ),
                ) if self.find(args[2]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-list', '--count', '--no-merges', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.count(args[4]))
                ) if self.find(args[4]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'show', '-s', '--format=%ct', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(
                        self.find(args[4]).timestamp,
                    )
                ) if self.find(args[4]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-a', '--contains', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(self.branches_on(args[4]))) + '\n',
                ) if self.find(args[4]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[2]) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable,
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=1,
                    stderr='usage: git [--version] [--help]...\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable,
                completion=mocks.ProcessCompletion(
                    returncode=128,
                    stderr='fatal: not a git repository (or any parent up to mount point)\nStopping at filesystem boundary (GIT_DISCOVERY_ACROSS_FILESYSTEM not set).\n',
                ),
            ), *git_svn_routes
        )

    @property
    def branch(self):
        return self.head.branch

    def find(self, something):
        if '~' in something:
            split = something.split('~')
            if len(split) == 2 and Commit.NUMBER_RE.match(split[1]):
                found = self.find(split[0])
                difference = int(split[1])
                if difference < found.identifier:
                    return self.commits[found.branch][found.identifier - difference - 1]
                difference -= found.identifier
                if difference < found.branch_point:
                    return self.commits[self.default_branch][found.branch_point - difference - 1]
                return None

        if something == 'HEAD':
            return self.head
        if something in self.commits.keys():
            return self.commits[something][-1]
        if something in self.tags.keys():
            return self.tags[something]

        something = str(something)
        if '..' in something:
            something = something.split('..')[1]
        for branch, commits in self.commits.items():
            if branch == something:
                return commits[-1]
            for commit in commits:
                if something == str(commit.revision):
                    return commit
                if len(something) > 4 and commit.hash.startswith(str(something)):
                    return commit
        return None

    def count(self, something):
        match = self.find(something)
        if '..' in something or not match.branch_point:
            return match.identifier
        return match.branch_point + match.identifier

    def branches_on(self, hash):
        result = set()
        found_identifier = 0
        for branch, commits in self.commits.items():

            for commit in commits:
                if commit.hash.startswith(hash):
                    found_identifier = max(commit.identifier, found_identifier)
                    result.add(branch)

        if self.default_branch in result:
            for branch, commits in self.commits.items():
                if commits[0].branch_point and commits[0].branch_point >= found_identifier:
                    result.add(branch)
        return result

    def checkout(self, something):
        commit = self.find(something)
        if commit:
            self.head = commit
            self.detached = something not in self.branches
        return True if commit else False
