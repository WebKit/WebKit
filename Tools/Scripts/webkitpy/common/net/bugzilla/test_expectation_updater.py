#!/usr/bin/env python

# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import argparse
import io
import logging
import zipfile

from webkitpy.common.host import Host
from webkitpy.common.net.bugzilla import bugzilla
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models import test_expectations

_log = logging.getLogger(__name__)


def configure_logging():
    class LogHandler(logging.StreamHandler):

        def format(self, record):
            if record.levelno > logging.INFO:
                return "%s: %s" % (record.levelname, record.getMessage())
            return record.getMessage()

    logger = logging.getLogger()
    logger.setLevel(logging.INFO)
    handler = LogHandler()
    handler.setLevel(logging.INFO)
    logger.addHandler(handler)
    return handler


def argument_parser():
    description = """Update expected.txt files from patches submitted by EWS bots on bugzilla. Given id refers to a bug id by default."""
    parser = argparse.ArgumentParser(prog='importupdate-test-expectations-from-bugzilla bugzilla_id', description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('-a', '--is-attachment-id', dest='is_bug_id', action='store_false', default=True, help='Whether the given id is a bugzilla attachment and not a bug id')
    parser.add_argument('-s', '--is-platform-specific', dest='is_attachment_platform_specific', action='store_true', default=False, help='Whether generic expected files should be updated or not')
    return parser


class TestExpectationUpdater(object):

    def __init__(self, host, bugzilla_id, is_bug_id=True, is_attachment_platform_specific=False, attachment_fetcher=bugzilla.Bugzilla(), unzip=None):
        self.host = host
        self.filesystem = self.host.filesystem
        self.unzip = unzip if unzip else lambda content: zipfile.ZipFile(io.BytesIO(content))
        if is_bug_id:
            self.platform_specific_attachments = {}
            for attachment in attachment_fetcher.fetch_bug(bugzilla_id).attachments():
                bot_type = self._bot_type(attachment)
                if bot_type:
                    self.platform_specific_attachments[bot_type] = attachment
            self.generic_attachment = self.platform_specific_attachments.pop("mac-wk2") if "mac-wk2" in self.platform_specific_attachments else None
        else:
            attachment = attachment_fetcher.fetch_attachment(bugzilla_id)
            self.platform_specific_attachments = {self._bot_type(attachment): attachment} if is_attachment_platform_specific else {}
            self.generic_attachment = attachment if not is_attachment_platform_specific else None

        webkit_finder = WebKitFinder(self.filesystem)
        self.layout_test_repository = webkit_finder.path_from_webkit_base("LayoutTests")

    def _bot_type(self, attachment):
        name = attachment.name()
        if "mac" in name and name.endswith("-wk2"):
            return "mac-wk2"
        if "mac" in name and not name.endswith("-wk2"):
            return "mac-wk1"
        if "simulator" in name:
            return "ios-wk2"
        if "win-future" in name:
            return "win"
        return None

    def _tests_to_update(self, attachment, bot_type=None):
        _log.info("Processing attachment " + str(attachment.id()))
        zip_file = self.unzip(attachment.contents())
        results = LayoutTestResults.results_from_string(zip_file.read("full_results.json"))
        results_to_update = [result.test_name for result in results.failing_test_results() if result.type == test_expectations.TEXT]
        return {result: zip_file.read(TestResultWriter.actual_filename(result, self.filesystem)) for result in results_to_update}

    def _file_content_if_exists(self, filename):
        return self.filesystem.read_text_file(filename) if self.filesystem.exists(filename) else ""

    # FIXME: Investigate the possibility to align what is done there with what single_test_runner is doing.
    # In particular the fact of not overwriting the file if content is the same.
    def _update_from_generic_attachment(self):
        for test_name, expected_content in self._tests_to_update(self.generic_attachment).iteritems():
            expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem))
            if expected_content != self._file_content_if_exists(expected_filename):
                _log.info("Updating " + test_name + " (" + expected_filename + ")")
                self.filesystem.write_text_file(expected_filename, expected_content)

    # FIXME: Investigate the possibility to align what is done there with what single_test_runner is doing.
    # In particular the ability to remove a new specific expectation if it is the same as the generic one.
    def _update_from_platform_specific_attachment(self, attachment, bot_type):
        for test_name, expected_content in self._tests_to_update(attachment, bot_type).iteritems():
            expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem, bot_type))
            generic_expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem))
            if expected_content != self._file_content_if_exists(generic_expected_filename):
                _log.info("Updating " + test_name + " for " + bot_type + " (" + expected_filename + ")")
                self.filesystem.maybe_make_directory(self.filesystem.dirname(expected_filename))
                self.filesystem.write_text_file(expected_filename, expected_content)
            elif self.filesystem.exists(expected_filename):
                self.filesystem.remove(expected_filename)

    def do_update(self):
        if not self.generic_attachment and not self.platform_specific_attachments:
            _log.info("No attachment to process")
            return
        if self.generic_attachment:
            self._update_from_generic_attachment()
        for bot_type, attachment in self.platform_specific_attachments.iteritems():
            self._update_from_platform_specific_attachment(attachment, bot_type)


def main(_argv, _stdout, _stderr):
    parser = argument_parser()
    options, args = parser.parse_known_args(_argv)

    if not args:
        raise Exception("Please provide a bug id or use -a option")

    configure_logging()

    bugzilla_id = args[0]
    updater = TestExpectationUpdater(Host(), bugzilla_id, options.is_bug_id, options.is_attachment_platform_specific)
    updater.do_update()
