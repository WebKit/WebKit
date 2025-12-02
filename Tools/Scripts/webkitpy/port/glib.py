# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2013 Samsung Electronics.  All rights reserved.
# Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
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

import os
import uuid

from webkitpy.port.base import Port
from webkitpy.port.leakdetector_valgrind import LeakDetectorValgrind
from webkitpy.port.linux_get_crash_log import GDBCrashLogGenerator


class GLibPort(Port):

    def __init__(self, *args, **kwargs):
        super(GLibPort, self).__init__(*args, **kwargs)
        self._display_server = self.get_option("display_server")

        if self.get_option("leaks"):
            self._leakdetector = LeakDetectorValgrind(self._executive, self._filesystem, self.results_directory())
            if not self.get_option("wrapper"):
                raise ValueError('use --wrapper=\"valgrind\" for memory leak detection')

        if self._should_use_jhbuild():
            self._jhbuild_wrapper = [self.path_from_webkit_base('Tools', 'jhbuild', 'jhbuild-wrapper'), self._port_flag_for_scripts(), 'run']
            if self.get_option('wrapper'):
                self.set_option('wrapper', ' '.join(self._jhbuild_wrapper) + ' ' + self.get_option('wrapper'))
            else:
                self.set_option_default('wrapper', ' '.join(self._jhbuild_wrapper))

    def default_timeout_ms(self):
        default_timeout = super().default_timeout_ms()
        # Starting an application under Valgrind takes a lot longer than normal
        # so increase the timeout (empirically 10x is enough to avoid timeouts).
        multiplier = 10 if self.get_option("leaks") else 1
        # Debug builds are slower (no compiler optimizations are used).
        if self.get_option('configuration') == 'Debug':
            multiplier *= 2
        return multiplier * default_timeout

    def _built_executables_path(self, *path):
        return self._build_path(*(('bin',) + path))

    def _prepend_to_env_value(self, new_value, current_value):
        if len(current_value) > 0:
            return new_value + ":" + current_value
        return new_value

    def setup_test_run(self, device_type=None):
        super(GLibPort, self).setup_test_run(device_type)

        if self.get_option("leaks"):
            self._leakdetector.clean_leaks_files_from_results_directory()

    def setup_environ_for_server(self, server_name=None):
        environment = super(GLibPort, self).setup_environ_for_server(server_name)
        self._copy_value_from_environ_if_set(environment, 'G_DEBUG')
        if 'G_DEBUG' not in environment.keys():
            environment['G_DEBUG'] = 'fatal-criticals'
        environment['GSETTINGS_BACKEND'] = 'memory'

        environment['TEST_RUNNER_INJECTED_BUNDLE_FILENAME'] = self._build_path('lib', 'libTestRunnerInjectedBundle.so')
        environment['WEBKIT_EXEC_PATH'] = self._build_path('bin')
        environment['WEBKIT_INSPECTOR_RESOURCES_PATH'] = self._build_path('share')
        environment['WEBKIT_TOP_LEVEL'] = self.path_from_webkit_base()
        environment['LD_LIBRARY_PATH'] = self._prepend_to_env_value(self._build_path('lib'), environment.get('LD_LIBRARY_PATH', ''))
        self._copy_value_from_environ_if_set(environment, 'LIBGL_ALWAYS_SOFTWARE')
        self._copy_value_from_environ_if_set(environment, 'AT_SPI_BUS_ADDRESS')

        # Copy all GStreamer related env vars
        self._copy_values_from_environ_with_prefix(environment, 'GST_')
        self._copy_value_from_environ_if_set(environment, 'WEBKIT_GST_DISABLE_WEBRTC_NETWORK_SANDBOX')

        gst_feature_rank_override = os.environ.get('GST_PLUGIN_FEATURE_RANK')
        # Disable hardware-accelerated device providers, encoders and decoders. Depending on the underlying platform
        # they might be selected and decrease tests reproducibility. They can still be re-enabled by
        # setting the GST_PLUGIN_FEATURE_RANK variable accordingly when calling run-webkit-tests.
        disabled_device_providers = ['alsa', 'decklink', 'oss', 'pipewire', 'pulse', 'v4l2', 'vulkan']
        downranked_elements = ['vah264dec', 'vah264enc', 'vah265dec', 'vah265enc', 'vaav1dec', 'vaav1enc', 'vajpegdec', 'vavp9dec', 'vavp8dec'] + [f'{provider}deviceprovider' for provider in disabled_device_providers]
        environment['GST_PLUGIN_FEATURE_RANK'] = 'fakeaudiosink:max,' + ','.join(['%s:0' % element for element in downranked_elements])
        if gst_feature_rank_override:
            environment['GST_PLUGIN_FEATURE_RANK'] += ',%s' % gst_feature_rank_override

        # Make sure GStreamer errors are logged to test -stderr files.
        gst_debug_override = os.environ.get('GST_DEBUG')
        environment['GST_DEBUG'] = '*:ERROR'
        if gst_debug_override:
            environment['GST_DEBUG'] += f',{gst_debug_override}'
        else:
            # If there is no user-supplied GST_DEBUG we can assume this runtime is some test bot, so
            # disable color output, making -stderr files more human-readable.
            environment['GST_DEBUG_NO_COLOR'] = '1'

        environment['WEBKIT_GST_ALLOW_PLAYBACK_OF_INVISIBLE_VIDEOS'] = '1'
        environment['WEBKIT_GST_WEBRTC_FORCE_EARLY_VIDEO_DECODING'] = '1'

        # Match our WebRTC stats cache expiration time with LibWebRTC, since some tests actually expect this.
        environment['WEBKIT_GST_WEBRTC_STATS_CACHE_EXPIRATION_TIME_MS'] = '50'

        # Fake a sound card with 2 output channels. Apparently we cannot assume test bots have an
        # actual sound card.
        environment['WEBKIT_GST_MAX_NUMBER_OF_AUDIO_OUTPUT_CHANNELS'] = '2'

        # Disable SIMD optimization in GStreamer's ORC. Some bots (WPE release) crash in ORC's optimizations.
        environment['ORC_CODE'] = 'backup'

        if self.get_option("leaks"):
            # Turn off GLib memory optimisations https://wiki.gnome.org/Valgrind.
            environment['G_SLICE'] = 'always-malloc'
            environment['G_DEBUG'] += ',gc-friendly'
            # Turn off bmalloc when running under Valgrind, see https://bugs.webkit.org/show_bug.cgi?id=177745
            environment['Malloc'] = '1'
            xmlfilename = "".join(("drt-%p-", uuid.uuid1().hex, "-leaks.xml"))
            xmlfile = os.path.join(self.results_directory(), xmlfilename)
            suppressionsfile = self.path_from_webkit_base('Tools', 'Scripts', 'valgrind', 'suppressions.txt')
            environment['VALGRIND_OPTS'] = \
                "--tool=memcheck " \
                "--num-callers=40 " \
                "--demangle=no " \
                "--trace-children=no " \
                "--smc-check=all-non-file " \
                "--leak-check=yes " \
                "--leak-resolution=high " \
                "--show-possibly-lost=no " \
                "--show-reachable=no " \
                "--leak-check=full " \
                "--undef-value-errors=no " \
                "--gen-suppressions=all " \
                "--xml=yes " \
                "--xml-file=%s " \
                "--suppressions=%s" % (xmlfile, suppressionsfile)

        # WTF_DateMath.calculateLocalTimeOffset test only pass in Pacific Time Zone
        environment['TZ'] = 'PST8PDT'

        return environment

    def setup_environ_for_minibrowser(self):
        env = os.environ.copy()
        env['WEBKIT_EXEC_PATH'] = self._build_path('bin')
        env['WEBKIT_INJECTED_BUNDLE_PATH'] = self._build_path('lib')
        env['WEBKIT_INSPECTOR_RESOURCES_PATH'] = self._build_path('share')
        env['WEBKIT_TOP_LEVEL'] = self.path_from_webkit_base()
        env['LD_LIBRARY_PATH'] = self._prepend_to_env_value(self._build_path('lib'), env.get('LD_LIBRARY_PATH', ''))
        return env

    def setup_sysprof_for_minibrowser(self):
        pass_fds = ()
        env = self.setup_environ_for_minibrowser()

        if os.environ.get("SYSPROF_CONTROL_FD"):
            try:
                control_fd = int(os.environ.get("SYSPROF_CONTROL_FD"))
                copy_fd = os.dup(control_fd)
                pass_fds += (copy_fd, )
                env["SYSPROF_CONTROL_FD"] = str(copy_fd)
            except (ValueError):
                pass

        return env, pass_fds

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, target_host=None):
        return GDBCrashLogGenerator(self._executive, name, pid, newer_than,
                                    self._filesystem, self._path_to_driver, self.port_name, self.get_option('configuration')).generate_crash_log(stdout, stderr)

    def setup_environ_for_webdriver(self):
        return self.setup_environ_for_minibrowser()

    def run_webdriver(self, args):
        env = self.setup_environ_for_webdriver()
        webDriver = self._built_executables_path(self.webdriver_name)
        if not (os.path.isfile(webDriver) and os.access(webDriver, os.X_OK)):
            raise RuntimeError(f'Unable to find an executable at path: {webDriver}')
        command = [webDriver]
        if self._should_use_jhbuild():
            command = self._jhbuild_wrapper + command
        return self._executive.run_command(command + args, cwd=self.webkit_base(), stdout=None, return_stderr=False, decode_output=False, env=env)

    API_TEST_BINARY_NAMES = ['TestWTF', 'TestJavaScriptCore', 'TestWebCore', 'TestWebKit']

    def path_to_api_test(self, program_name):
        return self._built_executables_path('TestWebKitAPI', program_name)

    def environment_for_api_tests(self):
        environment = super(GLibPort, self).environment_for_api_tests()
        environment['TEST_WEBKIT_API_WEBKIT2_RESOURCES_PATH'] = self.path_from_webkit_base('Tools', 'TestWebKitAPI', 'Tests', 'WebKit')
        environment['TEST_WEBKIT_API_WEBKIT2_INJECTED_BUNDLE_PATH'] = self._build_path('lib')
        return environment
