# Copyright (C) 2019, 2020 Apple Inc. All rights reserved.
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

from flask import abort
from resultsdbpy.controller.commit_controller import uuid_range_for_query, HasCommitContext
from resultsdbpy.controller.configuration_controller import configuration_for_query
from resultsdbpy.controller.suite_controller import time_range_for_query
from resultsdbpy.flask_support.util import AssertRequest, query_as_kwargs, limit_for_query, boolean_query
from resultsdbpy.view.site_menu import SiteMenu


class SuiteView(HasCommitContext):
    DEFAULT_LIMIT = 100

    def __init__(self, environment, upload_controller, suite_controller, site_menu=None):
        super(SuiteView, self).__init__(suite_controller.commit_context)
        self.environment = environment

        self.upload_controller = upload_controller
        self.suite_controller = suite_controller
        self.site_menu = site_menu

    # It's not that we actually need the value of all the query arguments here, but we want errors for the wrong
    # query arguments to be dealt with in a user-readable way
    @query_as_kwargs()
    @uuid_range_for_query()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    def _suites_for_configuration(
        self, suite=None, configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None,
        limit=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        with self.upload_controller.upload_context:
            suites_by_configuration = self.upload_controller.upload_context.find_suites(configurations=configurations, recent=boolean_query(*recent)[0] if recent else True)
            candidate_suites = set()
            for suites_for_config in suites_by_configuration.values():
                for s in suites_for_config:
                    candidate_suites.add(s)
            return sorted([s for s in candidate_suites if not suite or s in suite])

    @SiteMenu.render_with_site_menu()
    def results(self, **kwargs):
        return self.environment.get_template('suite_results.html').render(
            title=self.site_menu.title,
            suites=json.dumps(self._suites_for_configuration()),
            **kwargs)

    @query_as_kwargs()
    @uuid_range_for_query()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    def _suites_for_investigation(
        self, suite=None, configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None,
        unexpected=None,
        limit=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        with self.upload_controller.upload_context:
            suites_by_configuration = self.upload_controller.upload_context.find_suites(configurations=configurations, recent=boolean_query(*recent)[0] if recent else True)
            candidate_suites = set()
            for suites_for_config in suites_by_configuration.values():
                for s in suites_for_config:
                    candidate_suites.add(s)
            return sorted([s for s in candidate_suites if not suite or s in suite])

    @SiteMenu.render_with_site_menu()
    def investigate(self, **kwargs):
        return self.environment.get_template('investigate.html').render(
            title=self.site_menu.title,
            suites=json.dumps(self._suites_for_investigation()),
            **kwargs)

    @query_as_kwargs()
    @uuid_range_for_query()
    @limit_for_query(DEFAULT_LIMIT)
    @configuration_for_query()
    @time_range_for_query()
    def _suites_for_search(
        self, suite=None, configurations=None, recent=None,
        branch=None, begin=None, end=None,
        begin_query_time=None, end_query_time=None,
        test=None, limit=None, **kwargs
    ):
        AssertRequest.is_type(['GET'])
        AssertRequest.query_kwargs_empty(**kwargs)

        if test and (not suite or len(suite) != len(test)):
            abort(400, description='Each test must be paired with a suite')

        with self.upload_controller.upload_context:
            suites_by_configuration = self.upload_controller.upload_context.find_suites(configurations=configurations, recent=boolean_query(*recent)[0] if recent else True)
            candidate_suites = set()
            for suites_for_config in suites_by_configuration.values():
                for s in suites_for_config:
                    candidate_suites.add(s)
            return sorted([s for s in candidate_suites if test or not suite or s in suite])

    @SiteMenu.render_with_site_menu()
    def search(self, **kwargs):
        return self.environment.get_template('search.html').render(
            title=self.site_menu.title,
            suites=json.dumps(self._suites_for_search()),
            **kwargs)
