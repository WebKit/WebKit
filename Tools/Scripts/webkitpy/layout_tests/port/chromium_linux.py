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
import chromium


_log = logging.getLogger(__name__)


class ChromiumLinuxPort(chromium.ChromiumPort):
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

    def __init__(self, host, port_name=None, **kwargs):
        port_name = port_name or 'chromium-linux'
        chromium.ChromiumPort.__init__(self, host, port_name=port_name, **kwargs)
        # We re-set the port name once the base object is fully initialized
        # in order to be able to find the DRT binary properly.
        if port_name.endswith('-linux'):
            self._architecture = self._determine_architecture()
            # FIXME: This is an ugly hack to avoid renaming the GPU port.
            if port_name == 'chromium-linux':
                port_name = port_name + '-' + self._architecture
        else:
            base, arch = port_name.rsplit('-', 1)
            assert base in ('chromium-linux', 'chromium-gpu-linux')
            self._architecture = arch
        assert self._architecture in self.SUPPORTED_ARCHITECTURES
        assert port_name in ('chromium-linux', 'chromium-gpu-linux',
                             'chromium-linux-x86', 'chromium-linux-x86_64',
                             'chromium-gpu-linux-x86_64')
        self._name = port_name
        self._version = 'lucid'  # We only support lucid right now.

    def _determine_architecture(self):
        driver_path = self._path_to_driver()
        file_output = ''
        if self._filesystem.exists(driver_path):
            # The --dereference flag tells file to follow symlinks
            file_output = self._executive.run_command(['file', '--dereference', driver_path], return_stderr=True)

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
        if self.get_option('build_directory'):
            return self._filesystem.join(self.get_option('build_directory'), *comps)

        base = self.path_from_chromium_base()
        if self._filesystem.exists(self._filesystem.join(base, 'sconsbuild')):
            return self._filesystem.join(base, 'sconsbuild', *comps)
        if self._filesystem.exists(self._filesystem.join(base, 'out', *comps)):
            return self._filesystem.join(base, 'out', *comps)
        base = self.path_from_webkit_base()
        if self._filesystem.exists(self._filesystem.join(base, 'sconsbuild')):
            return self._filesystem.join(base, 'sconsbuild', *comps)
        return self._filesystem.join(base, 'out', *comps)

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
