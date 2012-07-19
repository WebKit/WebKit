#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi <rgabor@inf.u-szeged.hu>, University of Szeged
# Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

import itertools
import logging
import operator
import re
import time

from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.layout_tests.port import server_process, Port


_log = logging.getLogger(__name__)


class WebKitPort(Port):

    # FIXME: Eventually we should standarize port naming, and make this method smart enough
    # to use for all port configurations (including architectures, graphics types, etc).
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
            self._run_script("build-dumprendertree", args=self._build_driver_flags(), env=env)
            if self.get_option('webkit_test_runner'):
                self._run_script("build-webkittestrunner", args=self._build_driver_flags(), env=env)
        except ScriptError, e:
            _log.error(e.message_with_output(output_limit=None))
            return False
        return True

    def _build_driver_flags(self):
        return []

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

    def _image_diff_command(self, tolerance=None):
        # FIXME: There needs to be a more sane way of handling default
        # values for options so that you can distinguish between a default
        # value of None and a default value that wasn't set.
        if tolerance is None:
            if self.get_option('tolerance') is not None:
                tolerance = self.get_option('tolerance')
            else:
                tolerance = 0.1

        command = [self._path_to_image_diff(), '--tolerance', str(tolerance)]
        return command

    def _start_image_diff_process(self, expected_contents, actual_contents, tolerance=None):
        command = self._image_diff_command(tolerance)
        environment = self.setup_environ_for_server('ImageDiff')
        process = server_process.ServerProcess(self, 'ImageDiff', command, environment)

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
            if sp.timed_out or sp.has_crashed() or not output:
                break

            if output.startswith('diff'):  # This is the last line ImageDiff prints.
                break

            if output.startswith('Content-Length'):
                m = re.match('Content-Length: (\d+)', output)
                content_length = int(m.group(1))
                output_image = sp.read_stdout(deadline, content_length)
                output = sp.read_stdout_line(deadline)
                break

        stderr = sp.pop_all_buffered_stderr()
        if stderr:
            _log.warn("ImageDiff produced stderr output:\n" + stderr)
        if sp.timed_out:
            _log.error("ImageDiff timed out")
        if sp.has_crashed():
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
        """If a port makes certain features available only through runtime flags, it can override this routine to indicate which ones are available."""
        return None

    def nm_command(self):
        return 'nm'

    def _modules_to_search_for_symbols(self):
        path = self._path_to_webcore_library()
        if path:
            return [path]
        return []

    def _symbols_string(self):
        symbols = ''
        for path_to_module in self._modules_to_search_for_symbols():
            try:
                symbols += self._executive.run_command([self.nm_command(), path_to_module], error_handler=Executive.ignore_error)
            except OSError, e:
                _log.warn("Failed to run nm: %s.  Can't determine supported features correctly." % e)
        return symbols

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
            "CSSVariableValue": ["fast/css/variables", "inspector/styles/variables"],
        }

    def _has_test_in_directories(self, directory_lists, test_list):
        if not test_list:
            return False

        directories = itertools.chain.from_iterable(directory_lists)
        for directory, test in itertools.product(directories, test_list):
            if test.startswith(directory):
                return True
        return False

    def _skipped_tests_for_unsupported_features(self, test_list):
        # Only check the runtime feature list of there are tests in the test_list that might get skipped.
        # This is a performance optimization to avoid the subprocess call to DRT.
        if self._has_test_in_directories(self._missing_feature_to_skipped_tests().values(), test_list):
            # If the port supports runtime feature detection, disable any tests
            # for features missing from the runtime feature list.
            supported_feature_list = self._runtime_feature_list()
            # If _runtime_feature_list returns a non-None value, then prefer
            # runtime feature detection over static feature detection.
            if supported_feature_list is not None:
                return reduce(operator.add, [directories for feature, directories in self._missing_feature_to_skipped_tests().items() if feature not in supported_feature_list])

        # Only check the symbols of there are tests in the test_list that might get skipped.
        # This is a performance optimization to avoid the calling nm.
        if self._has_test_in_directories(self._missing_symbol_to_skipped_tests().values(), test_list):
            # Runtime feature detection not supported, fallback to static dectection:
            # Disable any tests for symbols missing from the executable or libraries.
            symbols_string = self._symbols_string()
            if symbols_string is not None:
                return reduce(operator.add, [directories for symbol_substring, directories in self._missing_symbol_to_skipped_tests().items() if symbol_substring not in symbols_string], [])

        # Failed to get any runtime or symbol information, don't skip any tests.
        return []

    def _wk2_port_name(self):
        # By current convention, the WebKit2 name is always mac-wk2, win-wk2, not mac-leopard-wk2, etc,
        # except for Qt because WebKit2 is only supported by Qt 5.0 (therefore: qt-5.0-wk2).
        return "%s-wk2" % self.port_name

    def _skipped_file_search_paths(self):
        # Unlike baseline_search_path, we only want to search [WK2-PORT, PORT-VERSION, PORT] and any directories
        # included via --additional-platform-directory, not the full casade.
        # Note order doesn't matter since the Skipped file contents are all combined.
        search_paths = set([self.port_name])
        if 'future' not in self.name():
            search_paths.add(self.name())
        if self.get_option('webkit_test_runner'):
            # Because nearly all of the skipped tests for WebKit 2 are due to cross-platform
            # issues, all wk2 ports share a skipped list under platform/wk2.
            search_paths.update([self._wk2_port_name(), "wk2"])
        search_paths.update(self.get_option("additional_platform_directory", []))

        return search_paths

    def skipped_layout_tests(self, test_list):
        tests_to_skip = set(self._expectations_from_skipped_files(self._skipped_file_search_paths()))
        tests_to_skip.update(self._tests_for_other_platforms())
        tests_to_skip.update(self._skipped_tests_for_unsupported_features(test_list))
        return tests_to_skip
