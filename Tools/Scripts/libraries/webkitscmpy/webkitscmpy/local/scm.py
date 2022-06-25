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

import json
import os
import re
import six

from webkitbugspy import Tracker, bugzilla, github, radar
from webkitcorepy import decorators
from webkitscmpy import ScmBase, Contributor


class Scm(ScmBase):
    # Projects can define for themselves what constitutes a development vs a production branch,
    # the following idioms seem common enough to be shared.
    DEV_BRANCHES = re.compile(r'.*[(eng)(dev)(bug)]/.+')
    PROD_BRANCHES = re.compile(r'\S+-[\d+\.]+-branch')

    @classmethod
    def executable(cls, program):
        # TODO: Use shutil directly when Python 2.7 is removed
        from whichcraft import which
        path = which(program)
        if path is None:
            raise OSError("Cannot find '{}' program".format(program))
        return os.path.realpath(path)

    @classmethod
    def from_path(cls, path, contributors=None, **kwargs):
        from webkitscmpy import local

        if local.Git.is_checkout(path):
            return local.Git(path, contributors=contributors, **kwargs)
        if local.Svn.is_checkout(path):
            return local.Svn(path, contributors=contributors, **kwargs)
        raise OSError("'{}' is not a known SCM type".format(path))

    def __init__(self, path, dev_branches=None, prod_branches=None, contributors=None, id=None):
        if not isinstance(path, six.string_types):
            raise ValueError("Expected 'path' to be a string type, not '{}'".format(type(path)))
        self.path = path

        root_path = self.root_path
        if not contributors and self.metadata:
            for candidate in [
                os.path.join(self.metadata, 'contributors.json'),
            ]:
                if not os.path.isfile(candidate):
                    continue
                with open(candidate, 'r') as file:
                    contributors = Contributor.Mapping.load(file)

        super(Scm, self).__init__(dev_branches=dev_branches, prod_branches=prod_branches, contributors=contributors, id=id)

        trackers = []
        if self.metadata:
            path = os.path.join(self.metadata, 'trackers.json')
            if os.path.isfile(path):
                with open(path, 'r') as file:
                    trackers = Tracker.from_json(json.load(file))
        for tracker in trackers:
            if isinstance(tracker, radar.Tracker) and (not tracker.radarclient() or not tracker.client):
                continue
            for contributor in self.contributors:
                if contributor.name and contributor.emails:
                    username = None
                    if isinstance(tracker, bugzilla.Tracker):
                        username = contributor.emails[0]
                    if isinstance(tracker, github.Tracker):
                        username = contributor.github
                    tracker.users.create(name=contributor.name, username=username, emails=contributor.emails)
            Tracker.register(tracker)

    @property
    @decorators.Memoize()
    def metadata(self):
        if not self.root_path:
            return None
        for name in ('metadata', '.repo-metadata'):
            candidate = os.path.join(self.root_path, name)
            if os.path.isdir(candidate):
                return candidate
        return None

    @property
    def root_path(self):
        raise NotImplementedError()

    @property
    def common_directory(self):
        raise NotImplementedError()

    @property
    def branch(self):
        raise NotImplementedError()

    def remote(self, name=None):
        raise NotImplementedError()

    def checkout(self, argument):
        raise NotImplementedError()

    def pull(self):
        raise NotImplementedError()
