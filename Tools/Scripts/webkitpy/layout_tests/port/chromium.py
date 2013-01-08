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

from webkitpy.common.system import executive
from webkitpy.common.system.path import cygpath
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port.base import Port, VirtualTestSuite


_log = logging.getLogger(__name__)


class ChromiumPort(Port):
    """Abstract base class for Chromium implementations of the Port class."""

    ALL_SYSTEMS = (
        ('snowleopard', 'x86'),
        ('lion', 'x86'),
        ('mountainlion', 'x86'),
        ('xp', 'x86'),
        ('win7', 'x86'),
        ('lucid', 'x86'),
        ('lucid', 'x86_64'),
        # FIXME: Technically this should be 'arm', but adding a third architecture type breaks TestConfigurationConverter.
        # If we need this to be 'arm' in the future, then we first have to fix TestConfigurationConverter.
        ('icecreamsandwich', 'x86'))

    ALL_BASELINE_VARIANTS = [
        'chromium-mac-mountainlion', 'chromium-mac-lion', 'chromium-mac-snowleopard',
        'chromium-win-win7', 'chromium-win-xp',
        'chromium-linux-x86_64', 'chromium-linux-x86',
    ]

    CONFIGURATION_SPECIFIER_MACROS = {
        'mac': ['snowleopard', 'lion', 'mountainlion'],
        'win': ['xp', 'win7'],
        'linux': ['lucid'],
        'android': ['icecreamsandwich'],
    }

    DEFAULT_BUILD_DIRECTORIES = ('out',)

    # overridden in subclasses.
    FALLBACK_PATHS = {}

    @classmethod
    def _static_build_path(cls, filesystem, build_directory, chromium_base, webkit_base, configuration, comps):
        if build_directory:
            return filesystem.join(build_directory, configuration, *comps)

        hits = []
        for directory in cls.DEFAULT_BUILD_DIRECTORIES:
            base_dir = filesystem.join(chromium_base, directory, configuration)
            path = filesystem.join(base_dir, *comps)
            if filesystem.exists(path):
                hits.append((filesystem.mtime(path), path))
        if hits:
            hits.sort(reverse=True)
            return hits[0][1]  # Return the newest file found.

        for directory in cls.DEFAULT_BUILD_DIRECTORIES:
            base_dir = filesystem.join(webkit_base, directory, configuration)
            path = filesystem.join(base_dir, *comps)
            if filesystem.exists(path):
                hits.append((filesystem.mtime(path), path))

        if hits:
            hits.sort(reverse=True)
            return hits[0][1]  # Return the newest file found.

        # We have to default to something, so pick the last one.
        return filesystem.join(base_dir, *comps)

    @classmethod
    def _chromium_base_dir(cls, filesystem):
        module_path = filesystem.path_to_module(cls.__module__)
        offset = module_path.find('third_party')
        if offset == -1:
            return filesystem.join(module_path[0:module_path.find('Tools')], 'Source', 'WebKit', 'chromium')
        else:
            return module_path[0:offset]

    def __init__(self, host, port_name, **kwargs):
        super(ChromiumPort, self).__init__(host, port_name, **kwargs)
        # All sub-classes override this, but we need an initial value for testing.
        self._chromium_base_dir_path = None

    def is_chromium(self):
        return True

    def default_max_locked_shards(self):
        """Return the number of "locked" shards to run in parallel (like the http tests)."""
        max_locked_shards = int(self.default_child_processes()) / 4
        if not max_locked_shards:
            return 1
        return max_locked_shards

    def default_pixel_tests(self):
        return True

    def default_baseline_search_path(self):
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS[self.version()])

    def default_timeout_ms(self):
        if self.get_option('configuration') == 'Debug':
            return 12 * 1000
        return 6 * 1000

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

        # It's okay if pretty patch and wdiff aren't available, but we will at least log messages.
        self._pretty_patch_available = self.check_pretty_patch()
        self._wdiff_available = self.check_wdiff()

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
        # tolerance is not used in chromium. Make sure caller doesn't pass tolerance other than zero or None.
        assert (tolerance is None) or tolerance == 0

        # If only one of them exists, return that one.
        if not actual_contents and not expected_contents:
            return (None, 0, None)
        if not actual_contents:
            return (expected_contents, 0, None)
        if not expected_contents:
            return (actual_contents, 0, None)

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
        # Note that although we are handed 'old', 'new', image_diff wants 'new', 'old'.
        comand = [executable, '--diff', native_actual_filename, native_expected_filename, native_diff_filename]

        result = None
        err_str = None
        try:
            exit_code = self._executive.run_command(comand, return_exit_code=True)
            if exit_code == 0:
                # The images are the same.
                result = None
            elif exit_code == 1:
                result = self._filesystem.read_binary_file(native_diff_filename)
            else:
                err_str = "image diff returned an exit code of %s" % exit_code
        except OSError, e:
            err_str = 'error running image diff: %s' % str(e)
        finally:
            self._filesystem.rmtree(str(tempdir))

        return (result, 0, err_str or None)  # FIXME: how to get % diff?

    def path_from_chromium_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        Chromium source tree and the list of path components in |*comps|."""
        if self._chromium_base_dir_path is None:
            self._chromium_base_dir_path = self._chromium_base_dir(self._filesystem)
        return self._filesystem.join(self._chromium_base_dir_path, *comps)

    def setup_environ_for_server(self, server_name=None):
        clean_env = super(ChromiumPort, self).setup_environ_for_server(server_name)
        # Webkit Linux (valgrind layout) bot needs these envvars.
        self._copy_value_from_environ_if_set(clean_env, 'VALGRIND_LIB')
        self._copy_value_from_environ_if_set(clean_env, 'VALGRIND_LIB_INNER')
        return clean_env

    def default_results_directory(self):
        try:
            return self.path_from_chromium_base('webkit', self.get_option('configuration'), 'layout-test-results')
        except AssertionError:
            return self._build_path('layout-test-results')

    def _missing_symbol_to_skipped_tests(self):
        # FIXME: Should WebKitPort have these definitions also?
        return {
            "ff_mp3_decoder": ["webaudio/codec-tests/mp3"],
            "ff_aac_decoder": ["webaudio/codec-tests/aac"],
        }

    def setup_test_run(self):
        super(ChromiumPort, self).setup_test_run()
        # Delete the disk cache if any to ensure a clean test run.
        dump_render_tree_binary_path = self._path_to_driver()
        cachedir = self._filesystem.dirname(dump_render_tree_binary_path)
        cachedir = self._filesystem.join(cachedir, "cache")
        if self._filesystem.exists(cachedir):
            self._filesystem.rmtree(cachedir)

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
            try:
                self._helper.stdin.write("x\n")
                self._helper.stdin.close()
                self._helper.wait()
            except IOError, e:
                pass
            finally:
                self._helper = None


    def exit_code_from_summarized_results(self, unexpected_results):
        # Turn bots red for missing results.
        return unexpected_results['num_regressions'] + unexpected_results['num_missing']

    def configuration_specifier_macros(self):
        return self.CONFIGURATION_SPECIFIER_MACROS

    def all_baseline_variants(self):
        return self.ALL_BASELINE_VARIANTS

    def _generate_all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for version, architecture in self.ALL_SYSTEMS:
            for build_type in self.ALL_BUILD_TYPES:
                test_configurations.append(TestConfiguration(version, architecture, build_type))
        return test_configurations

    try_builder_names = frozenset([
        'linux_layout',
        'mac_layout',
        'win_layout',
        'linux_layout_rel',
        'mac_layout_rel',
        'win_layout_rel',
    ])

    def warn_if_bug_missing_in_test_expectations(self):
        return True

    def expectations_files(self):
        paths = [self.path_to_test_expectations_file()]
        skia_expectations_path = self.path_from_chromium_base('skia', 'skia_test_expectations.txt')
        # FIXME: we should probably warn if this file is missing in some situations.
        # See the discussion in webkit.org/b/97699.
        if self._filesystem.exists(skia_expectations_path):
            paths.append(skia_expectations_path)

        builder_name = self.get_option('builder_name', 'DUMMY_BUILDER_NAME')
        if builder_name == 'DUMMY_BUILDER_NAME' or '(deps)' in builder_name or builder_name in self.try_builder_names:
            paths.append(self.path_from_chromium_base('webkit', 'tools', 'layout_tests', 'test_expectations.txt'))
        return paths

    def repository_paths(self):
        repos = super(ChromiumPort, self).repository_paths()
        repos.append(('chromium', self.path_from_chromium_base('build')))
        return repos

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than):
        if stderr and 'AddressSanitizer' in stderr:
            asan_filter_path = self.path_from_chromium_base('tools', 'valgrind', 'asan', 'asan_symbolize.py')
            if self._filesystem.exists(asan_filter_path):
                output = self._executive.run_command([asan_filter_path], input=stderr, decode_output=False)
                stderr = self._executive.run_command(['c++filt'], input=output, decode_output=False)

        return super(ChromiumPort, self)._get_crash_log(name, pid, stdout, stderr, newer_than)

    def virtual_test_suites(self):
        return [
            VirtualTestSuite('platform/chromium/virtual/gpu/fast/canvas',
                             'fast/canvas',
                             ['--enable-accelerated-2d-canvas']),
            VirtualTestSuite('platform/chromium/virtual/gpu/canvas/philip',
                             'canvas/philip',
                             ['--enable-accelerated-2d-canvas']),
            VirtualTestSuite('platform/chromium/virtual/threaded/compositing/visibility',
                             'compositing/visibility',
                             ['--enable-threaded-compositing']),
            VirtualTestSuite('platform/chromium/virtual/threaded/compositing/webgl',
                             'compositing/webgl',
                             ['--enable-threaded-compositing']),
            VirtualTestSuite('platform/chromium/virtual/gpu/fast/hidpi',
                             'fast/hidpi',
                             ['--force-compositing-mode']),
            VirtualTestSuite('platform/chromium/virtual/softwarecompositing',
                             'compositing',
                             ['--enable-software-compositing']),
            VirtualTestSuite('platform/chromium/virtual/deferred/fast/images',
                             'fast/images',
                             ['--enable-deferred-image-decoding', '--enable-per-tile-painting', '--force-compositing-mode']),
            VirtualTestSuite('platform/chromium/virtual/gpu/compositedscrolling/overflow',
                             'compositing/overflow',
                             ['--enable-accelerated-overflow-scroll']),
            VirtualTestSuite('platform/chromium/virtual/gpu/compositedscrolling/scrollbars',
                             'scrollbars',
                             ['--enable-accelerated-overflow-scroll']),
        ]

    #
    # PROTECTED METHODS
    #
    # These routines should only be called by other methods in this file
    # or any subclasses.
    #

    def _build_path(self, *comps):
        return self._build_path_with_configuration(None, *comps)

    def _build_path_with_configuration(self, configuration, *comps):
        # Note that we don't implement --root or do the option caching that the
        # base class does, because chromium doesn't use 'webkit-build-directory' and
        # hence finding the right directory is relatively fast.
        configuration = configuration or self.get_option('configuration')
        return self._static_build_path(self._filesystem, self.get_option('build_directory'),
            self.path_from_chromium_base(), self.path_from_webkit_base(), configuration, comps)

    def _path_to_image_diff(self):
        binary_name = 'ImageDiff'
        return self._build_path(binary_name)

    def _check_driver_build_up_to_date(self, configuration):
        if configuration in ('Debug', 'Release'):
            try:
                debug_path = self._path_to_driver('Debug')
                release_path = self._path_to_driver('Release')

                debug_mtime = self._filesystem.mtime(debug_path)
                release_mtime = self._filesystem.mtime(release_path)

                if (debug_mtime > release_mtime and configuration == 'Release' or
                    release_mtime > debug_mtime and configuration == 'Debug'):
                    most_recent_binary = 'Release' if configuration == 'Debug' else 'Debug'
                    _log.warning('You are running the %s binary. However the %s binary appears to be more recent. '
                                 'Please pass --%s.', configuration, most_recent_binary, most_recent_binary.lower())
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
