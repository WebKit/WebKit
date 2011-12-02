#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Chromium implementations of the Port interface."""

import base64
import errno
import logging
import re
import signal
import subprocess
import sys
import time
import webbrowser

from webkitpy.common.config import urls
from webkitpy.common.system import executive
from webkitpy.common.system.path import cygpath
from webkitpy.layout_tests.controllers.manager import Manager
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port.base import Port
from webkitpy.layout_tests.port.driver import Driver, DriverOutput
from webkitpy.layout_tests.port import builders
from webkitpy.layout_tests.servers import http_server
from webkitpy.layout_tests.servers import websocket_server


_log = logging.getLogger(__name__)


class ChromiumPort(Port):
    """Abstract base class for Chromium implementations of the Port class."""

    port_name = "chromium"

    ALL_SYSTEMS = (
        ('leopard', 'x86'),
        ('snowleopard', 'x86'),
        ('lion', 'x86'),
        ('xp', 'x86'),
        ('vista', 'x86'),
        ('win7', 'x86'),
        ('lucid', 'x86'),
        ('lucid', 'x86_64'))

    ALL_GRAPHICS_TYPES = ('cpu', 'gpu')
    CORE_GRAPHICS_VERSIONS = ('leopard', 'snowleopard', 'lion')
    CORE_GRAPHICS_TYPES = ('cpu-cg', 'gpu-cg')

    ALL_BASELINE_VARIANTS = [
        'chromium-mac-lion', 'chromium-mac-snowleopard', 'chromium-mac-leopard',
        'chromium-cg-mac-lion', 'chromium-cg-mac-snowleopard', 'chromium-cg-mac-leopard',
        'chromium-win-win7', 'chromium-win-vista', 'chromium-win-xp',
        'chromium-linux-x86_64', 'chromium-linux-x86',
        'chromium-gpu-mac-snowleopard', 'chromium-gpu-win-win7', 'chromium-gpu-linux-x86_64',
    ]

    CONFIGURATION_SPECIFIER_MACROS = {
        'mac': ['leopard', 'snowleopard', 'lion'],
        'win': ['xp', 'vista', 'win7'],
        'linux': ['lucid'],
    }

    def __init__(self, host, **kwargs):
        Port.__init__(self, host, **kwargs)
        # All sub-classes override this, but we need an initial value for testing.
        self._version = 'xp'
        self._chromium_base_dir = None

    def _check_file_exists(self, path_to_file, file_description,
                           override_step=None, logging=True):
        """Verify the file is present where expected or log an error.

        Args:
            file_name: The (human friendly) name or description of the file
                you're looking for (e.g., "HTTP Server"). Used for error logging.
            override_step: An optional string to be logged if the check fails.
            logging: Whether or not log the error messages."""
        if not self._filesystem.exists(path_to_file):
            if logging:
                _log.error('Unable to find %s' % file_description)
                _log.error('    at %s' % path_to_file)
                if override_step:
                    _log.error('    %s' % override_step)
                    _log.error('')
            return False
        return True


    def check_build(self, needs_http):
        result = True

        dump_render_tree_binary_path = self._path_to_driver()
        result = self._check_file_exists(dump_render_tree_binary_path,
                                         'test driver') and result
        if result and self.get_option('build'):
            result = self._check_driver_build_up_to_date(
                self.get_option('configuration'))
        else:
            _log.error('')

        helper_path = self._path_to_helper()
        if helper_path:
            result = self._check_file_exists(helper_path,
                                             'layout test helper') and result

        if self.get_option('pixel_tests'):
            result = self.check_image_diff(
                'To override, invoke with --no-pixel-tests') and result

        # It's okay if pretty patch isn't available, but we will at
        # least log a message.
        self._pretty_patch_available = self.check_pretty_patch()

        return result

    def check_sys_deps(self, needs_http):
        result = super(ChromiumPort, self).check_sys_deps(needs_http)

        cmd = [self._path_to_driver(), '--check-layout-test-sys-deps']

        local_error = executive.ScriptError()

        def error_handler(script_error):
            local_error.exit_code = script_error.exit_code

        output = self._executive.run_command(cmd, error_handler=error_handler)
        if local_error.exit_code:
            _log.error('System dependencies check failed.')
            _log.error('To override, invoke with --nocheck-sys-deps')
            _log.error('')
            _log.error(output)
            return False
        return result

    def check_image_diff(self, override_step=None, logging=True):
        image_diff_path = self._path_to_image_diff()
        return self._check_file_exists(image_diff_path, 'image diff exe',
                                       override_step, logging)

    def diff_image(self, expected_contents, actual_contents, tolerance=None):
        # FIXME: need unit tests for this.

        # tolerance is not used in chromium. Make sure caller doesn't pass tolerance other than zero or None.
        assert (tolerance is None) or tolerance == 0

        # If only one of them exists, return that one.
        if not actual_contents and not expected_contents:
            return (None, 0)
        if not actual_contents:
            return (expected_contents, 0)
        if not expected_contents:
            return (actual_contents, 0)

        tempdir = self._filesystem.mkdtemp()

        expected_filename = self._filesystem.join(str(tempdir), "expected.png")
        self._filesystem.write_binary_file(expected_filename, expected_contents)

        actual_filename = self._filesystem.join(str(tempdir), "actual.png")
        self._filesystem.write_binary_file(actual_filename, actual_contents)

        diff_filename = self._filesystem.join(str(tempdir), "diff.png")

        native_expected_filename = self._convert_path(expected_filename)
        native_actual_filename = self._convert_path(actual_filename)
        native_diff_filename = self._convert_path(diff_filename)

        executable = self._path_to_image_diff()
        comand = [executable, '--diff', native_actual_filename, native_expected_filename, native_diff_filename]

        result = None
        try:
            exit_code = self._executive.run_command(comand, return_exit_code=True)
            if exit_code == 0:
                # The images are the same.
                result = None
            elif exit_code != 1:
                _log.error("image diff returned an exit code of %s" % exit_code)
                # Returning None here causes the script to think that we
                # successfully created the diff even though we didn't.
                # FIXME: Consider raising an exception here, so that the error
                # is not accidentally overlooked while the test passes.
                result = None
        except OSError, e:
            if e.errno == errno.ENOENT or e.errno == errno.EACCES:
                _compare_available = False
            else:
                raise
        finally:
            if exit_code == 1:
                result = self._filesystem.read_binary_file(native_diff_filename)
            self._filesystem.rmtree(str(tempdir))
        return (result, 0)  # FIXME: how to get % diff?

    def path_from_chromium_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        Chromium source tree and the list of path components in |*comps|."""
        if not self._chromium_base_dir:
            chromium_module_path = self._filesystem.path_to_module(self.__module__)
            offset = chromium_module_path.find('third_party')
            if offset == -1:
                self._chromium_base_dir = self._filesystem.join(chromium_module_path[0:chromium_module_path.find('Tools')], 'Source', 'WebKit', 'chromium')
            else:
                self._chromium_base_dir = chromium_module_path[0:offset]
        return self._filesystem.join(self._chromium_base_dir, *comps)

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform', 'chromium', 'test_expectations.txt')

    def default_results_directory(self):
        try:
            return self.path_from_chromium_base('webkit', self.get_option('configuration'), 'layout-test-results')
        except AssertionError:
            return self._build_path(self.get_option('configuration'), 'layout-test-results')

    def setup_test_run(self):
        # Delete the disk cache if any to ensure a clean test run.
        dump_render_tree_binary_path = self._path_to_driver()
        cachedir = self._filesystem.dirname(dump_render_tree_binary_path)
        cachedir = self._filesystem.join(cachedir, "cache")
        if self._filesystem.exists(cachedir):
            self._filesystem.rmtree(cachedir)

    def _driver_class(self):
        return ChromiumDriver

    def start_helper(self):
        helper_path = self._path_to_helper()
        if helper_path:
            _log.debug("Starting layout helper %s" % helper_path)
            # Note: Not thread safe: http://bugs.python.org/issue2320
            self._helper = subprocess.Popen([helper_path],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=None)
            is_ready = self._helper.stdout.readline()
            if not is_ready.startswith('ready'):
                _log.error("layout_test_helper failed to be ready")

    def stop_helper(self):
        if self._helper:
            _log.debug("Stopping layout test helper")
            self._helper.stdin.write("x\n")
            self._helper.stdin.close()
            self._helper.wait()

    def exit_code_from_summarized_results(self, unexpected_results):
        # Turn bots red for missing results.
        return unexpected_results['num_regressions'] + unexpected_results['num_missing']

    def configuration_specifier_macros(self):
        return self.CONFIGURATION_SPECIFIER_MACROS

    def all_baseline_variants(self):
        return self.ALL_BASELINE_VARIANTS

    def test_expectations(self):
        """Returns the test expectations for this port.

        Basically this string should contain the equivalent of a
        test_expectations file. See test_expectations.py for more details."""
        expectations_path = self.path_to_test_expectations_file()
        return self._filesystem.read_text_file(expectations_path)

    def _generate_all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for version, architecture in self.ALL_SYSTEMS:
            for build_type in self.ALL_BUILD_TYPES:
                for graphics_type in self.ALL_GRAPHICS_TYPES:
                    test_configurations.append(TestConfiguration(version, architecture, build_type, graphics_type))
                if version in self.CORE_GRAPHICS_VERSIONS:
                    for graphics_type in self.CORE_GRAPHICS_TYPES:
                        test_configurations.append(TestConfiguration(version, architecture, build_type, graphics_type))
        return test_configurations

    try_builder_names = frozenset([
        'linux_layout',
        'mac_layout',
        'win_layout',
        'linux_layout_rel',
        'mac_layout_rel',
        'win_layout_rel',
    ])

    def test_expectations_overrides(self):
        # FIXME: It seems bad that run_webkit_tests.py uses a hardcoded dummy
        # builder string instead of just using None.
        builder_name = self.get_option('builder_name', 'DUMMY_BUILDER_NAME')
        if builder_name != 'DUMMY_BUILDER_NAME' and not '(deps)' in builder_name and not builder_name in self.try_builder_names:
            return None

        try:
            overrides_path = self.path_from_chromium_base('webkit', 'tools', 'layout_tests', 'test_expectations.txt')
        except AssertionError:
            return None
        if not self._filesystem.exists(overrides_path):
            return None
        return self._filesystem.read_text_file(overrides_path)

    def skipped_layout_tests(self, extra_test_files=None):
        expectations_str = self.test_expectations()
        overrides_str = self.test_expectations_overrides()
        is_debug_mode = False

        all_test_files = self.tests([])
        if extra_test_files:
            all_test_files.update(extra_test_files)

        expectations = test_expectations.TestExpectations(
            self, all_test_files, expectations_str, self.test_configuration(),
            is_lint_mode=False, overrides=overrides_str)
        return expectations.get_tests_with_result_type(test_expectations.SKIP)

    def test_repository_paths(self):
        # Note: for JSON file's backward-compatibility we use 'chrome' rather
        # than 'chromium' here.
        repos = super(ChromiumPort, self).test_repository_paths()
        repos.append(('chrome', self.path_from_chromium_base()))
        return repos

    #
    # PROTECTED METHODS
    #
    # These routines should only be called by other methods in this file
    # or any subclasses.
    #

    def _check_driver_build_up_to_date(self, configuration):
        if configuration in ('Debug', 'Release'):
            try:
                debug_path = self._path_to_driver('Debug')
                release_path = self._path_to_driver('Release')

                debug_mtime = self._filesystem.mtime(debug_path)
                release_mtime = self._filesystem.mtime(release_path)

                if (debug_mtime > release_mtime and configuration == 'Release' or
                    release_mtime > debug_mtime and configuration == 'Debug'):
                    _log.warning('You are not running the most '
                                 'recent DumpRenderTree binary. You need to '
                                 'pass --debug or not to select between '
                                 'Debug and Release.')
                    _log.warning('')
            # This will fail if we don't have both a debug and release binary.
            # That's fine because, in this case, we must already be running the
            # most up-to-date one.
            except OSError:
                pass
        return True

    def _chromium_baseline_path(self, platform):
        if platform is None:
            platform = self.name()
        return self.path_from_webkit_base('LayoutTests', 'platform', platform)

    def _convert_path(self, path):
        """Handles filename conversion for subprocess command line args."""
        # See note above in diff_image() for why we need this.
        if sys.platform == 'cygwin':
            return cygpath(path)
        return path

    def _path_to_image_diff(self):
        binary_name = 'ImageDiff'
        return self._build_path(self.get_option('configuration'), binary_name)


# FIXME: This should inherit from WebKitDriver now that Chromium has a DumpRenderTree process like the rest of WebKit.
class ChromiumDriver(Driver):
    def __init__(self, port, worker_number, pixel_tests):
        Driver.__init__(self, port, worker_number, pixel_tests)
        self._proc = None
        self._image_path = None
        if self._pixel_tests:
            self._image_path = self._port._filesystem.join(self._port.results_directory(), 'png_result%s.png' % self._worker_number)

    def _wrapper_options(self):
        cmd = []
        if self._pixel_tests:
            # See note above in diff_image() for why we need _convert_path().
            cmd.append("--pixel-tests=" + self._port._convert_path(self._image_path))
        # FIXME: This is not None shouldn't be necessary, unless --js-flags="''" changes behavior somehow?
        if self._port.get_option('js_flags') is not None:
            cmd.append('--js-flags="' + self._port.get_option('js_flags') + '"')

        # FIXME: We should be able to build this list using only an array of
        # option names, the options (optparse.Values) object, and the orignal
        # list of options from the main method by looking up the option
        # text from the options list if the value is non-None.
        # FIXME: How many of these options are still used?
        option_mappings = {
            'startup_dialog': '--testshell-startup-dialog',
            'gp_fault_error_box': '--gp-fault-error-box',
            'stress_opt': '--stress-opt',
            'stress_deopt': '--stress-deopt',
            'threaded_compositing': '--enable-threaded-compositing',
            'accelerated_2d_canvas': '--enable-accelerated-2d-canvas',
            'accelerated_drawing': '--enable-accelerated-drawing',
            'accelerated_video': '--enable-accelerated-video',
            'enable_hardware_gpu': '--enable-hardware-gpu',
        }
        for nrwt_option, drt_option in option_mappings.items():
            if self._port.get_option(nrwt_option):
                cmd.append(drt_option)

        cmd.extend(self._port.get_option('additional_drt_flag', []))
        return cmd

    def cmd_line(self):
        cmd = self._command_wrapper(self._port.get_option('wrapper'))
        cmd.append(self._port._path_to_driver())
        # FIXME: Why does --test-shell exist?  TestShell is dead, shouldn't this be removed?
        # It seems it's still in use in Tools/DumpRenderTree/chromium/DumpRenderTree.cpp as of 8/10/11.
        cmd.append('--test-shell')
        cmd.extend(self._wrapper_options())
        return cmd

    def _start(self):
        assert not self._proc
        # FIXME: This should use ServerProcess like WebKitDriver does.
        # FIXME: We should be reading stderr and stdout separately like how WebKitDriver does.
        close_fds = sys.platform != 'win32'
        self._proc = subprocess.Popen(self.cmd_line(), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=close_fds)

    def has_crashed(self):
        if self._proc is None:
            return False
        return self._proc.poll() is not None

    def _write_command_and_read_line(self, input=None):
        """Returns a tuple: (line, did_crash)"""
        try:
            if input:
                if isinstance(input, unicode):
                    # DRT expects utf-8
                    input = input.encode("utf-8")
                self._proc.stdin.write(input)
            # DumpRenderTree text output is always UTF-8.  However some tests
            # (e.g. webarchive) may spit out binary data instead of text so we
            # don't bother to decode the output.
            line = self._proc.stdout.readline()
            # We could assert() here that line correctly decodes as UTF-8.
            return (line, False)
        except IOError, e:
            _log.error("IOError communicating w/ DRT: " + str(e))
            return (None, True)

    def _test_shell_command(self, uri, timeoutms, checksum):
        cmd = uri
        if timeoutms:
            cmd += ' ' + str(timeoutms)
        if checksum:
            cmd += ' ' + checksum
        cmd += "\n"
        return cmd

    def _output_image(self):
        if self._image_path and self._port._filesystem.exists(self._image_path):
            return self._port._filesystem.read_binary_file(self._image_path)
        return None

    def _output_image_with_retry(self):
        # Retry a few more times because open() sometimes fails on Windows,
        # raising "IOError: [Errno 13] Permission denied:"
        retry_num = 50
        timeout_seconds = 5.0
        for _ in range(retry_num):
            try:
                return self._output_image()
            except IOError, e:
                if e.errno != errno.EACCES:
                    raise e
            # FIXME: We should have a separate retry delay.
            # This implementation is likely to exceed the timeout before the expected number of retries.
            time.sleep(timeout_seconds / retry_num)
        return self._output_image()

    def _clear_output_image(self):
        if self._image_path and self._port._filesystem.exists(self._image_path):
            self._port._filesystem.remove(self._image_path)

    def run_test(self, driver_input):
        if not self._proc:
            self._start()

        output = []
        error = []
        crash = False
        timeout = False
        actual_uri = None
        actual_checksum = None
        self._clear_output_image()
        start_time = time.time()
        has_audio = False
        has_base64 = False

        uri = self._port.test_to_uri(driver_input.test_name)
        cmd = self._test_shell_command(uri, driver_input.timeout, driver_input.image_hash)
        line, crash = self._write_command_and_read_line(input=cmd)

        while not crash and line.rstrip() != "#EOF":
            # Make sure we haven't crashed.
            if line == '' and self._proc.poll() is not None:
                # This is hex code 0xc000001d, which is used for abrupt
                # termination. This happens if we hit ctrl+c from the prompt
                # and we happen to be waiting on DRT.
                # sdoyon: Not sure for which OS and in what circumstances the
                # above code is valid. What works for me under Linux to detect
                # ctrl+c is for the subprocess returncode to be negative
                # SIGINT. And that agrees with the subprocess documentation.
                if (-1073741510 == self._proc.returncode or
                    - signal.SIGINT == self._proc.returncode):
                    raise KeyboardInterrupt
                crash = True
                break

            # Don't include #URL lines in our output
            if line.startswith("#URL:"):
                actual_uri = line.rstrip()[5:]
                if uri != actual_uri:
                    # GURL capitalizes the drive letter of a file URL.
                    if (not re.search("^file:///[a-z]:", uri) or uri.lower() != actual_uri.lower()):
                        _log.fatal("Test got out of sync:\n|%s|\n|%s|" % (uri, actual_uri))
                        raise AssertionError("test out of sync")
            elif line.startswith("#MD5:"):
                actual_checksum = line.rstrip()[5:]
            elif line.startswith("#TEST_TIMED_OUT"):
                timeout = True
                # Test timed out, but we still need to read until #EOF.
            elif line.startswith("Content-Type: audio/wav"):
                has_audio = True
            elif line.startswith("Content-Transfer-Encoding: base64"):
                has_base64 = True
            elif line.startswith("Content-Length:"):
                pass
            elif actual_uri:
                output.append(line)
            else:
                error.append(line)

            line, crash = self._write_command_and_read_line(input=None)

        run_time = time.time() - start_time
        output_image = self._output_image_with_retry()

        audio_bytes = None
        text = None
        if has_audio:
            if has_base64:
                audio_bytes = base64.b64decode(''.join(output))
            else:
                audio_bytes = ''.join(output).rstrip()
        else:
            text = ''.join(output)
            if not text:
                text = None

        error = ''.join(error)
        crashed_process_name = None
        # Currently the stacktrace is in the text output, not error, so append the two together so
        # that we can see stack in the output. See http://webkit.org/b/66806
        # FIXME: We really should properly handle the stderr output separately.
        if crash:
            error = error + str(text)
            crashed_process_name = self._port.driver_name()

        return DriverOutput(text, output_image, actual_checksum, audio=audio_bytes,
            crash=crash, crashed_process_name=crashed_process_name, test_time=run_time, timeout=timeout, error=error)

    def stop(self):
        if not self._proc:
            return
        # FIXME: If we used ServerProcess all this would happen for free with ServerProces.stop()
        self._proc.stdin.close()
        self._proc.stdout.close()
        if self._proc.stderr:
            self._proc.stderr.close()
        time_out_ms = self._port.get_option('time_out_ms')
        if time_out_ms:
            kill_timeout_seconds = 3.0 * int(time_out_ms) / Manager.DEFAULT_TEST_TIMEOUT_MS
        else:
            kill_timeout_seconds = 3.0

        # Closing stdin/stdout/stderr hangs sometimes on OS X,
        # (see __init__(), above), and anyway we don't want to hang
        # the harness if DRT is buggy, so we wait a couple
        # seconds to give DRT a chance to clean up, but then
        # force-kill the process if necessary.
        timeout = time.time() + kill_timeout_seconds
        while self._proc.poll() is None and time.time() < timeout:
            time.sleep(0.1)
        if self._proc.poll() is None:
            _log.warning('stopping test driver timed out, killing it')
            self._port._executive.kill_process(self._proc.pid)
        # FIXME: This is sometime None. What is wrong? assert self._proc.poll() is not None
        if self._proc.poll() is not None:
            self._proc.wait()
        self._proc = None
