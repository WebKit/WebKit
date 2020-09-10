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
from webkitscmpy.local.scm import Scm


class Svn(Scm):
    executable = '/usr/local/bin/svn'

    @classmethod
    def is_checkout(cls, path):
        return run([cls.executable, 'info'], cwd=path, capture_output=True).returncode == 0

    def __init__(self, path):
        super(Svn, self).__init__(path)
        if not self.root_path:
            raise OSError('Provided path {} is not a svn repository'.format(path))

    @decorators.Memoize(cached=False)
    def info(self):
        info_result = run([self.executable, 'info'], cwd=self.path, capture_output=True, encoding='utf-8')
        if info_result.returncode:
            return {}

        result = {}
        for line in info_result.stdout.splitlines():
            split = line.split(': ')
            result[split[0]] = ': '.join(split[1:])
        return result

    @property
    def root_path(self):
        return self.info(cached=True).get('Working Copy Root Path')

    @property
    def branch(self):
        local_path = self.path[len(self.root_path):]
        if local_path:
            return self.info()['Relative URL'][2:-len(local_path)]
        return self.info()['Relative URL'][2:]

    def list(self, category):
        list_result = run([self.executable, 'list', '^/{}'.format(category)], cwd=self.path, capture_output=True, encoding='utf-8')
        if list_result.returncode:
            return []
        return [element.rstrip('/') for element in list_result.stdout.splitlines()]

    @property
    def branches(self):
        return ['trunk'] + self.list('branches')

    @property
    def tags(self):
        return self.list('tags')

    def remote(self, name=None):
        return self.info()['Repository Root']
