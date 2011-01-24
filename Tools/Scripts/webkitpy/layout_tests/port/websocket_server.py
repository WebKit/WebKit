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

"""A class to help start/stop the PyWebSocket server used by layout tests."""


from __future__ import with_statement

import codecs
import logging
import optparse
import os
import subprocess
import sys
import tempfile
import time
import urllib

import factory
import http_server

from webkitpy.common.system.executive import Executive
from webkitpy.thirdparty.autoinstalled.pywebsocket import mod_pywebsocket


_log = logging.getLogger("webkitpy.layout_tests.port.websocket_server")

_WS_LOG_PREFIX = 'pywebsocket.ws.log-'
_WSS_LOG_PREFIX = 'pywebsocket.wss.log-'

_DEFAULT_WS_PORT = 8880
_DEFAULT_WSS_PORT = 9323


def url_is_alive(url):
    """Checks to see if we get an http response from |url|.
    We poll the url 20 times with a 0.5 second delay.  If we don't
    get a reply in that time, we give up and assume the httpd
    didn't start properly.

    Args:
      url: The URL to check.
    Return:
      True if the url is alive.
    """
    sleep_time = 0.5
    wait_time = 10
    while wait_time > 0:
        try:
            response = urllib.urlopen(url, proxies={})
            # Server is up and responding.
            return True
        except IOError:
            pass
        # Wait for sleep_time before trying again.
        wait_time -= sleep_time
        time.sleep(sleep_time)

    return False


class PyWebSocketNotStarted(Exception):
    pass


class PyWebSocketNotFound(Exception):
    pass


class PyWebSocket(http_server.Lighttpd):

    def __init__(self, port_obj, output_dir, port=_DEFAULT_WS_PORT,
                 root=None, use_tls=False,
                 pidfile=None):
        """Args:
          output_dir: the absolute path to the layout test result directory
        """
        http_server.Lighttpd.__init__(self, port_obj, output_dir,
                                      port=_DEFAULT_WS_PORT,
                                      root=root)
        self._output_dir = output_dir
        self._process = None
        self._port = port
        self._root = root
        self._use_tls = use_tls
        self._private_key = self._pem_file
        self._certificate = self._pem_file
        if self._port:
            self._port = int(self._port)
        if self._use_tls:
            self._server_name = 'PyWebSocket(Secure)'
        else:
            self._server_name = 'PyWebSocket'
        self._pidfile = pidfile
        self._wsout = None

        # Webkit tests
        if self._root:
            self._layout_tests = os.path.abspath(self._root)
            self._web_socket_tests = os.path.abspath(
                os.path.join(self._root, 'http', 'tests',
                             'websocket', 'tests'))
        else:
            try:
                self._layout_tests = self._port_obj.layout_tests_dir()
                self._web_socket_tests = os.path.join(self._layout_tests,
                     'http', 'tests', 'websocket', 'tests')
            except:
                self._web_socket_tests = None

    def start(self):
        if not self._web_socket_tests:
            _log.info('No need to start %s server.' % self._server_name)
            return
        if self.is_running():
            raise PyWebSocketNotStarted('%s is already running.' %
                                        self._server_name)

        time_str = time.strftime('%d%b%Y-%H%M%S')
        if self._use_tls:
            log_prefix = _WSS_LOG_PREFIX
        else:
            log_prefix = _WS_LOG_PREFIX
        log_file_name = log_prefix + time_str

        # Remove old log files. We only need to keep the last ones.
        self.remove_log_files(self._output_dir, log_prefix)

        error_log = os.path.join(self._output_dir, log_file_name + "-err.txt")

        output_log = os.path.join(self._output_dir, log_file_name + "-out.txt")
        self._wsout = codecs.open(output_log, "w", "utf-8")

        python_interp = sys.executable
        pywebsocket_base = os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(
            os.path.abspath(__file__)))), 'thirdparty',
            'autoinstalled', 'pywebsocket')
        pywebsocket_script = os.path.join(pywebsocket_base, 'mod_pywebsocket',
            'standalone.py')
        start_cmd = [
            python_interp, '-u', pywebsocket_script,
            '--server-host', '127.0.0.1',
            '--port', str(self._port),
            '--document-root', os.path.join(self._layout_tests, 'http', 'tests'),
            '--scan-dir', self._web_socket_tests,
            '--cgi-paths', '/websocket/tests',
            '--log-file', error_log,
        ]

        handler_map_file = os.path.join(self._web_socket_tests,
                                        'handler_map.txt')
        if os.path.exists(handler_map_file):
            _log.debug('Using handler_map_file: %s' % handler_map_file)
            start_cmd.append('--websock-handlers-map-file')
            start_cmd.append(handler_map_file)
        else:
            _log.warning('No handler_map_file found')

        if self._use_tls:
            start_cmd.extend(['-t', '-k', self._private_key,
                              '-c', self._certificate])

        env = self._port_obj.setup_environ_for_server()
        env['PYTHONPATH'] = (pywebsocket_base + os.path.pathsep +
                             env.get('PYTHONPATH', ''))

        _log.debug('Starting %s server on %d.' % (
                   self._server_name, self._port))
        _log.debug('cmdline: %s' % ' '.join(start_cmd))
        # FIXME: We should direct this call through Executive for testing.
        # Note: Not thread safe: http://bugs.python.org/issue2320
        self._process = subprocess.Popen(start_cmd,
                                         stdin=open(os.devnull, 'r'),
                                         stdout=self._wsout,
                                         stderr=subprocess.STDOUT,
                                         env=env)

        if self._use_tls:
            url = 'https'
        else:
            url = 'http'
        url = url + '://127.0.0.1:%d/' % self._port
        if not url_is_alive(url):
            if self._process.returncode == None:
                # FIXME: We should use a non-static Executive for easier
                # testing.
                Executive().kill_process(self._process.pid)
            with codecs.open(output_log, "r", "utf-8") as fp:
                for line in fp:
                    _log.error(line)
            raise PyWebSocketNotStarted(
                'Failed to start %s server on port %s.' %
                    (self._server_name, self._port))

        # Our process terminated already
        if self._process.returncode != None:
            raise PyWebSocketNotStarted(
                'Failed to start %s server.' % self._server_name)
        if self._pidfile:
            with codecs.open(self._pidfile, "w", "ascii") as file:
                file.write("%d" % self._process.pid)

    def stop(self, force=False):
        if not force and not self.is_running():
            return

        pid = None
        if self._process:
            pid = self._process.pid
        elif self._pidfile:
            with codecs.open(self._pidfile, "r", "ascii") as file:
                pid = int(file.read().strip())

        if not pid:
            raise PyWebSocketNotFound(
                'Failed to find %s server pid.' % self._server_name)

        _log.debug('Shutting down %s server %d.' % (self._server_name, pid))
        # FIXME: We should use a non-static Executive for easier testing.
        Executive().kill_process(pid)

        if self._process:
            # wait() is not threadsafe and can throw OSError due to:
            # http://bugs.python.org/issue1731717
            self._process.wait()
            self._process = None

        if self._wsout:
            self._wsout.close()
            self._wsout = None
