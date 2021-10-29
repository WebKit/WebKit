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
    RE_ELEMENT = re.compile(r'^\s+(?P<key>\S+)\s*=\s*(?P<value>\S+)')

    def __init__(
        self, path='/.invalid-git', datafile=None,
        remote=None, tags=None,
        detached=None, default_branch='main',
        git_svn=False,
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
            self.commits[key] = [Commit(**kwargs) for kwargs in commits]
            if not git_svn:
                for commit in self.commits[key]:
                    commit.revision = None

        self.head = self.commits[self.default_branch][-1]
        self.remotes = {'origin/{}'.format(branch): commits[-1] for branch, commits in self.commits.items()}
        self.tags = {}

        self.staged = {}
        self.modified = {}

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
                            date=datetime.utcfromtimestamp(self.find(args[2]).timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000'),
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
                self.executable, 'log', '--format=fuller', re.compile(r'.+\.\.\..+'),
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
                            date=datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000'),
                            log='\n'.join([
                                ('    ' + line) if line else '' for line in commit.message.splitlines()
                            ] + ([
                                '    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                   commit.revision,
                            )] if git_svn else []),
                        )) for commit in self.commits_in_range(args[3].split('...')[-1], args[3].split('...')[0])
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
                            date=datetime.utcfromtimestamp(commit.timestamp + time.timezone).strftime('%a %b %d %H:%M:%S %Y +0000'),
                            log='\n'.join([
                                ('    ' + line) if line else '' for line in commit.message.splitlines()
                            ] + ([
                                '    git-svn-id: https://svn.{}/repository/{}/trunk@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc'.format(
                                    self.remote.split('@')[-1].split(':')[0],
                                    os.path.basename(path),
                                   commit.revision,
                            )] if git_svn else []),
                        )) for commit in self.commits_in_range(self.commits[self.default_branch][0].hash, args[2])
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
                self.executable, 'checkout', '-b', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[3], create=True) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'checkout', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if self.checkout(args[2], create=False) else mocks.ProcessCompletion(returncode=1)
            ), mocks.Subprocess.Route(
                self.executable, 'filter-branch', '-f',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.filter_branch(
                    args[-1],
                    identifier_template=args[-2].split("'")[-2] if args[-3] == '--msg-filter' else None,
                    environment_shell=args[4] if args[3] == '--env-filter' and args[4] else None,
                )
            ), mocks.Subprocess.Route(
                self.executable, 'svn', 'fetch', '--log-window-size=5000', '-r', re.compile(r'\d+:HEAD'),
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(returncode=0) if git_svn or local.Git(self.path).is_svn else mocks.ProcessCompletion(returncode=-1),
            ), mocks.Subprocess.Route(
                self.executable, 'pull',
                cwd=self.path,
                completion=mocks.ProcessCompletion(returncode=0),
            ), mocks.Subprocess.Route(
                self.executable, 'config', '-l',
                cwd=self.path,
                generator=lambda *args, **kwargs:
                    mocks.ProcessCompletion(
                        returncode=0,
                        stdout='\n'.join(['{}={}'.format(key, value) for key, value in self.config().items()])
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
                self.executable, 'fetch', 'fork',
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
                generator=lambda *args, **kwargs: self.commit(amend=False),
            ), mocks.Subprocess.Route(
                self.executable, 'commit', '--date=now', '--amend',
                cwd=self.path,
                generator=lambda *args, **kwargs: self.commit(amend=True),
            ), mocks.Subprocess.Route(
                self.executable, 'add', re.compile(r'.+'),
                cwd=self.path,
                generator=lambda *args, **kwargs: self.add(args[2]),
            ), mocks.Subprocess.Route(
                self.executable, 'push', '-f',
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
                if difference < found.identifier:
                    return self.commits[found.branch][found.identifier - difference - 1]
                difference -= found.identifier
                if found.branch_point and difference < found.branch_point:
                    return self.commits[self.default_branch][found.branch_point - difference - 1]
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
            return self.remotes[something]

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
        if '..' not in something:
            match = self.find(something)
            return (match.branch_point or 0) + match.identifier

        a, b = something.split('..')
        a = self.find(a)
        b = self.find(b)
        if a.branch_point == b.branch_point:
            return abs(b.identifier - a.identifier)
        return b.identifier

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

    def checkout(self, something, create=False):
        commit = self.find(something)
        if create:
            if commit:
                return False
            if self.head.branch == self.default_branch:
                self.commits[something] = [self.head]
            else:
                self.commits[something] = [
                    commit for commit in self.commits[self.head.branch]
                    if not commit.branch_point or commit.identifier <= self.head.identifier
                ]
            self.commits[something][-1] = Commit.from_json(Commit.Encoder().default(self.head))
            self.head = self.commits[something][-1]
            self.head.branch = something
            if not self.head.branch_point:
                self.head.branch_point = self.head.identifier
                self.head.identifier = 0
            return True

        if commit:
            self.head = commit
            self.detached = something not in self.commits.keys()
        return True if commit else False

    def filter_branch(self, range, identifier_template=None, environment_shell=None):
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

    def commits_in_range(self, begin, end):
        begin = begin.replace('remotes/', '') if begin else begin
        end = end.replace('remotes/', '') if end else end

        if begin and self.remotes.get(begin):
            begin = self.remotes.get(begin).hash
        if end and self.remotes.get(end):
            end = self.remotes.get(end).hash

        branches = [self.default_branch]
        if end in self.commits.keys() and end != self.default_branch:
            branches.insert(0, end)
        else:
            for branch, commits in self.commits.items():
                if branch == self.default_branch:
                    continue
                for commit in commits:
                    if commit.hash.startswith(end):
                        branches.insert(0, branch)
                        break
                if len(branches) > 1:
                    break

        in_range = False
        previous = None
        for branch in branches:
            for commit in reversed(self.commits[branch]):
                if branch == begin:
                    break
                if commit.hash.startswith(end) or end == branch:
                    in_range = True
                if in_range and (not previous or commit.hash != previous.hash):
                    yield commit
                previous = commit
                if begin and commit.hash.startswith(begin):
                    in_range = False
                    break

            in_range = False
            if not previous or branch == self.default_branch:
                continue

            for commit in reversed(self.commits[self.default_branch]):
                if previous.branch_point == commit.identifier:
                    end = commit.hash

    @decorators.hybridmethod
    def config(context):
        if isinstance(context, type):
            return {
                'user.name': 'Tim Apple',
                'user.email': 'tapple@webkit.org',
                'sendemail.transferencoding': 'base64',
            }

        top = None
        result = Git.config()
        with open(os.path.join(context.path, '.git', 'config'), 'r') as configfile:
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

    def commit(self, amend=False):
        if not self.head:
            return mocks.ProcessCompletion(returncode=1, stdout='Allowed in git, but disallowed by reasonable workflows')
        if not self.staged and not amend:
            return mocks.ProcessCompletion(returncode=1, stdout='no changes added to commit (use "git add" and/or "git commit -a")\n')

        if not amend:
            self.head = Commit(
                branch=self.branch, repository_id=self.head.repository_id,
                timestamp=int(time.time()),
                identifier=self.head.identifier + 1 if self.head.branch_point else 1,
                branch_point=self.head.branch_point or self.head.identifier,
            )
            self.commits[self.branch].append(self.head)

        self.head.author = Contributor(self.config()['user.name'], [self.config()['user.email']])
        self.head.message = '{} commit\nReviewed by Jonathan Bedard\n\n * {}\n'.format(
            'Amended' if amend else 'Created',
            '\n * '.join(self.staged.keys()),
        )
        self.head.hash = hashlib.sha256(string_utils.encode(self.head.message)).hexdigest()[:40]
        self.staged = {}
        return mocks.ProcessCompletion(returncode=0)

    def add(self, file):
        if file not in self.modified:
            return mocks.ProcessCompletion(returncode=128, stdout="fatal: pathspec '{}' did not match any files\n".format(file))
        for key, value in self.modified.items():
            self.staged[key] = value
        self.modified = {}
        return mocks.ProcessCompletion(returncode=0)

    def rebase(self, target, base, head):
        if target not in self.commits or base not in self.commits or head not in self.commits:
            return mocks.ProcessCompletion(returncode=1)
        for commit in self.commits[head]:
            commit.branch_point = self.commits[target][-1].branch_point or self.commits[target][-1].identifier
            if self.commits[target][-1].branch_point:
                commit.identifier += self.commits[target][-1].identifier
        return mocks.ProcessCompletion(returncode=0)
