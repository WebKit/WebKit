# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import BaseHTTPServer
import os

from webkitpy.common.memoized import memoized
from webkitpy.tool.servers.reflectionhandler import ReflectionHandler
from webkitpy.layout_tests.controllers.test_expectations_editor import BugManager, TestExpectationsEditor
from webkitpy.layout_tests.models.test_expectations import TestExpectationParser, TestExpectations, TestExpectationSerializer
from webkitpy.layout_tests.models.test_configuration import TestConfigurationConverter
from webkitpy.layout_tests.port import builders


class BuildCoverageExtrapolator(object):
    def __init__(self, test_configuration_converter):
        self._test_configuration_converter = test_configuration_converter

    @memoized
    def _covered_test_configurations_for_builder_name(self):
        coverage = {}
        for builder_name in builders.all_builder_names():
            coverage[builder_name] = self._test_configuration_converter.to_config_set(builders.coverage_specifiers_for_builder_name(builder_name))
        return coverage

    def extrapolate_test_configurations(self, builder_name):
        return self._covered_test_configurations_for_builder_name()[builder_name]


class GardeningExpectationsUpdater(BugManager):
    def __init__(self, tool, port):
        self._converter = TestConfigurationConverter(port.all_test_configurations(), port.configuration_specifier_macros())
        self._extrapolator = BuildCoverageExtrapolator(self._converter)
        self._parser = TestExpectationParser(port, [], allow_rebaseline_modifier=False)
        self._path_to_test_expectations_file = port.path_to_test_expectations_file()
        self._tool = tool

    def close_bug(self, bug_id, reference_bug_id=None):
        # FIXME: Implement this properly.
        pass

    def create_bug(self):
        return "BUG_NEW"

    def update_expectations(self, failure_info_list):
        expectation_lines = self._parser.parse(self._tool.filesystem.read_text_file(self._path_to_test_expectations_file))
        editor = TestExpectationsEditor(expectation_lines, self)
        updated_expectation_lines = []
        # FIXME: Group failures by testName+failureTypeList.
        for failure_info in failure_info_list:
            expectation_set = set(filter(lambda expectation: expectation is not None,
                                         map(TestExpectations.expectation_from_string, failure_info['failureTypeList'])))
            assert(expectation_set)
            test_name = failure_info['testName']
            assert(test_name)
            builder_name = failure_info['builderName']
            affected_test_configuration_set = self._extrapolator.extrapolate_test_configurations(builder_name)
            updated_expectation_lines.extend(editor.update_expectation(test_name, affected_test_configuration_set, expectation_set))
        self._tool.filesystem.write_text_file(self._path_to_test_expectations_file, TestExpectationSerializer.list_to_string(expectation_lines, self._converter, reconstitute_only_these=updated_expectation_lines))


class GardeningHTTPServer(BaseHTTPServer.HTTPServer):
    def __init__(self, httpd_port, config):
        server_name = ''
        self.tool = config['tool']
        BaseHTTPServer.HTTPServer.__init__(self, (server_name, httpd_port), GardeningHTTPRequestHandler)

    def url(self):
        return 'file://' + os.path.join(GardeningHTTPRequestHandler.STATIC_FILE_DIRECTORY, 'garden-o-matic.html')


class GardeningHTTPRequestHandler(ReflectionHandler):
    STATIC_FILE_NAMES = frozenset()

    STATIC_FILE_DIRECTORY = os.path.join(
        os.path.dirname(__file__),
        '..',
        '..',
        '..',
        '..',
        'BuildSlaveSupport',
        'build.webkit.org-config',
        'public_html',
        'TestFailures')

    allow_cross_origin_requests = True

    def _run_webkit_patch(self, args):
        return self.server.tool.executive.run_command([self.server.tool.path()] + args, cwd=self.server.tool.scm().checkout_root)

    @memoized
    def _expectations_updater(self):
        # FIXME: Should split failure_info_list into lists per port, then edit each expectations file separately.
        # For now, assume Chromium port.
        port = self.server.tool.get("chromium-win-win7")
        return GardeningExpectationsUpdater(self.server.tool, port)

    def rollout(self):
        revision = self.query['revision'][0]
        reason = self.query['reason'][0]
        self._run_webkit_patch([
            'rollout',
            '--force-clean',
            '--non-interactive',
            revision,
            reason,
        ])
        self._serve_text('success')

    def ping(self):
        self._serve_text('pong')

    def updateexpectations(self):
        self._expectations_updater().update_expectations(self._read_entity_body_as_json())
        self._serve_text('success')

    def rebaseline(self):
        builder = self.query['builder'][0]
        test = self.query['test'][0]
        self._run_webkit_patch([
            'rebaseline-test',
            builder,
            test,
        ])
        self._serve_text('success')

    def optimizebaselines(self):
        test = self.query['test'][0]
        self._run_webkit_patch([
            'optimize-baselines',
            test,
        ])
        self._serve_text('success')
