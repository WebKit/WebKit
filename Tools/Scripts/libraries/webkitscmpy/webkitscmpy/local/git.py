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


from webkitcorepy import run, decorators
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

    @property
    @decorators.Memoize()
    def root_path(self):
        result = run([self.executable, 'rev-parse', '--show-toplevel'], cwd=self.path, capture_output=True, encoding='utf-8')
        if result.returncode:
            return None
        return result.stdout.rstrip()

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

    def remote(self, name=None):
        result = run([self.executable, 'remote', 'get-url', name or 'origin'], cwd=self.root_path, capture_output=True, encoding='utf-8')
        if result.returncode:
            raise self.Exception('Failed to retrieve remote for {}'.format(self.root_path))
        return result.stdout.rstrip()
