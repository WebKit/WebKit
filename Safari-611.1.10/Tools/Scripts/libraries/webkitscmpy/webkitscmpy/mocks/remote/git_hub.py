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

from datetime import datetime
from webkitcorepy import mocks
from webkitscmpy import Commit, remote as scmremote


class GitHub(mocks.Requests):
    top = None

    def __init__(
        self, remote='github.example.com/WebKit/webkit', datafile=None,
        default_branch='main', git_svn=False,
    ):
        if not scmremote.GitHub.is_webserver('https://{}'.format(remote)):
            raise ValueError('"{}" is not a valid GitHub remote'.format(remote))

        self.default_branch = default_branch
        self.remote = remote
        hostname = self.remote.split('/')[0]
        self.api_remote = 'api.{hostname}/repos/{repo}'.format(
            hostname=hostname,
            repo='/'.join(self.remote.split('/')[1:]),
        )

        super(GitHub, self).__init__(hostname, 'api.{}'.format(hostname))

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
        if delta < commit.branch_point:
            return self.commits[self.default_branch][commit.branch_point - delta - 1]
        return None

    def _api_response(self, url):
        return mocks.Response.fromJson({
            'id': 1,
            'name': self.remote.split('/')[2],
            'url': url,
            'default_branch': self.default_branch,
            'owner': {
                'login': self.remote.split('/')[1],
            }, 'organization': {
                'login': self.remote.split('/')[1],
            }, 'html_url': self.remote,
        }, url=url)

    def _list_refs_response(self, url, type):
        return mocks.Response.fromJson([
            {
                'name': reference,
                'commit': {
                    'sha': commit[-1].hash if isinstance(commit, list) else commit.hash,
                    'url': 'https://{}/commits/{}'.format(self.api_remote,
                                                          commit[-1].hash if isinstance(commit, list) else commit.hash),
                },
            } for reference, commit in (self.commits if type == 'branches' else self.tags).items()
        ], url=url)

    def _commit_response(self, url, ref):
        commit = self.commit(ref)
        if not commit:
            return mocks.Response(
                status_code=404,
                url=url,
                text=json.dumps(dict(message='No commit found for SHA: {}'.format(ref))),
            )
        return mocks.Response.fromJson({
            'sha': commit.hash,
            'commit': {
                'author': {
                    'name': commit.author.name,
                    'email': commit.author.email,
                    'date': datetime.fromtimestamp(commit.timestamp).strftime('%Y-%m-%dT%H:%M:%SZ'),
                }, 'committer': {
                    'name': commit.author.name,
                    'email': commit.author.email,
                    'date': datetime.fromtimestamp(commit.timestamp).strftime('%Y-%m-%dT%H:%M:%SZ'),
                }, 'message': commit.message,
                'url': 'https://{}/git/commits/{}'.format(self.api_remote, commit.hash),
            }, 'url': 'https://{}/commits/{}'.format(self.api_remote, commit.hash),
            'html_url': 'https://{}/commit/{}'.format(self.remote, commit.hash),
        }, url=url)

    def _compare_response(self, url, ref_a, ref_b):
        commit_a = self.commit(ref_a)
        commit_b = self.commit(ref_b)
        if not commit_a or not commit_b:
            return mocks.Response(
                status_code=404,
                url=url,
                text=json.dumps(dict(message='Not found')),
            )

        if commit_a.branch != self.default_branch or commit_b.branch == self.default_branch:
            raise NotImplementedError(
                'This is a valid comparison command, but has not been implemented in the mock GitHub API')

        return mocks.Response.fromJson({
            'status': 'diverged',
            'ahead_by': commit_b.identifier,
            'behind_by': commit_a.identifier - commit_b.branch_point,
        }, url=url)

    def _branches_for_request(self, url, ref):
        commit = self.commit(ref)
        if not commit:
            return mocks.Response.create404(url)
        return mocks.Response.fromText(
            '\n\n    <svg class="octicon octicon-git-branch"><path></path></svg>\n'
            '    <ul class="branches-list">\n'
            '        <li class="branch"><a href="/{}">{}</a></li>\n'
            '    </ul>\n'.format(
                '/'.join(self.remote.split('/')[1:]),
                commit.branch,
            ), url=url
        )

    def _parents_of_request(self, url, ref):
        commit = self.commit(ref)
        if not commit:
            return mocks.Response.create404(url)

        # This response is abbreviated since most is unused
        return mocks.Response.fromText(
            '\n\n\n\n<!DOCTYPE html>\n'
            '<html lang="en">\n'
            '...\n'
            '<div class="Box mb-3">\n'
            '    <div class="Box-header Box-header--blue position-relative">\n'
            '        <h2 class="sr-only">Latest commit</h2>\n'
            '        <div class="..." data-issue-and-pr-hovercards-enabled>\n'
            '            <include-fragment src="/WebKit/webkit/tree-commit/{ref}" aria-label="Loading latest commit">\n'
            '            </include-fragment>\n'
            '        <ul class="list-style-none d-flex">\n'
            '            <li class="ml-0 ml-md-3">\n'
            '                <a data-pjax href="/WebKit/webkit/commits/{ref}">\n'
            '                    <svg class="octicon octicon-history text-gray"><path></path></svg>\n'
            '                    <span class="d-none d-sm-inline">\n'
            '                        <strong>{count}</strong>\n'
            '                        <span aria-label="Commits on " class="...">commits</span>\n'
            '                        </span>\n'
            '                </a>\n'
            '            </li>\n'
            '        </ul>\n'
            '        </div>\n'
            '    </div>\n'
            '</div>\n'
            '...\n'
            '</html>\n'.format(
                ref=commit.hash,
                count=commit.identifier + (commit.branch_point or 0),
            ), url=url
        )

    def request(self, method, url, data=None, **kwargs):
        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        stripped_url = url.split('://')[-1]

        # Top-level API request
        if stripped_url == self.api_remote:
            return self._api_response(url=url)

        # Branches/Tags
        if stripped_url in ['{}/branches'.format(self.api_remote), '{}/tags'.format(self.api_remote)]:
            return self._list_refs_response(url=url, type=stripped_url.split('/')[-1])

        # Extract single commit
        if stripped_url.startswith('{}/commits/'.format(self.api_remote)):
            return self._commit_response(url=url, ref=stripped_url.split('/')[-1])

        # Compare two commits
        if stripped_url.startswith('{}/compare/'.format(self.api_remote)):
            ref_a, ref_b = stripped_url.split('/')[-1].split('...')
            return self._compare_response(url=url, ref_a=ref_a, ref_b=ref_b)

        # List branches a commit occurs on
        if stripped_url.startswith('{}/branch_commits/'.format(self.remote)):
            return self._branches_for_request(url=url, ref=stripped_url.split('/')[-1])

        # Find the number of parents a commit has
        if stripped_url.startswith('{}/tree/'.format(self.remote)):
            return self._parents_of_request(url=url, ref=stripped_url.split('/')[-1])

        return mocks.Response.create404(url)
