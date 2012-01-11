# Copyright (C) 2011 Google Inc. All rights reserved.
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

from webkitpy.layout_tests.port.webkit import WebKitPort
from webkitpy.layout_tests.models.test_configuration import TestConfiguration


_log = logging.getLogger(__name__)


class ApplePort(WebKitPort):
    """Shared logic between all of Apple's ports."""

    # This is used to represent the version of an operating system
    # corresponding to the "mac" or "win" base LayoutTests/platform
    # directory.  I'm not sure this concept is very useful,
    # but it gives us a way to refer to fallback paths *only* including
    # the base directory.
    # This is mostly done because TestConfiguration assumes that self.version()
    # will never return None. (None would be another way to represent this concept.)
    # Apple supposedly has explicit "future" results which are kept in an internal repository.
    # It's possible that Apple would want to fix this code to work better with those results.
    FUTURE_VERSION = 'future'  # FIXME: This whole 'future' thing feels like a hack.

    def _strip_port_name_prefix(self, port_name):
        # Callers treat this return value as the "version", which only works
        # because Apple ports use a simple name-version port_name scheme.
        # FIXME: This parsing wouldn't be needed if port_name handling was moved to factory.py
        # instead of the individual port constructors.
        return port_name[len(self.port_name + '-'):]

    def __init__(self, host, port_name=None, **kwargs):
        port_name = port_name or self.port_name
        WebKitPort.__init__(self, host, port_name=port_name, **kwargs)

        # FIXME: This sort of parsing belongs in factory.py!
        if port_name == '%s-wk2' % self.port_name:
            port_name = self.port_name
            # FIXME: This may be wrong, since options is a global property, and the port_name is specific to this object?
            self.set_option_default('webkit_test_runner', True)

        if port_name == self.port_name:
            self._version = host.platform.os_version
            self._name = self.port_name + '-' + self._version
        else:
            allowed_port_names = self.VERSION_FALLBACK_ORDER + [self.operating_system() + "-future"]
            assert port_name in allowed_port_names, "%s is not in %s" % (port_name, allowed_port_names)
            self._version = self._strip_port_name_prefix(port_name)

    # FIXME: A more sophisitcated version of this function should move to WebKitPort and replace all calls to name().
    def _port_name_with_version(self):
        components = [self.port_name]
        if self._version != self.FUTURE_VERSION:
            components.append(self._version)
        return '-'.join(components)

    def _generate_all_test_configurations(self):
        configurations = []
        for port_name in self.VERSION_FALLBACK_ORDER:
            if '-' in port_name:
                version = self._strip_port_name_prefix(port_name)
            else:
                # The version for the "base" port is currently defined as "future"
                # since TestConfiguration doesn't allow None as a valid version.
                version = self.FUTURE_VERSION

            for build_type in self.ALL_BUILD_TYPES:
                # Win and Mac ports both happen to only exist on x86 architectures and always use cpu graphics (gpu graphics is a chromium-only hack).
                # But at some later point we may need to make these configurable by the MacPort and WinPort subclasses.
                configurations.append(TestConfiguration(version=version, architecture='x86', build_type=build_type, graphics_type='cpu'))
        return configurations
