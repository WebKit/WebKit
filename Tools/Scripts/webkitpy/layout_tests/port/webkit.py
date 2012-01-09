#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi <rgabor@inf.u-szeged.hu>, University of Szeged
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

"""WebKit implementations of the Port interface."""

import base64
import logging
import operator
import os
import re
import sys
import time

from webkitpy.common.memoized import memoized
from webkitpy.common.net.buildbot import BuildBot
from webkitpy.common.system.environment import Environment
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.path import cygpath
from webkitpy.layout_tests.port import builders, server_process, Port, Driver, DriverOutput


_log = logging.getLogger(__name__)


class WebKitPort(Port):
    def __init__(self, host, **kwargs):
        Port.__init__(self, host, **kwargs)

        # FIXME: Disable pixel tests until they are run by default on build.webkit.org.
        self.set_option_default("pixel_tests", False)
        # WebKit ports expect a 35s timeout, or 350s timeout when running with -g/--guard-malloc.
        # FIXME: --guard-malloc is only supported on Mac, so this logic should be in mac.py.
        default_time_out_seconds = 350 if self.get_option('guard_malloc') else 35
        self.set_option_default("time_out_ms", default_time_out_seconds * 1000)

    def driver_name(self):
        if self.get_option('webkit_test_runner'):
            return "WebKitTestRunner"
        return "DumpRenderTree"

    # FIXME: Eventually we should standarize port naming, and make this method smart enough
    # to use for all port configurations (including architectures, graphics types, etc).
    def baseline_search_path(self):
        search_paths = []
        if self.get_option('webkit_test_runner'):
            search_paths.append(self._wk2_port_name())
        search_paths.append(self.name())
        if self.name() != self.port_name:
            search_paths.append(self.port_name)
        return map(self._webkit_baseline_path, search_paths)

    def path_to_test_expectations_file(self):
        # test_expectations are always in mac/ not mac-leopard/ by convention, hence we use port_name instead of name().
        return self._filesystem.join(self._webkit_baseline_path(self.port_name), 'test_expectations.txt')

    def _port_flag_for_scripts(self):
        # This is overrriden by ports which need a flag passed to scripts to distinguish the use of that port.
        # For example --qt on linux, since a user might have both Gtk and Qt libraries installed.
        # FIXME: Chromium should override this once ChromiumPort is a WebKitPort.
        return None

    # This is modeled after webkitdirs.pm argumentsForConfiguration() from old-run-webkit-tests
    def _arguments_for_configuration(self):
        config_args = []
        config_args.append(self._config.flag_for_configuration(self.get_option('configuration')))
        # FIXME: We may need to add support for passing --32-bit like old-run-webkit-tests had.
        port_flag = self._port_flag_for_scripts()
        if port_flag:
            config_args.append(port_flag)
        return config_args

    def _run_script(self, script_name, args=None, include_configuration_arguments=True, decode_output=True, env=None):
        run_script_command = [self._config.script_path(script_name)]
        if include_configuration_arguments:
            run_script_command.extend(self._arguments_for_configuration())
        if args:
            run_script_command.extend(args)
        output = self._executive.run_command(run_script_command, cwd=self._config.webkit_base_dir(), decode_output=decode_output, env=env)
        _log.debug('Output of %s:\n%s' % (run_script_command, output))
        return output

    def _build_driver(self):
        environment = self.host.copy_current_environment()
        environment.disable_gcc_smartquotes()
        env = environment.to_dictionary()

        # FIXME: We build both DumpRenderTree and WebKitTestRunner for
        # WebKitTestRunner runs because DumpRenderTree still includes
        # the DumpRenderTreeSupport module and the TestNetscapePlugin.
        # These two projects should be factored out into their own
        # projects.
        try:
            self._run_script("build-dumprendertree", env=env)
            if self.get_option('webkit_test_runner'):
                self._run_script("build-webkittestrunner", env=env)
        except ScriptError, e:
            _log.error(e.message_with_output(output_limit=None))
            return False
        return True

    def _check_driver(self):
        driver_path = self._path_to_driver()
        if not self._filesystem.exists(driver_path):
            _log.error("%s was not found at %s" % (self.driver_name(), driver_path))
            return False
        return True

    def check_build(self, needs_http):
        # If we're using a pre-built copy of WebKit (--root), we assume it also includes a build of DRT.
        if not self.get_option('root') and self.get_option('build') and not self._build_driver():
            return False
        if not self._check_driver():
            return False
        if self.get_option('pixel_tests'):
            if not self.check_image_diff():
                return False
        if not self._check_port_build():
            return False
        return True

    def _check_port_build(self):
        # Ports can override this method to do additional checks.
        return True

    def check_image_diff(self, override_step=None, logging=True):
        image_diff_path = self._path_to_image_diff()
        if not self._filesystem.exists(image_diff_path):
            _log.error("ImageDiff was not found at %s" % image_diff_path)
            return False
        return True

    def diff_image(self, expected_contents, actual_contents, tolerance=None):
        # Handle the case where the test didn't actually generate an image.
        # FIXME: need unit tests for this.
        if not actual_contents and not expected_contents:
            return (None, 0)
        if not actual_contents or not expected_contents:
            # FIXME: It's not clear what we should return in this case.
            # Maybe we should throw an exception?
            return (True, 0)

        process = self._start_image_diff_process(expected_contents, actual_contents, tolerance=tolerance)
        return self._read_image_diff(process)

    def _start_image_diff_process(self, expected_contents, actual_contents, tolerance=None):
        # FIXME: There needs to be a more sane way of handling default
        # values for options so that you can distinguish between a default
        # value of None and a default value that wasn't set.
        if tolerance is None:
            if self.get_option('tolerance') is not None:
                tolerance = self.get_option('tolerance')
            else:
                tolerance = 0.1
        command = [self._path_to_image_diff(), '--tolerance', str(tolerance)]
        process = server_process.ServerProcess(self, 'ImageDiff', command)

        process.write('Content-Length: %d\n%sContent-Length: %d\n%s' % (
            len(actual_contents), actual_contents,
            len(expected_contents), expected_contents))
        return process

    def _read_image_diff(self, sp):
        deadline = time.time() + 2.0
        output = None
        output_image = ""

        while True:
            output = sp.read_stdout_line(deadline)
            if sp.timed_out or sp.crashed or not output:
                break

            if output.startswith('diff'):  # This is the last line ImageDiff prints.
                break

            if output.startswith('Content-Length'):
                m = re.match('Content-Length: (\d+)', output)
                content_length = int(m.group(1))
                output_image = sp.read_stdout(deadline, content_length)
                output = sp.read_stdout_line(deadline)
                break

        if sp.timed_out:
            _log.error("ImageDiff timed out")
        if sp.crashed:
            _log.error("ImageDiff crashed")
        # FIXME: There is no need to shut down the ImageDiff server after every diff.
        sp.stop()

        diff_percent = 0
        if output and output.startswith('diff'):
            m = re.match('diff: (.+)% (passed|failed)', output)
            if m.group(2) == 'passed':
                return [None, 0]
            diff_percent = float(m.group(1))

        return (output_image, diff_percent)

    def setup_environ_for_server(self, server_name=None):
        clean_env = super(WebKitPort, self).setup_environ_for_server(server_name)
        self._copy_value_from_environ_if_set(clean_env, 'WEBKIT_TESTFONTS')
        return clean_env

    def default_results_directory(self):
        # Results are store relative to the built products to make it easy
        # to have multiple copies of webkit checked out and built.
        return self._build_path('layout-test-results')

    def _driver_class(self):
        return WebKitDriver

    def _tests_for_other_platforms(self):
        # By default we will skip any directory under LayoutTests/platform
        # that isn't in our baseline search path (this mirrors what
        # old-run-webkit-tests does in findTestsToRun()).
        # Note this returns LayoutTests/platform/*, not platform/*/*.
        entries = self._filesystem.glob(self._webkit_baseline_path('*'))
        dirs_to_skip = []
        for entry in entries:
            if self._filesystem.isdir(entry) and entry not in self.baseline_search_path():
                basename = self._filesystem.basename(entry)
                dirs_to_skip.append('platform/%s' % basename)
        return dirs_to_skip

    def _runtime_feature_list(self):
        """Return the supported features of DRT. If a port doesn't support
        this DRT switch, it has to override this method to return None"""
        supported_features_command = [self._path_to_driver(), '--print-supported-features']
        try:
            output = self._executive.run_command(supported_features_command, error_handler=Executive.ignore_error)
        except OSError, e:
            _log.warn("Exception running driver: %s, %s.  Driver must be built before calling WebKitPort.test_expectations()." % (supported_features_command, e))
            return None

        # Note: win/DumpRenderTree.cpp does not print a leading space before the features_string.
        match_object = re.match("SupportedFeatures:\s*(?P<features_string>.*)\s*", output)
        if not match_object:
            return None
        return match_object.group('features_string').split(' ')

    def _webcore_symbols_string(self):
        webcore_library_path = self._path_to_webcore_library()
        if not webcore_library_path:
            return None
        try:
            return self._executive.run_command(['nm', webcore_library_path], error_handler=Executive.ignore_error)
        except OSError, e:
            _log.warn("Failed to run nm: %s.  Can't determine WebCore supported features." % e)
        return None

    # Ports which use run-time feature detection should define this method and return
    # a dictionary mapping from Feature Names to skipped directoires.  NRWT will
    # run DumpRenderTree --print-supported-features and parse the output.
    # If the Feature Names are not found in the output, the corresponding directories
    # will be skipped.
    def _missing_feature_to_skipped_tests(self):
        """Return the supported feature dictionary. Keys are feature names and values
        are the lists of directories to skip if the feature name is not matched."""
        # FIXME: This list matches WebKitWin and should be moved onto the Win port.
        return {
            "Accelerated Compositing": ["compositing"],
            "3D Rendering": ["animations/3d", "transforms/3d"],
        }

    # Ports which use compile-time feature detection should define this method and return
    # a dictionary mapping from symbol substrings to possibly disabled test directories.
    # When the symbol substrings are not matched, the directories will be skipped.
    # If ports don't ever enable certain features, then those directories can just be
    # in the Skipped list instead of compile-time-checked here.
    def _missing_symbol_to_skipped_tests(self):
        """Return the supported feature dictionary. The keys are symbol-substrings
        and the values are the lists of directories to skip if that symbol is missing."""
        return {
            "MathMLElement": ["mathml"],
            "GraphicsLayer": ["compositing"],
            "WebCoreHas3DRendering": ["animations/3d", "transforms/3d"],
            "WebGLShader": ["fast/canvas/webgl", "compositing/webgl", "http/tests/canvas/webgl"],
            "MHTMLArchive": ["mhtml"],
        }

    def _skipped_tests_for_unsupported_features(self):
        # If the port supports runtime feature detection, disable any tests
        # for features missing from the runtime feature list.
        supported_feature_list = self._runtime_feature_list()
        # If _runtime_feature_list returns a non-None value, then prefer
        # runtime feature detection over static feature detection.
        if supported_feature_list is not None:
            return reduce(operator.add, [directories for feature, directories in self._missing_feature_to_skipped_tests().items() if feature not in supported_feature_list])

        # Runtime feature detection not supported, fallback to static dectection:
        # Disable any tests for symbols missing from the webcore symbol string.
        webcore_symbols_string = self._webcore_symbols_string()
        if webcore_symbols_string is not None:
            return reduce(operator.add, [directories for symbol_substring, directories in self._missing_symbol_to_skipped_tests().items() if symbol_substring not in webcore_symbols_string], [])
        # Failed to get any runtime or symbol information, don't skip any tests.
        return []

    def _tests_from_skipped_file_contents(self, skipped_file_contents):
        tests_to_skip = []
        for line in skipped_file_contents.split('\n'):
            line = line.strip()
            line = line.rstrip('/')  # Best to normalize directory names to not include the trailing slash.
            if line.startswith('#') or not len(line):
                continue
            tests_to_skip.append(line)
        return tests_to_skip

    def _wk2_port_name(self):
        # By current convention, the WebKit2 name is always mac-wk2, win-wk2, not mac-leopard-wk2, etc.
        return "%s-wk2" % self.port_name

    def _skipped_file_search_paths(self):
        # Unlike baseline_search_path, we only want to search [WK2-PORT, PORT-VERSION, PORT] not the full casade.
        # Note order doesn't matter since the Skipped file contents are all combined.
        #
        # FIXME: It's not correct to assume that port names map directly to
        # directory names. For example, mac-future is a port name that does
        # not have a cooresponding directory. The WebKit2 ports are another
        # example.
        search_paths = set([self.port_name, self.name()])
        if self.get_option('webkit_test_runner'):
            # Because nearly all of the skipped tests for WebKit 2 are due to cross-platform
            # issues, all wk2 ports share a skipped list under platform/wk2.
            search_paths.update([self._wk2_port_name(), "wk2"])
        return search_paths

    def _expectations_from_skipped_files(self):
        tests_to_skip = []
        for search_path in self._skipped_file_search_paths():
            filename = self._filesystem.join(self._webkit_baseline_path(search_path), "Skipped")
            if not self._filesystem.exists(filename):
                _log.debug("Skipped does not exist: %s" % filename)
                continue
            _log.debug("Using Skipped file: %s" % filename)
            skipped_file_contents = self._filesystem.read_text_file(filename)
            tests_to_skip.extend(self._tests_from_skipped_file_contents(skipped_file_contents))
        return tests_to_skip

    def test_expectations(self):
        # This allows ports to use a combination of test_expectations.txt files and Skipped lists.
        expectations = ''
        expectations_path = self.path_to_test_expectations_file()
        if self._filesystem.exists(expectations_path):
            _log.debug("Using test_expectations.txt: %s" % expectations_path)
            expectations = self._filesystem.read_text_file(expectations_path)
        return expectations

    def skipped_layout_tests(self):
        # Use a set to allow duplicates
        tests_to_skip = set(self._expectations_from_skipped_files())
        tests_to_skip.update(self._tests_for_other_platforms())
        tests_to_skip.update(self._skipped_tests_for_unsupported_features())
        return tests_to_skip

    def skipped_tests(self):
        return self.skipped_layout_tests()

    def _build_path(self, *comps):
        # --root is used for running with a pre-built root (like from a nightly zip).
        build_directory = self.get_option('root')
        if not build_directory:
            build_directory = self._config.build_directory(self.get_option('configuration'))
        return self._filesystem.join(build_directory, *comps)

    def _path_to_driver(self):
        return self._build_path(self.driver_name())

    def _path_to_webcore_library(self):
        return None

    def _path_to_helper(self):
        return None

    def _path_to_image_diff(self):
        return self._build_path('ImageDiff')

    def _path_to_wdiff(self):
        # FIXME: This does not exist on a default Mac OS X Leopard install.
        return 'wdiff'

    # FIXME: This does not belong on the port object.
    @memoized
    def _path_to_apache(self):
        # The Apache binary path can vary depending on OS and distribution
        # See http://wiki.apache.org/httpd/DistrosDefaultLayout
        for path in ["/usr/sbin/httpd", "/usr/sbin/apache2"]:
            if self._filesystem.exists(path):
                return path
        _log.error("Could not find apache. Not installed or unknown path.")
        return None

    # FIXME: This belongs on some platform abstraction instead of Port.
    def _is_redhat_based(self):
        return self._filesystem.exists('/etc/redhat-release')

    def _is_debian_based(self):
        return self._filesystem.exists('/etc/debian_version')

    # We pass sys_platform into this method to make it easy to unit test.
    def _apache_config_file_name_for_platform(self, sys_platform):
        if sys_platform == 'cygwin':
            return 'cygwin-httpd.conf'  # CYGWIN is the only platform to still use Apache 1.3.
        if sys_platform.startswith('linux'):
            if self._is_redhat_based():
                return 'fedora-httpd.conf'  # This is an Apache 2.x config file despite the naming.
            if self._is_debian_based():
                return 'apache2-debian-httpd.conf'
        # All platforms use apache2 except for CYGWIN (and Mac OS X Tiger and prior, which we no longer support).
        return "apache2-httpd.conf"

    def _path_to_apache_config_file(self):
        config_file_name = self._apache_config_file_name_for_platform(sys.platform)
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf', config_file_name)


class WebKitDriver(Driver):
    """WebKit implementation of the DumpRenderTree/WebKitTestRunner interface."""

    def __init__(self, port, worker_number, pixel_tests):
        Driver.__init__(self, port, worker_number, pixel_tests)
        self._driver_tempdir = port._filesystem.mkdtemp(prefix='%s-' % self._port.driver_name())
        # WebKitTestRunner can report back subprocess crashes by printing
        # "#CRASHED - PROCESSNAME".  Since those can happen at any time
        # and ServerProcess won't be aware of them (since the actual tool
        # didn't crash, just a subprocess) we record the crashed subprocess name here.
        self._crashed_subprocess_name = None

        # stderr reading is scoped on a per-test (not per-block) basis, so we store the accumulated
        # stderr output, as well as if we've seen #EOF on this driver instance.
        # FIXME: We should probably remove _read_first_block and _read_optional_image_block and
        # instead scope these locally in run_test.
        self.error_from_test = str()
        self.err_seen_eof = False
        self._server_process = None

    # FIXME: This may be unsafe, as python does not guarentee any ordering of __del__ calls
    # I believe it's possible that self._port or self._port._filesystem may already be destroyed.
    def __del__(self):
        self._port._filesystem.rmtree(str(self._driver_tempdir))

    def cmd_line(self):
        cmd = self._command_wrapper(self._port.get_option('wrapper'))
        cmd.append(self._port._path_to_driver())
        if self._pixel_tests:
            cmd.append('--pixel-tests')
        if self._port.get_option('gc_between_tests'):
            cmd.append('--gc-between-tests')
        if self._port.get_option('complex_text'):
            cmd.append('--complex-text')
        if self._port.get_option('threaded'):
            cmd.append('--threaded')
        # FIXME: We need to pass --timeout=SECONDS to WebKitTestRunner for WebKit2.

        cmd.extend(self._port.get_option('additional_drt_flag', []))
        cmd.append('-')
        return cmd

    def _start(self):
        server_name = self._port.driver_name()
        environment = self._port.setup_environ_for_server(server_name)
        environment['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        # FIXME: We're assuming that WebKitTestRunner checks this DumpRenderTree-named environment variable.
        environment['DUMPRENDERTREE_TEMP'] = str(self._driver_tempdir)
        environment['LOCAL_RESOURCE_ROOT'] = self._port.layout_tests_dir()
        self._crashed_subprocess_name = None
        self._server_process = server_process.ServerProcess(self._port, server_name, self.cmd_line(), environment)

    def has_crashed(self):
        if self._server_process is None:
            return False
        return self._server_process.poll() is not None

    def _check_for_driver_crash(self, error_line):
        if error_line == "#CRASHED\n":
            # This is used on Windows to report that the process has crashed
            # See http://trac.webkit.org/changeset/65537.
            self._server_process.set_crashed(True)
        elif error_line == "#CRASHED - WebProcess\n":
            # WebKitTestRunner uses this to report that the WebProcess subprocess crashed.
            self._subprocess_crashed("WebProcess")
        return self._detected_crash()

    def _detected_crash(self):
        # We can't just check self._server_process.crashed because WebKitTestRunner
        # can report subprocess crashes at any time by printing
        # "#CRASHED - WebProcess", we want to count those as crashes as well.
        return self._server_process.crashed or self._crashed_subprocess_name

    def _subprocess_crashed(self, subprocess_name):
        self._crashed_subprocess_name = subprocess_name

    def _crashed_process_name(self):
        if not self._detected_crash():
            return None
        return self._crashed_subprocess_name or self._server_process.process_name()

    def _command_from_driver_input(self, driver_input):
        if self.is_http_test(driver_input.test_name):
            command = self.test_to_uri(driver_input.test_name)
        else:
            command = self._port.abspath_for_test(driver_input.test_name)
            if sys.platform == 'cygwin':
                command = cygpath(command)

        if driver_input.image_hash:
            # FIXME: Why the leading quote?
            command += "'" + driver_input.image_hash
        return command + "\n"

    def _read_first_block(self, deadline):
        # returns (text_content, audio_content)
        block = self._read_block(deadline)
        if block.content_type == 'audio/wav':
            return (None, block.decoded_content)
        return (block.decoded_content, None)

    def _read_optional_image_block(self, deadline):
        # returns (image, actual_image_hash)
        block = self._read_block(deadline, wait_for_stderr_eof=True)
        if block.content and block.content_type == 'image/png':
            return (block.decoded_content, block.content_hash)
        return (None, block.content_hash)

    def run_test(self, driver_input):
        if not self._server_process:
            self._start()
        self.error_from_test = str()
        self.err_seen_eof = False

        command = self._command_from_driver_input(driver_input)
        start_time = time.time()
        deadline = time.time() + int(driver_input.timeout) / 1000.0

        self._server_process.write(command)
        text, audio = self._read_first_block(deadline)  # First block is either text or audio
        image, actual_image_hash = self._read_optional_image_block(deadline)  # The second (optional) block is image data.

        # We may not have read all of the output if an error (crash) occured.
        # Since some platforms output the stacktrace over error, we should
        # dump any buffered error into self.error_from_test.
        # FIXME: We may need to also read stderr until the process dies?
        self.error_from_test += self._server_process.pop_all_buffered_stderr()

        return DriverOutput(text, image, actual_image_hash, audio,
            crash=self._detected_crash(), test_time=time.time() - start_time,
            timeout=self._server_process.timed_out, error=self.error_from_test,
            crashed_process_name=self._crashed_process_name())

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
            or self._read_header(block, line, 'ActualHash: ', 'content_hash')):
            return
        # Note, we're not reading ExpectedHash: here, but we could.
        # If the line wasn't a header, we just append it to the content.
        block.content += line

    def _strip_eof(self, line):
        if line and line.endswith("#EOF\n"):
            return line[:-5], True
        return line, False

    def _read_block(self, deadline, wait_for_stderr_eof=False):
        block = ContentBlock()
        out_seen_eof = False

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

            if self._server_process.timed_out or self._detected_crash():
                break

            if out_line:
                assert not out_seen_eof
                out_line, out_seen_eof = self._strip_eof(out_line)
            if err_line:
                assert not self.err_seen_eof
                err_line, self.err_seen_eof = self._strip_eof(err_line)

            if out_line:
                if out_line[-1] != "\n":
                    _log.error("Last character read from DRT stdout line was not a newline!  This indicates either a NRWT or DRT bug.")
                content_length_before_header_check = block._content_length
                self._process_stdout_line(block, out_line)
                # FIXME: Unlike HTTP, DRT dumps the content right after printing a Content-Length header.
                # Don't wait until we're done with headers, just read the binary blob right now.
                if content_length_before_header_check != block._content_length:
                    block.content = self._server_process.read_stdout(deadline, block._content_length)

            if err_line:
                if self._check_for_driver_crash(err_line):
                    break
                self.error_from_test += err_line

        block.decode_content()
        return block

    def stop(self):
        if self._server_process:
            self._server_process.stop()
            self._server_process = None


class ContentBlock(object):
    def __init__(self):
        self.content_type = None
        self.encoding = None
        self.content_hash = None
        self._content_length = None
        # Content is treated as binary data even though the text output is usually UTF-8.
        self.content = str()  # FIXME: Should be bytearray() once we require Python 2.6.
        self.decoded_content = None

    def decode_content(self):
        if self.encoding == 'base64':
            self.decoded_content = base64.b64decode(self.content)
        else:
            self.decoded_content = self.content
