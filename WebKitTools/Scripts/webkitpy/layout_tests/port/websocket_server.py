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


import logging
import optparse
import os
import subprocess
import sys
import tempfile
import time
import urllib

import http_server

_WS_LOG_PREFIX = 'pywebsocket.ws.log-'
_WSS_LOG_PREFIX = 'pywebsocket.wss.log-'

_DEFAULT_WS_PORT = 8880
_DEFAULT_WSS_PORT = 9323


def url_is_alive(url):
    """Checks to see if we get an http response from |url|.
    We poll the url 5 times with a 1 second delay.  If we don't
    get a reply in that time, we give up and assume the httpd
    didn't start properly.

    Args:
      url: The URL to check.
    Return:
      True if the url is alive.
    """
    wait_time = 5
    while wait_time > 0:
        try:
            response = urllib.urlopen(url)
            # Server is up and responding.
            return True
        except IOError:
            pass
        wait_time -= 1
        # Wait a second and try again.
        time.sleep(1)

    return False


class PyWebSocketNotStarted(Exception):
    pass


class PyWebSocketNotFound(Exception):
    pass


class PyWebSocket(http_server.Lighttpd):

    def __init__(self, port_obj, output_dir, port=_DEFAULT_WS_PORT,
                 root=None, use_tls=False,
                 register_cygwin=None,
                 pidfile=None):
        """Args:
          output_dir: the absolute path to the layout test result directory
        """
        http_server.Lighttpd.__init__(self, port_obj, output_dir,
                                      port=_DEFAULT_WS_PORT,
                                      root=root,
                                      register_cygwin=register_cygwin)
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
                os.path.join(self._root, 'websocket', 'tests'))
        else:
            try:
                self._layout_tests = self._port_obj.layout_tests_dir()
                self._web_socket_tests = os.path.join(self._layout_tests,
                     'websocket', 'tests')
            except:
                self._web_socket_tests = None

    def start(self):
        if not self._web_socket_tests:
            logging.info('No need to start %s server.' % self._server_name)
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
        self._wsout = open(output_log, "w")

        python_interp = sys.executable
        pywebsocket_base = os.path.join(
            os.path.dirname(os.path.dirname(os.path.dirname(
            os.path.dirname(os.path.dirname(
            os.path.abspath(__file__)))))), 'pywebsocket')
        pywebsocket_script = os.path.join(pywebsocket_base, 'mod_pywebsocket',
            'standalone.py')
        start_cmd = [
            python_interp, pywebsocket_script,
            '-p', str(self._port),
            '-d', self._layout_tests,
            '-s', self._web_socket_tests,
            '-l', error_log,
        ]

        handler_map_file = os.path.join(self._web_socket_tests,
                                        'handler_map.txt')
        if os.path.exists(handler_map_file):
            logging.debug('Using handler_map_file: %s' % handler_map_file)
            start_cmd.append('-m')
            start_cmd.append(handler_map_file)
        else:
            logging.warning('No handler_map_file found')

        if self._use_tls:
            start_cmd.extend(['-t', '-k', self._private_key,
                              '-c', self._certificate])

        # Put the cygwin directory first in the path to find cygwin1.dll
        env = os.environ
        if sys.platform in ('cygwin', 'win32'):
            env['PATH'] = '%s;%s' % (
                self._port_obj.path_from_chromium_base('third_party',
                                                       'cygwin', 'bin'),
                env['PATH'])

        if sys.platform == 'win32' and self._register_cygwin:
            setup_mount = self._port_obj.path_from_chromium_base(
                'third_party', 'cygwin', 'setup_mount.bat')
            subprocess.Popen(setup_mount).wait()

        env['PYTHONPATH'] = (pywebsocket_base + os.path.pathsep +
                             env.get('PYTHONPATH', ''))

        logging.debug('Starting %s server on %d.' % (
            self._server_name, self._port))
        logging.debug('cmdline: %s' % ' '.join(start_cmd))
        self._process = subprocess.Popen(start_cmd, stdout=self._wsout,
                                         stderr=subprocess.STDOUT,
                                         env=env)

        # Wait a bit before checking the liveness of the server.
        time.sleep(0.5)

        if self._use_tls:
            url = 'https'
        else:
            url = 'http'
        url = url + '://127.0.0.1:%d/' % self._port
        if not url_is_alive(url):
            fp = open(output_log)
            try:
                for line in fp:
                    logging.error(line)
            finally:
                fp.close()
            raise PyWebSocketNotStarted(
                'Failed to start %s server on port %s.' %
                    (self._server_name, self._port))

        # Our process terminated already
        if self._process.returncode != None:
            raise PyWebSocketNotStarted(
                'Failed to start %s server.' % self._server_name)
        if self._pidfile:
            f = open(self._pidfile, 'w')
            f.write("%d" % self._process.pid)
            f.close()

    def stop(self, force=False):
        if not force and not self.is_running():
            return

        if self._process:
            pid = self._process.pid
        elif self._pidfile:
            f = open(self._pidfile)
            pid = int(f.read().strip())
            f.close()

        if not pid:
            raise PyWebSocketNotFound(
                'Failed to find %s server pid.' % self._server_name)

        logging.debug('Shutting down %s server %d.' % (self._server_name, pid))
        self._port_obj._kill_process(pid)

        if self._process:
            self._process.wait()
            self._process = None

        if self._wsout:
            self._wsout.close()
            self._wsout = None


if '__main__' == __name__:
    # Provide some command line params for starting the PyWebSocket server
    # manually.
    option_parser = optparse.OptionParser()
    option_parser.add_option('--server', type='choice',
                             choices=['start', 'stop'], default='start',
                             help='Server action (start|stop)')
    option_parser.add_option('-p', '--port', dest='port',
                             default=None, help='Port to listen on')
    option_parser.add_option('-r', '--root',
                             help='Absolute path to DocumentRoot '
                                  '(overrides layout test roots)')
    option_parser.add_option('-t', '--tls', dest='use_tls',
                             action='store_true',
                             default=False, help='use TLS (wss://)')
    option_parser.add_option('-k', '--private_key', dest='private_key',
                             default='', help='TLS private key file.')
    option_parser.add_option('-c', '--certificate', dest='certificate',
                             default='', help='TLS certificate file.')
    option_parser.add_option('--register_cygwin', action="store_true",
                             dest="register_cygwin",
                             help='Register Cygwin paths (on Win try bots)')
    option_parser.add_option('--pidfile', help='path to pid file.')
    options, args = option_parser.parse_args()

    if not options.port:
        if options.use_tls:
            options.port = _DEFAULT_WSS_PORT
        else:
            options.port = _DEFAULT_WS_PORT

    kwds = {'port': options.port, 'use_tls': options.use_tls}
    if options.root:
        kwds['root'] = options.root
    if options.private_key:
        kwds['private_key'] = options.private_key
    if options.certificate:
        kwds['certificate'] = options.certificate
    kwds['register_cygwin'] = options.register_cygwin
    if options.pidfile:
        kwds['pidfile'] = options.pidfile

    pywebsocket = PyWebSocket(tempfile.gettempdir(), **kwds)

    if 'start' == options.server:
        pywebsocket.start()
    else:
        pywebsocket.stop(force=True)
