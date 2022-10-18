# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import re
import six
import sys
import time

from datetime import datetime
from logging import NullHandler
from webkitscmpy import Commit, Contributor, log


class ScmBase(object):
    class Exception(RuntimeError):
        pass

    # Projects can define for themselves what constitutes a development vs a production branch,
    # the following idioms seem common enough to be shared.
    DEV_BRANCHES = re.compile(r'.*[(eng)(dev)(bug)]/.+')
    PROD_BRANCHES = re.compile(r'\S+-[\d+\.]+-branch')
    GIT_SVN_REVISION = re.compile(r'^git-svn-id: \S+:\/\/.+@(?P<revision>\d+) .+-.+-.+-.+', flags=re.MULTILINE)
    DEFAULT_BRANCHES = ['main', 'master', 'trunk']

    @classmethod
    def gmtoffset(cls):
        if sys.version_info >= (3, 0):
            return int(time.localtime().tm_gmtoff * 100 / (60 * 60))

        ts = time.time()
        return int((datetime.fromtimestamp(ts) - datetime.utcfromtimestamp(ts)).total_seconds() * 100 / (60 * 60))

    def __init__(self, dev_branches=None, prod_branches=None, contributors=None, id=None):
        self.dev_branches = dev_branches or self.DEV_BRANCHES
        self.prod_branches = prod_branches or self.PROD_BRANCHES
        self.path = getattr(self, 'path', None)
        self.contributors = Contributor.Mapping() if contributors is None else contributors

        if id and not isinstance(id, six.string_types):
            raise ValueError("Expected 'id' to be a string type, not '{}'".format(type(id)))
        self.id = id

    @property
    def is_svn(self):
        return False

    @property
    def is_git(self):
        return False

    @property
    def default_branch(self):
        raise NotImplementedError()

    @property
    def branches(self):
        raise NotImplementedError()

    def tags(self):
        raise NotImplementedError()

    def commit(self, hash=None, revision=None, identifier=None, branch=None, tag=None, include_log=True, include_identifier=True):
        raise NotImplementedError()

    def _commit_range(self, begin=None, end=None, include_log=False, include_identifier=True):
        begin_args = begin or dict()
        end_args = end or dict()

        if not begin_args:
            raise TypeError("_commit_range() missing required 'begin' arguments")
        if not end_args:
            raise TypeError("_commit_range() missing required 'end' arguments")

        if list(begin_args.keys()) == ['argument']:
            begin_result = self.find(include_log=include_log, include_identifier=False, **begin_args)
        else:
            begin_result = self.commit(include_log=include_log, include_identifier=False, **begin_args)

        if list(end_args.keys()) == ['argument']:
            end_result = self.find(include_log=include_log, include_identifier=include_identifier, **end_args)
        else:
            end_result = self.commit(include_log=include_log, include_identifier=include_identifier, **end_args)

        if not begin_result:
            raise TypeError("'{}' failed to define begin in _commit_range()".format(begin_args))
        if not end_result:
            raise TypeError("'{}' failed to define begin in _commit_range()".format(end_args))
        if begin_result.timestamp > end_result.timestamp:
            raise TypeError("'{}' pre-dates '{}' in _commit_range()".format(begin_result, end_result))
        if end_result.branch == self.default_branch and begin_result.branch != self.default_branch:
            raise TypeError("'{}' and '{}' do not share linear history".format(begin_result, end_result))
        return begin_result, end_result

    def commits(self, begin=None, end=None, include_log=True, include_identifier=True):
        raise NotImplementedError()

    def prioritize_branches(self, branches, preferred_branch=None):
        if len(branches) == 1:
            return branches[0]

        default_branch = self.default_branch
        if default_branch in branches:
            return default_branch

        # We don't have enough information to determine a branch. We prefer production branches first,
        # then any branch which isn't explicitly labeled a dev branch. If there are only dev branches,
        # then we prefer the branch specified by the caller (usually the currently checked out branch).
        # We then sort the list of candidate branches and pick the smallest.

        filtered_candidates = [candidate for candidate in branches if self.prod_branches.match(candidate)]
        if not filtered_candidates:
            filtered_candidates = [candidate for candidate in branches if not self.dev_branches.match(candidate)]
        if not filtered_candidates:
            if preferred_branch and preferred_branch in branches:
                return preferred_branch
            filtered_candidates = branches
        return sorted(filtered_candidates)[0]

    def find(self, argument, include_log=True, include_identifier=True):
        if not isinstance(argument, six.string_types):
            raise ValueError("Expected 'argument' to be a string, not '{}'".format(type(argument)))

        offset = 0
        if '~' in argument:
            for s in argument.split('~')[1:]:
                if s and not s.isdigit():
                    raise ValueError("'{}' is not a valid argument to Scm.find()".format(argument))
                offset += int(s) if s else 1
            argument = argument.split('~')[0]

        if argument in self.DEFAULT_BRANCHES:
            argument = self.default_branch

        if argument == 'HEAD':
            result = self.commit(include_log=include_log, include_identifier=include_identifier)

        elif argument in self.branches:
            result = self.commit(branch=argument, include_log=include_log, include_identifier=include_identifier)

        elif argument in self.tags():
            result = self.commit(tag=argument, include_log=include_log, include_identifier=include_identifier)

        else:
            if offset:
                raise ValueError("'~' offsets are not supported for revisions and identifiers")

            parsed_commit = Commit.parse(argument)
            if parsed_commit.branch in self.DEFAULT_BRANCHES:
                parsed_commit.branch = self.default_branch

            return self.commit(
                hash=parsed_commit.hash,
                revision=parsed_commit.revision,
                identifier=parsed_commit.identifier,
                branch=parsed_commit.branch,
                include_log=include_log,
                include_identifier=include_identifier,
            )

        if not offset:
            return result

        return self.commit(
            identifier=result.identifier - offset,
            branch=result.branch,
            include_log=include_log,
            include_identifier=include_identifier,
        )

    @classmethod
    def log(cls, message, level=logging.WARNING):
        if not log.handlers or all([isinstance(handle, NullHandler) for handle in log.handlers]):
            sys.stderr.write(message + '\n')
        else:
            log.log(level, message)

    def files_changed(self, argument=None):
        raise NotImplementedError()
