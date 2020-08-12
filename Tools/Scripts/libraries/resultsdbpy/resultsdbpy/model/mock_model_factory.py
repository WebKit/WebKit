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

import base64
import io
import time

import calendar
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.model.configuration_context_unittest import ConfigurationContextTest
from resultsdbpy.model.mock_repository import MockStashRepository, MockSVNRepository
from resultsdbpy.model.model import Model


class MockModelFactory(object):
    ARCHIVE_ZIP = """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"""
    THREE_WEEKS = 60 * 60 * 24 * 21

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
            archive_ttl_seconds=time.time() - oldest_commit + Model.TTL_WEEK,
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
            old = current - cls.THREE_WEEKS
            for complete_configuration in configurations:
                if complete_configuration != configuration:
                    continue

                timestamp_to_use = current
                if (complete_configuration.platform == 'Mac' and complete_configuration.version <= Configuration.version_to_integer('10.13')) \
                   or (complete_configuration.platform == 'iOS' and complete_configuration.version <= Configuration.version_to_integer('11')):
                    timestamp_to_use = old

                cls.iterate_all_commits(model, lambda commits: model.upload_context.upload_test_results(complete_configuration, commits, suite=suite, test_results=test_results, timestamp=timestamp_to_use))

    @classmethod
    def process_results(cls, model, configuration=Configuration(), suite='layout-tests'):
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

    @classmethod
    def add_mock_archives(cls, model, configuration=Configuration(), suite='layout-tests', archive=None):
        archive = archive or io.BytesIO(base64.b64decode(cls.ARCHIVE_ZIP))
        configurations = [configuration] if configuration.is_complete() else ConfigurationContextTest.CONFIGURATIONS

        with model.upload_context:
            current = time.time()
            old = current - cls.THREE_WEEKS
            for complete_configuration in configurations:
                if complete_configuration != configuration:
                    continue

                timestamp_to_use = current
                if (complete_configuration.platform == 'Mac' and complete_configuration.version <= Configuration.version_to_integer('10.13')) \
                   or (complete_configuration.platform == 'iOS' and complete_configuration.version <= Configuration.version_to_integer('11')):
                    timestamp_to_use = old

                cls.iterate_all_commits(model, lambda commits: model.archive_context.register(archive, complete_configuration, commits, suite=suite, timestamp=timestamp_to_use))
