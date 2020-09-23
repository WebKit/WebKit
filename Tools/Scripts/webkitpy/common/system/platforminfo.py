# Copyright (c) 2011 Google Inc. All rights reserved.
# Copyright (c) 2015-2017 Apple Inc. All rights reserved.
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

import os
import logging
import re
import sys
import platform

from webkitcorepy import Version

from webkitpy.common.memoized import memoized
from webkitpy.common.version_name_map import PUBLIC_TABLE, INTERNAL_TABLE, VersionNameMap
from webkitpy.common.system.executive import Executive, ScriptError


_log = logging.getLogger(__name__)


class PlatformInfo(object):
    MAX = 2147483647

    """This class provides a consistent (and mockable) interpretation of
    system-specific values (like sys.platform and platform.mac_ver())
    to be used by the rest of the webkitpy code base.

    Public (static) properties:
    -- os_name
    -- os_version

    Note that 'future' is returned for os_version if the operating system is
    newer than one known to the code.
    """

    def __init__(self, sys_module, platform_module, executive):
        self._executive = executive
        self._platform_module = platform_module
        self.os_name = self._determine_os_name(sys_module.platform)
        self.os_version = None

        self._is_cygwin = sys_module.platform == 'cygwin'

        if self.os_name.startswith('mac'):
            self.os_version = Version.from_string(self._executive.run_command(['sw_vers', '-productVersion']).rstrip())
        elif self.os_name.startswith('win'):
            self.os_version = self._win_version()
        else:
            # Most other platforms (namely iOS) return conforming version strings.
            version = re.search(r'\d+.\d+(.\d+)?', platform_module.release())
            if version:
                self.os_version = Version.from_string(version.group(0))
            else:
                _log.debug('No OS version number found')

    def is_mac(self):
        return self.os_name == 'mac'

    def is_ios(self):
        return self.os_name == 'ios'

    def is_watchos(self):
        return self.os_name == 'watchos'

    def is_win(self):
        return self.os_name == 'win'

    def is_native_win(self):
        return self.is_win() and not self.is_cygwin()

    def is_cygwin(self):
        return self._is_cygwin

    def is_linux(self):
        return self.os_name == 'linux'

    def is_freebsd(self):
        return self.os_name == 'freebsd'

    def is_openbsd(self):
        return self.os_name == 'openbsd'

    def is_netbsd(self):
        return self.os_name == 'netbsd'

    @memoized
    def architecture(self):
        try:
            # Windows doesn't have built in uname, nor guarantee of
            # os.uname()
            if os.name == 'nt':
                return platform.uname()[4]

            # os.uname() won't work on embedded devices, we may support multiple architectures for a single embedded platform
            output = self._executive.run_command(['uname', '-m']).rstrip()
            if output:
                if self.is_mac() and output != 'x86_64':
                    output = 'arm64'
                return output
        except ScriptError:
            pass
        return os.uname()[4]

    def display_name(self):
        # platform.platform() returns Darwin information for Mac, which is just confusing.
        if self.is_mac():
            return "Mac OS X %s" % self._platform_module.mac_ver()[0]

        # Returns strings like:
        # Linux-2.6.18-194.3.1.el5-i686-with-redhat-5.5-Final
        # Windows-2008ServerR2-6.1.7600
        return self._platform_module.platform()

    def os_version_name(self, table=None):
        if not self.os_version:
            return None
        if table:
            return VersionNameMap.map(self).to_name(self.os_version, table=table)
        version_name = VersionNameMap.map(self).to_name(self.os_version, table=PUBLIC_TABLE)
        if not version_name:
            version_name = VersionNameMap.map(self).to_name(self.os_version, table=INTERNAL_TABLE)
        return version_name

    def total_bytes_memory(self):
        if self.is_mac():
            return int(self._executive.run_command(["sysctl", "-n", "hw.memsize"]))
        return None

    def terminal_width(self):
        """Returns MAX if the width cannot be determined."""
        try:
            if self.is_win():
                # From http://code.activestate.com/recipes/440694-determine-size-of-console-window-on-windows/
                from ctypes import windll, create_string_buffer
                handle = windll.kernel32.GetStdHandle(-12)  # -12 == stderr
                console_screen_buffer_info = create_string_buffer(22)  # 22 == sizeof(console_screen_buffer_info)
                if windll.kernel32.GetConsoleScreenBufferInfo(handle, console_screen_buffer_info):
                    import struct
                    _, _, _, _, _, left, _, right, _, _, _ = struct.unpack("hhhhHhhhhhh", console_screen_buffer_info.raw)
                    # Note that we return 1 less than the width since writing into the rightmost column
                    # automatically performs a line feed.
                    return right - left
                return self.MAX
            else:
                import fcntl
                import struct
                import termios
                packed = fcntl.ioctl(sys.stderr.fileno(), termios.TIOCGWINSZ, '\0' * 8)
                _, columns, _, _ = struct.unpack('HHHH', packed)
                return columns
        except:
            return self.MAX

    def build_version(self):
        if self.is_mac():
            return self._executive.run_command(['/usr/bin/sw_vers', '-buildVersion'], return_stderr=False, ignore_errors=True).rstrip()
        return None

    @memoized
    def xcode_sdk_version(self, sdk_name):
        if self.is_mac():
            # Assumes that xcrun does not write to standard output on failure (e.g. SDK does not exist).
            xcrun_output = self._executive.run_command(['xcrun', '--sdk', sdk_name, '--show-sdk-version'], return_stderr=False, ignore_errors=True).rstrip()
            if xcrun_output:
                return Version.from_string(xcrun_output)
        return None

    def xcode_simctl_list(self):
        if not self.is_mac():
            return ()
        output = self._executive.run_command(['xcrun', 'simctl', 'list'], return_stderr=False)
        return (line for line in output.splitlines())

    @memoized
    def xcode_version(self):
        if not self.xcode_sdk_version('macosx'):
            raise NotImplementedError
        return Version.from_string(self._executive.run_command(['xcodebuild', '-version']).split()[1])

    @memoized
    def available_sdks(self):
        if not self.xcode_sdk_version('macosx'):
            return []

        XCODE_SDK_REGEX = re.compile('\-sdk (?P<sdk>\D+)\d+\.\d+(?P<specifier>\D*)')
        output = self._executive.run_command(['xcodebuild', '-showsdks'], return_stderr=False)

        sdks = list()
        for line in output.splitlines():
            match = XCODE_SDK_REGEX.search(line)
            if match:
                sdks.append(match.group('sdk') + match.group('specifier'))
        return sdks

    def _determine_os_name(self, sys_platform):
        if sys_platform == 'darwin':
            return 'mac'
        if sys_platform == 'ios' or sys_platform == 'watchos':
            return 'ios'
        if sys_platform.startswith('linux'):
            return 'linux'
        if sys_platform.startswith('win') or sys_platform == 'cygwin':
            return 'win'
        if sys_platform.startswith('freebsd'):
            return 'freebsd'
        if sys_platform.startswith('openbsd'):
            return 'openbsd'
        if sys_platform.startswith('haiku'):
            return 'haiku'
        raise AssertionError('unrecognized platform string "%s"' % sys_platform)

    def _win_version(self):
        version = self._win_version_str()
        match_object = re.search(r'(?P<major>\d+)\.(?P<minor>\d+)\.(?P<build>\d+)', version)
        assert match_object, 'cmd returned an unexpected version string: ' + version
        return Version.from_iterable(match_object.groups())

    def _win_version_str(self):
        version = self._platform_module.win32_ver()[1]
        if version:
            return version
        # Note that this should only ever be called on windows, so this should always work.
        return self._executive.run_command(['cmd', '/c', 'ver'], decode_output=False)
