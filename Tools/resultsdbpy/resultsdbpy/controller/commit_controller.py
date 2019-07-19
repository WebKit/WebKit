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

import json

from collections import defaultdict
from flask import abort, jsonify, request
from resultsdbpy.flask_support.util import AssertRequest, query_as_kwargs, limit_for_query
from resultsdbpy.model.repository import SCMException
from resultsdbpy.controller.commit import Commit


def _find_comparison(commit_context, repository_id, branch, id, uuid, timestamp, priority=min):
    if bool(id) + bool(uuid) + bool(timestamp) > 1:
        abort(400, description='Can only search by one of [commit id, commit uuid, timestamp] in a single request')

    try:
        if uuid:
            # We don't need real commit to search by uuid and CommitContexts have trouble diffrentiating between uuids and timestamps.
            uuid = priority([int(element) for element in uuid])
            return Commit('?', '?', '?', uuid // Commit.TIMESTAMP_TO_UUID_MULTIPLIER, uuid % Commit.TIMESTAMP_TO_UUID_MULTIPLIER)
        if timestamp:
            return priority([round(float(element)) for element in timestamp])
    except ValueError:
        abort(400, description='Timestamp and uuid must be integers')

    if not repository_id and not id:
        return None
    if not repository_id:
        repository_id = commit_context.repositories.keys()
    for repository in repository_id:
        if repository not in commit_context.repositories.keys():
            abort(404, description=f"\'{repository}\' is not a registered repository")

    result = []
    for repository in repository_id:
        for b in branch:
            if id:
                for elm in id:
                    result += commit_context.find_commits_by_id(repository, b, elm)
            else:
                result += commit_context.find_commits_in_range(repository, b, limit=1)

    if not result:
        abort(404, description='No commits found matching the specified criteria')
    return priority(result)


def uuid_range_for_commit_range_query():
    def decorator(method):
        def real_method(obj, branch=None,
                        after_repository_id=None, after_branch=None, after_id=None, after_uuid=None,
                        after_timestamp=None,
                        before_repository_id=None, before_branch=None, before_id=None, before_uuid=None,
                        before_timestamp=None, **kwargs):
            # We're making an asumption that the class using this decorator actually has a commit_context, if it does not,
            # this decorator will fail spectacularly
            with obj.commit_context:
                if not branch:
                    branch = [None]
                begin = _find_comparison(
                    obj.commit_context, repository_id=after_repository_id, branch=after_branch or branch,
                    id=after_id, uuid=after_uuid, timestamp=after_timestamp, priority=min,
                )
                end = _find_comparison(
                    obj.commit_context, repository_id=before_repository_id, branch=before_branch or branch,
                    id=before_id, uuid=before_uuid, timestamp=before_timestamp, priority=max,
                )
                return method(obj, begin=begin, end=end, branch=branch, **kwargs)

        real_method.__name__ = method.__name__
        return real_method

    return decorator


def uuid_range_for_query():
    def decorator(method):
        @uuid_range_for_commit_range_query()
        def real_method(obj, repository_id=None, branch=None, id=None, uuid=None, timestamp=None, begin=None, end=None, **kwargs):
            # We're making an asumption that the class using this decorator actually has a commit_context, if it does not,
            # this decorator will fail spectacularly
            with obj.commit_context:
                index = _find_comparison(
                    obj.commit_context, repository_id=repository_id, branch=branch,
                    id=id, uuid=uuid, timestamp=timestamp, priority=max,
                )
                if index:
                    if begin or end:
                        abort(400, description='Cannot define commit and range when searching')
                    begin = index
                    end = index

                if isinstance(end, Commit) and end.repository_id != '?':
                    repositories = list(obj.commit_context.repositories.keys())
                    repositories.remove(end.repository_id)
                    commits = [end]
                    [commits.extend(siblings) for siblings in obj.commit_context.sibling_commits(end, repositories).values()]
                    end = max(commits)

                return method(obj, branch=branch, begin=begin, end=end, **kwargs)

        real_method.__name__ = method.__name__
        return real_method

    return decorator


class HasCommitContext(object):
    def __init__(self, commit_context):
        self.commit_context = commit_context


class CommitController(HasCommitContext):
    DEFAULT_LIMIT = 100

    def default(self):
        if request.method == 'POST':
            return self.register()
        return self.find()

    @uuid_range_for_commit_range_query()
    @limit_for_query(DEFAULT_LIMIT)
    def _find(self, repository_id=None, branch=None, id=None, uuid=None, timestamp=None, limit=None, begin=None, end=None, **kwargs):
        AssertRequest.query_kwargs_empty(**kwargs)

        with self.commit_context:
            if not repository_id:
                repository_id = self.commit_context.repositories.keys()
            for repository in repository_id:
                if repository not in self.commit_context.repositories.keys():
                    abort(404, description=f"\'{repository}\' is not a registered repository")
            if not branch:
                branch = [None]

            if bool(id) + bool(uuid) + bool(timestamp) > 1:
                abort(400, description='Can only search by one of [commit id, commit uuid, timestamp] in a single request')

            result = []
            for repository in repository_id:
                # Limit makes most sense on a per-repository basis
                results_for_repo = []

                for b in branch:
                    if len(results_for_repo) >= limit:
                        continue

                    if id:
                        for elm in id:
                            results_for_repo += self.commit_context.find_commits_by_id(repository, b, elm, limit=limit - len(results_for_repo))
                    elif uuid:
                        for elm in uuid:
                            results_for_repo += self.commit_context.find_commits_by_uuid(repository, b, int(elm), limit=limit - len(results_for_repo))
                    elif timestamp:
                        for elm in timestamp:
                            results_for_repo += self.commit_context.find_commits_by_timestamp(repository, b, round(float(elm)), limit=limit - len(results_for_repo))
                    else:
                        results_for_repo += self.commit_context.find_commits_in_range(repository, b, limit=limit - len(results_for_repo), begin=begin, end=end)
                result += results_for_repo

            return sorted(result)

    def find(self):
        AssertRequest.is_type()
        result = self._find(**request.args.to_dict(flat=False))
        if not result:
            abort(404, description='No commits found matching the specified criteria')
        return jsonify(Commit.Encoder().default(result))

    def repositories(self):
        AssertRequest.is_type()
        AssertRequest.no_query()
        return jsonify(sorted(self.commit_context.repositories.keys()))

    @query_as_kwargs()
    @limit_for_query(DEFAULT_LIMIT)
    def branches(self, repository_id=None, branch=None, limit=None, **kwargs):
        AssertRequest.is_type()
        AssertRequest.query_kwargs_empty(**kwargs)

        result = defaultdict(list)
        with self.commit_context:
            for repository in repository_id or self.commit_context.repositories.keys():
                limit_for_repo = limit
                for b in branch or [None]:
                    if not limit_for_repo:
                        continue
                    matching_branches = self.commit_context.branches(repository, branch=b, limit=limit_for_repo)
                    if not matching_branches:
                        continue
                    limit_for_repo -= len(matching_branches)
                    result[repository] += matching_branches

        return jsonify(result)

    @query_as_kwargs()
    def siblings(self, limit=None, **kwargs):
        AssertRequest.is_type()
        AssertRequest.query_kwargs_empty(limit=limit)

        with self.commit_context:
            commits = self._find(**kwargs)
            if not commits:
                abort(404, description='No commits found matching the specified criteria')
            if len(commits) > 1:
                abort(404, description=f'{len(commits)} commits found matching the specified criteria')

            repositories = sorted(self.commit_context.repositories.keys())
            repositories.remove(commits[0].repository_id)
            return jsonify(Commit.Encoder().default(self.commit_context.sibling_commits(commits[0], repositories)))

    @query_as_kwargs()
    def next(self, limit=None, **kwargs):
        AssertRequest.is_type()
        AssertRequest.query_kwargs_empty(limit=limit)

        with self.commit_context:
            commits = self._find(**kwargs)
            if not commits:
                abort(404, description='No commits found matching the specified criteria')
            if len(commits) > 1:
                abort(404, description=f'{len(commits)} commits found matching the specified criteria')

            commit = self.commit_context.next_commit(commits[0])
            return jsonify(Commit.Encoder().default([commit] if commit else []))

    @query_as_kwargs()
    def previous(self, limit=None, **kwargs):
        AssertRequest.is_type()
        AssertRequest.query_kwargs_empty(limit=limit)

        with self.commit_context:
            commits = self._find(**kwargs)
            if not commits:
                abort(404, description='No commits found matching the specified criteria')
            if len(commits) > 1:
                abort(404, description=f'{len(commits)} commits found matching the specified criteria')

            commit = self.commit_context.previous_commit(commits[0])
            return jsonify(Commit.Encoder().default([commit] if commit else []))

    def register(self, commit=None):
        is_endpoint = not bool(commit)
        if is_endpoint:
            AssertRequest.is_type(['POST'])
            AssertRequest.no_query()

        if is_endpoint:
            try:
                commit = request.form or json.loads(request.get_data())
            except ValueError:
                abort(400, description='Expected uploaded data to be json')

        try:
            self.commit_context.register_commit(Commit.from_json(commit))
            if is_endpoint:
                return jsonify({'status': 'ok'})
            return Commit.from_json(commit)
        except ValueError:
            pass

        required_args = ['repository_id', 'id']
        optional_args = ['branch']
        for arg in required_args:
            if arg not in commit:
                abort(400, description=f"'{arg}' required to define commit")

        for arg in commit.keys():
            if arg in required_args + optional_args:
                continue
            if arg in ['timestamp', 'order', 'committer', 'message']:
                abort(400, description='Not enough arguments provided to define a commit, but too many to search for a commit')
            abort(400, description=f"'{arg}' is not valid for defining commits")

        try:
            commit = self.commit_context.register_commit_with_repo_and_id(
                repository_id=commit.get('repository_id'),
                branch=commit.get('branch'),
                commit_id=commit.get('id'),
            )
        except (RuntimeError, SCMException) as error:
            abort(404, description=str(error))

        if is_endpoint:
            return jsonify({'status': 'ok'})
        return commit
