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
import time

from webkitpy.port.driver import Driver

_log = logging.getLogger(__name__)


class MonadoDriver(Driver):

    sock_path = "/run/user/1000/monado_comp_ipc"
    pid_path = "/run/user/1000/monado.pid"

    @staticmethod
    def check_driver(port):
        monado_findcmd = ['which', 'monado-service']
        monado_path = port.host.executive.run_command(monado_findcmd)
        if len(monado_path) > 0:
            _log.info("monado-service found at %s", monado_path)
            return True
        _log.error("No monado-service found. Cannot run OpenXR API tests.")
        return False

    def __init__(self, *args, **kwargs):
        Driver.__init__(self, *args, **kwargs)
        self._startup_delay_secs = 1.0

    def _get_runtime_path(self, env):
        # Older OpenXR loaders seem to have problems finding the JSON configuration file on their own
        data_dirs = env.get("XDG_DATA_DIRS", "").split(":")
        for data_dir in data_dirs:
            candidate = os.path.join(data_dir, "openxr", "1", "openxr_monado.json")
            if os.path.exists(candidate):
                with open(candidate, 'r') as f:
                    runtime_config = f.read()
                _log.info("Using %s for path, which contains %s", candidate, runtime_config)
                return candidate
        _log.error("Unable to find a path to openxr_monado.json. Applications won't be able to communicate with the runtime.")
        return ""

    def _setup_environ_for_test(self):
        driver_environment = super(MonadoDriver, self)._setup_environ_for_test()
        driver_environment['WITH_OPENXR_RUNTIME'] = 'y'
        driver_environment['XRT_COMPOSITOR_NULL'] = 'TRUE'
        driver_environment['XRT_NO_STDIN'] = 'TRUE'
        driver_environment['XR_RUNTIME_JSON'] = self._get_runtime_path(driver_environment)

        monado_command = ['monado-service']
        self._monado_service_process = self._port.host.executive.popen(monado_command, stderr=self._port.host.executive.STDOUT, env=driver_environment)

        start = time.time()
        timeout = 30

        time.sleep(1)
        while not (os.path.exists(MonadoDriver.sock_path) and os.access(MonadoDriver.pid_path, os.R_OK)):
            if time.time() - start > timeout:
                _log.error("Timed out waiting for Monado to start")
                break
            time.sleep(0.25)

        if os.path.exists(MonadoDriver.pid_path):
            try:
                with open(MonadoDriver.pid_path) as f:
                    pid = int(f.readline().strip())
                os.kill(pid, 0)
                _log.info(f"monado-service successfully running with pid {pid}")
            except (ValueError, FileNotFoundError, ProcessLookupError, PermissionError) as e:
                _log.error(f"Pidfile and socket present, but the Monado process does not exist for pid {pid}. Error type: {type(e).__name__}, details: {e}. Tests will time out.")

        return driver_environment

    def stop(self):
        super(MonadoDriver, self).stop()
        if getattr(self, '_monado_service_process', None):
            self._monado_service_process.terminate()
            self._monado_service_process = None
