# Copyright (C) 2025 Igalia S.L. All rights reserved.
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


class MonadoDriver(Driver):
    @staticmethod
    def check_driver(port):
        monado_findcmd = ['which', 'monado-service']
        monado_found = port.host.executive.run_command(monado_findcmd, return_exit_code=True) == 0
        if not monado_found:
            _log.error("No monado-service found. Cannot run OpenXR API tests.")
        return monado_found

    def __init__(self, *args, **kwargs):
        Driver.__init__(self, *args, **kwargs)

    def _get_runtime_path(self, env):
        # Older OpenXR loaders seem to have problems finding the JSON configuration file on their own
        data_dirs = env.get("XDG_DATA_DIRS", "").split(":")
        for data_dir in data_dirs:
            candidate = os.path.join(data_dir, "openxr", "1", "openxr_monado.json")
            if os.path.exists(candidate):
                return candidate
        return ""

    def _setup_environ_for_test(self):
        driver_environment = super(MonadoDriver, self)._setup_environ_for_test()
        driver_environment['WITH_OPENXR_RUNTIME'] = 'y'
        driver_environment['XRT_COMPOSITOR_NULL'] = 'TRUE'
        driver_environment['XRT_NO_STDIN'] = 'TRUE'
        driver_environment['XR_RUNTIME_JSON'] = self._get_runtime_path(driver_environment)

        monado_command = ['monado-service']
        with open(os.devnull, 'w') as devnull:
            self._monado_service_process = self._port.host.executive.popen(monado_command, stderr=devnull, env=driver_environment)

        return driver_environment

    def stop(self):
        super(MonadoDriver, self).stop()
        if getattr(self, '_monado_service_process', None):
            self._monado_service_process.terminate()
            self._monado_service_process = None
