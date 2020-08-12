# Copyright (C) 2019 Apple Inc. All rights reserved.
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

from flask import abort, redirect, request
from resultsdbpy.controller.commit import Commit
from resultsdbpy.flask_support.util import AssertRequest, query_as_kwargs
from resultsdbpy.view.site_menu import SiteMenu


class CommitView(object):
    def __init__(self, environment, commit_controller, site_menu=None):
        self.environment = environment
        self.commit_context = commit_controller.commit_context
        self.commit_controller = commit_controller
        self.site_menu = site_menu

    @query_as_kwargs()
    def _single_commit(self, limit=None, **kwargs):
        AssertRequest.is_type()
        AssertRequest.query_kwargs_empty(limit=limit)

        with self.commit_context:
            commits = self.commit_controller._find(**kwargs)
            if not commits:
                abort(404, description='No commits found matching the specified criteria')
            if len(commits) > 1:
                abort(404, description=f'{len(commits)} commits found matching the specified criteria')
            return commits[0]

    @SiteMenu.render_with_site_menu()
    def commit(self, **kwargs):
        with self.commit_context:
            commit = self._single_commit()
            repositories = list(self.commit_context.repositories.keys())
            repositories.remove(commit.repository_id)
            siblings = self.commit_context.sibling_commits(commit, repositories)
        repositories = [commit.repository_id] + sorted([str(key) for key, lst in siblings.items() if lst])

        return self.environment.get_template('commit.html').render(
            title=self.site_menu.title + ': ' + str(commit.id),
            commit=commit,
            repository_ids=repositories,
            commits=Commit.Encoder().default([commit] + [item for lst in siblings.values() for item in lst]),
            **kwargs)

    def info(self):
        commit = self._single_commit()
        info_url = self.commit_context.repositories[commit.repository_id].url_for_commit(commit.id)
        if not info_url:
            abort(410, description=f'Found commit {len(commit.id)}, but no info url found')
        return redirect(info_url)

    def previous(self):
        with self.commit_context:
            original_commit = self._single_commit()
            commit = self.commit_context.previous_commit(original_commit)
            if not commit:
                abort(404, description=f'{original_commit.id} has no registered previous commit')
        return redirect(f'/commit?repository_id={commit.repository_id}&id={commit.id}')

    def next(self):
        with self.commit_context:
            original_commit = self._single_commit()
            commit = self.commit_context.next_commit(original_commit)
            if not commit:
                abort(404, description=f'{original_commit.id} has no registered subsequent commit')
            return redirect(f'/commit?repository_id={commit.repository_id}&id={commit.id}')

    @SiteMenu.render_with_site_menu()
    def commits(self, **kwargs):

        return self.environment.get_template('commits.html').render(
            title=self.site_menu.title + ': Commits',
            query=request.query_string,
            **kwargs)
