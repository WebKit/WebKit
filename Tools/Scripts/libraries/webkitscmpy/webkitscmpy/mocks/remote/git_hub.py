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
import time

from webkitcorepy import mocks
from webkitscmpy import Commit, remote as scmremote


class GitHub(mocks.Requests):
    top = None

    def __init__(
        self, remote='github.example.com/WebKit/WebKit', datafile=None,
        default_branch='main', git_svn=False,
    ):
        if not scmremote.GitHub.is_webserver('https://{}'.format(remote)):
            raise ValueError('"{}" is not a valid GitHub remote'.format(remote))

        self.default_branch = default_branch
        self.remote = remote
        self.forks = []
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
        self.pull_requests = []
        self.issues = dict()
        self.users = dict()
        self._environment = None

    def __enter__(self):
        prefix = self.remote.split('/')[0].replace('.', '_').upper()
        username_key = '{}_USERNAME'.format(prefix)
        token_key = '{}_TOKEN'.format(prefix)
        self._environment = {
            username_key: os.environ.get(username_key),
            token_key: os.environ.get(token_key),
        }
        os.environ[username_key] = 'username'
        os.environ[token_key] = 'token'

        return super(GitHub, self).__enter__()

    def __exit__(self, *args, **kwargs):
        result = super(GitHub, self).__exit__(*args, **kwargs)
        for key in self._environment.keys():
            if self._environment[key]:
                os.environ[key] = self._environment[key]
            else:
                del os.environ[key]
        return result

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

    def _commits_response(self, url, ref):
        from datetime import datetime, timedelta

        base = self.commit(ref)
        if not base:
            return mocks.Response(
                status_code=404,
                url=url,
                text=json.dumps(dict(message='No commit found for SHA: {}'.format(ref))),
            )

        response = []
        for branch in [self.default_branch] if base.branch == self.default_branch else [base.branch, self.default_branch]:
            in_range = False
            previous = None
            for commit in reversed(self.commits[branch]):
                if commit.hash == ref:
                    in_range = True
                if not in_range:
                    continue
                previous = commit
                response.append({
                    'sha': commit.hash,
                    'commit': {
                        'author': {
                            'name': commit.author.name,
                            'email': commit.author.email,
                            'date': datetime.utcfromtimestamp(commit.timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%SZ'),
                        }, 'committer': {
                            'name': commit.author.name,
                            'email': commit.author.email,
                            'date': datetime.utcfromtimestamp(commit.timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%SZ'),
                        }, 'message': commit.message + ('\ngit-svn-id: https://svn.example.org/repository/webkit/{}@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n'.format(
                            'trunk' if commit.branch == self.default_branch else commit.branch, commit.revision,
                        ) if commit.revision else ''),
                        'url': 'https://{}/git/commits/{}'.format(self.api_remote, commit.hash),
                    }, 'url': 'https://{}/commits/{}'.format(self.api_remote, commit.hash),
                    'html_url': 'https://{}/commit/{}'.format(self.remote, commit.hash),
                })
            if branch != self.default_branch:
                for commit in reversed(self.commits[self.default_branch]):
                    if previous.branch_point == commit.identifier:
                        ref = commit.hash

        return mocks.Response.fromJson(response, url=url)

    def _commit_response(self, url, ref):
        from datetime import datetime, timedelta

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
                    'date': datetime.utcfromtimestamp(commit.timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%SZ'),
                }, 'committer': {
                    'name': commit.author.name,
                    'email': commit.author.email,
                    'date': datetime.utcfromtimestamp(commit.timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%SZ'),
                }, 'message': commit.message + ('\ngit-svn-id: https://svn.example.org/repository/webkit/{}@{} 268f45cc-cd09-0410-ab3c-d52691b4dbfc\n'.format(
                    'trunk' if commit.branch == self.default_branch else commit.branch, commit.revision,
                ) if commit.revision else ''),
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
            '            <include-fragment src="/{project}/tree-commit/{ref}" aria-label="Loading latest commit">\n'
            '            </include-fragment>\n'
            '        <ul class="list-style-none d-flex">\n'
            '            <li class="ml-0 ml-md-3">\n'
            '                <a data-pjax href="/{project}/commits/{ref}">\n'
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
                project='/'.join(self.remote.split('/')[1:]),
                ref=commit.hash,
                count=commit.identifier + (commit.branch_point or 0),
            ), url=url
        )

    def _users(self, url, username):
        user = self.users.get(username)
        if not user:
            return mocks.Response.create404(url)
        return mocks.Response.fromJson(dict(
            name=user.name,
            email=user.email,
        ), url=url)

    def request(self, method, url, data=None, params=None, auth=None, json=None, **kwargs):
        from datetime import datetime, timedelta

        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        params = params or {}
        stripped_url = url.split('://')[-1]

        # Top-level API request
        if stripped_url == self.api_remote:
            return self._api_response(url=url)

        # Branches/Tags
        if stripped_url in ['{}/branches'.format(self.api_remote), '{}/tags'.format(self.api_remote)]:
            return self._list_refs_response(url=url, type=stripped_url.split('/')[-1])

        # Return a commit and it's parents
        if stripped_url == '{}/commits'.format(self.api_remote) and params.get('sha'):
            return self._commits_response(url=url, ref=params['sha'])

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

        # Check for existance of forked repo
        if stripped_url.startswith('{}/repos'.format(self.api_remote.split('/')[0])) and stripped_url.split('/')[-1] == self.remote.split('/')[-1]:
            username = stripped_url.split('/')[-2]
            if username in self.forks or username == self.remote.split('/')[-2]:
                return mocks.Response.fromJson(dict(
                    owmer=dict(
                        login=username,
                    ), fork=username in self.forks,
                    name=stripped_url.split('/')[-1],
                    full_name='/'.join(stripped_url.split('/')[-2:]),
                ), url=url)
            return mocks.Response.create404(url)

        # Add fork
        if stripped_url.startswith('{}/forks'.format(self.api_remote)) and method == 'POST':
            username = (json or {}).get('owner', None)
            if username:
                self.forks.append(username)
            return mocks.Response.fromJson({}, url=url) if username else mocks.Response.create404(url)

        # All pull-requests
        pr_base = '{}/pulls'.format(self.api_remote)
        if method == 'GET' and stripped_url == pr_base:
            prs = []
            for candidate in self.pull_requests:
                state = params.get('state', 'all')
                if state != 'all' and candidate.get('state', 'closed') != state:
                    continue
                base = params.get('base')
                if base and candidate.get('base', {}).get('ref') != base:
                    continue
                head = params.get('head')
                if head and head not in [candidate.get('head', {}).get('ref'), candidate.get('head', {}).get('label')]:
                    continue
                prs.append(candidate)
            return mocks.Response.fromJson(prs, url=url)

        # Pull-request by number
        if method == 'GET' and stripped_url.startswith(pr_base):
            for candidate in self.pull_requests:
                if stripped_url.split('/')[5] == str(candidate['number']):
                    if len(stripped_url.split('/')) == 7:
                        if stripped_url.split('/')[6] == 'requested_reviewers':
                            return mocks.Response.fromJson(dict(users=candidate.get('requested_reviews', [])))
                        if stripped_url.split('/')[6] == 'reviews':
                            return mocks.Response.fromJson(candidate.get('reviews', []))
                        return mocks.Response.create404(url)
                    return mocks.Response.fromJson({
                        key: value for key, value in candidate.items() if key not in ('requested_reviews', 'reviews')
                    }, url=url)
            return mocks.Response.create404(url)

        # Create/update pull-request
        pr = dict()
        if method == 'POST' and auth and stripped_url.startswith(pr_base):
            if json.get('title'):
                pr['title'] = json['title']
            if json.get('body'):
                pr['body'] = json['body']
            if json.get('head'):
                pr['head'] = dict(
                    label=json['head'],
                    ref=json['head'].split(':')[-1],
                    user=dict(login=auth.username),
                )
            if json.get('base'):
                pr['base'] = dict(
                    label='{}:{}'.format(self.remote.split('/')[-2], json['base']),
                    ref=json['base'],
                    user=dict(login=self.remote.split('/')[-2]),
                )
            if json.get('state'):
                pr['state'] = json.get('state')

        # Create specifically
        if method == 'POST' and auth and stripped_url == pr_base:
            pr['number'] = 1 + max([0] + [pr.get('number', 0) for pr in self.pull_requests])
            pr['user'] = dict(login=auth.username)
            pr['_links'] = dict(issue=dict(href='https://{}/issues/{}'.format(self.api_remote, pr['number'])))
            self.pull_requests.append(pr)
            if int(pr['number']) not in self.issues:
                self.issues[int(pr['number'])] = dict(
                    comments=[],
                    assignees=[],
                )
            return mocks.Response.fromJson(pr, url=url)

        # Update specifically
        if method == 'POST' and auth and stripped_url.startswith(pr_base):
            number = int(stripped_url.split('/')[-1])
            existing = None
            for i in range(len(self.pull_requests)):
                if self.pull_requests[i].get('number') == number:
                    existing = i
            if existing is None:
                return mocks.Response.create404(url)
            self.pull_requests[existing].update(pr)
            return mocks.Response.fromJson(self.pull_requests[existing], url=url)

        # Access user
        if method == 'GET' and stripped_url.startswith('{}/users'.format(self.api_remote.split('/')[0])):
            return self._users(url, stripped_url.split('/')[-1])

        # Access underlying issue
        if stripped_url.startswith('{}/issues/'.format(self.api_remote)):
            number = int(stripped_url.split('/')[5])
            issue = self.issues.get(number, dict(comments=[]))
            if method == 'GET' and stripped_url.split('/')[6] == 'comments':
                return mocks.Response.fromJson(issue['comments'], url=url)
            if method == 'POST' and stripped_url.split('/')[6] == 'comments':
                self.issues[number] = issue
                now = datetime.utcfromtimestamp(int(time.time()) - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%SZ')
                self.issues[number]['comments'].append(dict(
                    user=dict(login=auth.username),
                    created_at=now, updated_at=now,
                    body=json.get('body', ''),
                ))
                return mocks.Response.fromJson(issue['comments'], url=url)
            if method == 'POST' and stripped_url.split('/')[6] == 'assignees':
                self.issues[number]['assignees'] = {'login': name for name in json.get('assignees', [])}
                return mocks.Response.fromJson(issue['assignees'], url=url)
            return mocks.Response.create404(url)

        return mocks.Response.create404(url)
