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

from webkitpy.common.net.buildbot import BuildBot, LayoutTestResults
from webkitpy.common.system.user import User
from webkitpy.layout_tests.port import factory
from webkitpy.tool.grammar import pluralize
from webkitpy.tool.multicommandtool import AbstractDeclarativeCommand


# FIXME: I'm not sure where this logic should go in the end.
# For now it's here, until we have a second need for it.
class BuilderToPort(object):
    _builder_name_to_port_name = {
        r"SnowLeopard": "mac-snowleopard",
        r"Leopard": "mac-leopard",
        r"Tiger": "mac-tiger",
        r"Windows": "win",
        r"GTK": "gtk",
        r"Qt": "qt",
        r"Chromium Mac": "chromium-mac",
        r"Chromium Linux": "chromium-linux",
        r"Chromium Win": "chromium-win",
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
        parsed_results = build.layout_test_results().parsed_results()
        # FIXME: This probably belongs as API on LayoutTestResults
        # but .failing_tests() already means something else.
        return parsed_results[LayoutTestResults.fail_key]

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
            absolute_path = os.path.join(port.layout_tests_dir(), test)
            expected_file = port.expected_filename(absolute_path, ".txt")
            print test
            self._replace_expectation_with_remote_result(expected_file, results_url)

        # FIXME: We should handle new results too.
