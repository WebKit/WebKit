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

import json
import os
import re

from collections import OrderedDict
from webkitcorepy import mocks
from webkitscmpy import Commit, Contributor, remote as scmremote


class Svn(mocks.Requests):
    top = None
    remotes = []
    REVISION_REQUEST_RE = re.compile(r'svn/rvr/(?P<revision>\d+)(/(?P<category>\S+))?$')

    def __init__(self, remote='svn.example.org/repository/webkit', datafile=None):
        if not scmremote.Svn.is_webserver('https://{}'.format(remote)):
            raise ValueError('"{}" is not a valid Svn remote'.format(remote))

        super(Svn, self).__init__(remote.split('/')[0])
        if remote[-1] != '/':
            remote += '/'
        self.remote = remote
        self._cache_contents = None
        self.patches.append(scmremote.Svn('http://{}'.format(self.remote))._cache_lock())

        with open(datafile or os.path.join(os.path.dirname(os.path.dirname(__file__)), 'svn-repo.json')) as file:
            self.commits = json.load(file)
        for key, commits in self.commits.items():
            self.commits[key] = [Commit(**kwargs) for kwargs in commits]

    def __enter__(self):
        super(Svn, self).__enter__()

        self.remotes.append(self.hosts[0])
        cache_path = scmremote.Svn('http://{}'.format(self.remote))._cache_path
        if os.path.isfile(cache_path):
            with open(cache_path, 'r') as cache:
                self._cache_contents = cache.read()
            os.remove(cache_path)

        return self

    def __exit__(self, *args, **kwargs):
        cache_path = scmremote.Svn('http://{}'.format(self.remote))._cache_path
        if os.path.isfile(cache_path):
            os.remove(cache_path)
        if self._cache_contents:
            with open(cache_path, 'w') as cache:
                cache.write(self._cache_contents)

        self.remotes.remove(self.hosts[0])
        super(Svn, self).__exit__(*args, **kwargs)

    def latest(self):
        latest = self.commits['trunk'][-1]
        for branch in self.commits.values():
            for commit in branch:
                if commit.revision > latest.revision:
                    latest = commit
        return latest

    def branches(self, revision=None):
        revision = revision or self.latest()
        branches = set()
        for branch, commits in self.commits.items():
            for commit in commits:
                if commit.revision <= revision and not commit.branch.startswith('tags') and commit.branch != 'trunk':
                    branches.add(commit.branch)
        return sorted(branches)

    def tags(self, revision=None):
        revision = revision or self.latest()
        tags = set()
        for branch, commits in self.commits.items():
            for commit in commits:
                if commit.revision <= revision and commit.branch.startswith('tags'):
                    tags.add(commit.branch)
        return sorted(tags)

    def range(self, category=None, start=None, end=None):
        start = start or self.latest()
        end = end or 1

        if category and category.startswith('branches/'):
            category = category.split('/')[-1]
        category = [category] if category else self.branches(start) + self.tags(start)

        previous = None
        for b in category + ['trunk']:
            for candidate in reversed(self.commits.get(b, [])):
                if candidate.revision > start or candidate.revision < end:
                    continue
                if previous and previous.revision <= candidate.revision:
                    continue
                previous = candidate
                yield candidate

    def request(self, method, url, data=None, **kwargs):
        import xmltodict
        from datetime import datetime, timedelta

        if not url.startswith('http://') and not url.startswith('https://'):
            return mocks.Response.create404(url)

        data = xmltodict.parse(data) if data else None
        stripped_url = url.split('://')[-1]

        # Latest revision
        if method == 'OPTIONS' and stripped_url == self.remote and data == OrderedDict((
            ('D:options', OrderedDict((
                ('@xmlns:D', 'DAV:'),
                ('D:activity-collection-set', None),
            ))),
        )):
            return mocks.Response.fromText(
                url=url,
                data='<?xml version="1.0" encoding="utf-8"?>\n'
                    '<D:options-response xmlns:D="DAV:">\n'
                    '<D:activity-collection-set><D:href>/repository/webkit/!svn/act/</D:href></D:activity-collection-set></D:options-response>\n',
                headers={
                    'SVN-Youngest-Rev': str(self.latest().revision),
                }
            )

        # List category
        match = self.REVISION_REQUEST_RE.match(stripped_url.split('!')[-1])
        if method == 'PROPFIND' and stripped_url.startswith('{}!'.format(self.remote)) and match and data == OrderedDict((
            ('propfind', OrderedDict((
                ('@xmlns', 'DAV:'),
                ('prop', OrderedDict((
                    ('resourcetype', OrderedDict((
                        ('@xmlns', 'DAV:'),
                    ))),
                ))),
            ))),
        )):
            links = [stripped_url[len(stripped_url.split('/')[0]):]]
            if links[0][-1] != '/':
                links[0] += '/'

            if match.group('category') == 'branches':
                for branch in self.branches(int(match.group('revision'))):
                    links.append('{}{}/'.format(links[0], branch))

            elif match.group('category') == 'tags':
                for tag in self.tags(int(match.group('revision'))):
                    links.append('{}{}/'.format(links[0][:-5], tag))

            else:
                return mocks.Response.create404(url)

            return mocks.Response(
                status_code=207,
                url=url,
                text='<?xml version="1.0" encoding="utf-8"?>\n'
                    '<D:multistatus xmlns:D="DAV:" xmlns:ns0="DAV:">\n'
                    '{}</D:multistatus>\n'.format(
                        ''.join([
                            '<D:response xmlns:lp1="DAV:">\n'
                            '<D:href>{}</D:href>\n'
                            '<D:propstat>\n'
                            '<D:prop>\n'
                            '<lp1:resourcetype><D:collection/></lp1:resourcetype>\n'
                            '</D:prop>\n'
                            '<D:status>HTTP/1.1 200 OK</D:status>\n'
                            '</D:propstat>\n'
                            '</D:response>\n'.format(link) for link in links
                        ])),
            )

        # Info for commit, branch or tag
        if method == 'PROPFIND' and stripped_url.startswith('{}!'.format(self.remote)) and match and data == OrderedDict((
            ('propfind', OrderedDict((
                ('@xmlns', 'DAV:'),
                ('prop', OrderedDict((
                    ('resourcetype', OrderedDict((('@xmlns', 'DAV:'),))),
                    ('getcontentlength', OrderedDict((('@xmlns', 'DAV:'),))),
                    ('deadprop-count', OrderedDict((('@xmlns', 'http://subversion.tigris.org/xmlns/dav/'),))),
                    ('version-name', OrderedDict((('@xmlns', 'DAV:'),))),
                    ('creationdate', OrderedDict((('@xmlns', 'DAV:'),))),
                    ('creator-displayname', OrderedDict((('@xmlns', 'DAV:'),))),
                ))),
            ))),
        )):
            branch = match.group('category')
            if branch.startswith('branches'):
                branch = '/'.join(branch.split('/')[1:])

            if branch not in self.commits:
                return mocks.Response.create404(url)
            commit = self.commits[branch][0]
            for candidate in self.commits[branch]:
                if candidate.revision <= int(match.group('revision')):
                    commit = candidate

            stripped_url = stripped_url[len(stripped_url.split('/')[0]):]
            if stripped_url[-1] != '/':
                stripped_url += '/'

            return mocks.Response(
                status_code=207,
                url=url,
                text='<?xml version="1.0" encoding="utf-8"?>\n'
                    '<D:multistatus xmlns:D="DAV:" xmlns:ns0="DAV:">\n'
                    '<D:response xmlns:lp1="DAV:" xmlns:lp3="http://subversion.tigris.org/xmlns/dav/" xmlns:g0="DAV:">\n'
                    '<D:href>{}</D:href>\n'
                    '<D:propstat>\n'
                    '<D:prop>\n'
                    '<lp1:resourcetype><D:collection/></lp1:resourcetype>\n'
                    '<lp3:deadprop-count>1</lp3:deadprop-count>\n'
                    '<lp1:version-name>{}</lp1:version-name>\n'
                    '<lp1:creationdate>{}</lp1:creationdate>\n'
                    '<lp1:creator-displayname>{}</lp1:creator-displayname>\n'
                    '</D:prop>\n'
                    '<D:status>HTTP/1.1 200 OK</D:status>\n'
                    '</D:propstat>\n'
                    '<D:propstat>\n'
                    '<D:prop>\n'
                    '<g0:getcontentlength/>\n'
                    '</D:prop>\n'
                    '<D:status>HTTP/1.1 404 Not Found</D:status>\n'
                    '</D:propstat>\n'
                    '</D:response>\n'
                    '</D:multistatus>\n'.format(
                        stripped_url,
                        commit.revision,
                        datetime.utcfromtimestamp(commit.timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%S.103754Z'),
                        commit.author.email,
                ),
            )

        # Log for commit
        if method == 'REPORT' and stripped_url.startswith('{}!'.format(self.remote)) and match and data.get('S:log-report'):
            commits = list(self.range(
                category=match.group('category'),
                start=int(data['S:log-report']['S:start-revision']),
                end=int(data['S:log-report']['S:end-revision']),
            ))

            limit = int(data['S:log-report'].get('S:limit', 0))
            if limit and len(commits) > limit:
                commits = commits[:limit]

            if not commits:
                return mocks.Response.create404(url)

            return mocks.Response(
                status_code=200,
                url=url,
                text='<?xml version="1.0" encoding="utf-8"?>\n'
                '<S:log-report xmlns:S="svn:" xmlns:D="DAV:">\n'
                '{}</S:log-report>\n'.format(
                    ''.join([
                        '<S:log-item>\n'
                        '<D:version-name>{}</D:version-name>\n'
                        '<S:date>{}</S:date>\n'
                        '{}{}</S:log-item>\n'.format(
                            commit.revision,
                            datetime.utcfromtimestamp(commit.timestamp - timedelta(hours=7).seconds).strftime('%Y-%m-%dT%H:%M:%S.103754Z'),
                            '' if data['S:log-report'].get('S:revpro') else '<D:comment>{}</D:comment>\n'
                            '<D:creator-displayname>{}</D:creator-displayname>\n'.format(
                                commit.message,
                                commit.author.email,
                            ),
                            '<S:modified-path node-kind="file" text-mods="true" prop-mods="false">/{branch}/Changelog</S:modified-path>\n'
                            '<S:modified-path node-kind="file" text-mods="true" prop-mods="false">/{branch}/file.cpp</S:modified-path>\n'
                            '<S:added-path node-kind="file" text-mods="true" prop-mods="false">/{branch}/deleted.cpp</S:added-path>\n'
                            '<S:deleted-path node-kind="file" text-mods="true" prop-mods="false">/{branch}/added.cpp</S:deleted-path>\n'.format(
                                branch=commit.branch if commit.branch.split('/')[0] in ['trunk', 'tags'] else 'branches/{}'.format(commit.branch),
                            ) if 'S:discover-changed-paths' in data['S:log-report'] else '',
                        ) for commit in commits
                    ])),
            )

        return mocks.Response.create404(url)
