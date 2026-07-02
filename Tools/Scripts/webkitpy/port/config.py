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

"""Wrapper objects for WebKit-specific utility routines."""

# FIXME: This file needs to be unified with common/config/ports.py .

import logging

from webkitpy.common import webkit_finder
from webkitpy.common.memoized import memoized


_log = logging.getLogger(__name__)


@memoized
def apple_additions():
    try:
        # A level of 0 means that only absolute paths will be used.
        return __import__('apple_additions', globals=globals(), locals=None, fromlist=[], level=0)
    except ImportError:
        return None


class Config(object):
    _FLAGS_FROM_CONFIGURATIONS = {
        "Debug": "--debug",
        "Release": "--release",
    }

    # Shared across all Config instances: webkit-build-directory is a ~110ms
    # perl invocation whose result is invariant for given inputs within a run.
    _build_directories = {}

    def __init__(self, executive, filesystem, port_implementation=None, use_cmake=False):
        self._executive = executive
        self._filesystem = filesystem
        self._webkit_finder = webkit_finder.WebKitFinder(self._filesystem)
        self._default_configuration = None
        self._port_implementation = port_implementation
        self._use_cmake = use_cmake

    @classmethod
    def _clear_cache_for_testing(cls):
        cls._build_directories = {}

    def build_directory(self, configuration, for_host=False):
        """Returns the path to the build directory for the configuration."""
        port_impl = self._port_implementation if not for_host else None
        cache_key = (port_impl, configuration or "", for_host, self._use_cmake)
        if self._build_directories.get(cache_key):
            return self._build_directories[cache_key]

        if configuration:
            flags = ["--configuration", self.flag_for_configuration(configuration)]
        else:
            flags = []
        if port_impl:
            flags.append('--' + port_impl)
        if self._use_cmake:
            flags.append('--cmake')

        args = ["perl", self._webkit_finder.path_to_script("webkit-build-directory")] + flags
        output = self._executive.run_command(args, cwd=self._webkit_finder.webkit_base(), return_stderr=False).rstrip()
        parts = output.split("\n")
        self._build_directories[cache_key] = parts[0]

        # With no --configuration flag, webkit-build-directory also prints the
        # default-configuration dir on a second line; cache it for later hits.
        if len(parts) == 2:
            default_configuration = parts[1][len(parts[0]):]
            if default_configuration.startswith("/"):
                default_configuration = default_configuration[1:]
            self._build_directories[(port_impl, default_configuration, for_host, self._use_cmake)] = parts[1]

        return self._build_directories[cache_key]

    def flag_for_configuration(self, configuration):
        if not configuration:
            configuration = self.default_configuration()
        return self._FLAGS_FROM_CONFIGURATIONS[configuration]

    def default_configuration(self):
        """Returns the default configuration for the user.

        Returns the value set by 'set-webkit-configuration', or "Release"
        if that has not been set. This mirrors the logic in webkitdirs.pm."""
        if not self._default_configuration:
            self._default_configuration = self._determine_configuration()
        if not self._default_configuration:
            self._default_configuration = 'Release'
        if self._default_configuration not in self._FLAGS_FROM_CONFIGURATIONS:
            _log.warn("Configuration \"%s\" is not a recognized value.\n" % self._default_configuration)
            _log.warn("Scripts may fail.  See 'set-webkit-configuration --help'.")
        return self._default_configuration

    @memoized
    def _determine_configuration(self):
        # This mirrors the logic in webkitdirs.pm:determineConfiguration().
        contents = self._read_configuration()
        if not contents:
            contents = "Release"
        if contents == "Deployment":
            contents = "Release"
        if contents == "Development":
            contents = "Debug"
        return contents

    def _read_configuration(self):
        try:
            configuration_path = self._filesystem.join(self.build_directory(None), "Configuration")
            if not self._filesystem.exists(configuration_path):
                return None
        except:
            return None

        return self._filesystem.read_text_file(configuration_path).rstrip()

    @property
    @memoized
    def asan(self):
        try:
            return self._filesystem.exists(self._filesystem.join(self.build_directory(None), "ASan"))
        except:
            return False
