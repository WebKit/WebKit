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

import time

import calendar
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.configuration_context_unittest import ConfigurationContextTest
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.model import Model


class MockModelFactory(object):

    @classmethod
    def create(cls, redis, cassandra, async_processing=False):
        oldest_commit = time.time()
        for repo in [MockStashRepository.safari(), MockSVNRepository.webkit()]:
            for commits in repo.commits.values():
                for commit in commits:
                    oldest_commit = min(oldest_commit, calendar.timegm(commit.timestamp.timetuple()))

        model = Model(
            redis=redis,
            cassandra=cassandra,
            repositories=[
                MockStashRepository.safari(redis=redis),
                MockSVNRepository.webkit(redis=redis),
            ],
            default_ttl_seconds=time.time() - oldest_commit + Model.TTL_WEEK,
            async_processing=async_processing,
        )
        with model.commit_context, model.commit_context.cassandra.batch_query_context():
            for repository in model.commit_context.repositories.values():
                for branch_commits in repository.commits.values():
                    for commit in branch_commits:
                        model.commit_context.register_commit(commit)
        return model

    @classmethod
    def layout_test_results(cls):
        default_result = {'expected': 'PASS', 'modifiers': '', 'actual': 'PASS', 'time': 1.2}
        return dict(
            details=dict(link='dummy-link'),
            run_stats=dict(tests_skipped=0),
            results={
                'fast': {
                    'encoding': {
                        'css-cached-bom.html': default_result,
                        'css-charset-default.xhtml': default_result,
                        'css-charset.html': default_result,
                        'css-link-charset.html': default_result,
                    }
                }
            },
        )

    @classmethod
    def iterate_all_commits(cls, model, callback):
        repos = ('webkit', 'safari')
        branches = (None, 'safari-606-branch')
        for branch in branches:
            commit_index = {repo: 0 for repo in repos}
            commits_for_repo = {repo: sorted(model.commit_context.find_commits_in_range(repo, branch)) for repo in repos}
            with model.upload_context.cassandra.batch_query_context():
                for repo in repos:
                    while max([commits_for_repo[r][commit_index[r]] for r in repos]) > commits_for_repo[repo][commit_index[repo]]:
                        if commit_index[repo] + 1 >= len(commits_for_repo[repo]):
                            break
                        commit_index[repo] += 1

                while True:
                    commits = []
                    for repo in repos:
                        commits.append(commits_for_repo[repo][commit_index[repo]])
                    callback(commits)

                    youngest_next_repo = None
                    for repo in repos:
                        if commit_index[repo] + 1 >= len(commits_for_repo[repo]):
                            continue
                        if not youngest_next_repo:
                            youngest_next_repo = repo
                            continue
                        if commits_for_repo[youngest_next_repo][commit_index[youngest_next_repo] + 1] > commits_for_repo[repo][commit_index[repo] + 1]:
                            youngest_next_repo = repo
                    if not youngest_next_repo:
                        break
                    commit_index[youngest_next_repo] += 1

    @classmethod
    def add_mock_results(cls, model, configuration=Configuration(), suite='layout-tests', test_results=None):
        if test_results is None:
            test_results = cls.layout_test_results()

        configurations = [configuration] if configuration.is_complete() else ConfigurationContextTest.CONFIGURATIONS

        with model.upload_context:
            current = time.time()
            old = current - 60 * 60 * 24 * 21
            for complete_configuration in configurations:
                if complete_configuration != configuration:
                    continue

                timestamp_to_use = current
                if (complete_configuration.platform == 'Mac' and complete_configuration.version <= Configuration.version_to_integer('10.13')) \
                   or (complete_configuration.platform == 'iOS' and complete_configuration.version <= Configuration.version_to_integer('11')):
                    timestamp_to_use = old

                cls.iterate_all_commits(model, lambda commits: model.upload_context.upload_test_results(complete_configuration, commits, suite=suite, test_results=test_results, timestamp=timestamp_to_use))

    @classmethod
    def process_results(self, model, configuration=Configuration(), suite='layout-tests'):
        configurations = [configuration] if configuration.is_complete() else ConfigurationContextTest.CONFIGURATIONS

        with model.upload_context:
            for complete_configuration in configurations:
                if complete_configuration != configuration:
                    continue
                for branch in (None, 'safari-606-branch'):
                    results_dict = model.upload_context.find_test_results(
                        configurations=[complete_configuration], suite=suite,
                        branch=branch, recent=False,
                    )
                    for config, results in results_dict.items():
                        for result in results:
                            model.upload_context.process_test_results(
                                configuration=config, commits=result['commits'], suite=suite,
                                test_results=result['test_results'], timestamp=result['timestamp'],
                            )
