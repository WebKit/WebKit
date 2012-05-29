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
import logging
import json
import os

from webkitpy.common.memoized import memoized
from webkitpy.tool.servers.reflectionhandler import ReflectionHandler
from webkitpy.layout_tests.controllers.test_expectations_editor import BugManager, TestExpectationsEditor
from webkitpy.layout_tests.models.test_expectations import TestExpectationParser, TestExpectations, TestExpectationSerializer
from webkitpy.layout_tests.models.test_configuration import TestConfigurationConverter
from webkitpy.layout_tests.port import builders


_log = logging.getLogger(__name__)


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
        command = [ 'rebaseline-test' ]

        if 'suffixes' in self.query:
            command.append('--suffixes')
            command.append(self.query['suffixes'][0])

        command.append(builder)
        command.append(self.query['test'][0])

        command.extend(builders.fallback_port_names_for_new_port(builder))
        self._run_webkit_patch(command)
        self._serve_text('success')

    def _builders_to_fetch_from(self, builders):
        # This routine returns the subset of builders that will cover all of the baseline search paths
        # used in the input list. In particular, if the input list contains both Release and Debug
        # versions of a configuration, we *only* return the Release version (since we don't save
        # debug versions of baselines).
        release_builders = set()
        debug_builders = set()
        builders_to_fallback_paths = {}
        for builder in builders:
            port = self.server.tool.port_factory.get_from_builder_name(builder)
            if port.test_configuration().build_type == 'Release':
                release_builders.add(builder)
            else:
                debug_builders.add(builder)
        for builder in list(release_builders) + list(debug_builders):
            port = self.server.tool.port_factory.get_from_builder_name(builder)
            fallback_path = port.baseline_search_path()
            if fallback_path not in builders_to_fallback_paths.values():
                builders_to_fallback_paths[builder] = fallback_path
        return builders_to_fallback_paths.keys()

    def _rebaseline_commands(self, test_list):
        path_to_webkit_patch = self.server.tool.path()
        cwd = self.server.tool.scm().checkout_root
        commands = []
        for test in test_list:
            for builder in self._builders_to_fetch_from(test_list[test]):
                suffixes = ','.join(test_list[test][builder])
                cmd_line = [path_to_webkit_patch, 'rebaseline-test', '--print-scm-changes', '--suffixes', suffixes, builder, test]
                commands.append(tuple([cmd_line, cwd]))
        return commands

    def _files_to_add(self, command_results):
        files_to_add = set()
        for output in [result[1] for result in command_results]:
            try:
                files_to_add.update(json.loads(output)['add'])
            except ValueError, e:
                _log.warning('"%s" is not a JSON object, ignoring' % output)

        return list(files_to_add)

    def _optimize_baselines(self, test_list):
        # We don't run this in parallel because modifying the SCM in parallel is unreliable.
        for test in test_list:
            all_suffixes = set()
            for builder in self._builders_to_fetch_from(test_list[test]):
                all_suffixes.update(test_list[test][builder])
            self._run_webkit_patch(['optimize-baselines', '--suffixes', ','.join(all_suffixes), test])

    def rebaselineall(self):
        test_list = self._read_entity_body_as_json()

        commands = self._rebaseline_commands(test_list)
        command_results = self.server.tool.executive.run_in_parallel(commands)

        files_to_add = self._files_to_add(command_results)
        self.server.tool.scm().add_list(list(files_to_add))

        self._optimize_baselines(test_list)
        self._serve_text('success')
