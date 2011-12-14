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

"""Wrapper objects for WebKit-specific utility routines."""

# FIXME: This file needs to be unified with common/checkout/scm.py and
# common/config/ports.py .

from webkitpy.common.system import logutils
from webkitpy.common.system import executive


_log = logutils.get_logger(__file__)

#
# FIXME: This is used to record if we've already hit the filesystem to look
# for a default configuration. We cache this to speed up the unit tests,
# but this can be reset with clear_cached_configuration(). This should be
# replaced with us consistently using MockConfigs() for tests that don't
# hit the filesystem at all and provide a reliable value.
#
_have_determined_configuration = False
_configuration = "Release"


def clear_cached_configuration():
    global _have_determined_configuration, _configuration
    _have_determined_configuration = False
    _configuration = "Release"


class Config(object):
    _FLAGS_FROM_CONFIGURATIONS = {
        "Debug": "--debug",
        "Release": "--release",
    }

    def __init__(self, executive, filesystem):
        self._executive = executive
        self._filesystem = filesystem
        self._webkit_base_dir = None
        self._default_configuration = None
        self._build_directories = {}

    def build_directory(self, configuration):
        """Returns the path to the build directory for the configuration."""
        if configuration:
            flags = ["--configuration",
                     self._FLAGS_FROM_CONFIGURATIONS[configuration]]
        else:
            configuration = ""
            flags = ["--top-level"]

        if not self._build_directories.get(configuration):
            args = ["perl", self._script_path("webkit-build-directory")] + flags
            self._build_directories[configuration] = (
                self._executive.run_command(args).rstrip())

        return self._build_directories[configuration]

    def build_dumprendertree(self, configuration):
        """Builds DRT in the given configuration.

        Returns True if the  build was successful and up-to-date."""
        flag = self._FLAGS_FROM_CONFIGURATIONS[configuration]
        exit_code = self._executive.run_command([
            self._script_path("build-dumprendertree"), flag],
            return_exit_code=True)
        if exit_code != 0:
            _log.error("Failed to build DumpRenderTree")
            return False
        return True

    def default_configuration(self):
        """Returns the default configuration for the user.

        Returns the value set by 'set-webkit-configuration', or "Release"
        if that has not been set. This mirrors the logic in webkitdirs.pm."""
        if not self._default_configuration:
            self._default_configuration = self._determine_configuration()
        if not self._default_configuration:
            self._default_configuration = 'Release'
        if self._default_configuration not in self._FLAGS_FROM_CONFIGURATIONS:
            _log.warn("Configuration \"%s\" is not a recognized value.\n" %
                      self._default_configuration)
            _log.warn("Scripts may fail.  "
                      "See 'set-webkit-configuration --help'.")
        return self._default_configuration

    def path_from_webkit_base(self, *comps):
        return self._filesystem.join(self.webkit_base_dir(), *comps)

    def webkit_base_dir(self):
        """Returns the absolute path to the top of the WebKit tree.

        Raises an AssertionError if the top dir can't be determined."""
        # Note: this code somewhat duplicates the code in
        # scm.find_checkout_root(). However, that code only works if the top
        # of the SCM repository also matches the top of the WebKit tree. The
        # Chromium ports, for example, only check out subdirectories like
        # Tools/Scripts, and so we still have to do additional work
        # to find the top of the tree.
        #
        # This code will also work if there is no SCM system at all.
        if not self._webkit_base_dir:
            abspath = self._filesystem.abspath(__file__)
            self._webkit_base_dir = abspath[0:abspath.find('Tools') - 1]
        return self._webkit_base_dir

    def _script_path(self, script_name):
        return self._filesystem.join(self.webkit_base_dir(), "Tools",
                                     "Scripts", script_name)

    def _determine_configuration(self):
        # This mirrors the logic in webkitdirs.pm:determineConfiguration().
        #
        # FIXME: See the comment at the top of the file regarding unit tests
        # and our use of global mutable static variables.
        global _have_determined_configuration, _configuration
        if not _have_determined_configuration:
            contents = self._read_configuration()
            if not contents:
                contents = "Release"
            if contents == "Deployment":
                contents = "Release"
            if contents == "Development":
                contents = "Debug"
            _configuration = contents
            _have_determined_configuration = True
        return _configuration

    def _read_configuration(self):
        try:
            configuration_path = self._filesystem.join(self.build_directory(None),
                                                       "Configuration")
            if not self._filesystem.exists(configuration_path):
                return None
        except (OSError, executive.ScriptError):
            return None

        return self._filesystem.read_text_file(configuration_path).rstrip()
