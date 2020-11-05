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


import logging
import os
import re
import six
import sys

from logging import NullHandler
from webkitcorepy import run
from webkitscmpy import log, Commit


class Scm(object):
    class Exception(RuntimeError):
        pass

    # Projects can define for themselves what constitutes a development vs a production branch,
    # the following idioms seem common enough to be shared.
    DEV_BRANCHES = re.compile(r'.*[(eng)(dev)(bug)]/.+')
    PROD_BRANCHES = re.compile(r'\S+-[\d+\.]+-branch')

    @classmethod
    def executable(cls, program):
        for candidate in ['/usr/bin', '/usr/bin/local']:
            candidate = os.path.join(candidate, program)
            if os.path.exists(candidate):
                return candidate

        which = run(['/usr/bin/which', program], capture_output=True, encoding='utf-8')
        if not which.returncode:
            return os.path.realpath(which.stdout.rstrip())
        raise OSError("Cannot find '{}' program".format(program))

    @classmethod
    def from_path(cls, path):
        from webkitscmpy import local

        if local.Git.is_checkout(path):
            return local.Git(path)
        if local.Svn.is_checkout(path):
            return local.Svn(path)
        raise OSError("'{}' is not a known SCM type".format(path))

    def __init__(self, path, dev_branches=None, prod_branches=None):
        if not isinstance(path, str):
            raise ValueError('')
        self.path = path

        self.dev_branches = dev_branches or self.DEV_BRANCHES
        self.prod_branches = prod_branches or self.PROD_BRANCHES

    @property
    def is_svn(self):
        return False

    @property
    def is_git(self):
        return False

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

    def find(self, argument):
        if not isinstance(argument, six.string_types):
            raise ValueError("Expected 'argument' to be a string, not '{}'".format(type(argument)))

        offset = 0
        if '~' in argument:
            for s in argument.split('~')[1:]:
                if s and not s.isdigit():
                    raise ValueError("'{}' is not a valid argument to Scm.find()".format(argument))
                offset += int(s) if s else 1
            argument = argument.split('~')[0]

        if argument == 'HEAD':
            result = self.commit()

        elif argument in self.branches:
            result = self.commit(branch=argument)

        elif argument in self.tags:
            result = self.commit(tag=argument)

        else:
            if offset:
                raise ValueError("'~' offsets are not supported for revisions and identifiers")

            parsed_commit = Commit.parse(argument)
            return self.commit(
                hash=parsed_commit.hash,
                revision=parsed_commit.revision,
                identifier=parsed_commit.identifier,
                branch=parsed_commit.branch,
            )

        if not offset:
            return result

        return self.commit(
            identifier=result.identifier - offset,
            branch=result.branch,
        )

    def checkout(self, argument):
        raise NotImplementedError()

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None):
        raise NotImplementedError()

    def prioritize_branches(self, branches):
        if len(branches) == 1:
            return branches[0]

        default_branch = self.default_branch
        if default_branch in branches:
            return default_branch

        # We don't have enough information to determine a branch. We will attempt to first use the branch specified
        # by the caller, then the one then checkout is currently on. If both those fail, we will pick one of the
        # other branches. We prefer production branches first, then any branch which isn't explicitly labeled a
        # dev branch. We then sort the list of candidate branches and pick the smallest
        filtered_candidates = [candidate for candidate in branches if self.prod_branches.match(candidate)]
        if not filtered_candidates:
            filtered_candidates = [candidate for candidate in branches if not self.dev_branches.match(candidate)]
        if not filtered_candidates:
            filtered_candidates = branches
        return sorted(filtered_candidates)[0]

    @classmethod
    def log(cls, message, level=logging.WARNING):
        if not log.handlers or all([isinstance(handle, NullHandler) for handle in log.handlers]):
            sys.stderr.write(message + '\n')
        else:
            log.log(level, message)
