# Copyright (c) 2011 Google Inc. All rights reserved.
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

import logging

from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.unittestresults import UnitTestResults
from webkitpy.tool.steps.runtests import RunTests

_log = logging.getLogger(__name__)


class LayoutTestResultsReader(object):
    def __init__(self, tool, archive_directory):
        self._tool = tool
        self._archive_directory = archive_directory

    # FIXME: This exists for mocking, but should instead be mocked via
    # tool.filesystem.read_text_file.  They have different error handling at the moment.
    def _read_file_contents(self, path):
        try:
            return self._tool.filesystem.read_text_file(path)
        except IOError, e:  # File does not exist or can't be read.
            return None

    # FIXME: This logic should move to the port object.
    def _create_layout_test_results(self):
        results_path = self._tool.deprecated_port().layout_tests_results_path()
        results_html = self._read_file_contents(results_path)
        if not results_html:
            return None
        return LayoutTestResults.results_from_string(results_html)

    def _create_unit_test_results(self):
        results_path = self._tool.deprecated_port().unit_tests_results_path()
        if not results_path:
            return None
        results_xml = self._read_file_contents(results_path)
        if not results_xml:
            return None
        return UnitTestResults.results_from_string(results_xml)

    def results(self):
        layout_test_results = self._create_layout_test_results()
        unit_test_results = self._create_unit_test_results()
        if layout_test_results:
            # FIXME: We should not have to set failure_limit_count, but we
            # do until run-webkit-tests can be updated save off the value
            # of --exit-after-N-failures in results.html/results.json.
            # https://bugs.webkit.org/show_bug.cgi?id=58481
            layout_test_results.set_failure_limit_count(RunTests.NON_INTERACTIVE_FAILURE_LIMIT_COUNT)
            if unit_test_results:
                layout_test_results.add_unit_test_failures(unit_test_results)
        return layout_test_results

    def _results_directory(self):
        results_path = self._tool.deprecated_port().layout_tests_results_path()
        # FIXME: This is wrong in two ways:
        # 1. It assumes that results.html is at the top level of the results tree.
        # 2. This uses the "old" ports.py infrastructure instead of the new layout_tests/port
        # which will not support Chromium.  However the new arch doesn't work with old-run-webkit-tests
        # so we have to use this for now.
        return self._tool.filesystem.dirname(results_path)

    def archive(self, patch):
        results_directory = self._results_directory()
        results_name, _ = self._tool.filesystem.splitext(self._tool.filesystem.basename(results_directory))
        # Note: We name the zip with the bug_id instead of patch_id to match work_item_log_path().
        zip_path = self._tool.workspace.find_unused_filename(self._archive_directory, "%s-%s" % (patch.bug_id(), results_name), "zip")
        if not zip_path:
            return None
        if not self._tool.filesystem.isdir(results_directory):
            _log.info("%s does not exist, not archiving." % results_directory)
            return None
        archive = self._tool.workspace.create_zip(zip_path, results_directory)
        # Remove the results directory to prevent http logs, etc. from getting huge between runs.
        # We could have create_zip remove the original, but this is more explicit.
        self._tool.filesystem.rmtree(results_directory)
        return archive
