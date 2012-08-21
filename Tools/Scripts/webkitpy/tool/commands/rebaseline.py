# Copyright (c) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

import json
import logging
import optparse
import os.path
import re
import shutil
import sys
import urllib

import webkitpy.common.config.urls as config_urls
from webkitpy.common.checkout.baselineoptimizer import BaselineOptimizer
from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.user import User
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.models.test_expectations import TestExpectations, BASELINE_SUFFIX_LIST
from webkitpy.layout_tests.port import builders
from webkitpy.tool.grammar import pluralize
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand


_log = logging.getLogger(__name__)

# FIXME: Should TestResultWriter know how to compute this string?
def _baseline_name(fs, test_name, suffix):
    return fs.splitext(test_name)[0] + TestResultWriter.FILENAME_SUFFIX_EXPECTED + "." + suffix


class AbstractRebaseliningCommand(AbstractDeclarativeCommand):
    def __init__(self, options=None):
        options = options or []
        options.extend([
            optparse.make_option('--suffixes', default=','.join(BASELINE_SUFFIX_LIST), action='store',
                                 help='file types to rebaseline')])
        AbstractDeclarativeCommand.__init__(self, options=options)
        self._baseline_suffix_list = BASELINE_SUFFIX_LIST


class RebaselineTest(AbstractRebaseliningCommand):
    name = "rebaseline-test-internal"
    help_text = "Rebaseline a single test from a buildbot. Only intended for use by other webkit-patch commands."

    def __init__(self):
        options = [
            optparse.make_option("--builder", help="Builder to pull new baselines from"),
            optparse.make_option("--platform-to-move-to", help="Platform to move existing baselines to before rebaselining. This is for dealing with bringing up new ports that interact with non-tree portions of the fallback graph."),
            optparse.make_option("--test", help="Test to rebaseline"),
        ]
        AbstractRebaseliningCommand.__init__(self, options=options)
        self._scm_changes = {'add': []}

    def _results_url(self, builder_name):
        return self._tool.buildbot_for_builder_name(builder_name).builder_with_name(builder_name).latest_layout_test_results_url()

    def _baseline_directory(self, builder_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        override_dir = builders.rebaseline_override_dir(builder_name)
        if override_dir:
            return self._tool.filesystem.join(port.layout_tests_dir(), 'platform', override_dir)
        return port.baseline_version_dir()

    def _copy_existing_baseline(self, platforms_to_move_existing_baselines_to, test_name, suffix):
        old_baselines = []
        new_baselines = []

        # Need to gather all the baseline paths before modifying the filesystem since
        # the modifications can affect the results of port.expected_filename.
        for platform in platforms_to_move_existing_baselines_to:
            port = self._tool.port_factory.get(platform)
            old_baseline = port.expected_filename(test_name, "." + suffix)
            if not self._tool.filesystem.exists(old_baseline):
                _log.info("No existing baseline for %s." % test_name)
                continue

            new_baseline = self._tool.filesystem.join(port.baseline_path(), self._file_name_for_expected_result(test_name, suffix))
            if self._tool.filesystem.exists(new_baseline):
                _log.info("Existing baseline at %s, not copying over it." % new_baseline)
                continue

            old_baselines.append(old_baseline)
            new_baselines.append(new_baseline)

        for i in range(len(old_baselines)):
            old_baseline = old_baselines[i]
            new_baseline = new_baselines[i]

            _log.info("Copying baseline from %s to %s." % (old_baseline, new_baseline))
            self._tool.filesystem.maybe_make_directory(self._tool.filesystem.dirname(new_baseline))
            self._tool.filesystem.copyfile(old_baseline, new_baseline)
            if not self._tool.scm().exists(new_baseline):
                self._add_to_scm(new_baseline)

    def _save_baseline(self, data, target_baseline):
        if not data:
            return
        filesystem = self._tool.filesystem
        filesystem.maybe_make_directory(filesystem.dirname(target_baseline))
        filesystem.write_binary_file(target_baseline, data)
        if not self._tool.scm().exists(target_baseline):
            self._add_to_scm(target_baseline)

    def _add_to_scm(self, path):
        self._scm_changes['add'].append(path)

    def _update_expectations_file(self, builder_name, test_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        expectations = TestExpectations(port, include_overrides=False)

        for test_configuration in port.all_test_configurations():
            if test_configuration.version == port.test_configuration().version:
                expectationsString = expectations.remove_configuration_from_test(test_name, test_configuration)

        self._tool.filesystem.write_text_file(port.path_to_test_expectations_file(), expectationsString)

    def _test_root(self, test_name):
        return os.path.splitext(test_name)[0]

    def _file_name_for_actual_result(self, test_name, suffix):
        return "%s-actual.%s" % (self._test_root(test_name), suffix)

    def _file_name_for_expected_result(self, test_name, suffix):
        return "%s-expected.%s" % (self._test_root(test_name), suffix)

    def _rebaseline_test(self, builder_name, test_name, platforms_to_move_existing_baselines_to, suffix):
        results_url = self._results_url(builder_name)
        baseline_directory = self._baseline_directory(builder_name)

        source_baseline = "%s/%s" % (results_url, self._file_name_for_actual_result(test_name, suffix))
        target_baseline = self._tool.filesystem.join(baseline_directory, self._file_name_for_expected_result(test_name, suffix))

        if platforms_to_move_existing_baselines_to:
            self._copy_existing_baseline(platforms_to_move_existing_baselines_to, test_name, suffix)

        _log.info("Retrieving %s." % source_baseline)
        self._save_baseline(self._tool.web.get_binary(source_baseline, convert_404_to_None=True), target_baseline)

    def _rebaseline_test_and_update_expectations(self, builder_name, test_name, platforms_to_move_existing_baselines_to):
        for suffix in self._baseline_suffix_list:
            self._rebaseline_test(builder_name, test_name, platforms_to_move_existing_baselines_to, suffix)
        self._update_expectations_file(builder_name, test_name)

    def execute(self, options, args, tool):
        self._baseline_suffix_list = options.suffixes.split(',')
        self._rebaseline_test_and_update_expectations(options.builder, options.test, options.platform_to_move_to)
        print json.dumps(self._scm_changes)


class OptimizeBaselines(AbstractRebaseliningCommand):
    name = "optimize-baselines"
    help_text = "Reshuffles the baselines for the given tests to use as litte space on disk as possible."
    argument_names = "TEST_NAMES"

    def _optimize_baseline(self, test_name):
        for suffix in self._baseline_suffix_list:
            baseline_name = _baseline_name(self._tool.filesystem, test_name, suffix)
            if not self._baseline_optimizer.optimize(baseline_name):
                print "Hueristics failed to optimize %s" % baseline_name

    def execute(self, options, args, tool):
        self._baseline_suffix_list = options.suffixes.split(',')
        self._baseline_optimizer = BaselineOptimizer(tool)
        self._port = tool.port_factory.get("chromium-win-win7")  # FIXME: This should be selectable.

        for test_name in self._port.tests(args):
            print "Optimizing %s." % test_name
            self._optimize_baseline(test_name)


class AnalyzeBaselines(AbstractRebaseliningCommand):
    name = "analyze-baselines"
    help_text = "Analyzes the baselines for the given tests and prints results that are identical."
    argument_names = "TEST_NAMES"

    def _print(self, baseline_name, directories_by_result):
        for result, directories in directories_by_result.items():
            if len(directories) <= 1:
                continue
            results_names = [self._tool.filesystem.join(directory, baseline_name) for directory in directories]
            print ' '.join(results_names)

    def _analyze_baseline(self, test_name):
        for suffix in self._baseline_suffix_list:
            baseline_name = _baseline_name(self._tool.filesystem, test_name, suffix)
            directories_by_result = self._baseline_optimizer.directories_by_result(baseline_name)
            self._print(baseline_name, directories_by_result)

    def execute(self, options, args, tool):
        self._baseline_suffix_list = options.suffixes.split(',')
        self._baseline_optimizer = BaselineOptimizer(tool)
        self._port = tool.port_factory.get("chromium-win-win7")  # FIXME: This should be selectable.

        for test_name in self._port.tests(args):
            self._analyze_baseline(test_name)


class AbstractParallelRebaselineCommand(AbstractDeclarativeCommand):
    def __init__(self, options=None):
        options = options or []
        options.extend([
            optparse.make_option('--no-optimize', dest='optimize', action='store_false', default=True,
                help=('Do not optimize/de-dup the expectations after rebaselining '
                      '(default is to de-dup automatically). '
                      'You can use "webkit-patch optimize-baselines" to optimize separately.'))])
        AbstractDeclarativeCommand.__init__(self, options=options)

    def _run_webkit_patch(self, args):
        try:
            self._tool.executive.run_command([self._tool.path()] + args, cwd=self._tool.scm().checkout_root)
        except ScriptError, e:
            _log.error(e)

    def _builders_to_fetch_from(self, builders):
        # This routine returns the subset of builders that will cover all of the baseline search paths
        # used in the input list. In particular, if the input list contains both Release and Debug
        # versions of a configuration, we *only* return the Release version (since we don't save
        # debug versions of baselines).
        release_builders = set()
        debug_builders = set()
        builders_to_fallback_paths = {}
        for builder in builders:
            port = self._tool.port_factory.get_from_builder_name(builder)
            if port.test_configuration().build_type == 'Release':
                release_builders.add(builder)
            else:
                debug_builders.add(builder)
        for builder in list(release_builders) + list(debug_builders):
            port = self._tool.port_factory.get_from_builder_name(builder)
            fallback_path = port.baseline_search_path()
            if fallback_path not in builders_to_fallback_paths.values():
                builders_to_fallback_paths[builder] = fallback_path
        return builders_to_fallback_paths.keys()

    def _rebaseline_commands(self, test_list):
        path_to_webkit_patch = self._tool.path()
        cwd = self._tool.scm().checkout_root
        commands = []
        for test in test_list:
            for builder in self._builders_to_fetch_from(test_list[test]):
                suffixes = ','.join(test_list[test][builder])
                cmd_line = [path_to_webkit_patch, 'rebaseline-test-internal', '--suffixes', suffixes, '--builder', builder, '--test', test]
                commands.append(tuple([cmd_line, cwd]))
        return commands

    def _files_to_add(self, command_results):
        files_to_add = set()
        for output in [result[1].split('\n') for result in command_results]:
            file_added = False
            for line in output:
                try:
                    files_to_add.update(json.loads(line)['add'])
                    file_added = True
                except ValueError, e:
                    _log.debug('"%s" is not a JSON object, ignoring' % line)

            if not file_added:
                _log.debug('Could not add file based off output "%s"' % output)


        return list(files_to_add)

    def _optimize_baselines(self, test_list):
        # We don't run this in parallel because modifying the SCM in parallel is unreliable.
        for test in test_list:
            all_suffixes = set()
            for builder in self._builders_to_fetch_from(test_list[test]):
                all_suffixes.update(test_list[test][builder])
            self._run_webkit_patch(['optimize-baselines', '--suffixes', ','.join(all_suffixes), test])

    def _rebaseline(self, options, test_list):
        commands = self._rebaseline_commands(test_list)
        command_results = self._tool.executive.run_in_parallel(commands)

        files_to_add = self._files_to_add(command_results)
        if files_to_add:
            self._tool.scm().add_list(list(files_to_add))

        if options.optimize:
            self._optimize_baselines(test_list)


class RebaselineJson(AbstractParallelRebaselineCommand):
    name = "rebaseline-json"
    help_text = "Rebaseline based off JSON passed to stdin. Intended to only be called from other scripts."

    def execute(self, options, args, tool):
        self._rebaseline(options, json.loads(sys.stdin.read()))


class RebaselineExpectations(AbstractParallelRebaselineCommand):
    name = "rebaseline-expectations"
    help_text = "Rebaselines the tests indicated in TestExpectations."

    def _update_expectations_files(self, port_name):
        port = self._tool.port_factory.get(port_name)

        expectations = TestExpectations(port, include_overrides=False)
        for path in port.expectations_dict():
            if self._tool.filesystem.exists(path):
                self._tool.filesystem.write_text_file(path, expectations.remove_rebaselined_tests(expectations.get_rebaselining_failures(), path))

    def _tests_to_rebaseline(self, port):
        tests_to_rebaseline = {}
        expectations = TestExpectations(port, include_overrides=True)
        for test in expectations.get_rebaselining_failures():
            tests_to_rebaseline[test] = TestExpectations.suffixes_for_expectations(expectations.get_expectations(test))
        return tests_to_rebaseline

    def _add_tests_to_rebaseline_for_port(self, port_name):
        builder_name = builders.builder_name_for_port_name(port_name)
        if not builder_name:
            return
        tests = self._tests_to_rebaseline(self._tool.port_factory.get(port_name)).items()

        if tests:
            _log.info("Retrieving results for %s from %s." % (port_name, builder_name))

        for test_name, suffixes in tests:
            _log.info("    %s (%s)" % (test_name, ','.join(suffixes)))
            if test_name not in self._test_list:
                self._test_list[test_name] = {}
            self._test_list[test_name][builder_name] = suffixes

    def execute(self, options, args, tool):
        self._test_list = {}
        for port_name in tool.port_factory.all_port_names():
            self._add_tests_to_rebaseline_for_port(port_name)
        self._rebaseline(options, self._test_list)

        for port_name in tool.port_factory.all_port_names():
            self._update_expectations_files(port_name)


class Rebaseline(AbstractParallelRebaselineCommand):
    name = "rebaseline"
    help_text = "Rebaseline tests with results from the build bots. Shows the list of failing tests on the builders if no test names are provided."
    argument_names = "[TEST_NAMES]"

    def __init__(self):
        options = [
            optparse.make_option("--builders", default=None, action="append", help="Comma-separated-list of builders to pull new baselines from (can also be provided multiple times)"),
            optparse.make_option("--suffixes", default=BASELINE_SUFFIX_LIST, action="append", help="Comma-separated-list of file types to rebaseline (can also be provided multiple times)"),
        ]
        AbstractParallelRebaselineCommand.__init__(self, options=options)

    def _builders_to_pull_from(self):
        chromium_buildbot_builder_names = []
        webkit_buildbot_builder_names = []
        for name in builders.all_builder_names():
            if self._tool.port_factory.get_from_builder_name(name).is_chromium():
                chromium_buildbot_builder_names.append(name)
            else:
                webkit_buildbot_builder_names.append(name)

        titles = ["build.webkit.org bots", "build.chromium.org bots"]
        lists = [webkit_buildbot_builder_names, chromium_buildbot_builder_names]

        chosen_names = self._tool.user.prompt_with_multiple_lists("Which builder to pull results from:", titles, lists, can_choose_multiple=True)
        return [self._builder_with_name(name) for name in chosen_names]

    def _builder_with_name(self, name):
        return self._tool.buildbot_for_builder_name(name).builder_with_name(name)

    def _tests_to_update(self, builder):
        failing_tests = builder.latest_layout_test_results().tests_matching_failure_types([test_failures.FailureTextMismatch])
        return self._tool.user.prompt_with_list("Which test(s) to rebaseline for %s:" % builder.name(), failing_tests, can_choose_multiple=True)

    def _suffixes_to_update(self, options):
        suffixes = set()
        for suffix_list in options.suffixes:
            suffixes |= set(suffix_list.split(","))
        return list(suffixes)

    def execute(self, options, args, tool):
        if options.builders:
            builders = []
            for builder_names in options.builders:
                builders += [self._builder_with_name(name) for name in builder_names.split(",")]
        else:
            builders = self._builders_to_pull_from()

        test_list = {}

        for builder in builders:
            tests = args or self._tests_to_update(builder)
            for test in tests:
                if test not in test_list:
                    test_list[test] = {}
                test_list[test][builder.name()] = self._suffixes_to_update(options)

        if options.verbose:
            print "rebaseline-json: " + str(test_list)

        self._rebaseline(options, test_list)
