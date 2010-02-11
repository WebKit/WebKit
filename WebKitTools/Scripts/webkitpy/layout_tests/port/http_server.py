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

"""A class to help start/stop the lighttpd server used by layout tests."""


import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib

import http_server_base


class HttpdNotStarted(Exception):
    pass


class Lighttpd(http_server_base.HttpServerBase):

    def __init__(self, port_obj, output_dir, background=False, port=None,
                 root=None, register_cygwin=None, run_background=None):
        """Args:
          output_dir: the absolute path to the layout test result directory
        """
        # Webkit tests
        http_server_base.HttpServerBase.__init__(self, port_obj)
        self._output_dir = output_dir
        self._process = None
        self._port = port
        self._root = root
        self._register_cygwin = register_cygwin
        self._run_background = run_background
        if self._port:
            self._port = int(self._port)

        try:
            self._webkit_tests = os.path.join(
                self._port_obj.layout_tests_dir(), 'http', 'tests')
            self._js_test_resource = os.path.join(
                self._port_obj.layout_tests_dir(), 'fast', 'js', 'resources')
        except:
            self._webkit_tests = None
            self._js_test_resource = None

        # Self generated certificate for SSL server (for client cert get
        # <base-path>\chrome\test\data\ssl\certs\root_ca_cert.crt)
        self._pem_file = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'httpd2.pem')

        # One mapping where we can get to everything
        self.VIRTUALCONFIG = []

        if self._webkit_tests:
            self.VIRTUALCONFIG.extend(
               # Three mappings (one with SSL) for LayoutTests http tests
               [{'port': 8000, 'docroot': self._webkit_tests},
                {'port': 8080, 'docroot': self._webkit_tests},
                {'port': 8443, 'docroot': self._webkit_tests,
                 'sslcert': self._pem_file}])

    def is_running(self):
        return self._process != None

    def start(self):
        if self.is_running():
            raise 'Lighttpd already running'

        base_conf_file = self._port_obj.path_from_webkit_base('WebKitTools',
            'Scripts', 'webkitpy', 'layout_tests', 'port', 'lighttpd.conf')
        out_conf_file = os.path.join(self._output_dir, 'lighttpd.conf')
        time_str = time.strftime("%d%b%Y-%H%M%S")
        access_file_name = "access.log-" + time_str + ".txt"
        access_log = os.path.join(self._output_dir, access_file_name)
        log_file_name = "error.log-" + time_str + ".txt"
        error_log = os.path.join(self._output_dir, log_file_name)

        # Remove old log files. We only need to keep the last ones.
        self.remove_log_files(self._output_dir, "access.log-")
        self.remove_log_files(self._output_dir, "error.log-")

        # Write out the config
        f = file(base_conf_file, 'rb')
        base_conf = f.read()
        f.close()

        f = file(out_conf_file, 'wb')
        f.write(base_conf)

        # Write out our cgi handlers.  Run perl through env so that it
        # processes the #! line and runs perl with the proper command
        # line arguments. Emulate apache's mod_asis with a cat cgi handler.
        f.write(('cgi.assign = ( ".cgi"  => "/usr/bin/env",\n'
                 '               ".pl"   => "/usr/bin/env",\n'
                 '               ".asis" => "/bin/cat",\n'
                 '               ".php"  => "%s" )\n\n') %
                                     self._port_obj._path_to_lighttpd_php())

        # Setup log files
        f.write(('server.errorlog = "%s"\n'
                 'accesslog.filename = "%s"\n\n') % (error_log, access_log))

        # Setup upload folders. Upload folder is to hold temporary upload files
        # and also POST data. This is used to support XHR layout tests that
        # does POST.
        f.write(('server.upload-dirs = ( "%s" )\n\n') % (self._output_dir))

        # Setup a link to where the js test templates are stored
        f.write(('alias.url = ( "/js-test-resources" => "%s" )\n\n') %
                    (self._js_test_resource))

        # dump out of virtual host config at the bottom.
        if self._root:
            if self._port:
                # Have both port and root dir.
                mappings = [{'port': self._port, 'docroot': self._root}]
            else:
                # Have only a root dir - set the ports as for LayoutTests.
                # This is used in ui_tests to run http tests against a browser.

                # default set of ports as for LayoutTests but with a
                # specified root.
                mappings = [{'port': 8000, 'docroot': self._root},
                            {'port': 8080, 'docroot': self._root},
                            {'port': 8443, 'docroot': self._root,
                             'sslcert': self._pem_file}]
        else:
            mappings = self.VIRTUALCONFIG
        for mapping in mappings:
            ssl_setup = ''
            if 'sslcert' in mapping:
                ssl_setup = ('  ssl.engine = "enable"\n'
                             '  ssl.pemfile = "%s"\n' % mapping['sslcert'])

            f.write(('$SERVER["socket"] == "127.0.0.1:%d" {\n'
                     '  server.document-root = "%s"\n' +
                     ssl_setup +
                     '}\n\n') % (mapping['port'], mapping['docroot']))
        f.close()

        executable = self._port_obj._path_to_lighttpd()
        module_path = self._port_obj._path_to_lighttpd_modules()
        start_cmd = [executable,
                     # Newly written config file
                     '-f', os.path.join(self._output_dir, 'lighttpd.conf'),
                     # Where it can find its module dynamic libraries
                     '-m', module_path]

        if not self._run_background:
            start_cmd.append(# Don't background
                             '-D')

        # Copy liblightcomp.dylib to /tmp/lighttpd/lib to work around the
        # bug that mod_alias.so loads it from the hard coded path.
        if sys.platform == 'darwin':
            tmp_module_path = '/tmp/lighttpd/lib'
            if not os.path.exists(tmp_module_path):
                os.makedirs(tmp_module_path)
            lib_file = 'liblightcomp.dylib'
            shutil.copyfile(os.path.join(module_path, lib_file),
                            os.path.join(tmp_module_path, lib_file))

        # Put the cygwin directory first in the path to find cygwin1.dll
        env = os.environ
        if sys.platform in ('cygwin', 'win32'):
            env['PATH'] = '%s;%s' % (
                self._port_obj.path_from_chromium_base('third_party',
                                                       'cygwin', 'bin'),
                env['PATH'])

        if sys.platform == 'win32' and self._register_cygwin:
            setup_mount = port.path_from_chromium_base('third_party',
                'cygwin', 'setup_mount.bat')
            subprocess.Popen(setup_mount).wait()

        logging.debug('Starting http server')
        self._process = subprocess.Popen(start_cmd, env=env)

        # Wait for server to start.
        self.mappings = mappings
        server_started = self.wait_for_action(
            self.is_server_running_on_all_ports)

        # Our process terminated already
        if not server_started or self._process.returncode != None:
            raise google.httpd_utils.HttpdNotStarted('Failed to start httpd.')

        logging.debug("Server successfully started")

    # TODO(deanm): Find a nicer way to shutdown cleanly.  Our log files are
    # probably not being flushed, etc... why doesn't our python have os.kill ?

    def stop(self, force=False):
        if not force and not self.is_running():
            return

        httpd_pid = None
        if self._process:
            httpd_pid = self._process.pid
        self._port_obj._shut_down_http_server(httpd_pid)

        if self._process:
            self._process.wait()
            self._process = None

if '__main__' == __name__:
    # Provide some command line params for starting/stopping the http server
    # manually. Also used in ui_tests to run http layout tests in a browser.
    option_parser = optparse.OptionParser()
    option_parser.add_option('-k', '--server',
        help='Server action (start|stop)')
    option_parser.add_option('-p', '--port',
        help='Port to listen on (overrides layout test ports)')
    option_parser.add_option('-r', '--root',
        help='Absolute path to DocumentRoot (overrides layout test roots)')
    option_parser.add_option('--register_cygwin', action="store_true",
        dest="register_cygwin", help='Register Cygwin paths (on Win try bots)')
    option_parser.add_option('--run_background', action="store_true",
        dest="run_background",
        help='Run on background (for running as UI test)')
    options, args = option_parser.parse_args()

    if not options.server:
        print ('Usage: %s --server {start|stop} [--root=root_dir]'
               ' [--port=port_number]' % sys.argv[0])
    else:
        if (options.root is None) and (options.port is not None):
            # specifying root but not port means we want httpd on default
            # set of ports that LayoutTest use, but pointing to a different
            # source of tests. Specifying port but no root does not seem
            # meaningful.
            raise 'Specifying port requires also a root.'
        httpd = Lighttpd(tempfile.gettempdir(),
                         port=options.port,
                         root=options.root,
                         register_cygwin=options.register_cygwin,
                         run_background=options.run_background)
        if 'start' == options.server:
            httpd.start()
        else:
            httpd.stop(force=True)
