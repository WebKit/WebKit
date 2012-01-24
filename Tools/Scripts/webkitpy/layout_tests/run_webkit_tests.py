#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
# Copyright (C) 2011 Apple Inc. All rights reserved.
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

import errno
import logging
import optparse
import os
import signal
import sys

from webkitpy.common.host import Host
from webkitpy.layout_tests.controllers.manager import Manager, WorkerException
from webkitpy.layout_tests.views import printing


_log = logging.getLogger(__name__)


def run(port, options, args, regular_output=sys.stderr, buildbot_output=sys.stdout):
    warnings = _set_up_derived_options(port, options)

    printer = printing.Printer(port, options, regular_output, buildbot_output, configure_logging=True)

    for warning in warnings:
        _log.warning(warning)

    if options.help_printing:
        printer.help_printing()
        printer.cleanup()
        return 0

    # We wrap any parts of the run that are slow or likely to raise exceptions
    # in a try/finally to ensure that we clean up the logging configuration.
    unexpected_result_count = -1
    try:
        manager = Manager(port, options, printer)
        manager.print_config()

        if options.lint_test_files:
            return manager.lint()

        printer.print_update("Collecting tests ...")
        try:
            manager.collect_tests(args)
        except IOError, e:
            if e.errno == errno.ENOENT:
                return -1
            raise

        printer.print_update("Checking build ...")
        if not port.check_build(manager.needs_servers()):
            _log.error("Build check failed")
            return -1

        printer.print_update("Parsing expectations ...")
        manager.parse_expectations()

        result_summary = manager.set_up_run()
        if result_summary:
            unexpected_result_count = manager.run(result_summary)
            manager.clean_up_run()
            _log.debug("Testing completed, Exit status: %d" % unexpected_result_count)
    finally:
        printer.cleanup()

    return unexpected_result_count


def _set_up_derived_options(port, options):
    """Sets the options values that depend on other options values."""
    # We return a list of warnings to print after the printer is initialized.
    warnings = []

    if options.worker_model is None:
        options.worker_model = port.default_worker_model()

    if options.worker_model == 'inline':
        if options.child_processes and int(options.child_processes) > 1:
            warnings.append("--worker-model=inline overrides --child-processes")
        options.child_processes = "1"
    if not options.child_processes:
        options.child_processes = os.environ.get("WEBKIT_TEST_CHILD_PROCESSES",
                                                 str(port.default_child_processes()))

    if not options.configuration:
        options.configuration = port.default_configuration()

    if options.pixel_tests is None:
        options.pixel_tests = True

    if not options.time_out_ms:
        if options.configuration == "Debug":
            options.time_out_ms = str(2 * Manager.DEFAULT_TEST_TIMEOUT_MS)
        else:
            options.time_out_ms = str(Manager.DEFAULT_TEST_TIMEOUT_MS)

    options.slow_time_out_ms = str(5 * int(options.time_out_ms))

    if options.additional_platform_directory:
        normalized_platform_directories = []
        for path in options.additional_platform_directory:
            if not port.host.filesystem.isabs(path):
                warnings.append("--additional-platform-directory=%s is ignored since it is not absolute" % path)
                continue
            normalized_platform_directories.append(port.host.filesystem.normpath(path))
        options.additional_platform_directory = normalized_platform_directories

    if not options.http and options.force:
        warnings.append("--no-http is ignored since --force is also provided")
        options.http = True

    return warnings


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
        optparse.make_option("--platform", help="Override port/platform being tested (i.e. chromium-mac)"),
        optparse.make_option("--chromium", action="store_const", const='chromium', dest='platform', help='Alias for --platform=chromium'),
        optparse.make_option('--efl', action='store_const', const='efl', dest="platform", help='Alias for --platform=efl'),
        optparse.make_option('--gtk', action='store_const', const='gtk', dest="platform", help='Alias for --platform=gtk'),
        optparse.make_option('--qt', action='store_const', const='qt', dest="platform", help='Alias for --platform=qt'),
    ]

    print_options = printing.print_options()

    # FIXME: These options should move onto the ChromiumPort.
    chromium_options = [
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
        optparse.make_option("--accelerated-video",
            action="store_true",
            help="Use hardware-accelerated compositing for video"),
        optparse.make_option("--no-accelerated-video",
            action="store_false",
            dest="accelerated_video",
            help="Don't use hardware-accelerated compositing for video"),
        optparse.make_option("--threaded-compositing",
            action="store_true",
            help="Use threaded compositing for rendering"),
        optparse.make_option("--accelerated-2d-canvas",
            action="store_true",
            help="Use hardware-accelerated 2D Canvas calls"),
        optparse.make_option("--no-accelerated-2d-canvas",
            action="store_false",
            dest="accelerated_2d_canvas",
            help="Don't use hardware-accelerated 2D Canvas calls"),
        optparse.make_option("--accelerated-painting",
            action="store_true",
            default=False,
            help="Use hardware accelerated painting of composited pages"),
        optparse.make_option("--enable-hardware-gpu",
            action="store_true",
            default=False,
            help="Run graphics tests on real GPU hardware vs software"),
        optparse.make_option("--per-tile-painting",
            action="store_true",
            help="Use per-tile painting of composited pages"),
    ]

    webkit_options = [
        optparse.make_option("--gc-between-tests", action="store_true", default=False,
            help="Force garbage collection between each test"),
        optparse.make_option("--complex-text", action="store_true", default=False,
            help="Use the complex text code path for all text (Mac OS X and Windows only)"),
        optparse.make_option("-l", "--leaks", action="store_true", default=False,
            help="Enable leaks checking (Mac OS X only)"),
        optparse.make_option("-g", "--guard-malloc", action="store_true", default=False,
            help="Enable malloc guard (Mac OS X only)"),
        optparse.make_option("--threaded", action="store_true", default=False,
            help="Run a concurrent JavaScript thread with each test"),
        optparse.make_option("--webkit-test-runner", "-2", action="store_true",
            help="Use WebKitTestRunner rather than DumpRenderTree."),
        optparse.make_option("--root", action="store",
            help="Path to a pre-built root of WebKit (for running tests using a nightly build of WebKit)"),
    ]

    old_run_webkit_tests_compat = [
        # FIXME: Remove this option once the bots don't refer to it.
        # results.html is smart enough to figure this out itself.
        _compat_shim_option("--use-remote-links-to-tests"),
    ]

    results_options = [
        optparse.make_option("-p", "--pixel-tests", action="store_true",
            dest="pixel_tests", help="Enable pixel-to-pixel PNG comparisons"),
        optparse.make_option("--no-pixel-tests", action="store_false",
            dest="pixel_tests", help="Disable pixel-to-pixel PNG comparisons"),
        optparse.make_option("--no-sample-on-timeout", action="store_false",
            dest="sample_on_timeout", help="Don't run sample on timeout (Mac OS X only)"),
        optparse.make_option("--no-ref-tests", action="store_true",
            dest="no_ref_tests", help="Skip all ref tests"),
        optparse.make_option("--tolerance",
            help="Ignore image differences less than this percentage (some "
                "ports may ignore this option)", type="float"),
        optparse.make_option("--results-directory", help="Location of test results"),
        optparse.make_option("--build-directory",
            help="Path to the directory under which build files are kept (should not include configuration)"),
        optparse.make_option("--new-baseline", action="store_true",
            default=False, help="Save generated results as new baselines "
                 "into the *platform* directory, overwriting whatever's "
                 "already there."),
        optparse.make_option("--reset-results", action="store_true",
            default=False, help="Reset expectations to the "
                 "generated results in their existing location."),
        optparse.make_option("--no-new-test-results", action="store_false",
            dest="new_test_results", default=True,
            help="Don't create new baselines when no expected results exist"),
        optparse.make_option("--skip-failing-tests", action="store_true",
            default=False, help="Skip tests that are expected to fail. "
                 "Note: When using this option, you might miss new crashes "
                 "in these tests."),
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
        optparse.make_option("--noshow-results", action="store_false", dest="show_results", help="Deprecated, same as --no-show-results."),
        optparse.make_option("--no-launch-safari", action="store_false", dest="show_results", help="Deprecated, same as --no-show-results."),
        optparse.make_option("--full-results-html", action="store_true",
            default=False,
            help="Show all failures in results.html, rather than only regressions"),
        optparse.make_option("--clobber-old-results", action="store_true",
            default=False, help="Clobbers test results from previous runs."),
        optparse.make_option("--no-record-results", action="store_false",
            default=True, dest="record_results",
            help="Don't record the results."),
        optparse.make_option("--http", action="store_true", dest="http",
            default=True, help="Run HTTP and WebSocket tests (default)"),
        optparse.make_option("--no-http", action="store_false", dest="http",
            help="Don't run HTTP and WebSocket tests"),
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
                  "DumpRenderTree is relaunched."), type="int", default=None),
        # old-run-webkit-tests calls --run-singly: -1|--singly
        # Isolate each test case run (implies --nthly 1 --verbose)
        optparse.make_option("--run-singly", action="store_true",
            default=False, help="run a separate DumpRenderTree for each test"),
        optparse.make_option("--child-processes",
            help="Number of DumpRenderTrees to run in parallel."),
        # FIXME: Display default number of child processes that will run.
        optparse.make_option("--worker-model", action="store",
            default=None, help=("controls worker model. Valid values are "
                                "'inline' and 'processes'.")),
        optparse.make_option("-f", "--experimental-fully-parallel",
            action="store_true",
            help="run all tests in parallel"),
        optparse.make_option("--no-experimental-fully-parallel",
            action="store_false",
            dest="experimental_fully_parallel",
            help="do not run all tests in parallel"),
        optparse.make_option("--exit-after-n-failures", type="int", default=500,
            help="Exit after the first N failures instead of running all "
            "tests"),
        optparse.make_option("--exit-after-n-crashes-or-timeouts", type="int",
            default=20, help="Exit after the first N crashes instead of "
            "running all tests"),
        optparse.make_option("--iterations", type="int", help="Number of times to run the set of tests (e.g. ABCABCABC)"),
        optparse.make_option("--repeat-each", type="int", help="Number of times to run each test (e.g. AAABBBCCC)"),
        optparse.make_option("--retry-failures", action="store_true",
            default=True,
            help="Re-try any tests that produce unexpected results (default)"),
        optparse.make_option("--no-retry-failures", action="store_false",
            dest="retry_failures",
            help="Don't re-try any tests that produce unexpected results."),
        optparse.make_option("--max-locked-shards", type="int",
            help="Set the maximum number of locked shards"),
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
                   chromium_options + webkit_options + results_options + test_options +
                   misc_options + results_json_options + old_run_webkit_tests_compat)
    option_parser = optparse.OptionParser(option_list=option_list)

    return option_parser.parse_args(args)


def main():
    options, args = parse_args()
    host = Host()
    host._initialize_scm()
    port = host.port_factory.get(options.platform, options)
    return run(port, options, args)


if '__main__' == __name__:
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        # This mirrors what the shell normally does.
        INTERRUPTED_EXIT_STATUS = signal.SIGINT + 128
        sys.exit(INTERRUPTED_EXIT_STATUS)
    except WorkerException:
        # This is a randomly chosen exit code that can be tested against to
        # indicate that an unexpected exception occurred.
        EXCEPTIONAL_EXIT_STATUS = 254
        sys.exit(EXCEPTIONAL_EXIT_STATUS)
