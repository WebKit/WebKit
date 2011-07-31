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

import os.path
import re
import shutil
import urllib

import webkitpy.common.config.urls as config_urls
from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.user import User
from webkitpy.layout_tests.models import test_failures
from webkitpy.layout_tests.port import factory
from webkitpy.tool.grammar import pluralize
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand


# FIXME: This logic should be moved to builders.py.
class BuilderToPort(object):
    _builder_name_to_port_name = {
        # These builders are on build.webkit.org.
        r"SnowLeopard": "mac-snowleopard",
        r"Leopard": "mac-leopard",
        r"Windows": "win",
        r"GTK": "gtk",
        r"Qt": "qt",
        r"Chromium Mac": "chromium-mac",
        r"Chromium Linux": "chromium-linux",
        r"Chromium Win": "chromium-win",

        # These builders are on build.chromium.org.
        r"Webkit Win$": "chromium-win-xp",
        r"Webkit Vista": "chromium-win-vista",
        r"Webkit Win7": "chromium-win-win7",
        r"Webkit Win \(dbg\)\(1\)": "chromium-win-win7",  # FIXME: Is this correct?
        r"Webkit Win \(dbg\)\(2\)": "chromium-win-win7",  # FIXME: Is this correct?
        r"Webkit Linux": "chromium-linux-x86_64",
        r"Webkit Linux 32": "chromium-linux-x86",
        r"Webkit Linux \(dbg\)\(1\)": "chromium-linux-x86_64",
        r"Webkit Linux \(dbg\)\(2\)": "chromium-linux-x86_64",
        r"Webkit Mac10\.5": "chromium-mac-leopard",
        r"Webkit Mac10\.5 \(dbg\)\(1\)": "chromium-mac-leopard",
        r"Webkit Mac10\.5 \(dbg\)\(2\)": "chromium-mac-leopard",
        r"Webkit Mac10\.6": "chromium-mac-snowleopard",
        r"Webkit Mac10\.6 \(dbg\)": "chromium-mac-snowleopard",
    }

    def _port_name_for_builder_name(self, builder_name):
        for regexp, port_name in self._builder_name_to_port_name.items():
            if re.match(regexp, builder_name):
                return port_name

    def port_for_builder(self, builder_name):
        port_name = self._port_name_for_builder_name(builder_name)
        assert(port_name)  # Need to update _builder_name_to_port_name
        port = factory.get(port_name)
        assert(port)  # Need to update _builder_name_to_port_name
        return port


class RebaselineTest(AbstractDeclarativeCommand):
    name = "rebaseline-test"
    help_text = "Rebaseline a single test from a buildbot.  (Currently works only with build.chromium.org buildbots.)"
    argument_names = "BUILDER_NAME TEST_NAME SUFFIX"

    def _results_url(self, builder_name):
        # FIXME: Generalize this command to work with non-build.chromium.org builders.
        builder = self._tool.chromium_buildbot().builder_with_name(builder_name)
        return builder.accumulated_results_url()

    def _baseline_directory(self, builder_name):
        port = BuilderToPort().port_for_builder(builder_name)
        return port.baseline_path()

    def _save_baseline(self, data, target_baseline):
        filesystem = self._tool.filesystem
        filesystem.maybe_make_directory(filesystem.dirname(target_baseline))
        filesystem.write_binary_file(target_baseline, data)
        if not self._tool.scm().exists(target_baseline):
            self._tool.scm().add(target_baseline)

    def _test_root(self, test_name):
        return os.path.splitext(test_name)[0]

    def _file_name_for_actual_result(self, test_name, suffix):
        return "%s-actual.%s" % (self._test_root(test_name), suffix)

    def _file_name_for_expected_result(self, test_name, suffix):
        return "%s-expected.%s" % (self._test_root(test_name), suffix)

    def _rebaseline_test(self, builder_name, test_name, suffix):
        results_url = self._results_url(builder_name)
        baseline_directory = self._baseline_directory(builder_name)

        source_baseline = "%s/%s" % (results_url, self._file_name_for_actual_result(test_name, suffix))
        target_baseline = os.path.join(baseline_directory, self._file_name_for_expected_result(test_name, suffix))

        print "Retrieving %s ..." % source_baseline
        self._save_baseline(self._tool.web.get_binary(source_baseline), target_baseline)

    def execute(self, options, args, tool):
        self._rebaseline_test(args[0], args[1], args[2])


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
        port = BuilderToPort().port_for_builder(builder.name())

        for test in self._tests_to_update(build):
            results_url = self._results_url_for_test(build, test)
            # Port operates with absolute paths.
            expected_file = port.expected_filename(test, '.txt')
            print test
            self._replace_expectation_with_remote_result(expected_file, results_url)

        # FIXME: We should handle new results too.
