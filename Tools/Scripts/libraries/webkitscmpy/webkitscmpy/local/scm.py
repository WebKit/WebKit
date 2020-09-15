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


class Scm(object):
    class Exception(RuntimeError):
        pass

    executable = None

    @classmethod
    def from_path(cls, path):
        from webkitscmpy import local

        if local.Git.is_checkout(path):
            return local.Git(path)
        if local.Svn.is_checkout(path):
            return local.Svn(path)
        raise OSError('{} is not a known SCM type')

    def __init__(self, path):
        if not isinstance(path, str):
            raise ValueError('')
        self.path = path

    @property
    def root_path(self):
        raise NotImplementedError()

    @property
    def default_branch(self):
        raise NotImplementedError()

    @property
    def branch(self):
        raise NotImplementedError()

    @property
    def branches(self):
        raise NotImplementedError()

    @property
    def tags(self):
        raise NotImplementedError()

    def remote(self, name=None):
        raise NotImplementedError()
