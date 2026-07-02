# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of the copyright holder nor the names of its
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
import shutil
import socket
import subprocess
import tempfile
import unittest
from unittest.mock import patch, MagicMock

from webkitpy.port.linux_container_sdk_utils import _strip_unix_path_prefix, _get_at_spi_bus_socket_or_dir_and_var


class StripUnixPathPrefixTest(unittest.TestCase):

    def test_dbus_standard_path(self):
        path = "/run/user/1000/bus"
        DBUS_SESSION_BUS_ADDRESS = f"unix:path={path}"
        path_resolved = _strip_unix_path_prefix(DBUS_SESSION_BUS_ADDRESS)
        self.assertEqual(path, path_resolved)

    def test_dbus_autolaunch_path(self):
        path = "/tmp/dbus-OxJnk3YDFt"
        DBUS_SESSION_BUS_ADDRESS = f"unix:path={path},guid=cd2293c3498d62c28d9e98816a0d8b27"
        path_resolved = _strip_unix_path_prefix(DBUS_SESSION_BUS_ADDRESS)
        self.assertEqual(path, path_resolved)

    def test_pulse_server_path(self):
        path = "/run/user/1000/pulse/native"
        PULSE_SERVER = f"unix:{path}"
        path_resolved = _strip_unix_path_prefix(PULSE_SERVER)
        self.assertEqual(path, path_resolved)

    def test_at_spi_bus_path(self):
        path = "/run/user/1000/at-spi/bus"
        AT_SPI_BUS_ADDRESS = f"unix:path={path},guid=b6602b80724b3d489a0e9c9c6a0d8b2d"
        path_resolved = _strip_unix_path_prefix(AT_SPI_BUS_ADDRESS)
        self.assertEqual(path, path_resolved)


class GetAtSpiBusDirTest(unittest.TestCase):
    def setUp(self):
        # Short prefix: AF_UNIX paths are limited to 108 chars on Linux.
        self.tmpdir = tempfile.mkdtemp(prefix='atspi-')
        self.addCleanup(shutil.rmtree, self.tmpdir, ignore_errors=True)
        self._sockets = []
        self.addCleanup(self._close_sockets)
        # Don't let a real AT_SPI_BUS_ADDRESS from the host leak in.
        env_patch = patch.dict(os.environ)
        env_patch.start()
        self.addCleanup(env_patch.stop)
        os.environ.pop('AT_SPI_BUS_ADDRESS', None)

    def _close_sockets(self):
        for s in self._sockets:
            s.close()

    def _make_socket(self, name):
        path = os.path.join(self.tmpdir, name)
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.bind(path)
        self._sockets.append(s)
        return path

    def _mock_gdbus(self, address=None, exc=None):
        target = 'webkitpy.port.linux_container_sdk_utils.subprocess.run'
        if exc is not None:
            return patch(target, side_effect=exc)
        result = MagicMock()
        result.stdout = "('{}',)\n".format(address) if address else ""
        return patch(target, return_value=result)

    def test_env_var_points_to_socket(self):
        sock = self._make_socket('bus_0')
        os.environ['AT_SPI_BUS_ADDRESS'] = "unix:path={},guid=abc".format(sock)
        with self._mock_gdbus() as mock_run:
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertEqual(at_spi_path, sock)
            self.assertEqual(at_spi_env_var, 'AT_SPI_BUS_ADDRESS')
            mock_run.assert_not_called()  # env var hit, gdbus must be skipped

    def test_env_var_missing_socket_falls_through(self):
        os.environ['AT_SPI_BUS_ADDRESS'] = "unix:path=/does/not/exist,guid=abc"
        with self._mock_gdbus():
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertIsNone(at_spi_path)
            self.assertIsNone(at_spi_env_var)

    def test_env_var_regular_file_falls_through(self):
        regular = os.path.join(self.tmpdir, 'plain.txt')
        open(regular, 'w').close()
        os.environ['AT_SPI_BUS_ADDRESS'] = "unix:path={},guid=abc".format(regular)
        with self._mock_gdbus():
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertIsNone(at_spi_path)
            self.assertIsNone(at_spi_env_var)

    def test_gdbus_returns_socket(self):
        sock = self._make_socket('bus_0')
        with self._mock_gdbus("unix:path={},guid=abc".format(sock)):
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertEqual(at_spi_path, sock)
            self.assertIsNone(at_spi_env_var)

    def test_gdbus_missing_socket_falls_through(self):
        with self._mock_gdbus("unix:path=/does/not/exist,guid=abc"):
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertIsNone(at_spi_path)
            self.assertIsNone(at_spi_env_var)

    def test_gdbus_not_installed_falls_through(self):
        with self._mock_gdbus(exc=FileNotFoundError()):
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertIsNone(at_spi_path)
            self.assertIsNone(at_spi_env_var)

    def test_gdbus_timeout_falls_through(self):
        with self._mock_gdbus(exc=subprocess.TimeoutExpired('gdbus', 5)):
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertIsNone(at_spi_path)
            self.assertIsNone(at_spi_env_var)

    def test_gdbus_malformed_output_falls_through(self):
        with self._mock_gdbus():  # stdout is ''
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertIsNone(at_spi_path)
            self.assertIsNone(at_spi_env_var)

    def test_falls_back_to_xdg_dir(self):
        xdg_dir = os.path.join(self.tmpdir, 'at-spi')
        os.makedirs(xdg_dir)
        with self._mock_gdbus():
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertEqual(at_spi_path, xdg_dir)
            self.assertIsNone(at_spi_env_var)

    def test_returns_none_when_nothing_resolves(self):
        with self._mock_gdbus():
            at_spi_path, at_spi_env_var = _get_at_spi_bus_socket_or_dir_and_var(self.tmpdir)
            self.assertIsNone(at_spi_path)
            self.assertIsNone(at_spi_env_var)
