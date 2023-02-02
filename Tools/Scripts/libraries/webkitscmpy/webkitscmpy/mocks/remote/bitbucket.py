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

import os
import json
import time

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
            commit_objs = []
            for kwargs in commits:
                changeFiles = None
                if 'changeFiles' in kwargs:
                    changeFiles = kwargs['changeFiles']
                    del kwargs['changeFiles']
                commit = Commit(**kwargs)
                if changeFiles:
                    setattr(commit, '__mock__changeFiles', changeFiles)
                commit_objs.append(commit)
            self.commits[key] = commit_objs
            if not git_svn:
                for commit in self.commits[key]:
                    commit.revision = None

        self.head = self.commits[self.default_branch][-1]
        self.tags = {}
        self.pull_requests = []

    def resolve_all_commits(self, branch):
        all_commits = self.commits[branch][:]
        last_commit = all_commits[0]
        while last_commit.branch != branch:
            head_index = None
            commits_part = self.commits[last_commit.branch]
            for i in range(len(commits_part)):
                if commits_part[i].hash == last_commit.hash:
                    head_index = i
                    break
            all_commits = commits_part[:head_index] + all_commits
            last_commit = all_commits[0]
            if last_commit.branch == self.default_branch and last_commit.identifier == 1:
                break
        return all_commits

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

        all_commits = self.resolve_all_commits(commit.branch)
        commit_index = 0
        for i in range(len(all_commits)):
            if all_commits[i].hash == commit.hash:
                commit_index = i
                break
        if commit_index - delta >= 0:
            return all_commits[commit_index - delta]
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

    def request(self, method, url, data=None, params=None, json=None, **kwargs):
        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        stripped_url = url.split('://')[-1]
        if stripped_url == '{}/plugins/servlet/applinks/whoami'.format(self.hosts[0]):
            return mocks.Response.fromText('timcommitter')

        if stripped_url == '{}/rest/api/1.0/{}/branches/default'.format(self.hosts[0], self.project):
            return self._branches_default(url)

        if stripped_url == '{}/rest/api/1.0/{}/branches'.format(self.hosts[0], self.project):
            return self._branches(url, params or {})

        if stripped_url == '{}/rest/api/1.0/{}/tags'.format(self.hosts[0], self.project):
            return self._tags(url, params or {})

        if stripped_url.startswith('{}/rest/api/1.0/{}/commits/'.format(self.hosts[0], self.project)):
            if stripped_url.endswith('/changes'):
                # FIXME: All mock commits have the same set of files changed with this implementation
                return mocks.Response.fromJson(dict(
                    values=[dict(
                        path=dict(
                            components=path.split('/'),
                            toString=path,
                        )
                    ) for path in ('Source/main.cpp', 'Source/main.h')],
                ))
            commit = self.commit(stripped_url.split('/')[-1])
            if not commit:
                return mocks.Response.create404(url)
            return mocks.Response.fromJson(dict(
                id=commit.hash,
                displayId=commit.hash[:12],
                author=dict(
                    emailAddress=commit.author.email,
                    displayName=commit.author.name,
                    name=commit.author.name.lower().replace(' ', ''),
                ), committer=dict(
                    emailAddress=commit.author.email,
                    displayName=commit.author.name,
                    name=commit.author.name.lower().replace(' ', ''),
                ),
                committerTimestamp=commit.timestamp * 1000,
                message=commit.message + ('\ngit-svn-id: https://svn.example.org/repository/webkit/{}@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n'.format(
                    'trunk' if commit.branch == self.default_branch else commit.branch, commit.revision,
                ) if commit.revision else ''),
            ))

        if stripped_url.startswith('{}/rest/branch-utils/latest/{}/branches/info/'.format(self.hosts[0], self.project)):
            return self._branches_for(stripped_url.split('/')[-1], url, params or {})

        # All pull-requests
        pr_base = '{}/rest/api/1.0/{}/pull-requests'.format(self.hosts[0], self.project)
        if method == 'GET' and stripped_url == pr_base:
            prs = []
            for candidate in self.pull_requests:
                states = (params or {}).get('state', [])
                states = states if isinstance(states, list) else [states]
                if states and candidate.get('state') not in states:
                    continue
                at = (params or {}).get('at', None)
                if at and candidate.get('fromRef', {}).get('id') != at:
                    continue
                prs.append({key: value for key, value in candidate.items() if key != 'activities'})

            return mocks.Response.fromJson(dict(
                size=len(prs),
                isLastPage=True,
                values=prs,
            ))

        # Create pull-request
        if method == 'POST' and stripped_url == pr_base:
            json['author'] = dict(user=dict(displayName='Tim Committer', emailAddress='committer@webkit.org', name='timcommitter'))
            json['participants'] = [json['author']]
            json['id'] = 1 + max([0] + [pr.get('id', 0) for pr in self.pull_requests])
            json['fromRef']['displayId'] = '/'.join(json['fromRef']['id'].split('/')[-2:])
            json['fromRef']['latestCommit'] = json['fromRef']['latestCommit']
            json['toRef']['displayId'] = '/'.join(json['toRef']['id'].split('/')[-2:])
            json['state'] = 'OPEN'
            json['activities'] = []
            self.pull_requests.append(json)
            return mocks.Response.fromJson(json)

        # Update or access pull-request
        if stripped_url.startswith(pr_base):
            split_url = stripped_url.split('/')
            number = int(split_url[9])
            existing = None
            for i in range(len(self.pull_requests)):
                if self.pull_requests[i].get('id') == number:
                    existing = i
            if existing is None:
                return mocks.Response.create404(url)
            if method == 'PUT' and split_url[-2] == 'participants':
                slug = (json.get('user') or {}).get('name') or split_url[-1]
                for candidate in self.pull_requests[existing]['reviewers']:
                    name = (candidate.get('user') or {}).get('name') or ''
                    display_name = (candidate.get('user') or {}).get('displayName') or ''
                    if name == slug or display_name.lower().replace(' ', '') == slug:
                        reviewer = candidate
                        break
                else:
                    if not self.pull_requests[existing]['reviewers']:
                        self.pull_requests[existing]['reviewers'] = []
                    self.pull_requests[existing]['reviewers'].append(dict(
                        user=json.get('user', dict(name=slug))
                    ))
                    reviewer = self.pull_requests[existing]['reviewers'][-1]
                    reviewer['user']['displayName'] = reviewer['user'].get('displayName', slug)
                reviewer['approved'] = json.get('approved', False)
                reviewer['status'] = json.get('status', 'UNAPPROVED')
                return mocks.Response.fromJson({})
            if method == 'PUT':
                self.pull_requests[existing].update(json)
                self.pull_requests[existing]['fromRef']['displayId'] = '/'.join(json['fromRef']['id'].split('/')[-2:])
                self.pull_requests[existing]['fromRef']['latestCommit'] = json['fromRef']['latestCommit']
                self.pull_requests[existing]['toRef']['displayId'] = '/'.join(json['toRef']['id'].split('/')[-2:])
            if len(split_url) < 11:
                return mocks.Response.fromJson({key: value for key, value in self.pull_requests[existing].items() if key != 'activities'})

            if method == 'GET' and split_url[-1] == 'activities':
                return mocks.Response.fromJson(dict(
                    size=len(self.pull_requests[existing].get('activities', [])),
                    isLastPage=True,
                    values=self.pull_requests[existing].get('activities', []),
                ))
            if method == 'POST' and split_url[-1] == 'comments':
                self.pull_requests[existing]['activities'].append(dict(comment=dict(
                    author=dict(displayName='Tim Committer', emailAddress='committer@webkit.org', name='timcommitter'),
                    createdDate=int(time.time() * 1000),
                    updatedDate=int(time.time() * 1000),
                    text=json.get('text', ''),
                )))
                return mocks.Response.fromJson({})
            if method == 'POST' and split_url[-1] == 'decline':
                self.pull_requests[existing]['open'] = False
                self.pull_requests[existing]['closed'] = True
                self.pull_requests[existing]['state'] = 'DECLINED'
                return mocks.Response.fromJson({})
            if method == 'POST' and split_url[-1] == 'reopen':
                self.pull_requests[existing]['open'] = True
                self.pull_requests[existing]['closed'] = False
                self.pull_requests[existing]['state'] = 'OPEN'
                return mocks.Response.fromJson({})
            return mocks.Response.create404(url)

        return mocks.Response.create404(url)
