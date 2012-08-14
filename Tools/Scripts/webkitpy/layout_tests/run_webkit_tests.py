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
import traceback

from webkitpy.common.host import Host
from webkitpy.common.system import stack_utils
from webkitpy.layout_tests.controllers.manager import Manager, WorkerException, TestRunInterruptedException
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.port import port_options
from webkitpy.layout_tests.views import printing


_log = logging.getLogger(__name__)


# This mirrors what the shell normally does.
INTERRUPTED_EXIT_STATUS = signal.SIGINT + 128

# This is a randomly chosen exit code that can be tested against to
# indicate that an unexpected exception occurred.
EXCEPTIONAL_EXIT_STATUS = 254


def lint(port, options):
    host = port.host
    if options.platform:
        ports_to_lint = [port]
    else:
        ports_to_lint = [host.port_factory.get(name) for name in host.port_factory.all_port_names()]

    files_linted = set()
    lint_failed = False

    for port_to_lint in ports_to_lint:
        expectations_file = port_to_lint.path_to_test_expectations_file()
        if expectations_file in files_linted:
            continue

        try:
            test_expectations.TestExpectations(port_to_lint, is_lint_mode=True)
        except test_expectations.ParseError, e:
            lint_failed = True
            _log.error('')
            for warning in e.warnings:
                _log.error(warning)
            _log.error('')
        files_linted.add(expectations_file)

    if lint_failed:
        _log.error('Lint failed.')
        return -1
    _log.info('Lint succeeded.')
    return 0


def run(port, options, args, regular_output=sys.stderr, buildbot_output=sys.stdout):
    try:
        warnings = _set_up_derived_options(port, options)

        printer = printing.Printer(port, options, regular_output, buildbot_output, logger=logging.getLogger())

        for warning in warnings:
            _log.warning(warning)

        if options.lint_test_files:
            return lint(port, options)

        # We wrap any parts of the run that are slow or likely to raise exceptions
        # in a try/finally to ensure that we clean up the logging configuration.
        unexpected_result_count = -1

        manager = Manager(port, options, printer)
        printer.print_config()

        unexpected_result_count = manager.run(args)
        _log.debug("Testing completed, Exit status: %d" % unexpected_result_count)
    except Exception:
        exception_type, exception_value, exception_traceback = sys.exc_info()
        if exception_type not in (KeyboardInterrupt, TestRunInterruptedException, WorkerException):
            print >> sys.stderr, '\n%s raised: %s' % (exception_type.__name__, exception_value)
            stack_utils.log_traceback(_log.error, exception_traceback)
        raise
    finally:
        printer.cleanup()

    return unexpected_result_count


def _set_up_derived_options(port, options):
    """Sets the options values that depend on other options values."""
    # We return a list of warnings to print after the printer is initialized.
    warnings = []

    if not options.child_processes:
        options.child_processes = os.environ.get("WEBKIT_TEST_CHILD_PROCESSES",
                                                 str(port.default_child_processes()))

    if not options.configuration:
        options.configuration = port.default_configuration()

    if options.pixel_tests is None:
        options.pixel_tests = port.default_pixel_tests()

    if not options.time_out_ms:
        options.time_out_ms = str(port.default_timeout_ms())

    options.slow_time_out_ms = str(5 * int(options.time_out_ms))

    if options.additional_platform_directory:
        normalized_platform_directories = []
        for path in options.additional_platform_directory:
            if not port.host.filesystem.isabs(path):
                warnings.append("--additional-platform-directory=%s is ignored since it is not absolute" % path)
                continue
            normalized_platform_directories.append(port.host.filesystem.normpath(path))
        options.additional_platform_directory = normalized_platform_directories

    if not options.http and options.skipped in ('ignore', 'only'):
        warnings.append("--force/--skipped=%s overrides --no-http." % (options.skipped))
        options.http = True

    if options.ignore_metrics and (options.new_baseline or options.reset_results):
        warnings.append("--ignore-metrics has no effect with --new-baselines or with --reset-results")

    if options.new_baseline:
        options.reset_results = True
        options.add_platform_exceptions = True

    if options.pixel_test_directories:
        options.pixel_tests = True
        varified_dirs = set()
        pixel_test_directories = options.pixel_test_directories
        for directory in pixel_test_directories:
            # FIXME: we should support specifying the directories all the ways we support it for additional
            # arguments specifying which tests and directories to run. We should also move the logic for that
            # to Port.
            filesystem = port.host.filesystem
            if not filesystem.isdir(filesystem.join(port.layout_tests_dir(), directory)):
                warnings.append("'%s' was passed to --pixel-test-directories, which doesn't seem to be a directory" % str(directory))
            else:
                varified_dirs.add(directory)

        options.pixel_test_directories = list(varified_dirs)

    if options.run_singly:
        options.verbose = True

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

    option_group_definitions = []

    option_group_definitions.append(("Configuration options", port_options()))
    option_group_definitions.append(("Printing Options", printing.print_options()))

    # FIXME: These options should move onto the ChromiumPort.
    option_group_definitions.append(("Chromium-specific Options", [
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
        optparse.make_option("--per-tile-painting",
            action="store_true",
            help="Use per-tile painting of composited pages"),
        optparse.make_option("--adb-device",
            action="append", default=[],
            help="Run Android layout tests on these devices."),
    ]))

    option_group_definitions.append(("EFL-specific Options", [
        optparse.make_option("--webprocess-cmd-prefix", type="string",
            default=False, help="Prefix used when spawning the Web process (Debug mode only)"),
    ]))

    option_group_definitions.append(("WebKit Options", [
        optparse.make_option("--gc-between-tests", action="store_true", default=False,
            help="Force garbage collection between each test"),
        optparse.make_option("--complex-text", action="store_true", default=False,
            help="Use the complex text code path for all text (Mac OS X and Windows only)"),
        optparse.make_option("-l", "--leaks", action="store_true", default=False,
            help="Enable leaks checking (Mac OS X only)"),
        optparse.make_option("-g", "--guard-malloc", action="store_true", default=False,
            help="Enable Guard Malloc (Mac OS X only)"),
        optparse.make_option("--threaded", action="store_true", default=False,
            help="Run a concurrent JavaScript thread with each test"),
        optparse.make_option("--webkit-test-runner", "-2", action="store_true",
            help="Use WebKitTestRunner rather than DumpRenderTree."),
        # FIXME: We should merge this w/ --build-directory and only have one flag.
        optparse.make_option("--root", action="store",
            help="Path to a directory containing the executables needed to run tests."),
    ]))

    option_group_definitions.append(("ORWT Compatibility Options", [
        # FIXME: Remove this option once the bots don't refer to it.
        # results.html is smart enough to figure this out itself.
        _compat_shim_option("--use-remote-links-to-tests"),
    ]))

    option_group_definitions.append(("Results Options", [
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
        optparse.make_option("--add-platform-exceptions", action="store_true", default=False,
            help="Save generated results into the *most-specific-platform* directory rather than the *generic-platform* directory"),
        optparse.make_option("--new-baseline", action="store_true",
            default=False, help="Save generated results as new baselines "
                 "into the *most-specific-platform* directory, overwriting whatever's "
                 "already there. Equivalent to --reset-results --add-platform-exceptions"),
        optparse.make_option("--reset-results", action="store_true",
            default=False, help="Reset expectations to the "
                 "generated results in their existing location."),
        optparse.make_option("--no-new-test-results", action="store_false",
            dest="new_test_results", default=True,
            help="Don't create new baselines when no expected results exist"),

        #FIXME: we should support a comma separated list with --pixel-test-directory as well.
        optparse.make_option("--pixel-test-directory", action="append", default=[], dest="pixel_test_directories",
            help="A directory where it is allowed to execute tests as pixel tests. "
                 "Specify multiple times to add multiple directories. "
                 "This option implies --pixel-tests. If specified, only those tests "
                 "will be executed as pixel tests that are located in one of the "
                 "directories enumerated with the option. Some ports may ignore this "
                 "option while others can have a default value that can be overridden here."),

        optparse.make_option("--skip-failing-tests", action="store_true",
            default=False, help="Skip tests that are expected to fail. "
                 "Note: When using this option, you might miss new crashes "
                 "in these tests."),
        optparse.make_option("--additional-drt-flag", action="append",
            default=[], help="Additional command line flag to pass to DumpRenderTree "
                 "Specify multiple times to add multiple flags."),
        optparse.make_option("--driver-name", type="string",
            help="Alternative DumpRenderTree binary to use"),
        optparse.make_option("--additional-platform-directory", action="append",
            default=[], help="Additional directory where to look for test "
                 "baselines (will take precendence over platform baselines). "
                 "Specify multiple times to add multiple search path entries."),
        optparse.make_option("--additional-expectations", action="append", default=[],
            help="Path to a test_expectations file that will override previous expectations. "
                 "Specify multiple times for multiple sets of overrides."),
        optparse.make_option("--compare-port", action="store", default=None,
            help="Use the specified port's baselines first"),
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
        optparse.make_option("--ignore-metrics", action="store_true", dest="ignore_metrics",
            default=False, help="Ignore rendering metrics related information from test "
            "output, only compare the structure of the rendertree."),
    ]))

    option_group_definitions.append(("Testing Options", [
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
        optparse.make_option("--wrapper",
            help="wrapper command to insert before invocations of "
                 "DumpRenderTree; option is split on whitespace before "
                 "running. (Example: --wrapper='valgrind --smc-check=all')"),
        optparse.make_option("-i", "--ignore-tests", action="append", default=[],
            help="directories or test to ignore (may specify multiple times)"),
        optparse.make_option("--test-list", action="append",
            help="read list of tests to run from file", metavar="FILE"),
        optparse.make_option("--skipped", action="store", default="default",
            help="control how tests marked SKIP are run. 'default' == Skip, 'ignore' == Run them anyway, 'only' == only run the SKIP tests."),
        optparse.make_option("--force", dest="skipped", action="store_const", const='ignore',
            help="Run all tests, even those marked SKIP in the test list (same as --skipped=ignore)"),
        optparse.make_option("--time-out-ms",
            help="Set the timeout for each test"),
        optparse.make_option("--randomize-order", action="store_true",
            default=False, help=("Run tests in random order (useful "
                                "for tracking down corruption)")),
        optparse.make_option("--run-chunk",
            help=("Run a specified chunk (n:l), the nth of len l, "
                 "of the layout tests")),
        optparse.make_option("--run-part", help=("Run a specified part (n:m), "
                  "the nth of m parts, of the layout tests")),
        optparse.make_option("--batch-size",
            help=("Run a the tests in batches (n), after every n tests, "
                  "DumpRenderTree is relaunched."), type="int", default=None),
        optparse.make_option("--run-singly", action="store_true",
            default=False, help="run a separate DumpRenderTree for each test (implies --verbose)"),
        optparse.make_option("--child-processes",
            help="Number of DumpRenderTrees to run in parallel."),
        # FIXME: Display default number of child processes that will run.
        optparse.make_option("-f", "--fully-parallel", action="store_true",
            help="run all tests in parallel"),
        optparse.make_option("--exit-after-n-failures", type="int", default=None,
            help="Exit after the first N failures instead of running all "
            "tests"),
        optparse.make_option("--exit-after-n-crashes-or-timeouts", type="int",
            default=None, help="Exit after the first N crashes instead of "
            "running all tests"),
        optparse.make_option("--iterations", type="int", default=1, help="Number of times to run the set of tests (e.g. ABCABCABC)"),
        optparse.make_option("--repeat-each", type="int", default=1, help="Number of times to run each test (e.g. AAABBBCCC)"),
        optparse.make_option("--retry-failures", action="store_true",
            default=True,
            help="Re-try any tests that produce unexpected results (default)"),
        optparse.make_option("--no-retry-failures", action="store_false",
            dest="retry_failures",
            help="Don't re-try any tests that produce unexpected results."),
        optparse.make_option("--max-locked-shards", type="int", default=1,
            help="Set the maximum number of locked shards"),
        optparse.make_option("--additional-env-var", type="string", action="append", default=[],
            help="Passes that environment variable to the tests (--additional-env-var=NAME=VALUE)"),
    ]))

    option_group_definitions.append(("Miscellaneous Options", [
        optparse.make_option("--lint-test-files", action="store_true",
        default=False, help=("Makes sure the test files parse for all "
                            "configurations. Does not run any tests.")),
    ]))

    # FIXME: Move these into json_results_generator.py
    option_group_definitions.append(("Result JSON Options", [
        optparse.make_option("--master-name", help="The name of the buildbot master."),
        optparse.make_option("--builder-name", default="",
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
    ]))

    option_parser = optparse.OptionParser()

    for group_name, group_options in option_group_definitions:
        option_group = optparse.OptionGroup(option_parser, group_name)
        option_group.add_options(group_options)
        option_parser.add_option_group(option_group)

    return option_parser.parse_args(args)


def main(argv=None):
    try:
        options, args = parse_args(argv)
        if options.platform and 'test' in options.platform:
            # It's a bit lame to import mocks into real code, but this allows the user
            # to run tests against the test platform interactively, which is useful for
            # debugging test failures.
            from webkitpy.common.host_mock import MockHost
            host = MockHost()
        else:
            host = Host()
        port = host.port_factory.get(options.platform, options)
    except NotImplementedError, e:
        # FIXME: is this the best way to handle unsupported port names?
        print >> sys.stderr, str(e)
        return EXCEPTIONAL_EXIT_STATUS
    except Exception, e:
        print >> sys.stderr, '\n%s raised: %s' % (e.__class__.__name__, str(e))
        traceback.print_exc(file=sys.stderr)
        raise

    logging.getLogger().setLevel(logging.DEBUG if options.debug_rwt_logging else logging.INFO)
    return run(port, options, args)


if '__main__' == __name__:
    try:
        return_code = main()
    except BaseException, e:
        if e.__class__ in (KeyboardInterrupt, TestRunInterruptedException):
            sys.exit(INTERRUPTED_EXIT_STATUS)
        sys.exit(EXCEPTIONAL_EXIT_STATUS)

    sys.exit(return_code)
