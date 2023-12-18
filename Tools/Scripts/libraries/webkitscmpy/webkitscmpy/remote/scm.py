# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

import re

from webkitscmpy.scm_base import ScmBase
from webkitcorepy import string_utils


class Scm(ScmBase):
    EMAIL_RE = re.compile(r'(?P<email>[^@]+@[^@]+)(@.*)?')

    class PRGenerator(object):
        SUPPORTS_DRAFTS = False

        def __init__(self, repository):
            self.repository = repository

        def get(self, number):
            raise NotImplementedError()

        def find(self, opened=True, head=None, base=None):
            raise NotImplementedError()

        def create(self, head, title, body=None, commits=None, base=None, draft=None):
            raise NotImplementedError()

        def update(self, pull_request, head=None, title=None, body=None, commits=None, base=None, opened=None, draft=None):
            raise NotImplementedError()

        def reviewers(self, pull_request):
            raise NotImplementedError()

        def comment(self, pull_request, content):
            raise NotImplementedError()

        def comments(self, pull_request):
            raise NotImplementedError()

        def review(self, pull_request, comment=None, approve=None):
            raise NotImplementedError()

        def statuses(self, pull_request):
            raise NotImplementedError()


    @classmethod
    def from_url(cls, url, contributors=None, classifier=None):
        from webkitscmpy import remote

        if 'bitbucket' in url or 'stash' in url:
            match = re.match(r'(?P<protocol>https?)://(?P<host>[^/]+)/(projects/)?(?P<project>[^/]+)/(repos/)?(?P<repo>[^/]+)', url)
            url = '{}://{}/projects/{}/repos/{}'.format(
                match.group('protocol'),
                match.group('host'),
                match.group('project').upper(),
                match.group('repo'),
            )

        for candidate in [remote.Svn, remote.GitHub, remote.BitBucket]:
            if candidate.is_webserver(url):
                return candidate(url, contributors=contributors, classifier=classifier)

        raise OSError("'{}' is not a known SCM server".format(url))

    def __init__(self, url, dev_branches=None, prod_branches=None, contributors=None, id=None, classifier=None):
        super(Scm, self).__init__(
            dev_branches=dev_branches,
            prod_branches=prod_branches,
            contributors=contributors,
            id=id,
            classifier=classifier,
        )

        if not isinstance(url, string_utils.basestring):
            raise ValueError("Expected 'url' to be a string type, not '{}'".format(type(url)))
        self.url = url
        self.pull_requests = None

    def checkout_url(self, ssh=False, http=False):
        raise NotImplementedError()
