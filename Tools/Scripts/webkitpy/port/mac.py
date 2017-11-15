# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2012, 2013, 2016 Apple Inc. All rights reserved.
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
import time
import re

from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.port.config import apple_additions
from webkitpy.port.darwin import DarwinPort

_log = logging.getLogger(__name__)


class MacPort(DarwinPort):
    port_name = "mac"

    VERSION_FALLBACK_ORDER = ['mac-snowleopard', 'mac-lion', 'mac-mountainlion', 'mac-mavericks', 'mac-yosemite', 'mac-elcapitan', 'mac-sierra', 'mac-highsierra']
    SDK = 'macosx'

    ARCHITECTURES = ['x86_64', 'x86']

    DEFAULT_ARCHITECTURE = 'x86_64'

    def __init__(self, host, port_name, **kwargs):
        DarwinPort.__init__(self, host, port_name, **kwargs)
        self._os_version = port_name.split('-')[1] if port_name.split('-') > 1 else self.host.platform.os_version

    def _build_driver_flags(self):
        return ['ARCHS=i386'] if self.architecture() == 'x86' else []

    def _apple_additions_path(self, name):
        if name == 'wk2':
            return None
        split_name = name.split('-')
        os_index = -1
        if split_name[-1] == 'wk1' or split_name[-1] == 'wk2':
            os_index = -2
        if split_name[os_index] != split_name[0]:
            os_name = apple_additions().mac_os_name(split_name[os_index])
            if not os_name:
                return None
            split_name[os_index] = os_name
        name = '-'.join(split_name)
        return self._filesystem.join(apple_additions().layout_tests_path(), name)

    @memoized
    def default_baseline_search_path(self):
        mac_version = 'mac-{}'.format(self._os_version)
        if mac_version.endswith(self.FUTURE_VERSION) or mac_version not in self.VERSION_FALLBACK_ORDER:
            version_fallback = [mac_version]
        else:
            version_fallback = self.VERSION_FALLBACK_ORDER[self.VERSION_FALLBACK_ORDER.index(mac_version):-1]
        wk_string = 'wk1'
        if self.get_option('webkit_test_runner'):
            wk_string = 'wk2'

        fallback_names = []
        for version in version_fallback:
            fallback_names.append('{}-{}'.format(version, wk_string))
            fallback_names.append(version)
        fallback_names = fallback_names + [
            '{}-{}'.format(self.port_name, wk_string),
            self.port_name,
        ]
        if self.get_option('webkit_test_runner'):
            fallback_names.append('wk2')

        webkit_expectations = map(self._webkit_baseline_path, fallback_names)
        if apple_additions() and getattr(apple_additions(), "layout_tests_path", None):
            apple_expectations = map(self._apple_additions_path, fallback_names)
            result = []
            for i in xrange(len(webkit_expectations)):
                if apple_expectations[i]:
                    result.append(apple_expectations[i])
                result.append(webkit_expectations[i])
            return result
        return webkit_expectations

    def configuration_specifier_macros(self):
        return {
            "highsierra+": ["highsierra", "future"],
            "sierra+": ["sierra", "highsierra", "future"],
            "elcapitan+": ["elcapitan", "sierra", "highsierra", "future"],
            "yosemite+": ["yosemite", "elcapitan", "sierra", "highsierra", "future"],
        }

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
            self._append_value_colon_separated(env, 'DYLD_INSERT_LIBRARIES', self._build_path("libWebCoreTestShim.dylib"))
        env['XML_CATALOG_FILES'] = ''  # work around missing /etc/catalog <rdar://problem/4292995>
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

    # Belongs on a Platform object.
    def is_mavericks(self):
        return self._version == 'mavericks'

    def default_child_processes(self):
        default_count = super(MacPort, self).default_child_processes()

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

    def _build_java_test_support(self):
        # FIXME: This is unused. Remove.
        java_tests_path = self._filesystem.join(self.layout_tests_dir(), "java")
        build_java = [self.make_command(), "-C", java_tests_path]
        if self._executive.run_command(build_java, return_exit_code=True):  # Paths are absolute, so we don't need to set a cwd.
            _log.error("Failed to build Java support files: %s" % build_java)
            return False
        return True

    def _check_port_build(self):
        return not self.get_option('java') or self._build_java_test_support()

    def start_helper(self, pixel_tests=False):
        helper_path = self._path_to_helper()
        if not helper_path:
            _log.error("No path to LayoutTestHelper binary")
            return False
        _log.debug("Starting layout helper %s" % helper_path)
        arguments = [helper_path, '--install-color-profile']
        self._helper = self._executive.popen(arguments,
            stdin=self._executive.PIPE, stdout=self._executive.PIPE, stderr=None)
        is_ready = self._helper.stdout.readline()
        if not is_ready.startswith('ready'):
            _log.error("LayoutTestHelper could not start")
            return False
        return True

    def reset_preferences(self):
        _log.debug("Resetting persistent preferences")

        for domain in ["DumpRenderTree", "WebKitTestRunner"]:
            try:
                self._executive.run_command(["defaults", "delete", domain])
            except ScriptError, e:
                # 'defaults' returns 1 if the domain did not exist
                if e.exit_code != 1:
                    raise e

    def stop_helper(self):
        if self._helper:
            _log.debug("Stopping LayoutTestHelper")
            try:
                self._helper.stdin.write("x\n")
                self._helper.stdin.close()
                self._helper.wait()
            except IOError, e:
                _log.debug("IOError raised while stopping helper: %s" % str(e))
            self._helper = None

    def logging_patterns_to_strip(self):
        # FIXME: Remove this after <rdar://problem/15605007> is fixed
        return [(re.compile('(AVF|GVA) info:.*\n'), '')]

    def stderr_patterns_to_strip(self):
        worthless_patterns = []
        worthless_patterns.append((re.compile('.*(Fig|fig|itemasync|vt|mv_|PullParamSetSPS|ccrp_|client).* signalled err=.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< FigFilePlayer >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< FigFile >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< FAQ >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< MediaValidator >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<<< VMC >>>>.*\n'), ''))
        worthless_patterns.append((re.compile('.*<<< FFR_Common >>>.*\n'), ''))
        return worthless_patterns
