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

import os
import json

from webkitcorepy import mocks
from webkitscmpy import Commit, remote as scmremote


class BitBucket(mocks.Requests):
    top = None

    def __init__(
        self, remote='bitbucket.example.com/projects/WEBKIT/repos/webkit', datafile=None,
        default_branch='main', git_svn=False,
    ):
        if not scmremote.BitBucket.is_webserver('https://{}'.format(remote)):
            raise ValueError('"{}" is not a valid BitBucket remote'.format(remote))

        self.default_branch = default_branch
        self.remote = remote
        self.project = '/'.join(remote.split('/')[1:])

        super(BitBucket, self).__init__(self.remote.split('/')[0])

        with open(datafile or os.path.join(os.path.dirname(os.path.dirname(__file__)), 'git-repo.json')) as file:
            self.commits = json.load(file)
        for key, commits in self.commits.items():
            self.commits[key] = [Commit(**kwargs) for kwargs in commits]
            if not git_svn:
                for commit in self.commits[key]:
                    commit.revision = None

        self.head = self.commits[self.default_branch][-1]
        self.tags = {}

    def commit(self, ref):
        if ref in self.commits:
            return self.commits[ref][-1]
        if ref in self.tags:
            return self.tags[ref]

        for branch, commits in self.commits.items():
            for commit in commits:
                if commit.hash.startswith(ref):
                    return commit

        if '~' not in ref:
            return None
        ref, delta = ref.split('~')
        commit = self.commit(ref)
        if not commit:
            return None
        delta = int(delta)

        if delta < commit.identifier:
            return self.commits[commit.branch][commit.identifier - delta - 1]
        delta -= commit.identifier
        if commit.branch_point and delta < commit.branch_point:
            return self.commits[self.default_branch][commit.branch_point - delta - 1]
        return None

    def _branches_default(self, url):
        recent = self.commit(self.default_branch)
        return mocks.Response.fromJson(dict(
            id='refs/heads/{}'.format(self.default_branch),
            displayId=self.default_branch,
            type='BRANCH',
            latestCommit=recent.hash,
            latestChangeset=recent.hash,
            isDefault=True,
        ), url=url)

    def _branches(self, url, params):
        limit = params.get('limit', 25)
        start = params.get('start', 0)
        branches = [branch for branch in sorted(self.commits.keys())[start * limit: (start + 1) * limit]]

        return mocks.Response.fromJson(dict(
            size=len(branches),
            limit=limit,
            isLastPage=(start + 1) * limit > len(self.commits.keys()),
            start=start,
            nextPageStart=start + limit,
            values=[
                dict(
                    id='refs/heads/{}'.format(branch),
                    displayId=branch,
                    type='BRANCH',
                    isDefault=self.default_branch == branch,
                    latestCommit=self.commits[branch][-1].hash,
                ) for branch in branches
            ],
        ), url=url)

    def _tags(self, url, params):
        limit = params.get('limit', 25)
        start = params.get('start', 0)
        tags = [tag for tag in sorted(self.tags.keys())[start * limit: (start + 1) * limit]]

        return mocks.Response.fromJson(dict(
            size=len(tags),
            limit=limit,
            isLastPage=(start + 1) * limit > len(self.tags.keys()),
            start=start,
            nextPageStart=start + limit,
            values=[
                dict(
                    id='refs/tags/{}'.format(tag),
                    displayId=tag,
                    type='TAG',
                    latestCommit=self.tags[tag].hash,
                ) for tag in tags
            ],
        ), url=url)

    def _branches_for(self, ref, url, params):
        limit = params.get('limit', 25)
        start = params.get('start', 0)
        commit = self.commit(ref)
        if not commit:
            return mocks.Response.create404(url)

        branches = [commit.branch][start * limit: (start + 1) * limit]
        return mocks.Response.fromJson(dict(
            size=len(branches),
            limit=limit,
            isLastPage=(start + 1) * limit > 1,
            start=start,
            nextPageStart=start + limit,
            values=[
                dict(
                    id='refs/tags/{}'.format(branch),
                    displayId=branch,
                    type='BRANCH',
                ) for branch in branches
            ],
        ), url=url)

    def request(self, method, url, data=None, params=None, **kwargs):
        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        stripped_url = url.split('://')[-1]
        if stripped_url == '{}/rest/api/1.0/{}/branches/default'.format(self.hosts[0], self.project):
            return self._branches_default(url)

        if stripped_url == '{}/rest/api/1.0/{}/branches'.format(self.hosts[0], self.project):
            return self._branches(url, params or {})

        if stripped_url == '{}/rest/api/1.0/{}/tags'.format(self.hosts[0], self.project):
            return self._tags(url, params or {})

        if stripped_url.startswith('{}/rest/api/1.0/{}/commits/'.format(self.hosts[0], self.project)):
            commit = self.commit(stripped_url.split('/')[-1])
            if not commit:
                return mocks.Response.create404(url)
            return mocks.Response.fromJson(dict(
                id=commit.hash,
                displayId=commit.hash[:12],
                author=dict(
                    emailAddress=commit.author.email,
                    displayName=commit.author.name,
                ), committer=dict(
                    emailAddress=commit.author.email,
                    displayName=commit.author.name,
                ),
                committerTimestamp=commit.timestamp * 1000,
                message=commit.message + ('\ngit-svn-id: https://svn.example.org/repository/webkit/{}@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n'.format(
                    'trunk' if commit.branch == self.default_branch else commit.branch, commit.revision,
                ) if commit.revision else ''),
            ))

        if stripped_url.startswith('{}/rest/branch-utils/latest/{}/branches/info/'.format(self.hosts[0], self.project)):
            return self._branches_for(stripped_url.split('/')[-1], url, params or {})

        return mocks.Response.create404(url)
