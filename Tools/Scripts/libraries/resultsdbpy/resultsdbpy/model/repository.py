# Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

from resultsdbpy.controller.commit import Commit
from webkitscmpy import remote


class Repository(object):

    def __init__(self, key, default_branch=None):
        self.key = key
        self.default_branch = default_branch or 'main'

    def commit_for_id(self, id):
        raise NotImplementedError()

    def url_for_commit(self, commit):
        return None


class StashRepository(Repository):

    def __init__(self, url, key=None, default_branch=None):
        self.remote = remote.BitBucket(url)
        super(StashRepository, self).__init__(
            key=key or self.remote.name,
            default_branch=default_branch or self.remote.default_branch,
        )

    def commit_for_id(self, id):
        commit = self.remote.commit(hash=id, include_identifier=False)
        return Commit(
            repository_id=self.key,
            id=commit.hash,
            branch=commit.branch,
            timestamp=commit.timestamp,
            order=commit.order,
            committer=commit.author.email,
            message=commit.message,
        )

    def url_for_commit(self, commit):
        return f'{self.remote.url}/commits/{commit}'


class WebKitRepository(Repository):

    def __init__(self):
        self.remote = remote.Svn('https://svn.webkit.org/repository/webkit')
        super(WebKitRepository, self).__init__(
            key='webkit',
            default_branch=self.remote.default_branch,
        )

    def commit_for_id(self, id):
        commit = self.remote.commit(revision=id, include_identifier=False)
        return Commit(
            repository_id=self.key,
            id=commit.revision,
            branch=commit.branch,
            timestamp=commit.timestamp,
            order=commit.order,
            committer=commit.author.email,
            message=commit.message,
        )

    def url_for_commit(self, commit):
        return f'https://commits.webkit.org/r{commit}'
