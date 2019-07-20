# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (c) 2015-2019 Apple Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

import base64
import logging
import re
import shlex
import sys
import time
import os
from collections import defaultdict

from os.path import normpath
from webkitpy.common.system import path
from webkitpy.common.system.profiler import ProfilerFactory


_log = logging.getLogger(__name__)


class DriverInput(object):
    def __init__(self, test_name, timeout, image_hash, should_run_pixel_test, should_dump_jsconsolelog_in_stderr=None, args=None):
        self.test_name = test_name
        self.timeout = timeout  # in ms
        self.image_hash = image_hash
        self.should_run_pixel_test = should_run_pixel_test
        self.should_dump_jsconsolelog_in_stderr = should_dump_jsconsolelog_in_stderr
        self.args = args or []

    def __repr__(self):
        return "DriverInput(test_name='{}', timeout={}, image_hash={}, should_run_pixel_test={}, should_dump_jsconsolelog_in_stderr={}'".format(self.test_name, self.timeout, self.image_hash, self.should_run_pixel_test, self.should_dump_jsconsolelog_in_stderr)


class DriverOutput(object):
    """Groups information about a output from driver for easy passing
    and post-processing of data."""

    metrics_patterns = []
    metrics_patterns.append((re.compile('at \(-?[0-9]+,-?[0-9]+\) *'), ''))
    metrics_patterns.append((re.compile('size -?[0-9]+x-?[0-9]+ *'), ''))
    metrics_patterns.append((re.compile('text run width -?[0-9]+: '), ''))
    metrics_patterns.append((re.compile('text run width -?[0-9]+ [a-zA-Z ]+: '), ''))
    metrics_patterns.append((re.compile('RenderButton {BUTTON} .*'), 'RenderButton {BUTTON}'))
    metrics_patterns.append((re.compile('RenderImage {INPUT} .*'), 'RenderImage {INPUT}'))
    metrics_patterns.append((re.compile('RenderBlock {INPUT} .*'), 'RenderBlock {INPUT}'))
    metrics_patterns.append((re.compile('RenderTextControl {INPUT} .*'), 'RenderTextControl {INPUT}'))
    metrics_patterns.append((re.compile('\([0-9]+px'), 'px'))
    metrics_patterns.append((re.compile(' *" *\n +" *'), ' '))
    metrics_patterns.append((re.compile('" +$'), '"'))
    metrics_patterns.append((re.compile('- '), '-'))
    metrics_patterns.append((re.compile('\n( *)"\s+'), '\n\g<1>"'))
    metrics_patterns.append((re.compile('\s+"\n'), '"\n'))
    metrics_patterns.append((re.compile('scrollWidth [0-9]+'), 'scrollWidth'))
    metrics_patterns.append((re.compile('scrollHeight [0-9]+'), 'scrollHeight'))
    metrics_patterns.append((re.compile('scrollX [0-9]+'), 'scrollX'))
    metrics_patterns.append((re.compile('scrollY [0-9]+'), 'scrollY'))
    metrics_patterns.append((re.compile('scrolled to [0-9]+,[0-9]+'), 'scrolled'))

    def __init__(self, text, image, image_hash, audio, crash=False,
            test_time=0, measurements=None, timeout=False, error='', crashed_process_name='??',
            crashed_pid=None, crash_log=None, pid=None):
        # FIXME: Args could be renamed to better clarify what they do.
        self.text = text
        self.image = image  # May be empty-string if the test crashes.
        self.image_hash = image_hash
        self.image_diff = None  # image_diff gets filled in after construction.
        self.audio = audio  # Binary format is port-dependent.
        self.crash = crash
        self.crashed_process_name = crashed_process_name
        self.crashed_pid = crashed_pid
        self.crash_log = crash_log
        self.test_time = test_time
        self.measurements = measurements
        self.timeout = timeout
        self.error = error  # stderr output
        self.pid = pid

    def has_stderr(self):
        return bool(self.error)

    def strip_metrics(self):
        self.strip_patterns(self.metrics_patterns)

    def strip_patterns(self, patterns):
        if not self.text:
            return
        for pattern in patterns:
            self.text = re.sub(pattern[0], pattern[1], self.text)

    def strip_stderror_patterns(self, patterns):
        if not self.error:
            return
        for pattern in patterns:
            self.error = re.sub(pattern[0], pattern[1], self.error)


class DriverPostTestOutput(object):
    """Groups data collected for a set of tests, collected after all those testse have run
    (for example, data about leaked objects)"""
    def __init__(self, world_leaks_dict):
        self.world_leaks_dict = world_leaks_dict


class Driver(object):
    """object for running test(s) using DumpRenderTree/WebKitTestRunner."""

    def __init__(self, port, worker_number, pixel_tests, no_timeout=False):
        """Initialize a Driver to subsequently run tests.

        Typically this routine will spawn DumpRenderTree in a config
        ready for subsequent input.

        port - reference back to the port object.
        worker_number - identifier for a particular worker/driver instance
        """
        self._port = port
        self._worker_number = worker_number
        self._no_timeout = no_timeout
        self._target_host = port.target_host(worker_number)

        self._driver_tempdir = None
        self._driver_user_directory_suffix = None
        self._driver_user_cache_directory = None

        # WebKitTestRunner can report back subprocess crashes by printing
        # "#CRASHED - PROCESSNAME".  Since those can happen at any time and ServerProcess
        # won't be aware of them (since the actual tool didn't crash, just a subprocess)
        # we record the crashed subprocess name here.
        self._crashed_process_name = None
        self._crashed_pid = None

        self._driver_timed_out = False

        # stderr reading is scoped on a per-test (not per-block) basis, so we store the accumulated
        # stderr output, as well as if we've seen #EOF on this driver instance.
        # FIXME: We should probably remove _read_first_block and _read_optional_image_block and
        # instead scope these locally in run_test.
        self.error_from_test = str()
        self.err_seen_eof = False

        self._server_name = self._port.driver_name()
        self._server_process = None

        self._measurements = {}
        if self._port.get_option("profile"):
            profiler_name = self._port.get_option("profiler")
            self._profiler = ProfilerFactory.create_profiler(self._port.host,
                self._port._path_to_driver(), self._port.results_directory(), profiler_name)
        else:
            self._profiler = None

        self.web_platform_test_server_doc_root = self._port.web_platform_test_server_doc_root()
        self.web_platform_test_server_base_http_url = self._port.web_platform_test_server_base_http_url()
        self.web_platform_test_server_base_https_url = self._port.web_platform_test_server_base_https_url()

    def __del__(self):
        self.stop()

    def run_test(self, driver_input, stop_when_done):
        """Run a single test and return the results.

        Note that it is okay if a test times out or crashes and leaves
        the driver in an indeterminate state. The upper layers of the program
        are responsible for cleaning up and ensuring things are okay.

        Returns a DriverOutput object.
        """
        start_time = time.time()
        self.start(driver_input.should_run_pixel_test, driver_input.args)
        test_begin_time = time.time()
        self._driver_timed_out = False
        self._crash_report_from_driver = None
        self.error_from_test = str()
        self.err_seen_eof = False

        command = self._command_from_driver_input(driver_input)

        # Certain timeouts are detected by the tool itself; tool detection is better,
        # because results contain partial output in this case. Make script timeout longer
        # by 5 seconds to avoid racing for which timeout is detected first.
        # FIXME: It's not the job of the driver to decide what the timeouts should be.
        # Move the additional timeout to driver_input.
        if self._no_timeout:
            deadline = test_begin_time + 60 * 60 * 24 * 7  # 7 days. Using sys.maxint causes a hang.
        else:
            deadline = test_begin_time + int(driver_input.timeout) / 1000.0 + 5

        self._server_process.write(command)
        text, audio = self._read_first_block(deadline, driver_input.test_name)  # First block is either text or audio
        image, actual_image_hash = self._read_optional_image_block(deadline, driver_input.test_name)  # The second (optional) block is image data.

        crashed = self.has_crashed()
        timed_out = self._server_process.timed_out
        driver_timed_out = self._driver_timed_out
        pid = self._server_process.pid()

        if stop_when_done or crashed or timed_out:
            # We call stop() even if we crashed or timed out in order to get any remaining stdout/stderr output.
            # In the timeout case, we kill the hung process as well.
            out, err = self._server_process.stop(self._port.driver_stop_timeout() if stop_when_done else 0.0)
            if out:
                text += out
            if err:
                self.error_from_test += err
            self._server_process = None

        crash_log = None
        if self._crash_report_from_driver:
            crash_log = self._crash_report_from_driver
        elif crashed:
            self.error_from_test, crash_log = self._get_crash_log(text, self.error_from_test, newer_than=start_time)
            # If we don't find a crash log use a placeholder error message instead.
            if not crash_log:
                pid_str = str(self._crashed_pid) if self._crashed_pid else "unknown pid"
                crash_log = 'No crash log found for %s:%s.\n' % (self._crashed_process_name, pid_str)

                # Print stdout and stderr to the placeholder crash log; we want as much context as possible.
                if self.error_from_test:
                    crash_log += '\nstdout:\n%s\nstderr:\n%s\n' % (text, self.error_from_test)

        return DriverOutput(text, image, actual_image_hash, audio,
            crash=crashed, test_time=time.time() - test_begin_time, measurements=self._measurements,
            timeout=timed_out or driver_timed_out, error=self.error_from_test,
            crashed_process_name=self._crashed_process_name,
            crashed_pid=self._crashed_pid, crash_log=crash_log, pid=pid)

    def do_post_tests_work(self):
        if not self._server_process:
            return None

        if self._port.get_option('leaks'):
            _log.debug('Gathering child processes...')
            self._server_process.write('#LIST CHILD PROCESSES\n')
            deadline = time.time() + 20
            block = self._read_block(deadline, '', wait_for_stderr_eof=True)
            self._server_process.set_child_processes(self._parse_child_processes_output(block.decoded_content))

        if self._port.get_option('world_leaks'):
            _log.debug('Checking for world leaks...')
            self._server_process.write('#CHECK FOR WORLD LEAKS\n')
            deadline = time.time() + 20
            block = self._read_block(deadline, '', wait_for_stderr_eof=True)

            _log.debug('World leak result: %s' % (block.decoded_content))

            return self._parse_world_leaks_output(block.decoded_content)

        return None

    @staticmethod
    def _parse_child_processes_output(output):
        child_processes = defaultdict(list)

        for line in output.splitlines():
            m = re.match('^([^:]+): ([0-9]+)$', line)
            if m:
                process_name = m.group(1)
                process_id = m.group(2)
                child_processes[process_name].append(process_id)

        return child_processes

    def _parse_world_leaks_output(self, output):
        tests_with_world_leaks = defaultdict(list)

        last_test = None
        for line in output.splitlines():
            m = re.match('^TEST: (.+)$', line)
            if m:
                last_test = self.uri_to_test(m.group(1))
            m = re.match('^ABANDONED DOCUMENT: (.+)$', line)
            if m:
                leaked_document_url = m.group(1)
                if last_test:
                    tests_with_world_leaks[last_test].append(leaked_document_url)

        return DriverPostTestOutput(tests_with_world_leaks)

    def _get_crash_log(self, stdout, stderr, newer_than):
        return self._port._get_crash_log(self._crashed_process_name, self._crashed_pid, stdout, stderr, newer_than, target_host=self._target_host)

    def _command_wrapper(self):
        # Hook for injecting valgrind or other runtime instrumentation, used by e.g. tools/valgrind/valgrind_tests.py.
        wrapper_arguments = []
        if self._profiler:
            wrapper_arguments = self._profiler.wrapper_arguments()
        if self._port.get_option('wrapper'):
            return shlex.split(self._port.get_option('wrapper')) + wrapper_arguments
        return wrapper_arguments

    HTTP_DIR = "http/tests/"
    HTTP_LOCAL_DIR = "http/tests/local/"
    WEBKIT_SPECIFIC_WEB_PLATFORM_TEST_SUBDIR = "http/wpt/"
    WEBKIT_WEB_PLATFORM_TEST_SERVER_ROUTE = "WebKit/"

    def is_http_test(self, test_name):
        return test_name.startswith(self.HTTP_DIR) and not test_name.startswith(self.HTTP_LOCAL_DIR)

    def is_webkit_specific_web_platform_test(self, test_name):
        return test_name.startswith(self.WEBKIT_SPECIFIC_WEB_PLATFORM_TEST_SUBDIR)

    def is_web_platform_test(self, test_name):
        return test_name.startswith(self.web_platform_test_server_doc_root)

    def wpt_test_path_to_uri(self, path):
        return self.web_platform_test_server_base_https_url + path if ".https." in path else self.web_platform_test_server_base_http_url + path

    def http_test_path_to_uri(self, path):
        path = path.replace(os.sep, '/')
        return self.http_base_url(secure=self.is_secure_path(path)) + path

    def is_secure_path(self, path):
        return path.startswith("ssl") or ".https." in path

    def http_base_url(self, secure=None):
        return "%s://127.0.0.1:%d/" % (('https', 8443) if secure else ('http', 8000))

    def test_to_uri(self, test_name):
        """Convert a test name to a URI."""
        if self.is_web_platform_test(test_name):
            return self.wpt_test_path_to_uri(test_name[len(self.web_platform_test_server_doc_root):])
        if self.is_webkit_specific_web_platform_test(test_name):
            return self.wpt_test_path_to_uri(self.WEBKIT_WEB_PLATFORM_TEST_SERVER_ROUTE + test_name[len(self.WEBKIT_SPECIFIC_WEB_PLATFORM_TEST_SUBDIR):])

        if not self.is_http_test(test_name):
            return path.abspath_to_uri(self._port.host.platform, self._port.abspath_for_test(test_name))

        return self.http_test_path_to_uri(test_name[len(self.HTTP_DIR):])

    def uri_to_test(self, uri):
        """Return the base layout test name for a given URI.

        This returns the test name for a given URI, e.g., if you passed in
        "file:///src/LayoutTests/fast/html/keygen.html" it would return
        "fast/html/keygen.html".

        """
        if uri.startswith("file:///"):
            prefix = path.abspath_to_uri(self._port.host.platform, self._port.layout_tests_dir())
            if not prefix.endswith('/'):
                prefix += '/'
            return uri[len(prefix):]
        if uri.startswith(self.web_platform_test_server_base_http_url + self.WEBKIT_WEB_PLATFORM_TEST_SERVER_ROUTE):
            return uri.replace(self.web_platform_test_server_base_http_url + self.WEBKIT_WEB_PLATFORM_TEST_SERVER_ROUTE, self.WEBKIT_SPECIFIC_WEB_PLATFORM_TEST_SUBDIR)
        if uri.startswith(self.web_platform_test_server_base_https_url + self.WEBKIT_WEB_PLATFORM_TEST_SERVER_ROUTE):
            return uri.replace(self.web_platform_test_server_base_https_url + self.WEBKIT_WEB_PLATFORM_TEST_SERVER_ROUTE, self.WEBKIT_SPECIFIC_WEB_PLATFORM_TEST_SUBDIR)
        if uri.startswith(self.web_platform_test_server_base_http_url):
            return uri.replace(self.web_platform_test_server_base_http_url, self.web_platform_test_server_doc_root)
        if uri.startswith(self.web_platform_test_server_base_https_url):
            return uri.replace(self.web_platform_test_server_base_https_url, self.web_platform_test_server_doc_root)
        if uri.startswith("http://"):
            return uri.replace(self.http_base_url(secure=False), self.HTTP_DIR)
        if uri.startswith("https://"):
            return uri.replace(self.http_base_url(secure=True), self.HTTP_DIR)
        raise NotImplementedError('unknown url type: %s' % uri)

    def has_crashed(self):
        if self._server_process is None:
            return False
        if self._crashed_process_name:
            return True
        if self._server_process.has_crashed():
            self._crashed_process_name = self._server_process.process_name()
            self._crashed_pid = self._server_process.system_pid()
            return True
        return False

    def start(self, pixel_tests, per_test_args):
        # FIXME: Callers shouldn't normally call this, since this routine
        # may not be specifying the correct combination of pixel test and
        # per_test args.
        #
        # The only reason we have this routine at all is so the perftestrunner
        # can pause before running a test; it might be better to push that
        # into run_test() directly.
        if not self._server_process:
            self._start(pixel_tests, per_test_args)
            self._run_post_start_tasks()

    def _append_environment_variable_path(self, environment, variable, path):
        if variable in environment:
            environment[variable] = environment[variable] + os.pathsep + path
        else:
            environment[variable] = path

    def _setup_environ_for_driver(self, environment):
        self._port._clear_global_caches_and_temporary_files()
        self._create_temporal_directories()
        build_root_path = str(self._port._build_path())
        self._append_environment_variable_path(environment, 'DYLD_LIBRARY_PATH', build_root_path)
        self._append_environment_variable_path(environment, '__XPC_DYLD_LIBRARY_PATH', build_root_path)
        self._append_environment_variable_path(environment, 'DYLD_FRAMEWORK_PATH', build_root_path)
        self._append_environment_variable_path(environment, '__XPC_DYLD_FRAMEWORK_PATH', build_root_path)
        # Use an isolated temp directory that can be deleted after testing (especially important on Mac, as
        # CoreMedia disk cache is in the temp directory).
        environment['TMPDIR'] = str(self._driver_tempdir)
        environment['DIRHELPER_USER_DIR_SUFFIX'] = self._driver_user_directory_suffix
        # Put certain normally persistent files into the temp directory (e.g. IndexedDB storage).
        if sys.platform == 'cygwin':
            environment['DUMPRENDERTREE_TEMP'] = path.cygpath(str(self._driver_tempdir))
        else:
            environment['DUMPRENDERTREE_TEMP'] = str(self._driver_tempdir)
        environment['LOCAL_RESOURCE_ROOT'] = str(self._port.layout_tests_dir())
        environment['ASAN_OPTIONS'] = "allocator_may_return_null=1"
        environment['__XPC_ASAN_OPTIONS'] = environment['ASAN_OPTIONS']

        # Disable vnode-guard related simulated crashes for WKTR / DRT (rdar://problem/40674034).
        environment['SQLITE_EXEMPT_PATH_FROM_VNODE_GUARDS'] = os.path.realpath(environment['DUMPRENDERTREE_TEMP'])
        environment['__XPC_SQLITE_EXEMPT_PATH_FROM_VNODE_GUARDS'] = environment['SQLITE_EXEMPT_PATH_FROM_VNODE_GUARDS']

        if sys.platform.startswith('linux'):
            # Currently on WebKit2, there is no API for setting the application cache directory.
            # Each worker should have it's own and it should be cleaned afterwards.
            # Set it to inside the temporary folder by prepending XDG_CACHE_HOME with DRIVER_TEMPDIR.
            environment['XDG_CACHE_HOME'] = self._port.host.filesystem.join(str(self._driver_tempdir), 'appcache')
            # Use an empty/volatile home inside DRIVER_TEMPDIR to ensure that the test results
            # are not affected by the user settings of any library.
            environment['HOME'] = self._port.host.filesystem.join(str(self._driver_tempdir), 'home')
            self._target_host.filesystem.maybe_make_directory(environment['HOME'])

        if self._profiler:
            environment = self._profiler.adjusted_environment(environment)
        return environment

    def _setup_environ_for_test(self):
        environment = self._port.setup_environ_for_server(self._server_name)
        environment = self._setup_environ_for_driver(environment)
        return environment

    def _create_temporal_directories(self):
        # Each driver process should be using individual directories under _driver_tempdir (which is deleted when stopping),
        # however some subsystems on some platforms could end up using process default ones.
        if self._driver_tempdir is None:
            self._driver_tempdir = self._port._driver_tempdir(self._target_host)
            self._driver_user_directory_suffix = os.path.basename(str(self._driver_tempdir))
            user_cache_directory = self._port._path_to_user_cache_directory(self._driver_user_directory_suffix)
            if user_cache_directory:
                self._target_host.filesystem.maybe_make_directory(user_cache_directory)
                self._driver_user_cache_directory = user_cache_directory

    def _start(self, pixel_tests, per_test_args):
        self.stop()
        environment = self._setup_environ_for_test()
        self._crashed_process_name = None
        self._crashed_pid = None
        self._server_process = self._port._test_runner_process_constructor(self._port, self._server_name, self.cmd_line(pixel_tests, per_test_args), environment, target_host=self._target_host)
        self._server_process.start()

    def _run_post_start_tasks(self):
        # Remote drivers may override this to delay post-start tasks until the server has ack'd.
        if self._profiler:
            self._profiler.attach_to_pid(self._pid_on_target())

    def _pid_on_target(self):
        # Remote drivers will override this method to return the pid on the device.
        return self._server_process.pid()

    def _delete_temporal_directories(self):
        if self._driver_tempdir:
            self._target_host.filesystem.rmtree(str(self._driver_tempdir))
            self._driver_tempdir = None
        if self._driver_user_cache_directory:
            self._target_host.filesystem.rmtree(self._driver_user_cache_directory)
            self._driver_user_cache_directory = None

    def stop(self):
        if self._server_process:
            self._server_process.stop(self._port.driver_stop_timeout())
            self._server_process = None
            if self._profiler:
                self._profiler.profile_after_exit()
        self._delete_temporal_directories()

    def cmd_line(self, pixel_tests, per_test_args):
        cmd = self._command_wrapper()
        cmd.append(self._port._path_to_driver())
        if self._port.get_option('gc_between_tests'):
            cmd.append('--gc-between-tests')
        if self._port.get_option('complex_text'):
            cmd.append('--complex-text')
        if self._port.get_option('accelerated_drawing'):
            cmd.append('--accelerated-drawing')
        if self._port.get_option('remote_layer_tree'):
            cmd.append('--remote-layer-tree')
        if self._port.get_option('world_leaks'):
            cmd.append('--world-leaks')
        if self._port.get_option('threaded'):
            cmd.append('--threaded')
        if self._no_timeout:
            cmd.append('--no-timeout')
        if self._port.get_option('show_touches'):
            cmd.append('--show-touches')

        for allowed_host in self._port.allowed_hosts():
            cmd.append('--allowed-host')
            cmd.append(allowed_host)

        cmd.extend(self._port.get_option('additional_drt_flag', []))
        cmd.extend(self._port.additional_drt_flag())

        cmd.extend(per_test_args)

        cmd.append('-')
        return cmd

    def _check_for_driver_timeout(self, out_line):
        if out_line.startswith("#PID UNRESPONSIVE - "):
            match = re.match('#PID UNRESPONSIVE - (\S+)', out_line)
            child_process_name = match.group(1) if match else 'WebProcess'
            match = re.search('pid (\d+)', out_line)
            child_process_pid = int(match.group(1)) if match else None
            err_line = 'Wait on notifyDone timed out, process ' + child_process_name + ' pid = ' + str(child_process_pid)
            self.error_from_test += err_line
            _log.debug(err_line)
            if self._port.get_option("sample_on_timeout"):
                self._port.sample_process(child_process_name, child_process_pid, self._target_host)
        if out_line == "FAIL: Timed out waiting for notifyDone to be called\n":
            self._driver_timed_out = True

    def _check_for_address_sanitizer_violation(self, error_line):
        if "ERROR: AddressSanitizer" in error_line:
            return True

    def _check_for_driver_crash_or_unresponsiveness(self, error_line):
        crashed_check = error_line.rstrip('\r\n')
        if crashed_check == "#CRASHED":
            self._crashed_process_name = self._server_process.process_name()
            self._crashed_pid = self._server_process.system_pid()
            return True
        elif error_line.startswith("#CRASHED - "):
            match = re.match('#CRASHED - (\S+)', error_line)
            self._crashed_process_name = match.group(1) if match else 'WebProcess'
            match = re.search('pid (\d+)', error_line)
            self._crashed_pid = int(match.group(1)) if match else None
            _log.debug('%s crash, pid = %s' % (self._crashed_process_name, str(self._crashed_pid)))
            return True
        elif error_line.startswith("#PROCESS UNRESPONSIVE - "):
            match = re.match('#PROCESS UNRESPONSIVE - (\S+)', error_line)
            child_process_name = match.group(1) if match else 'WebProcess'
            match = re.search('pid (\d+)', error_line)
            child_process_pid = int(match.group(1)) if match else None
            _log.debug('%s is unresponsive, pid = %s' % (child_process_name, str(child_process_pid)))
            self._driver_timed_out = True
            if child_process_pid:
                self._port.sample_process(child_process_name, child_process_pid, self._target_host)
            self.error_from_test += error_line
            self._server_process.write('#SAMPLE FINISHED\n', True)  # Must be able to ignore a broken pipe here, target process may already be closed.
            return True
        return self.has_crashed()

    def _command_from_driver_input(self, driver_input):
        # FIXME: performance tests pass in full URLs instead of test names.
        if driver_input.test_name.startswith('http://') or driver_input.test_name.startswith('https://')  or driver_input.test_name == ('about:blank'):
            command = driver_input.test_name
        elif self.is_web_platform_test(driver_input.test_name) or self.is_webkit_specific_web_platform_test(driver_input.test_name) or self.is_http_test(driver_input.test_name):
            command = self.test_to_uri(driver_input.test_name)
            command += "'--absolutePath'"
            absPath = self._port.abspath_for_test(driver_input.test_name, self._target_host)
            if sys.platform == 'cygwin':
                absPath = path.cygpath(absPath)
            command += absPath
        else:
            command = self._port.abspath_for_test(driver_input.test_name, self._target_host)
            if sys.platform == 'cygwin':
                command = path.cygpath(command)

        assert not driver_input.image_hash or driver_input.should_run_pixel_test

        # ' is the separator between arguments.
        if self._port.supports_per_test_timeout():
            command += "'--timeout'%s" % driver_input.timeout
        if driver_input.should_run_pixel_test:
            command += "'--pixel-test"
        if driver_input.should_dump_jsconsolelog_in_stderr:
            command += "'--dump-jsconsolelog-in-stderr"
        if driver_input.image_hash:
            command += "'" + driver_input.image_hash
        return command + "\n"

    def _read_first_block(self, deadline, test_name):
        # returns (text_content, audio_content)
        block = self._read_block(deadline, test_name)
        if block.malloc:
            self._measurements['Malloc'] = float(block.malloc)
        if block.js_heap:
            self._measurements['JSHeap'] = float(block.js_heap)
        if block.content_type == 'audio/wav':
            return (None, block.decoded_content)
        return (block.decoded_content, None)

    def _read_optional_image_block(self, deadline, test_name):
        # returns (image, actual_image_hash)
        block = self._read_block(deadline, test_name, wait_for_stderr_eof=True)
        if block.content and block.content_type == 'image/png':
            return (block.decoded_content, block.content_hash)
        return (None, block.content_hash)

    def _read_header(self, block, line, header_text, header_attr, header_filter=None):
        if line.startswith(header_text) and getattr(block, header_attr) is None:
            value = line.split()[1]
            if header_filter:
                value = header_filter(value)
            setattr(block, header_attr, value)
            return True
        return False

    def _process_stdout_line(self, block, line):
        if (self._read_header(block, line, 'Content-Type: ', 'content_type')
            or self._read_header(block, line, 'Content-Transfer-Encoding: ', 'encoding')
            or self._read_header(block, line, 'Content-Length: ', '_content_length', int)
            or self._read_header(block, line, 'ActualHash: ', 'content_hash')
            or self._read_header(block, line, 'DumpMalloc: ', 'malloc')
            or self._read_header(block, line, 'DumpJSHeap: ', 'js_heap')):
            return
        # Note, we're not reading ExpectedHash: here, but we could.
        # If the line wasn't a header, we just append it to the content.
        block.content += line

    def _strip_eof(self, line):
        if line and line.endswith("#EOF\n"):
            return line[:-5], True
        return line, False

    def _read_block(self, deadline, test_name, wait_for_stderr_eof=False):
        block = ContentBlock()
        out_seen_eof = False
        asan_violation_detected = False

        while True:
            if out_seen_eof and (self.err_seen_eof or not wait_for_stderr_eof):
                break

            if self.err_seen_eof:
                out_line = self._server_process.read_stdout_line(deadline)
                err_line = None
            elif out_seen_eof:
                out_line = None
                err_line = self._server_process.read_stderr_line(deadline)
            else:
                out_line, err_line = self._server_process.read_either_stdout_or_stderr_line(deadline)

            # ServerProcess returns None for time outs and crashes.
            if out_line is None and err_line is None:
                break

            if out_line:
                assert not out_seen_eof
                out_line, out_seen_eof = self._strip_eof(out_line)
            if err_line:
                assert not self.err_seen_eof
                err_line, self.err_seen_eof = self._strip_eof(err_line)

            if out_line:
                self._check_for_driver_timeout(out_line)
                if out_line[-1] != "\n":
                    _log.error("  %s -> Last character read from DRT stdout line was not a newline!  This indicates either a NRWT or DRT bug." % test_name)
                content_length_before_header_check = block._content_length
                self._process_stdout_line(block, out_line)
                # FIXME: Unlike HTTP, DRT dumps the content right after printing a Content-Length header.
                # Don't wait until we're done with headers, just read the binary blob right now.
                if content_length_before_header_check != block._content_length:
                    block.content = self._server_process.read_stdout(deadline, block._content_length)

            if err_line:
                if self._check_for_driver_crash_or_unresponsiveness(err_line):
                    break
                elif self._check_for_address_sanitizer_violation(err_line):
                    asan_violation_detected = True
                    self._crash_report_from_driver = ""
                    # ASan report starts with a nondescript line, we only detect the second line.
                    end_of_previous_error_line = self.error_from_test.rfind('\n', 0, -1)
                    if end_of_previous_error_line > 0:
                        self.error_from_test = self.error_from_test[:end_of_previous_error_line]
                    else:
                        self.error_from_test = ""
                    # Symbolication can take a very long time, give it 10 extra minutes to finish.
                    # FIXME: This can likely be removed once <rdar://problem/18701447> is fixed.
                    deadline += 10 * 60 * 1000
                if asan_violation_detected:
                    self._crash_report_from_driver += err_line
                else:
                    self.error_from_test += err_line

        if asan_violation_detected and not self._crashed_process_name:
            self._crashed_process_name = self._server_process.process_name()
            self._crashed_pid = self._server_process.system_pid()

        block.decode_content()
        return block

    @staticmethod
    def check_driver(port):
        # This checks if the required system dependencies for the driver are met.
        # Since this is the generic class implementation, just return True.
        return True


class ContentBlock(object):
    def __init__(self):
        self.content_type = None
        self.encoding = None
        self.content_hash = None
        self._content_length = None
        # Content is treated as binary data even though the text output is usually UTF-8.
        self.content = str()  # FIXME: Should be bytearray() once we require Python 2.6.
        self.decoded_content = None
        self.malloc = None
        self.js_heap = None

    def decode_content(self):
        if self.encoding == 'base64' and self.content is not None:
            self.decoded_content = base64.b64decode(self.content)
        else:
            self.decoded_content = self.content


class DriverProxy(object):
    """A wrapper for managing two Driver instances, one with pixel tests and
    one without. This allows us to handle plain text tests and ref tests with a
    single driver."""

    def __init__(self, port, worker_number, driver_instance_constructor, pixel_tests, no_timeout):
        self._port = port
        self._worker_number = worker_number
        self._driver_instance_constructor = driver_instance_constructor
        self._no_timeout = no_timeout

        # FIXME: We shouldn't need to create a driver until we actually run a test.
        self._driver = self._make_driver(pixel_tests)
        self._driver_cmd_line = None

    def _make_driver(self, pixel_tests):
        return self._driver_instance_constructor(self._port, self._worker_number, pixel_tests, self._no_timeout)

    @property
    def host(self):
        return self._driver._target_host

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def is_http_test(self, test_name):
        return self._driver.is_http_test(test_name)

    def is_web_platform_test(self, test_name):
        return self._driver.is_web_platform_test(test_name)

    def is_webkit_specific_web_platform_test(self, test_name):
        return self._driver.is_webkit_specific_web_platform_test(test_name)

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def test_to_uri(self, test_name):
        return self._driver.test_to_uri(test_name)

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def uri_to_test(self, uri):
        return self._driver.uri_to_test(uri)

    def run_test(self, driver_input, stop_when_done):
        pixel_tests_needed = driver_input.should_run_pixel_test
        cmd_line_key = self._cmd_line_as_key(pixel_tests_needed, driver_input.args)
        if cmd_line_key != self._driver_cmd_line:
            self._driver.stop()
            self._driver = self._make_driver(pixel_tests_needed)
            self._driver_cmd_line = cmd_line_key

        return self._driver.run_test(driver_input, stop_when_done)

    def do_post_tests_work(self):
        return self._driver.do_post_tests_work()

    def has_crashed(self):
        return self._driver.has_crashed()

    def stop(self):
        self._driver.stop()

    # FIXME: this should be a @classmethod (or implemented on Port instead).
    def cmd_line(self, pixel_tests=None, per_test_args=None):
        return self._driver.cmd_line(pixel_tests, per_test_args or [])

    def _cmd_line_as_key(self, pixel_tests, per_test_args):
        return ' '.join(self.cmd_line(pixel_tests, per_test_args))
