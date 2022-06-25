# Copyright (C) 2016 Igalia S.L. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#
#     * Neither the name of the copyright holder nor the names of its
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

from webkitpy.port.driver import Driver

_log = logging.getLogger(__name__)


class WaylandDriver(Driver):
    @staticmethod
    def check_driver(port):
        if not any(wayland_env_var in os.environ for wayland_env_var in ['WAYLAND_DISPLAY', 'WAYLAND_SOCKET']):
            _log.error('WAYLAND_DISPLAY or WAYLAND_SOCKET not found in the environment. Cannot run tests.')
            return False
        return True

    def _setup_environ_for_test(self):
        driver_environment = super(WaylandDriver, self)._setup_environ_for_test()
        self._port._copy_value_from_environ_if_set(driver_environment, 'WAYLAND_DISPLAY')
        self._port._copy_value_from_environ_if_set(driver_environment, 'WAYLAND_SOCKET')
        driver_environment['GDK_BACKEND'] = 'wayland'
        driver_environment['EGL_PLATFORM'] = 'wayland'
        return driver_environment
