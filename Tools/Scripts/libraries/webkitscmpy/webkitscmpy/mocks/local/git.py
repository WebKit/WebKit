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

import hashlib
import json
import os
import re
import time

from datetime import datetime
from mock import patch

from webkitcorepy import decorators, mocks, string_utils, OutputCapture, StringIO
from webkitscmpy import local, Commit, Contributor
from webkitscmpy.program.canonicalize.committer import main as committer_main
from webkitscmpy.program.canonicalize.message import main as message_main


class Git(mocks.Subprocess):
    # Parse a .git/config that looks like this
    # [core]
    #     repositoryformatversion = 0
    # [branch "main"]
    #     remote = origin
    # 	  merge = refs/heads/main
    RE_SINGLE_TOP = re.compile(r'^\[\s*(?P<key>\S+)\s*\]')
    RE_MULTI_TOP = re.compile(r'^\[\s*(?P<keya>\S+) "(?P<keyb>\S+)"\s*\]')
    RE_ELEMENT = re.compile(r'^\s+(?P<key>\S+)\s*=\s*(?P<value>.*\S+)')

    def __init__(
        self, path='/.invalid-git', datafile=None,
        remote=None, tags=None,
        detached=None, default_branch='main',
        git_svn=False, remotes=None,
    ):
        self.path = path
        self.default_branch = default_branch
        self.remote = remote or 'git@example.org:/mock/{}'.format(os.path.basename(path))
        self.detached = detached or False

        self.tags = tags or {}

        try:
            self.executable = local.Git.executable()
        except (OSError, AssertionError):
            self.executable = '/usr/bin/git'

        with open(datafile or os.path.join(os.path.dirname(os.path.dirname(__file__)), 'git-repo.json')) as file:
            self.commits = json.load(file)
        for key, commits in self.commits.items():
            commit_objs = []
            for kwargs in commits:
                changeFiles = None
                if 'changeFiles' in kwargs:
                    changeFiles = kwargs['changeFiles']
                    del kwargs['changeFiles']
                commit = Commit(**kwargs)
                if changeFiles:
                    setattr(commit, '__mock__changeFiles', changeFiles)
                commit_objs.append(commit)
            self.commits[key] = commit_objs
            if not git_svn:
                for commit in self.commits[key]:
                    commit.revision = None

        self.head = self.commits[self.default_branch][-1]
        self.remotes = {
            'origin/{}'.format(branch): commits[:] for branch, commits in self.commits.items()
            if not local.Git.DEV_BRANCHES.match(branch)
        }
        for name in (remotes or {}).keys():
            for branch, commits in self.commits.items():
                self.remotes['{}/{}'.format(name, branch)] = commits[:]

        self.tags = {}

        self.staged = {}
        self.modified = {}
        self.revert_message = None

        self.has_git_lfs = False

        # If the directory provided actually exists, populate it
        if self.path != '/' and os.path.isdir(self.path):
            if not os.path.isdir(os.path.join(self.path, '.git')):
                os.mkdir(os.path.join(self.path, '.git'))
            with open(os.path.join(self.path, '.git', 'config'), 'w') as config:
                config.write(
                    '[core]\n'
                    '\trepositoryformatversion = 0\n'
                    '\tfilemode = true\n'
                    '\tbare = false\n'
                    '\tlogallrefupdates = true\n'
                    '\tignorecase = true\n'
                    '\tprecomposeunicode = true\n'
                    '[pull]\n'
	                '\trebase = true\n'
                    '[remote "origin"]\n'
                    '\turl = {remote}\n'
                    '\tfetch = +refs/heads/*:refs/remotes/origin/*\n'
                    '[branch "{branch}"]\n'
                    '\tremote = origin\n'
                    '\tmerge = refs/heads/{branch}\n'.format(
                        remote=self.remote,
                        branch=self.default_branch,
                    ))
                for name, url in (remotes or {}).items():
                    config.write(
                        '[remote "{name}"]\n'
                        '\turl = {url}\n'
                        '\tfetch = +refs/heads/*:refs/remotes/{name}/*\n'.format(
                            name=name, url=url,
                        )
                    )
                if git_svn:
                    domain = 'webkit.org'
                    if self.remote.startswith('https://'):
                        domain = self.remote.split('/')[2]
                    elif '@' in self.remote:
                        domain = self.remote.split('@')[1].split(':')[0]

                    config.write(
                        '[svn-remote "svn"]\n'
                        '    url = https://svn.{domain}/repository/webkit\n'
                        '    fetch = trunk:refs/remotes/origin/{branch}'.format(
                            domain=domain,
                            branch=self.default_branch,
                        )
                    )

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
                ), mocks.Subprocess.Route(
                    self.executable, 'svn', 'fetch',
                    cwd=self.path,
                    completion=mocks.ProcessCompletion(returncode=0)
                ), mocks.Subprocess.Route(
                    self.executable, 'svn', 'dcommit',
                    cwd=self.path,
                    generator=lambda *args, **kwargs: self.dcommit(),
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
                self.executable, 'rev-parse', '--git-common-dir',
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='.git\n',
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
                self.executable, 'remote', 'add', re.compile(r'.+'),
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=0,
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-a',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(['* ' + self.branch] + list(({default_branch} | set(self.commits.keys())) - {self.branch}))) +
                           '\nremotes/origin/HEAD -> origin/{}\n'.format(default_branch) + '\n'.join(['  remotes/{}'.format(name) for name in self.remotes.keys()]) + '\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'tag',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(self.tags.keys())) + '\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'ls-remote', '--tags', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        '{hash}\trefs/tags/{tag}\n{hash}\trefs/tags/{tag}^{{}}'.format(hash=commit.hash, tag=tag) for tag, commit in sorted(self.tags.items())
                    ]) + '\n',
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
                self.executable, 'log', re.compile(r'.+'), '-1', '--no-decorate', '--date=unix',
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
                            date=self.find(args[2]).timestamp,
                            log='\n'.join([
                                    ('    ' + line) if line else '' for line in self.find(args[2]).message.splitlines()
                                ] + (['    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                    self.find(args[2]).revision,
                                )] if git_svn else []),
                            )
                        ),
                ) if self.find(args[2]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'log', '--format=fuller', '--no-decorate', '--date=unix', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        'commit {hash}\n'
                        'Author:     {author} <{email}>\n'
                        'AuthorDate: {date}\n'
                        'Commit:     {author} <{email}>\n'
                        'CommitDate: {date}\n'
                        '\n{log}\n'.format(
                            hash=commit.hash,
                            author=commit.author.name,
                            email=commit.author.email,
                            date=commit.timestamp,
                            log='\n'.join(
                                [
                                    ('    ' + line) if line else '' for line in commit.message.splitlines()
                                ] + (['    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                    commit.revision,
                                )] if git_svn else []),
                            )
                        ) for commit in list(self.rev_list(args[5]))
                    ])
                )
            ),
            # We don't have modified files for our mock commits, so we assume that every scope
            # applies to all odd commits
            mocks.Subprocess.Route(
                self.executable, 'log', '--pretty=%H', re.compile(r'.+'), '--', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        commit.hash for commit in list(self.rev_list(args[3])) if commit.identifier % 2
                    ])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'log', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        'commit {hash}\n'
                        'Author: {author} <{email}>\n'
                        'Date:   {date}\n'
                        '\n{log}\n'.format(
                            hash=commit.hash,
                            author=commit.author.name,
                            email=commit.author.email,
                            date=commit.timestamp if '--date=unix' in args else datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000'),
                            log='\n'.join(
                                [
                                    ('    ' + line) if line else '' for line in commit.message.splitlines()
                                ] + (['    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                    commit.revision,
                                )] if git_svn else [])
                            )
                        ) for commit in self.rev_list(args[2])
                    ])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'rev-list', '--count', '--no-merges', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='{}\n'.format(self.count(args[4]))
                ) if self.find(args[4]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'rev-list', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(map(lambda commit: commit.hash, self.rev_list(args[2])))
                ),
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
                self.executable, 'branch', '--contains', re.compile(r'.+'), '-a',
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join(sorted(self.branches_on(args[3]))) + '\n',
                ) if self.find(args[3]) else mocks.ProcessCompletion(returncode=128),
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', '-b', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[3], create=True) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', '-B', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[3], create=True, force=True) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[2], create=False) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'rebase', 'HEAD', re.compile(r'.+'), '--autostash',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[3], create=False) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'filter-branch', '-f', '--env-filter', re.compile(r'.*'), '--msg-filter',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.filter_branch(
                    args[-1],
                    identifier_template=args[-2].split("'")[-2] if args[-3] == '--msg-filter' else None,
                    environment_shell=args[4] if args[3] == '--env-filter' and args[4] else None,
                )
            ), mocks.Subprocess.Route(
                self.executable, 'filter-branch', '-f', '--env-filter', re.compile(r'.*'), '--msg-filter', re.compile(r'sed .*'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.filter_branch(
                    args[-1],
                    environment_shell=args[4] if args[3] == '--env-filter' and args[4] else None,
                    sed=args[6].split('sed ')[-1] if args[5] == '--msg-filter' else None,
                )
            ), mocks.Subprocess.Route(
                self.executable, 'filter-branch', '-f',
                cwd=self.path,
                completion=mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable, 'svn', 'fetch', '--log-window-size=5000', '-r', re.compile(r'\d+:HEAD'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if git_svn or local.Git(self.path).is_svn else mocks.ProcessCompletion(returncode=-1),
            ), mocks.Subprocess.Route(
                self.executable, 'pull',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.pull(),
            ), mocks.Subprocess.Route(
                self.executable, 'pull', '--rebase=True', '--autostash',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.pull(autostash=True),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '-l',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join(['{}={}'.format(key, value) for key, value in self.config().items()])
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '-l', '--file', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join([
                            '{}={}'.format(key, value) for key, value in self.config(path=os.path.join(self.path, args[4])).items()
                        ])
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '-l', '--global',
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join(['{}={}'.format(key, value) for key, value in Git.config().items()])
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'config', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    self.edit_config(args[2], args[3]),
            ), mocks.Subprocess.Route(
                self.executable, 'fetch', re.compile(r'.+'),
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=0,
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'diff', '--name-only',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0, stdout='\n'.join(sorted([
                        key for key, value in self.modified.items() if value.startswith('diff')
                    ]))),
            ), mocks.Subprocess.Route(
                self.executable, 'diff', '--name-only', '--staged',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0, stdout='\n'.join(sorted(self.staged.keys()))),
            ), mocks.Subprocess.Route(
                self.executable, 'diff', '--name-status', '--staged',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0, stdout='\n'.join(sorted([
                        '{}       {}'.format('M' if value.startswith('diff') else 'A', key) for key, value in self.staged.items()
                    ]))),
            ), mocks.Subprocess.Route(
                self.executable, 'check-ref-format', re.compile(r'.+'),
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if re.match(r'^[A-Za-z0-9-]+/[A-Za-z0-9/-]+$', args[2]) else mocks.ProcessCompletion(),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '--date=now',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(amend=False, env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '--date=now', '--amend',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(amend=True, env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'revert', '--no-commit', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.revert(commit_hashes=[args[3]], no_commit=True),
            ), mocks.Subprocess.Route(
                self.executable, 'revert', '--continue', '--no-edit',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.revert(revert_continue=True),
            ), mocks.Subprocess.Route(
                self.executable, 'revert', '--abort',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.revert(revert_abort=True),
            ), mocks.Subprocess.Route(
                self.executable, 'cherry-pick', '-e',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.cherry_pick(args[3], env=kwargs.get('env', dict())),
            ), mocks.Subprocess.Route(
                self.executable, 'restore', '--staged', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.restore(args[3], staged=True),
            ), mocks.Subprocess.Route(
                self.executable, 'add', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.add(args[2]),
            ), mocks.Subprocess.Route(
                self.executable, 'push', '-f', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable, 'fetch', 'origin', re.compile(r'.+:.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable, 'rebase', '--onto', re.compile(r'.+'), re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.rebase(args[3], args[4], args[5]),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-f', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.move_branch(args[3], args[4]),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '-D', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.delete_branch(args[3]),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.move_branch(args[2], args[3]),
            ), mocks.Subprocess.Route(
                self.executable, 'push', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.push(args[2], args[4 if args[3] == '--delete' else 3].split(':')[0]),
            ), mocks.Subprocess.Route(
                self.executable, 'diff', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                    stdout='\n'.join([
                        '--- a/ChangeLog\n+++ b/ChangeLog\n@@ -1,0 +1,0 @@\n{}'.format(
                            '\n'.join(['+ {}'.format(line) for line in commit.message.splitlines()])
                        ) for commit in list(self.rev_list(args[2] if '..' in args[2] else '{}..HEAD'.format(args[2])))
                    ])
                )
            ), mocks.Subprocess.Route(
                self.executable, 'reset', 'HEAD',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.reset(int(args[2].split('~')[-1]) if '~' in args[2] else None),
            ),  mocks.Subprocess.Route(
                self.executable, 'reset', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.reset_commit(args[2]),
            ),  mocks.Subprocess.Route(
                self.executable, 'show', re.compile(r'.+'), '--pretty=', '--name-only',
                cwd=self.path,
                # FIXME: All mock commits have the same set of files changed with this implementation
                completion=mocks.ProcessCompletion(
                    returncode=0,
                    stdout='Source/main.cpp\nSource/main.h\n',
                ),
            ),  mocks.Subprocess.Route(
                self.executable, 'branch', '--set-upstream-to', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                ) if args[4] in self.remotes else mocks.ProcessCompletion(returncode=128, stderr="fatal: branch '{}' does not exist".format(args[4])),
            ), mocks.Subprocess.Route(
                self.executable, 'branch', '--track', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: mocks.ProcessCompletion(
                    returncode=0,
                ) if args[4] in self.remotes else mocks.ProcessCompletion(returncode=128, stderr="fatal: branch '{}' does not exist".format(args[4])),
            ), mocks.Subprocess.Route(
                self.executable, 'merge-base', re.compile(r'.+'), re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.merge_base(args[2], args[3]),
            ), mocks.Subprocess.Route(
                self.executable,
                cwd=self.path,
                completion=mocks.ProcessCompletion(
                    returncode=1,
                    stderr='usage: git [--version] [--help]...\n',
                ),
            ), mocks.Subprocess.Route(
                self.executable, 'status',
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout=''''On branch {branch}
Your branch is up to date with 'origin/{branch}'.

nothing to commit, working tree clean
'''.format(branch=self.branch),
                    ),
            ), mocks.Subprocess.Route(
                self.executable,
                completion=mocks.ProcessCompletion(
                    returncode=128,
                    stderr='fatal: not a git repository (or any parent up to mount point)\nStopping at filesystem boundary (GIT_DISCOVERY_ACROSS_FILESYSTEM not set).\n',
                ),
            ), mocks.Subprocess.Route(
                'sudo', 'sh', re.compile(r'.+/install.sh'),
                generator=lambda *args, **kwargs: self._install_git_lfs(),
            ), mocks.Subprocess.Route(
                self.executable, 'lfs', '--version',
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='git-lfs/3.1.2 (???)\n',
                    ) if self.has_git_lfs else mocks.ProcessCompletion(
                        returncode=1,
                        stderr='usage: git [--version] [--help]...\n',
                    ),
            ), mocks.Subprocess.Route(
                self.executable, 'lfs', 'install',
                generator=lambda *args, **kwargs: self._configure_git_lfs(),
            ), *git_svn_routes
        )

    def __enter__(self):
        # TODO: Use shutil directly when Python 2.7 is removed
        from whichcraft import which
        self.patches.append(patch('whichcraft.which', lambda cmd: dict(git=self.executable).get(cmd, which(cmd))))
        return super(Git, self).__enter__()

    @property
    def branch(self):
        return self.head.branch

    def find(self, something):
        if '~' in something:
            split = something.split('~')
            if len(split) == 2 and Commit.NUMBER_RE.match(split[1]):
                found = self.find(split[0])
                difference = int(split[1])
                if split[0] in self.remotes:
                    all_commits = self.resolve_all_commits(found.branch, remote=split[0].replace('/{}'.format(found.branch), ''))
                else:
                    all_commits = self.resolve_all_commits(found.branch)
                head_index = None
                for i in range(len(all_commits)):
                    if found == all_commits[i]:
                        head_index = i
                if head_index is not None and head_index - difference >= 0:
                    return all_commits[head_index - difference]
                return None

        something = str(something).replace('remotes/', '')
        if '..' in something:
            a, b = something.split('..')
            a = self.find(a)
            b = self.find(b)
            return b if a and b else None

        if something == 'HEAD':
            return self.head
        if something in self.commits.keys():
            return self.commits[something][-1]
        if something in self.tags.keys():
            return self.tags[something]
        if something in self.remotes.keys():
            return self.remotes[something][-1]

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
        rev_list = self.rev_list(something)
        return len(rev_list)

    def branches_on(self, hash):
        result = set()
        found_identifier = 0
        if '/' in hash:
            _, hash = hash.split('/', 1)
        for remote in self.remotes.keys():
            if remote.endswith('/{}'.format(hash)):
                result.add('remotes/{}'.format(remote))
        for branch, commits in self.commits.items():
            for commit in commits:
                if commit.hash.startswith(hash) or commit.branch == hash:
                    if commit.identifier is not None:
                        found_identifier = max(commit.identifier, found_identifier)
                    result.add(commit.branch)

        if self.default_branch in result:
            for branch, commits in self.commits.items():
                if commits[0].branch_point and commits[0].branch_point >= found_identifier:
                    result.add(branch)
        return result

    def checkout(self, something, create=False, force=False):
        if something in self.modified:
            del self.modified[something]
            return mocks.ProcessCompletion(returncode=0, stdout='Updated 1 path from the index')
        commit = self.find(something)
        if create:
            if commit:
                if force:
                    self.head = commit
                    self.detached = something not in self.commits.keys()
                    return True
                return False
            self.commits[something] = [Commit.from_json(Commit.Encoder().default(self.head))]
            # Copy one more to create a bridge commit
            self.commits[something].append(Commit.from_json(Commit.Encoder().default(self.head)))
            self.head = self.commits[something][-1]
            setattr(self.head, 'bridge_commit', True)
            self.head.branch = something
            if not self.head.branch_point:
                self.head.branch_point = self.head.identifier
                self.head.identifier = 0
            return True

        if commit:
            self.head = commit
            self.detached = something not in self.commits.keys()
        return True if commit else False

    def filter_branch(self, range, identifier_template=None, environment_shell=None, sed=None, autostash=False):
        if not autostash and (self.modified or self.staged):
            return mocks.ProcessCompletion(returncode=128)

        # We can't effectively mock the bash script in the command, but we can mock the python code that
        # script calls, which is where the program logic is.
        head, start = range.split('...')
        head = self.find(head)
        start = self.find(start)

        commits_to_edit = []
        for commit in reversed(self.commits[head.branch]):
            if commit.branch == start.branch and commit.identifier <= start.identifier:
                break
            commits_to_edit.insert(0, commit)
        if head.branch != self.default_branch:
            for commit in reversed(self.commits[self.default_branch][:head.branch_point]):
                if commit.identifier <= start.identifier:
                    break
                commits_to_edit.insert(0, commit)

        stdout = StringIO()
        original_env = {key: os.environ.get('OLDPWD') for key in [
            'OLDPWD', 'GIT_COMMIT',
            'GIT_AUTHOR_NAME', 'GIT_AUTHOR_EMAIL',
            'GIT_COMMITTER_NAME', 'GIT_COMMITTER_EMAIL',
        ]}

        try:
            count = 0
            os.environ['OLDPWD'] = self.path
            for commit in commits_to_edit:
                count += 1
                os.environ['GIT_COMMIT'] = commit.hash
                os.environ['GIT_AUTHOR_NAME'] = commit.author.name
                os.environ['GIT_AUTHOR_EMAIL'] = commit.author.email
                os.environ['GIT_COMMITTER_NAME'] = commit.author.name
                os.environ['GIT_COMMITTER_EMAIL'] = commit.author.email

                stdout.write(
                    'Rewrite {hash} ({count}/{total}) (--- seconds passed, remaining --- predicted)\n'.format(
                        hash=commit.hash,
                        count=count,
                        total=len(commits_to_edit),
                    ))

                if identifier_template:
                    messagefile = StringIO()
                    messagefile.write(commit.message)
                    messagefile.seek(0)
                    with OutputCapture() as captured:
                        message_main(messagefile, identifier_template)
                    lines = captured.stdout.getvalue().splitlines()
                    if lines[-1].startswith('git-svn-id: https://svn'):
                        lines.pop(-1)
                    commit.message = '\n'.join(lines)

                if sed:
                    match = re.match(r'"s/(?P<re>.+)/(?P<value>.+)/g"', sed)
                    if match:
                        commit.message = re.sub(
                            match.group('re').replace('(', '\\(').replace(')', '\\)'),
                            match.group('value'), commit.message,
                        )

                if not environment_shell:
                    continue
                if re.search(r'echo "Overwriting', environment_shell):
                    stdout.write('Overwriting {}\n'.format(commit.hash))

                match = re.search(r'(?P<json>\S+\.json)', environment_shell)
                if match:
                    with OutputCapture() as captured:
                        committer_main(match.group('json'))
                    captured.stdout.seek(0)
                    for line in captured.stdout.readlines():
                        line = line.rstrip()
                        os.environ[line.split(' ')[0]] = ' '.join(line.split(' ')[1:])

                commit.author = Contributor(name=os.environ['GIT_AUTHOR_NAME'], emails=[os.environ['GIT_AUTHOR_EMAIL']])

                if re.search(r'echo "\s+', environment_shell):
                    for key in ['GIT_AUTHOR_NAME', 'GIT_AUTHOR_EMAIL', 'GIT_COMMITTER_NAME', 'GIT_COMMITTER_EMAIL']:
                        stdout.write('    {}={}\n'.format(key, os.environ[key]))

        finally:
            for key, value in original_env.items():
                if value is not None:
                    os.environ[key] = value
                else:
                    del os.environ[key]

        return mocks.ProcessCompletion(
            returncode=0,
            stdout=stdout.getvalue(),
        )

    @decorators.hybridmethod
    def config(context, path=None):
        if isinstance(context, type):
            return {
                'user.name': 'Tim Apple',
                'user.email': 'tapple@webkit.org',
                'sendemail.transferencoding': 'base64',
            }

        top = None
        result = Git.config()
        path = path or os.path.join(context.path, '.git', 'config')
        if not os.path.isfile(path):
            return result
        with open(path, 'r') as configfile:
            for line in configfile.readlines():
                match = context.RE_MULTI_TOP.match(line)
                if match:
                    top = '{}.{}'.format(match.group('keya'), match.group('keyb'))
                    continue
                match = context.RE_SINGLE_TOP.match(line)
                if match:
                    top = match.group('key')
                    continue

                match = context.RE_ELEMENT.match(line)
                if top and match:
                    result['{}.{}'.format(top, match.group('key'))] = match.group('value')
        return result

    def edit_config(self, key, value):
        with open(os.path.join(self.path, '.git', 'config'), 'r') as configfile:
            lines = [line for line in configfile.readlines()]

        key_a = key.split('.')[0]
        key_b = '.'.join(key.split('.')[1:])

        did_print = False
        with open(os.path.join(self.path, '.git', 'config'), 'w') as configfile:
            for line in lines:
                match = self.RE_ELEMENT.match(line)
                if not match or match.group('key') != key_b:
                    configfile.write(line)
                match = self.RE_MULTI_TOP.match(line)
                if not match or '{}.{}'.format(match.group('keya'), match.group('keyb')) != key_a:
                    continue
                configfile.write('\t{}={}\n'.format(key_b, value))
                did_print = True

            if not did_print:
                configfile.write('[{}]\n'.format(key_a))
                configfile.write('\t{}={}\n'.format(key_b, value))

        return mocks.ProcessCompletion(returncode=0)

    def commit(self, amend=False, env=None):
        env = env or dict()
        if not self.head:
            return mocks.ProcessCompletion(returncode=1, stdout='Allowed in git, but disallowed by reasonable workflows')
        if not self.staged and not amend:
            return mocks.ProcessCompletion(returncode=1, stdout='no changes added to commit (use "git add" and/or "git commit -a")\n')

        if not amend:
            # Remove the temp bridge commit
            if hasattr(self.head, 'bridge_commit'):
                self.commits[self.head.branch].remove(self.head)
            self.head = Commit(
                branch=self.branch, repository_id=self.head.repository_id,
                timestamp=int(time.time()),
                identifier=self.head.identifier + 1 if self.head.branch_point else 1,
                branch_point=self.head.branch_point or self.head.identifier,
            )
            self.commits[self.branch].append(self.head)

        self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
        self.head.message = '{}{}\nReviewed by Jonathan Bedard\n\n * {}\n{}'.format(
            env.get('COMMIT_MESSAGE_TITLE', '') or '[Testing] {} commits'.format('Amending' if amend else 'Creating'),
            ('\n' + env.get('COMMIT_MESSAGE_BUG', '')) if env.get('COMMIT_MESSAGE_BUG', '') else '',
            '\n * '.join(self.staged.keys()),
            env.get('COMMIT_MESSAGE_CONTENT', '')
        )
        self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
        self.staged = {}
        return mocks.ProcessCompletion(returncode=0)

    def revert(self, commit_hashes=[], no_commit=False, revert_continue=False, revert_abort=False):
        if revert_continue:
            if not self.staged:
                return mocks.ProcessCompletion(returncode=1, stdout='error: no cherry-pick or revert in progress\nfatal: revert failed')
            self.staged = {}
            self.head = Commit(
                branch=self.branch, repository_id=self.head.repository_id,
                timestamp=int(time.time()),
                identifier=self.head.identifier + 1 if self.head.branch_point else 1,
                branch_point=self.head.branch_point or self.head.identifier,
                message=self.revert_message
            )
            self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
            self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
            self.commits[self.branch].append(self.head)
            self.revert_message = None
            return mocks.ProcessCompletion(returncode=0)

        if revert_abort:
            if not self.staged:
                return mocks.ProcessCompletion(returncode=1, stdout='error: no cherry-pick or revert in progress\nfatal: revert failed')
            self.staged = {}
            self.revert_message = None
            return mocks.ProcessCompletion(returncode=0)

        if self.modified:
            return mocks.ProcessCompletion(returncode=1, stdout='error: your local changes would be overwritten by revert.')

        is_reverted_something = False
        for hash in commit_hashes:
            commit_revert = self.find(hash)
            if not no_commit:
                self.head = Commit(
                    branch=self.branch, repository_id=self.head.repository_id,
                    timestamp=int(time.time()),
                    identifier=self.head.identifier + 1 if self.head.branch_point else 1,
                    branch_point=self.head.branch_point or self.head.identifier,
                    message='Revert "{}"\n\nThis reverts commit {}'.format(commit_revert.message.splitlines()[0], hash)
                )
                self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
                self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
                self.commits[self.branch].append(self.head)
            else:
                self.staged['{}/some_file'.format(hash)] = 'modified'
                self.staged['{}/ChangeLog'.format(hash)] = 'modified'
                # git revert only generate one commit message
                self.revert_message = 'Revert "{}"\n\nThis reverts commit {}'.format(commit_revert.message.splitlines()[0], hash)

            is_reverted_something = True
        if not is_reverted_something:
            return mocks.ProcessCompletion(returncode=1, stdout='On branch {}\nnothing to commit, working tree clean'.format(self.branch))

        return mocks.ProcessCompletion(returncode=0)

    def cherry_pick(self, hash, env=None):
        commit = self.find(hash)
        env = env or dict()

        if self.staged:
            return mocks.ProcessCompletion(returncode=1, stdout='error: your local changes would be overwritten by cherry-pick.\nfatal: cherry-pick failed\n')
        if not commit:
            return mocks.ProcessCompletion(returncode=128, stdout="fatal: bad revision '{}'\n".format(hash))

        self.head = Commit(
            branch=self.branch, repository_id=self.head.repository_id,
            timestamp=int(time.time()),
            identifier=self.head.identifier + 1 if self.head.branch_point else 1,
            branch_point=self.head.branch_point or self.head.identifier,
            message='Cherry-pick {}. {}\n    {}\n'.format(
                env.get('GIT_WEBKIT_CHERRY_PICKED', '') or commit.hash,
                env.get('GIT_WEBKIT_BUG', '') or '<bug>',
                '\n    '.join(commit.message.splitlines()),
            ),
        )
        self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
        self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
        self.commits[self.branch].append(self.head)

        return mocks.ProcessCompletion(returncode=0)

    def restore(self, file, staged=False):
        if staged:
            if file in self.staged:
                self.modified[file] = self.staged[file]
                del self.staged[file]
                return mocks.ProcessCompletion(returncode=0)
            return mocks.ProcessCompletion(returncode=0)
        return mocks.ProcessCompletion(returncode=1)

    def add(self, file):
        if file not in self.modified:
            return mocks.ProcessCompletion(returncode=128, stdout="fatal: pathspec '{}' did not match any files\n".format(file))
        for key, value in self.modified.items():
            self.staged[key] = value
        del self.modified[file]
        return mocks.ProcessCompletion(returncode=0)

    def rebase(self, target, base, head):
        if target not in self.commits or base not in self.commits or head not in self.commits:
            return mocks.ProcessCompletion(returncode=1)

        base = self.commits[target][-1]
        self.commits[head][0] = base
        for commit in self.commits[head][1:]:
            commit.branch_point = base.branch_point or base.identifier
            if base.branch_point:
                commit.identifier += base.identifier
        return mocks.ProcessCompletion(returncode=0)

    def pull(self, autostash=False):
        if not autostash and (self.modified or self.staged):
            return mocks.ProcessCompletion(returncode=128)
        self.head = self.commits[self.head.branch][-1]
        return mocks.ProcessCompletion(returncode=0)

    def move_branch(self, to_be_moved, moved_to):
        if moved_to.startswith('remotes/'):
            moved_to = moved_to.split('/', 2)[-1]
        if moved_to == self.default_branch:
            return mocks.ProcessCompletion(returncode=0)
        if to_be_moved != self.default_branch:
            self.commits[to_be_moved] = self.commits[moved_to]
            self.head = self.commits[to_be_moved][-1]
            return mocks.ProcessCompletion(returncode=0)
        self.commits[to_be_moved] += [
            Commit(
                branch=to_be_moved, repository_id=commit.repository_id,
                timestamp=commit.timestamp,
                identifier=commit.identifier + (commit.branch_point or 0), branch_point=None,
                hash=commit.hash, revision=commit.revision,
                author=commit.author, message=commit.message,
            ) for commit in self.commits[moved_to]
        ]
        self.head = self.commits[to_be_moved][-1]
        return mocks.ProcessCompletion(returncode=0)

    def delete_branch(self, branch):
        if branch in self.commits:
            del self.commits[branch]
            return mocks.ProcessCompletion(returncode=0)
        return mocks.ProcessCompletion(
            returncode=1,
            stdout="error: branch '{}' not found.\n".format(branch),
        )

    def push(self, remote, branch):
        remote_branch = '{}/{}'.format(remote, branch)
        if branch in self.commits:
            self.remotes[remote_branch] = self.commits[branch][:]
        elif remote_branch in self.remotes:
            del self.remotes[remote_branch]
        return mocks.ProcessCompletion(returncode=0)

    def dcommit(self, remote='origin', branch=None):
        branch = branch or self.default_branch
        self.remotes['{}/{}'.format(remote, branch)] = self.commits[branch][:]
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='Committed r{}\n\tM\tFiles/Changed.txt\n'.format(self.commits[branch][-1].revision),
        )

    def reset_commit(self, something):
        commit = self.find(something)
        pre_branch = self.branch
        rev_list = self.rev_list('HEAD...{}'.format(something))
        commits = self.commits[self.branch]
        if commit is not None:
            self.head = commit
        for commit in rev_list:
            if hasattr(commit, '__mock__changeFiles'):
                files = getattr(commit, '__mock__changeFiles')
                for file in files:
                    self.modified[file] = files[file]
            commits.remove(commit)
        if pre_branch != self.branch:
            # Add a fake commit to simulate a same commit in different branch
            bridge_commit = Commit(
                hash=self.head.hash, revision=self.head.revision,
                identifier=self.head.identifier, branch=pre_branch, branch_point=self.head.branch_point,
                timestamp=self.head.timestamp, author=self.head.author, message=self.head.message,
                order=self.head.order, repository_id=self.head.repository_id
            )
            setattr(bridge_commit, 'bridge_commit', True)
            commits.append(bridge_commit)
            self.head = commits[-1]
        return mocks.ProcessCompletion(returncode=0)

    def reset(self, index):
        if index is None:
            self.modified = {}
            self.staged = {}
            return mocks.ProcessCompletion(returncode=0)

        self.head = self.commits[self.head.branch][-(index + 1)]
        return mocks.ProcessCompletion(returncode=0)

    def resolve_all_commits(self, branch, remote=None):
        if not remote:
            all_commits = self.commits[branch][:]
        else:
            all_commits = self.remotes['{}/{}'.format(remote, branch)][:]
        last_commit = all_commits[0]
        while last_commit.branch != branch:
            head_index = None
            if not remote:
                commits_part = self.commits[last_commit.branch]
            else:
                commits_part = self.remotes['{}/{}'.format(remote, last_commit.branch)]
            for i in range(len(commits_part)):
                if commits_part[i].hash == last_commit.hash:
                    head_index = i
                    break
            all_commits = commits_part[:head_index] + all_commits
            last_commit = all_commits[0]
            if last_commit.branch == self.default_branch and last_commit.identifier == 1:
                break
        if remote:
            for commit in all_commits:
                setattr(commit, '__mock__remotes', set([remote]))
        return all_commits

    def rev_list(self, something):
        """
        A..B = A u B - A
        A...B = A u B - A n B
        """
        two_dots = False
        triple_dots = False
        a_commit = None
        a_remote = None
        b_commit = None
        b_remote = None
        if '...' in something:
            something = something.split('...')
            triple_dots = True
            a_commit = self.find(something[0])
            b_commit = self.find(something[1])
            if something[0] in self.remotes:
                a_remote = something[0].replace('/{}'.format(a_commit.branch), '')
            if something[1] in self.remotes:
                b_remote = something[1].replace('/{}'.format(b_commit.branch), '')
        elif '..' in something:
            something = something.split('..')
            two_dots = True
            a_commit = self.find(something[0])
            b_commit = self.find(something[1])
            if something[0] in self.remotes:
                a_remote = something[0].replace('/{}'.format(a_commit.branch), '')
            if something[1] in self.remotes:
                b_remote = something[1].replace('/{}'.format(b_commit.branch), '')
        else:
            a_commit = self.find(something)
            if something in self.remotes:
                a_remote = something.replace('/{}'.format(a_commit.branch), '')

        a_commits = []
        a_branch_commits = self.resolve_all_commits(a_commit.branch, remote=a_remote) if a_commit else []
        for commit in a_branch_commits:
            a_commits.append(commit)
            if commit.hash == a_commit.hash:
                break

        b_commits = []
        b_branch_commits = self.resolve_all_commits(b_commit.branch, remote=b_remote) if b_commit else []
        for commit in b_branch_commits:
            b_commits.append(commit)
            if commit.hash == b_commit.hash:
                break

        if not two_dots and not triple_dots:
            return list(reversed(a_commits))

        res = []
        # To make things easier, we only mock that two branch will share same init commit
        assert a_commits[0].hash == b_commits[0].hash
        for i in range(max(len(a_commits), len(b_commits))):
            if i >= len(a_commits) and i < len(b_commits):
                res.append(b_commits[i])
            elif i >= len(b_commits) and i < len(a_commits):
                if triple_dots:
                    res.append(a_commits[i])
            elif i < len(b_commits) and i < len(a_commits) and a_commits[i].hash != b_commits[i].hash:
                if triple_dots:
                    res.append(a_commits[i])
                    res.append(b_commits[i])
                if two_dots:
                    res.append(b_commits[i])
        res.reverse()
        return res

    def _install_git_lfs(self):
        self.has_git_lfs = True
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='Git LFS initialized.\n',
        )

    def _configure_git_lfs(self):
        if not self.has_git_lfs:
            return mocks.ProcessCompletion(
                returncode=1,
                stderr='usage: git [--version] [--help]...\n',
            )
        self.edit_config('lfs.repositoryformatversion', '0')
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='Updated Git hooks.\nGit LFS initialized.\n',
        )

    def merge_base(self, *refs):
        objs = [self.find(ref) for ref in refs]
        for i in [0, 1]:
            if not refs[i] or not objs[i]:
                return mocks.ProcessCompletion(
                    returncode=128,
                    stderr='fatal: Not a valid object name {}\n'.format(refs[i]),
                )
        if objs[0].branch != objs[1].branch:
            for i in [0, 1]:
                if objs[i].branch == self.default_branch:
                    continue
                objs[i] = self.commits[self.default_branch][objs[i].branch_point - 1]

        base = objs[0] if objs[0].identifier < objs[1].identifier else objs[1]
        return mocks.ProcessCompletion(
            returncode=0,
            stdout='{}\n'.format(base.hash),
        )
