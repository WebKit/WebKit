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

import logging

from webkitpy.layout_tests.port import chromium
from webkitpy.layout_tests.port import config


_log = logging.getLogger(__name__)


class ChromiumLinuxPort(chromium.ChromiumPort):
    port_name = 'chromium-linux'

    SUPPORTED_ARCHITECTURES = ('x86', 'x86_64')

    FALLBACK_PATHS = {
        'x86_64': [
            'chromium-linux',
            'chromium-win',
            'chromium',
            'win',
            'mac',
        ],
        'x86': [
            'chromium-linux-x86',
            'chromium-linux',
            'chromium-win',
            'chromium',
            'win',
            'mac',
        ],
    }

    @classmethod
    def _determine_driver_path_statically(cls, host, options):
        config_object = config.Config(host.executive, host.filesystem)
        build_directory = getattr(options, 'build_directory', None)
        webkit_base = config_object.path_from_webkit_base()
        chromium_base = cls._chromium_base_dir(host.filesystem)
        if hasattr(options, 'configuration') and options.configuration:
            configuration = options.configuration
        else:
            configuration = config_object.default_configuration()
        return cls._static_build_path(host.filesystem, build_directory, chromium_base, webkit_base, configuration, 'DumpRenderTree')

    @staticmethod
    def _static_build_path(filesystem, build_directory, chromium_base, webkit_base, *comps):
        if build_directory:
            return filesystem.join(build_directory, *comps)
        if filesystem.exists(filesystem.join(chromium_base, 'sconsbuild')):
            return filesystem.join(chromium_base, 'sconsbuild', *comps)
        if filesystem.exists(filesystem.join(chromium_base, 'out')):
            return filesystem.join(chromium_base, 'out', *comps)
        if filesystem.exists(filesystem.join(webkit_base, 'sconsbuild')):
            return filesystem.join(webkit_base, 'sconsbuild', *comps)
        return filesystem.join(webkit_base, 'out', *comps)

    @staticmethod
    def _determine_architecture(filesystem, executive, driver_path):
        file_output = ''
        if filesystem.exists(driver_path):
            # The --dereference flag tells file to follow symlinks
            file_output = executive.run_command(['file', '--dereference', driver_path], return_stderr=True)

        if 'ELF 32-bit LSB executable' in file_output:
            return 'x86'
        if 'ELF 64-bit LSB executable' in file_output:
            return 'x86_64'
        if file_output:
            _log.warning('Could not determine architecture from "file" output: %s' % file_output)

        # We don't know what the architecture is; default to 'x86' because
        # maybe we're rebaselining and the binary doesn't actually exist,
        # or something else weird is going on. It's okay to do this because
        # if we actually try to use the binary, check_build() should fail.
        return 'x86_64'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('-linux'):
            return port_name + '-' + cls._determine_architecture(host.filesystem, host.executive, cls._determine_driver_path_statically(host, options))
        return port_name

    def __init__(self, host, port_name, **kwargs):
        chromium.ChromiumPort.__init__(self, host, port_name, **kwargs)
        (base, arch) = port_name.rsplit('-', 1)
        assert base in ('chromium-linux', 'chromium-gpu-linux')
        assert arch in self.SUPPORTED_ARCHITECTURES
        assert port_name in ('chromium-linux', 'chromium-gpu-linux',
                             'chromium-linux-x86', 'chromium-linux-x86_64',
                             'chromium-gpu-linux-x86_64')
        self._version = 'lucid'  # We only support lucid right now.
        self._architecture = arch

    def baseline_search_path(self):
        port_names = self.FALLBACK_PATHS[self._architecture]
        return map(self._webkit_baseline_path, port_names)

    def check_build(self, needs_http):
        result = chromium.ChromiumPort.check_build(self, needs_http)
        result = self.check_wdiff() and result

        if not result:
            _log.error('For complete Linux build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/LinuxBuildInstructions')
        return result

    def operating_system(self):
        return 'linux'

    #
    # PROTECTED METHODS
    #

    def _build_path(self, *comps):
        return self._static_build_path(self._filesystem, self.get_option('build_directory'), self.path_from_chromium_base(), self.path_from_webkit_base(), *comps)

    def _check_apache_install(self):
        result = self._check_file_exists(self._path_to_apache(), "apache2")
        result = self._check_file_exists(self._path_to_apache_config_file(), "apache2 config file") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install apache2 libapache2-mod-php5"')
            _log.error('')
        return result

    def _check_lighttpd_install(self):
        result = self._check_file_exists(
            self._path_to_lighttpd(), "LigHTTPd executable")
        result = self._check_file_exists(self._path_to_lighttpd_php(), "PHP CGI executable") and result
        result = self._check_file_exists(self._path_to_lighttpd_modules(), "LigHTTPd modules") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install lighttpd php5-cgi"')
            _log.error('')
        return result

    def check_wdiff(self, logging=True):
        result = self._check_file_exists(self._path_to_wdiff(), 'wdiff')
        if not result and logging:
            _log.error('    Please install using: "sudo apt-get install wdiff"')
            _log.error('')
        # FIXME: The ChromiumMac port always returns True.
        return result

    def _path_to_apache(self):
        if self._is_redhat_based():
            return '/usr/sbin/httpd'
        else:
            return '/usr/sbin/apache2'

    def _path_to_apache_config_file(self):
        if self._is_redhat_based():
            config_name = 'fedora-httpd.conf'
        else:
            config_name = 'apache2-debian-httpd.conf'

        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf', config_name)

    def _path_to_lighttpd(self):
        return "/usr/sbin/lighttpd"

    def _path_to_lighttpd_modules(self):
        return "/usr/lib/lighttpd"

    def _path_to_lighttpd_php(self):
        return "/usr/bin/php-cgi"

    def _path_to_driver(self, configuration=None):
        if not configuration:
            configuration = self.get_option('configuration')
        binary_name = 'DumpRenderTree'
        return self._build_path(configuration, binary_name)

    def _path_to_helper(self):
        return None

    def _path_to_wdiff(self):
        if self._is_redhat_based():
            return '/usr/bin/dwdiff'
        else:
            return '/usr/bin/wdiff'

    def _is_redhat_based(self):
        return self._filesystem.exists(self._filesystem.join('/etc', 'redhat-release'))
