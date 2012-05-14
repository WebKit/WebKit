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

import optparse
import os.path
import re
import shutil
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
from webkitpy.layout_tests.models.test_expectations import TestExpectations
from webkitpy.layout_tests.port import builders
from webkitpy.tool.grammar import pluralize
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand


# FIXME: Pull this from Port.baseline_extensions().
_baseline_suffix_list = ['png', 'wav', 'txt']


# FIXME: Should TestResultWriter know how to compute this string?
def _baseline_name(fs, test_name, suffix):
    return fs.splitext(test_name)[0] + TestResultWriter.FILENAME_SUFFIX_EXPECTED + "." + suffix


class AbstractRebaseliningCommand(AbstractDeclarativeCommand):
    def __init__(self):
        options = [
            optparse.make_option('--suffixes', default=','.join(_baseline_suffix_list), action='store',
                                 help='file types to rebaseline')]
        AbstractDeclarativeCommand.__init__(self, options=options)
        self._baseline_suffix_list = _baseline_suffix_list


class RebaselineTest(AbstractRebaseliningCommand):
    name = "rebaseline-test"
    help_text = "Rebaseline a single test from a buildbot.  (Currently works only with build.chromium.org buildbots.)"
    argument_names = "BUILDER_NAME TEST_NAME [PLATFORMS_TO_MOVE_EXISTING_BASELINES_TO]"

    def _results_url(self, builder_name):
        # FIXME: Generalize this command to work with non-build.chromium.org builders.
        builder = self._tool.chromium_buildbot().builder_with_name(builder_name)
        return builder.accumulated_results_url()

    def _baseline_directory(self, builder_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        return port.baseline_path()

    def _copy_existing_baseline(self, platforms_to_move_existing_baselines_to, test_name, suffix):
        old_baselines = []
        new_baselines = []

        # Need to gather all the baseline paths before modifying the filesystem since
        # the modifications can affect the results of port.expected_filename.
        for platform in platforms_to_move_existing_baselines_to:
            port = self._tool.port_factory.get(platform)
            old_baseline = port.expected_filename(test_name, "." + suffix)
            if not self._tool.filesystem.exists(old_baseline):
                print("No existing baseline for %s." % test_name)
                continue

            new_baseline = self._tool.filesystem.join(port.baseline_path(), self._file_name_for_expected_result(test_name, suffix))
            if self._tool.filesystem.exists(new_baseline):
                print("Existing baseline at %s, not copying over it." % new_baseline)
                continue

            old_baselines.append(old_baseline)
            new_baselines.append(new_baseline)

        for i in range(len(old_baselines)):
            old_baseline = old_baselines[i]
            new_baseline = new_baselines[i]

            print("Copying baseline from %s to %s." % (old_baseline, new_baseline))
            self._tool.filesystem.maybe_make_directory(self._tool.filesystem.dirname(new_baseline))
            self._tool.filesystem.copyfile(old_baseline, new_baseline)
            if not self._tool.scm().exists(new_baseline):
                self._tool.scm().add(new_baseline)

    def _save_baseline(self, data, target_baseline):
        if not data:
            return
        filesystem = self._tool.filesystem
        filesystem.maybe_make_directory(filesystem.dirname(target_baseline))
        filesystem.write_binary_file(target_baseline, data)
        if not self._tool.scm().exists(target_baseline):
            self._tool.scm().add(target_baseline)

    def _update_expectations_file(self, builder_name, test_name):
        port = self._tool.port_factory.get_from_builder_name(builder_name)
        expectationsString = port.test_expectations()
        expectations = TestExpectations(port, None, expectationsString, port.test_configuration())

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

        print "Retrieving %s." % source_baseline
        self._save_baseline(self._tool.web.get_binary(source_baseline, convert_404_to_None=True), target_baseline)

    def _rebaseline_test_and_update_expectations(self, builder_name, test_name, platforms_to_move_existing_baselines_to):
        for suffix in self._baseline_suffix_list:
            self._rebaseline_test(builder_name, test_name, platforms_to_move_existing_baselines_to, suffix)
        self._update_expectations_file(builder_name, test_name)

    def execute(self, options, args, tool):
        self._baseline_suffix_list = options.suffixes.split(',')

        if len(args) > 2:
            platforms_to_move_existing_baselines_to = args[2:]
        else:
            platforms_to_move_existing_baselines_to = None
        self._rebaseline_test_and_update_expectations(args[0], args[1], platforms_to_move_existing_baselines_to)


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


class RebaselineExpectations(AbstractDeclarativeCommand):
    name = "rebaseline-expectations"
    help_text = "Rebaselines the tests indicated in test_expectations.txt."

    def __init__(self):
        options = [
            optparse.make_option('--no-optimize', dest='optimize', action='store_false', default=True,
                help=('Do not optimize/de-dup the expectations after rebaselining '
                      '(default is to de-dup automatically). '
                      'You can use "webkit-patch optimize-baselines" to optimize separately.')),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    def _run_webkit_patch(self, args):
        try:
            self._tool.executive.run_command([self._tool.path()] + args, cwd=self._tool.scm().checkout_root)
        except ScriptError, e:
            pass

    def _is_supported_port(self, port_name):
        # FIXME: Support non-Chromium ports.
        return port_name.startswith('chromium-')

    def _expectations(self, port):
        return TestExpectations(port, None, port.test_expectations(), port.test_configuration())

    def _update_expectations_file(self, port_name):
        if not self._is_supported_port(port_name):
            return
        port = self._tool.port_factory.get(port_name)
        expectations = self._expectations(port)
        path = port.path_to_test_expectations_file()
        self._tool.filesystem.write_text_file(path, expectations.remove_rebaselined_tests(expectations.get_rebaselining_failures()))

    def _tests_to_rebaseline(self, port):
        return self._expectations(port).get_rebaselining_failures()

    def _rebaseline_port(self, port_name):
        if not self._is_supported_port(port_name):
            return
        builder_name = builders.builder_name_for_port_name(port_name)
        if not builder_name:
            return
        print "Retrieving results for %s from %s." % (port_name, builder_name)
        for test_name in self._tests_to_rebaseline(self._tool.port_factory.get(port_name)):
            self._touched_test_names.add(test_name)
            print "    %s" % test_name
            # FIXME: need to extract the correct list of suffixes here.
            self._run_webkit_patch(['rebaseline-test', builder_name, test_name])

    def execute(self, options, args, tool):
        self._touched_test_names = set([])
        for port_name in tool.port_factory.all_port_names():
            self._rebaseline_port(port_name)
        for port_name in tool.port_factory.all_port_names():
            self._update_expectations_file(port_name)
        if not options.optimize:
            return
        for test_name in self._touched_test_names:
            print "Optimizing baselines for %s." % test_name
            self._run_webkit_patch(['optimize-baselines', test_name])


class Rebaseline(AbstractDeclarativeCommand):
    name = "rebaseline"
    help_text = "Replaces local expected.txt files with new results from build bots"

    # FIXME: This should share more code with FailureReason._builder_to_explain
    def _builder_to_pull_from(self):
        builder_statuses = self._tool.buildbot.builder_statuses()
        red_statuses = [status for status in builder_statuses if not status["is_green"]]
        print "%s failing" % (pluralize("builder", len(red_statuses)))
        builder_choices = [status["name"] for status in red_statuses]
        chosen_name = self._tool.user.prompt_with_list("Which builder to pull results from:", builder_choices)
        # FIXME: prompt_with_list should really take a set of objects and a set of names and then return the object.
        for status in red_statuses:
            if status["name"] == chosen_name:
                return (self._tool.buildbot.builder_with_name(chosen_name), status["build_number"])

    def _replace_expectation_with_remote_result(self, local_file, remote_file):
        (downloaded_file, headers) = urllib.urlretrieve(remote_file)
        shutil.move(downloaded_file, local_file)

    def _tests_to_update(self, build):
        failing_tests = build.layout_test_results().tests_matching_failure_types([test_failures.FailureTextMismatch])
        return self._tool.user.prompt_with_list("Which test(s) to rebaseline:", failing_tests, can_choose_multiple=True)

    def _results_url_for_test(self, build, test):
        test_base = os.path.splitext(test)[0]
        actual_path = test_base + "-actual.txt"
        return build.results_url() + "/" + actual_path

    def execute(self, options, args, tool):
        builder, build_number = self._builder_to_pull_from()
        build = builder.build(build_number)
        port = tool.port_factory.get_from_builder_name(builder.name())

        for test in self._tests_to_update(build):
            results_url = self._results_url_for_test(build, test)
            # Port operates with absolute paths.
            expected_file = port.expected_filename(test, '.txt')
            print test
            self._replace_expectation_with_remote_result(expected_file, results_url)

        # FIXME: We should handle new results too.
