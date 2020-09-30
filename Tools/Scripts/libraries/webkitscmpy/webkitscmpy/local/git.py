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


from webkitcorepy import run, decorators, TimeoutExpired
from webkitscmpy.local import Scm


class Git(Scm):
    executable = '/usr/bin/git'

    @classmethod
    def is_checkout(cls, path):
        return run([cls.executable, 'rev-parse', '--show-toplevel'], cwd=path, capture_output=True).returncode == 0

    def __init__(self, path):
        super(Git, self).__init__(path)
        if not self.root_path:
            raise OSError('Provided path {} is not a git repository'.format(path))

    @decorators.Memoize(cached=False)
    def info(self):
        if not self.is_svn:
            raise self.Exception('Cannot run SVN info on a git checkout which is not git-svn')

        info_result = run([self.executable, 'svn', 'info'], cwd=self.path, capture_output=True, encoding='utf-8')
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
                [self.executable, 'svn', 'find-rev', 'r1'],
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
        result = run([self.executable, 'rev-parse', '--show-toplevel'], cwd=self.path, capture_output=True, encoding='utf-8')
        if result.returncode:
            return None
        return result.stdout.rstrip()

    @property
    def default_branch(self):
        result = run([self.executable, 'rev-parse', '--abbrev-ref', 'origin/HEAD'], cwd=self.path, capture_output=True, encoding='utf-8')
        if result.returncode:
            return None
        return '/'.join(result.stdout.rstrip().split('/')[1:])

    @property
    def branch(self):
        status = run([self.executable, 'status'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if status.returncode:
            raise self.Exception('Failed to run `git status` for {}'.format(self.root_path))
        if status.stdout.splitlines()[0].startswith('HEAD detached at'):
            return None

        result = run([self.executable, 'rev-parse', '--abbrev-ref', 'HEAD'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if result.returncode:
            raise self.Exception('Failed to retrieve branch for {}'.format(self.root_path))
        return result.stdout.rstrip()

    @property
    def branches(self):
        branch = run([self.executable, 'branch', '-a'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if branch.returncode:
            raise self.Exception('Failed to retrieve branch list for {}'.format(self.root_path))
        result = [branch.lstrip(' *') for branch in filter(lambda branch: '->' not in branch, branch.stdout.splitlines())]
        return ['/'.join(branch.split('/')[2:]) if branch.startswith('remotes/origin/') else branch for branch in result]

    @property
    def tags(self):
        tags = run([self.executable, 'tag'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if tags.returncode:
            raise self.Exception('Failed to retrieve tag list for {}'.format(self.root_path))
        return tags.stdout.splitlines()

    def remote(self, name=None):
        result = run([self.executable, 'remote', 'get-url', name or 'origin'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if result.returncode:
            raise self.Exception('Failed to retrieve remote for {}'.format(self.root_path))
        return result.stdout.rstrip()
