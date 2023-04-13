# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2012-2022 Apple Inc. All rights reserved.
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

import logging
import os
import re

from webkitcorepy import Version

from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.version_name_map import PUBLIC_TABLE, INTERNAL_TABLE
from webkitpy.common.version_name_map import VersionNameMap
from webkitpy.port.base import Port
from webkitpy.port.config import apple_additions, Config
from webkitpy.port.darwin import DarwinPort
from webkitpy.port.driver import DriverInput

_log = logging.getLogger(__name__)


class MacPort(DarwinPort):
    port_name = "mac"

    CURRENT_VERSION = Version(13, 0)
    LAST_MACOSX = Version(10, 15)

    SDK = 'macosx'

    ARCHITECTURES = ['x86_64', 'x86', 'arm64']

    DEFAULT_ARCHITECTURE = 'x86_64'

    def __init__(self, host, port_name, **kwargs):
        DarwinPort.__init__(self, host, port_name, **kwargs)
        version_name_map = VersionNameMap.map(host.platform)
        self._os_version = None
        split_port_name = port_name.split('-')
        if len(split_port_name) > 1 and split_port_name[1] != 'wk2':
            self._os_version = version_name_map.from_name(split_port_name[1])[1]
        elif self.host.platform.is_mac() and apple_additions():
            self._os_version = self.host.platform.os_version
        if not self._os_version:
            self._os_version = MacPort.CURRENT_VERSION

    def architecture(self):
        result = self.get_option('architecture') or self.host.platform.architecture()
        if result == 'arm64e':
            return 'arm64'
        return result

    # FIXME: This is a work-around for Rosetta, remove once <https://bugs.webkit.org/show_bug.cgi?id=213761> is resolved
    def expectations_dict(self, device_type=None):
        result = super(MacPort, self).expectations_dict(device_type=device_type)
        if self.architecture() == 'x86_64' and self.host.platform.architecture() == 'arm64':
            rosetta_expectations = self._filesystem.join(self.layout_tests_dir(), 'platform', 'mac', 'TestExpectationsRosetta')
            if self._filesystem.exists(rosetta_expectations):
                result[rosetta_expectations] = self._filesystem.read_text_file(rosetta_expectations)
            else:
                _log.warning('Failed to find Rosetta special-case expectation path at {}'.format(rosetta_expectations))
        return result


    def _build_driver_flags(self):
        architecture = self.architecture()
        # The Internal SDK should always prefer arm64e binaries to arm64 ones
        if architecture == 'arm64' and apple_additions():
            architecture = 'arm64e'
        if architecture == 'x86':
            return ['ARCHS=i386']
        return ['ARCHS={}'.format(architecture)]

    def default_baseline_search_path(self, **kwargs):
        versions_to_fallback = []
        version_name_map = VersionNameMap.map(self.host.platform)

        if self._os_version == self.CURRENT_VERSION:
            versions_to_fallback = [self.CURRENT_VERSION]
        else:
            temp_version = Version(self._os_version.major, self._os_version.minor)
            while temp_version != self.CURRENT_VERSION:
                versions_to_fallback.append(Version.from_iterable(temp_version))
                if temp_version < self.CURRENT_VERSION:
                    if temp_version.major == self.LAST_MACOSX.major:
                        if temp_version.minor < self.LAST_MACOSX.minor:
                            temp_version.minor += 1
                        else:
                            temp_version = Version(11, 0)
                    else:
                        temp_version = Version(temp_version.major + 1)
                else:
                    temp_version = Version(temp_version.major - 1)

        wk_string = 'wk1'
        if self.get_option('webkit_test_runner'):
            wk_string = 'wk2'

        expectations = []
        for version in versions_to_fallback:
            version_name = version_name_map.to_name(version, platform=self.port_name)
            if version_name:
                standardized_version_name = version_name.lower().replace(' ', '')
            apple_name = None
            if apple_additions():
                apple_name = version_name_map.to_name(version, platform=self.port_name, table=INTERNAL_TABLE)

            if apple_name:
                expectations.append(self._apple_baseline_path('mac-{}-{}'.format(apple_name.lower().replace(' ', ''), wk_string)))
            if version_name:
                expectations.append(self._webkit_baseline_path('mac-{}-{}'.format(standardized_version_name, wk_string)))
            if apple_name:
                expectations.append(self._apple_baseline_path('mac-{}'.format(apple_name.lower().replace(' ', ''))))
            if version_name:
                expectations.append(self._webkit_baseline_path('mac-{}'.format(standardized_version_name)))

        if apple_additions():
            expectations.append(self._apple_baseline_path('{}-{}'.format(self.port_name, wk_string)))
        expectations.append(self._webkit_baseline_path('{}-{}'.format(self.port_name, wk_string)))
        if apple_additions():
            expectations.append(self._apple_baseline_path('{}'.format(self.port_name)))
        expectations.append(self._webkit_baseline_path(self.port_name))

        if self.get_option('webkit_test_runner'):
            expectations.append(self._webkit_baseline_path('wk2'))
        return expectations

    @memoized
    def configuration_specifier_macros(self):
        config_map = {}
        version_name_map = VersionNameMap.map(self.host.platform)
        for version in self._allowed_versions():
            version_names = []
            for newer in self._allowed_versions()[self._allowed_versions().index(version):]:
                version_name = version_name_map.to_name(newer, platform=self.port_name)
                if not version_name:
                    version_name = version_name_map.to_name(newer, platform=self.port_name, table=INTERNAL_TABLE)
                version_names.append(version_name.lower().replace(' ', ''))
            for table in [PUBLIC_TABLE, INTERNAL_TABLE]:
                version_name = version_name_map.to_name(version, platform=self.port_name, table=table)
                if not version_name:
                    continue
                config_map[version_name.lower().replace(' ', '') + '+'] = version_names
        return config_map

    def environment_for_api_tests(self):
        result = super(MacPort, self).environment_for_api_tests()
        if self.get_option('guard_malloc'):
            result['DYLD_INSERT_LIBRARIES'] = '/usr/lib/libgmalloc.dylib'
            result['__XPC_DYLD_INSERT_LIBRARIES'] = '/usr/lib/libgmalloc.dylib'
        return result

    def setup_environ_for_server(self, server_name=None):
        env = super(MacPort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name():
            if self.get_option('leaks'):
                env['MallocStackLogging'] = '1'
                env['__XPC_MallocStackLogging'] = '1'
                env['MallocScribble'] = '1'
                env['__XPC_MallocScribble'] = '1'
            if self.get_option('guard_malloc'):
                self._append_value_colon_separated(env, 'DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
                self._append_value_colon_separated(env, '__XPC_DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
        env['XML_CATALOG_FILES'] = ''  # work around missing /etc/catalog <rdar://problem/4292995>
        return env

    def port_adjust_environment_for_test_driver(self, env):
        env = super(MacPort, self).port_adjust_environment_for_test_driver(env)
        env['CA_DISABLE_GENERIC_SHADERS'] = '1'
        env['__XPC_CA_DISABLE_GENERIC_SHADERS'] = '1'
        return env

    def _clear_global_caches_and_temporary_files(self):
        self._filesystem.rmtree(os.path.expanduser('~/Library/' + self.driver_name()))
        self._filesystem.rmtree(os.path.expanduser('~/Library/Application Support/' + self.driver_name()))
        self._filesystem.rmtree(os.path.expanduser('~/Library/Caches/' + self.driver_name()))
        self._filesystem.rmtree(os.path.expanduser('~/Library/WebKit/' + self.driver_name()))

    def _path_to_user_cache_directory(self, suffix=None):
        DIRHELPER_USER_DIR_SUFFIX = 'DIRHELPER_USER_DIR_SUFFIX'
        CS_DARWIN_USER_CACHE_DIR = 65538

        # The environment variable DIRHELPER_USER_DIR_SUFFIX is only honored on systems with
        # System Integrity Protection disabled or with an Apple-Internal OS. To make this code
        # work for all system configurations we compute the path with respect to the suffix
        # by hand and temporarily unset the environment variable DIRHELPER_USER_DIR_SUFFIX (if set)
        # to avoid it influencing confstr() on systems that honor DIRHELPER_USER_DIR_SUFFIX.
        saved_suffix = None
        if DIRHELPER_USER_DIR_SUFFIX in os.environ:
            saved_suffix = os.environ[DIRHELPER_USER_DIR_SUFFIX]
            del os.environ[DIRHELPER_USER_DIR_SUFFIX]
        result = os.path.join(os.confstr(CS_DARWIN_USER_CACHE_DIR), suffix or '')
        if saved_suffix is not None:
            os.environ[DIRHELPER_USER_DIR_SUFFIX] = saved_suffix
        return result

    def operating_system(self):
        return 'mac'

    def default_child_processes(self, **kwargs):
        default_count = super(MacPort, self).default_child_processes()

        # FIXME: arm64 Mac hardware can handle more processes than the default number we calculate.
        # Double the amount of default workers until we implement more sophisticated test scheduling.
        if self.architecture() == 'arm64':
            default_count = default_count * 2

        # FIXME: https://bugs.webkit.org/show_bug.cgi?id=95906  With too many WebProcess WK2 tests get stuck in resource contention.
        # To alleviate the issue reduce the number of running processes
        # Anecdotal evidence suggests that a 4 core/8 core logical machine may run into this, but that a 2 core/4 core logical machine does not.
        should_throttle_for_wk2 = self.get_option('webkit_test_runner') and default_count > 4
        # We also want to throttle for leaks bots.
        if should_throttle_for_wk2 or self.get_option('leaks'):
            default_count = int(.75 * default_count)

        if should_throttle_for_wk2 and self.get_option('guard_malloc'):
            # Some 12 core Macs get a lot of tests time out when running 18 WebKitTestRunner processes (it's not clear what this depends on).
            # <rdar://problem/25750302>
            default_count = min(default_count, 12)

        # Make sure we have enough ram to support that many instances:
        total_memory = self.host.platform.total_bytes_memory()
        if total_memory:
            bytes_per_drt = 256 * 1024 * 1024  # Assume each DRT needs 256MB to run.
            overhead = 2048 * 1024 * 1024  # Assume we need 2GB free for the O/S
            supportable_instances = max((total_memory - overhead) / bytes_per_drt, 1)  # Always use one process, even if we don't have space for it.
            if supportable_instances < default_count:
                _log.warning("This machine could support %s child processes, but only has enough memory for %s." % (default_count, supportable_instances))
        else:
            _log.warning("Cannot determine available memory for child processes, using default child process count of %s." % default_count)
            supportable_instances = default_count
        return min(supportable_instances, default_count)

    def start_helper(self, pixel_tests=False, prefer_integrated_gpu=False):
        self.stop_helper()

        helper_path = self._path_to_helper()
        if not helper_path:
            _log.error("No path to LayoutTestHelper binary")
            return False
        _log.debug("Starting layout helper %s" % helper_path)
        arguments = [helper_path, '--install-color-profile']
        if prefer_integrated_gpu:
            arguments.append('--prefer-integrated-gpu')
        Port.helper = self._executive.popen(arguments,
            stdin=self._executive.PIPE, stdout=self._executive.PIPE, stderr=None)
        is_ready = Port.helper.stdout.readline()
        if not is_ready.startswith(b'ready'):
            _log.error("LayoutTestHelper could not start")
            return False
        return True

    def reset_preferences(self):
        _log.debug("Resetting persistent preferences")

        for domain in ["com.apple.WebKit.DumpRenderTree", "com.apple.WebKit.WebKitTestRunner"]:
            try:
                self._executive.run_command(["defaults", "delete", domain])
            except ScriptError as e:
                # 'defaults' returns 1 if the domain did not exist
                if e.exit_code != 1:
                    raise e

    def logging_patterns_to_strip(self):
        logging_patterns = super(MacPort, self).logging_patterns_to_strip()

        # FIXME: Remove this after <rdar://problem/35954459> is fixed.
        logging_patterns.append(('AVDCreateGPUAccelerator: Error loading GPU renderer\n', ''))

        # FIXME: Remove this after <rdar://problem/51191120> is fixed.
        logging_patterns.append((re.compile('GVA warning: getFreeDRMInstanceCount, maxDRMInstanceCount: .*\n'), ''))

        # FIXME: Remove this after <rdar://problem/52897406> is fixed.
        logging_patterns.append((re.compile('VPA info:.*\n'), ''))

        # FIXME: Find where this is coming from and file a bug to have it removed (then remove this line).
        logging_patterns.append((re.compile('VP9 Info:.*\n'), ''))

        # FIXME: Find where this is coming from and file a bug to have it removed (then remove this line).
        logging_patterns.append((re.compile('set AppID to 1\n'), ''))

        return logging_patterns

    def logging_detectors_to_strip_text_start(self, test_name):
        logging_detectors = []

        if 'webrtc' in test_name and self._os_version.major == 11:
            logging_detectors.append('')

        return logging_detectors

    def stderr_patterns_to_strip(self):
        worthless_patterns = super(MacPort, self).stderr_patterns_to_strip()
        worthless_patterns.append((re.compile('.*(Fig|fig|itemasync|vt|mv_|PullParamSetSPS|ccrp_|client).* signalled err=.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< FigFilePlayer >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< FigFile >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< FAQ >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< MediaValidator >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< VMC >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<< FFR_Common >>>.*\n'), ''))
        return worthless_patterns

    def configuration_for_upload(self, host=None):
        host = host or self.host
        configuration = super(MacPort, self).configuration_for_upload(host=host)

        # --model should override the detected model
        if not configuration.get('model'):
            output = host.executive.run_command(['/usr/sbin/sysctl', 'hw.model']).rstrip()
            match = re.match(r'hw.model: (?P<model>.*)', output)
            if match:
                configuration['model'] = match.group('model')

        return configuration

    def setup_test_run(self, device_type=None):
        super(MacPort, self).setup_test_run(device_type)
        # Warm-up can be disabled with `--no-timeout`. This is useful when trying to avoid debugger
        # attaching to the warmup process when debugging with `lldb --wait-for --attach-name ...`.
        if not self.get_option("no_timeout"):
            _log.debug('Warming up the runner ...')
            warmup_driver = self.create_driver(0)
            warmup_driver.run_test(DriverInput('file:///warmup-does-not-exist', 60000., None, should_run_pixel_test=False), stop_when_done=True)


class MacCatalystPort(MacPort):
    port_name = "maccatalyst"

    def __init__(self, *args, **kwargs):
        super(MacCatalystPort, self).__init__(*args, **kwargs)
        self._config = Config(self._executive, self._filesystem, MacCatalystPort.port_name)

    def _build_driver_flags(self):
        return ['SDK_VARIANT=iosmac'] + super(MacCatalystPort, self)._build_driver_flags()

    def configuration_for_upload(self, host=None):
        configuration = super(MacCatalystPort, self).configuration_for_upload(host=host)
        configuration['platform'] = self.port_name
        return configuration
