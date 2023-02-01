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
import re
import time

import json as jsonlib
from webkitbugspy import mocks as bmocks, Issue
from webkitcorepy import mocks
from webkitscmpy import Commit, remote as scmremote


class GitHub(bmocks.GitHub):
    top = None

    def __init__(
        self, remote='github.example.com/WebKit/WebKit', datafile=None,
        default_branch='main', git_svn=False, environment=None,
        releases=None, issues=None, projects=None, labels=None,
    ):
        if not scmremote.GitHub.is_webserver('https://{}'.format(remote)):
            raise ValueError('"{}" is not a valid GitHub remote'.format(remote))

        self.default_branch = default_branch
        self.remote = remote
        self.forks = []

        super(GitHub, self).__init__(remote, environment=environment, issues=issues, projects=projects, labels=labels)

        with open(datafile or os.path.join(os.path.dirname(os.path.dirname(__file__)), 'git-repo.json')) as file:
            self.commits = jsonlib.load(file)
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
        self.releases = releases or dict()

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
            return mocks.Response.fromJson(
                dict(message='No commit found for SHA: {}'.format(ref)),
                url=url,
                status_code=404,
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
            return mocks.Response.fromJson(
                dict(message='No commit found for SHA: {}'.format(ref)),
                url=url,
                status_code=404,
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
            # FIXME: All mock commits have the same set of files changed with this implementation
            'files': [dict(filename=name) for name in ('Source/main.cpp', 'Source/main.h')],
        }, url=url)

    def _compare_response(self, url, ref_a, ref_b):
        commit_a = self.commit(ref_a)
        commit_b = self.commit(ref_b)
        if not commit_a or not commit_b:
            return mocks.Response.fromJson(
                dict(message='Not found'),
                url=url,
                status_code=404,
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

    # FIXME: Not a very flexible mock of GitHub's GraphQL API, only supports pull-request querying
    def graphql(self, url, auth=None, json=None):
        query = (json or {}).get('query')
        if not query:
            return mocks.Response.create404(url)

        qline = query.splitlines()[1]
        pr_search = re.match(r'\s*search\(query:\s+"(?P<query>.+)",\s+type:\s+ISSUE,\s+last:\s+(?P<last>\d+)\)\s*\{', qline)
        if pr_search:
            query_bits = {}
            for bit in pr_search.group('query').split():
                key, value = bit.split(':')
                if key in query_bits:
                    query_bits[key].append(value)
                else:
                    query_bits[key] = [value]

            repo_name = '/'.join(self.remote.split('/')[-2:])
            if 'pr' not in query_bits.get('is', []) or repo_name not in query_bits.get('repo', []):
                return mocks.Response.fromJson(
                    dict(data=dict(search=dict(edges=[]))),
                    url=url,
                )

            head = query_bits.get('head', [None])[0]
            base = query_bits.get('base', [None])[0]
            state = 'open' if 'open' in query_bits.get('is', []) else None
            state = 'closed' if 'closed' in query_bits.get('is', []) else state

            nodes = []
            for candidate in self.pull_requests:
                chead = candidate.get('head', {}).get('ref', '').split(':')[-1]
                cbase = candidate.get('base', {}).get('ref', '').split(':')[-1]
                if head and chead != head:
                    continue
                if base and cbase != base:
                    continue
                if state and candidate.get('state', 'closed') != state:
                    continue

                nodes.append(dict(node=dict(
                    number=candidate['number'],
                    title=candidate['title'],
                    body=candidate['body'],
                    state=candidate.get('state', 'closed').upper(),
                    isDraft=candidate.get('draft', False),
                    author=dict(login=candidate['user']['login']),
                    baseRefName=cbase,
                    headRefName=chead,
                    headRef=dict(target=dict(oid=(candidate.get('head') or {}).get('sha', None))),
                    headRepository=dict(nameWithOwner='{}/{}'.format(candidate['user']['login'], repo_name.split('/')[-1])),
                )))

            return mocks.Response.fromJson(
                dict(data=dict(search=dict(edges=nodes))),
                url=url,
            )

        return mocks.Response.create404(url)

    def request(self, method, url, data=None, params=None, auth=None, json=None, **kwargs):
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

        # Check for existence of forked repo
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

            return mocks.Response.fromJson(
                dict(message='Not found'),
                url=url,
                status_code=404,
            )

        # Add review
        if method == 'POST' and auth and stripped_url.startswith(pr_base) and stripped_url.endswith('/reviews'):
            for candidate in self.pull_requests:
                if stripped_url.split('/')[5] != str(candidate['number']):
                    continue
                candidate['reviews'].append(
                    dict(user=dict(login=auth.username), state=json.get('event', 'COMMENT')),
                )
                if 'body' in json:
                    self.issues[candidate['number']]['comments'].append(
                        Issue.Comment(user=self.users.get(auth.username), timestamp=int(time.time()), content=json['body']),
                    )
                return mocks.Response.fromJson(candidate['reviews'])

            return mocks.Response.fromJson(
                dict(message='Not found'),
                url=url,
                status_code=404,
            )

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
            if json.get('draft'):
                pr['draft'] = json.get('draft')

        # Create specifically
        if method == 'POST' and auth and stripped_url == pr_base:
            pr['number'] = 1 + max([0] + [pr.get('number', 0) for pr in self.pull_requests])
            pr['state'] = 'open'
            pr['user'] = dict(login=auth.username)
            pr['_links'] = dict(issue=dict(href='https://{}/issues/{}'.format(self.api_remote, pr['number'])))
            pr['draft'] = pr.get('draft', False)
            self.pull_requests.append(pr)
            if pr['number'] not in self.issues:
                self.issues[pr['number']] = dict(
                    creator=self.users.create(username=pr['user']['login']),
                    timestamp=time.time(),
                    assignee=None,
                    comments=[],
                )
            self.issues[pr['number']].update(dict(
                title=pr['title'],
                opened=pr['state'] == 'open',
                description=pr['body'],
            ))
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
            self.issues[number].update(dict(
                title=self.pull_requests[existing]['title'],
                opened=self.pull_requests[existing]['state'] == 'open',
                description=self.pull_requests[existing]['body'],
            ))
            return mocks.Response.fromJson(self.pull_requests[existing], url=url)

        # Releases
        download_base = '{}/releases/download/'.format(self.remote)
        if method == 'GET' and stripped_url.startswith(download_base):
            return self.releases.get(stripped_url[len(download_base):], mocks.Response.create404(url))

        # GraphQL library
        if method == 'POST' and auth and stripped_url == '{}/graphql'.format(self.api_remote.split('/')[0]):
            return self.graphql(url, auth=auth, json=json)

        return super(GitHub, self).request(method, url, data=data, params=params, auth=auth, json=json, **kwargs)
