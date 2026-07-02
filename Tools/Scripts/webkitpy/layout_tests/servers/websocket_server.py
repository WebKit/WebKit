# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2016 Apple Inc. All rights reserved.
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

"""A class to help start/stop the PyWebSocket server used by layout tests."""

import logging
import os
import sys

from webkitpy.layout_tests.servers import http_server_base

_log = logging.getLogger(__name__)


_WS_LOG_NAME = 'pywebsocket.ws.log'
_WSS_LOG_NAME = 'pywebsocket.wss.log'


class PyWebSocket(http_server_base.HttpServerBase):
    DEFAULT_WS_PORT = 8880
    DEFAULT_WSS_PORT = 9323

    def __init__(self, port_obj, output_dir, port=DEFAULT_WS_PORT,
                 root=None, use_tls=False,
                 private_key=None, certificate=None, ca_certificate=None,
                 pidfile=None):
        """Args:
          output_dir: the absolute path to the layout test result directory
        """
        super(PyWebSocket, self).__init__(port_obj)

        self._output_dir = output_dir
        self._pid_file = pidfile
        self._process = None

        self._port = port
        self._root = root
        self._use_tls = use_tls

        self._name = 'pywebsocket'
        if self._use_tls:
            self._name = 'pywebsocket_secure'

        # Self generated certificate for SSL server (for client cert get
        # <base-path>\chrome\test\data\ssl\certs\root_ca_cert.crt)
        self._pem_file = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'httpd2.pem')

        if private_key:
            self._private_key = private_key
        else:
            self._private_key = self._pem_file
        if certificate:
            self._certificate = certificate
        else:
            self._certificate = self._pem_file
        self._ca_certificate = ca_certificate
        if self._port:
            self._port = int(self._port)
        self._wsout = None
        self._mappings = [{'port': self._port}]

        if not self._pid_file:
            self._pid_file = self._filesystem.join(self._runtime_path, '%s.pid' % self._name)

        # Webkit tests
        # FIXME: This is the wrong way to detect if we're in Chrome vs. WebKit!
        # The port objects are supposed to abstract this.
        if self._root:
            self._layout_tests = self._filesystem.abspath(self._root)
            self._web_socket_tests = self._filesystem.abspath(self._filesystem.join(self._root, 'http', 'tests', 'websocket', 'tests'))
        else:
            try:
                self._layout_tests = self._port_obj.layout_tests_dir()
                self._web_socket_tests = self._filesystem.join(self._layout_tests, 'http', 'tests', 'websocket', 'tests')
            except Exception as e:
                _log.error('Failed to join path for layout_test websocket server: %s' % str(e))
                self._web_socket_tests = None

        if self._use_tls:
            self._log_prefix = _WSS_LOG_NAME
        else:
            self._log_prefix = _WS_LOG_NAME

    def ports_to_forward(self):
        return [self._port]

    def _prepare_config(self):
        self._filesystem.maybe_make_directory(self._output_dir)
        log_file_name = self._log_prefix
        error_log = self._filesystem.join(self._output_dir, log_file_name + "-err.txt")
        output_log = self._filesystem.join(self._output_dir, log_file_name + "-out.txt")
        self._wsout = self._filesystem.open_text_file_for_writing(output_log)

        python_interp = sys.executable

        wpt_tools_base = self._filesystem.join(self._layout_tests, "imported", "w3c", "web-platform-tests", "tools")
        pywebsocket_base = self._filesystem.join(wpt_tools_base, "third_party", "pywebsocket3")
        pywebsocket_deps = [self._filesystem.join(wpt_tools_base, "third_party", "six")]
        pywebsocket_script = self._filesystem.join(pywebsocket_base, 'pywebsocket3', 'standalone.py')
        start_cmd = [
            python_interp, '-u', pywebsocket_script,
            '--server-host', '0.0.0.0' if self._port_obj.get_option("http_all_interfaces") else 'localhost',
            '--port', str(self._port),
            # FIXME: Don't we have a self._port_obj.layout_test_path?
            '--document-root', self._filesystem.join(self._layout_tests, 'http', 'tests'),
            '--scan-dir', self._web_socket_tests,
            '--cgi-paths', '/websocket/tests',
            '--log-file', error_log,
        ]

        handler_map_file = self._filesystem.join(self._web_socket_tests, 'handler_map.txt')
        if self._filesystem.exists(handler_map_file):
            _log.debug('Using handler_map_file: %s' % handler_map_file)
            start_cmd.append('--websock-handlers-map-file')
            start_cmd.append(handler_map_file)
        else:
            _log.warning('No handler_map_file found')

        if self._use_tls:
            start_cmd.extend(['-t', '-k', self._private_key,
                              '-c', self._certificate])
            if self._ca_certificate:
                start_cmd.append('--tls-client-ca')
                start_cmd.append(self._ca_certificate)

        self._start_cmd = start_cmd
        server_name = self._filesystem.basename(pywebsocket_script)
        self._env = self._port_obj.setup_environ_for_server(server_name)
        self._env['PYTHONPATH'] = os.path.pathsep.join([pywebsocket_base] + pywebsocket_deps + [self._env.get('PYTHONPATH', '')])

    def _remove_stale_logs(self):
        try:
            self._remove_log_files(self._output_dir, self._log_prefix)
        except OSError as e:
            _log.warning('Failed to remove stale %s log files: %s' % (self._name, str(e)))

    def _spawn_process(self):
        _log.debug('Starting %s server, cmd="%s"' % (self._name, self._start_cmd))
        self._process = self._executive.popen(self._start_cmd, env=self._env, shell=False, stdin=self._executive.PIPE, stdout=self._wsout, stderr=self._executive.STDOUT)
        self._filesystem.write_text_file(self._pid_file, str(self._process.pid))
        return self._process.pid

    def _stop_running_server(self):
        # FIXME: It would be nice if we had a cleaner way of killing this process.
        # Currently we throw away the process object created in _spawn_process,
        # since there doesn't appear to be any way to kill the server any more
        # cleanly using it than just killing the pid, and we need to support
        # killing a pid directly anyway for run-webkit-httpd and run-webkit-websocketserver.
        self._wait_for_action(self._check_and_kill)
        if self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)

        if self._wsout:
            self._wsout.close()
            self._wsout = None

    def _check_and_kill(self):
        if self._process is not None:
            self._process.poll()
            if self._process.returncode is not None:
                return True

        if self._executive.check_running_pid(self._pid):
            host = self._port_obj.host
            if host.platform.is_win() and not host.platform.is_cygwin():
                # FIXME: https://bugs.webkit.org/show_bug.cgi?id=106838
                # We need to kill all of the child processes as well as the
                # parent, so we can't use executive.kill_process().
                #
                # If this is actually working, we should figure out a clean API.
                self._executive.run_command(["taskkill.exe", "/f", "/t", "/pid", self._pid], ignore_errors=True)
            else:
                self._executive.kill_process(self._pid)
            return False
        return True

    def _is_server_running_on_all_ports(self):
        if self._process is not None:
            self._process.poll()
            if self._process.returncode is not None:
                _log.debug("Server isn't running at all")
                raise http_server_base.ServerError("Server exited")

        return super(PyWebSocket, self)._is_server_running_on_all_ports()


def is_web_socket_server_running():
    return http_server_base.HttpServerBase._is_running_on_port(PyWebSocket.DEFAULT_WS_PORT)
