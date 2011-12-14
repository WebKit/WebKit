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

"""A class to start/stop the apache http server used by layout tests."""


from __future__ import with_statement

import codecs
import logging
import optparse
import os
import re
import subprocess
import sys

import http_server_base

_log = logging.getLogger("webkitpy.layout_tests.port.apache_http_server")


class LayoutTestApacheHttpd(http_server_base.HttpServerBase):

    def __init__(self, port_obj, output_dir):
        """Args:
          port_obj: handle to the platform-specific routines
          output_dir: the absolute path to the layout test result directory
        """
        http_server_base.HttpServerBase.__init__(self, port_obj)
        self._output_dir = output_dir
        self._httpd_proc = None
        port_obj.maybe_make_directory(output_dir)

        self.mappings = [{'port': 8000},
                         {'port': 8080},
                         {'port': 8081},
                         {'port': 8443, 'sslcert': True}]

        # The upstream .conf file assumed the existence of /tmp/WebKit for
        # placing apache files like the lock file there.
        self._runtime_path = os.path.join("/tmp", "WebKit")
        port_obj.maybe_make_directory(self._runtime_path)

        # The PID returned when Apache is started goes away (due to dropping
        # privileges?). The proper controlling PID is written to a file in the
        # apache runtime directory.
        self._pid_file = os.path.join(self._runtime_path, 'httpd.pid')

        test_dir = self._port_obj.layout_tests_dir()
        js_test_resources_dir = self._cygwin_safe_join(test_dir, "fast", "js",
            "resources")
        media_resources_dir = self._cygwin_safe_join(test_dir, "media")
        mime_types_path = self._cygwin_safe_join(test_dir, "http", "conf",
            "mime.types")
        cert_file = self._cygwin_safe_join(test_dir, "http", "conf",
            "webkit-httpd.pem")
        access_log = self._cygwin_safe_join(output_dir, "access_log.txt")
        error_log = self._cygwin_safe_join(output_dir, "error_log.txt")
        document_root = self._cygwin_safe_join(test_dir, "http", "tests")

        # FIXME: We shouldn't be calling a protected method of _port_obj!
        executable = self._port_obj._path_to_apache()
        if self._is_cygwin():
            executable = self._get_cygwin_path(executable)

        cmd = [executable,
            '-f', "\"%s\"" % self._get_apache_config_file_path(test_dir, output_dir),
            '-C', "\'DocumentRoot \"%s\"\'" % document_root,
            '-c', "\'Alias /js-test-resources \"%s\"'" % js_test_resources_dir,
            '-c', "\'Alias /media-resources \"%s\"'" % media_resources_dir,
            '-C', "\'Listen %s\'" % "127.0.0.1:8000",
            '-C', "\'Listen %s\'" % "127.0.0.1:8081",
            '-c', "\'TypesConfig \"%s\"\'" % mime_types_path,
            '-c', "\'CustomLog \"%s\" common\'" % access_log,
            '-c', "\'ErrorLog \"%s\"\'" % error_log,
            '-C', "\'User \"%s\"\'" % os.environ.get("USERNAME",
                os.environ.get("USER", ""))]

        if self._is_cygwin():
            cygbin = self._port_obj._path_from_base('third_party', 'cygwin',
                'bin')
            # Not entirely sure why, but from cygwin we need to run the
            # httpd command through bash.
            self._start_cmd = [
                os.path.join(cygbin, 'bash.exe'),
                '-c',
                'PATH=%s %s' % (self._get_cygwin_path(cygbin), " ".join(cmd)),
              ]
        else:
            # TODO(ojan): When we get cygwin using Apache 2, use set the
            # cert file for cygwin as well.
            cmd.extend(['-c', "\'SSLCertificateFile %s\'" % cert_file])
            # Join the string here so that Cygwin/Windows and Mac/Linux
            # can use the same code. Otherwise, we could remove the single
            # quotes above and keep cmd as a sequence.
            self._start_cmd = " ".join(cmd)

    def _is_cygwin(self):
        return sys.platform in ("win32", "cygwin")

    def _cygwin_safe_join(self, *parts):
        """Returns a platform appropriate path."""
        path = os.path.join(*parts)
        if self._is_cygwin():
            return self._get_cygwin_path(path)
        return path

    def _get_cygwin_path(self, path):
        """Convert a Windows path to a cygwin path.

        The cygpath utility insists on converting paths that it thinks are
        Cygwin root paths to what it thinks the correct roots are.  So paths
        such as "C:\b\slave\webkit-release\build\third_party\cygwin\bin"
        are converted to plain "/usr/bin".  To avoid this, we
        do the conversion manually.

        The path is expected to be an absolute path, on any drive.
        """
        drive_regexp = re.compile(r'([a-z]):[/\\]', re.IGNORECASE)

        def lower_drive(matchobj):
            return '/cygdrive/%s/' % matchobj.group(1).lower()
        path = drive_regexp.sub(lower_drive, path)
        return path.replace('\\', '/')

    def _get_apache_config_file_path(self, test_dir, output_dir):
        """Returns the path to the apache config file to use.
        Args:
          test_dir: absolute path to the LayoutTests directory.
          output_dir: absolute path to the layout test results directory.
        """
        httpd_config = self._port_obj._path_to_apache_config_file()
        httpd_config_copy = os.path.join(output_dir, "httpd.conf")
        # httpd.conf is always utf-8 according to http://archive.apache.org/gnats/10125
        with codecs.open(httpd_config, "r", "utf-8") as httpd_config_file:
            httpd_conf = httpd_config_file.read()
        if self._is_cygwin():
            # This is a gross hack, but it lets us use the upstream .conf file
            # and our checked in cygwin. This tells the server the root
            # directory to look in for .so modules. It will use this path
            # plus the relative paths to the .so files listed in the .conf
            # file. We have apache/cygwin checked into our tree so
            # people don't have to install it into their cygwin.
            cygusr = self._port_obj._path_from_base('third_party', 'cygwin',
                'usr')
            httpd_conf = httpd_conf.replace('ServerRoot "/usr"',
                'ServerRoot "%s"' % self._get_cygwin_path(cygusr))

        with codecs.open(httpd_config_copy, "w", "utf-8") as file:
            file.write(httpd_conf)

        if self._is_cygwin():
            return self._get_cygwin_path(httpd_config_copy)
        return httpd_config_copy

    def _get_virtual_host_config(self, document_root, port, ssl=False):
        """Returns a <VirtualHost> directive block for an httpd.conf file.
        It will listen to 127.0.0.1 on each of the given port.
        """
        return '\n'.join(('<VirtualHost 127.0.0.1:%s>' % port,
                          'DocumentRoot "%s"' % document_root,
                          ssl and 'SSLEngine On' or '',
                          '</VirtualHost>', ''))

    def _start_httpd_process(self):
        """Starts the httpd process and returns whether there were errors."""
        # Use shell=True because we join the arguments into a string for
        # the sake of Window/Cygwin and it needs quoting that breaks
        # shell=False.
        # FIXME: We should not need to be joining shell arguments into strings.
        # shell=True is a trail of tears.
        # Note: Not thread safe: http://bugs.python.org/issue2320
        _log.debug('Starting http server, cmd="%s"' % str(self._start_cmd))
        self._httpd_proc = subprocess.Popen(self._start_cmd,
                                            stderr=subprocess.PIPE,
            shell=True)
        err = self._httpd_proc.stderr.read()
        if len(err):
            _log.debug(err)
            return False
        return True

    def start(self):
        """Starts the apache http server."""
        # Stop any currently running servers.
        self.stop()

        _log.debug("Starting apache http server")
        server_started = self.wait_for_action(self._start_httpd_process)
        if server_started:
            _log.debug("Apache started. Testing ports")
            server_started = self.wait_for_action(
                self.is_server_running_on_all_ports)

        if server_started:
            _log.debug("Server successfully started")
        else:
            raise Exception('Failed to start http server')

    def stop(self):
        """Stops the apache http server."""
        _log.debug("Shutting down any running http servers")
        httpd_pid = None
        if os.path.exists(self._pid_file):
            httpd_pid = int(open(self._pid_file).readline())
        # FIXME: We shouldn't be calling a protected method of _port_obj!
        self._port_obj._shut_down_http_server(httpd_pid)
