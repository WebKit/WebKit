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
from webkitpy.common.system.executive import ScriptError
from webkitpy.layout_tests.port import builders, server_process, Port, Driver, DriverOutput


_log = logging.getLogger(__name__)


class WebKitPort(Port):
    def __init__(self, **kwargs):
        Port.__init__(self, **kwargs)

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
        expectations_directory = self._wk2_port_name() if self.get_option('webkit_test_runner') else self.port_name
        return self._filesystem.join(self._webkit_baseline_path(expectations_directory), 'test_expectations.txt')

    def _driver_build_script_name(self):
        if self.get_option('webkit_test_runner'):
            return "build-webkittestrunner"
        return "build-dumprendertree"

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

    def _run_script(self, script_name, args=None, include_configuration_arguments=True, decode_output=True):
        run_script_command = [self._config.script_path(script_name)]
        if include_configuration_arguments:
            run_script_command.extend(self._arguments_for_configuration())
        if args:
            run_script_command.extend(args)
        return self._executive.run_command(run_script_command, cwd=self._config.webkit_base_dir(), decode_output=decode_output)

    def _build_driver(self):
        try:
            self._run_script(self._driver_build_script_name())
        except ScriptError:
            _log.error("Failed to build %s" % self.driver_name())
            return False
        return True

    def _check_driver(self):
        driver_path = self._path_to_driver()
        if not self._filesystem.exists(driver_path):
            _log.error("%s was not found at %s" % (self.driver_name(), driver_path))
            return False
        return True

    def check_build(self, needs_http):
        if self.get_option('build') and not self._build_driver():
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

    def diff_image(self, expected_contents, actual_contents):
        # Handle the case where the test didn't actually generate an image.
        # FIXME: need unit tests for this.
        if not actual_contents and not expected_contents:
            return None
        if not actual_contents or not expected_contents:
            # FIXME: It's not clear what we should return in this case.
            # Maybe we should throw an exception?
            return True

        process = self._start_image_diff_process(expected_contents, actual_contents)
        return self._read_image_diff(process)

    def _start_image_diff_process(self, expected_contents, actual_contents):
        # FIXME: There needs to be a more sane way of handling default
        # values for options so that you can distinguish between a default
        # value of None and a default value that wasn't set.
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
        timeout = 2.0
        deadline = time.time() + timeout
        output = sp.read_line(timeout)
        while not sp.timed_out and not sp.crashed and output:
            if output.startswith('Content-Length'):
                m = re.match('Content-Length: (\d+)', output)
                content_length = int(m.group(1))
                timeout = deadline - time.time()
                output = sp.read(timeout, content_length)
                break
            elif output.startswith('diff'):
                break
            else:
                timeout = deadline - time.time()
                output = sp.read_line(deadline)

        if sp.timed_out:
            _log.error("ImageDiff timed out")
        if sp.crashed:
            _log.error("ImageDiff crashed")
        sp.stop()
        if output.startswith('diff'):
            m = re.match('diff: (.+)% (passed|failed)', output)
            if m.group(2) == 'passed':
                return None
        return output

    def default_results_directory(self):
        # Results are store relative to the built products to make it easy
        # to have multiple copies of webkit checked out and built.
        return self._build_path('layout-test-results')

    def create_driver(self, worker_number):
        return WebKitDriver(self, worker_number)

    def _tests_for_other_platforms(self):
        # By default we will skip any directory under LayoutTests/platform
        # that isn't in our baseline search path (this mirrors what
        # old-run-webkit-tests does in findTestsToRun()).
        # Note this returns LayoutTests/platform/*, not platform/*/*.
        entries = self._filesystem.glob(self._webkit_baseline_path('*'))
        dirs_to_skip = []
        for entry in entries:
            if self._filesystem.isdir(entry) and not entry in self.baseline_search_path():
                basename = self._filesystem.basename(entry)
                dirs_to_skip.append('platform/%s' % basename)
        return dirs_to_skip

    def _runtime_feature_list(self):
        """Return the supported features of DRT. If a port doesn't support
        this DRT switch, it has to override this method to return None"""
        driver_path = self._path_to_driver()
        feature_list = ' '.join(os.popen(driver_path + " --print-supported-features 2>&1").readlines())
        if "SupportedFeatures:" in feature_list:
            return feature_list
        return None

    def _supported_symbol_list(self):
        """Return the supported symbols of WebCore."""
        webcore_library_path = self._path_to_webcore_library()
        if not webcore_library_path:
            return None
        symbol_list = ' '.join(os.popen("nm " + webcore_library_path).readlines())
        return symbol_list

    def _directories_for_features(self):
        """Return the supported feature dictionary. The keys are the
        features and the values are the directories in lists."""
        directories_for_features = {
            "Accelerated Compositing": ["compositing"],
            "3D Rendering": ["animations/3d", "transforms/3d"],
        }
        return directories_for_features

    def _directories_for_symbols(self):
        """Return the supported feature dictionary. The keys are the
        symbols and the values are the directories in lists."""
        directories_for_symbol = {
            "MathMLElement": ["mathml"],
            "GraphicsLayer": ["compositing"],
            "WebCoreHas3DRendering": ["animations/3d", "transforms/3d"],
            "WebGLShader": ["fast/canvas/webgl", "compositing/webgl", "http/tests/canvas/webgl"],
            "parseWCSSInputProperty": ["fast/wcss"],
            "isXHTMLMPDocument": ["fast/xhtmlmp"],
            "MHTMLArchive": ["mhtml"],
        }
        return directories_for_symbol

    def _skipped_tests_for_unsupported_features(self):
        """Return the directories of unsupported tests. Search for the
        symbols in the symbol_list, if found add the corresponding
        directories to the skipped directory list."""
        feature_list = self._runtime_feature_list()
        directories = self._directories_for_features()

        # if DRT feature detection not supported
        if not feature_list:
            feature_list = self._supported_symbol_list()
            directories = self._directories_for_symbols()

        if not feature_list:
            return []

        skipped_directories = [directories[feature]
                              for feature in directories.keys()
                              if feature not in feature_list]
        return reduce(operator.add, skipped_directories)

    def _tests_from_skipped_file_contents(self, skipped_file_contents):
        tests_to_skip = []
        for line in skipped_file_contents.split('\n'):
            line = line.strip()
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
                _log.warn("Failed to open Skipped file: %s" % filename)
                continue
            _log.debug("Using Skipped file: %s" % filename)
            skipped_file_contents = self._filesystem.read_text_file(filename)
            tests_to_skip.extend(self._tests_from_skipped_file_contents(skipped_file_contents))
        return tests_to_skip

    def test_expectations(self):
        # This allows ports to use a combination of test_expectations.txt files and Skipped lists.
        expectations = self._skipped_list_as_expectations()
        expectations_path = self.path_to_test_expectations_file()
        if self._filesystem.exists(expectations_path):
            _log.debug("Using test_expectations.txt: %s" % expectations_path)
            expectations = self._filesystem.read_text_file(expectations_path) + expectations
        return expectations

    def _skipped_list_as_expectations(self):
        # Each Skipped file contains a list of files
        # or directories to be skipped during the test run. The total list
        # of tests to skipped is given by the contents of the generic
        # Skipped file found in platform/X plus a version-specific file
        # found in platform/X-version. Duplicate entries are allowed.
        # This routine reads those files and turns contents into the
        # format expected by test_expectations.

        tests_to_skip = self.skipped_layout_tests()
        skip_lines = map(lambda test_path: "BUG_SKIPPED SKIP : %s = FAIL" % test_path, tests_to_skip)
        return "\n".join(skip_lines)

    def skipped_layout_tests(self):
        # Use a set to allow duplicates
        tests_to_skip = set(self._expectations_from_skipped_files())
        tests_to_skip.update(self._tests_for_other_platforms())
        tests_to_skip.update(self._skipped_tests_for_unsupported_features())
        return tests_to_skip

    def _build_path(self, *comps):
        return self._filesystem.join(self._config.build_directory(
            self.get_option('configuration')), *comps)

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

    def __init__(self, port, worker_number):
        Driver.__init__(self, port, worker_number)
        self._driver_tempdir = port._filesystem.mkdtemp(prefix='%s-' % self._port.driver_name())

    # FIXME: This may be unsafe, as python does not guarentee any ordering of __del__ calls
    # I believe it's possible that self._port or self._port._filesystem may already be destroyed.
    def __del__(self):
        self._port._filesystem.rmtree(str(self._driver_tempdir))

    def cmd_line(self):
        cmd = self._command_wrapper(self._port.get_option('wrapper'))
        cmd.append(self._port._path_to_driver())
        if self._port.get_option('pixel_tests'):
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

    def start(self):
        server_name = self._port.driver_name()
        environment = self._port.setup_environ_for_server(server_name)
        environment['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        # FIXME: We're assuming that WebKitTestRunner checks this DumpRenderTree-named environment variable.
        environment['DUMPRENDERTREE_TEMP'] = str(self._driver_tempdir)
        self._server_process = server_process.ServerProcess(self._port, server_name, self.cmd_line(), environment)

    def poll(self):
        return self._server_process.poll()

    def detected_crash(self):
        # FIXME: We can't just check self._server_process.crashed for two reasons:
        # 1. WebKitTestRunner will print "#CRASHED - WebProcess" and then exit if the WebProcess crashes.
        # 2. Adam Roben tells me Windows DumpRenderTree can print "#CRASHED" yet still exit cleanly.
        return self._server_process.crashed

    def _command_from_driver_input(self, driver_input):
        uri = self._port.test_to_uri(driver_input.test_name)
        command = uri[7:] if uri.startswith("file:///") else uri

        if driver_input.image_hash:
            # FIXME: Why the leading quote?
            command += "'" + driver_input.image_hash
        return command + "\n"

    def _read_first_block(self, deadline):
        """Reads a block from the server_process and returns (text_content, audio_content)."""
        if self.detected_crash():
            return (None, None)

        block = self._read_block(deadline)
        if block.content_type == 'audio/wav':
            return (None, block.decoded_content)
        return (block.decoded_content, None)

    def _read_optional_image_block(self, deadline):
        """Reads a block from the server_process and returns (image, actual_image_hash)."""
        if self.detected_crash():
            return (None, None)

        block = self._read_block(deadline)
        if block.content and block.content_type == 'image/png':
            return (block.decoded_content, block.content_hash)
        return (None, block.content_hash)

    def run_test(self, driver_input):
        command = self._command_from_driver_input(driver_input)
        start_time = time.time()
        deadline = time.time() + int(driver_input.timeout) / 1000.0

        self._server_process.write(command)
        text, audio = self._read_first_block(deadline)  # First block is either text or audio
        image, actual_image_hash = self._read_optional_image_block(deadline)  # The second (optional) block is image data.

        error_lines = self._server_process.error.splitlines()
        # FIXME: This is a hack.  It is unclear why sometimes
        # we do not get any error lines from the server_process
        # probably we are not flushing stderr.
        if error_lines and error_lines[-1] == "#EOF":
            error_lines.pop()  # Remove the expected "#EOF"
        error = "\n".join(error_lines)

        # FIXME: This seems like the wrong section of code to be resetting _server_process.error.
        self._server_process.error = ""
        return DriverOutput(text, image, actual_image_hash, audio,
            crash=self.detected_crash(), test_time=time.time() - start_time,
            timeout=self._server_process.timed_out, error=error)

    def _read_block(self, deadline):
        LENGTH_HEADER = 'Content-Length: '
        HASH_HEADER = 'ActualHash: '
        TYPE_HEADER = 'Content-Type: '
        ENCODING_HEADER = 'Content-Transfer-Encoding: '
        content_type = None
        encoding = None
        content_hash = None
        content_length = None

        # Content is treated as binary data even though the text output is usually UTF-8.
        content = ''
        timeout = deadline - time.time()
        line = self._server_process.read_line(timeout)
        eof = False
        while (not self._server_process.timed_out and not self.detected_crash() and not eof):
            chomped_line = line.rstrip()
            if chomped_line.endswith("#EOF"):
                eof = True
                line = chomped_line[:-4]

            if line.startswith(TYPE_HEADER) and content_type is None:
                content_type = line.split()[1]
            elif line.startswith(ENCODING_HEADER) and encoding is None:
                encoding = line.split()[1]
            elif line.startswith(LENGTH_HEADER) and content_length is None:
                timeout = deadline - time.time()
                content_length = int(line[len(LENGTH_HEADER):])
                # FIXME: Technically there should probably be another blank
                # line here, but DRT doesn't write one.
                content = self._server_process.read(timeout, content_length)
            elif line.startswith(HASH_HEADER):
                content_hash = line.split()[1]
            elif line:
                content += line
            if not eof:
                line = self._server_process.read_line(timeout)
                timeout = deadline - time.time()
        return ContentBlock(content_type, encoding, content_hash, content)

    def stop(self):
        if self._server_process:
            self._server_process.stop()
            self._server_process = None


class ContentBlock(object):
    def __init__(self, content_type, encoding, content_hash, content):
        self.content_type = content_type
        self.encoding = encoding
        self.content_hash = content_hash
        self.content = content
        if self.encoding == 'base64':
            self.decoded_content = base64.b64decode(content)
        else:
            self.decoded_content = content
