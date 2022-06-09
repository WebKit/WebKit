#!/usr/bin/env python

# Copyright (C) 2017 Apple Inc. All rights reserved.
# Copyright (C) 2020 Igalia S.L.
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

from webkitpy.common.config.urls import ewsserver_default_host
from webkitpy.common.host import Host
from webkitpy.common.net.bugzilla import bugzilla
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models import test_expectations

import json
import re
import requests

# Buildbot status codes referenced from https://github.com/buildbot/buildbot/blob/master/master/buildbot/process/results.py
EWS_STATECODES = {'SUCCESS': 0, 'WARNINGS': 1, 'FAILURE': 2, 'SKIPPED': 3, 'EXCEPTION': 4, 'RETRY': 5, 'CANCELLED': 6}


_log = logging.getLogger(__name__)


def configure_logging(debug=False):
    class LogHandler(logging.StreamHandler):

        def format(self, record):
            if record.levelno > logging.INFO:
                return "%s: %s" % (record.levelname, record.getMessage())
            return record.getMessage()

    log_level = logging.DEBUG if debug else logging.INFO
    logger = logging.getLogger(__name__)
    logger.setLevel(log_level)
    handler = LogHandler()
    handler.setLevel(log_level)
    logger.addHandler(handler)
    return handler


def argument_parser():
    description = """Update expected.txt files from patches submitted by EWS bots on bugzilla. Given id refers to a bug id by default."""
    parser = argparse.ArgumentParser(prog='importupdate-test-expectations-from-bugzilla bugzilla_id', description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-a', '--is-attachment-id', dest='is_bug_id', action='store_false', default=True, help='Whether the given id is a bugzilla attachment and not a bug id')
    parser.add_argument('-b', '--bot-filter', dest='bot_filter_name', action='store', default=None, help='Only process EWS results for bots where BOT_FILTER_NAME its part of the name')
    parser.add_argument('-d', '--debug', dest='debug', action='store_true', default=False, help='Log debug messages')
    return parser


class TestExpectationUpdater(object):

    def __init__(self, host, bugzilla_id, is_bug_id=True, bot_filter_name=None, attachment_fetcher=bugzilla.Bugzilla(), unzip=None):
        self.host = host
        self.filesystem = self.host.filesystem
        self.bot_filter_name = bot_filter_name
        self.unzip = unzip if unzip else lambda content: zipfile.ZipFile(io.BytesIO(content))
        self.layout_test_repository = WebKitFinder(self.filesystem).path_from_webkit_base("LayoutTests")
        if is_bug_id:
            bug_info = attachment_fetcher.fetch_bug(bugzilla_id)
            attachments = [attachments for attachments in bug_info.attachments(include_obsolete=False) if attachments.is_patch()]
            if len(attachments) > 1:
                raise RuntimeError("Found more than one non-obsolete patch in bug {}. Please specify which one to process.".format(bugzilla_id))
            if len(attachments) < 1:
                raise RuntimeError("Couldn't find any non-obsolete patch in bug {}. Please specify which one to process.".format(bugzilla_id))
            self.patch = attachments[0]
        else:
            self.patch = attachment_fetcher.fetch_attachment(bugzilla_id)
        if not self.patch.is_patch():
            raise RuntimeError("Attachment {} its not a patch. Can't continue.".format(bugzilla_id))

    def _platform_name(self, bot_name):
        name = bot_name
        if "mac" in name and name.endswith("-wk2"):
            return "mac-wk2"
        if "mac" in name and not name.endswith("-wk2"):
            return "mac-wk1"
        if "simulator" in name:
            return "ios-wk2"
        if "win-future" in name:
            return "win"
        if "gtk" in name:
            return "gtk"
        if "wpe" in name:
            return "wpe"
        if "-debug" in name:
            name = name.replace("-debug", "")
        return name

    def _get_layout_tests_run(self, bot_name, ews_build_url):
        if not re.match("http.*webkit.org/#/builders/[0-9]+/builds/[0-9]+$", ews_build_url):
            raise ValueError("The URL {} from EWS has an unexpected format".format(ews_build_url))
        ews_buildbot_server, ews_buildbot_path = ews_build_url.split('#')
        ews_buildbot_steps_url = "{}api/v2{}/steps".format(ews_buildbot_server, ews_buildbot_path)
        ews_buildbot_steps_request = requests.get(ews_buildbot_steps_url)
        ews_buildbot_steps = json.loads(ews_buildbot_steps_request.text)
        _log.debug("Trying to finding failed layout test run for bot {} at url {}".format(bot_name, ews_buildbot_steps_url))
        for step in ews_buildbot_steps['steps']:
            # pick the first run as the one for updating the layout tests.
            if step['name'] != 'layout-tests':
                continue
            if not step['complete']:
                _log.info("Skipping to process unfinished layout-tests run for bot {}. Please retry later".format(bot_name))
                continue
            for url_entry in step['urls']:
                url = url_entry['url']
                if url.endswith('.zip'):
                    if url.startswith('/'):
                        url = "{}{}".format(ews_buildbot_server, url.lstrip('/'))
                    return {"layout-tests-results-string": step['state_string'], "layout-tests-archive-url": url}
        return None

    def _lookup_ews_results(self):
        _log.info("Looking for EWS results in patch attachment %s from bug %s" % (self.patch.id(), self.patch.bug().id()))
        ews_bubbles_url = "https://{}/status/{}/".format(ewsserver_default_host, self.patch.id())
        _log.debug("Querying bubble status at {}".format(ews_bubbles_url))
        ews_bubbles_status_request = requests.get(ews_bubbles_url)
        ews_bubbles_status = json.loads(ews_bubbles_status_request.text)
        self.ews_results = {}
        for bot in ews_bubbles_status:
            if ews_bubbles_status[bot]['state'] is None:
                _log.info("EWS bot {} has still not finished. Ignoring".format(bot))
                continue
            _log.debug("Found EWS run at {} for bot {} with state {}".format(ews_bubbles_status[bot]['url'], bot,
                       [STATE for STATE in EWS_STATECODES.keys()][ews_bubbles_status[bot]['state']]))
            if self.bot_filter_name:
                if self.bot_filter_name in bot:
                    self.ews_results[bot] = self._get_layout_tests_run(bot, ews_bubbles_status[bot]['url'])
            elif ews_bubbles_status[bot]['state'] in [EWS_STATECODES['FAILURE'], EWS_STATECODES['WARNINGS']]:
                self.ews_results[bot] = self._get_layout_tests_run(bot, ews_bubbles_status[bot]['url'])

        # Delete entries with null value (if any)
        for bot in self.ews_results.keys():
            if not self.ews_results[bot]:
                del self.ews_results[bot]

        if len(self.ews_results) == 0:
            if self.bot_filter_name:
                raise RuntimeError("Couldn't find any failed EWS result in attachment/patch {} that matches filter name.".format(self.patch.id(), self.bot_filter_name))
            raise RuntimeError("Couldn't find any failed EWS result for attachment/patch {}. Try to specify a bot filter name manually.".format(self.patch.id()))

    def _tests_to_update(self, bot_name):
        _log.info("{} archive: {}".format(bot_name, self.ews_results[bot_name]['layout-tests-archive-url']))
        _log.info("{} status: {}".format(bot_name, self.ews_results[bot_name]['layout-tests-results-string']))
        layout_tests_archive_request = requests.get(self.ews_results[bot_name]['layout-tests-archive-url'])
        layout_tests_archive_content = layout_tests_archive_request.content
        zip_file = self.unzip(layout_tests_archive_content)
        results = LayoutTestResults.results_from_string(zip_file.read("full_results.json"))
        results_to_update = [result.test_name for result in results.failing_test_results() if result.type in [test_expectations.TEXT, test_expectations.MISSING]]
        return {result: zip_file.read(TestResultWriter.actual_filename(result, self.filesystem)) for result in results_to_update}

    def _file_content_if_exists(self, filename):
        return self.filesystem.read_text_file(filename) if self.filesystem.exists(filename) else ""

    def _update_for_generic_bot(self, bot_name):
        for test_name, expected_content in self._tests_to_update(bot_name).items():
            expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem))
            if expected_content != self._file_content_if_exists(expected_filename):
                _log.info("Updating " + test_name + " for " + bot_name + " (" + expected_filename + ")")
                self.filesystem.maybe_make_directory(self.filesystem.dirname(expected_filename))
                self.filesystem.write_text_file(expected_filename, expected_content)

    def _update_for_platform_specific_bot(self, bot_name):
        platform_name = self._platform_name(bot_name)
        for test_name, expected_content in self._tests_to_update(bot_name).items():
            expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem, platform_name))
            generic_expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem))
            if expected_content != self._file_content_if_exists(generic_expected_filename):
                if expected_content != self._file_content_if_exists(expected_filename):
                    _log.info("Updating " + test_name + " for " + bot_name + " (" + expected_filename + ")")
                    self.filesystem.maybe_make_directory(self.filesystem.dirname(expected_filename))
                    self.filesystem.write_text_file(expected_filename, expected_content)
            elif self.filesystem.exists(expected_filename):
                _log.info("Updating " + test_name + " for " + bot_name + " ( REMOVED: " + expected_filename + ")")
                self.filesystem.remove(expected_filename)

    def do_update(self):
        self._lookup_ews_results()
        generic_bots = [bot_name for bot_name in self.ews_results.keys() if self._platform_name(bot_name) == "mac-wk2"]
        platform_bots = [bot_name for bot_name in self.ews_results.keys() if self._platform_name(bot_name) != "mac-wk2"]
        # First update the generic results, so updates for platform bots can use this new generic result as base.
        for bot_name in generic_bots:
            _log.info("Updating results from bot {} (generic)".format(bot_name))
            self._update_for_generic_bot(bot_name)
        for bot_name in platform_bots:
            _log.info("Updating results from bot {} (platform)".format(bot_name))
            self._update_for_platform_specific_bot(bot_name)


def main(_argv, _stdout, _stderr):
    parser = argument_parser()
    options, args = parser.parse_known_args(_argv)

    if not args:
        raise Exception("Please provide a bug id or use -a option")

    configure_logging(options.debug)

    bugzilla_id = args[0]
    updater = TestExpectationUpdater(Host(), bugzilla_id, options.is_bug_id, options.bot_filter_name)
    updater.do_update()
