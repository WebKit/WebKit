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
import logging
import zipfile

import requests

from webkitpy.common.host import Host
from webkitpy.common.net.bugzilla import bugzilla
from webkitpy.common.net.bugzilla.results_fetcher import (
    lookup_ews_results_from_bugzilla,
)
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.version_name_map import VersionNameMap
from webkitpy.layout_tests.controllers.test_result_writer import TestResultWriter
from webkitpy.layout_tests.models import test_expectations

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
    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-a', '--is-attachment-id', dest='is_bug_id', action='store_false', default=True, help='Whether the given id is a bugzilla attachment and not a bug id')
    parser.add_argument('-b', '--bot-filter', dest='bot_filter_name', action='store', default=None, help='Only process EWS results for bots where BOT_FILTER_NAME its part of the name')
    parser.add_argument('-d', '--debug', dest='debug', action='store_true', default=False, help='Log debug messages')
    parser.add_argument('bugzilla_id', help='Bugzilla ID to lookup')
    return parser


class TestExpectationUpdater(object):

    def __init__(self, port, bugzilla_id, is_bug_id=True, bot_filter_name=None, attachment_fetcher=None):
        self.attachment_fetcher = bugzilla.Bugzilla() if attachment_fetcher is None else attachment_fetcher

        self.filesystem = port.host.filesystem
        self.bot_filter_name = bot_filter_name
        self.bugzilla_id = bugzilla_id
        self.is_bug_id = is_bug_id

        self.layout_test_repository = port.path_from_webkit_base("LayoutTests")

        self.ews_results = None

    def _update_for_bot_results(
        self, platform_name, test_configuration, results, zip_file, generic_platform_name=None
    ):
        split_platform_name = platform_name.split("-")

        if test_configuration["version"] in split_platform_name:
            split_platform_name.remove(test_configuration["version"])

        if split_platform_name[-1] not in ("wk1", "wk2"):
            split_platform_name.append("wk1")

        if "simulator" in split_platform_name:
            split_platform_name.remove("simulator")

        used_platform_name = "-".join(split_platform_name)

        fs = self.filesystem
        results_to_update = [
            result.test_name
            for result in results.failing_test_results()
            if result.type in [test_expectations.TEXT, test_expectations.MISSING]
        ]

        for result in results_to_update:
            actual_filename = TestResultWriter.actual_filename(result, fs)
            actual_info = zip_file.getinfo(actual_filename)
            actual_size = actual_info.file_size

            generic_name = fs.join(
                self.layout_test_repository,
                TestResultWriter.expected_filename(result, fs),
            )

            if platform_name == generic_platform_name:
                expected_name = generic_name
            else:
                expected_name = fs.join(
                    self.layout_test_repository,
                    TestResultWriter.expected_filename(result, fs, port_name=used_platform_name),
                )

            if (
                generic_platform_name is not None
                and platform_name != generic_platform_name
                and fs.exists(generic_name)
                and fs.getsize(generic_name) == actual_size
                and fs.read_binary_file(generic_name) == zip_file.read(actual_info)
            ):
                # If we have generic platform, and we're not that generic platform, but
                # the generic result matches the actual result, we can just delete any
                # existing expected result.
                if fs.exists(expected_name):
                    self.filesystem.remove(expected_name)

            elif not (
                fs.exists(expected_name)
                and fs.getsize(expected_name) == actual_size
                and fs.read_binary_file(expected_name) == zip_file.read(actual_info)
            ):
                fs.maybe_make_directory(fs.dirname(expected_name))
                fs.write_binary_file(expected_name, zip_file.read(actual_info))

    def _config_for_bot(self, bot_name):
        # This should be effectively frozen, as it is only needed for old layout-tests
        # results files which don't include port and test configuration within them.
        if bot_name == "gtk-wk2":
            return (
                "gtk-wk2",
                {"version": "", "architecture": "x86", "build_type": "release"},
            )
        elif bot_name in ("ios-wk2", "ios-wk2-wpt"):
            return (
                "iphone-simulator-sonoma-wk2",
                {"version": "", "architecture": "arm64", "build_type": "release"},
            )
        elif bot_name == "mac-AS-debug-wk2":
            return (
                "mac-sonoma-wk2",
                {"version": "sonoma", "architecture": "arm64", "build_type": "debug"},
            )
        elif bot_name == "mac-wk1":
            return (
                "mac-monterey",
                {
                    "version": "monterey",
                    "architecture": "x86_64",
                    "build_type": "release",
                },
            )
        elif bot_name in ("mac-wk2", "mac-wk2-stress"):
            return (
                "mac-monterey-wk2",
                {
                    "version": "monterey",
                    "architecture": "x86_64",
                    "build_type": "release",
                },
            )
        else:
            msg = "unknown bot, but also why doesn't the full_results.json tell us the config?"
            raise ValueError(msg)

    def do_update(self):
        self.ews_results = lookup_ews_results_from_bugzilla(
            self.bugzilla_id,
            self.is_bug_id,
            self.attachment_fetcher,
            self.bot_filter_name,
        )

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

        # This is order from oldest to newest.
        mac_port_candidates = [
            "mac-" + name.replace(" ", "").lower() + "-wk2"
            for name in VersionNameMap().names("mac")
        ]

        best_mac_port_idx = -1
        generic_bots = []
        platform_bots = []
        skipped_bots = []

        for bot_name, urls in self.ews_results.items():
            for url in urls:
                f, path = self.filesystem.open_binary_tempfile("")
                r = requests.get(url, stream=True)
                for chunk in r.iter_content(chunk_size=0xFFFF):
                    f.write(chunk)
                # Close and reopen because we don't support seeking our writeable mock file...
                f.close()
                f = self.filesystem.open_binary_file_for_reading(path)
                zip_file = zipfile.ZipFile(f, "r")
                results = LayoutTestResults.results_from_string(
                    zip_file.read("full_results.json")
                )

                port_name = results.port_name()
                test_configuration = results.test_configuration()
                if port_name is None and test_configuration is None:
                    port_name, test_configuration = self._config_for_bot(bot_name)

                archive = (
                    path,
                    bot_name,
                    port_name,
                    test_configuration,
                    results,
                    zip_file,
                )
                if test_configuration["build_type"] != "release":
                    skipped_bots.append(archive)
                elif port_name in mac_port_candidates:
                    idx = mac_port_candidates.index(port_name)
                    if idx > best_mac_port_idx:
                        platform_bots.extend(generic_bots)
                        generic_bots = [archive]
                        best_mac_port_idx = idx
                    elif idx == best_mac_port_idx:
                        generic_bots.append(archive)
                    else:
                        platform_bots.append(archive)
                else:
                    platform_bots.append(archive)

        if best_mac_port_idx == -1:
            generic_mac_port = None
        else:
            generic_mac_port = mac_port_candidates[best_mac_port_idx]

        # First update the generic results, so updates for platform bots can use this
        # new generic result as base.
        for _, _, platform_name, test_configuration, results, zip_file in (generic_bots + platform_bots):
            _log.info("Updating results from bot {}".format(platform_name))
            self._update_for_bot_results(
                platform_name, test_configuration, results, zip_file, generic_platform_name=generic_mac_port
            )


def main(_argv, _stdout, _stderr):
    parser = argument_parser()
    options = parser.parse_args(_argv)

    configure_logging(options.debug)

    host = Host()
    port = host.port_factory.get()

    updater = TestExpectationUpdater(port, options.bugzilla_id, options.is_bug_id, options.bot_filter_name)
    updater.do_update()
