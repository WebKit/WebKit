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

import requests
from webkitscmpy import local

from webkitpy.common.host import Host
from webkitpy.common.net.bugzilla.results_fetcher import (
    lookup_ews_results_from_bugzilla,
    lookup_ews_results_from_repo_via_pr,
)
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models import test_expectations

# Buildbot status codes referenced from https://github.com/buildbot/buildbot/blob/master/master/buildbot/process/results.py
EWS_STATECODES = {
    "SUCCESS": 0,
    "WARNINGS": 1,
    "FAILURE": 2,
    "SKIPPED": 3,
    "EXCEPTION": 4,
    "RETRY": 5,
    "CANCELLED": 6,
}


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


def argument_parser(prog=None):
    description = "Update test baselines from CI."
    parser = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        prog=prog,
    )

    parser.add_argument(
        "-d",
        "--debug",
        dest="debug",
        action="store_true",
        default=False,
        help="Log debug messages",
    )

    subparsers = parser.add_subparsers(
        title="Subcommands", dest="subcommand", required=True
    )

    bugzilla_parser = subparsers.add_parser(
        "bugzilla", help="Update from Bugzilla patch, from EWS bots"
    )

    bugzilla_parser.add_argument(
        "-a",
        "--is-attachment-id",
        dest="is_bug_id",
        action="store_false",
        default=True,
        help="Search by attachment id (rather than bug id)",
    )
    bugzilla_parser.add_argument(
        "-b",
        "--bot-filter",
        dest="bot_filter_name",
        action="store",
        default=None,
        help="Only process results for bots where BOT_FILTER_NAME is a substring of the bot name",
    )
    bugzilla_parser.add_argument("bugzilla_id", help="Bugzilla bug ID to lookup")

    github_pr_parser = subparsers.add_parser(
        "github-pr", help="Update from GitHub PR, from EWS bots"
    )

    return parser


class TestExpectationUpdater(object):
    def __init__(self, port):
        self.filesystem = port.host.filesystem
        self.layout_test_repository = port.path_from_webkit_base("LayoutTests")
        self.ews_results = None

    def _tests_to_update(self, platform_name):
        urls = self.ews_results[platform_name]
        if len(urls) == 0:
            return {}

        # Take the last run for a given platform. Because that's what the
        # unittests expect.
        url = urls[-1]

        _log.info("{} archive: {}".format(platform_name, url))
        layout_tests_archive_request = requests.get(url)
        layout_tests_archive_content = layout_tests_archive_request.content
        zip_file = zipfile.ZipFile(io.BytesIO(layout_tests_archive_content))
        results = LayoutTestResults.results_from_string(zip_file.read("full_results.json"))
        results_to_update = [result.test_name for result in results.failing_test_results() if result.type in [test_expectations.TEXT, test_expectations.MISSING]]
        return {result: zip_file.read(TestResultWriter.actual_filename(result, self.filesystem)) for result in results_to_update}

    def _file_content_if_exists(self, filename):
        return self.filesystem.read_binary_file(filename) if self.filesystem.exists(filename) else b""

    def _update_for_generic_bot(self, platform_name):
        for test_name, expected_content in self._tests_to_update(platform_name).items():
            expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem))
            if expected_content != self._file_content_if_exists(expected_filename):
                _log.info("Updating " + test_name + " for " + platform_name + " (" + expected_filename + ")")
                self.filesystem.maybe_make_directory(self.filesystem.dirname(expected_filename))
                self.filesystem.write_binary_file(expected_filename, expected_content)

    def _update_for_platform_specific_bot(self, platform_name):
        for test_name, expected_content in self._tests_to_update(platform_name).items():
            expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem, platform_name))
            generic_expected_filename = self.filesystem.join(self.layout_test_repository, TestResultWriter.expected_filename(test_name, self.filesystem))
            if expected_content != self._file_content_if_exists(generic_expected_filename):
                if expected_content != self._file_content_if_exists(expected_filename):
                    _log.info("Updating " + test_name + " for " + platform_name + " (" + expected_filename + ")")
                    self.filesystem.maybe_make_directory(self.filesystem.dirname(expected_filename))
                    self.filesystem.write_binary_file(expected_filename, expected_content)
            elif self.filesystem.exists(expected_filename):
                _log.info("Updating " + test_name + " for " + platform_name + " ( REMOVED: " + expected_filename + ")")
                self.filesystem.remove(expected_filename)

    def fetch_from_bugzilla(
        self, bugzilla_id, is_bug_id=True, bot_filter_name=None, attachment_fetcher=None
    ):
        self.ews_results = lookup_ews_results_from_bugzilla(
            bugzilla_id,
            is_bug_id,
            attachment_fetcher,
            bot_filter_name,
        )

    def fetch_from_github_pr(self, repository):
        self.ews_results = lookup_ews_results_from_repo_via_pr(repository)

    def do_update(self):
        if len(self.ews_results) == 0:
            if self.bot_filter_name:
                msg = (
                    "Couldn't find any failed EWS result in attachment/patch {} that "
                    "matches filter name."
                ).format(self.patch.id())
                raise RuntimeError(msg)

            msg = (
                "Couldn't find any failed EWS result for attachment/patch {}. "
                "Try to specify a bot filter name manually."
            ).format(self.patch.id())
            raise RuntimeError(msg)

        generic_bots = [platform_name for platform_name in self.ews_results if platform_name == "mac-wk2"]
        platform_bots = [platform_name for platform_name in self.ews_results if platform_name != "mac-wk2"]

        # First update the generic results, so updates for platform bots can use this new generic result as base.
        for platform_name in generic_bots:
            _log.info("Updating results from bot {} (generic)".format(platform_name))
            self._update_for_generic_bot(platform_name)

        for platform_name in platform_bots:
            _log.info("Updating results from bot {} (platform)".format(platform_name))
            self._update_for_platform_specific_bot(platform_name)


def main(_argv, _stdout, _stderr, prog=None):
    parser = argument_parser(prog=prog)
    options = parser.parse_args(_argv)

    configure_logging(options.debug)

    host = Host()
    port = host.port_factory.get()

    updater = TestExpectationUpdater(port)

    if options.subcommand == "bugzilla":
        updater.fetch_from_bugzilla(
            options.bugzilla_id, options.is_bug_id, options.bot_filter_name
        )
    elif options.subcommand == "github-pr":
        repo_path = WebKitFinder(host.filesystem).webkit_base()
        repository = local.Scm.from_path(
            path=repo_path,
        )
        updater.fetch_from_github_pr(repository)
    else:
        raise NotImplementedError("unreachable")

    updater.do_update()
