# Copyright (C) 2022 Apple Inc. All rights reserved.
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

from functools import reduce

from resultsdbpy.model.test_context import Expectations
from resultsdbpy.controller.configuration import Configuration


def translate_selected_dots_to_bug_title_and_description(commit_context, selected_rows, test, suite, repositories, will_filter_expected):

    failed_scopes = []
    for selected_row in selected_rows:
        config = selected_row['config']
        results = selected_row['results']
        init_failure_type, init_failure_number = Expectations.get_test_result_status(results[0], will_filter_expected)
        if not init_failure_type:
            continue
        sucess_results = list(filter(lambda result: not Expectations.get_test_result_status(result, will_filter_expected)[0], results))
        if len(sucess_results):
            last_sucess = sucess_results[0]
        else:
            last_sucess = None
        failed_scopes.append((results[0], init_failure_type, init_failure_number, last_sucess, config))

    if len(failed_scopes) == 0:
        raise ValueError('No failures dectected')

    title_components = set()
    description_components = []

    for failed_scope in failed_scopes:
        result, init_failure_type, init_failure_number, last_sucess, config = failed_scope
        version_name = config['version_name'] if 'version_name' in config else Configuration.integer_to_version(config['version'])
        if version_name and config['model']:
            title_components.add('{} on {}({})'.format(init_failure_type, version_name, config['model']))
        else:
            title_components.add('{} on {}'.format(init_failure_type, (version_name if version_name else config['model'])))

        if init_failure_number is not None:
            description_components.append('{} {}'.format(init_failure_number, init_failure_type))
        description_components.append('Hardware:     \t{}'.format(config['model']))
        description_components.append('Architecture: \t{}'.format(config['architecture']))
        description_components.append('OS:           \t{}'.format(version_name))
        description_components.append('Style:        \t{}'.format(config['style']))
        if suite == 'layout-tests':
            description_components.append('Flavor        \t{}'.format(config['flavor']))
        if 'sdk' in config:
            description_components.append('SDK:          \t{}'.format(config['sdk']))
        description_components.append('-----------------------------')
        failure_commits = reduce(
            lambda a, b: a + b,
            map(lambda repo: commit_context.find_commits_by_uuid(repo, 'main', result['uuid']),
                repositories)
        )
        description_components.append('Most recent failures:')
        description_components += list(
            map(lambda commit: '{}: {}'.format(commit.identifier, commit_context.url(commit)),
                failure_commits)
        )
        description_components.append('-----------------------------')
        if last_sucess:
            description_components.append('Last success:')
            last_sucess_commits = reduce(
                lambda a, b: a + b,
                map(lambda repo: commit_context.find_commits_by_uuid(repo, 'main', last_sucess['uuid']),
                    repositories)
            )
            description_components += list(
                map(lambda commit: '{}: {}'.format(commit.identifier, commit_context.url(commit)),
                    last_sucess_commits)
            )
    return title_components, description_components
