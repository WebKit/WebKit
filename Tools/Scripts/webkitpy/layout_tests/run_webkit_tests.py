#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

"""Run layout tests."""

import errno
import logging
import optparse
import os
import signal
import sys

from layout_package import json_results_generator
from layout_package import printing
from layout_package import test_runner
from layout_package import test_runner2

from webkitpy.common.system import user
from webkitpy.thirdparty import simplejson

import port

_log = logging.getLogger(__name__)


def run(port, options, args, regular_output=sys.stderr,
        buildbot_output=sys.stdout):
    """Run the tests.

    Args:
      port: Port object for port-specific behavior
      options: a dictionary of command line options
      args: a list of sub directories or files to test
      regular_output: a stream-like object that we can send logging/debug
          output to
      buildbot_output: a stream-like object that we can write all output that
          is intended to be parsed by the buildbot to
    Returns:
      the number of unexpected results that occurred, or -1 if there is an
          error.

    """
    warnings = _set_up_derived_options(port, options)

    printer = printing.Printer(port, options, regular_output, buildbot_output,
        int(options.child_processes), options.experimental_fully_parallel)
    for w in warnings:
        _log.warning(w)

    if options.help_printing:
        printer.help_printing()
        printer.cleanup()
        return 0

    last_unexpected_results = _gather_unexpected_results(port)
    if options.print_last_failures:
        printer.write("\n".join(last_unexpected_results) + "\n")
        printer.cleanup()
        return 0

    # We wrap any parts of the run that are slow or likely to raise exceptions
    # in a try/finally to ensure that we clean up the logging configuration.
    num_unexpected_results = -1
    try:
        runner = test_runner2.TestRunner2(port, options, printer)
        runner._print_config()

        printer.print_update("Collecting tests ...")
        try:
            runner.collect_tests(args, last_unexpected_results)
        except IOError, e:
            if e.errno == errno.ENOENT:
                return -1
            raise

        if options.lint_test_files:
            return runner.lint()

        printer.print_update("Parsing expectations ...")
        runner.parse_expectations()

        printer.print_update("Checking build ...")
        if not port.check_build(runner.needs_http()):
            _log.error("Build check failed")
            return -1

        result_summary = runner.set_up_run()
        if result_summary:
            num_unexpected_results = runner.run(result_summary)
            runner.clean_up_run()
            _log.debug("Testing completed, Exit status: %d" %
                       num_unexpected_results)
    finally:
        printer.cleanup()

    return num_unexpected_results


def _set_up_derived_options(port_obj, options):
    """Sets the options values that depend on other options values."""
    # We return a list of warnings to print after the printer is initialized.
    warnings = []

    if options.worker_model is None:
        options.worker_model = port_obj.default_worker_model()

    if options.worker_model == 'inline':
        if options.child_processes and int(options.child_processes) > 1:
            warnings.append("--worker-model=inline overrides --child-processes")
        options.child_processes = "1"
    if not options.child_processes:
        options.child_processes = os.environ.get("WEBKIT_TEST_CHILD_PROCESSES",
                                                 str(port_obj.default_child_processes()))

    if not options.configuration:
        options.configuration = port_obj.default_configuration()

    if options.pixel_tests is None:
        options.pixel_tests = True

    if not options.use_apache:
        options.use_apache = sys.platform in ('darwin', 'linux2')

    if not options.time_out_ms:
        if options.configuration == "Debug":
            options.time_out_ms = str(2 * test_runner.TestRunner.DEFAULT_TEST_TIMEOUT_MS)
        else:
            options.time_out_ms = str(test_runner.TestRunner.DEFAULT_TEST_TIMEOUT_MS)

    options.slow_time_out_ms = str(5 * int(options.time_out_ms))

    if options.additional_platform_directory:
        normalized_platform_directories = []
        for path in options.additional_platform_directory:
            if not port_obj._filesystem.isabs(path):
                warnings.append("--additional-platform-directory=%s is ignored since it is not absolute" % path)
                continue
            normalized_platform_directories.append(port_obj._filesystem.normpath(path))
        options.additional_platform_directory = normalized_platform_directories

    return warnings


def _gather_unexpected_results(port):
    """Returns the unexpected results from the previous run, if any."""
    filesystem = port._filesystem
    results_directory = port.results_directory()
    options = port._options
    last_unexpected_results = []
    if options.print_last_failures or options.retest_last_failures:
        unexpected_results_filename = filesystem.join(results_directory, "unexpected_results.json")
        if filesystem.exists(unexpected_results_filename):
            results = json_results_generator.load_json(filesystem, unexpected_results_filename)
            last_unexpected_results = results['tests'].keys()
    return last_unexpected_results


def _compat_shim_callback(option, opt_str, value, parser):
    print "Ignoring unsupported option: %s" % opt_str


def _compat_shim_option(option_name, **kwargs):
    return optparse.make_option(option_name, action="callback",
        callback=_compat_shim_callback,
        help="Ignored, for old-run-webkit-tests compat only.", **kwargs)


def parse_args(args=None):
    """Provides a default set of command line args.

    Returns a tuple of options, args from optparse"""

    # FIXME: All of these options should be stored closer to the code which
    # FIXME: actually uses them. configuration_options should move
    # FIXME: to WebKitPort and be shared across all scripts.
    configuration_options = [
        optparse.make_option("-t", "--target", dest="configuration",
                             help="(DEPRECATED)"),
        # FIXME: --help should display which configuration is default.
        optparse.make_option('--debug', action='store_const', const='Debug',
                             dest="configuration",
                             help='Set the configuration to Debug'),
        optparse.make_option('--release', action='store_const',
                             const='Release', dest="configuration",
                             help='Set the configuration to Release'),
        # old-run-webkit-tests also accepts -c, --configuration CONFIGURATION.
    ]

    print_options = printing.print_options()

    # FIXME: These options should move onto the ChromiumPort.
    chromium_options = [
        optparse.make_option("--chromium", action="store_true", default=False,
            help="use the Chromium port"),
        optparse.make_option("--startup-dialog", action="store_true",
            default=False, help="create a dialog on DumpRenderTree startup"),
        optparse.make_option("--gp-fault-error-box", action="store_true",
            default=False, help="enable Windows GP fault error box"),
        optparse.make_option("--js-flags",
            type="string", help="JavaScript flags to pass to tests"),
        optparse.make_option("--stress-opt", action="store_true",
            default=False,
            help="Enable additional stress test to JavaScript optimization"),
        optparse.make_option("--stress-deopt", action="store_true",
            default=False,
            help="Enable additional stress test to JavaScript optimization"),
        optparse.make_option("--nocheck-sys-deps", action="store_true",
            default=False,
            help="Don't check the system dependencies (themes)"),
        optparse.make_option("--accelerated-compositing",
            action="store_true",
            help="Use hardware-accelerated compositing for rendering"),
        optparse.make_option("--no-accelerated-compositing",
            action="store_false",
            dest="accelerated_compositing",
            help="Don't use hardware-accelerated compositing for rendering"),
        optparse.make_option("--accelerated-2d-canvas",
            action="store_true",
            help="Use hardware-accelerated 2D Canvas calls"),
        optparse.make_option("--no-accelerated-2d-canvas",
            action="store_false",
            dest="accelerated_2d_canvas",
            help="Don't use hardware-accelerated 2D Canvas calls"),
        optparse.make_option("--enable-hardware-gpu",
            action="store_true",
            default=False,
            help="Run graphics tests on real GPU hardware vs software"),
    ]

    # Missing Mac-specific old-run-webkit-tests options:
    # FIXME: Need: -g, --guard for guard malloc support on Mac.
    # FIXME: Need: -l --leaks    Enable leaks checking.
    # FIXME: Need: --sample-on-timeout Run sample on timeout

    old_run_webkit_tests_compat = [
        # NRWT doesn't generate results by default anyway.
        _compat_shim_option("--no-new-test-results"),
        # NRWT doesn't sample on timeout yet anyway.
        _compat_shim_option("--no-sample-on-timeout"),
        # FIXME: NRWT needs to support remote links eventually.
        _compat_shim_option("--use-remote-links-to-tests"),
    ]

    results_options = [
        # NEED for bots: --use-remote-links-to-tests Link to test files
        # within the SVN repository in the results.
        optparse.make_option("-p", "--pixel-tests", action="store_true",
            dest="pixel_tests", help="Enable pixel-to-pixel PNG comparisons"),
        optparse.make_option("--no-pixel-tests", action="store_false",
            dest="pixel_tests", help="Disable pixel-to-pixel PNG comparisons"),
        optparse.make_option("--tolerance",
            help="Ignore image differences less than this percentage (some "
                "ports may ignore this option)", type="float"),
        optparse.make_option("--results-directory", help="Location of test results"),
        optparse.make_option("--build-directory",
            help="Path to the directory under which build files are kept (should not include configuration)"),
        optparse.make_option("--new-baseline", action="store_true",
            default=False, help="Save all generated results as new baselines "
                 "into the platform directory, overwriting whatever's "
                 "already there."),
        optparse.make_option("--reset-results", action="store_true",
            default=False, help="Reset any existing baselines to the "
                 "generated results"),
        optparse.make_option("--additional-drt-flag", action="append",
            default=[], help="Additional command line flag to pass to DumpRenderTree "
                 "Specify multiple times to add multiple flags."),
        optparse.make_option("--additional-platform-directory", action="append",
            default=[], help="Additional directory where to look for test "
                 "baselines (will take precendence over platform baselines). "
                 "Specify multiple times to add multiple search path entries."),
        optparse.make_option("--no-show-results", action="store_false",
            default=True, dest="show_results",
            help="Don't launch a browser with results after the tests "
                 "are done"),
        # FIXME: We should have a helper function to do this sort of
        # deprectated mapping and automatically log, etc.
        optparse.make_option("--noshow-results", action="store_false",
            dest="show_results",
            help="Deprecated, same as --no-show-results."),
        optparse.make_option("--no-launch-safari", action="store_false",
            dest="show_results",
            help="old-run-webkit-tests compat, same as --noshow-results."),
        # old-run-webkit-tests:
        # --[no-]launch-safari    Launch (or do not launch) Safari to display
        #                         test results (default: launch)
        optparse.make_option("--full-results-html", action="store_true",
            default=False,
            help="Show all failures in results.html, rather than only "
                 "regressions"),
        optparse.make_option("--clobber-old-results", action="store_true",
            default=False, help="Clobbers test results from previous runs."),
        optparse.make_option("--platform",
            help="Override the platform for expected results"),
        optparse.make_option("--no-record-results", action="store_false",
            default=True, dest="record_results",
            help="Don't record the results."),
        # old-run-webkit-tests also has HTTP toggle options:
        # --[no-]http                     Run (or do not run) http tests
        #                                 (default: run)
    ]

    test_options = [
        optparse.make_option("--build", dest="build",
            action="store_true", default=True,
            help="Check to ensure the DumpRenderTree build is up-to-date "
                 "(default)."),
        optparse.make_option("--no-build", dest="build",
            action="store_false", help="Don't check to see if the "
                                       "DumpRenderTree build is up-to-date."),
        optparse.make_option("-n", "--dry-run", action="store_true",
            default=False,
            help="Do everything but actually run the tests or upload results."),
        # old-run-webkit-tests has --valgrind instead of wrapper.
        optparse.make_option("--wrapper",
            help="wrapper command to insert before invocations of "
                 "DumpRenderTree; option is split on whitespace before "
                 "running. (Example: --wrapper='valgrind --smc-check=all')"),
        # old-run-webkit-tests:
        # -i|--ignore-tests               Comma-separated list of directories
        #                                 or tests to ignore
        optparse.make_option("--test-list", action="append",
            help="read list of tests to run from file", metavar="FILE"),
        # old-run-webkit-tests uses --skipped==[default|ignore|only]
        # instead of --force:
        optparse.make_option("--force", action="store_true", default=False,
            help="Run all tests, even those marked SKIP in the test list"),
        optparse.make_option("--use-apache", action="store_true",
            default=False, help="Whether to use apache instead of lighttpd."),
        optparse.make_option("--time-out-ms",
            help="Set the timeout for each test"),
        # old-run-webkit-tests calls --randomize-order --random:
        optparse.make_option("--randomize-order", action="store_true",
            default=False, help=("Run tests in random order (useful "
                                "for tracking down corruption)")),
        optparse.make_option("--run-chunk",
            help=("Run a specified chunk (n:l), the nth of len l, "
                 "of the layout tests")),
        optparse.make_option("--run-part", help=("Run a specified part (n:m), "
                  "the nth of m parts, of the layout tests")),
        # old-run-webkit-tests calls --batch-size: --nthly n
        #   Restart DumpRenderTree every n tests (default: 1000)
        optparse.make_option("--batch-size",
            help=("Run a the tests in batches (n), after every n tests, "
                  "DumpRenderTree is relaunched."), type="int", default=0),
        # old-run-webkit-tests calls --run-singly: -1|--singly
        # Isolate each test case run (implies --nthly 1 --verbose)
        optparse.make_option("--run-singly", action="store_true",
            default=False, help="run a separate DumpRenderTree for each test"),
        optparse.make_option("--child-processes",
            help="Number of DumpRenderTrees to run in parallel."),
        # FIXME: Display default number of child processes that will run.
        optparse.make_option("--worker-model", action="store",
            default=None, help=("controls worker model. Valid values are "
                                "'inline', 'threads', and 'processes'.")),
        optparse.make_option("--experimental-fully-parallel",
            action="store_true", default=False,
            help="run all tests in parallel"),
        optparse.make_option("--exit-after-n-failures", type="int", default=500,
            help="Exit after the first N failures instead of running all "
            "tests"),
        optparse.make_option("--exit-after-n-crashes-or-timeouts", type="int",
            default=20, help="Exit after the first N crashes instead of "
            "running all tests"),
        # FIXME: consider: --iterations n
        #      Number of times to run the set of tests (e.g. ABCABCABC)
        optparse.make_option("--print-last-failures", action="store_true",
            default=False, help="Print the tests in the last run that "
            "had unexpected failures (or passes) and then exit."),
        optparse.make_option("--retest-last-failures", action="store_true",
            default=False, help="re-test the tests in the last run that "
            "had unexpected failures (or passes)."),
        optparse.make_option("--retry-failures", action="store_true",
            default=True,
            help="Re-try any tests that produce unexpected results (default)"),
        optparse.make_option("--no-retry-failures", action="store_false",
            dest="retry_failures",
            help="Don't re-try any tests that produce unexpected results."),
    ]

    misc_options = [
        optparse.make_option("--lint-test-files", action="store_true",
        default=False, help=("Makes sure the test files parse for all "
                            "configurations. Does not run any tests.")),
    ]

    # FIXME: Move these into json_results_generator.py
    results_json_options = [
        optparse.make_option("--master-name", help="The name of the buildbot master."),
        optparse.make_option("--builder-name", default="DUMMY_BUILDER_NAME",
            help=("The name of the builder shown on the waterfall running "
                  "this script e.g. WebKit.")),
        optparse.make_option("--build-name", default="DUMMY_BUILD_NAME",
            help=("The name of the builder used in its path, e.g. "
                  "webkit-rel.")),
        optparse.make_option("--build-number", default="DUMMY_BUILD_NUMBER",
            help=("The build number of the builder running this script.")),
        optparse.make_option("--test-results-server", default="",
            help=("If specified, upload results json files to this appengine "
                  "server.")),
    ]

    option_list = (configuration_options + print_options +
                   chromium_options + results_options + test_options +
                   misc_options + results_json_options +
                   old_run_webkit_tests_compat)
    option_parser = optparse.OptionParser(option_list=option_list)

    return option_parser.parse_args(args)


def main():
    options, args = parse_args()
    port_obj = port.get(options.platform, options)
    return run(port_obj, options, args)


if '__main__' == __name__:
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        # this mirrors what the shell normally does
        sys.exit(signal.SIGINT + 128)
